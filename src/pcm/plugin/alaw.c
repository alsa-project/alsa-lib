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
static inline unsigned char linear2alaw(int pcm_val)	/* 2's complement (16-bit range) */
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
static inline int alaw2linear(unsigned char a_val)
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

typedef enum {
	_S8_ALAW,
	_U8_ALAW,
	_S16LE_ALAW,
	_U16LE_ALAW,
	_S16BE_ALAW,
	_U16BE_ALAW,
	_ALAW_S8,
	_ALAW_U8,
	_ALAW_S16LE,
	_ALAW_U16LE,
	_ALAW_S16BE,
	_ALAW_U16BE
} combination_t; 
 
struct alaw_private_data {
	combination_t cmd;
};

static void alaw_conv_u8bit_alaw(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	unsigned int pcm;

	while (size-- > 0) {
		pcm = ((*src_ptr++) ^ 0x80) << 8;
		*dst_ptr++ = linear2alaw((signed short)(pcm));
	}
}

static void alaw_conv_s8bit_alaw(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	unsigned int pcm;

	while (size-- > 0) {
		pcm = *src_ptr++ << 8;
		*dst_ptr++ = linear2alaw((signed short)(pcm));
	}
}

static void alaw_conv_s16bit_alaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = linear2alaw((signed short)(*src_ptr++));
}

static void alaw_conv_s16bit_swap_alaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = linear2alaw((signed short)(bswap_16(*src_ptr++)));
}

static void alaw_conv_u16bit_alaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = linear2alaw((signed short)((*src_ptr++) ^ 0x8000));
}

static void alaw_conv_u16bit_swap_alaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = linear2alaw((signed short)(bswap_16((*src_ptr++) ^ 0x8000)));
}

static void alaw_conv_alaw_u8bit(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = (alaw2linear(*src_ptr++) >> 8) ^ 0x80;
}

static void alaw_conv_alaw_s8bit(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = alaw2linear(*src_ptr++) >> 8;
}

static void alaw_conv_alaw_s16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = alaw2linear(*src_ptr++);
}

static void alaw_conv_alaw_swap_s16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = bswap_16(alaw2linear(*src_ptr++));
}

static void alaw_conv_alaw_u16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = alaw2linear(*src_ptr++) ^ 0x8000;
}

static void alaw_conv_alaw_swap_u16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = bswap_16(alaw2linear(*src_ptr++) ^ 0x8000);
}

