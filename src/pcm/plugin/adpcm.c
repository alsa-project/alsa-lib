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

static inline char adpcm_encoder(int sl, adpcm_state_t * state)
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


static inline int adpcm_decoder(unsigned char code, adpcm_state_t * state)
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

typedef enum {
	_S8_ADPCM,
	_U8_ADPCM,
	_S16LE_ADPCM,
	_U16LE_ADPCM,
	_S16BE_ADPCM,
	_U16BE_ADPCM,
	_ADPCM_S8,
	_ADPCM_U8,
	_ADPCM_S16LE,
	_ADPCM_U16LE,
	_ADPCM_S16BE,
	_ADPCM_U16BE
} combination_t;

struct adpcm_private_data {
	combination_t cmd;
	adpcm_state_t state;
};

static void adpcm_conv_u8bit_adpcm(adpcm_state_t * state_ptr, unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	unsigned int pcm;

	while (size-- > 0) {
		pcm = ((*src_ptr++) ^ 0x80) << 8;

		state_ptr->io_buffer |= adpcm_encoder((signed short)(pcm), state_ptr) << state_ptr->io_shift;
		if (!(state_ptr->io_shift)) {
			*dst_ptr++ = state_ptr->io_buffer & 0xff;
			state_ptr->io_buffer = 0;
		}
		state_ptr->io_shift ^= 4;
	}
	if (!(state_ptr->io_shift)) {
		*dst_ptr = state_ptr->io_buffer & 0xf0;
	}
}

static void adpcm_conv_s8bit_adpcm(adpcm_state_t * state_ptr, unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	unsigned int pcm;

	while (size-- > 0) {
		pcm = *src_ptr++ << 8;

		state_ptr->io_buffer |= adpcm_encoder((signed short)(pcm), state_ptr) << state_ptr->io_shift;
		if (!(state_ptr->io_shift)) {
			*dst_ptr++ = state_ptr->io_buffer & 0xff;
			state_ptr->io_buffer = 0;
		}
		state_ptr->io_shift ^= 4;
	}
	if (!(state_ptr->io_shift)) {
		*dst_ptr = state_ptr->io_buffer & 0xf0;
	}
}

static void adpcm_conv_s16bit_adpcm(adpcm_state_t * state_ptr, unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0) {
		state_ptr->io_buffer |= adpcm_encoder((signed short)(*src_ptr++), state_ptr) << state_ptr->io_shift;
		if (!(state_ptr->io_shift)) {
			*dst_ptr++ = state_ptr->io_buffer & 0xff;
			state_ptr->io_buffer = 0;
		}
		state_ptr->io_shift ^= 4;
	}
	if (!(state_ptr->io_shift)) {
		*dst_ptr = state_ptr->io_buffer & 0xf0;
	}
}

static void adpcm_conv_s16bit_swap_adpcm(adpcm_state_t * state_ptr, unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0) {
		state_ptr->io_buffer |= adpcm_encoder((signed short)(bswap_16(*src_ptr++)), state_ptr) << state_ptr->io_shift;
		if (!(state_ptr->io_shift)) {
			*dst_ptr++ = state_ptr->io_buffer & 0xff;
			state_ptr->io_buffer = 0;
		}
		state_ptr->io_shift ^= 4;
	}
	if (!(state_ptr->io_shift)) {
		*dst_ptr = state_ptr->io_buffer & 0xf0;
	}
}

static void adpcm_conv_u16bit_adpcm(adpcm_state_t * state_ptr, unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0) {
		state_ptr->io_buffer |= adpcm_encoder((signed short)((*src_ptr++) ^ 0x8000), state_ptr) << state_ptr->io_shift;
		if (!(state_ptr->io_shift)) {
			*dst_ptr++ = state_ptr->io_buffer & 0xff;
			state_ptr->io_buffer = 0;
		}
		state_ptr->io_shift ^= 4;
	}
	if (!(state_ptr->io_shift)) {
		*dst_ptr = state_ptr->io_buffer & 0xf0;
	}
}

static void adpcm_conv_u16bit_swap_adpcm(adpcm_state_t * state_ptr, unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0) {
		state_ptr->io_buffer |= adpcm_encoder((signed short)(bswap_16(*src_ptr++) ^ 0x8000), state_ptr) << state_ptr->io_shift;
		if (!(state_ptr->io_shift)) {
			*dst_ptr++ = state_ptr->io_buffer & 0xff;
			state_ptr->io_buffer = 0;
		}
		state_ptr->io_shift ^= 4;
	}
	if (!(state_ptr->io_shift)) {
		*dst_ptr = state_ptr->io_buffer & 0xf0;
	}
}

static void adpcm_conv_adpcm_u8bit(adpcm_state_t * state_ptr, unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0) {
		if (state_ptr->io_shift) {
			state_ptr->io_buffer = *src_ptr++;
		}
		*dst_ptr++ = (adpcm_decoder((state_ptr->io_buffer >> state_ptr->io_shift) & 0xf, state_ptr) >> 8) ^ 0x80;
		state_ptr->io_shift ^= 4;
	}
}

