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
 
typedef enum {
	_8BIT_16BIT,
	_8BIT_24BIT,
	_8BIT_32BIT,
	_16BIT_8BIT,
	_16BIT_24BIT,
	_16BIT_32BIT,
	_24BIT_8BIT,
	_24BIT_16BIT,
	_24BIT_32BIT,
	_32BIT_8BIT,
	_32BIT_16BIT,
	_32BIT_24BIT
} combination_t;
 
typedef enum {
	NONE = 0,
	SOURCE = 1,
	DESTINATION = 2,
	BOTH = 3,
	SIGN = 4,
	SIGN_NONE = 4,
	SIGN_SOURCE = 5,
	SIGN_DESTINATION = 6,
	SIGN_BOTH = 7,
} endian_t;
 
struct linear_private_data {
	combination_t cmd;
	endian_t endian;
};

static void linear_conv_8bit_16bit(unsigned char *src_ptr,
				   unsigned short *dst_ptr,
				   size_t size)
{
	while (size--)
		*dst_ptr++ = ((unsigned short)*src_ptr++) << 8;
}

static void linear_conv_8bit_16bit_swap(unsigned char *src_ptr,
					unsigned short *dst_ptr,
					size_t size)
{
	while (size--)
		*dst_ptr++ = (unsigned short)*src_ptr++;
}

static void linear_conv_sign_8bit_16bit(unsigned char *src_ptr,
					unsigned short *dst_ptr,
					size_t size)
{
	while (size--)
		*dst_ptr++ = (((unsigned short)*src_ptr++) << 8) ^ 0x8000;
}

static void linear_conv_sign_8bit_16bit_swap(unsigned char *src_ptr,
					     unsigned short *dst_ptr,
					     size_t size)
{
	while (size--)
		*dst_ptr++ = ((unsigned short)*src_ptr++) ^ 0x80;
}

static void linear_conv_16bit_8bit(unsigned short *src_ptr,
				   unsigned char *dst_ptr,
				   size_t size)
{
	while (size--)
		*dst_ptr++ = (*src_ptr++) >> 8;
}

static void linear_conv_16bit_8bit_swap(unsigned short *src_ptr,
					unsigned char *dst_ptr,
					size_t size)
{
	while (size--)
		*dst_ptr++ = (unsigned char)*src_ptr++;
}

static void linear_conv_sign_16bit_8bit(unsigned short *src_ptr,
					unsigned char *dst_ptr,
					size_t size)
{
	while (size--)
		*dst_ptr++ = (((unsigned short)*src_ptr++) >> 8) ^ 0x80;
}

static void linear_conv_sign_16bit_8bit_swap(unsigned short *src_ptr,
					     unsigned char *dst_ptr,
					     size_t size)
{
	while (size--)
		*dst_ptr++ = ((unsigned char)*src_ptr++) ^ 0x80;
}

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
	switch (data->cmd) {
	case _8BIT_16BIT:
		if ((dst_size >> 1) < src_size)
			return -EINVAL;
		switch (data->endian) {
		case NONE:
			linear_conv_8bit_16bit(src_ptr, (short *)dst_ptr, src_size);
			break;
		case DESTINATION:
			linear_conv_8bit_16bit_swap(src_ptr, (short *)dst_ptr, src_size);
			break;
		case SIGN_NONE:
			linear_conv_sign_8bit_16bit(src_ptr, (short *)dst_ptr, src_size);
			break;
		case SIGN_DESTINATION:
			linear_conv_sign_8bit_16bit_swap(src_ptr, (short *)dst_ptr, src_size);
			break;
		default:
			return -EINVAL;
		}
		return src_size << 1;
	case _16BIT_8BIT:
		if (dst_size < (src_size >> 1))
			return -EINVAL;
		src_size >>= 1;
		switch (data->endian) {
		case NONE:
			linear_conv_16bit_8bit((short *)src_ptr, dst_ptr, src_size);
			break;
		case SOURCE:
			linear_conv_16bit_8bit_swap((short *)src_ptr, dst_ptr, src_size);
			break;
		case SIGN_NONE:
			linear_conv_sign_16bit_8bit((short *)src_ptr, dst_ptr, src_size);
			break;
		case SIGN_SOURCE:
			linear_conv_sign_16bit_8bit_swap((short *)src_ptr, dst_ptr, src_size);
			break;
		default:
			return -EINVAL;
		}
		return src_size;
	default:
		return -EIO;
	}
}

