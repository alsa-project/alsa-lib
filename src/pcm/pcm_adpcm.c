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

typedef struct {
	int pred_val;		/* Calculated predicted value */
	int step_idx;		/* Previous StepSize lookup index */
} adpcm_state_t;

typedef void (*adpcm_f)(snd_pcm_channel_area_t *src_areas,
			size_t src_offset,
			snd_pcm_channel_area_t *dst_areas,
			size_t dst_offset,
			size_t frames, size_t channels, int getputidx,
			adpcm_state_t *states);

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	int getput_idx;
	adpcm_f func;
	int sformat;
	int cformat;
	int cxfer_mode, cmmap_shape;
	adpcm_state_t *states;
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

static char adpcm_encoder(int sl, adpcm_state_t * state)
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


static int adpcm_decoder(unsigned char code, adpcm_state_t * state)
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

static void adpcm_decode(snd_pcm_channel_area_t *src_areas,
			 size_t src_offset,
			 snd_pcm_channel_area_t *dst_areas,
			 size_t dst_offset,
			 size_t frames, size_t channels, int putidx,
			 adpcm_state_t *states)
{
#define PUT_S16_LABELS
#include "plugin_ops.h"
#undef PUT_S16_LABELS
	void *put = put_s16_labels[putidx];
	size_t channel;
	for (channel = 0; channel < channels; ++channel, ++states) {
		char *src;
		int srcbit;
		char *dst;
		int src_step, srcbit_step, dst_step;
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
#define PUT_S16_END after
#include "plugin_ops.h"
#undef PUT_S16_END
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

static void adpcm_encode(snd_pcm_channel_area_t *src_areas,
			 size_t src_offset,
			 snd_pcm_channel_area_t *dst_areas,
			 size_t dst_offset,
			 size_t frames, size_t channels, int getidx,
			 adpcm_state_t *states)
{
#define GET_S16_LABELS
#include "plugin_ops.h"
#undef GET_S16_LABELS
	void *get = get_s16_labels[getidx];
	size_t channel;
	int16_t sample = 0;
	for (channel = 0; channel < channels; ++channel, ++states) {
		char *src;
		char *dst;
		int dstbit;
		int src_step, dst_step, dstbit_step;
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
#define GET_S16_END after
#include "plugin_ops.h"
#undef GET_S16_END
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

static int snd_pcm_adpcm_close(snd_pcm_t *pcm)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	int err = 0;
	if (adpcm->plug.close_slave)
		err = snd_pcm_close(adpcm->plug.slave);
	if (adpcm->states)
		free(adpcm->states);
	free(adpcm);
	return 0;
}

static int snd_pcm_adpcm_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t * info)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	unsigned int req_mask = info->req_mask;
	unsigned int sfmt = info->req.format.sfmt;
	int err;
	if (req_mask & SND_PCM_PARAMS_SFMT) {
		if (adpcm->sformat == SND_PCM_SFMT_IMA_ADPCM ?
		    !snd_pcm_format_linear(sfmt) :
		    sfmt != SND_PCM_SFMT_IMA_ADPCM) {
			info->req.fail_mask = SND_PCM_PARAMS_SFMT;
			info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
			return -EINVAL;
		}
	}
	info->req_mask |= SND_PCM_PARAMS_SFMT;
	info->req_mask &= ~(SND_PCM_PARAMS_MMAP_SHAPE | 
			    SND_PCM_PARAMS_XFER_MODE);
	info->req.format.sfmt = adpcm->sformat;
	err = snd_pcm_params_info(adpcm->plug.slave, info);
	info->req_mask = req_mask;
	info->req.format.sfmt = sfmt;
	if (err < 0)
		return err;
	if (req_mask & SND_PCM_PARAMS_SFMT)
		info->formats = 1 << sfmt;
	else
		info->formats = adpcm->sformat == SND_PCM_SFMT_IMA_ADPCM ?
			SND_PCM_LINEAR_FORMATS : 1 << SND_PCM_SFMT_IMA_ADPCM;
	info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	info->flags |= SND_PCM_INFO_INTERLEAVED | SND_PCM_INFO_NONINTERLEAVED;
	return err;
}

static int snd_pcm_adpcm_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	snd_pcm_t *slave = adpcm->plug.slave;
	int err;
	if (adpcm->sformat == SND_PCM_SFMT_IMA_ADPCM ?
	    !snd_pcm_format_linear(params->format.sfmt) :
	    params->format.sfmt != SND_PCM_SFMT_IMA_ADPCM) {
		params->fail_mask = SND_PCM_PARAMS_SFMT;
		params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	adpcm->cformat = params->format.sfmt;
	adpcm->cxfer_mode = params->xfer_mode;
	adpcm->cmmap_shape = params->mmap_shape;
	params->format.sfmt = adpcm->sformat;
	params->xfer_mode = SND_PCM_XFER_UNSPECIFIED;
	params->mmap_shape = SND_PCM_MMAP_UNSPECIFIED;;
	err = snd_pcm_params_mmap(slave, params);
	params->format.sfmt = adpcm->cformat;
	params->xfer_mode = adpcm->cxfer_mode;
	params->mmap_shape = adpcm->cmmap_shape;
	return err;
}

static int snd_pcm_adpcm_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	int err = snd_pcm_setup(adpcm->plug.slave, setup);
	if (err < 0)
		return err;
	assert(adpcm->sformat == setup->format.sfmt);
	if (adpcm->cxfer_mode == SND_PCM_XFER_UNSPECIFIED)
		setup->xfer_mode = SND_PCM_XFER_NONINTERLEAVED;
	else
		setup->xfer_mode = adpcm->cxfer_mode;
	if (adpcm->cmmap_shape == SND_PCM_MMAP_UNSPECIFIED)
		setup->mmap_shape = SND_PCM_MMAP_NONINTERLEAVED;
	else
		setup->mmap_shape = adpcm->cmmap_shape;
	setup->format.sfmt = adpcm->cformat;
	setup->mmap_bytes = 0;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		if (adpcm->sformat == SND_PCM_SFMT_IMA_ADPCM) {
			adpcm->getput_idx = getput_index(adpcm->cformat);
			adpcm->func = adpcm_encode;
		} else {
			adpcm->getput_idx = getput_index(adpcm->sformat);
			adpcm->func = adpcm_decode;
		}
	} else {
		if (adpcm->sformat == SND_PCM_SFMT_IMA_ADPCM) {
			adpcm->getput_idx = getput_index(adpcm->cformat);
			adpcm->func = adpcm_decode;
		} else {
			adpcm->getput_idx = getput_index(adpcm->sformat);
			adpcm->func = adpcm_encode;
		}
	}
	if (adpcm->states)
		free(adpcm->states);
	adpcm->states = malloc(setup->format.channels * sizeof(*adpcm->states));
	return 0;
}