static void adpcm_conv_adpcm_s8bit(adpcm_state_t * state_ptr, unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0) {
		if (state_ptr->io_shift) {
			state_ptr->io_buffer = *src_ptr++;
		}
		*dst_ptr++ = adpcm_decoder((state_ptr->io_buffer >> state_ptr->io_shift) & 0xf, state_ptr) >> 8;
		state_ptr->io_shift ^= 4;
	}
}

static void adpcm_conv_adpcm_s16bit(adpcm_state_t * state_ptr, unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0) {
		if (state_ptr->io_shift) {
			state_ptr->io_buffer = *src_ptr++;
		}
		*dst_ptr++ = adpcm_decoder((state_ptr->io_buffer >> state_ptr->io_shift) & 0xf, state_ptr);
		state_ptr->io_shift ^= 4;
	}
}

static void adpcm_conv_adpcm_swap_s16bit(adpcm_state_t * state_ptr, unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0) {
		if (state_ptr->io_shift) {
			state_ptr->io_buffer = *src_ptr++;
		}
		*dst_ptr++ = bswap_16(adpcm_decoder((state_ptr->io_buffer >> state_ptr->io_shift) & 0xf, state_ptr));
		state_ptr->io_shift ^= 4;
	}
}

static void adpcm_conv_adpcm_u16bit(adpcm_state_t * state_ptr, unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0) {
		if (state_ptr->io_shift) {
			state_ptr->io_buffer = *src_ptr++;
		}
		*dst_ptr++ = adpcm_decoder((state_ptr->io_buffer >> state_ptr->io_shift) & 0xf, state_ptr) ^ 0x8000;
		state_ptr->io_shift ^= 4;
	}
}

static void adpcm_conv_adpcm_swap_u16bit(adpcm_state_t * state_ptr, unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0) {
		if (state_ptr->io_shift) {
			state_ptr->io_buffer = *src_ptr++;
		}
		*dst_ptr++ = bswap_16(adpcm_decoder((state_ptr->io_buffer >> state_ptr->io_shift) & 0xf, state_ptr) ^ 0x8000);
		state_ptr->io_shift ^= 4;
	}
}

