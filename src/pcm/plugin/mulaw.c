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
#include "../../include/pcm_plugin.h"
#define bswap_16(x) __swab16((x))
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
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
static inline unsigned char linear2ulaw(int pcm_val)	/* 2's complement (16-bit range) */
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
static inline int ulaw2linear(unsigned char u_val)
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

typedef enum {
	_S8_MULAW,
	_U8_MULAW,
	_S16LE_MULAW,
	_U16LE_MULAW,
	_S16BE_MULAW,
	_U16BE_MULAW,
	_MULAW_S8,
	_MULAW_U8,
	_MULAW_S16LE,
	_MULAW_U16LE,
	_MULAW_S16BE,
	_MULAW_U16BE
} combination_t; 
 
struct mulaw_private_data {
	combination_t cmd;
};

static void mulaw_conv_u8bit_mulaw(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	unsigned int pcm;

	while (size-- > 0) {
		pcm = ((*src_ptr++) ^ 0x80) << 8;
		*dst_ptr++ = linear2ulaw((signed short)(pcm));
	}
}

static void mulaw_conv_s8bit_mulaw(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	unsigned int pcm;

	while (size-- > 0) {
		pcm = *src_ptr++ << 8;
		*dst_ptr++ = linear2ulaw((signed short)(pcm));
	}
}

static void mulaw_conv_s16bit_mulaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = linear2ulaw((signed short)(*src_ptr++));
}

static void mulaw_conv_s16bit_swap_mulaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = linear2ulaw((signed short)(bswap_16(*src_ptr++)));
}

static void mulaw_conv_u16bit_mulaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = linear2ulaw((signed short)((*src_ptr++) ^ 0x8000));
}

static void mulaw_conv_u16bit_swap_mulaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = linear2ulaw((signed short)(bswap_16(*src_ptr++) ^ 0x8000));
}

static void mulaw_conv_mulaw_u8bit(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = (ulaw2linear(*src_ptr++) >> 8) ^ 0x80;
}

static void mulaw_conv_mulaw_s8bit(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = ulaw2linear(*src_ptr++) >> 8;
}

static void mulaw_conv_mulaw_s16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = ulaw2linear(*src_ptr++);
}

static void mulaw_conv_mulaw_swap_s16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = bswap_16(ulaw2linear(*src_ptr++));
}

static void mulaw_conv_mulaw_u16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = ulaw2linear(*src_ptr++) ^ 0x8000;
}

static void mulaw_conv_mulaw_swap_u16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = bswap_16(ulaw2linear(*src_ptr++) ^ 0x8000);
}

