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

typedef struct adpcm_state {
	int pred_val;		/* Calculated predicted value */
	int step_idx;		/* Previous StepSize lookup index */
	unsigned int io_buffer;	/* input / output bit packing buffer */
	int io_shift;		/* shift input / output buffer */
} adpcm_state_t;

static void adpcm_init_state(adpcm_state_t * state_ptr)
{
	state_ptr->pred_val = 0;
	state_ptr->step_idx = 0;
	state_ptr->io_buffer = 0;
	state_ptr->io_shift = 4;
}

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

/*
 *  Basic Ima-ADPCM plugin
 */

typedef void (*adpcm_f)(adpcm_state_t *state, void *src_ptr, void *dst_ptr, int samples);

typedef struct adpcm_private_data {
	adpcm_f func;
	adpcm_state_t state;
} adpcm_t;

#define ADPCM_FUNC_DECODE(name, dsttype, val) \
static void adpcm_decode_##name(adpcm_state_t *state, \
				void *src_ptr, void *dst_ptr, int samples) \
{ \
	unsigned char *src = src_ptr; \
	dsttype *dst = dst_ptr; \
	unsigned int s; \
	samples <<= 1; \
	while (samples--) { \
		if (state->io_shift) \
			state->io_buffer = *src++; \
		s = adpcm_decoder((state->io_buffer >> state->io_shift) & 0x0f, state); \
		*dst++ = val; \
		state->io_shift ^= 4; \
	} \
}

#define ADPCM_FUNC_ENCODE(name, srctype, val) \
static void adpcm_encode_##name(adpcm_state_t *state, \
				void *src_ptr, void *dst_ptr, int samples) \
{ \
	srctype *src = src_ptr; \
	unsigned char *dst = dst_ptr; \
	unsigned int s; \
	samples <<= 1; \
	while (samples--) { \
		s = *src++; \
		state->io_buffer |= adpcm_encoder((signed short)(val), state) << state->io_shift; \
		if (state->io_shift == 0) { \
			*dst++ = state->io_buffer & 0xff; \
			state->io_buffer = 0; \
		} \
		state->io_shift ^= 4; \
	} \
}

ADPCM_FUNC_DECODE(u8, u_int8_t, (s >> 8) ^ 0x80)
ADPCM_FUNC_DECODE(s8, u_int8_t, s >> 8)
ADPCM_FUNC_DECODE(u16n, u_int16_t, s ^ 0x8000)
ADPCM_FUNC_DECODE(u16s, u_int16_t, bswap_16(s ^ 0x8000))
ADPCM_FUNC_DECODE(s16n, u_int16_t, s)
ADPCM_FUNC_DECODE(s16s, u_int16_t, bswap_16(s))
ADPCM_FUNC_DECODE(u24n, u_int32_t, (s << 8) ^ 0x800000)
ADPCM_FUNC_DECODE(u24s, u_int32_t, bswap_32((s << 8) ^ 0x800000))
ADPCM_FUNC_DECODE(s24n, u_int32_t, s << 8)
ADPCM_FUNC_DECODE(s24s, u_int32_t, bswap_32(s << 8))
ADPCM_FUNC_DECODE(u32n, u_int32_t, (s << 16) ^ 0x80000000)
ADPCM_FUNC_DECODE(u32s, u_int32_t, bswap_32((s << 16) ^ 0x80000000))
ADPCM_FUNC_DECODE(s32n, u_int32_t, s << 16)
ADPCM_FUNC_DECODE(s32s, u_int32_t, bswap_32(s << 16))

ADPCM_FUNC_ENCODE(u8, u_int8_t, s << 8)
ADPCM_FUNC_ENCODE(s8, u_int8_t, (s << 8) ^ 0x8000)
ADPCM_FUNC_ENCODE(u16n, u_int16_t, s ^ 0x8000)
ADPCM_FUNC_ENCODE(u16s, u_int16_t, bswap_16(s ^ 0x8000))
ADPCM_FUNC_ENCODE(s16n, u_int16_t, s)
ADPCM_FUNC_ENCODE(s16s, u_int16_t, bswap_16(s))
ADPCM_FUNC_ENCODE(u24n, u_int32_t, (s ^ 0x800000) >> 8)
ADPCM_FUNC_ENCODE(u24s, u_int32_t, bswap_32((s ^ 0x800000) >> 8))
ADPCM_FUNC_ENCODE(s24n, u_int32_t, s >> 8)
ADPCM_FUNC_ENCODE(s24s, u_int32_t, bswap_32(s >> 8))
ADPCM_FUNC_ENCODE(u32n, u_int32_t, (s ^ 0x80000000) >> 16)
ADPCM_FUNC_ENCODE(u32s, u_int32_t, bswap_32((s ^ 0x80000000) >> 16))
ADPCM_FUNC_ENCODE(s32n, u_int32_t, s >> 16)
ADPCM_FUNC_ENCODE(s32s, u_int32_t, bswap_32(s >> 16))

/* wide, sign, swap endian */
static adpcm_f adpcm_functions_decode[4 * 4 * 2 * 2] = {
	adpcm_decode_u8,	/* decode:8-bit:unsigned:none */
	adpcm_decode_u8,	/* decode:8-bit:unsigned:swap */
	adpcm_decode_s8,	/* decode:8-bit:signed:none */
	adpcm_decode_s8,	/* decode:8-bit:signed:swap */
	adpcm_decode_u16n,	/* decode:16-bit:unsigned:none */
	adpcm_decode_u16s,	/* decode:16-bit:unsigned:swap */
	adpcm_decode_s16n,	/* decode:16-bit:signed:none */
	adpcm_decode_s16s,	/* decode:16-bit:signed:swap */
	adpcm_decode_u24n,	/* decode:24-bit:unsigned:none */
	adpcm_decode_u24s,	/* decode:24-bit:unsigned:swap */
	adpcm_decode_s24n,	/* decode:24-bit:signed:none */
	adpcm_decode_s24s,	/* decode:24-bit:signed:swap */
	adpcm_decode_u32n,	/* decode:32-bit:unsigned:none */
	adpcm_decode_u32s,	/* decode:32-bit:unsigned:swap */
	adpcm_decode_s32n,	/* decode:32-bit:signed:none */
	adpcm_decode_s32s,	/* decode:32-bit:signed:swap */
};

