/*
 *  PCM MMAP Plug-In Interface
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
 *  Basic mmap plugin
 */
 
struct mmap_private_data {
	snd_pcm_t *pcm;
	int channel;
	snd_pcm_mmap_control_t *control;
	char *buffer;
	int frag;
};

static int mmap_transfer_src_ptr(snd_pcm_plugin_t *plugin, char **buffer, size_t *size)
{
	struct mmap_private_data *data;
	snd_pcm_mmap_control_t *control;
	int interleave, err;

	if (plugin == NULL || buffer == NULL || size == NULL)
			return -EINVAL;
	data = (struct mmap_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	control = data->control;
	if (control == NULL)
		return -EINVAL;
	interleave = control->status.voices < 0;
	if (interleave) {
		*buffer = data->buffer + control->fragments[data->frag].addr;
		if (data->channel == SND_PCM_CHANNEL_PLAYBACK) {
			/* wait until the block is not free */
			while (control->fragments[data->frag].data) {
				switch (control->status.status) {
				case SND_PCM_STATUS_PREPARED:
					err = snd_pcm_channel_go(data->pcm, data->channel);
					if (err < 0)
						return err;
					break;
				case SND_PCM_STATUS_RUNNING:
					usleep(10000);
					break;
				default:
					return -EIO;
				}
			}
		}
		*size = control->status.frag_size;
	} else {
		*buffer = NULL;	/* use another buffer */
	}
	return 0;
}

static ssize_t mmap_transfer(snd_pcm_plugin_t *plugin,
			      char *src_ptr, size_t src_size,
			      char *dst_ptr, size_t dst_size)
{
	struct mmap_private_data *data;
	snd_pcm_mmap_control_t *control;
	int interleave, voice, err;
	char *addr;

	if (plugin == NULL || dst_ptr == NULL || dst_size <= 0)
			return -EINVAL;
	data = (struct mmap_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data == NULL)
		return -EINVAL;
	control = data->control;
	if (control == NULL)
		return -EINVAL;
	interleave = control->status.voices < 0;
	if (interleave) {
		if (dst_size != control->status.frag_size)
			return -EINVAL;
	} else {
		if (dst_size != control->status.frag_size * control->status.voices)
			return -EINVAL;
	}
	if (data->channel == SND_PCM_CHANNEL_PLAYBACK) {
		if (interleave) {
			while (control->fragments[data->frag].data) {
				switch (control->status.status) {
				case SND_PCM_STATUS_PREPARED:
					err = snd_pcm_channel_go(data->pcm, data->channel);
					if (err < 0)
						return err;
					break;
				case SND_PCM_STATUS_RUNNING:
					usleep(10000);
					break;
				default:
					return -EIO;
				}
			}
			addr = data->buffer + control->fragments[data->frag].addr;
			if (dst_ptr != addr)
				memcpy(addr, dst_ptr, dst_size);
			control->fragments[data->frag++].data = 1;
			data->frag %= control->status.frags;
		} else {
			int frag;

			for (voice = 0; voice < control->status.voices; voice++) {
				frag = data->frag + (voice * (control->status.frags / control->status.voices));
				while (control->fragments[frag].data) {
					switch (control->status.status) {
					case SND_PCM_STATUS_PREPARED:
						err = snd_pcm_channel_go(data->pcm, data->channel);
						if (err < 0)
							return err;
						break;
					case SND_PCM_STATUS_RUNNING:
						usleep(10000);
						break;
					default:
						return -EIO;
					}
				}
				addr = data->buffer + control->fragments[frag].addr;
				if (dst_ptr != addr)
					memcpy(addr, dst_ptr, control->status.frag_size);
				control->fragments[frag].data = 1;
				dst_ptr += control->status.frag_size;
			}
			data->frag++;
			data->frag %= control->status.frags;
		}
		return dst_size;
	} else if (data->channel == SND_PCM_CHANNEL_CAPTURE) {
		if (interleave) {
			while (!control->fragments[data->frag].data) {
				switch (control->status.status) {
				case SND_PCM_STATUS_PREPARED:
					err = snd_pcm_channel_go(data->pcm, data->channel);
					if (err < 0)
						return err;
					break;
				case SND_PCM_STATUS_RUNNING:
					usleep(10000);
					break;
				default:
					return -EIO;
				}
			}
			addr = data->buffer + control->fragments[data->frag].addr;
			if (dst_ptr != addr)
				memcpy(dst_ptr, addr, dst_size);
			control->fragments[data->frag++].data = 0;
			data->frag %= control->status.frags;
		} else {
			for (voice = 0; voice < control->status.voices; voice++) {
				while (!control->fragments[data->frag].data) {
					switch (control->status.status) {
					case SND_PCM_STATUS_PREPARED:
						err = snd_pcm_channel_go(data->pcm, data->channel);
						if (err < 0)
							return err;
						break;
					case SND_PCM_STATUS_RUNNING:
						usleep(10000);
						break;
					default:
						return -EIO;
					}
				}
				addr = data->buffer + control->fragments[data->frag].addr;
				if (dst_ptr != addr)
					memcpy(dst_ptr, addr, control->status.frag_size);
				control->fragments[data->frag++].data = 0;
				data->frag %= control->status.frags;
				dst_ptr += control->status.frag_size;
			}
		}
		return dst_size;
	} else {
		return -EINVAL;
	}
}
 
static int mmap_action(snd_pcm_plugin_t *plugin, snd_pcm_plugin_action_t action)
{
	struct mmap_private_data *data;

	if (plugin == NULL)
		return -EINVAL;
	data = (struct mmap_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (action == INIT) {
		if (data->control)
			snd_pcm_munmap(data->pcm, data->channel);
		return snd_pcm_mmap(data->pcm, data->channel, &data->control, (void **)&data->buffer);
	} else if (action == PREPARE) {
		data->frag = 0;
	} else if (action == DRAIN && data->channel == SND_PCM_CHANNEL_PLAYBACK) {
		data->frag = 0;
	} else if (action == FLUSH) {
		data->frag = 0;
	}
	return 0;	/* silenty ignore other actions */
}

static void mmap_free(snd_pcm_plugin_t *plugin, void *private_data)
{
	struct mmap_private_data *data;

	if (plugin == NULL)
		return;
	data = (struct mmap_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (data->control)
		snd_pcm_munmap(data->pcm, data->channel);
}
 
int snd_pcm_plugin_build_mmap(snd_pcm_t *pcm, int channel, snd_pcm_plugin_t **r_plugin)
{
	struct mmap_private_data *data;
	snd_pcm_plugin_t *plugin;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	plugin = snd_pcm_plugin_build(channel == SND_PCM_CHANNEL_PLAYBACK ?
						"I/O mmap playback" :
						"I/O mmap capture",
						sizeof(struct mmap_private_data));
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct mmap_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->pcm = pcm;
	data->channel = channel;
	plugin->transfer_src_ptr = mmap_transfer_src_ptr;
	plugin->transfer = mmap_transfer;
	plugin->action = mmap_action;
	plugin->private_free = mmap_free;
	*r_plugin = plugin;
	return 0;
}
