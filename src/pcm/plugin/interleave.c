/*
 *  Interleave / non-interleave conversion Plug-In
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
 *  Basic interleave / non-interleave conversion plugin
 */
 
typedef enum {
	_INTERLEAVE_NON,
	_NON_INTERLEAVE
} combination_t;
 
struct interleave_private_data {
	combination_t cmd;
	int size;
};

static void separate_8bit(unsigned char *src_ptr,
			  unsigned char *dst_ptr,
			  unsigned int size)
{
	unsigned char *dst1, *dst2;

	dst1 = dst_ptr;
	dst2 = dst_ptr + (size / 2);
	size /= 2;
	while (size--) {
		*dst1++ = *src_ptr++;
		*dst2++ = *src_ptr++;
	}
}

static void separate_16bit(unsigned char *src_ptr,
			   unsigned char *dst_ptr,
			   unsigned int size)
{
	unsigned short *src, *dst1, *dst2;

	src = (short *)src_ptr;
	dst1 = (short *)dst_ptr;
	dst2 = (short *)(dst_ptr + (size / 2));
	size /= 4;
	while (size--) {
		*dst1++ = *src++;
		*dst2++ = *src++;
	}
}

static void separate_32bit(unsigned char *src_ptr,
			   unsigned char *dst_ptr,
			   unsigned int size)
{
	unsigned int *src, *dst1, *dst2;

	src = (int *)src_ptr;
	dst1 = (int *)dst_ptr;
	dst2 = (int *)(dst_ptr + (size / 2));
	size /= 8;
	while (size--) {
		*dst1++ = *src++;
		*dst2++ = *src++;
	}
}

static void interleave_8bit(unsigned char *src_ptr,
			    unsigned char *dst_ptr,
			    unsigned int size)
{
	unsigned char *src1, *src2;

	src1 = src_ptr;
	src2 = src_ptr + (size / 2);
	size /= 2;
	while (size--) {
		*dst_ptr++ = *src1++;
		*dst_ptr++ = *src2++;
	}
}

static void interleave_16bit(unsigned char *src_ptr,
			     unsigned char *dst_ptr,
			     unsigned int size)
{
	unsigned short *src1, *src2, *dst;

	src1 = (short *)src_ptr;
	src2 = (short *)(src_ptr + (size / 2));
	dst = (short *)dst_ptr;
	size /= 4;
	while (size--) {
		*dst++ = *src1++;
		*dst++ = *src2++;
	}
}

static void interleave_32bit(unsigned char *src_ptr,
			     unsigned char *dst_ptr,
			     unsigned int size)
{
	unsigned int *src1, *src2, *dst;

	src1 = (int *)src_ptr;
	src2 = (int *)(src_ptr + (size / 2));
	dst = (int *)dst_ptr;
	size /= 8;
	while (size--) {
		*dst++ = *src1++;
		*dst++ = *src2++;
	}
}

static ssize_t interleave_transfer(snd_pcm_plugin_t *plugin,
				   char *src_ptr, size_t src_size,
				   char *dst_ptr, size_t dst_size)
{
	struct interleave_private_data *data;

	if (plugin == NULL || src_ptr == NULL || src_size < 0 ||
	                      dst_ptr == NULL || dst_size < 0)
		return -EINVAL;
	if (src_size == 0)
		return 0;
	if (src_size != dst_size)
		return -EINVAL;
	data = (struct interleave_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	switch (data->cmd) {
	case _INTERLEAVE_NON:
		switch (data->size) {
		case 1:	separate_8bit(src_ptr, dst_ptr, src_size); break;
		case 2: separate_16bit(src_ptr, dst_ptr, src_size); break;
		case 4: separate_32bit(src_ptr, dst_ptr, src_size); break;
		default:
			return -EINVAL;
		}
		break;
	case _NON_INTERLEAVE:
		switch (data->size) {
		case 1:	interleave_8bit(src_ptr, dst_ptr, src_size); break;
		case 2: interleave_16bit(src_ptr, dst_ptr, src_size); break;
		case 4: interleave_32bit(src_ptr, dst_ptr, src_size); break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return src_size;
}

int snd_pcm_plugin_build_interleave(snd_pcm_format_t *src_format,
				    snd_pcm_format_t *dst_format,
				    snd_pcm_plugin_t **r_plugin)
{
	struct interleave_private_data *data;
	snd_pcm_plugin_t *plugin;
	combination_t cmd;
	int size;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->interleave && !dst_format->interleave) {
		cmd = _INTERLEAVE_NON;
	} else if (!src_format->interleave && dst_format->interleave) {
		cmd = _NON_INTERLEAVE;
	} else {
		return -EINVAL;
	}
	if (src_format->format != dst_format->format)
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;

	switch (dst_format->format) {
	case SND_PCM_SFMT_S8:
	case SND_PCM_SFMT_U8:		size = 1; break;
	case SND_PCM_SFMT_S16_LE:
	case SND_PCM_SFMT_S16_BE:
	case SND_PCM_SFMT_U16_LE:	size = 2; break;
	case SND_PCM_SFMT_S24_LE:
	case SND_PCM_SFMT_S24_BE:
	case SND_PCM_SFMT_U24_LE:
	case SND_PCM_SFMT_U24_BE:
	case SND_PCM_SFMT_S32_LE:
	case SND_PCM_SFMT_S32_BE:
	case SND_PCM_SFMT_U32_LE:
	case SND_PCM_SFMT_U32_BE:
	case SND_PCM_SFMT_FLOAT:	size = 4; break;
	case SND_PCM_SFMT_FLOAT64:	size = 8; break;
	case SND_PCM_SFMT_IEC958_SUBFRAME_LE:
	case SND_PCM_SFMT_IEC958_SUBFRAME_BE: 	size = 4; break;
	case SND_PCM_SFMT_MU_LAW:
	case SND_PCM_SFMT_A_LAW:	size = 1; break;
	default:
		return -EINVAL;
	}
	plugin = snd_pcm_plugin_build("interleave conversion",
				      sizeof(struct interleave_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct interleave_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->cmd = cmd;
	data->size = size;
	plugin->transfer = interleave_transfer;
	*r_plugin = plugin;
	return 0;
}
