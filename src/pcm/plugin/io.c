/*
 *  PCM I/O Plug-In Interface
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
#define snd_pcm_write(handle,buf,count) snd_pcm_oss_write3(handle,buf,count,1)
#define snd_pcm_writev(handle,vec,count) snd_pcm_oss_writev3(handle,vec,count,1)
#define snd_pcm_read(handle,buf,count) snd_pcm_oss_read3(handle,buf,count,1)
#define snd_pcm_readv(handle,vec,count) snd_pcm_oss_readv3(handle,vec,count,1)
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>
#include "../pcm_local.h"
#endif

/*
 *  Basic io plugin
 */
 
typedef struct io_private_data {
	snd_pcm_plugin_handle_t *slave;
} io_t;

static ssize_t io_transfer(snd_pcm_plugin_t *plugin,
			      const snd_pcm_plugin_channel_t *src_channels,
			      snd_pcm_plugin_channel_t *dst_channels,
			      size_t frames)
{
	io_t *data;
	ssize_t result;
	struct iovec *vec;
	int count, channel;

	assert(plugin);
	data = (io_t *)plugin->extra_data;
	assert(data);
	vec = (struct iovec *)((char *)data + sizeof(*data));
	if (plugin->stream == SND_PCM_STREAM_PLAYBACK) {
		assert(src_channels);
		if ((result = snd_pcm_plugin_src_frames_to_size(plugin, frames)) < 0)
			return result;
		count = plugin->src_format.channels;
		if (plugin->src_format.interleave) {
			result = snd_pcm_write(data->slave, src_channels->area.addr, result);
		} else {
			result /= count;
			for (channel = 0; channel < count; channel++) {
				if (src_channels[channel].enabled)
					vec[channel].iov_base = src_channels[channel].area.addr;
				else
					vec[channel].iov_base = 0;
				vec[channel].iov_len = result;
			}
			result = snd_pcm_writev(data->slave, vec, count);
		}
		if (result < 0)
			return result;
		return snd_pcm_plugin_src_size_to_frames(plugin, result);
	} else if (plugin->stream == SND_PCM_STREAM_CAPTURE) {
		assert(dst_channels);
		if ((result = snd_pcm_plugin_dst_frames_to_size(plugin, frames)) < 0)
			return result;
		count = plugin->dst_format.channels;
		if (plugin->dst_format.interleave) {
			result = snd_pcm_read(data->slave, dst_channels->area.addr, result);
			for (channel = 0; channel < count; channel++) {
				dst_channels[channel].enabled = src_channels[channel].enabled;
			}
		} else {
			result /= count;
			for (channel = 0; channel < count; channel++) {
				dst_channels[channel].enabled = src_channels[channel].enabled;
				if (dst_channels[channel].enabled)
					vec[channel].iov_base = dst_channels[channel].area.addr;
				else
					vec[channel].iov_base = 0;
				vec[channel].iov_len = result;
			}
			result = snd_pcm_readv(data->slave, vec, count);
		}
		if (result < 0)
			return result;
		return snd_pcm_plugin_dst_size_to_frames(plugin, result);
	} else {
		assert(0);
	}
	return 0;
}
 
static ssize_t io_src_channels(snd_pcm_plugin_t *plugin,
			     size_t frames,
			     snd_pcm_plugin_channel_t **channels)
{
	int err;
	unsigned int channel;
	snd_pcm_plugin_channel_t *v;
	err = snd_pcm_plugin_client_channels(plugin, frames, &v);
	if (err < 0)
		return err;
	*channels = v;
	for (channel = 0; channel < plugin->src_format.channels; ++channel, ++v)
		v->wanted = 1;
	return frames;
}

int snd_pcm_plugin_build_io(snd_pcm_plugin_handle_t *pcm,
			       int stream,
			       snd_pcm_plugin_handle_t *slave,
			       snd_pcm_format_t *format,
			       snd_pcm_plugin_t **r_plugin)
{
	int err;
	io_t *data;
	snd_pcm_plugin_t *plugin;

	assert(r_plugin);
	*r_plugin = NULL;
	assert(pcm && format);
	err = snd_pcm_plugin_build(pcm, stream,
				   "I/O io",
				   format, format,
				   sizeof(io_t) + sizeof(struct iovec) * format->channels,
				   &plugin);
	if (err < 0)
		return err;
	data = (io_t *)plugin->extra_data;
	data->slave = slave;
	plugin->transfer = io_transfer;
	if (format->interleave && stream == SND_PCM_STREAM_PLAYBACK)
		plugin->client_channels = io_src_channels;
	*r_plugin = plugin;
	return 0;
}
