/*
 *  PCM - Ima-ADPC conversion
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *  Copyright (c) 1999 by Uros Bizjak <uros@kss-loka.si>
 *                        Jaroslav Kysela <perex@suse.cz>
 *
 *  Based on Version 1.2, 18-Dec-92 implementation of Intel/DVI ADPCM code
 *  by Jack Jansen, CWI, Amsterdam <Jack.Jansen@cwi.nl>, Copyright 1992
 *  by Stichting Mathematisch Centrum, Amsterdam, The Netherlands.
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
  
/*
These routines convert 16 bit linear PCM samples to 4 bit ADPCM code
and vice versa. The ADPCM code used is the Intel/DVI ADPCM code which
is being recommended by the IMA Digital Audio Technical Working Group.

The algorithm for this coder was taken from:
Proposal for Standardized Audio Interstreamge Formats,
IMA compatability project proceedings, Vol 2, Issue 2, May 1992.

- No, this is *not* a G.721 coder/decoder. The algorithm used by G.721
  is very complicated, requiring oodles of floating-point ops per
  sample (resulting in very poor performance). I have not done any
  tests myself but various people have assured my that 721 quality is
  actually lower than DVI quality.

- No, it probably isn't a RIFF ADPCM decoder either. Trying to decode
  RIFF ADPCM with these routines seems to result in something
  recognizable but very distorted.

- No, it is not a CDROM-XA coder either, as far as I know. I haven't
  come across a good description of XA yet.
 */

#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

typedef void (*adpcm_f)(const snd_pcm_channel_area_t *src_areas,
			snd_pcm_uframes_t src_offset,
			const snd_pcm_channel_area_t *dst_areas,
			snd_pcm_uframes_t dst_offset,
			unsigned int channels, snd_pcm_uframes_t frames, int getputidx,
			snd_pcm_adpcm_state_t *states);

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	int getput_idx;
	adpcm_f func;
	int sformat;
	snd_pcm_adpcm_state_t *states;
} snd_pcm_adpcm_t;

