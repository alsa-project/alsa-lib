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
#include <assert.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include "../pcm_local.h"

/*
 *  Basic mmap plugin
 */
 
typedef struct mmap_private_data {
	snd_pcm_t *slave;
	snd_pcm_mmap_control_t *control;
	void *buffer;
	unsigned int frag;
	size_t samples_frag_size;
	char *silence;
	snd_pcm_plugin_voice_t voices[0];
} mmap_t;


static int mmap_src_voices(snd_pcm_plugin_t *plugin,
			   size_t samples,
			   snd_pcm_plugin_voice_t **voices)
{
	mmap_t *data;
	unsigned int voice;
        snd_pcm_plugin_voice_t *dv, *sv;
	struct snd_pcm_chan *chan;
	snd_pcm_channel_setup_t *setup;
	snd_pcm_mmap_control_t *ctrl;
	int frag, f;
	struct pollfd pfd;
	int ready;

	if (plugin == NULL || voices == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	if (samples != data->samples_frag_size)
		return -EINVAL;

	ctrl = data->control;
	chan = &plugin->handle->chan[plugin->channel];
	setup = &chan->setup;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;

	ready = snd_pcm_mmap_ready(data->slave, plugin->channel);
	if (ready < 0)
		return ready;
	if (!ready) {
		if (ctrl->status != SND_PCM_STATUS_RUNNING)
			return -EPIPE;
		if (chan->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		pfd.fd = snd_pcm_file_descriptor(plugin->handle, plugin->channel);
		pfd.events = POLLOUT | POLLERR;
		ready = poll(&pfd, 1, 10000);
		if (ready < 0)
			return ready;
		if (ready && pfd.revents & POLLERR)
			return -EPIPE;
		assert(snd_pcm_mmap_ready(data->slave, plugin->channel));
	}
	frag = ctrl->frag_data;
	f = frag % setup->frags;

	dv = data->voices;
	sv = plugin->src_voices;
	*voices = sv;
	for (voice = 0; voice < plugin->src_format.voices; ++voice) {
		sv->enabled = 1;
		sv->wanted = !data->silence[voice * setup->frags + f];
		sv->aptr = 0;
		sv->addr = dv->addr + (dv->step * data->samples_frag_size * f) / 8;
		sv->first = dv->first;
		sv->step = dv->step;
		++sv;
		++dv;
	}
	data->frag = frag;
	return 0;
}

static int mmap_dst_voices(snd_pcm_plugin_t *plugin,
			   size_t samples,
			   snd_pcm_plugin_voice_t **voices)
{
	mmap_t *data;
	int err;
	unsigned int voice;
        snd_pcm_plugin_voice_t *dv, *sv;
	struct snd_pcm_chan *chan;
	snd_pcm_channel_setup_t *setup;
	snd_pcm_mmap_control_t *ctrl;
	int frag, f;
	struct pollfd pfd;
	int ready;

	if (plugin == NULL || voices == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	if (samples != data->samples_frag_size)
		return -EINVAL;

	chan = &plugin->handle->chan[plugin->channel];
	setup = &chan->setup;
	ctrl = data->control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	if (ctrl->status == SND_PCM_STATUS_PREPARED &&
	    chan->setup.start_mode == SND_PCM_START_DATA) {
		err = snd_pcm_channel_go(data->slave, plugin->channel);
		if (err < 0)
			return err;
	}
	ready = snd_pcm_mmap_ready(data->slave, plugin->channel);
	if (ready < 0)
		return ready;
	if (!ready) {
		if (ctrl->status == SND_PCM_STATUS_PREPARED &&
		    chan->setup.start_mode == SND_PCM_START_FULL) {
			err = snd_pcm_channel_go(data->slave, plugin->channel);
			if (err < 0)
				return err;
		}
		if (ctrl->status != SND_PCM_STATUS_RUNNING)
			return -EPIPE;
		if (chan->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		pfd.fd = snd_pcm_file_descriptor(plugin->handle, plugin->channel);
		pfd.events = POLLIN | POLLERR;
		ready = poll(&pfd, 1, 10000);
		if (ready < 0)
			return ready;
		if (ready && pfd.revents & POLLERR)
			return -EPIPE;
		assert(snd_pcm_mmap_ready(data->slave, plugin->channel));
	}

	frag = ctrl->frag_data;
	f = frag % setup->frags;

	sv = data->voices;
	dv = plugin->dst_voices;
	*voices = dv;
	for (voice = 0; voice < plugin->dst_format.voices; ++voice) {
		dv->enabled = 1;
		dv->wanted = 0;
		dv->aptr = 0;
		dv->addr = sv->addr + (sv->step * data->samples_frag_size * f) / 8;
		dv->first = sv->first;
		dv->step = sv->step;
		++sv;
		++dv;
	}
	data->frag = frag;
	return 0;
}

static ssize_t mmap_playback_transfer(snd_pcm_plugin_t *plugin,
				      const snd_pcm_plugin_voice_t *src_voices,
				      snd_pcm_plugin_voice_t *dst_voices UNUSED,
				      size_t samples)
{
	mmap_t *data;
	unsigned int voice;
	snd_pcm_channel_setup_t *setup;
	snd_pcm_mmap_control_t *ctrl;
	struct snd_pcm_chan *chan;
	unsigned int frag, f;
	int err;

	if (plugin == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	if (src_voices == NULL)
		return -EINVAL;
	if (plugin->prev == NULL)
		return -EINVAL;
	ctrl = data->control;
	if (ctrl == NULL)
		return -EINVAL;
	chan = &data->slave->chan[SND_PCM_CHANNEL_PLAYBACK];
	setup = &chan->setup;
	frag = ctrl->frag_data;
	if (frag != data->frag)
		return -EIO;
	f = frag % setup->frags;

	for (voice = 0; voice < plugin->src_format.voices; voice++) {
		if (src_voices[voice].enabled)
			data->silence[voice * setup->frags + f] = 0;
	}

	frag++;
	if (frag == setup->frag_boundary) {
		ctrl->frag_data = 0;
		ctrl->pos_data = 0;
	} else {
		ctrl->frag_data = frag;
		ctrl->pos_data += setup->frag_size;
	}
	if (ctrl->status == SND_PCM_STATUS_PREPARED &&
	    (chan->setup.start_mode == SND_PCM_START_DATA ||
	     (chan->setup.start_mode == SND_PCM_START_FULL &&
	      !snd_pcm_mmap_ready(data->slave, plugin->channel)))) {
		err = snd_pcm_channel_go(data->slave, plugin->channel);
		if (err < 0)
			return err;
	}
	return samples;
}
 
static ssize_t mmap_capture_transfer(snd_pcm_plugin_t *plugin,
				     const snd_pcm_plugin_voice_t *src_voices UNUSED,
				     snd_pcm_plugin_voice_t *dst_voices UNUSED,
				     size_t samples)
{
	mmap_t *data;
	snd_pcm_channel_setup_t *setup;
	snd_pcm_mmap_control_t *ctrl;
	unsigned int frag;

	if (plugin == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	if (plugin->next == NULL)
		return -EINVAL;

	ctrl = data->control;
	if (ctrl == NULL)
		return -EINVAL;
	frag = ctrl->frag_data;
	if (frag != data->frag)
		return -EIO;
	setup = &data->slave->chan[SND_PCM_CHANNEL_CAPTURE].setup;

	/* FIXME: not here the increment */
	frag++;
	if (frag == setup->frag_boundary) {
		ctrl->frag_data = 0;
		ctrl->pos_data = 0;
	} else {
		ctrl->frag_data = frag;
		ctrl->pos_data += setup->frag_size;
	}
	return samples;
}
 
static int mmap_action(snd_pcm_plugin_t *plugin,
		       snd_pcm_plugin_action_t action,
		       unsigned long udata UNUSED)
{
	struct mmap_private_data *data;

	if (plugin == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	if (action == INIT) {
		snd_pcm_channel_setup_t *setup;
		int result;
		unsigned int voice;
		snd_pcm_plugin_voice_t *v;

		if (data->control)
			snd_pcm_munmap(data->slave, plugin->channel);
		result = snd_pcm_mmap(data->slave, plugin->channel, &data->control, (void **)&data->buffer);
		if (result < 0)
			return result;
		setup = &data->slave->chan[plugin->channel].setup;
		data->samples_frag_size = setup->frag_size / snd_pcm_format_size(setup->format.format, setup->format.voices);

		v = data->voices;
		for (voice = 0; voice < setup->format.voices; ++voice) {
			snd_pcm_voice_setup_t vsetup;
			
			vsetup.voice = voice;
			if ((result = snd_pcm_voice_setup(data->slave, plugin->channel, &vsetup)) < 0)
				return result;
			if (vsetup.addr < 0)
				return -EBADFD;
			v->addr = data->buffer + vsetup.addr;
			v->first = vsetup.first;
			v->step = vsetup.step;
			v++;
		}
		if (plugin->channel == SND_PCM_CHANNEL_PLAYBACK) {
			data->silence = malloc(setup->frags * setup->format.voices);
			memset(data->silence, 0, setup->frags * setup->format.voices);
		} else
			data->silence = 0;
		return 0;
	}
	return 0;	/* silenty ignore other actions */
}

static void mmap_free(snd_pcm_plugin_t *plugin, void *private_data UNUSED)
{
	struct mmap_private_data *data;

	if (plugin == NULL)
		return;
	data = (mmap_t *)plugin->extra_data;
	if (data->silence)
		free(data->silence);
	if (data->control)
		snd_pcm_munmap(data->slave, plugin->channel);
}
 
int snd_pcm_plugin_build_mmap(snd_pcm_plugin_handle_t *pcm,
			      int channel,
			      snd_pcm_t *slave,
			      snd_pcm_format_t *format,
			      snd_pcm_plugin_t **r_plugin)
{
	int err;
	mmap_t *data;
	snd_pcm_plugin_t *plugin;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;
	if (!pcm)
		return -EINVAL;
	err = snd_pcm_plugin_build(pcm, channel,
				   "I/O mmap",
				   format, format,
				   sizeof(mmap_t) + sizeof(snd_pcm_plugin_voice_t) * format->voices,
				   &plugin);
	if (err < 0)
		return err;
	data = (mmap_t *)plugin->extra_data;
	data->slave = slave;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin->client_voices = mmap_src_voices;
		plugin->transfer = mmap_playback_transfer;
	} else {
		plugin->client_voices = mmap_dst_voices;
		plugin->transfer = mmap_capture_transfer;
	}
	plugin->action = mmap_action;
	plugin->private_free = mmap_free;
	*r_plugin = plugin;
	return 0;
}
