/*
 *  Mu-Law conversion Plug-In Interface
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
 *                        Uros Bizjak <uros@kss-loka.si>
 *
 *  Based on reference implementation by Sun Microsystems, Inc.
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
  
#ifdef __KERNEL__
#include "../../include/driver.h"
#include "../../include/pcm.h"
#include "../../include/pcm_plugin.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include <sys/uio.h>
#include "../pcm_local.h"
#endif

#define	SIGN_BIT	(0x80)		/* Sign bit for a u-law byte. */
#define	QUANT_MASK	(0xf)		/* Quantization field mask. */
#define	NSEGS		(8)		/* Number of u-law segments. */
#define	SEG_SHIFT	(4)		/* Left shift for segment number. */
#define	SEG_MASK	(0x70)		/* Segment field mask. */

static short ulaw_seg_end[8] = {0xFF, 0x1FF, 0x3FF, 0x7FF,
				0xFFF, 0x1FFF, 0x3FFF, 0x7FFF};

static inline int search(int val, short *table, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (val <= *table++)
			return (i);
	}
	return size;
}

#define	BIAS		(0x84)		/* Bias for linear code. */

/*
 * linear2ulaw() - Convert a linear PCM value to u-law
 *
 * In order to simplify the encoding process, the original linear magnitude
 * is biased by adding 33 which shifts the encoding range from (0 - 8158) to
 * (33 - 8191). The result can be seen in the following encoding table:
 *
 *	Biased Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	00000001wxyza			000wxyz
 *	0000001wxyzab			001wxyz
 *	000001wxyzabc			010wxyz
 *	00001wxyzabcd			011wxyz
 *	0001wxyzabcde			100wxyz
 *	001wxyzabcdef			101wxyz
 *	01wxyzabcdefg			110wxyz
 *	1wxyzabcdefgh			111wxyz
 *
 * Each biased linear code has a leading 1 which identifies the segment
 * number. The value of the segment number is equal to 7 minus the number
 * of leading 0's. The quantization interval is directly available as the
 * four bits wxyz.  * The trailing bits (a - h) are ignored.
 *
 * Ordinarily the complement of the resulting code word is used for
 * transmission, and so the code word is complemented before it is returned.
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */
static unsigned char linear2ulaw(int pcm_val)	/* 2's complement (16-bit range) */
{
	int mask;
	int seg;
	unsigned char uval;

	/* Get the sign and the magnitude of the value. */
	if (pcm_val < 0) {
		pcm_val = BIAS - pcm_val;
		mask = 0x7F;
	} else {
		pcm_val += BIAS;
		mask = 0xFF;
	}

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, ulaw_seg_end, NSEGS);

	/*
	 * Combine the sign, segment, quantization bits;
	 * and complement the code word.
	 */
	if (seg >= 8)		/* out of range, return maximum value. */
		return 0x7F ^ mask;
	else {
		uval = (seg << 4) | ((pcm_val >> (seg + 3)) & 0xF);
		return uval ^ mask;
	}
}

/*
 * ulaw2linear() - Convert a u-law value to 16-bit linear PCM
 *
 * First, a biased linear code is derived from the code word. An unbiased
 * output can then be obtained by subtracting 33 from the biased code.
 *
 * Note that this function expects to be passed the complement of the
 * original code word. This is in keeping with ISDN conventions.
 */
