/*
 *  PCM - Linear conversion
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
  
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	int conv_idx;
	int sformat;
} snd_pcm_linear_t;

static void linear_transfer(const snd_pcm_channel_area_t *src_areas, size_t src_offset,
			    const snd_pcm_channel_area_t *dst_areas, size_t dst_offset,
			    size_t channels, size_t frames, int convidx)
{
#define CONV_LABELS
#include "plugin_ops.h"
#undef CONV_LABELS
	void *conv = conv_labels[convidx];
	unsigned int channel;
	for (channel = 0; channel < channels; ++channel) {
		char *src;
		char *dst;
		int src_step, dst_step;
		size_t frames1;
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
#if 0
		if (!src_area->enabled) {
			if (dst_area->wanted)
				snd_pcm_area_silence(dst_area, dst_offset, frames, dst_sfmt);
			dst_area->enabled = 0;
			continue;
		}
		dst_area->enabled = 1;
#endif
		src = snd_pcm_channel_area_addr(src_area, src_offset);
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		src_step = snd_pcm_channel_area_step(src_area);
		dst_step = snd_pcm_channel_area_step(dst_area);
		frames1 = frames;
		while (frames1-- > 0) {
			goto *conv;
#define CONV_END after
#include "plugin_ops.h"
#undef CONV_END
		after:
			src += src_step;
			dst += dst_step;
		}
	}
}

static int snd_pcm_linear_hw_info(snd_pcm_t *pcm, snd_pcm_hw_info_t * info)
{
	snd_pcm_linear_t *linear = pcm->private;
	unsigned int format_mask, access_mask;
	int err;
	info->access_mask &= (SND_PCM_ACCBIT_MMAP_INTERLEAVED | 
			      SND_PCM_ACCBIT_RW_INTERLEAVED |
			      SND_PCM_ACCBIT_MMAP_NONINTERLEAVED | 
			      SND_PCM_ACCBIT_RW_NONINTERLEAVED);
	access_mask = info->access_mask;
	if (access_mask == 0)
		return -EINVAL;
	info->format_mask &= SND_PCM_FMTBIT_LINEAR;
	format_mask = info->format_mask;
	if (format_mask == 0)
		return -EINVAL;

	info->format_mask = 1U << linear->sformat;
	info->access_mask = SND_PCM_ACCBIT_MMAP;
	err = snd_pcm_hw_info(linear->plug.slave, info);
	info->format_mask = format_mask;
	info->access_mask = access_mask;
	if (err < 0)
		return err;
	info->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	snd_pcm_hw_info_complete(info);
	return 0;
}

static int snd_pcm_linear_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_linear_t *linear = pcm->private;
	snd_pcm_t *slave = linear->plug.slave;
	snd_pcm_hw_info_t sinfo;
	snd_pcm_hw_params_t sparams;
	int err;
	snd_pcm_hw_params_to_info(params, &sinfo);
	sinfo.format_mask = 1 << linear->sformat;
	sinfo.access_mask = SND_PCM_ACCBIT_MMAP;
	err = snd_pcm_hw_params_info(slave, &sparams, &sinfo);
	params->fail_mask = sparams.fail_mask;
	if (err < 0)
		return err;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		linear->conv_idx = conv_index(params->format,
					      linear->sformat);
	else
		linear->conv_idx = conv_index(linear->sformat,
					      params->format);
	return 0;
}

static ssize_t snd_pcm_linear_write_areas(snd_pcm_t *pcm,
					  const snd_pcm_channel_area_t *areas,
					  size_t offset,
					  size_t size,
					  size_t *slave_sizep)
{
	snd_pcm_linear_t *linear = pcm->private;
	snd_pcm_t *slave = linear->plug.slave;
	size_t xfer = 0;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_playback_xfer(slave, size - xfer);
		linear_transfer(areas, offset, 
				snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
				pcm->channels, frames, linear->conv_idx);
		err = snd_pcm_mmap_forward(slave, frames);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		offset += err;
		xfer += err;
		snd_pcm_mmap_hw_forward(pcm, err);
	}
	if (xfer > 0) {
		if (slave_sizep)
			*slave_sizep = xfer;
		return xfer;
	}
	return err;
}

static ssize_t snd_pcm_linear_read_areas(snd_pcm_t *pcm,
					 const snd_pcm_channel_area_t *areas,
					 size_t offset,
					 size_t size,
					 size_t *slave_sizep)
{
	snd_pcm_linear_t *linear = pcm->private;
	snd_pcm_t *slave = linear->plug.slave;
	size_t xfer = 0;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_capture_xfer(slave, size - xfer);
		linear_transfer(snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
				areas, offset, 
				pcm->channels, frames, linear->conv_idx);
		err = snd_pcm_mmap_forward(slave, frames);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		offset += err;
		xfer += err;
		snd_pcm_mmap_hw_forward(pcm, err);
	}
	if (xfer > 0) {
		if (slave_sizep)
			*slave_sizep = xfer;
		return xfer;
	}
	return err;
}

static void snd_pcm_linear_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_linear_t *linear = pcm->private;
	fprintf(fp, "Linear conversion PCM (%s)\n", 
		snd_pcm_format_name(linear->sformat));
	if (pcm->setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(linear->plug.slave, fp);
}

snd_pcm_ops_t snd_pcm_linear_ops = {
	close: snd_pcm_plugin_close,
	info: snd_pcm_plugin_info,
	hw_info: snd_pcm_linear_hw_info,
	hw_params: snd_pcm_linear_hw_params,
	sw_params: snd_pcm_plugin_sw_params,
	dig_info: snd_pcm_plugin_dig_info,
	dig_params: snd_pcm_plugin_dig_params,
	channel_info: snd_pcm_plugin_channel_info,
	dump: snd_pcm_linear_dump,
	nonblock: snd_pcm_plugin_nonblock,
	async: snd_pcm_plugin_async,
	mmap: snd_pcm_plugin_mmap,
	munmap: snd_pcm_plugin_munmap,
};

int snd_pcm_linear_open(snd_pcm_t **pcmp, char *name, int sformat, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_linear_t *linear;
	assert(pcmp && slave);
	if (snd_pcm_format_linear(sformat) != 1)
		return -EINVAL;
	linear = calloc(1, sizeof(snd_pcm_linear_t));
	if (!linear) {
		return -ENOMEM;
	}
	linear->sformat = sformat;
	linear->plug.read = snd_pcm_linear_read_areas;
	linear->plug.write = snd_pcm_linear_write_areas;
	linear->plug.slave = slave;
	linear->plug.close_slave = close_slave;

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(linear);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_LINEAR;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_linear_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = linear;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = &linear->plug.hw_ptr;
	pcm->appl_ptr = &linear->plug.appl_ptr;
	*pcmp = pcm;

	return 0;
}

int _snd_pcm_linear_open(snd_pcm_t **pcmp, char *name,
			 snd_config_t *conf, 
			 int stream, int mode)
{
	snd_config_iterator_t i;
	char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	int sformat = -1;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "sname") == 0) {
			err = snd_config_string_get(n, &sname);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "sformat") == 0) {
			char *f;
			err = snd_config_string_get(n, &f);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			sformat = snd_pcm_format_value(f);
			if (sformat < 0) {
				ERR("Unknown sformat");
				return -EINVAL;
			}
			if (snd_pcm_format_linear(sformat) != 1) {
				ERR("sformat is not linear");
				return -EINVAL;
			}
			continue;
		}
		ERR("Unknown field %s", n->id);
		return -EINVAL;
	}
	if (!sname) {
		ERR("sname is not defined");
		return -EINVAL;
	}
	if (sformat < 0) {
		ERR("sformat is not defined");
		return -EINVAL;
	}
	/* This is needed cause snd_config_update may destroy config */
	sname = strdup(sname);
	if (!sname)
		return  -ENOMEM;
	err = snd_pcm_open(&spcm, sname, stream, mode);
	free(sname);
	if (err < 0)
		return err;
	err = snd_pcm_linear_open(pcmp, name, sformat, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				

