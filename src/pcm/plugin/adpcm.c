/*
 *  Ima-ADPCM conversion Plug-In Interface
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
Proposal for Standardized Audio Interchange Formats,
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include "../pcm_local.h"

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

typedef struct {
	int pred_val;		/* Calculated predicted value */
	int step_idx;		/* Previous StepSize lookup index */
} adpcm_voice_t;

typedef void (*adpcm_f)(snd_pcm_plugin_t *plugin,
			const snd_pcm_plugin_voice_t *src_voices,
			const snd_pcm_plugin_voice_t *dst_voices,
			size_t samples);

typedef struct adpcm_private_data {
	adpcm_f func;
	int conv;
	adpcm_voice_t voices[0];
} adpcm_t;


static void adpcm_init(snd_pcm_plugin_t *plugin)
{
	int voice;
	adpcm_t *data = (adpcm_t *)plugin->extra_data;
	for (voice = 0; voice < plugin->src_format.voices; voice++) {
		adpcm_voice_t *v = &data->voices[voice];
		v->pred_val = 0;
		v->step_idx = 0;
	}
}

static char adpcm_encoder(int sl, adpcm_voice_t * state)
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


static int adpcm_decoder(unsigned char code, adpcm_voice_t * state)
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

/*
 *  Basic Ima-ADPCM plugin
 */

static void adpcm_decode(snd_pcm_plugin_t *plugin,
			 const snd_pcm_plugin_voice_t *src_voices,
			 const snd_pcm_plugin_voice_t *dst_voices,
			 size_t samples)
{
#define PUT16_LABELS
#include "plugin_ops.h"
#undef PUT16_LABELS
	adpcm_t *data = (adpcm_t *)plugin->extra_data;
	void *put = put16_labels[data->conv];
	int voice;
	int nvoices = plugin->src_format.voices;
	for (voice = 0; voice < nvoices; ++voice) {
		char *src;
		int srcbit;
		char *dst;
		int src_step, srcbit_step, dst_step;
		size_t samples1;
		adpcm_voice_t *state;
		if (src_voices[voice].addr == NULL) {
			if (dst_voices[voice].addr != NULL) {
//				null_voice(&dst_voices[voice]);
				zero_voice(plugin, &dst_voices[voice], samples);
			}
			continue;
		}
		src = src_voices[voice].addr + src_voices[voice].first / 8;
		srcbit = src_voices[voice].first % 8;
		dst = dst_voices[voice].addr + dst_voices[voice].first / 8;
		src_step = src_voices[voice].step / 8;
		srcbit_step = src_voices[voice].step % 8;
		dst_step = dst_voices[voice].step / 8;
		state = &data->voices[voice];
		samples1 = samples;
		while (samples1-- > 0) {
			signed short sample;
			int v;
			if (srcbit)
				v = *src & 0x0f;
			else
				v = (*src >> 4) & 0x0f;
			sample = adpcm_decoder(v, state);
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

static void adpcm_encode(snd_pcm_plugin_t *plugin,
			const snd_pcm_plugin_voice_t *src_voices,
			const snd_pcm_plugin_voice_t *dst_voices,
			size_t samples)
{
#define GET16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
	adpcm_t *data = (adpcm_t *)plugin->extra_data;
	void *get = get16_labels[data->conv];
	int voice;
	int nvoices = plugin->src_format.voices;
	signed short sample = 0;
	for (voice = 0; voice < nvoices; ++voice) {
		char *src;
		char *dst;
		int dstbit;
		int src_step, dst_step, dstbit_step;
		size_t samples1;
		adpcm_voice_t *state;
		if (src_voices[voice].addr == NULL) {
			if (dst_voices[voice].addr != NULL) {
//				null_voice(&dst_voices[voice]);
				zero_voice(plugin, &dst_voices[voice], samples);
			}
			continue;
		}
		src = src_voices[voice].addr + src_voices[voice].first / 8;
		dst = dst_voices[voice].addr + dst_voices[voice].first / 8;
		dstbit = dst_voices[voice].first % 8;
		src_step = src_voices[voice].step / 8;
		dst_step = dst_voices[voice].step / 8;
		dstbit_step = dst_voices[voice].step % 8;
		state = &data->voices[voice];
		samples1 = samples;
		while (samples1-- > 0) {
			int v;
			goto *get;
#define GET16_END after
#include "plugin_ops.h"
#undef GET16_END
		after:
			v = adpcm_encoder(sample, state);
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

static ssize_t adpcm_transfer(snd_pcm_plugin_t *plugin,
			      const snd_pcm_plugin_voice_t *src_voices,
			      const snd_pcm_plugin_voice_t *dst_voices,
			      size_t samples)
{
	adpcm_t *data;
	int voice;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL)
		return -EFAULT;
	if (samples < 0)
		return -EINVAL;
	if (samples == 0)
		return 0;
	for (voice = 0; voice < plugin->src_format.voices; voice++) {
		if (src_voices[voice].addr != NULL && 
		    dst_voices[voice].addr == NULL)
			return -EFAULT;
		if (plugin->src_format.format == SND_PCM_SFMT_IMA_ADPCM) {
			if (src_voices[voice].first % 4 != 0 ||
			    src_voices[voice].step % 4 != 0 ||
			    dst_voices[voice].first % 8 != 0 ||
			    dst_voices[voice].step % 8 != 0)
				return -EINVAL;
		} else {
			if (src_voices[voice].first % 8 != 0 ||
			    src_voices[voice].step % 8 != 0 ||
			    dst_voices[voice].first % 4 != 0 ||
			    dst_voices[voice].step % 4 != 0)
				return -EINVAL;
		}
	}
	data = (adpcm_t *)plugin->extra_data;
	data->func(plugin, src_voices, dst_voices, samples);
	return samples;
}

static int adpcm_action(snd_pcm_plugin_t * plugin,
			snd_pcm_plugin_action_t action,
			unsigned long udata)
{
	if (plugin == NULL)
		return -EINVAL;
	switch (action) {
	case INIT:
	case PREPARE:
	case DRAIN:
	case FLUSH:
		adpcm_init(plugin);
		break;
	}
	return 0;	/* silenty ignore other actions */
}

int snd_pcm_plugin_build_adpcm(snd_pcm_plugin_handle_t *handle,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin)
{
	struct adpcm_private_data *data;
	snd_pcm_plugin_t *plugin;
	snd_pcm_format_t *format;
	adpcm_f func;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;

	if (dst_format->format == SND_PCM_SFMT_IMA_ADPCM) {
		format = src_format;
		func = adpcm_encode;
	}
	else if (src_format->format == SND_PCM_SFMT_IMA_ADPCM) {
		format = dst_format;
		func = adpcm_decode;
	}
	else
		return -EINVAL;
	if (!snd_pcm_format_linear(format->format))
		return -EINVAL;

	plugin = snd_pcm_plugin_build(handle,
				      "Ima-ADPCM<->linear conversion",
				      src_format,
				      dst_format,
				      sizeof(adpcm_t) + src_format->voices * sizeof(adpcm_voice_t));
	if (plugin == NULL)
		return -ENOMEM;
	data = (adpcm_t *)plugin->extra_data;
	data->func = func;
	data->conv = getput_index(format->format);
	plugin->transfer = adpcm_transfer;
	plugin->action = adpcm_action;
	*r_plugin = plugin;
	return 0;
}
