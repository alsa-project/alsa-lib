/*
 *  PCM Block Plug-In Interface
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
#include "../pcm_local.h"

/*
 *  Basic block plugin
 */
 
struct block_private_data {
	snd_pcm_t *pcm;
	int channel;
};

static ssize_t block_transfer(snd_pcm_plugin_t *plugin,
			      char *src_ptr, size_t src_size,
			      char *dst_ptr, size_t dst_size)
{
	struct block_private_data *data;

	if (plugin == NULL || dst_ptr == NULL || dst_size <= 0)
			return -EINVAL;
	data = (struct block_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	if (data->channel == SND_PCM_CHANNEL_PLAYBACK) {
		return snd_pcm_write(data->pcm, dst_ptr, dst_size);
	} else if (data->channel == SND_PCM_CHANNEL_CAPTURE) {
		return snd_pcm_read(data->pcm, dst_ptr, dst_size);
	} else {
		return -EINVAL;
	}
}
 
static int block_action(snd_pcm_plugin_t *plugin, snd_pcm_plugin_action_t action)
{
	struct block_private_data *data;

	if (plugin == NULL)
		return -EINVAL;
	data = (struct block_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (action == DRAIN && data->channel == SND_PCM_CHANNEL_PLAYBACK) {
		return snd_pcm_drain_playback(data->pcm);
	} else if (action == FLUSH) {
		return snd_pcm_flush_channel(data->pcm, data->channel);
	}
	return 0;	/* silenty ignore other actions */
}
 
int snd_pcm_plugin_build_block(snd_pcm_t *pcm, int channel, snd_pcm_plugin_t **r_plugin)
{
	struct block_private_data *data;
	snd_pcm_plugin_t *plugin;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	plugin = snd_pcm_plugin_build(channel == SND_PCM_CHANNEL_PLAYBACK ?
						"I/O block playback" :
						"I/O block capture",
						sizeof(struct block_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct block_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->pcm = pcm;
	data->channel = channel;
	plugin->transfer = block_transfer;
	plugin->action = block_action;
	*r_plugin = plugin;
	return 0;
}
