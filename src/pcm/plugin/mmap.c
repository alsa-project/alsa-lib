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
	int frag_size, samples_frag_size;
	int start_mode, stop_mode;
	int frags, frags_used;
	int frags_min, frags_max;
	unsigned int lastblock;
	snd_pcm_plugin_voice_t voices[0];
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
	int err;
	int voice;
        snd_pcm_plugin_voice_t *dv, *sv;

	if (plugin == NULL || voices == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	if (data->channel != SND_PCM_CHANNEL_PLAYBACK)
		return -EINVAL;
	if (snd_pcm_plugin_dst_samples_to_size(plugin, samples) != data->frag_size)
		return -EINVAL;
	/* wait until the block is not free */
	while (!playback_ok(plugin)) {
		err = query_playback(plugin, 0);
		if (err < 0)
			return err;
	}	
	sv = data->voices;
	dv = plugin->voices;
	for (voice = 0; voice < plugin->src_format.voices; ++voice) {
		dv->addr = sv->addr + (sv->step * data->samples_frag_size * data->frag) / 8;
		dv->first = sv->first;
		dv->step = sv->step;
		++sv;
		++dv;
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
	int voice;
        snd_pcm_plugin_voice_t *dv, *sv;

	if (plugin == NULL || voices == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	if (data->channel != SND_PCM_CHANNEL_CAPTURE)
		return -EINVAL;
	if (snd_pcm_plugin_src_samples_to_size(plugin, samples) != data->frag_size)
		return -EINVAL;
	sv = data->voices;
	dv = plugin->voices;
	for (voice = 0; voice < plugin->src_format.voices; ++voice) {
		dv->addr = sv->addr + (sv->step * data->samples_frag_size * data->frag) / 8;
		dv->first = sv->first;
		dv->step = sv->step;
		++sv;
		++dv;
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
	int err;

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
		if (size != data->frag_size)
			return -EINVAL;
		if (src_voices != data->voices) {
			if (plugin->src_format.interleave) {
				void *dst = data->voices[0].addr + data->frag * data->frag_size;
				/* Paranoia: add check for src_voices */
				memcpy(dst, src_voices[0].addr, size);
			} else {
				int voice;
				size /= plugin->src_format.voices;
				for (voice = 0; voice < plugin->src_format.voices; ++voice) {
					void *dst = data->voices[voice].addr + (data->voices[voice].step * data->samples_frag_size * data->frag) / 8;
					/* Paranoia: add check for src_voices */
					memcpy(dst, src_voices[voice].addr, size);
				}
			}
		}
		control->fragments[data->frag].data = 1;
		data->frag++;
		data->frag %= data->frags;
		data->frags_used++;
		return samples;
	} else if (data->channel == SND_PCM_CHANNEL_CAPTURE) {
		if (dst_voices == NULL)
			return -EINVAL;
		while (!capture_ok(plugin)) {
			err = query_capture(plugin, 0);
			if (err < 0)
				return err;
		}
		size = snd_pcm_plugin_dst_samples_to_size(plugin, samples);
		if (size != data->frag_size)
			return -EINVAL;
		if (dst_voices != data->voices) {
			if (plugin->dst_format.interleave) {
				void *src = data->voices[0].addr + data->frag * data->frag_size;
				/* Paranoia: add check for dst_voices */
				memcpy(dst_voices[0].addr, src, size);
			} else {
				int voice;
				size /= plugin->src_format.voices;
				for (voice = 0; voice < plugin->src_format.voices; ++voice) {
					void *src = data->voices[voice].addr + (data->voices[voice].step * data->samples_frag_size * data->frag) / 8;
					/* Paranoia: add check for dst_voices */
					memcpy(dst_voices[voice].addr, src, size);
				}
			}
			control->fragments[data->frag].data = 0;
		} else {
			int prev_frag = data->frag - 1;
			if (prev_frag < 0)
				prev_frag = data->frags - 1;
			control->fragments[prev_frag].data = 0;
		}
		data->frag++;
		data->frag %= data->frags;
		data->frags_used--;
		return samples;
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
		int voice;
		snd_pcm_plugin_voice_t *v;

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
		data->frag_size = setup.buf.block.frag_size;
		data->samples_frag_size = data->frag_size / snd_pcm_format_size(plugin->src_format.format, plugin->src_format.voices);
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

		v = data->voices;
		for (voice = 0; voice < plugin->src_format.voices; ++voice) {
			snd_pcm_voice_setup_t vsetup;
			
			vsetup.voice = voice;
			if ((result = snd_pcm_voice_setup(plugin->handle, data->channel, &vsetup)) < 0)
				return result;
			if (vsetup.addr < 0)
				return -EBADFD;
			v->addr = data->buffer + vsetup.addr;
			v->first = vsetup.first;
			v->step = vsetup.step;
			v++;
		}
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
				      sizeof(mmap_t) + sizeof(snd_pcm_plugin_voice_t) * format->voices);
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
