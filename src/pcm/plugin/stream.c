/*
 *  PCM Stream Plug-In Interface
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
#include <sys/uio.h>
#include "../pcm_local.h"

/*
 *  Basic stream plugin
 */
 
typedef struct stream_private_data {
	int channel;
} stream_t;

static ssize_t stream_transfer(snd_pcm_plugin_t *plugin,
			       const snd_pcm_plugin_voice_t *src_voices,
			       const snd_pcm_plugin_voice_t *dst_voices,
			       size_t samples)
{
	stream_t *data;
	ssize_t result;
	struct iovec *vec;
	int count, voice;

	if (plugin == NULL)
		return -EINVAL;
	data = (stream_t *)plugin->extra_data;
	if (data == NULL)
		return -EINVAL;
	vec = (struct iovec *)((char *)data + sizeof(*data));
	if (data->channel == SND_PCM_CHANNEL_PLAYBACK) {
		if (src_voices == NULL)
			return -EINVAL;
		if ((result = snd_pcm_plugin_src_samples_to_size(plugin, samples)) < 0)
			return result;
		if (plugin->src_format.interleave) {
			result = snd_pcm_write(plugin->handle, src_voices->addr, result);
		} else {
			count = plugin->src_format.voices;
			result /= count;
			for (voice = 0; voice < count; voice++) {
				vec[voice].iov_base = src_voices[voice].addr;
				vec[voice].iov_len = result;
			}
			result = snd_pcm_writev(plugin->handle, vec, count);
		}
		if (result < 0)
			return result;
		return snd_pcm_plugin_src_size_to_samples(plugin, result);
	} else if (data->channel == SND_PCM_CHANNEL_CAPTURE) {
		if (dst_voices == NULL)
			return -EINVAL;
		if ((result = snd_pcm_plugin_dst_samples_to_size(plugin, samples)) < 0)
			return result;
		if (plugin->dst_format.interleave) {
			result = snd_pcm_read(plugin->handle, dst_voices->addr, result);
			
		} else {
			count = plugin->dst_format.voices;
			result /= count;
			for (voice = 0; voice < count; voice++) {
				vec[voice].iov_base = dst_voices[voice].addr;
				vec[voice].iov_len = result;
			}
			result = snd_pcm_readv(plugin->handle, vec, count);
		}
		if (result < 0)
			return result;
		return snd_pcm_plugin_dst_size_to_samples(plugin, result);
	} else {
		return -EINVAL;
	}
}
 
int snd_pcm_plugin_build_stream(snd_pcm_t *pcm, int channel,
				snd_pcm_format_t *format,
				snd_pcm_plugin_t **r_plugin)
{
	stream_t *data;
	snd_pcm_plugin_t *plugin;

	if (!r_plugin)
		return -EINVAL;
	*r_plugin = NULL;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	plugin = snd_pcm_plugin_build(pcm,
				      channel == SND_PCM_CHANNEL_PLAYBACK ?
						"I/O stream playback" :
						"I/O stream capture",
				      format, format,
				      sizeof(stream_t) + sizeof(struct iovec) * format->voices);
	if (plugin == NULL)
		return -ENOMEM;
	data = (stream_t *)plugin->extra_data;
	data->channel = channel;
	plugin->transfer = stream_transfer;
	*r_plugin = plugin;
	return 0;
}