static int ulaw2linear(unsigned char u_val)
{
	int t;

	/* Complement to obtain normal u-law value. */
	u_val = ~u_val;

	/*
	 * Extract and bias the quantization bits. Then
	 * shift up by the segment number and subtract out the bias.
	 */
	t = ((u_val & QUANT_MASK) << 3) + BIAS;
	t <<= ((unsigned)u_val & SEG_MASK) >> SEG_SHIFT;

	return ((u_val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
}

/*
 *  Basic Mu-Law plugin
 */

typedef void (*mulaw_f)(void *src_ptr, void *dst_ptr, int samples);

typedef struct mulaw_private_data {
	int src_byte_width;
	int dst_byte_width;
	mulaw_f func;
} mulaw_t;

#define MULAW_FUNC_DECODE(name, dsttype, val) \
static void mulaw_decode_##name(void *src_ptr, void *dst_ptr, int samples) \
{ \
	unsigned char *src = src_ptr; \
	dsttype *dst = dst_ptr; \
	unsigned int s; \
	while (samples--) { \
		s = ulaw2linear(*src++); \
		*dst++ = val; \
	} \
}

#define MULAW_FUNC_ENCODE(name, srctype, val) \
static void mulaw_encode_##name(void *src_ptr, void *dst_ptr, int samples) \
{ \
	srctype *src = src_ptr; \
	unsigned char *dst = dst_ptr; \
	unsigned int s; \
	while (samples--) { \
		s = *src++; \
		*dst++ = linear2ulaw(val); \
	} \
}

MULAW_FUNC_DECODE(u8, u_int8_t, (s >> 8) ^ 0x80)
MULAW_FUNC_DECODE(s8, u_int8_t, s >> 8)
MULAW_FUNC_DECODE(u16n, u_int16_t, s ^ 0x8000)
MULAW_FUNC_DECODE(u16s, u_int16_t, bswap_16(s ^ 0x8000))
MULAW_FUNC_DECODE(s16n, u_int16_t, s)
MULAW_FUNC_DECODE(s16s, u_int16_t, bswap_16(s))
MULAW_FUNC_DECODE(u24n, u_int32_t, (s << 8) ^ 0x800000)
MULAW_FUNC_DECODE(u24s, u_int32_t, bswap_32((s << 8) ^ 0x800000))
MULAW_FUNC_DECODE(s24n, u_int32_t, s << 8)
MULAW_FUNC_DECODE(s24s, u_int32_t, bswap_32(s << 8))
MULAW_FUNC_DECODE(u32n, u_int32_t, (s << 16) ^ 0x80000000)
MULAW_FUNC_DECODE(u32s, u_int32_t, bswap_32((s << 16) ^ 0x80000000))
MULAW_FUNC_DECODE(s32n, u_int32_t, s << 16)
MULAW_FUNC_DECODE(s32s, u_int32_t, bswap_32(s << 16))

MULAW_FUNC_ENCODE(u8, u_int8_t, s << 8)
MULAW_FUNC_ENCODE(s8, u_int8_t, (s << 8) ^ 0x8000)
MULAW_FUNC_ENCODE(u16n, u_int16_t, s ^ 0x8000)
MULAW_FUNC_ENCODE(u16s, u_int16_t, bswap_16(s ^ 0x8000))
MULAW_FUNC_ENCODE(s16n, u_int16_t, s)
MULAW_FUNC_ENCODE(s16s, u_int16_t, bswap_16(s))
MULAW_FUNC_ENCODE(u24n, u_int32_t, (s ^ 0x800000) >> 8)
MULAW_FUNC_ENCODE(u24s, u_int32_t, bswap_32((s ^ 0x800000) >> 8))
MULAW_FUNC_ENCODE(s24n, u_int32_t, s >> 8)
MULAW_FUNC_ENCODE(s24s, u_int32_t, bswap_32(s >> 8))
MULAW_FUNC_ENCODE(u32n, u_int32_t, (s ^ 0x80000000) >> 16)
MULAW_FUNC_ENCODE(u32s, u_int32_t, bswap_32((s ^ 0x80000000) >> 16))
MULAW_FUNC_ENCODE(s32n, u_int32_t, s >> 16)
MULAW_FUNC_ENCODE(s32s, u_int32_t, bswap_32(s >> 16))

/* wide, sign, swap endian */
static mulaw_f mulaw_functions_decode[4 * 4 * 2 * 2] = {
	mulaw_decode_u8,	/* decode:8-bit:unsigned:none */
	mulaw_decode_u8,	/* decode:8-bit:unsigned:swap */
	mulaw_decode_s8,	/* decode:8-bit:signed:none */
	mulaw_decode_s8,	/* decode:8-bit:signed:swap */
	mulaw_decode_u16n,	/* decode:16-bit:unsigned:none */
	mulaw_decode_u16s,	/* decode:16-bit:unsigned:swap */
	mulaw_decode_s16n,	/* decode:16-bit:signed:none */
	mulaw_decode_s16s,	/* decode:16-bit:signed:swap */
	mulaw_decode_u24n,	/* decode:24-bit:unsigned:none */
	mulaw_decode_u24s,	/* decode:24-bit:unsigned:swap */
	mulaw_decode_s24n,	/* decode:24-bit:signed:none */
	mulaw_decode_s24s,	/* decode:24-bit:signed:swap */
	mulaw_decode_u32n,	/* decode:32-bit:unsigned:none */
	mulaw_decode_u32s,	/* decode:32-bit:unsigned:swap */
	mulaw_decode_s32n,	/* decode:32-bit:signed:none */
	mulaw_decode_s32s,	/* decode:32-bit:signed:swap */
};

/* wide, sign, swap endian */
static mulaw_f mulaw_functions_encode[4 * 2 * 2] = {
	mulaw_encode_u8,	/* from:8-bit:unsigned:none */
	mulaw_encode_u8,	/* from:8-bit:unsigned:swap */
	mulaw_encode_s8,	/* from:8-bit:signed:none */
	mulaw_encode_s8,	/* from:8-bit:signed:swap */
	mulaw_encode_u16n,	/* from:16-bit:unsigned:none */
	mulaw_encode_u16s,	/* from:16-bit:unsigned:swap */
	mulaw_encode_s16n,	/* from:16-bit:signed:none */
	mulaw_encode_s16s,	/* from:16-bit:signed:swap */
	mulaw_encode_u24n,	/* from:24-bit:unsigned:none */
	mulaw_encode_u24s,	/* from:24-bit:unsigned:swap */
	mulaw_encode_s24n,	/* from:24-bit:signed:none */
	mulaw_encode_s24s,	/* from:24-bit:signed:swap */
	mulaw_encode_u32n,	/* from:32-bit:unsigned:none */
	mulaw_encode_u32s,	/* from:32-bit:unsigned:swap */
	mulaw_encode_s32n,	/* from:32-bit:signed:none */
	mulaw_encode_s32s,	/* from:32-bit:signed:swap */
};

static ssize_t mulaw_transfer(snd_pcm_plugin_t *plugin,
			      const snd_pcm_plugin_voice_t *src_voices,
			      const snd_pcm_plugin_voice_t *dst_voices,
			      size_t samples)
{
	mulaw_t *data;
	int voice;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL || samples < 0)
		return -EINVAL;
	if (samples == 0)
		return 0;
	data = (mulaw_t *)plugin->extra_data;
	if (plugin->src_format.interleave) {
		data->func(src_voices[0].addr,
			   dst_voices[0].addr,
			   samples * plugin->src_format.voices);
	} else {
		for (voice = 0; voice < plugin->src_format.voices; voice++) {
			if (src_voices[voice].addr == NULL)
				continue;
			data->func(src_voices[voice].addr,
				   dst_voices[voice].addr,
				   samples);
		}
	}
	return samples;
}

int snd_pcm_plugin_build_mulaw(snd_pcm_plugin_handle_t *handle,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin)
{
	struct mulaw_private_data *data;
	snd_pcm_plugin_t *plugin;
	int endian, src_width, dst_width, sign;
	mulaw_f func;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->interleave != dst_format->interleave && 
	    src_format->voices > 1)
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;


	if (dst_format->format == SND_PCM_SFMT_MU_LAW) {
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
		func = ((mulaw_f(*)[2][2])mulaw_functions_encode)[(src_width/8)-1][sign][endian];
	} else if (src_format->format == SND_PCM_SFMT_MU_LAW) {
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
		func = ((mulaw_f(*)[2][2])mulaw_functions_decode)[(dst_width/8)-1][sign][endian];
	} else {
		return -EINVAL;
	}
	plugin = snd_pcm_plugin_build(handle,
				      "Mu-Law<->linear conversion",
				      src_format,
				      dst_format,
				      sizeof(struct mulaw_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct mulaw_private_data *)plugin->extra_data;
	data->src_byte_width = src_width / 8;
	data->dst_byte_width = dst_width / 8;
	data->func = func;
	plugin->transfer = mulaw_transfer;
	*r_plugin = plugin;
	return 0;
}
