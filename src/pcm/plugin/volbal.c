/*
 *  Volume and balance Plug-In
 *  Copyright (c) 1999 by Abramo Bagnara <abbagnara@racine.ra.it>
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


#define VOLBAL_RESOLUTION 16

struct volbal_private_data {
	int src_voices;
	int noop;
	int ttable[0];
};

static void volbal(int voices, int samples, int *ttable,
		   signed short *src_ptr, signed short *dst_ptr)
{
	while (samples-- > 0) {
		int dst_voice;
		int *t = ttable;
		for (dst_voice = 0; dst_voice < voices; ++dst_voice) {
			int v = 0;
			int src_voice;
			signed short *s = src_ptr;
			for (src_voice = 0; src_voice < voices; ++src_voice) {
				v +=  *s++ * *t++ / VOLBAL_RESOLUTION;
			}
			*dst_ptr++ = v;
			src_ptr += voices;
		}
	}
}

static ssize_t volbal_transfer(snd_pcm_plugin_t *plugin,
			     char *src_ptr, size_t src_size,
			     char *dst_ptr, size_t dst_size)
{
	struct volbal_private_data *data;
	if (plugin == NULL || src_ptr == NULL || src_size < 0 ||
	                      dst_ptr == NULL || dst_size < 0)
		return -EINVAL;
	if (src_size == 0)
		return 0;
	data = (struct volbal_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	if (data->noop)
		return 0;

	volbal(data->src_voices, src_size / 2 / data->src_voices, data->ttable,
	       (signed short *)src_ptr, (signed short *)dst_ptr);
	return src_size;
}

static ssize_t volbal_src_size(snd_pcm_plugin_t *plugin, size_t size)
{
	if (!plugin || size <= 0)
		return -EINVAL;
	return size;
}

static ssize_t volbal_dst_size(snd_pcm_plugin_t *plugin, size_t size)
{
	if (!plugin || size <= 0)
		return -EINVAL;
	return size;
}


static int volbal_load_ttable(struct volbal_private_data *data, 
			      const int *src_ttable)
{
	int src_voice, dst_voice;
	const int *sptr;
	int *dptr;
	data->noop = 1;
	if (src_ttable == NULL)
		return 0;
	sptr = src_ttable;
	dptr = data->ttable;
	for (dst_voice = 0; dst_voice < data->src_voices; ++dst_voice) {
		int t = 0;
		for (src_voice = 0; src_voice < data->src_voices; ++src_voice) {
			if (*sptr < 0 || *sptr > VOLBAL_RESOLUTION)
				return -EINVAL;
			if (src_voice == dst_voice) {
				if (*sptr != VOLBAL_RESOLUTION)
					data->noop = 0;
			}
			else {
				if (*sptr != 0)
					data->noop = 0;
			}
			t += *sptr;
			*dptr++ = *sptr++;
		}
		if (t > VOLBAL_RESOLUTION)
			return -EINVAL;
	}
	return 0;
}


int snd_pcm_plugin_build_volbal(snd_pcm_format_t *src_format,
				snd_pcm_format_t *dst_format,
				int *ttable,
				snd_pcm_plugin_t **r_plugin)
{
	struct volbal_private_data *data;
	snd_pcm_plugin_t *plugin;
	int res;

	if (!r_plugin)
		return -EINVAL;
	*r_plugin = NULL;
	if (src_format->interleave != dst_format->interleave)
		return -EINVAL;
	if (!dst_format->interleave)
		return -EINVAL;
	if (src_format->format != dst_format->format)
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;

	if (src_format->voices < 1)
		return -EINVAL;
	if (src_format->format != SND_PCM_SFMT_S16_LE)
		return -EINVAL;
	plugin = snd_pcm_plugin_build("Volume/balance conversion",
				      sizeof(struct volbal_private_data) + 
				      sizeof(data->ttable[0]) * src_format->voices * src_format->voices);
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct volbal_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->src_voices = src_format->voices;
	if ((res = volbal_load_ttable(data, ttable)) < 0)
		return res;

	plugin->transfer = volbal_transfer;
	plugin->src_size = volbal_src_size;
	plugin->dst_size = volbal_dst_size;
	*r_plugin = plugin;
	return 0;
}
