/*
 *  Linear conversion Plug-In
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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

typedef struct copy_private_data {
	int copy;
} copy_t;

static void copy(snd_pcm_plugin_t *plugin,
		 const snd_pcm_plugin_voice_t *src_voices,
		 snd_pcm_plugin_voice_t *dst_voices,
		 size_t samples)
{
#define COPY_LABELS
#include "plugin_ops.h"
#undef COPY_LABELS
	copy_t *data = (copy_t *)plugin->extra_data;
	void *copy = copy_labels[data->copy];
	int voice;
	int nvoices = plugin->src_format.voices;
	for (voice = 0; voice < nvoices; ++voice) {
		char *src;
		char *dst;
		int src_step, dst_step;
		size_t samples1;
		if (!src_voices[voice].enabled) {
			if (dst_voices[voice].wanted)
				snd_pcm_plugin_silence_voice(plugin, &dst_voices[voice], samples);
			dst_voices[voice].enabled = 0;
			continue;
		}
		dst_voices[voice].enabled = 1;
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

static ssize_t copy_transfer(snd_pcm_plugin_t *plugin,
			     const snd_pcm_plugin_voice_t *src_voices,
			     snd_pcm_plugin_voice_t *dst_voices,
			     size_t samples)
{
	copy_t *data;
	unsigned int voice;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL)
		return -EFAULT;
	data = (copy_t *)plugin->extra_data;
	if (samples == 0)
		return 0;
	for (voice = 0; voice < plugin->src_format.voices; voice++) {
		if (src_voices[voice].first % 8 != 0 || 
		    src_voices[voice].step % 8 != 0)
			return -EINVAL;
		if (dst_voices[voice].first % 8 != 0 || 
		    dst_voices[voice].step % 8 != 0)
			return -EINVAL;
	}
	copy(plugin, src_voices, dst_voices, samples);
	return samples;
}

int copy_index(int format)
{
	int size = snd_pcm_format_physical_width(format);
	switch (size) {
	case 8:
		return 0;
	case 16:
		return 1;
	case 32:
		return 2;
	case 64:
		return 3;
	default:
		return -EINVAL;
	}
}
	
int snd_pcm_plugin_build_copy(snd_pcm_plugin_handle_t *handle,
			      int channel,
			      snd_pcm_format_t *format,
			      snd_pcm_plugin_t **r_plugin)
{
	int err;
	struct copy_private_data *data;
	snd_pcm_plugin_t *plugin;
	int copy;

	if (r_plugin == NULL)
		return -EFAULT;
	*r_plugin = NULL;

	copy = copy_index(format->format);
	if (copy < 0)
		return -EINVAL;

	err = snd_pcm_plugin_build(handle, channel,
				   "copy",
				   format,
				   format,
				   sizeof(copy_t),
				   &plugin);
	if (err < 0)
		return err;
	data = (copy_t *)plugin->extra_data;
	data->copy = copy;
	plugin->transfer = copy_transfer;
	*r_plugin = plugin;
	return 0;
}