static ssize_t adpcm_transfer(snd_pcm_plugin_t * plugin,
			      char *src_ptr, size_t src_size,
			      char *dst_ptr, size_t dst_size)
{
	struct adpcm_private_data *data;

	if (plugin == NULL || src_ptr == NULL || src_size < 0 ||
	                      dst_ptr == NULL || dst_size < 0)
		return -EINVAL;
	if (src_size == 0)
		return 0;
	data = (struct adpcm_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	switch (data->cmd) {
	case _U8_ADPCM:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
		adpcm_conv_u8bit_adpcm(&data->state, src_ptr, dst_ptr, src_size);
		return src_size >> 1;
	case _S8_ADPCM:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
		adpcm_conv_s8bit_adpcm(&data->state, src_ptr, dst_ptr, src_size);
		return src_size >> 1;
	case _S16LE_ADPCM:
		if ((dst_size << 2) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		adpcm_conv_s16bit_adpcm(&data->state, (short *) src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		adpcm_conv_s16bit_swap_adpcm(&data->state, (short *) src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 2;
	case _U16LE_ADPCM:
		if ((dst_size << 2) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		adpcm_conv_u16bit_adpcm(&data->state, (short *) src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		adpcm_conv_u16bit_swap_adpcm(&data->state, (short *) src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 2;
	case _S16BE_ADPCM:
		if ((dst_size << 2) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		adpcm_conv_s16bit_swap_adpcm(&data->state, (short *) src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		adpcm_conv_s16bit_adpcm(&data->state, (short *) src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 2;
	case _U16BE_ADPCM:
		if ((dst_size << 2) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		adpcm_conv_u16bit_swap_adpcm(&data->state, (short *) src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		adpcm_conv_u16bit_adpcm(&data->state, (short *) src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 2;
	case _ADPCM_U8:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
		adpcm_conv_adpcm_u8bit(&data->state, src_ptr, dst_ptr, src_size << 1);
		return src_size << 1;
	case _ADPCM_S8:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
		adpcm_conv_adpcm_s8bit(&data->state, src_ptr, dst_ptr, src_size << 1);
		return src_size << 1;
	case _ADPCM_S16LE:
		if ((dst_size >> 2) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		adpcm_conv_adpcm_s16bit(&data->state, src_ptr, (short *) dst_ptr, src_size << 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		adpcm_conv_adpcm_swap_s16bit(&data->state, src_ptr, (short *) dst_ptr, src_size << 1);
#else
#error "Have to be coded..."
#endif
		return src_size << 2;
	case _ADPCM_U16LE:
		if ((dst_size >> 2) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		adpcm_conv_adpcm_u16bit(&data->state, src_ptr, (short *) dst_ptr, src_size << 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		adpcm_conv_adpcm_swap_u16bit(&data->state, src_ptr, (short *) dst_ptr, src_size << 1);
#else
#error "Have to be coded..."
#endif
		return src_size << 2;
	case _ADPCM_S16BE:
		if ((dst_size >> 2) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		adpcm_conv_adpcm_swap_s16bit(&data->state, src_ptr, (short *) dst_ptr, src_size << 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		adpcm_conv_adpcm_s16bit(&data->state, src_ptr, (short *) dst_ptr, src_size << 1);
#else
#error "Have to be coded..."
#endif
		return src_size << 2;
	case _ADPCM_U16BE:
		if ((dst_size << 2) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		adpcm_conv_adpcm_swap_u16bit(&data->state, src_ptr, (short *) dst_ptr, src_size << 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		adpcm_conv_adpcm_u16bit(&data->state, src_ptr, (short *) dst_ptr, src_size << 1);
#else
#error "Have to be coded..."
#endif
		return src_size << 2;
	default:
		return -EIO;
	}
}

static int adpcm_action(snd_pcm_plugin_t * plugin, snd_pcm_plugin_action_t action)
{
	struct adpcm_private_data *data;

	if (plugin == NULL)
		return -EINVAL;
	data = (struct adpcm_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (action == PREPARE)
		adpcm_init_state(&data->state);
	return 0;		/* silenty ignore other actions */
}

static ssize_t adpcm_src_size(snd_pcm_plugin_t * plugin, size_t size)
{
	struct adpcm_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct adpcm_private_data *)snd_pcm_plugin_extra_data(plugin);
	switch (data->cmd) {
	case _U8_ADPCM:
	case _S8_ADPCM:
		return size * 2;
	case _ADPCM_U8:
	case _ADPCM_S8:
		return size / 2;
	case _U16LE_ADPCM:
	case _S16LE_ADPCM:
	case _U16BE_ADPCM:
	case _S16BE_ADPCM:
		return size * 4;
	case _ADPCM_U16LE:
	case _ADPCM_S16LE:
	case _ADPCM_U16BE:
	case _ADPCM_S16BE:
		return size / 4;
	default:
		return -EIO;
	}
}

static ssize_t adpcm_dst_size(snd_pcm_plugin_t * plugin, size_t size)
{
	struct adpcm_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct adpcm_private_data *)snd_pcm_plugin_extra_data(plugin);
	switch (data->cmd) {
	case _U8_ADPCM:
	case _S8_ADPCM:
		return size / 2;
	case _ADPCM_U8:
	case _ADPCM_S8:
		return size * 2;
	case _U16LE_ADPCM:
	case _S16LE_ADPCM:
	case _U16BE_ADPCM:
	case _S16BE_ADPCM:
		return size / 4;
	case _ADPCM_U16LE:
	case _ADPCM_S16LE:
	case _ADPCM_U16BE:
	case _ADPCM_S16BE:
		return size * 4;
	default:
		return -EIO;
	}
}

int snd_pcm_plugin_build_adpcm(snd_pcm_format_t * src_format,
			       snd_pcm_format_t * dst_format,
			       snd_pcm_plugin_t ** r_plugin)
{
	struct adpcm_private_data *data;
	snd_pcm_plugin_t *plugin;
	combination_t cmd;

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
		switch (src_format->format) {
		case SND_PCM_SFMT_U8:		cmd = _U8_ADPCM;	break;
		case SND_PCM_SFMT_S8:		cmd = _S8_ADPCM;	break;
		case SND_PCM_SFMT_U16_LE:	cmd = _U16LE_ADPCM;	break;
		case SND_PCM_SFMT_S16_LE:	cmd = _S16LE_ADPCM;	break;
		case SND_PCM_SFMT_U16_BE:	cmd = _U16BE_ADPCM;	break;
		case SND_PCM_SFMT_S16_BE:	cmd = _S16BE_ADPCM;	break;
		default:
			return -EINVAL;
		}
	} else if (src_format->format == SND_PCM_SFMT_IMA_ADPCM) {
		switch (dst_format->format) {
		case SND_PCM_SFMT_U8:		cmd = _ADPCM_U8;	break;
		case SND_PCM_SFMT_S8:		cmd = _ADPCM_S8;	break;
		case SND_PCM_SFMT_U16_LE:	cmd = _ADPCM_U16LE;	break;
		case SND_PCM_SFMT_S16_LE:	cmd = _ADPCM_S16LE;	break;
		case SND_PCM_SFMT_U16_BE:	cmd = _ADPCM_U16BE;	break;
		case SND_PCM_SFMT_S16_BE:	cmd = _ADPCM_S16BE;	break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}
	plugin = snd_pcm_plugin_build("Ima-ADPCM<->linear conversion",
				      sizeof(struct adpcm_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct adpcm_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->cmd = cmd;
	plugin->transfer = adpcm_transfer;
	plugin->src_size = adpcm_src_size;
	plugin->dst_size = adpcm_dst_size;
	plugin->action = adpcm_action;
	*r_plugin = plugin;
	return 0;
}
