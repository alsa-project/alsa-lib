/*
 *  Interleave / non-interleave conversion Plug-In
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>,
 *			  Jaroslav Kysela <perex@suse.cz>
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
#include <sys/uio.h>
#include "../pcm_local.h"
#endif

/*
 *  Basic interleave / non-interleave conversion plugin
 */
 
typedef void (*interleave_f)(const snd_pcm_plugin_voice_t *src_voices,
			     const snd_pcm_plugin_voice_t *dst_voices,
			     int voices, size_t samples);

typedef struct interleave_private_data {
	interleave_f func;
} interleave_t;


#define INTERLEAVE_FUNC(name, type) \
static void name(const snd_pcm_plugin_voice_t *src_voices, \
		 const snd_pcm_plugin_voice_t *dst_voices, \
		 int voices, size_t samples) \
{ \
	type *src, *dst; \
	int voice, sample; \
	for (voice = 0; voice < voices; voice++) { \
		src = (type *)src_voices[voice].addr; \
		dst = (type *)dst_voices[voice].addr + voice; \
		for (sample = 0; sample < samples; sample++) { \
			*dst = *src++; \
			dst += voices; \
		} \
	} \
} \

#define DEINTERLEAVE_FUNC(name, type) \
static void name(const snd_pcm_plugin_voice_t *src_voices, \
		 const snd_pcm_plugin_voice_t *dst_voices, \
		 int voices, size_t samples) \
{ \
	type *src, *dst; \
	int voice, sample; \
	for (voice = 0; voice < voices; voice++) { \
		src = (type *)src_voices[voice].addr + voice; \
		dst = (type *)dst_voices[voice].addr; \
		for (sample = 0; sample < samples; sample++) { \
			*dst++ = *src; \
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
				   const snd_pcm_plugin_voice_t *src_voices,
				   const snd_pcm_plugin_voice_t *dst_voices,
				   size_t samples)
{
	interleave_t *data;

	if (plugin == NULL || src_voices == NULL || src_voices == NULL || samples < 0)
		return -EINVAL;
	if (samples == 0)
		return 0;
	data = (interleave_t *)plugin->extra_data;
	if (data == NULL)
		return -EINVAL;
	data->func(src_voices, dst_voices, plugin->src_format.voices, samples);
	return samples;
}

int snd_pcm_plugin_build_interleave(snd_pcm_plugin_handle_t *handle,
				    snd_pcm_format_t *src_format,
				    snd_pcm_format_t *dst_format,
				    snd_pcm_plugin_t **r_plugin)
{
	struct interleave_private_data *data;
	snd_pcm_plugin_t *plugin;
	interleave_f func;

	if (r_plugin == NULL || src_format == NULL || dst_format == NULL)
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
	if (!src_format->interleave) {
		switch (snd_pcm_format_width(src_format->format)) {
		case 8:
			func = int_1;
			break;
		case 16:
			func = int_2;
			break;
		case 32:
			func = int_4;
			break;
		case 64:
			func = int_8;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (snd_pcm_format_width(src_format->format)) {
		case 8:
			func = deint_1;
			break;
		case 16:
			func = deint_2;
			break;
		case 32:
			func = deint_4;
			break;
		case 64:
			func = deint_8;
			break;
		default:
			return -EINVAL;
		}
	}

	plugin = snd_pcm_plugin_build(handle,
				      "interleave conversion",
				      src_format, dst_format,
				      sizeof(interleave_t));
	if (plugin == NULL)
		return -ENOMEM;
	data = (interleave_t *)plugin->extra_data;
	data->func = func;
	plugin->transfer = interleave_transfer;
	*r_plugin = plugin;
	return 0;
}