/* First table lookup for Ima-ADPCM quantizer */
static char IndexAdjust[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

/* Second table lookup for Ima-ADPCM quantizer */
static short StepSize[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static char adpcm_encoder(int sl, snd_pcm_adpcm_state_t * state)
{
	short diff;		/* Difference between sl and predicted sample */
	short pred_diff;	/* Predicted difference to next sample */

	unsigned char sign;	/* sign of diff */
	short step;		/* holds previous StepSize value */
	unsigned char adjust_idx;	/* Index to IndexAdjust lookup table */

	int i;

	/* Compute difference to previous predicted value */
	diff = sl - state->pred_val;
	sign = (diff < 0) ? 0x8 : 0x0;
	if (sign) {
		diff = -diff;
	}

	/*
	 * This code *approximately* computes:
	 *    adjust_idx = diff * 4 / step;
	 *    pred_diff = (adjust_idx + 0.5) * step / 4;
	 *
	 * But in shift step bits are dropped. The net result of this is
	 * that even if you have fast mul/div hardware you cannot put it to
	 * good use since the fixup would be too expensive.
	 */

	step = StepSize[state->step_idx];

	/* Divide and clamp */
	pred_diff = step >> 3;
	for (adjust_idx = 0, i = 0x4; i; i >>= 1, step >>= 1) {
		if (diff >= step) {
			adjust_idx |= i;
			diff -= step;
			pred_diff += step;
		}
	}

	/* Update and clamp previous predicted value */
	state->pred_val += sign ? -pred_diff : pred_diff;

	if (state->pred_val > 32767) {
		state->pred_val = 32767;
	} else if (state->pred_val < -32768) {
		state->pred_val = -32768;
	}

	/* Update and clamp StepSize lookup table index */
	state->step_idx += IndexAdjust[adjust_idx];

	if (state->step_idx < 0) {
		state->step_idx = 0;
	} else if (state->step_idx > 88) {
		state->step_idx = 88;
	}
	return (sign | adjust_idx);
}


static int adpcm_decoder(unsigned char code, snd_pcm_adpcm_state_t * state)
{
	short pred_diff;	/* Predicted difference to next sample */
	short step;		/* holds previous StepSize value */
	char sign;

	int i;

	/* Separate sign and magnitude */
	sign = code & 0x8;
	code &= 0x7;

	/*
	 * Computes pred_diff = (code + 0.5) * step / 4,
	 * but see comment in adpcm_coder.
	 */

	step = StepSize[state->step_idx];

	/* Compute difference and new predicted value */
	pred_diff = step >> 3;
	for (i = 0x4; i; i >>= 1, step >>= 1) {
		if (code & i) {
			pred_diff += step;
		}
	}
	state->pred_val += (sign) ? -pred_diff : pred_diff;

	/* Clamp output value */
	if (state->pred_val > 32767) {
		state->pred_val = 32767;
	} else if (state->pred_val < -32768) {
		state->pred_val = -32768;
	}

	/* Find new StepSize index value */
	state->step_idx += IndexAdjust[code];

	if (state->step_idx < 0) {
		state->step_idx = 0;
	} else if (state->step_idx > 88) {
		state->step_idx = 88;
	}
	return (state->pred_val);
}

void snd_pcm_adpcm_decode(const snd_pcm_channel_area_t *src_areas,
			  snd_pcm_uframes_t src_offset,
			  const snd_pcm_channel_area_t *dst_areas,
			  snd_pcm_uframes_t dst_offset,
			  unsigned int channels, snd_pcm_uframes_t frames, int putidx,
			  snd_pcm_adpcm_state_t *states)
{
#define PUT16_LABELS
#include "plugin_ops.h"
#undef PUT16_LABELS
	void *put = put16_labels[putidx];
	unsigned int channel;
	for (channel = 0; channel < channels; ++channel, ++states) {
		char *src;
		int srcbit;
		char *dst;
		int src_step, srcbit_step, dst_step;
		snd_pcm_uframes_t frames1;
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
		srcbit = src_area->first + src_area->step * src_offset;
		src = src_area->addr + srcbit / 8;
		srcbit %= 8;
		src_step = src_area->step / 8;
		srcbit_step = src_area->step % 8;
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		dst_step = snd_pcm_channel_area_step(dst_area);
		frames1 = frames;
		while (frames1-- > 0) {
			int16_t sample;
			int v;
			if (srcbit)
				v = *src & 0x0f;
			else
				v = (*src >> 4) & 0x0f;
			sample = adpcm_decoder(v, states);
			goto *put;
#define PUT16_END after
#include "plugin_ops.h"
#undef PUT16_END
		after:
			src += src_step;
			srcbit += srcbit_step;
			if (srcbit == 8) {
				src++;
				srcbit = 0;
			}
			dst += dst_step;
		}
	}
}

void snd_pcm_adpcm_encode(const snd_pcm_channel_area_t *src_areas,
			  snd_pcm_uframes_t src_offset,
			  const snd_pcm_channel_area_t *dst_areas,
			  snd_pcm_uframes_t dst_offset,
			  unsigned int channels, snd_pcm_uframes_t frames, int getidx,
			  snd_pcm_adpcm_state_t *states)
{
#define GET16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
	void *get = get16_labels[getidx];
	unsigned int channel;
	int16_t sample = 0;
	for (channel = 0; channel < channels; ++channel, ++states) {
		char *src;
		char *dst;
		int dstbit;
		int src_step, dst_step, dstbit_step;
		snd_pcm_uframes_t frames1;
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
		src_step = snd_pcm_channel_area_step(src_area);
		dstbit = dst_area->first + dst_area->step * dst_offset;
		dst = dst_area->addr + dstbit / 8;
		dstbit %= 8;
		dst_step = dst_area->step / 8;
		dstbit_step = dst_area->step % 8;
		frames1 = frames;
		while (frames1-- > 0) {
			int v;
			goto *get;
#define GET16_END after
#include "plugin_ops.h"
#undef GET16_END
		after:
			v = adpcm_encoder(sample, states);
			if (dstbit)
				*dst = (*dst & 0xf0) | v;
			else
				*dst = (*dst & 0x0f) | (v << 4);
			src += src_step;
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
	}
}

static int snd_pcm_adpcm_hw_refine_cprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	int err;
	mask_t *access_mask = alloca(mask_sizeof());
	mask_load(access_mask, SND_PCM_ACCBIT_PLUGIN);
	err = _snd_pcm_hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS,
				     access_mask);
	if (err < 0)
		return err;
	if (adpcm->sformat == SND_PCM_FORMAT_IMA_ADPCM) {
		mask_t *format_mask = alloca(mask_sizeof());
		mask_load(format_mask, SND_PCM_FMTBIT_LINEAR);
		err = _snd_pcm_hw_param_mask(params, SND_PCM_HW_PARAM_FORMAT,
					     format_mask);
	} else {
		err = _snd_pcm_hw_param_set(params,
					    SND_PCM_HW_PARAM_FORMAT,
					    SND_PCM_FORMAT_IMA_ADPCM, 0);
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

static int snd_pcm_adpcm_hw_refine_sprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *sparams)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	mask_t *saccess_mask = alloca(mask_sizeof());
	mask_load(saccess_mask, SND_PCM_ACCBIT_MMAP);
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				saccess_mask);
	_snd_pcm_hw_param_set(sparams, SND_PCM_HW_PARAM_FORMAT,
			      adpcm->sformat, 0);
	_snd_pcm_hw_param_set(sparams, SND_PCM_HW_PARAM_SUBFORMAT,
			      SND_PCM_SUBFORMAT_STD, 0);
	return 0;
}

