/*
 *  Interleave / non-interleave conversion Plug-In
 *  Copyright (c) 2000 by Abramo Bagnara <abbagnara@racine.ra.it>
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
 *  Basic interleave / non-interleave conversion plugin
 */
 
typedef void (*interleave_f)(void* src_ptr, void* dst_ptr,
			     int voices, size_t samples);

struct interleave_private_data {
	int sample_size;
	int voices;
	interleave_f func;
};


#define INTERLEAVE_FUNC(name, type) \
static void name(void* src_ptr, void* dst_ptr, \
		       int voices, size_t samples) \
{ \
	type* src = src_ptr; \
	int voice; \
	for (voice = 0; voice < voices; ++voice) { \
		type *dst = (type*)dst_ptr + voice; \
		int s; \
		for (s = 0; s < samples; ++s) { \
			*dst = *src; \
			src++; \
			dst += voices; \
		} \
	} \
} \

#define DEINTERLEAVE_FUNC(name, type) \
static void name(void* src_ptr, void* dst_ptr, \
			 int voices, size_t samples) \
{ \
	type* dst = dst_ptr; \
	int voice; \
	for (voice = 0; voice < voices; ++voice) { \
		type *src = (type*)src_ptr + voice; \
		int s; \
		for (s = 0; s < samples; ++s) { \
			*dst = *src; \
			dst++; \
			src += voices; \
		} \
	} \
}

#define FUNCS(name, type) \
INTERLEAVE_FUNC(int_##name, type); \
DEINTERLEAVE_FUNC(deint_##name, type);

FUNCS(1, int8_t);
FUNCS(2, int16_t);
FUNCS(4, int32_t);
FUNCS(8, int64_t);

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
	data->func(src_ptr, dst_ptr, data->voices,
		   src_size / (data->voices * data->sample_size));
	return src_size;
}

int snd_pcm_plugin_build_interleave(snd_pcm_format_t *src_format,
				    snd_pcm_format_t *dst_format,
				    snd_pcm_plugin_t **r_plugin)
{
	struct interleave_private_data *data;
	snd_pcm_plugin_t *plugin;
	interleave_f func;
	int size;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->interleave == dst_format->interleave)
		return -EINVAL;
	if (src_format->format != dst_format->format)
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;
	size = snd_pcm_format_size(dst_format->format, 1);
	if (dst_format->interleave) {
		switch (size) {
		case 1:
			func = int_1;
			break;
		case 2:
			func = int_2;
			break;
		case 4:
			func = int_4;
			break;
		case 8:
			func = int_8;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (size) {
		case 1:
			func = deint_1;
			break;
		case 2:
			func = deint_2;
			break;
		case 4:
			func = deint_4;
			break;
		case 8:
			func = deint_8;
			break;
		default:
			return -EINVAL;
		}
	}

	plugin = snd_pcm_plugin_build("interleave conversion",
				      sizeof(struct interleave_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct interleave_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->sample_size = size;
	data->voices = src_format->voices;
	data->func = func;
	plugin->transfer = interleave_transfer;
	*r_plugin = plugin;
	return 0;
}
