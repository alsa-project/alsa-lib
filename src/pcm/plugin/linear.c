/*
 *  Linear conversion Plug-In
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
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

#ifdef __KERNEL__
#include "../../include/driver.h"
#include "../../include/pcm.h"
#include "../../include/pcm_plugin.h"
#define bswap_16(x) __swab16((x))
#define bswap_32(x) __swab32((x))
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

/*
 *  Basic linear conversion plugin
 */
 
typedef void (*conv_f)(void *src, void *dst, size_t size);

struct linear_private_data {
	int src_sample_size, dst_sample_size;
	conv_f func;
};

#define CONV_FUNC(name, srctype, dsttype, val) \
static void conv_##name(void *src_ptr, void *dst_ptr, size_t size) \
{ \
	unsigned srctype *srcp = src_ptr; \
	unsigned dsttype *dstp = dst_ptr; \
	while (size--) { \
		unsigned srctype src = *srcp++; \
		*dstp++ = val; \
	} \
}

CONV_FUNC(8_sign, char, char, src ^ 0x80)

CONV_FUNC(8_16, char, short, (unsigned short)src << 8)
CONV_FUNC(8_16_end, char, short, (unsigned short)src)
CONV_FUNC(8_16_sign, char, short, (unsigned short)(src ^ 0x80) << 8)
CONV_FUNC(8_16_sign_end, char, short, (unsigned short)src ^ 0x80)

CONV_FUNC(8_24, char, long, (unsigned long)src << 16)
CONV_FUNC(8_24_end, char, long, (unsigned long)src << 8)
CONV_FUNC(8_24_sign, char, long, (unsigned long)(src ^ 0x80) << 16)
CONV_FUNC(8_24_sign_end, char, long, (unsigned long)(src ^ 0x80) << 8)

CONV_FUNC(8_32, char, long, (unsigned long)src << 24)
CONV_FUNC(8_32_end, char, long, (unsigned long)src)
CONV_FUNC(8_32_sign, char, long, (unsigned long)(src ^ 0x80) << 24)
CONV_FUNC(8_32_sign_end, char, long, (unsigned long)src ^ 0x80)

CONV_FUNC(16_8, short, char, src >> 8)
CONV_FUNC(16_end_8, short, char, src)
CONV_FUNC(16_8_sign, short, char, (src >> 8) ^ 0x80)
CONV_FUNC(16_end_8_sign, short, char, src ^ 0x80)

CONV_FUNC(16_sign, short, short, src ^ 0x8000)
CONV_FUNC(16_end, short, short, bswap_16(src))
CONV_FUNC(16_end_sign, short, short, bswap_16(src) ^ 0x8000)
CONV_FUNC(16_sign_end, short, short, bswap_16(src ^ 0x8000))
CONV_FUNC(16_end_sign_end, short, short, src ^ 0x80)

CONV_FUNC(16_24, short, long, (unsigned long)src << 8)
CONV_FUNC(16_24_sign, short, long, (unsigned long)(src ^ 0x8000) << 8)
CONV_FUNC(16_24_end, short, long, (unsigned long)bswap_16(src) << 8)
CONV_FUNC(16_24_sign_end, short, long, (unsigned long)bswap_16(src ^ 0x8000) << 8)
CONV_FUNC(16_end_24, short, long, (unsigned long)bswap_16(src) << 8)
CONV_FUNC(16_end_24_sign, short, long, (unsigned long)(bswap_16(src) ^ 0x8000) << 8)
CONV_FUNC(16_end_24_end, short, long, (unsigned long)src << 8)
CONV_FUNC(16_end_24_sign_end, short, long, ((unsigned long)src ^ 0x80) << 8)

