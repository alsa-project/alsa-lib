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
	int cformat;
	int cxfer_mode, cmmap_shape;
} snd_pcm_linear_t;

static void linear_transfer(snd_pcm_channel_area_t *src_areas, size_t src_offset,
			    snd_pcm_channel_area_t *dst_areas, size_t dst_offset,
			    size_t frames, size_t channels, int convidx)
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
		snd_pcm_channel_area_t *src_area = &src_areas[channel];
		snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
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

static int snd_pcm_linear_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t * info)
{
	snd_pcm_linear_t *linear = pcm->private;
	unsigned int req_mask = info->req_mask;
	unsigned int sfmt = info->req.format.sfmt;
	int err;
	if (req_mask & SND_PCM_PARAMS_SFMT &&
	    !snd_pcm_format_linear(sfmt)) {
		info->req.fail_mask = SND_PCM_PARAMS_SFMT;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	info->req_mask |= SND_PCM_PARAMS_SFMT;
	info->req_mask &= ~(SND_PCM_PARAMS_MMAP_SHAPE | 
			    SND_PCM_PARAMS_XFER_MODE);
	info->req.format.sfmt = linear->sformat;
	err = snd_pcm_params_info(linear->plug.slave, info);
	info->req_mask = req_mask;
	info->req.format.sfmt = sfmt;
	if (err < 0)
		return err;
	if (req_mask & SND_PCM_PARAMS_SFMT)
		info->formats = 1 << sfmt;
	else
		info->formats = SND_PCM_LINEAR_FORMATS;
	info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	info->flags |= SND_PCM_INFO_INTERLEAVED | SND_PCM_INFO_NONINTERLEAVED;
	return err;
}

static int snd_pcm_linear_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_linear_t *linear = pcm->private;
	snd_pcm_t *slave = linear->plug.slave;
	int err;
	if (!snd_pcm_format_linear(params->format.sfmt)) {
		params->fail_mask = SND_PCM_PARAMS_SFMT;
		params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	if (slave->mmap_data) {
		err = snd_pcm_munmap_data(slave);
		if (err < 0)
			return err;
	}
	linear->cformat = params->format.sfmt;
	linear->cxfer_mode = params->xfer_mode;
	linear->cmmap_shape = params->mmap_shape;
	params->format.sfmt = linear->sformat;
	params->xfer_mode = SND_PCM_XFER_UNSPECIFIED;
	params->mmap_shape = SND_PCM_MMAP_UNSPECIFIED;
	err = snd_pcm_params(slave, params);
	params->format.sfmt = linear->cformat;
	params->xfer_mode = linear->cxfer_mode;
	params->mmap_shape = linear->cmmap_shape;
	if (slave->valid_setup) {
		int r = snd_pcm_mmap_data(slave, NULL);
		assert(r >= 0);
	}
	return err;
}

static int snd_pcm_linear_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_linear_t *linear = pcm->private;
	int err = snd_pcm_setup(linear->plug.slave, setup);
	if (err < 0)
		return err;
	assert(linear->sformat == setup->format.sfmt);
	
	if (linear->cxfer_mode == SND_PCM_XFER_UNSPECIFIED)
		setup->xfer_mode = SND_PCM_XFER_NONINTERLEAVED;
	else
		setup->xfer_mode = linear->cxfer_mode;
	if (linear->cmmap_shape == SND_PCM_MMAP_UNSPECIFIED)
		setup->mmap_shape = SND_PCM_MMAP_NONINTERLEAVED;
	else
		setup->mmap_shape = linear->cmmap_shape;
	setup->format.sfmt = linear->cformat;
	setup->mmap_bytes = 0;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		linear->conv_idx = conv_index(linear->cformat,
					      linear->sformat);
	else
		linear->conv_idx = conv_index(linear->sformat,
					      linear->cformat);
	return 0;
}

static ssize_t snd_pcm_linear_write_areas(snd_pcm_t *pcm,
					  snd_pcm_channel_area_t *areas,
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
				frames, pcm->setup.format.channels, linear->conv_idx);
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
					 snd_pcm_channel_area_t *areas,
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
				frames, pcm->setup.format.channels, linear->conv_idx);
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
	if (pcm->valid_setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(linear->plug.slave, fp);
}

struct snd_pcm_ops snd_pcm_linear_ops = {
	close: snd_pcm_plugin_close,
	info: snd_pcm_plugin_info,
	params_info: snd_pcm_linear_params_info,
	params: snd_pcm_linear_params,
	setup: snd_pcm_linear_setup,
	channel_info: snd_pcm_plugin_channel_info,
	channel_params: snd_pcm_plugin_channel_params,
	channel_setup: snd_pcm_plugin_channel_setup,
	dump: snd_pcm_linear_dump,
	nonblock: snd_pcm_plugin_nonblock,
	mmap_status: snd_pcm_plugin_mmap_status,
	mmap_control: snd_pcm_plugin_mmap_control,
	mmap_data: snd_pcm_plugin_mmap_data,
	munmap_status: snd_pcm_plugin_munmap_status,
	munmap_control: snd_pcm_plugin_munmap_control,
	munmap_data: snd_pcm_plugin_munmap_data,
};

int snd_pcm_linear_open(snd_pcm_t **handlep, char *name, int sformat, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *handle;
	snd_pcm_linear_t *linear;
	int err;
	assert(handlep && slave);
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

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		free(linear);
		return -ENOMEM;
	}
	if (name)
		handle->name = strdup(name);
	handle->type = SND_PCM_TYPE_LINEAR;
	handle->stream = slave->stream;
	handle->ops = &snd_pcm_linear_ops;
	handle->op_arg = handle;
	handle->fast_ops = &snd_pcm_plugin_fast_ops;
	handle->fast_op_arg = handle;
	handle->mode = slave->mode;
	handle->private = linear;
	err = snd_pcm_init(handle);
	if (err < 0) {
		snd_pcm_close(handle);
		return err;
	}
	*handlep = handle;

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
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "sformat") == 0) {
			char *f;
			err = snd_config_string_get(n, &f);
			if (err < 0)
				return -EINVAL;
			sformat = snd_pcm_format_value(f);
			if (sformat < 0)
				return -EINVAL;
			if (snd_pcm_format_linear(sformat) != 1)
				return -EINVAL;
			continue;
		}
		return -EINVAL;
	}
	if (!sname || !sformat)
		return -EINVAL;
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
				

