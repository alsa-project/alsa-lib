/*
 *  PCM - A-Law conversion
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

typedef void (*alaw_f)(snd_pcm_channel_area_t *src_areas,
			size_t src_offset,
			snd_pcm_channel_area_t *dst_areas,
			size_t dst_offset,
			size_t frames, size_t channels, int getputidx);

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	int getput_idx;
	alaw_f func;
	int sformat;
	int cformat;
	int cxfer_mode, cmmap_shape;
} snd_pcm_alaw_t;

static inline int val_seg(int val)
{
	int r = 1;
	val >>= 8;
	if (val & 0xf0) {
		val >>= 4;
		r += 4;
	}
	if (val & 0x0c) {
		val >>= 2;
		r += 2;
	}
	if (val & 0x02)
		r += 1;
	return r;
}

/*
 * s16_to_alaw() - Convert a 16-bit linear PCM value to 8-bit A-law
 *
 * s16_to_alaw() accepts an 16-bit integer and encodes it as A-law data.
 *
 *		Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	0000000wxyza			000wxyz
 *	0000001wxyza			001wxyz
 *	000001wxyzab			010wxyz
 *	00001wxyzabc			011wxyz
 *	0001wxyzabcd			100wxyz
 *	001wxyzabcde			101wxyz
 *	01wxyzabcdef			110wxyz
 *	1wxyzabcdefg			111wxyz
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */

static unsigned char s16_to_alaw(int pcm_val)
{
	int		mask;
	int		seg;
	unsigned char	aval;

	if (pcm_val >= 0) {
		mask = 0xD5;
	} else {
		mask = 0x55;
		pcm_val = -pcm_val;
		if (pcm_val > 0x7fff)
			pcm_val = 0x7fff;
	}

	if (pcm_val < 256)
		aval = pcm_val >> 4;
	else {
		/* Convert the scaled magnitude to segment number. */
		seg = val_seg(pcm_val);
		aval = (seg << 4) | ((pcm_val >> (seg + 3)) & 0x0f);
	}
	return aval ^ mask;
}

/*
 * alaw_to_s16() - Convert an A-law value to 16-bit linear PCM
 *
 */
static int alaw_to_s16(unsigned char a_val)
{
	int		t;
	int		seg;

	a_val ^= 0x55;
	t = a_val & 0x7f;
	if (t < 16)
		t = (t << 4) + 8;
	else {
		seg = (t >> 4) & 0x07;
		t = ((t & 0x0f) << 4) + 0x108;
		t <<= seg -1;
	}
	return ((a_val & 0x80) ? t : -t);
}

static void alaw_decode(snd_pcm_channel_area_t *src_areas,
			size_t src_offset,
			snd_pcm_channel_area_t *dst_areas,
			size_t dst_offset,
			size_t frames, size_t channels, int putidx)
{
#define PUT_S16_LABELS
#include "plugin_ops.h"
#undef PUT_S16_LABELS
	void *put = put_s16_labels[putidx];
	size_t channel;
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
				snd_pcm_area_silence(&dst_areas[channel], dst_offset, frames, dst_sfmt);
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
			int16_t sample = alaw_to_s16(*src);
			goto *put;
#define PUT_S16_END after
#include "plugin_ops.h"
#undef PUT_S16_END
		after:
			src += src_step;
			dst += dst_step;
		}
	}
}

static void alaw_encode(snd_pcm_channel_area_t *src_areas,
			 size_t src_offset,
			 snd_pcm_channel_area_t *dst_areas,
			 size_t dst_offset,
			 size_t frames, size_t channels, int getidx)
{
#define GET_S16_LABELS
#include "plugin_ops.h"
#undef GET_S16_LABELS
	void *get = get_s16_labels[getidx];
	size_t channel;
	int16_t sample = 0;
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
				snd_pcm_area_silence(&dst_area->area, 0, frames, dst_sfmt);
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
			goto *get;
#define GET_S16_END after
#include "plugin_ops.h"
#undef GET_S16_END
		after:
			*dst = s16_to_alaw(sample);
			src += src_step;
			dst += dst_step;
		}
	}
}

