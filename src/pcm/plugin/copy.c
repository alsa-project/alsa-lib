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

static ssize_t copy_transfer(snd_pcm_plugin_t *plugin,
			     const snd_pcm_plugin_channel_t *src_channels,
			     snd_pcm_plugin_channel_t *dst_channels,
			     size_t frames)
{
	unsigned int channel;
	unsigned int nchannels;

	if (plugin == NULL || src_channels == NULL || dst_channels == NULL)
		return -EFAULT;
	if (frames == 0)
		return 0;
	nchannels = plugin->src_format.channels;
	for (channel = 0; channel < nchannels; channel++) {
		if (src_channels->area.first % 8 != 0 || 
		    src_channels->area.step % 8 != 0)
			return -EINVAL;
		if (dst_channels->area.first % 8 != 0 || 
		    dst_channels->area.step % 8 != 0)
			return -EINVAL;
		if (!src_channels->enabled) {
			if (dst_channels->wanted)
				snd_pcm_area_silence(&dst_channels->area, 0, frames, plugin->dst_format.format);
			dst_channels->enabled = 0;
			continue;
		}
		dst_channels->enabled = 1;
		snd_pcm_area_copy(&src_channels->area, 0, &dst_channels->area, 0, frames, plugin->src_format.format);
		src_channels++;
		dst_channels++;
	}
	return frames;
}

int snd_pcm_plugin_build_copy(snd_pcm_plugin_handle_t *handle,
			      int stream,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin)
{
	int err;
	snd_pcm_plugin_t *plugin;
	int width;

	if (r_plugin == NULL)
		return -EFAULT;
	*r_plugin = NULL;

	if (src_format->format != dst_format->format)
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->channels != dst_format->channels)
		return -EINVAL;

	width = snd_pcm_format_physical_width(src_format->format);
	if (width < 0)
		return -EINVAL;

	err = snd_pcm_plugin_build(handle, stream,
				   "copy",
				   src_format,
				   dst_format,
				   0,
				   &plugin);
	if (err < 0)
		return err;
	plugin->transfer = copy_transfer;
	*r_plugin = plugin;
	return 0;
}
