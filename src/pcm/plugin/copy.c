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
			     const snd_pcm_plugin_voice_t *src_voices,
			     snd_pcm_plugin_voice_t *dst_voices,
			     size_t samples)
{
	unsigned int voice;
	unsigned int nvoices;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL)
		return -EFAULT;
	if (samples == 0)
		return 0;
	nvoices = plugin->src_format.voices;
	for (voice = 0; voice < nvoices; voice++) {
		if (src_voices->area.first % 8 != 0 || 
		    src_voices->area.step % 8 != 0)
			return -EINVAL;
		if (dst_voices->area.first % 8 != 0 || 
		    dst_voices->area.step % 8 != 0)
			return -EINVAL;
		if (!src_voices->enabled) {
			if (dst_voices->wanted)
				snd_pcm_area_silence(&dst_voices->area, 0, samples, plugin->dst_format.format);
			dst_voices->enabled = 0;
			continue;
		}
		dst_voices->enabled = 1;
		snd_pcm_area_copy(&src_voices->area, 0, &dst_voices->area, 0, samples, plugin->src_format.format);
		src_voices++;
		dst_voices++;
	}
	return samples;
}

int snd_pcm_plugin_build_copy(snd_pcm_plugin_handle_t *handle,
			      int channel,
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
	if (src_format->voices != dst_format->voices)
		return -EINVAL;

	width = snd_pcm_format_physical_width(src_format->format);
	if (width < 0)
		return -EINVAL;

	err = snd_pcm_plugin_build(handle, channel,
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