/* wide, sign, swap endian */
static adpcm_f adpcm_functions_encode[4 * 2 * 2] = {
	adpcm_encode_u8,	/* encode:8-bit:unsigned:none */
	adpcm_encode_u8,	/* encode:8-bit:unsigned:swap */
	adpcm_encode_s8,	/* encode:8-bit:signed:none */
	adpcm_encode_s8,	/* encode:8-bit:signed:swap */
	adpcm_encode_u16n,	/* encode:16-bit:unsigned:none */
	adpcm_encode_u16s,	/* encode:16-bit:unsigned:swap */
	adpcm_encode_s16n,	/* encode:16-bit:signed:none */
	adpcm_encode_s16s,	/* encode:16-bit:signed:swap */
	adpcm_encode_u24n,	/* encode:24-bit:unsigned:none */
	adpcm_encode_u24s,	/* encode:24-bit:unsigned:swap */
	adpcm_encode_s24n,	/* encode:24-bit:signed:none */
	adpcm_encode_s24s,	/* encode:24-bit:signed:swap */
	adpcm_encode_u32n,	/* encode:32-bit:unsigned:none */
	adpcm_encode_u32s,	/* encode:32-bit:unsigned:swap */
	adpcm_encode_s32n,	/* encode:32-bit:signed:none */
	adpcm_encode_s32s,	/* encode:32-bit:signed:swap */
};

static ssize_t adpcm_transfer(snd_pcm_plugin_t *plugin,
			      const snd_pcm_plugin_voice_t *src_voices,
			      const snd_pcm_plugin_voice_t *dst_voices,
			      size_t samples)
{
	adpcm_t *data;
	int voice;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL || samples < 0)
		return -EINVAL;
	if (samples == 0)
		return 0;
	data = (adpcm_t *)plugin->extra_data;
	/* FIXME */
	if (plugin->src_format.interleave) {
		data->func(&data->state,
			   src_voices[0].addr,
			   dst_voices[0].addr,
			   samples * plugin->src_format.voices);
	} else {
		for (voice = 0; voice < plugin->src_format.voices; voice++) {
			if (src_voices[voice].addr == NULL)
				continue;
			data->func(&data->state,
				   src_voices[voice].addr,
				   dst_voices[voice].addr,
				   samples);
		}
	}
	return samples;
}

static int adpcm_action(snd_pcm_plugin_t * plugin,
			snd_pcm_plugin_action_t action,
			unsigned long udata)
{
	adpcm_t *data;

	if (plugin == NULL)
		return -EINVAL;
	data = (adpcm_t *)plugin->extra_data;
	if (action == PREPARE)
		adpcm_init_state(&data->state);
	return 0;		/* silenty ignore other actions */
}

int snd_pcm_plugin_build_adpcm(snd_pcm_plugin_handle_t *handle,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin)
{
	struct adpcm_private_data *data;
	snd_pcm_plugin_t *plugin;
	int endian, src_width, dst_width, sign;
	adpcm_f func;

	if (!r_plugin || !src_format || !dst_format)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->interleave != dst_format->interleave && 
	    src_format->voices > 1)
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;

	if (dst_format->format == SND_PCM_SFMT_IMA_ADPCM) {
		if (!snd_pcm_format_linear(src_format->format))
			return -EINVAL;
		sign = snd_pcm_format_signed(src_format->format);
		src_width = snd_pcm_format_width(src_format->format);
		if ((src_width % 8) != 0 || src_width < 8 || src_width > 32)
			return -EINVAL;
		dst_width = 8;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		endian = snd_pcm_format_big_endian(src_format->format);
#elif __BYTE_ORDER == __BIG_ENDIAN
		endian = snd_pcm_format_little_endian(src_format->format);
#else
#error "Unsupported endian..."
#endif
		func = ((adpcm_f(*)[2][2])adpcm_functions_encode)[src_width/8][sign][endian];
	} else if (src_format->format == SND_PCM_SFMT_IMA_ADPCM) {
		if (!snd_pcm_format_linear(dst_format->format))
			return -EINVAL;
		sign = snd_pcm_format_signed(dst_format->format);
		dst_width = snd_pcm_format_width(dst_format->format);
		if ((dst_width % 8) != 0 || dst_width < 8 || dst_width > 32)
			return -EINVAL;
		src_width = 8;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		endian = snd_pcm_format_big_endian(dst_format->format);
#elif __BYTE_ORDER == __BIG_ENDIAN
		endian = snd_pcm_format_little_endian(dst_format->format);
#else
#error "Unsupported endian..."
#endif
		func = ((adpcm_f(*)[2][2])adpcm_functions_decode)[dst_width/8][sign][endian];
	} else {
		return -EINVAL;
	}
	plugin = snd_pcm_plugin_build(handle,
				      "Ima-ADPCM<->linear conversion",
				      src_format,
				      dst_format,
				      sizeof(struct adpcm_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (adpcm_t *)plugin->extra_data;
	plugin->transfer = adpcm_transfer;
	plugin->action = adpcm_action;
	*r_plugin = plugin;
	return 0;
}