static ssize_t linear_src_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct linear_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct linear_private_data *)snd_pcm_plugin_extra_data(plugin);
	switch (data->cmd) {
	case _8BIT_16BIT:
	case _16BIT_24BIT:
	case _16BIT_32BIT:
		return size / 2;
	case _8BIT_24BIT:
	case _8BIT_32BIT:
		return size / 4;
	case _16BIT_8BIT:
	case _24BIT_16BIT:
	case _32BIT_16BIT:
		return size * 2;
	case _24BIT_8BIT:
	case _32BIT_8BIT:
		return size * 4;
	case _24BIT_32BIT:
	case _32BIT_24BIT:
		return size;
	default:
		return -EIO;
	}
}

static ssize_t linear_dst_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct linear_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct linear_private_data *)snd_pcm_plugin_extra_data(plugin);
	switch (data->cmd) {
	case _8BIT_16BIT:
	case _16BIT_24BIT:
	case _16BIT_32BIT:
		return size * 2;
	case _8BIT_24BIT:
	case _8BIT_32BIT:
		return size * 4;
	case _16BIT_8BIT:
	case _24BIT_16BIT:
	case _32BIT_16BIT:
		return size / 2;
	case _24BIT_8BIT:
	case _32BIT_8BIT:
		return size / 4;
	case _24BIT_32BIT:
	case _32BIT_24BIT:
		return size;
	default:
		return -EIO;
	}
}

int snd_pcm_plugin_build_linear(snd_pcm_format_t *src_format,
				snd_pcm_format_t *dst_format,
				snd_pcm_plugin_t **r_plugin)
{
	struct linear_private_data *data;
	snd_pcm_plugin_t *plugin;
	combination_t cmd;
	int width1, width2, endian1, endian2, sign1, sign2;

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

	width1 = snd_pcm_format_width(src_format->format);
	sign1 = snd_pcm_format_signed(src_format->format);
	width2 = snd_pcm_format_width(dst_format->format);
	sign2 = snd_pcm_format_signed(dst_format->format);
#if __BYTE_ORDER == __LITTLE_ENDIAN
	endian1 = snd_pcm_format_little_endian(src_format->format);
	endian2 = snd_pcm_format_little_endian(dst_format->format);
#elif __BYTE_ORDER == __BIG_ENDIAN
	endian1 = snd_pcm_format_big_endian(src_format->format);
	endian2 = snd_pcm_format_big_endian(dst_format->format);
#else
#error "Unsupported endian..."
#endif
	cmd = _8BIT_16BIT;
	switch (width1) {
	case 8:
		switch (width2) {
		case 16:	cmd = _8BIT_16BIT; break;
		case 24:	cmd = _8BIT_24BIT; break;
		case 32:	cmd = _8BIT_32BIT; break;
		default:	return -EINVAL;
		}
		break;
	case 16:
		switch (width2) {
		case 8:		cmd = _16BIT_8BIT; break;
		case 24:	cmd = _16BIT_24BIT; break;
		case 32:	cmd = _16BIT_32BIT; break;
		default:	return -EINVAL;
		}
		break;
	case 24:
		switch (width2) {
		case 8:		cmd = _24BIT_8BIT; break;
		case 16:	cmd = _24BIT_16BIT; break;
		case 32:	cmd = _24BIT_32BIT; break;
		default:	return -EINVAL;
		}
		break;
	case 32:
		switch (width2) {
		case 8:		cmd = _32BIT_8BIT; break;
		case 16:	cmd = _32BIT_16BIT; break;
		case 24:	cmd = _32BIT_24BIT; break;
		default:	return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	plugin = snd_pcm_plugin_build("linear format conversion",
				      sizeof(struct linear_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct linear_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->cmd = cmd;
	data->endian = NONE;
	if (endian1 == 0)
		data->endian |= SOURCE;
	if (endian2 == 0)
		data->endian |= DESTINATION;
	if (sign1 != sign2)
		data->endian |= SIGN;
	plugin->transfer = linear_transfer;
	plugin->src_size = linear_src_size;
	plugin->dst_size = linear_dst_size;
	*r_plugin = plugin;
	return 0;
}

#ifdef __KERNEL__
EXPORT_SYMBOL(snd_pcm_plugin_build_linear);
#endif
