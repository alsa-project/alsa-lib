/*
 *  Linear conversion Plug-In
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>,
 *			  Abramo Bagnara <abramo@alsa-project.org>
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
 *  Basic linear conversion plugin
 */
 
typedef struct linear_private_data {
	int copy;
} linear_t;

static void convert(snd_pcm_plugin_t *plugin,
		    const snd_pcm_plugin_voice_t *src_voices,
		    const snd_pcm_plugin_voice_t *dst_voices,
		    size_t samples)
{
#define COPY_LABELS
#include "plugin_ops.h"
#undef COPY_LABELS
	linear_t *data = (linear_t *)plugin->extra_data;
	void *copy = copy_labels[data->copy];
	int voice;
	int nvoices = plugin->src_format.voices;
	for (voice = 0; voice < nvoices; ++voice) {
		char *src;
		char *dst;
		int src_step, dst_step;
		size_t samples1;
		if (src_voices[voice].addr == NULL) {
			if (dst_voices[voice].addr != NULL) {
//				null_voice(&dst_voices[voice]);
				zero_voice(plugin, &dst_voices[voice], samples);
			}
			continue;
		}
		src = src_voices[voice].addr + src_voices[voice].first / 8;
		dst = dst_voices[voice].addr + dst_voices[voice].first / 8;
		src_step = src_voices[voice].step / 8;
		dst_step = dst_voices[voice].step / 8;
		samples1 = samples;
		while (samples1-- > 0) {
			goto *copy;
#define COPY_END after
#include "plugin_ops.h"
#undef COPY_END
		after:
			src += src_step;
			dst += dst_step;
		}
	}
}

static ssize_t linear_transfer(snd_pcm_plugin_t *plugin,
			       const snd_pcm_plugin_voice_t *src_voices,
			       const snd_pcm_plugin_voice_t *dst_voices,
			       size_t samples)
{
	linear_t *data;
	int voice;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL)
		return -EFAULT;
	data = (linear_t *)plugin->extra_data;
	if (samples < 0)
		return -EINVAL;
	if (samples == 0)
		return 0;
	for (voice = 0; voice < plugin->src_format.voices; voice++) {
		if (src_voices[voice].addr != NULL && 
		    dst_voices[voice].addr == NULL)
			return -EFAULT;
		if (src_voices[voice].first % 8 != 0 || 
		    src_voices[voice].step % 8 != 0)
			return -EINVAL;
		if (dst_voices[voice].first % 8 != 0 || 
		    dst_voices[voice].step % 8 != 0)
			return -EINVAL;
	}
	convert(plugin, src_voices, dst_voices, samples);
	return samples;
}

int copy_index(int src_format, int dst_format)
{
	int src_endian, dst_endian, sign, src_width, dst_width;

	sign = (snd_pcm_format_signed(src_format) !=
		snd_pcm_format_signed(dst_format));
#if __BYTE_ORDER == __LITTLE_ENDIAN
	src_endian = snd_pcm_format_big_endian(src_format);
	dst_endian = snd_pcm_format_big_endian(dst_format);
#elif __BYTE_ORDER == __BIG_ENDIAN
	src_endian = snd_pcm_format_little_endian(src_format);
	dst_endian = snd_pcm_format_little_endian(dst_format);
#else
#error "Unsupported endian..."
#endif

	if (src_endian < 0)
		src_endian = 0;
	if (dst_endian < 0)
		dst_endian = 0;

	src_width = snd_pcm_format_width(src_format) / 8 - 1;
	dst_width = snd_pcm_format_width(dst_format) / 8 - 1;

	return src_width * 32 + src_endian * 16 + sign * 8 + dst_width * 2 + dst_endian;
}

int snd_pcm_plugin_build_linear(snd_pcm_plugin_handle_t *handle,
				snd_pcm_format_t *src_format,
				snd_pcm_format_t *dst_format,
				snd_pcm_plugin_t **r_plugin)
{
	struct linear_private_data *data;
	snd_pcm_plugin_t *plugin;

	if (r_plugin == NULL)
		return -EFAULT;
	*r_plugin = NULL;

	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;
	if (!(snd_pcm_format_linear(src_format->format) &&
	      snd_pcm_format_linear(dst_format->format)))
		return -EINVAL;

	plugin = snd_pcm_plugin_build(handle,
				      "linear format conversion",
				      src_format,
				      dst_format,
				      sizeof(linear_t));
	if (plugin == NULL)
		return -ENOMEM;
	data = (linear_t *)plugin->extra_data;
	data->copy = copy_index(src_format->format, dst_format->format);
	plugin->transfer = linear_transfer;
	*r_plugin = plugin;
	return 0;
}
