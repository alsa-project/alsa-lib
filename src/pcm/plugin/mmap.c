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
#include <sys/uio.h>
#include "../pcm_local.h"

/*
 *  Basic mmap plugin
 */
 
typedef struct mmap_private_data {
	int channel;
	snd_pcm_mmap_control_t *control;
	char *buffer;
	int frag;
	int start_mode, stop_mode;
	int frags, frags_used;
	int frags_min, frags_max;
	unsigned int lastblock;
	struct iovec *vector;
	int vector_count;
} mmap_t;

static int playback_ok(snd_pcm_plugin_t *plugin)
{
	mmap_t *data = (mmap_t *)plugin->extra_data;
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

static int query_playback(snd_pcm_plugin_t *plugin, int not_use_poll)
{
	mmap_t *data = (mmap_t *)plugin->extra_data;
	snd_pcm_mmap_control_t *control = data->control;
	int err;

	switch (control->status.status) {
	case SND_PCM_STATUS_PREPARED:
		if (data->start_mode == SND_PCM_START_GO)
			return -EAGAIN;
		if ((data->start_mode == SND_PCM_START_DATA &&
		     playback_ok(plugin)) ||
		    (data->start_mode == SND_PCM_START_FULL &&
		     data->frags_used == data->frags)) {
			err = snd_pcm_channel_go(plugin->handle, data->channel);
			if (err < 0)
				return err;
		}
		break;
	case SND_PCM_STATUS_RUNNING:
		if (!not_use_poll) {
			control->status.expblock = control->status.block + 1;
			err = poll_playback(plugin->handle);
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

static int capture_ok(snd_pcm_plugin_t *plugin)
{
	mmap_t *data = (mmap_t *)plugin->extra_data;
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

static int query_capture(snd_pcm_plugin_t *plugin, int not_use_poll)
{
	mmap_t *data = (mmap_t *)plugin->extra_data;	
	snd_pcm_mmap_control_t *control = data->control;
	int err;

	switch (control->status.status) {
	case SND_PCM_STATUS_PREPARED:
		if (data->start_mode != SND_PCM_START_DATA)
			return -EAGAIN;
		err = snd_pcm_channel_go(plugin->handle, data->channel);
		if (err < 0)
			return err;
		break;
	case SND_PCM_STATUS_RUNNING:
		if (!not_use_poll) {
			control->status.expblock = control->status.block + data->frags_min;
			err = poll_capture(plugin->handle);
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

static int mmap_src_voices(snd_pcm_plugin_t *plugin,
			   snd_pcm_plugin_voice_t **voices,
			   size_t samples,
			   void *(*plugin_alloc)(snd_pcm_plugin_handle_t *handle, size_t size))
{
	mmap_t *data;
	snd_pcm_mmap_control_t *control;
	snd_pcm_plugin_voice_t *v;
	int err;

	if (plugin == NULL || voices == NULL)
		return -EINVAL;
	*voices = NULL;
	data = (mmap_t *)plugin->extra_data;
	if (data->channel != SND_PCM_CHANNEL_PLAYBACK)
		return -EINVAL;
	if ((control = data->control) == NULL)
		return -EBADFD;
	/* wait until the block is not free */
	while (!playback_ok(plugin)) {
		err = query_playback(plugin, 0);
		if (err < 0)
			return err;
	}	
	v = plugin->voices;
	if (plugin->src_format.interleave) {
		void *addr;
		int voice;
		if (control->status.frag_size != snd_pcm_plugin_dst_samples_to_size(plugin, samples))
			return -EINVAL;
		addr = data->buffer + control->fragments[data->frag].addr;
		for (voice = 0; voice < plugin->src_format.voices; voice++, v++) {
			v->aptr = NULL;
			v->addr = addr;
			v->offset = voice * plugin->src_width;
			v->next = plugin->src_format.voices * plugin->src_width;
		}
	} else {
		int frag, voice;
		if (control->status.frag_size != snd_pcm_plugin_src_samples_to_size(plugin, samples) / plugin->src_format.voices)
			return -EINVAL;
		for (voice = 0; voice < plugin->src_format.voices; voice++, v++) {
			frag = data->frag + (voice * data->frags);
			v->aptr = NULL;
			v->addr = data->buffer + control->fragments[frag].addr;
			v->offset = 0;
			v->next = plugin->src_width;
		}
	}
	*voices = plugin->voices;
	return 0;
}

static int mmap_dst_voices(snd_pcm_plugin_t *plugin,
			   snd_pcm_plugin_voice_t **voices,
			   size_t samples,
			   void *(*plugin_alloc)(snd_pcm_plugin_handle_t *handle, size_t size))
{
	mmap_t *data;
	snd_pcm_mmap_control_t *control;
	snd_pcm_plugin_voice_t *v;

	if (plugin == NULL || voices == NULL)
		return -EINVAL;
	*voices = NULL;
	data = (mmap_t *)plugin->extra_data;
	if (data->channel != SND_PCM_CHANNEL_CAPTURE)
		return -EINVAL;
	if ((control = data->control) == NULL)
		return -EBADFD;
	v = plugin->voices;
	if (plugin->dst_format.interleave) {
		void *addr;
		int voice;
		if (control->status.frag_size != snd_pcm_plugin_dst_samples_to_size(plugin, samples))
			return -EINVAL;
		addr = data->buffer + control->fragments[data->frag].addr;
		for (voice = 0; voice < plugin->dst_format.voices; voice++, v++) {
			v->addr = addr;
			v->offset = voice * plugin->src_width;
			v->next = plugin->dst_format.voices * plugin->dst_width;
		}
	} else {
		int frag, voice;
		if (control->status.frag_size != snd_pcm_plugin_dst_samples_to_size(plugin, samples) / plugin->dst_format.voices)
			return -EINVAL;
		for (voice = 0; voice < plugin->dst_format.voices; voice++, v++) {
			frag = data->frag + (voice * data->frags);
			v->addr = data->buffer + control->fragments[frag].addr;
			v->offset = 0;
			v->next = plugin->dst_width;
		}
	}
	*voices = plugin->voices;
	return 0;
}

static ssize_t mmap_transfer(snd_pcm_plugin_t *plugin,
			     const snd_pcm_plugin_voice_t *src_voices,
			     const snd_pcm_plugin_voice_t *dst_voices,
			     size_t samples)
{
	mmap_t *data;
	snd_pcm_mmap_control_t *control;
	ssize_t size;
	int voice, err;
	char *addr;

	if (plugin == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	control = data->control;
	if (control == NULL)
		return -EINVAL;
	if (data->channel == SND_PCM_CHANNEL_PLAYBACK) {
		if (src_voices == NULL)
			return -EINVAL;
		while (!playback_ok(plugin)) {
			err = query_playback(plugin, 0);
			if (err < 0)
				return err;
		}
		size = snd_pcm_plugin_src_samples_to_size(plugin, samples);
		if (size < 0)
			return size;
		if (plugin->src_format.interleave) {
			if (size != control->status.frag_size)
				return -EINVAL;
			addr = data->buffer + control->fragments[data->frag].addr;
			if (src_voices->addr != addr)
				memcpy(addr, src_voices->addr, size);
			control->fragments[data->frag++].data = 1;
			data->frag %= control->status.frags;
			data->frags_used++;
			return samples;
		} else {
			int frag;

			if ((size / plugin->src_format.voices) != control->status.frag_size)
				return -EINVAL;
			for (voice = 0; voice < plugin->src_format.voices; voice++) {
				frag = data->frag + (voice * data->frags);
				while (control->fragments[frag].data) {
					err = query_playback(plugin, 1);
					if (err < 0)
						return err;
				}
				addr = data->buffer + control->fragments[frag].addr;
				if (src_voices[voice].addr != addr)
					memcpy(addr, src_voices[voice].addr, control->status.frag_size);
				control->fragments[frag].data = 1;
			}
			data->frag++;
			data->frag %= data->frags;
			data->frags_used++;
			return samples;
		}
	} else if (data->channel == SND_PCM_CHANNEL_CAPTURE) {
		if (dst_voices == NULL)
			return -EINVAL;
		while (!capture_ok(plugin)) {
			err = query_capture(plugin, 0);
			if (err < 0)
				return err;
		}
		size = snd_pcm_plugin_dst_samples_to_size(plugin, samples);
		if (size < 0)
			return size;
		if (plugin->dst_format.interleave) {
			if (size != control->status.frag_size)
				return -EINVAL;
			addr = data->buffer + control->fragments[data->frag].addr;
			if (dst_voices->addr != addr)
				memcpy(dst_voices->addr, addr, size);
			control->fragments[data->frag++].data = 0;
			data->frag %= control->status.frags;
			data->frags_used--;
			return samples;
		} else {
			int frag;

			if ((size / plugin->dst_format.voices) != control->status.frag_size)
				return -EINVAL;
			for (voice = 0; voice < plugin->dst_format.voices; voice++) {
				frag = data->frag + (voice * data->frags);
				while (!control->fragments[data->frag].data) {
					err = query_capture(plugin, 1);
					if (err < 0)
						return err;
				}
				addr = data->buffer + control->fragments[frag].addr;
				if (dst_voices[voice].addr != addr)
					memcpy(dst_voices[voice].addr, addr, control->status.frag_size);
				control->fragments[frag].data = 0;
			}
			data->frag++;
			data->frag %= data->frags;
			data->frags_used--;
			return samples;
		}
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
	data = (mmap_t *)plugin->extra_data;
	if (action == INIT) {
		snd_pcm_channel_params_t *params;
		snd_pcm_channel_setup_t setup;
		int result;

		if (data->control)
			snd_pcm_munmap(plugin->handle, data->channel);
		result = snd_pcm_mmap(plugin->handle, data->channel, &data->control, (void **)&data->buffer);
		if (result < 0)
			return result;
		params = (snd_pcm_channel_params_t *)udata;
		data->start_mode = params->start_mode;
		data->stop_mode = params->stop_mode;
		memset(&setup, 0, sizeof(setup));
		setup.channel = data->channel;
		if ((result = snd_pcm_channel_setup(plugin->handle, &setup)) < 0)
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
	data = (mmap_t *)plugin->extra_data;
	if (data->control)
		snd_pcm_munmap(plugin->handle, data->channel);
}
 
int snd_pcm_plugin_build_mmap(snd_pcm_t *pcm, int channel,
			      snd_pcm_format_t *format,
			      snd_pcm_plugin_t **r_plugin)
{
	mmap_t *data;
	snd_pcm_plugin_t *plugin;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	plugin = snd_pcm_plugin_build(pcm,
				      channel == SND_PCM_CHANNEL_PLAYBACK ?
						"I/O mmap playback" :
						"I/O mmap capture",
				      format, format,
				      sizeof(mmap_t));
	if (plugin == NULL)
		return -ENOMEM;
	data = (mmap_t *)plugin->extra_data;
	data->channel = channel;
	plugin->src_voices = mmap_src_voices;
	plugin->dst_voices = mmap_dst_voices;
	plugin->transfer = mmap_transfer;
	plugin->action = mmap_action;
	plugin->private_free = mmap_free;
	*r_plugin = plugin;
	return 0;
}