static ssize_t mulaw_transfer(snd_pcm_plugin_t *plugin,
			   char *src_ptr, size_t src_size,
			   char *dst_ptr, size_t dst_size)
{
	struct mulaw_private_data *data;

	if (plugin == NULL || src_ptr == NULL || src_size < 0 ||
	                      dst_ptr == NULL || dst_size < 0)
		return -EINVAL;
	if (src_size == 0)
		return 0;
	data = (struct mulaw_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	switch (data->cmd) {
	case _U8_MULAW:
		if (dst_size < src_size)
			return -EINVAL;
		mulaw_conv_u8bit_mulaw(src_ptr, dst_ptr, src_size);
		return src_size;
	case _S8_MULAW:
		if (dst_size < src_size)
			return -EINVAL;
		mulaw_conv_s8bit_mulaw(src_ptr, dst_ptr, src_size);
		return src_size;
	case _S16LE_MULAW:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		mulaw_conv_s16bit_mulaw((short *)src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		mulaw_conv_s16bit_swap_mulaw((short *)src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 1;
	case _U16LE_MULAW:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		mulaw_conv_u16bit_mulaw((short *)src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		mulaw_conv_u16bit_swap_mulaw((short *)src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 1;
	case _S16BE_MULAW:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		mulaw_conv_s16bit_swap_mulaw((short *)src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		mulaw_conv_s16bit_mulaw((short *)src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 1;
	case _U16BE_MULAW:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		mulaw_conv_u16bit_swap_mulaw((short *)src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		mulaw_conv_u16bit_mulaw((short *)src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 1;
	case _MULAW_U8:
		if (dst_size < src_size)
			return -EINVAL;
		mulaw_conv_mulaw_u8bit(src_ptr, dst_ptr, src_size);
		return src_size;
	case _MULAW_S8:
		if (dst_size < src_size)
			return -EINVAL;
		mulaw_conv_mulaw_s8bit(src_ptr, dst_ptr, src_size);
		return src_size;
	case _MULAW_S16LE:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		mulaw_conv_mulaw_s16bit(src_ptr, (short *)dst_ptr, src_size);
#elif __BYTE_ORDER == __BIG_ENDIAN
		mulaw_conv_mulaw_swap_s16bit(src_ptr, (short *)dst_ptr, src_size);
#else
#error "Have to be coded..."
#endif
		return src_size << 1;
	case _MULAW_U16LE:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		mulaw_conv_mulaw_u16bit(src_ptr, (short *)dst_ptr, src_size);
#elif __BYTE_ORDER == __BIG_ENDIAN
		mulaw_conv_mulaw_swap_u16bit(src_ptr, (short *)dst_ptr, src_size);
#else
#error "Have to be coded..."
#endif
		return src_size << 1;
	case _MULAW_S16BE:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		mulaw_conv_mulaw_swap_s16bit(src_ptr, (short *)dst_ptr, src_size);
#elif __BYTE_ORDER == __BIG_ENDIAN
		mulaw_conv_mulaw_s16bit(src_ptr, (short *)dst_ptr, src_size);
#else
#error "Have to be coded..."
#endif
		return src_size << 1;
	case _MULAW_U16BE:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		mulaw_conv_mulaw_swap_u16bit(src_ptr, (short *)dst_ptr, src_size);
#elif __BYTE_ORDER == __BIG_ENDIAN
		mulaw_conv_mulaw_u16bit(src_ptr, (short *)dst_ptr, src_size);
#else
#error "Have to be coded..."
#endif
		return src_size << 1;
	default:
		return -EIO;
	}
}

static ssize_t mulaw_src_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct mulaw_private_data *data;

	if (plugin == NULL || size <= 0)
		return -EINVAL;
	data = (struct mulaw_private_data *)snd_pcm_plugin_extra_data(plugin);
	switch (data->cmd) {
	case _U8_MULAW:
	case _S8_MULAW:
	case _MULAW_U8:
	case _MULAW_S8:
		return size;
	case _U16LE_MULAW:
	case _S16LE_MULAW:
	case _U16BE_MULAW:
	case _S16BE_MULAW:
		return size * 2;
	case _MULAW_U16LE:
	case _MULAW_S16LE:
	case _MULAW_U16BE:
	case _MULAW_S16BE:
		return size / 2;
	default:
		return -EIO;
	}
}

static ssize_t mulaw_dst_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct mulaw_private_data *data;

	if (plugin == NULL || size <= 0)
		return -EINVAL;
	data = (struct mulaw_private_data *)snd_pcm_plugin_extra_data(plugin);
	switch (data->cmd) {
	case _U8_MULAW:
	case _S8_MULAW:
	case _MULAW_U8:
	case _MULAW_S8:
		return size;
	case _U16LE_MULAW:
	case _S16LE_MULAW:
	case _U16BE_MULAW:
	case _S16BE_MULAW:
		return size / 2;
	case _MULAW_U16LE:
	case _MULAW_S16LE:
	case _MULAW_U16BE:
	case _MULAW_S16BE:
		return size * 2;
	default:
		return -EIO;
	}
}
 
int snd_pcm_plugin_build_mulaw(snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin)
{
	struct mulaw_private_data *data;
	snd_pcm_plugin_t *plugin;
	combination_t cmd;

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
		switch (src_format->format) {
		case SND_PCM_SFMT_U8:		cmd = _U8_MULAW;	break;
		case SND_PCM_SFMT_S8:		cmd = _S8_MULAW;	break;
		case SND_PCM_SFMT_U16_LE:	cmd = _U16LE_MULAW;	break;
		case SND_PCM_SFMT_S16_LE:	cmd = _S16LE_MULAW;	break;
		case SND_PCM_SFMT_U16_BE:	cmd = _U16BE_MULAW;	break;
		case SND_PCM_SFMT_S16_BE:	cmd = _S16BE_MULAW;	break;
		default:
			return -EINVAL;
		}
	} else if (src_format->format == SND_PCM_SFMT_MU_LAW) {
		switch (dst_format->format) {
		case SND_PCM_SFMT_U8:		cmd = _MULAW_U8;	break;
		case SND_PCM_SFMT_S8:		cmd = _MULAW_S8;	break;
		case SND_PCM_SFMT_U16_LE:	cmd = _MULAW_U16LE;	break;
		case SND_PCM_SFMT_S16_LE:	cmd = _MULAW_S16LE;	break;
		case SND_PCM_SFMT_U16_BE:	cmd = _MULAW_U16BE;	break;
		case SND_PCM_SFMT_S16_BE:	cmd = _MULAW_S16BE;	break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}
	plugin = snd_pcm_plugin_build("Mu-Law<->linear conversion",
				      sizeof(struct mulaw_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct mulaw_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->cmd = cmd;
	plugin->transfer = mulaw_transfer;
	plugin->src_size = mulaw_src_size;
	plugin->dst_size = mulaw_dst_size;
	*r_plugin = plugin;
	return 0;
}

#ifdef __KERNEL__
EXPORT_SYMBOL(snd_pcm_plugin_build_mulaw);
#endif