static int snd_pcm_alaw_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t * info)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	unsigned int req_mask = info->req_mask;
	unsigned int sfmt = info->req.format.sfmt;
	int err;
	if (req_mask & SND_PCM_PARAMS_SFMT) {
		if (alaw->sformat == SND_PCM_SFMT_A_LAW ?
		    !snd_pcm_format_linear(sfmt) :
		    sfmt != SND_PCM_SFMT_A_LAW) {
			info->req.fail_mask = SND_PCM_PARAMS_SFMT;
			info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
			return -EINVAL;
		}
	}
	info->req_mask |= SND_PCM_PARAMS_SFMT;
	info->req_mask &= ~(SND_PCM_PARAMS_MMAP_SHAPE | 
			    SND_PCM_PARAMS_XFER_MODE);
	info->req.format.sfmt = alaw->sformat;
	err = snd_pcm_params_info(alaw->plug.slave, info);
	info->req_mask = req_mask;
	info->req.format.sfmt = sfmt;
	if (err < 0)
		return err;
	if (req_mask & SND_PCM_PARAMS_SFMT)
		info->formats = 1 << sfmt;
	else
		info->formats = alaw->sformat == SND_PCM_SFMT_A_LAW ?
			SND_PCM_LINEAR_FORMATS : 1 << SND_PCM_SFMT_A_LAW;
	info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	info->flags |= SND_PCM_INFO_INTERLEAVED | SND_PCM_INFO_NONINTERLEAVED;
	return err;
}

static int snd_pcm_alaw_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	snd_pcm_t *slave = alaw->plug.slave;
	int err;
	if (alaw->sformat == SND_PCM_SFMT_A_LAW ?
	    !snd_pcm_format_linear(params->format.sfmt) :
	    params->format.sfmt != SND_PCM_SFMT_A_LAW) {
		params->fail_mask = SND_PCM_PARAMS_SFMT;
		params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	if (slave->mmap_data) {
		err = snd_pcm_munmap_data(slave);
		if (err < 0)
			return err;
	}
	alaw->cformat = params->format.sfmt;
	alaw->cxfer_mode = params->xfer_mode;
	alaw->cmmap_shape = params->mmap_shape;
	params->format.sfmt = alaw->sformat;
	params->xfer_mode = SND_PCM_XFER_UNSPECIFIED;
	params->mmap_shape = SND_PCM_MMAP_UNSPECIFIED;;
	err = snd_pcm_params(slave, params);
	params->format.sfmt = alaw->cformat;
	params->xfer_mode = alaw->cxfer_mode;
	params->mmap_shape = alaw->cmmap_shape;
	if (slave->valid_setup) {
		int r = snd_pcm_mmap_data(slave, NULL);
		assert(r >= 0);
	}
	return err;
}

static int snd_pcm_alaw_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	int err = snd_pcm_setup(alaw->plug.slave, setup);
	if (err < 0)
		return err;
	assert(alaw->sformat == setup->format.sfmt);
	if (alaw->cxfer_mode == SND_PCM_XFER_UNSPECIFIED)
		setup->xfer_mode = SND_PCM_XFER_NONINTERLEAVED;
	else
		setup->xfer_mode = alaw->cxfer_mode;
	if (alaw->cmmap_shape == SND_PCM_MMAP_UNSPECIFIED)
		setup->mmap_shape = SND_PCM_MMAP_NONINTERLEAVED;
	else
		setup->mmap_shape = alaw->cmmap_shape;
	setup->format.sfmt = alaw->cformat;
	setup->mmap_bytes = 0;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		if (alaw->sformat == SND_PCM_SFMT_A_LAW) {
			alaw->getput_idx = getput_index(alaw->cformat);
			alaw->func = alaw_encode;
		} else {
			alaw->getput_idx = getput_index(alaw->sformat);
			alaw->func = alaw_decode;
		}
	} else {
		if (alaw->sformat == SND_PCM_SFMT_A_LAW) {
			alaw->getput_idx = getput_index(alaw->cformat);
			alaw->func = alaw_decode;
		} else {
			alaw->getput_idx = getput_index(alaw->sformat);
			alaw->func = alaw_encode;
		}
	}
	return 0;
}

