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

typedef void (*alaw_f)(const snd_pcm_channel_area_t *src_areas,
			snd_pcm_uframes_t src_offset,
			const snd_pcm_channel_area_t *dst_areas,
			snd_pcm_uframes_t dst_offset,
			unsigned int channels, snd_pcm_uframes_t frames, int getputidx);

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	int getput_idx;
	alaw_f func;
	int sformat;
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

void snd_pcm_alaw_decode(const snd_pcm_channel_area_t *src_areas,
			 snd_pcm_uframes_t src_offset,
			 const snd_pcm_channel_area_t *dst_areas,
			 snd_pcm_uframes_t dst_offset,
			 unsigned int channels, snd_pcm_uframes_t frames, int putidx)
{
#define PUT16_LABELS
#include "plugin_ops.h"
#undef PUT16_LABELS
	void *put = put16_labels[putidx];
	unsigned int channel;
	for (channel = 0; channel < channels; ++channel) {
		char *src;
		char *dst;
		int src_step, dst_step;
		snd_pcm_uframes_t frames1;
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
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
#define PUT16_END after
#include "plugin_ops.h"
#undef PUT16_END
		after:
			src += src_step;
			dst += dst_step;
		}
	}
}

void snd_pcm_alaw_encode(const snd_pcm_channel_area_t *src_areas,
			 snd_pcm_uframes_t src_offset,
			 const snd_pcm_channel_area_t *dst_areas,
			 snd_pcm_uframes_t dst_offset,
			 unsigned int channels, snd_pcm_uframes_t frames, int getidx)
{
#define GET16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
	void *get = get16_labels[getidx];
	unsigned int channel;
	int16_t sample = 0;
	for (channel = 0; channel < channels; ++channel) {
		char *src;
		char *dst;
		int src_step, dst_step;
		snd_pcm_uframes_t frames1;
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
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
#define GET16_END after
#include "plugin_ops.h"
#undef GET16_END
		after:
			*dst = s16_to_alaw(sample);
			src += src_step;
			dst += dst_step;
		}
	}
}

static int snd_pcm_alaw_hw_refine_cprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	int err;
	mask_t *access_mask = alloca(mask_sizeof());
	mask_load(access_mask, SND_PCM_ACCBIT_PLUGIN);
	err = _snd_pcm_hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS,
				     access_mask);
	if (err < 0)
		return err;
	if (alaw->sformat == SND_PCM_FORMAT_A_LAW) {
		mask_t *format_mask = alloca(mask_sizeof());
		mask_load(format_mask, SND_PCM_FMTBIT_LINEAR);
		err = _snd_pcm_hw_param_mask(params, SND_PCM_HW_PARAM_FORMAT,
					     format_mask);
	} else {
		err = _snd_pcm_hw_param_set(params,
					    SND_PCM_HW_PARAM_FORMAT,
					    SND_PCM_FORMAT_A_LAW, 0);
	}
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_set(params, SND_PCM_HW_PARAM_SUBFORMAT,
				     SND_PCM_SUBFORMAT_STD, 0);
	if (err < 0)
		return err;
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_alaw_hw_refine_sprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *sparams)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	mask_t *saccess_mask = alloca(mask_sizeof());
	mask_load(saccess_mask, SND_PCM_ACCBIT_MMAP);
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				saccess_mask);
	_snd_pcm_hw_param_set(sparams, SND_PCM_HW_PARAM_FORMAT,
			      alaw->sformat, 0);
	_snd_pcm_hw_param_set(sparams, SND_PCM_HW_PARAM_SUBFORMAT,
			      SND_PCM_SUBFORMAT_STD, 0);
	return 0;
}

static int snd_pcm_alaw_hw_refine_schange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
					    snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_CHANNELS |
			      SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	err = _snd_pcm_hw_params_refine(sparams, links, params);
	if (err < 0)
		return err;
	return 0;
}
	
static int snd_pcm_alaw_hw_refine_cchange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
					    snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_CHANNELS |
			      SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	err = _snd_pcm_hw_params_refine(params, links, sparams);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_alaw_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_alaw_hw_refine_cprepare,
				       snd_pcm_alaw_hw_refine_cchange,
				       snd_pcm_alaw_hw_refine_sprepare,
				       snd_pcm_alaw_hw_refine_schange,
				       snd_pcm_plugin_hw_refine_slave);
}