static int snd_pcm_adpcm_init(snd_pcm_t *pcm)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	unsigned int k;
	for (k = 0; k < pcm->setup.format.channels; ++k) {
		adpcm->states[k].pred_val = 0;
		adpcm->states[k].step_idx = 0;
	}
	return 0;
}

static ssize_t snd_pcm_adpcm_write_areas(snd_pcm_t *pcm,
					 snd_pcm_channel_area_t *areas,
					 size_t offset,
					 size_t size,
					 size_t *slave_sizep)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	snd_pcm_t *slave = adpcm->plug.slave;
	size_t xfer = 0;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_playback_xfer(slave, size - xfer);
		adpcm->func(areas, offset, 
			    snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
			    frames, pcm->setup.format.channels,
			    adpcm->getput_idx, adpcm->states);
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

static ssize_t snd_pcm_adpcm_read_areas(snd_pcm_t *pcm,
					snd_pcm_channel_area_t *areas,
					size_t offset,
					size_t size,
					size_t *slave_sizep)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	snd_pcm_t *slave = adpcm->plug.slave;
	size_t xfer = 0;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_capture_xfer(slave, size - xfer);
		adpcm->func(snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
			    areas, offset, 
			    frames, pcm->setup.format.channels,
			    adpcm->getput_idx, adpcm->states);
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

static void snd_pcm_adpcm_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_adpcm_t *adpcm = pcm->private;
	fprintf(fp, "Ima-ADPCM conversion PCM (%s)\n", 
		snd_pcm_format_name(adpcm->sformat));
	if (pcm->valid_setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(adpcm->plug.slave, fp);
}

snd_pcm_ops_t snd_pcm_adpcm_ops = {
	close: snd_pcm_adpcm_close,
	info: snd_pcm_plugin_info,
	params_info: snd_pcm_adpcm_params_info,
	params: snd_pcm_adpcm_params,
	setup: snd_pcm_adpcm_setup,
	channel_info: snd_pcm_plugin_channel_info,
	channel_params: snd_pcm_plugin_channel_params,
	channel_setup: snd_pcm_plugin_channel_setup,
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
	    sformat != SND_PCM_SFMT_IMA_ADPCM)
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
			    sformat != SND_PCM_SFMT_IMA_ADPCM)
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
	err = snd_pcm_adpcm_open(pcmp, name, sformat, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				