static int snd_pcm_adpcm_hw_refine_schange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
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
	
static int snd_pcm_adpcm_hw_refine_cchange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
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

static int snd_pcm_adpcm_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_adpcm_hw_refine_cprepare,
				       snd_pcm_adpcm_hw_refine_cchange,
				       snd_pcm_adpcm_hw_refine_sprepare,
				       snd_pcm_adpcm_hw_refine_schange,
				       snd_pcm_plugin_hw_refine_slave);
}

static int snd_pcm_adpcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_adpcm_hw_refine_cchange,
					  snd_pcm_adpcm_hw_refine_sprepare,
					  snd_pcm_adpcm_hw_refine_schange,
					  snd_pcm_plugin_hw_params_slave);
	if (err < 0)
		return err;


	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		if (adpcm->sformat == SND_PCM_FORMAT_IMA_ADPCM) {
			adpcm->getput_idx = snd_pcm_linear_get_index(snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_FORMAT, 0), SND_PCM_FORMAT_S16);
			adpcm->func = snd_pcm_adpcm_encode;
		} else {
			adpcm->getput_idx = snd_pcm_linear_put_index(SND_PCM_FORMAT_S16, adpcm->sformat);
			adpcm->func = snd_pcm_adpcm_decode;
		}
	} else {
		if (adpcm->sformat == SND_PCM_FORMAT_IMA_ADPCM) {
			adpcm->getput_idx = snd_pcm_linear_put_index(SND_PCM_FORMAT_S16, snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_FORMAT, 0));
			adpcm->func = snd_pcm_adpcm_decode;
		} else {
			adpcm->getput_idx = snd_pcm_linear_get_index(adpcm->sformat, SND_PCM_FORMAT_S16);
			adpcm->func = snd_pcm_adpcm_encode;
		}
	}
	assert(!adpcm->states);
	adpcm->states = malloc(snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_CHANNELS, 0) * sizeof(*adpcm->states));
	return 0;
}