static int snd_pcm_alaw_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_alaw_hw_refine_cchange,
					  snd_pcm_alaw_hw_refine_sprepare,
					  snd_pcm_alaw_hw_refine_schange,
					  snd_pcm_plugin_hw_params_slave);
	if (err < 0)
		return err;

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		if (alaw->sformat == SND_PCM_FORMAT_A_LAW) {
			alaw->getput_idx = snd_pcm_linear_get_index(snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_FORMAT, 0), SND_PCM_FORMAT_S16);
			alaw->func = snd_pcm_alaw_encode;
		} else {
			alaw->getput_idx = snd_pcm_linear_put_index(SND_PCM_FORMAT_S16, alaw->sformat);
			alaw->func = snd_pcm_alaw_decode;
		}
	} else {
		if (alaw->sformat == SND_PCM_FORMAT_A_LAW) {
			alaw->getput_idx = snd_pcm_linear_put_index(SND_PCM_FORMAT_S16, snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_FORMAT, 0));
			alaw->func = snd_pcm_alaw_decode;
		} else {
			alaw->getput_idx = snd_pcm_linear_get_index(alaw->sformat, SND_PCM_FORMAT_S16);
			alaw->func = snd_pcm_alaw_encode;
		}
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_alaw_write_areas(snd_pcm_t *pcm,
					const snd_pcm_channel_area_t *areas,
					snd_pcm_uframes_t offset,
					snd_pcm_uframes_t size,
					snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	snd_pcm_t *slave = alaw->plug.slave;
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_sframes_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		snd_pcm_uframes_t frames = snd_pcm_mmap_playback_xfer(slave, size - xfer);
		alaw->func(areas, offset, 
			    snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
			    pcm->channels, frames,
			    alaw->getput_idx);
		err = snd_pcm_mmap_forward(slave, frames);
		if (err < 0)
			break;
		assert((snd_pcm_uframes_t)err == frames);
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

static snd_pcm_sframes_t snd_pcm_alaw_read_areas(snd_pcm_t *pcm,
				       const snd_pcm_channel_area_t *areas,
				       snd_pcm_uframes_t offset,
				       snd_pcm_uframes_t size,
				       snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	snd_pcm_t *slave = alaw->plug.slave;
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_sframes_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		snd_pcm_uframes_t frames = snd_pcm_mmap_capture_xfer(slave, size - xfer);
		alaw->func(snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
			   areas, offset, 
			   pcm->channels, frames,
			   alaw->getput_idx);
		err = snd_pcm_mmap_forward(slave, frames);
		if (err < 0)
			break;
		assert((snd_pcm_uframes_t)err == frames);
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

static void snd_pcm_alaw_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_alaw_t *alaw = pcm->private;
	snd_output_printf(out, "A-Law conversion PCM (%s)\n", 
		snd_pcm_format_name(alaw->sformat));
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(alaw->plug.slave, out);
}

snd_pcm_ops_t snd_pcm_alaw_ops = {
	close: snd_pcm_plugin_close,
	card: snd_pcm_plugin_card,
	info: snd_pcm_plugin_info,
	hw_refine: snd_pcm_alaw_hw_refine,
	hw_params: snd_pcm_alaw_hw_params,
	hw_free: snd_pcm_plugin_hw_free,
	sw_params: snd_pcm_plugin_sw_params,
	channel_info: snd_pcm_plugin_channel_info,
	dump: snd_pcm_alaw_dump,
	nonblock: snd_pcm_plugin_nonblock,
	async: snd_pcm_plugin_async,
	mmap: snd_pcm_plugin_mmap,
	munmap: snd_pcm_plugin_munmap,
};

int snd_pcm_alaw_open(snd_pcm_t **pcmp, char *name, int sformat, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_alaw_t *alaw;
	assert(pcmp && slave);
	if (snd_pcm_format_linear(sformat) != 1 &&
	    sformat != SND_PCM_FORMAT_A_LAW)
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

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(alaw);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_ALAW;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_alaw_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = alaw;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = &alaw->plug.hw_ptr;
	pcm->appl_ptr = &alaw->plug.appl_ptr;
	*pcmp = pcm;

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
			if (snd_pcm_format_linear(sformat) != 1 &&
			    sformat != SND_PCM_FORMAT_A_LAW) {
				ERR("Invalid sformat");
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
	err = snd_pcm_alaw_open(pcmp, name, sformat, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				