CONV_FUNC(16_32, short, long, (unsigned long)src << 16)
CONV_FUNC(16_32_sign, short, long, (unsigned long)(src ^ 0x8000) << 16)
CONV_FUNC(16_32_end, short, long, (unsigned long)bswap_16(src))
CONV_FUNC(16_32_sign_end, short, long, (unsigned long)bswap_16(src ^ 0x8000))
CONV_FUNC(16_end_32, short, long, (unsigned long)bswap_16(src) << 16)
CONV_FUNC(16_end_32_sign, short, long, (unsigned long)(bswap_16(src) ^ 0x8000) << 16)
CONV_FUNC(16_end_32_end, short, long, (unsigned long)src)
CONV_FUNC(16_end_32_sign_end, short, long, (unsigned long)src ^ 0x80)

CONV_FUNC(24_8, long, char, src >> 16)
CONV_FUNC(24_end_8, long, char, src >> 8)
CONV_FUNC(24_8_sign, long, char, (src >> 16) ^ 0x80)
CONV_FUNC(24_end_8_sign, long, char, (src >> 8) ^ 0x80)

CONV_FUNC(24_16, long, short, src >> 8)
CONV_FUNC(24_16_sign, long, short, (src >> 8) ^ 0x8000)
CONV_FUNC(24_16_end, long, short, bswap_32(src >> 8))
CONV_FUNC(24_16_sign_end, long, short, bswap_32((src >> 8) ^ 0x8000))
CONV_FUNC(24_end_16, long, short, bswap_32(src) >> 8)
CONV_FUNC(24_end_16_sign, long, short, (bswap_32(src) >> 8) ^ 0x8000)
CONV_FUNC(24_end_16_end, long, short, src >> 8)
CONV_FUNC(24_end_16_sign_end, long, short, (src >> 8) ^ 0x80)

CONV_FUNC(24_sign, long, long, src ^ 0x800000)
CONV_FUNC(24_end, long, long, bswap_32(src))
CONV_FUNC(24_end_sign, long, long, bswap_32(src) ^ 0x800000)
CONV_FUNC(24_sign_end, long, long, bswap_32(src) ^ 0x80)
CONV_FUNC(24_end_sign_end, long, long, src ^ 0x80)

CONV_FUNC(24_32, long, long, src << 8)
CONV_FUNC(24_32_sign, long, long, (src << 8) ^ 0x80000000)
CONV_FUNC(24_32_end, long, long, bswap_32(src << 8))
CONV_FUNC(24_32_sign_end, long, long, bswap_32((src << 8) ^ 0x80000000))
CONV_FUNC(24_end_32, long, long, bswap_32(src) << 8)
CONV_FUNC(24_end_32_sign, long, long, (bswap_32(src) << 8) ^ 0x80000000)
CONV_FUNC(24_end_32_end, long, long, src >> 8)
CONV_FUNC(24_end_32_sign_end, long, long, (src >> 8) ^ 0x80)

CONV_FUNC(32_8, long, char, src >> 24)
CONV_FUNC(32_end_8, long, char, src)
CONV_FUNC(32_8_sign, long, char, (src >> 24) ^ 0x80)
CONV_FUNC(32_end_8_sign, long, char, src ^ 0x80)

CONV_FUNC(32_16, long, short, src >> 16)
CONV_FUNC(32_16_sign, long, short, (src >> 16) ^ 0x8000)
CONV_FUNC(32_16_end, long, short, bswap_16(src >> 16))
CONV_FUNC(32_16_sign_end, long, short, bswap_16((src >> 16) ^ 0x8000))
CONV_FUNC(32_end_16, long, short, bswap_16(src))
CONV_FUNC(32_end_16_sign, long, short, bswap_16(src) ^ 0x8000)
CONV_FUNC(32_end_16_end, long, short, src)
CONV_FUNC(32_end_16_sign_end, long, short, src ^ 0x80)

CONV_FUNC(32_24, long, long, src >> 8)
CONV_FUNC(32_24_sign, long, long, (src >> 8) ^ 0x800000)
CONV_FUNC(32_24_end, long, long, bswap_32(src >> 8))
CONV_FUNC(32_24_sign_end, long, long, bswap_32((src >> 8) ^ 0x800000))
CONV_FUNC(32_end_24, long, long, bswap_32(src) >> 8)
CONV_FUNC(32_end_24_sign, long, long, (bswap_32(src) >> 8) ^ 0x800000)
CONV_FUNC(32_end_24_end, long, long, src << 8)
CONV_FUNC(32_end_24_sign_end, long, long, (src << 8) ^ 0x80)