static int snd_pcm_adpcm_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	if (adpcm->states) {
		free(adpcm->states);
		adpcm->states = 0;
	}
	return snd_pcm_hw_free(adpcm->plug.slave);
}

static int snd_pcm_adpcm_init(snd_pcm_t *pcm)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	unsigned int k;
	for (k = 0; k < pcm->channels; ++k) {
		adpcm->states[k].pred_val = 0;
		adpcm->states[k].step_idx = 0;
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_adpcm_write_areas(snd_pcm_t *pcm,
					 const snd_pcm_channel_area_t *areas,
					 snd_pcm_uframes_t offset,
					 snd_pcm_uframes_t size,
					 snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	snd_pcm_t *slave = adpcm->plug.slave;
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_sframes_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		snd_pcm_uframes_t frames = snd_pcm_mmap_playback_xfer(slave, size - xfer);
		adpcm->func(areas, offset, 
			    snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
			    pcm->channels, frames,
			    adpcm->getput_idx, adpcm->states);
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

static snd_pcm_sframes_t snd_pcm_adpcm_read_areas(snd_pcm_t *pcm,
					const snd_pcm_channel_area_t *areas,
					snd_pcm_uframes_t offset,
					snd_pcm_uframes_t size,
					snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	snd_pcm_t *slave = adpcm->plug.slave;
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_sframes_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		snd_pcm_uframes_t frames = snd_pcm_mmap_capture_xfer(slave, size - xfer);
		adpcm->func(snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
			    areas, offset, 
			    pcm->channels, frames,
			    adpcm->getput_idx, adpcm->states);
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

static void snd_pcm_adpcm_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	snd_output_printf(out, "Ima-ADPCM conversion PCM (%s)\n", 
		snd_pcm_format_name(adpcm->sformat));
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(adpcm->plug.slave, out);
}

snd_pcm_ops_t snd_pcm_adpcm_ops = {
	close: snd_pcm_plugin_close,
	card: snd_pcm_plugin_card,
	info: snd_pcm_plugin_info,
	hw_refine: snd_pcm_adpcm_hw_refine,
	hw_params: snd_pcm_adpcm_hw_params,
	hw_free: snd_pcm_adpcm_hw_free,
	sw_params: snd_pcm_plugin_sw_params,
	channel_info: snd_pcm_plugin_channel_info,
	dump: snd_pcm_adpcm_dump,
	nonblock: snd_pcm_plugin_nonblock,
	async: snd_pcm_plugin_async,
	mmap: snd_pcm_plugin_mmap,
	munmap: snd_pcm_plugin_munmap,
};

int snd_pcm_adpcm_open(snd_pcm_t **pcmp, char *name, int sformat, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_adpcm_t *adpcm;
	assert(pcmp && slave);
	if (snd_pcm_format_linear(sformat) != 1 &&
	    sformat != SND_PCM_FORMAT_IMA_ADPCM)
		return -EINVAL;
	adpcm = calloc(1, sizeof(snd_pcm_adpcm_t));
	if (!adpcm) {
		return -ENOMEM;
	}
	adpcm->sformat = sformat;
	adpcm->plug.read = snd_pcm_adpcm_read_areas;
	adpcm->plug.write = snd_pcm_adpcm_write_areas;
	adpcm->plug.init = snd_pcm_adpcm_init;
	adpcm->plug.slave = slave;
	adpcm->plug.close_slave = close_slave;

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(adpcm);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_ADPCM;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_adpcm_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = adpcm;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = &adpcm->plug.hw_ptr;
	pcm->appl_ptr = &adpcm->plug.appl_ptr;
	*pcmp = pcm;

	return 0;
}

int _snd_pcm_adpcm_open(snd_pcm_t **pcmp, char *name,
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
			    sformat != SND_PCM_FORMAT_IMA_ADPCM) {
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
	err = snd_pcm_adpcm_open(pcmp, name, sformat, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				

