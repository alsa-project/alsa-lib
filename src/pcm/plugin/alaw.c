/*
 *  A-Law conversion Plug-In Interface
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
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include <sys/uio.h>
#include "../pcm_local.h"

#define	SIGN_BIT	(0x80)		/* Sign bit for a A-law byte. */
#define	QUANT_MASK	(0xf)		/* Quantization field mask. */
#define	NSEGS		(8)		/* Number of A-law segments. */
#define	SEG_SHIFT	(4)		/* Left shift for segment number. */
#define	SEG_MASK	(0x70)		/* Segment field mask. */

static short alaw_seg_end[8] = {0xFF, 0x1FF, 0x3FF, 0x7FF,
				0xFFF, 0x1FFF, 0x3FFF, 0x7FFF};

static inline int search(int val, short *table, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (val <= *table++)
			return (i);
	}
	return (size);
}

/*
 * linear2alaw() - Convert a 16-bit linear PCM value to 8-bit A-law
 *
 * linear2alaw() accepts an 16-bit integer and encodes it as A-law data.
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
static unsigned char linear2alaw(int pcm_val)	/* 2's complement (16-bit range) */
{
	int		mask;
	int		seg;
	unsigned char	aval;

	if (pcm_val >= 0) {
		mask = 0xD5;		/* sign (7th) bit = 1 */
	} else {
		mask = 0x55;		/* sign bit = 0 */
		pcm_val = -pcm_val - 8;
	}

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, alaw_seg_end, NSEGS);

	/* Combine the sign, segment, and quantization bits. */

	if (seg >= 8)		/* out of range, return maximum value. */
		return (0x7F ^ mask);
	else {
		aval = seg << SEG_SHIFT;
		if (seg < 2)
			aval |= (pcm_val >> 4) & QUANT_MASK;
		else
			aval |= (pcm_val >> (seg + 3)) & QUANT_MASK;
		return (aval ^ mask);
	}
}

/*
 * alaw2linear() - Convert an A-law value to 16-bit linear PCM
 *
 */
static int alaw2linear(unsigned char a_val)
{
	int		t;
	int		seg;

	a_val ^= 0x55;

	t = (a_val & QUANT_MASK) << 4;
	seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
	switch (seg) {
	case 0:
		t += 8;
		break;
	case 1:
		t += 0x108;
		break;
	default:
		t += 0x108;
		t <<= seg - 1;
	}
	return ((a_val & SIGN_BIT) ? t : -t);
}


/*
 *  Basic A-Law plugin
 */

typedef void (*alaw_f)(void *src_ptr, void *dst_ptr, int samples);

typedef struct alaw_private_data {
	int src_byte_width;
	int dst_byte_width;
	alaw_f func;
} alaw_t;

#define ALAW_FUNC_DECODE(name, dsttype, val) \
static void alaw_decode_##name(void *src_ptr, void *dst_ptr, int samples) \
{ \
	unsigned char *src = src_ptr; \
	dsttype *dst = dst_ptr; \
	unsigned int s; \
	while (samples--) { \
		s = alaw2linear(*src++); \
		*dst++ = val; \
	} \
}

#define ALAW_FUNC_ENCODE(name, srctype, val) \
static void alaw_encode_##name(void *src_ptr, void *dst_ptr, int samples) \
{ \
	srctype *src = src_ptr; \
	unsigned char *dst = dst_ptr; \
	unsigned int s; \
	while (samples--) { \
		s = *src++; \
		*dst++ = linear2alaw(val); \
	} \
}

ALAW_FUNC_DECODE(u8, u_int8_t, (s >> 8) ^ 0x80)
ALAW_FUNC_DECODE(s8, u_int8_t, s >> 8)
ALAW_FUNC_DECODE(u16n, u_int16_t, s ^ 0x8000)
ALAW_FUNC_DECODE(u16s, u_int16_t, bswap_16(s ^ 0x8000))
ALAW_FUNC_DECODE(s16n, u_int16_t, s)
ALAW_FUNC_DECODE(s16s, u_int16_t, bswap_16(s))
ALAW_FUNC_DECODE(u24n, u_int32_t, (s << 8) ^ 0x800000)
ALAW_FUNC_DECODE(u24s, u_int32_t, bswap_32((s << 8) ^ 0x800000))
ALAW_FUNC_DECODE(s24n, u_int32_t, s << 8)
ALAW_FUNC_DECODE(s24s, u_int32_t, bswap_32(s << 8))
ALAW_FUNC_DECODE(u32n, u_int32_t, (s << 16) ^ 0x80000000)
ALAW_FUNC_DECODE(u32s, u_int32_t, bswap_32((s << 16) ^ 0x80000000))
ALAW_FUNC_DECODE(s32n, u_int32_t, s << 16)
ALAW_FUNC_DECODE(s32s, u_int32_t, bswap_32(s << 16))

