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
#include <sys/poll.h>
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
	int start_mode, stop_mode;
	int frags, frags_used;
	int frags_min, frags_max;
	unsigned int lastblock;
};

static int playback_ok(struct mmap_private_data *data)
{
	snd_pcm_mmap_control_t *control = data->control;
	int delta = control->status.block;
	
	if (delta < data->lastblock) {
		delta += (~0 - data->lastblock) + 1;
	} else {
		delta -= data->lastblock;
	}
	data->frags_used -= delta;
	if (data->frags_used < 0) {
		/* correction for rollover */
		data->frag += -data->frags_used;
		data->frag %= data->frags;
		data->frags_used = 0;
	}
	data->lastblock += delta;
	return data->frags_used <= data->frags_max &&
	       (data->frags - data->frags_used) >= data->frags_min;
}

static int poll_playback(snd_pcm_t *pcm)
{
	int err;
	struct pollfd pfd;
	
	if (pcm->mode & SND_PCM_OPEN_NONBLOCK)
		return -EAGAIN;
	pfd.fd = pcm->fd[SND_PCM_CHANNEL_PLAYBACK];
	pfd.events = POLLOUT;
	pfd.revents = 0;
	err = poll(&pfd, 1, 1000);
	return err < 0 ? err : 0;
}

static int query_playback(struct mmap_private_data *data, int not_use_poll)
{
	snd_pcm_mmap_control_t *control = data->control;
	int err;

	switch (control->status.status) {
	case SND_PCM_STATUS_PREPARED:
		if (data->start_mode == SND_PCM_START_GO)
			return -EAGAIN;
		if ((data->start_mode == SND_PCM_START_DATA &&
		     playback_ok(data)) ||
		    (data->start_mode == SND_PCM_START_FULL &&
		     data->frags_used == data->frags)) {
			err = snd_pcm_channel_go(data->pcm, data->channel);
			if (err < 0)
				return err;
		}
		break;
	case SND_PCM_STATUS_RUNNING:
		if (!not_use_poll) {
			control->status.expblock = control->status.block + 1;
			err = poll_playback(data->pcm);
			if (err < 0)
				return err;
		}
		break;
	case SND_PCM_STATUS_UNDERRUN:
		return -EPIPE;
	default:
		return -EIO;
	}
	return 0;
}

static int capture_ok(struct mmap_private_data *data)
{
	snd_pcm_mmap_control_t *control = data->control;
	int delta = control->status.block;
	
	if (delta < data->lastblock) {
		delta += (~0 - data->lastblock) + 1;
	} else {
		delta -= data->lastblock;
	}
	data->frags_used += delta;
	if (data->frags_used > data->frags) {
		/* correction for rollover */
		data->frag += data->frags_used - data->frags;
		data->frag %= data->frags;
		data->frags_used = data->frags;
	}
	data->lastblock += delta;
	return data->frags_used >= data->frags_min;
}

static int poll_capture(snd_pcm_t *pcm)
{
	int err;
	struct pollfd pfd;

	if (pcm->mode & SND_PCM_OPEN_NONBLOCK)
		return -EAGAIN;
	pfd.fd = pcm->fd[SND_PCM_CHANNEL_CAPTURE];
	pfd.events = POLLIN;
	pfd.revents = 0;
	err = poll(&pfd, 1, 1000);
	return err < 0 ? err : 0;
}