CONV_FUNC(32_sign, long, long, src ^ 0x80000000)
CONV_FUNC(32_end, long, long, bswap_32(src))
CONV_FUNC(32_end_sign, long, long, bswap_32(src) ^ 0x80000000)
CONV_FUNC(32_sign_end, long, long, bswap_32(src) ^ 0x80)
CONV_FUNC(32_end_sign_end, long, long, src ^ 0x80)

/* src_wid dst_wid src_endswap, dst_endswap, sign_swap */
conv_f convert_functions[4][4][2][2][2] = {
	NULL,			/* 8->8: Nothing to do */
	conv_8_sign,		/* 8->8 sign: conv_8_sign */
	NULL,			/* 8->8 dst_end: Nothing to do */
	conv_8_sign,		/* 8->8 dst_end sign: conv_8_sign */
	NULL,			/* 8->8 src_end: Nothing to do */
	conv_8_sign,		/* 8->8 src_end sign: conv_8_sign */
	NULL,			/* 8->8 src_end dst_end: Nothing to do */
	conv_8_sign,		/* 8->8 src_end dst_end sign: conv_8_sign */
	conv_8_16,		/* 8->16: conv_8_16 */
	conv_8_16_sign,		/* 8->16 sign: conv_8_16_sign */
	conv_8_16_end,		/* 8->16 dst_end: conv_8_16_end */
	conv_8_16_sign_end,	/* 8->16 dst_end sign: conv_8_16_sign_end */
	conv_8_16,		/* 8->16 src_end: conv_8_16 */
	conv_8_16_sign,		/* 8->16 src_end sign: conv_8_16_sign */
	conv_8_16_end,		/* 8->16 src_end dst_end: conv_8_16_end */
	conv_8_16_sign_end,	/* 8->16 src_end dst_end sign: conv_8_16_sign_end */
	conv_8_24,		/* 8->24: conv_8_24 */
	conv_8_24_sign,		/* 8->24 sign: conv_8_24_sign */
	conv_8_24_end,		/* 8->24 dst_end: conv_8_24_end */
	conv_8_24_sign_end,	/* 8->24 dst_end sign: conv_8_24_sign_end */
	conv_8_24,		/* 8->24 src_end: conv_8_24 */
	conv_8_24_sign,		/* 8->24 src_end sign: conv_8_24_sign */
	conv_8_24_end,		/* 8->24 src_end dst_end: conv_8_24_end */
	conv_8_24_sign_end,	/* 8->24 src_end dst_end sign: conv_8_24_sign_end */
	conv_8_32,			/* 8->32: conv_8_32 */
	conv_8_32_sign,			/* 8->32 sign: conv_8_32_sign */
	conv_8_32_end,			/* 8->32 dst_end: conv_8_32_end */
	conv_8_32_sign_end,			/* 8->32 dst_end sign: conv_8_32_sign_end */
	conv_8_32,			/* 8->32 src_end: conv_8_32 */
	conv_8_32_sign,			/* 8->32 src_end sign: conv_8_32_sign */
	conv_8_32_end,			/* 8->32 src_end dst_end: conv_8_32_end */
	conv_8_32_sign_end,			/* 8->32 src_end dst_end sign: conv_8_32_sign_end */
	conv_16_8,		/* 16->8: conv_16_8 */
	conv_16_8_sign,		/* 16->8 sign: conv_16_8_sign */
	conv_16_8,		/* 16->8 dst_end: conv_16_8 */
	conv_16_8_sign,		/* 16->8 dst_end sign: conv_16_8_sign */
	conv_16_end_8,		/* 16->8 src_end: conv_16_end_8 */
	conv_16_end_8_sign,	/* 16->8 src_end sign: conv_16_end_8_sign */
	conv_16_end_8,		/* 16->8 src_end dst_end: conv_16_end_8 */
	conv_16_end_8_sign,	/* 16->8 src_end dst_end sign: conv_16_end_8_sign */
	NULL,			/* 16->16: Nothing to do */
	conv_16_sign,		/* 16->16 sign: conv_16_sign */
	conv_16_end,		/* 16->16 dst_end: conv_16_end */
	conv_16_sign_end,	/* 16->16 dst_end sign: conv_16_sign_end */
	conv_16_end,		/* 16->16 src_end: conv_16_end */
	conv_16_end_sign,	/* 16->16 src_end sign: conv_16_end_sign */
	NULL,			/* 16->16 src_end dst_end: Nothing to do */
	conv_16_end_sign_end,	/* 16->16 src_end dst_end sign: conv_16_end_sign_end */
	conv_16_32,		/* 16->32: conv_16_32 */
	conv_16_32_sign,	/* 16->32 sign: conv_16_32_sign */
	conv_16_32_end,		/* 16->32 dst_end: conv_16_32_end */
	conv_16_32_sign_end,	/* 16->32 dst_end sign: conv_16_32_sign_end */
	conv_16_end_32,		/* 16->32 src_end: conv_16_end_32 */
	conv_16_end_32_sign,	/* 16->32 src_end sign: conv_16_end_32_sign */
	conv_16_end_32_end,	/* 16->32 src_end dst_end: conv_16_end_32_end */
	conv_16_end_32_sign_end,/* 16->32 src_end dst_end sign: conv_16_end_32_sign_end */
	conv_16_24,		/* 16->24: conv_16_24 */
	conv_16_24_sign,	/* 16->24 sign: conv_16_24_sign */
	conv_16_24_end,		/* 16->24 dst_end: conv_16_24_end */
	conv_16_24_sign_end,	/* 16->24 dst_end sign: conv_16_24_sign_end */
	conv_16_end_24,		/* 16->24 src_end: conv_16_end_24 */
	conv_16_end_24_sign,	/* 16->24 src_end sign: conv_16_end_24_sign */
	conv_16_end_24_end,	/* 16->24 src_end dst_end: conv_16_end_24_end */
	conv_16_end_24_sign_end,/* 16->24 src_end dst_end sign: conv_16_end_24_sign_end */
	conv_24_8,		/* 24->8: conv_24_8 */
	conv_24_8_sign,		/* 24->8 sign: conv_24_8_sign */
	conv_24_8,		/* 24->8 dst_end: conv_24_8 */
	conv_24_8_sign,		/* 24->8 dst_end sign: conv_24_8_sign */
	conv_24_end_8,		/* 24->8 src_end: conv_24_end_8 */
	conv_24_end_8_sign,	/* 24->8 src_end sign: conv_24_end_8_sign */
	conv_24_end_8,		/* 24->8 src_end dst_end: conv_24_end_8 */
	conv_24_end_8_sign,	/* 24->8 src_end dst_end sign: conv_24_end_8_sign */
	conv_24_16,		/* 24->16: conv_24_16 */
	conv_24_16_sign,	/* 24->16 sign: conv_24_16_sign */
	conv_24_16_end,		/* 24->16 dst_end: conv_24_16_end */
	conv_24_16_sign_end,	/* 24->16 dst_end sign: conv_24_16_sign_end */
	conv_24_end_16,		/* 24->16 src_end: conv_24_end_16 */
	conv_24_end_16_sign,	/* 24->16 src_end sign: conv_24_end_16_sign */
	conv_24_end_16_end,	/* 24->16 src_end dst_end: conv_24_end_16_end */
	conv_24_end_16_sign_end,/* 24->16 src_end dst_end sign: conv_24_end_16_sign_end */
	NULL,			/* 24->24: Nothing to do */
	conv_24_sign,		/* 24->24 sign: conv_24_sign */
	conv_24_end,		/* 24->24 dst_end: conv_24_end */
	conv_24_sign_end,	/* 24->24 dst_end sign: conv_24_sign_end */
	conv_24_end,		/* 24->24 src_end: conv_24_end */
	conv_24_end_sign,	/* 24->24 src_end sign: conv_24_end_sign */
	NULL,			/* 24->24 src_end dst_end: Nothing to do */
	conv_24_end_sign_end,	/* 24->24 src_end dst_end sign: conv_24_end_sign_end */
	conv_24_32,		/* 24->32: conv_24_32 */
	conv_24_32_sign,	/* 24->32 sign: conv_24_32_sign */
	conv_24_32_end,		/* 24->32 dst_end: conv_24_32_end */
	conv_24_32_sign_end,	/* 24->32 dst_end sign: conv_24_32_sign_end */
	conv_24_end_32,		/* 24->32 src_end: conv_24_end_32 */
	conv_24_end_32_sign,	/* 24->32 src_end sign: conv_24_end_32_sign */
	conv_24_end_32_end,	/* 24->32 src_end dst_end: conv_24_end_32_end */
	conv_24_end_32_sign_end,/* 24->32 src_end dst_end sign: conv_24_end_32_sign_end */
	conv_32_8,		/* 32->8: conv_32_8 */
	conv_32_8_sign,		/* 32->8 sign: conv_32_8_sign */
	conv_32_8,		/* 32->8 dst_end: conv_32_8 */
	conv_32_8_sign,		/* 32->8 dst_end sign: conv_32_8_sign */
	conv_32_end_8,		/* 32->8 src_end: conv_32_end_8 */
	conv_32_end_8_sign,	/* 32->8 src_end sign: conv_32_end_8_sign */
	conv_32_end_8,		/* 32->8 src_end dst_end: conv_32_end_8 */
	conv_32_end_8_sign,	/* 32->8 src_end dst_end sign: conv_32_end_8_sign */
	conv_32_16,		/* 32->16: conv_32_16 */
	conv_32_16_sign,	/* 32->16 sign: conv_32_16_sign */
	conv_32_16_end,		/* 32->16 dst_end: conv_32_16_end */
	conv_32_16_sign_end,	/* 32->16 dst_end sign: conv_32_16_sign_end */
	conv_32_end_16,		/* 32->16 src_end: conv_32_end_16 */
	conv_32_end_16_sign,	/* 32->16 src_end sign: conv_32_end_16_sign */
	conv_32_end_16_end,	/* 32->16 src_end dst_end: conv_32_end_16_end */
	conv_32_end_16_sign_end,/* 32->16 src_end dst_end sign: conv_32_end_16_sign_end */
	conv_32_24,		/* 32->24: conv_32_24 */
	conv_32_24_sign,	/* 32->24 sign: conv_32_24_sign */
	conv_32_24_end,		/* 32->24 dst_end: conv_32_24_end */
	conv_32_24_sign_end,	/* 32->24 dst_end sign: conv_32_24_sign_end */
	conv_32_end_24,		/* 32->24 src_end: conv_32_end_24 */
	conv_32_end_24_sign,	/* 32->24 src_end sign: conv_32_end_24_sign */
	conv_32_end_24_end,	/* 32->24 src_end dst_end: conv_32_end_24_end */
	conv_32_end_24_sign_end,/* 32->24 src_end dst_end sign: conv_32_end_24_sign_end */
	NULL,			/* 32->32: Nothing to do */
	conv_32_sign,		/* 32->32 sign: conv_32_sign */
	conv_32_end,		/* 32->32 dst_end: conv_32_end */
	conv_32_sign_end,	/* 32->32 dst_end sign: conv_32_sign_end */
	conv_32_end,		/* 32->32 src_end: conv_32_end */
	conv_32_end_sign,	/* 32->32 src_end sign: conv_32_end_sign */
	NULL,			/* 32->32 src_end dst_end: Nothing to do */
	conv_32_end_sign_end	/* 32->32 src_end dst_end sign: conv_32_end_sign_end */
};