static ssize_t snd_pcm_alaw_write_areas(snd_pcm_t *pcm,
					snd_pcm_channel_area_t *areas,
					size_t offset,
					size_t size,
					size_t *slave_sizep)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	snd_pcm_t *slave = alaw->plug.slave;
	size_t xfer = 0;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_playback_xfer(slave, size - xfer);
		alaw->func(areas, offset, 
			    slave->mmap_areas, snd_pcm_mmap_offset(slave),
			    frames, pcm->setup.format.channels,
			    alaw->getput_idx);
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

static ssize_t snd_pcm_alaw_read_areas(snd_pcm_t *pcm,
				       snd_pcm_channel_area_t *areas,
				       size_t offset,
				       size_t size,
				       size_t *slave_sizep)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	snd_pcm_t *slave = alaw->plug.slave;
	size_t xfer = 0;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_capture_xfer(slave, size - xfer);
		alaw->func(slave->mmap_areas, snd_pcm_mmap_offset(slave),
			   areas, offset, 
			   frames, pcm->setup.format.channels,
			   alaw->getput_idx);
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

static void snd_pcm_alaw_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	fprintf(fp, "A-Law conversion PCM (%s)\n", 
		snd_pcm_format_name(alaw->sformat));
	if (pcm->valid_setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(alaw->plug.slave, fp);
}

struct snd_pcm_ops snd_pcm_alaw_ops = {
	close: snd_pcm_plugin_close,
	info: snd_pcm_plugin_info,
	params_info: snd_pcm_alaw_params_info,
	params: snd_pcm_alaw_params,
	setup: snd_pcm_alaw_setup,
	channel_info: snd_pcm_plugin_channel_info,
	channel_params: snd_pcm_plugin_channel_params,
	channel_setup: snd_pcm_plugin_channel_setup,
	dump: snd_pcm_alaw_dump,
	nonblock: snd_pcm_plugin_nonblock,
	mmap_status: snd_pcm_plugin_mmap_status,
	mmap_control: snd_pcm_plugin_mmap_control,
	mmap_data: snd_pcm_plugin_mmap_data,
	munmap_status: snd_pcm_plugin_munmap_status,
	munmap_control: snd_pcm_plugin_munmap_control,
	munmap_data: snd_pcm_plugin_munmap_data,
};

int snd_pcm_alaw_open(snd_pcm_t **handlep, int sformat, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *handle;
	snd_pcm_alaw_t *alaw;
	int err;
	assert(handlep && slave);
	if (snd_pcm_format_linear(sformat) != 1 &&
	    sformat != SND_PCM_SFMT_A_LAW)
		return -EINVAL;
	alaw = calloc(1, sizeof(snd_pcm_alaw_t));
	if (!alaw) {
		return -ENOMEM;
	}
	alaw->sformat = sformat;
	alaw->plug.read = snd_pcm_alaw_read_areas;
	alaw->plug.write = snd_pcm_alaw_write_areas;
	alaw->plug.slave = slave;
	alaw->plug.close_slave = close_slave;

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		free(alaw);
		return -ENOMEM;
	}
	handle->type = SND_PCM_TYPE_ALAW;
	handle->stream = slave->stream;
	handle->ops = &snd_pcm_alaw_ops;
	handle->op_arg = handle;
	handle->fast_ops = &snd_pcm_plugin_fast_ops;
	handle->fast_op_arg = handle;
	handle->mode = slave->mode;
	handle->private = alaw;
	err = snd_pcm_init(handle);
	if (err < 0) {
		snd_pcm_close(handle);
		return err;
	}
	*handlep = handle;

	return 0;
}

int _snd_pcm_alaw_open(snd_pcm_t **pcmp, char *name,
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
			if (snd_pcm_format_linear(sformat) != 1 &&
			    sformat != SND_PCM_SFMT_A_LAW)
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
	err = snd_pcm_alaw_open(pcmp, sformat, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				

