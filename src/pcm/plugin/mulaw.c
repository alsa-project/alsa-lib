/*
 *  muLaw conversion Plug-In Interface
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
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include "../pcm_local.h"
#include "mulaw.h"

/*
 *  Basic muLaw plugin
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
	unsigned int idx;

	while (size-- > 0) {
		idx = ((*src_ptr++) ^ 0x80) << 8;
		*dst_ptr++ = lintomulaw[idx];
	}
}

static void mulaw_conv_s8bit_mulaw(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	unsigned int idx;

	while (size-- > 0) {
		idx = *src_ptr++ << 8;
		*dst_ptr++ = lintomulaw[idx];
	}
}

static void mulaw_conv_s16bit_mulaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = lintomulaw[*src_ptr++];
}

static void mulaw_conv_s16bit_swap_mulaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = lintomulaw[bswap_16(*src_ptr++)];
}

static void mulaw_conv_u16bit_mulaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = lintomulaw[(*src_ptr++) ^ 0x8000];
}

static void mulaw_conv_u16bit_swap_mulaw(unsigned short *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = lintomulaw[bswap_16(*src_ptr++) ^ 0x8000];
}

static void mulaw_conv_mulaw_u8bit(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = (mulawtolin[*src_ptr++] >> 8) ^ 0x80;
}

static void mulaw_conv_mulaw_s8bit(unsigned char *src_ptr, unsigned char *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = mulawtolin[*src_ptr++] >> 8;
}

static void mulaw_conv_mulaw_s16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = mulawtolin[*src_ptr++];
}

static void mulaw_conv_mulaw_swap_s16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = bswap_16(mulawtolin[*src_ptr++]);
}

static void mulaw_conv_mulaw_u16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = mulawtolin[*src_ptr++] ^ 0x8000;
}

static void mulaw_conv_mulaw_swap_u16bit(unsigned char *src_ptr, unsigned short *dst_ptr, size_t size)
{
	while (size-- > 0)
		*dst_ptr++ = bswap_16(mulawtolin[*src_ptr++] ^ 0x8000);
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
		return dst_size << 1;
	default:
		return -EIO;
	}
}

static ssize_t mulaw_src_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct mulaw_private_data *data;

	if (!plugin || size <= 0)
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

	if (!plugin || size <= 0)
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
 
int snd_pcm_plugin_build_mulaw(int src_format, int dst_format, snd_pcm_plugin_t **r_plugin)
{
	struct mulaw_private_data *data;
	snd_pcm_plugin_t *plugin;
	combination_t cmd;

	if (!r_plugin)
		return -EINVAL;
	*r_plugin = NULL;
	if (dst_format == SND_PCM_SFMT_MU_LAW) {
		switch (src_format) {
		case SND_PCM_SFMT_U8:		cmd = _U8_MULAW;	break;
		case SND_PCM_SFMT_S8:		cmd = _S8_MULAW;	break;
		case SND_PCM_SFMT_U16_LE:	cmd = _U16LE_MULAW;	break;
		case SND_PCM_SFMT_S16_LE:	cmd = _S16LE_MULAW;	break;
		case SND_PCM_SFMT_U16_BE:	cmd = _U16BE_MULAW;	break;
		case SND_PCM_SFMT_S16_BE:	cmd = _S16BE_MULAW;	break;
		default:
			return -EINVAL;
		}
	} else if (src_format == SND_PCM_SFMT_MU_LAW) {
		switch (dst_format) {
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
	plugin = snd_pcm_plugin_build("muLaw<->linear conversion",
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