static int query_capture(struct mmap_private_data *data, int not_use_poll)
{
	snd_pcm_mmap_control_t *control = data->control;
	int err;

	switch (control->status.status) {
	case SND_PCM_STATUS_PREPARED:
		if (data->start_mode != SND_PCM_START_DATA)
			return -EAGAIN;
		err = snd_pcm_channel_go(data->pcm, data->channel);
		if (err < 0)
			return err;
		break;
	case SND_PCM_STATUS_RUNNING:
		if (!not_use_poll) {
			control->status.expblock = control->status.block + data->frags_min;
			err = poll_capture(data->pcm);
			if (err < 0)
				return err;
		}
		break;
	case SND_PCM_STATUS_OVERRUN:
		return -EPIPE;
	default:
		return -EIO;
	}
	return 0;
}

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
			while (!playback_ok(data)) {
				err = query_playback(data, 0);
				if (err < 0)
					return err;
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
		while (!playback_ok(data)) {
			err = query_playback(data, 0);
			if (err < 0)
				return err;
		}
		if (interleave) {
			addr = data->buffer + control->fragments[data->frag].addr;
			if (dst_ptr != addr)
				memcpy(addr, dst_ptr, dst_size);
			control->fragments[data->frag++].data = 1;
			data->frag %= control->status.frags;
			data->frags_used++;
		} else {
			int frag;

			for (voice = 0; voice < control->status.voices; voice++) {
				frag = data->frag + (voice * data->frags);
				while (control->fragments[frag].data) {
					err = query_playback(data, 1);
					if (err < 0)
						return err;
				}
				addr = data->buffer + control->fragments[frag].addr;
				if (dst_ptr != addr)
					memcpy(addr, dst_ptr, control->status.frag_size);
				control->fragments[frag].data = 1;
				dst_ptr += control->status.frag_size;
			}
			data->frag++;
			data->frag %= data->frags;
			data->frags_used++;
		}
		return dst_size;
	} else if (data->channel == SND_PCM_CHANNEL_CAPTURE) {
		while (!capture_ok(data)) {
			err = query_capture(data, 0);
			if (err < 0)
				return err;
		}
		if (interleave) {
			addr = data->buffer + control->fragments[data->frag].addr;
			if (dst_ptr != addr)
				memcpy(dst_ptr, addr, dst_size);
			control->fragments[data->frag++].data = 0;
			data->frag %= control->status.frags;
			data->frags_used--;
		} else {
			int frag;

			for (voice = 0; voice < control->status.voices; voice++) {
				frag = data->frag + (voice * data->frags);
				while (!control->fragments[data->frag].data) {
					err = query_capture(data, 1);
					if (err < 0)
						return err;
				}
				addr = data->buffer + control->fragments[frag].addr;
				if (dst_ptr != addr)
					memcpy(dst_ptr, addr, control->status.frag_size);
				control->fragments[frag].data = 0;
				dst_ptr += control->status.frag_size;
			}
			data->frag++;
			data->frag %= data->frags;
			data->frags_used--;
		}
		return dst_size;
	} else {
		return -EINVAL;
	}
}
 
static int mmap_action(snd_pcm_plugin_t *plugin,
		       snd_pcm_plugin_action_t action,
		       unsigned long udata)
{
	struct mmap_private_data *data;

	if (plugin == NULL)
		return -EINVAL;
	data = (struct mmap_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (action == INIT) {
		snd_pcm_channel_params_t *params;
		snd_pcm_channel_setup_t setup;
		int result, frags;

		if (data->control)
			snd_pcm_munmap(data->pcm, data->channel);
		result = snd_pcm_mmap(data->pcm, data->channel, &data->control, (void **)&data->buffer);
		if (result < 0)
			return result;
		params = (snd_pcm_channel_params_t *)udata;
		data->start_mode = params->start_mode;
		data->stop_mode = params->stop_mode;
		memset(&setup, 0, sizeof(setup));
		setup.channel = data->channel;
		if ((result = snd_pcm_channel_setup(data->pcm, &setup)) < 0)
			return result;
		data->frags = setup.buf.block.frags;
		data->frags_min = setup.buf.block.frags_min;
		data->frags_max = setup.buf.block.frags_max;
		if (data->frags_min < 0)
			data->frags_min = 0;
		if (data->frags_min >= setup.buf.block.frags)
			data->frags_min = setup.buf.block.frags - 1;
		if (data->frags_max < 0)
			data->frags_max = setup.buf.block.frags + data->frags_max;
		if (data->frags_max < data->frags_min)
			data->frags_max = data->frags_min;
		if (data->frags_max < 1)
			data->frags_max = 1;
		if (data->frags_max > setup.buf.block.frags)
			data->frags_max = setup.buf.block.frags;
		return 0;
	} else if (action == PREPARE) {
		data->frag = 0;
		data->lastblock = 0;
	} else if (action == DRAIN && data->channel == SND_PCM_CHANNEL_PLAYBACK) {
		data->frag = 0;
		data->lastblock = 0;
	} else if (action == FLUSH) {
		data->frag = 0;
		data->lastblock = 0;
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