static ssize_t linear_transfer(snd_pcm_plugin_t *plugin,
			       char *src_ptr, size_t src_size,
			       char *dst_ptr, size_t dst_size)
{
	struct linear_private_data *data;

	if (plugin == NULL || src_ptr == NULL || src_size < 0 ||
	                      dst_ptr == NULL || dst_size < 0)
		return -EINVAL;
	if (src_size == 0)
		return 0;
	data = (struct linear_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	if (src_size % data->src_sample_size != 0)
		return -EINVAL;
	if (dst_size < src_size*data->dst_sample_size/data->src_sample_size)
		return -EINVAL;
	data->func(src_ptr, dst_ptr, src_size / data->src_sample_size);
	return src_size*data->dst_sample_size/data->src_sample_size;
}

static ssize_t linear_src_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct linear_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct linear_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	return size*data->src_sample_size/data->dst_sample_size;
}

static ssize_t linear_dst_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct linear_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct linear_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	return size*data->dst_sample_size/data->src_sample_size;
}

int snd_pcm_plugin_build_linear(snd_pcm_format_t *src_format,
				snd_pcm_format_t *dst_format,
				snd_pcm_plugin_t **r_plugin)
{
	struct linear_private_data *data;
	snd_pcm_plugin_t *plugin;
	conv_f func;
	int src_endian, dst_endian, sign, src_width, dst_width;
	int src_sample_size, dst_sample_size;

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
	if (!(snd_pcm_format_linear(src_format->format) &&
	      snd_pcm_format_linear(dst_format->format)))
		return -EINVAL;

	sign = (snd_pcm_format_signed(src_format->format) !=
		snd_pcm_format_signed(dst_format->format));
	src_width = snd_pcm_format_width(src_format->format);
	dst_width = snd_pcm_format_width(dst_format->format);
#if __BYTE_ORDER == __LITTLE_ENDIAN
	src_endian = snd_pcm_format_big_endian(src_format->format);
	dst_endian = snd_pcm_format_big_endian(dst_format->format);
#elif __BYTE_ORDER == __BIG_ENDIAN
	src_endian = snd_pcm_format_little_endian(src_format->format);
	dst_endian = snd_pcm_format_little_endian(dst_format->format);
#else
#error "Unsupported endian..."
#endif

	switch (src_width) {
	case 8:
		src_width = 0;
		src_sample_size = 1;
		break;
	case 16:
		src_width = 1;
		src_sample_size = 2;
		break;
	case 24:
		src_width = 2;
		src_sample_size = 4;
		break;
	case 32:
		src_width = 3;
		src_sample_size = 4;
		break;
	default:
		return -EINVAL;
	}
	switch (dst_width) {
	case 8:
		dst_width = 0;
		dst_sample_size = 1;
		break;
	case 16:
		dst_width = 1;
		dst_sample_size = 2;
		break;
	case 24:
		dst_width = 2;
		dst_sample_size = 4;
		break;
	case 32:
		dst_width = 3;
		dst_sample_size = 4;
		break;
	default:
		return -EINVAL;
	}

	if (src_endian < 0)
		src_endian = 0;
	if (dst_endian < 0)
		src_endian = 0;

	func = convert_functions[src_width][dst_width][src_endian][dst_endian][sign];

	if (func == NULL)
		return -EINVAL;

	plugin = snd_pcm_plugin_build("linear format conversion",
				      sizeof(struct linear_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct linear_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->func = func;
	data->src_sample_size = src_sample_size;
	data->dst_sample_size = dst_sample_size;

	plugin->transfer = linear_transfer;
	plugin->src_size = linear_src_size;
	plugin->dst_size = linear_dst_size;
	*r_plugin = plugin;
	return 0;
}

#ifdef __KERNEL__
EXPORT_SYMBOL(snd_pcm_plugin_build_linear);
#endif
