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
#define pcm_write(plug,buf,count) snd_pcm_oss_write3(plug,buf,count,1)
#define pcm_writev(plug,vec,count) snd_pcm_oss_writev3(plug,vec,count,1)
#define pcm_read(plug,buf,count) snd_pcm_oss_read3(plug,buf,count,1)
#define pcm_readv(plug,vec,count) snd_pcm_oss_readv3(plug,vec,count,1)
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>
#include "../pcm_local.h"
#define pcm_write(plug,buf,count) snd_pcm_write(plug->slave,buf,count)
#define pcm_writev(plug,vec,count) snd_pcm_writev(plug->slave,vec,count)
#define pcm_read(plug,buf,count) snd_pcm_read(plug->slave,buf,count)
#define pcm_readv(plug,vec,count) snd_pcm_readv(plug->slave,vec,count)
#endif

/*
 *  Basic io plugin
 */
 
static ssize_t io_playback_transfer(snd_pcm_plugin_t *plugin,
				    const snd_pcm_plugin_channel_t *src_channels,
				    snd_pcm_plugin_channel_t *dst_channels UNUSED,
				    size_t frames)
{
	struct iovec *vec;
	int count, channel;

	assert(plugin);
	vec = (struct iovec *)plugin->extra_data;
	assert(vec);
	assert(src_channels);
	count = plugin->src_format.channels;
	if (plugin->src_format.interleave) {
		return pcm_write(plugin->plug, src_channels->area.addr, frames);
	} else {
		for (channel = 0; channel < count; channel++) {
			if (src_channels[channel].enabled)
				vec[channel].iov_base = src_channels[channel].area.addr;
			else
				vec[channel].iov_base = 0;
			vec[channel].iov_len = frames;
		}
		return pcm_writev(plugin->plug, vec, count);
	}
}
 
static ssize_t io_capture_transfer(snd_pcm_plugin_t *plugin,
				   const snd_pcm_plugin_channel_t *src_channels UNUSED,
				   snd_pcm_plugin_channel_t *dst_channels,
				   size_t frames)
{
	struct iovec *vec;
	int count, channel;

	assert(plugin);
	vec = (struct iovec *)plugin->extra_data;
	assert(vec);
	assert(dst_channels);
	count = plugin->dst_format.channels;
	if (plugin->dst_format.interleave) {
		return pcm_read(plugin->plug, dst_channels->area.addr, frames);
	} else {
		for (channel = 0; channel < count; channel++) {
			if (dst_channels[channel].enabled)
				vec[channel].iov_base = dst_channels[channel].area.addr;
			else
				vec[channel].iov_base = 0;
			vec[channel].iov_len = frames;
		}
		return pcm_readv(plugin->plug, vec, count);
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
	if (plugin->src_format.interleave) {
		for (channel = 0; channel < plugin->src_format.channels; ++channel, ++v)
			v->wanted = 1;
	}
	return frames;
}

int snd_pcm_plugin_build_io(snd_pcm_plug_t *plug,
			    snd_pcm_format_t *format,
			    snd_pcm_plugin_t **r_plugin)
{
	int err;
	snd_pcm_plugin_t *plugin;

	assert(r_plugin);
	*r_plugin = NULL;
	assert(plug && format);
	err = snd_pcm_plugin_build(plug, "I/O io",
				   format, format,
				   sizeof(struct iovec) * format->channels,
				   &plugin);
	if (err < 0)
		return err;
	if (snd_pcm_plug_stream(plug) == SND_PCM_STREAM_PLAYBACK) {
		plugin->transfer = io_playback_transfer;
		if (format->interleave)
			plugin->client_channels = io_src_channels;
	} else {
		plugin->transfer = io_capture_transfer;
	}

	*r_plugin = plugin;
	return 0;
}