ALAW_FUNC_ENCODE(u8, u_int8_t, s << 8)
ALAW_FUNC_ENCODE(s8, u_int8_t, (s << 8) ^ 0x8000)
ALAW_FUNC_ENCODE(u16n, u_int16_t, s ^ 0x8000)
ALAW_FUNC_ENCODE(u16s, u_int16_t, bswap_16(s ^ 0x8000))
ALAW_FUNC_ENCODE(s16n, u_int16_t, s)
ALAW_FUNC_ENCODE(s16s, u_int16_t, bswap_16(s))
ALAW_FUNC_ENCODE(u24n, u_int32_t, (s ^ 0x800000) >> 8)
ALAW_FUNC_ENCODE(u24s, u_int32_t, bswap_32((s ^ 0x800000) >> 8))
ALAW_FUNC_ENCODE(s24n, u_int32_t, s >> 8)
ALAW_FUNC_ENCODE(s24s, u_int32_t, bswap_32(s >> 8))
ALAW_FUNC_ENCODE(u32n, u_int32_t, (s ^ 0x80000000) >> 16)
ALAW_FUNC_ENCODE(u32s, u_int32_t, bswap_32((s ^ 0x80000000) >> 16))
ALAW_FUNC_ENCODE(s32n, u_int32_t, s >> 16)
ALAW_FUNC_ENCODE(s32s, u_int32_t, bswap_32(s >> 16))

/* wide, sign, swap endian */
static alaw_f alaw_functions_decode[4 * 2 * 2] = {
	alaw_decode_u8,		/* decode:8-bit:unsigned:none */
	alaw_decode_u8,		/* decode:8-bit:unsigned:swap */
	alaw_decode_s8,		/* decode:8-bit:signed:none */
	alaw_decode_s8,		/* decode:8-bit:signed:swap */
	alaw_decode_u16n,	/* decode:16-bit:unsigned:none */
	alaw_decode_u16s,	/* decode:16-bit:unsigned:swap */
	alaw_decode_s16n,	/* decode:16-bit:signed:none */
	alaw_decode_s16s,	/* decode:16-bit:signed:swap */
	alaw_decode_u24n,	/* decode:24-bit:unsigned:none */
	alaw_decode_u24s,	/* decode:24-bit:unsigned:swap */
	alaw_decode_s24n,	/* decode:24-bit:signed:none */
	alaw_decode_s24s,	/* decode:24-bit:signed:swap */
	alaw_decode_u32n,	/* decode:32-bit:unsigned:none */
	alaw_decode_u32s,	/* decode:32-bit:unsigned:swap */
	alaw_decode_s32n,	/* decode:32-bit:signed:none */
	alaw_decode_s32s,	/* decode:32-bit:signed:swap */
};

/* wide, sign, swap endian */
static alaw_f alaw_functions_encode[4 * 2 * 2] = {
	alaw_encode_u8,		/* encode:8-bit:unsigned:none */
	alaw_encode_u8,		/* encode:8-bit:unsigned:swap */
	alaw_encode_s8,		/* encode:8-bit:signed:none */
	alaw_encode_s8,		/* encode:8-bit:signed:swap */
	alaw_encode_u16n,	/* encode:16-bit:unsigned:none */
	alaw_encode_u16s,	/* encode:16-bit:unsigned:swap */
	alaw_encode_s16n,	/* encode:16-bit:signed:none */
	alaw_encode_s16s,	/* encode:16-bit:signed:swap */
	alaw_encode_u24n,	/* encode:24-bit:unsigned:none */
	alaw_encode_u24s,	/* encode:24-bit:unsigned:swap */
	alaw_encode_s24n,	/* encode:24-bit:signed:none */
	alaw_encode_s24s,	/* encode:24-bit:signed:swap */
	alaw_encode_u32n,	/* encode:32-bit:unsigned:none */
	alaw_encode_u32s,	/* encode:32-bit:unsigned:swap */
	alaw_encode_s32n,	/* encode:32-bit:signed:none */
	alaw_encode_s32s,	/* encode:32-bit:signed:swap */
};

static ssize_t alaw_transfer(snd_pcm_plugin_t *plugin,
			     const snd_pcm_plugin_voice_t *src_voices,
			     const snd_pcm_plugin_voice_t *dst_voices,
			     size_t samples)
{
	alaw_t *data;
	int voice;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL || samples < 0)
		return -EINVAL;
	if (samples == 0)
		return 0;
	data = (alaw_t *)plugin->extra_data;
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

int snd_pcm_plugin_build_alaw(snd_pcm_plugin_handle_t *handle,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin)
{
	struct alaw_private_data *data;
	snd_pcm_plugin_t *plugin;
	int endian, src_width, dst_width, sign;
	alaw_f func;

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
		func = ((alaw_f(*)[2][2])alaw_functions_encode)[(src_width/8)-1][sign][endian];
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
		func = ((alaw_f(*)[2][2])alaw_functions_decode)[(dst_width/8)-1][sign][endian];
	} else {
		return -EINVAL;
	}
	plugin = snd_pcm_plugin_build(handle,
				      "A-Law<->linear conversion",
				      src_format,
				      dst_format,
				      sizeof(struct alaw_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct alaw_private_data *)plugin->extra_data;
	data->src_byte_width = src_width / 8;
	data->dst_byte_width = dst_width / 8;
	data->func = func;
	plugin->transfer = alaw_transfer;
	*r_plugin = plugin;
	return 0;
}
