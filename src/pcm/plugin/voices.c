/*
 *  Voices conversion Plug-In
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
 *  Basic voices conversion plugin
 */
 
struct voices_private_data {
	int src_voices;
	int dst_voices;
	int width;		/* in bites */
	int flg_signed: 1;
};

static void divide_8bit(char *src_ptr, char *dst_ptr, int size)
{
	while (size-- > 0) {
		*dst_ptr++ = *src_ptr;
		*dst_ptr++ = *src_ptr++;
	}
}

static void divide_16bit(short *src_ptr, short *dst_ptr, int size)
{
	while (size-- > 0) {
		*dst_ptr++ = *src_ptr;
		*dst_ptr++ = *src_ptr++;
	}
}

static void merge_8bit_unsigned(unsigned char *src_ptr,
			        unsigned char *dst_ptr,
			        int size)
{
	while (size-- > 0) {
		*dst_ptr++ = ((int)*src_ptr + (int)*(src_ptr + 1)) / 2;
		src_ptr += 2;
	}
}

static void merge_8bit_signed(signed char *src_ptr,
			      signed char *dst_ptr,
			      int size)
{
	while (size-- > 0) {
		*dst_ptr++ = ((int)*src_ptr + (int)*(src_ptr + 1)) / 2;
		src_ptr += 2;
	}
}

static void merge_16bit_unsigned(unsigned short *src_ptr,
			         unsigned short *dst_ptr,
			         int size)
{
	while (size-- > 0) {
		*dst_ptr++ = ((int)*src_ptr + (int)*(src_ptr + 1)) / 2;
		src_ptr += 2;
	}
}

static void merge_16bit_signed(signed short *src_ptr,
			       signed short *dst_ptr,
			       int size)
{
	while (size-- > 0) {
		*dst_ptr++ = ((int)*src_ptr + (int)*(src_ptr + 1)) / 2;
		src_ptr += 2;
	}
}

static ssize_t voices_transfer(snd_pcm_plugin_t *plugin,
			     char *src_ptr, size_t src_size,
			     char *dst_ptr, size_t dst_size)
{
	struct voices_private_data *data;

	if (plugin == NULL || src_ptr == NULL || src_size < 0 ||
	                      dst_ptr == NULL || dst_size < 0)
		return -EINVAL;
	if (src_size == 0)
		return 0;
	data = (struct voices_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	switch (data->width) {
	case 8:
		if (data->src_voices > data->dst_voices) {
			if (data->flg_signed) {
				merge_8bit_signed(src_ptr, dst_ptr, src_size / 2);
			} else {
				merge_8bit_unsigned(src_ptr, dst_ptr, src_size / 2);
			}
		} else {
			divide_8bit(src_ptr, dst_ptr, src_size);
		}
		break;
	case 16:
		if (data->src_voices > data->dst_voices) {
			if (data->flg_signed) {
				merge_16bit_signed((short *)src_ptr, (short *)dst_ptr, src_size / 4);
			} else {
				merge_16bit_unsigned((short *)src_ptr, (short *)dst_ptr, src_size / 4);
			}
		} else {
			divide_16bit((short *)src_ptr, (short *)dst_ptr, src_size / 2);
		}
		break;
	default:
		return -EINVAL;
	} 
	return (src_size * data->dst_voices) / data->src_voices;
}

static ssize_t voices_src_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct voices_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct voices_private_data *)snd_pcm_plugin_extra_data(plugin);
	return (size * data->src_voices) / data->dst_voices;
}

static ssize_t voices_dst_size(snd_pcm_plugin_t *plugin, size_t size)
{
	struct voices_private_data *data;

	if (!plugin || size <= 0)
		return -EINVAL;
	data = (struct voices_private_data *)snd_pcm_plugin_extra_data(plugin);
	return (size * data->dst_voices) / data->src_voices;
}

int snd_pcm_plugin_build_voices(snd_pcm_format_t *src_format,
			        snd_pcm_format_t *dst_format,
			        snd_pcm_plugin_t **r_plugin)
{
	struct voices_private_data *data;
	snd_pcm_plugin_t *plugin;

	if (!r_plugin)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->interleave != dst_format->interleave && 
	    src_format->voices > 1)
		return -EINVAL;
	if (!dst_format->interleave)
		return -EINVAL;
	if (src_format->format != dst_format->format)
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices == dst_format->voices)
		return -EINVAL;
	if (src_format->voices < 1 || src_format->voices > 2 ||
	    dst_format->voices < 1 || dst_format->voices > 2)
		return -EINVAL;
	if (src_format->format < SND_PCM_SFMT_S8 || src_format->format > SND_PCM_SFMT_U16_BE) {
		if (src_format->format != SND_PCM_SFMT_MU_LAW && src_format->format != SND_PCM_SFMT_A_LAW)
			return -EINVAL;
	}
	plugin = snd_pcm_plugin_build("voices conversion",
				      sizeof(struct voices_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct voices_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->src_voices = src_format->voices;
	data->dst_voices = dst_format->voices;
	data->width = snd_pcm_format_width(src_format->format);
	data->flg_signed = snd_pcm_format_signed(src_format->format);
	plugin->transfer = voices_transfer;
	plugin->src_size = voices_src_size;
	plugin->dst_size = voices_dst_size;
	*r_plugin = plugin;
	return 0;
}

#ifdef __KERNEL__
EXPORT_SYMBOL(snd_pcm_plugin_build_voices);
#endif