static ssize_t alaw_transfer(snd_pcm_plugin_t *plugin,
			      char *src_ptr, size_t src_size,
			      char *dst_ptr, size_t dst_size)
{
	struct alaw_private_data *data;

	if (plugin == NULL || src_ptr == NULL || src_size < 0 ||
	                      dst_ptr == NULL || dst_size < 0)
		return -EINVAL;
	if (src_size == 0)
		return 0;
	data = (struct alaw_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	switch (data->cmd) {
	case _U8_ALAW:
		if (dst_size < src_size)
			return -EINVAL;
		alaw_conv_u8bit_alaw(src_ptr, dst_ptr, src_size);
		return src_size;
	case _S8_ALAW:
		if (dst_size < src_size)
			return -EINVAL;
		alaw_conv_s8bit_alaw(src_ptr, dst_ptr, src_size);
		return src_size;
	case _S16LE_ALAW:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		alaw_conv_s16bit_alaw((short *)src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		alaw_conv_s16bit_swap_alaw((short *)src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 1;
	case _U16LE_ALAW:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		alaw_conv_u16bit_alaw((short *)src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		alaw_conv_u16bit_swap_alaw((short *)src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 1;
	case _S16BE_ALAW:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		alaw_conv_s16bit_swap_alaw((short *)src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		alaw_conv_s16bit_alaw((short *)src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 1;
	case _U16BE_ALAW:
		if ((dst_size << 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		alaw_conv_u16bit_swap_alaw((short *)src_ptr, dst_ptr, src_size >> 1);
#elif __BYTE_ORDER == __BIG_ENDIAN
		alaw_conv_u16bit_alaw((short *)src_ptr, dst_ptr, src_size >> 1);
#else
#error "Have to be coded..."
#endif
		return src_size >> 1;
	case _ALAW_U8:
		if (dst_size < src_size)
			return -EINVAL;
		alaw_conv_alaw_u8bit(src_ptr, dst_ptr, src_size);
		return src_size;
	case _ALAW_S8:
		if (dst_size < src_size)
			return -EINVAL;
		alaw_conv_alaw_s8bit(src_ptr, dst_ptr, src_size);
		return src_size;
	case _ALAW_S16LE:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		alaw_conv_alaw_s16bit(src_ptr, (short *)dst_ptr, src_size);
#elif __BYTE_ORDER == __BIG_ENDIAN
		alaw_conv_alaw_swap_s16bit(src_ptr, (short *)dst_ptr, src_size);
#else
#error "Have to be coded..."
#endif
		return src_size << 1;
	case _ALAW_U16LE:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		alaw_conv_alaw_u16bit(src_ptr, (short *)dst_ptr, src_size);
#elif __BYTE_ORDER == __BIG_ENDIAN
		alaw_conv_alaw_swap_u16bit(src_ptr, (short *)dst_ptr, src_size);
#else
#error "Have to be coded..."
#endif
		return src_size << 1;
	case _ALAW_S16BE:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		alaw_conv_alaw_swap_s16bit(src_ptr, (short *)dst_ptr, src_size);
#elif __BYTE_ORDER == __BIG_ENDIAN
		alaw_conv_alaw_s16bit(src_ptr, (short *)dst_ptr, src_size);
#else
#error "Have to be coded..."
#endif
		return src_size << 1;
	case _ALAW_U16BE:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		alaw_conv_alaw_swap_u16bit(src_ptr, (short *)dst_ptr, src_size);
#elif __BYTE_ORDER == __BIG_ENDIAN
		alaw_conv_alaw_u16bit(src_ptr, (short *)dst_ptr, src_size);
#else
#error "Have to be coded..."
#endif
		return src_size << 1;
	default:
		return -EIO;
	}
}

static ssize_t alaw_src_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct alaw_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct alaw_private_data *)snd_pcm_plugin_extra_data(plugin);
	switch (data->cmd) {
	case _U8_ALAW:
	case _S8_ALAW:
	case _ALAW_U8:
	case _ALAW_S8:
		return size;
	case _U16LE_ALAW:
	case _S16LE_ALAW:
	case _U16BE_ALAW:
	case _S16BE_ALAW:
		return size * 2;
	case _ALAW_U16LE:
	case _ALAW_S16LE:
	case _ALAW_U16BE:
	case _ALAW_S16BE:
		return size / 2;
	default:
		return -EIO;
	}
}

static ssize_t alaw_dst_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct alaw_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct alaw_private_data *)snd_pcm_plugin_extra_data(plugin);
	switch (data->cmd) {
	case _U8_ALAW:
	case _S8_ALAW:
	case _ALAW_U8:
	case _ALAW_S8:
		return size;
	case _U16LE_ALAW:
	case _S16LE_ALAW:
	case _U16BE_ALAW:
	case _S16BE_ALAW:
		return size / 2;
	case _ALAW_U16LE:
	case _ALAW_S16LE:
	case _ALAW_U16BE:
	case _ALAW_S16BE:
		return size * 2;
	default:
		return -EIO;
	}
}
 
int snd_pcm_plugin_build_alaw(snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin)
{
	struct alaw_private_data *data;
	snd_pcm_plugin_t *plugin;
	combination_t cmd;

	if (!r_plugin)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->interleave != dst_format->interleave && 
	    src_format->voices > 1)
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;

	if (dst_format->format == SND_PCM_SFMT_A_LAW) {
		switch (src_format->format) {
		case SND_PCM_SFMT_U8:		cmd = _U8_ALAW;		break;
		case SND_PCM_SFMT_S8:		cmd = _S8_ALAW;		break;
		case SND_PCM_SFMT_U16_LE:	cmd = _U16LE_ALAW;	break;
		case SND_PCM_SFMT_S16_LE:	cmd = _S16LE_ALAW;	break;
		case SND_PCM_SFMT_U16_BE:	cmd = _U16BE_ALAW;	break;
		case SND_PCM_SFMT_S16_BE:	cmd = _S16BE_ALAW;	break;
		default:
			return -EINVAL;
		}
	} else if (src_format->format == SND_PCM_SFMT_A_LAW) {
		switch (dst_format->format) {
		case SND_PCM_SFMT_U8:		cmd = _ALAW_U8;		break;
		case SND_PCM_SFMT_S8:		cmd = _ALAW_S8;		break;
		case SND_PCM_SFMT_U16_LE:	cmd = _ALAW_U16LE;	break;
		case SND_PCM_SFMT_S16_LE:	cmd = _ALAW_S16LE;	break;
		case SND_PCM_SFMT_U16_BE:	cmd = _ALAW_U16BE;	break;
		case SND_PCM_SFMT_S16_BE:	cmd = _ALAW_S16BE;	break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}
	plugin = snd_pcm_plugin_build("A-Law<->linear conversion",
				      sizeof(struct alaw_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct alaw_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->cmd = cmd;
	plugin->transfer = alaw_transfer;
	plugin->src_size = alaw_src_size;
	plugin->dst_size = alaw_dst_size;
	*r_plugin = plugin;
	return 0;
}
