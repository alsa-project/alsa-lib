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
	char *silence;
} mmap_t;


static int mmap_src_voices(snd_pcm_plugin_t *plugin,
			   size_t samples,
			   snd_pcm_plugin_voice_t **voices)
{
	mmap_t *data;
	unsigned int voice;
        snd_pcm_plugin_voice_t *sv;
	snd_pcm_voice_area_t *dv;
	struct snd_pcm_chan *chan;
	snd_pcm_channel_setup_t *setup;
	snd_pcm_mmap_control_t *ctrl;
	int frag, f;
	int ready;

	if (plugin == NULL || voices == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	ctrl = data->control;
	chan = &data->slave->chan[plugin->channel];
	if (samples != chan->samples_per_frag)
		return -EINVAL;

	setup = &chan->setup;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;

	ready = snd_pcm_mmap_ready(data->slave, plugin->channel);
	if (ready < 0)
		return ready;
	if (!ready) {
		struct pollfd pfd;
		if (ctrl->status != SND_PCM_STATUS_RUNNING)
			return -EPIPE;
		if (chan->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		pfd.fd = snd_pcm_file_descriptor(data->slave, plugin->channel);
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

	sv = plugin->src_voices;
	dv = chan->voices;
	*voices = sv;
	for (voice = 0; voice < plugin->src_format.voices; ++voice) {
		sv->enabled = 1;
		sv->wanted = !data->silence[voice * setup->frags + f];
		sv->aptr = 0;
		sv->area.addr = dv->addr + (dv->step * chan->samples_per_frag * f) / 8;
		sv->area.first = dv->first;
		sv->area.step = dv->step;
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
        snd_pcm_plugin_voice_t *dv;
	snd_pcm_voice_area_t *sv;
	struct snd_pcm_chan *chan;
	snd_pcm_channel_setup_t *setup;
	snd_pcm_mmap_control_t *ctrl;
	int frag, f;
	int ready;

	if (plugin == NULL || voices == NULL)
		return -EINVAL;
	data = (mmap_t *)plugin->extra_data;
	chan = &data->slave->chan[plugin->channel];
	if (samples != chan->samples_per_frag)
		return -EINVAL;

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
		struct pollfd pfd;
		if (ctrl->status != SND_PCM_STATUS_RUNNING)
			return -EPIPE;
		if (chan->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		pfd.fd = snd_pcm_file_descriptor(data->slave, plugin->channel);
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

	sv = chan->voices;
	dv = plugin->dst_voices;
	*voices = dv;
	for (voice = 0; voice < plugin->dst_format.voices; ++voice) {
		dv->enabled = 1;
		dv->wanted = 0;
		dv->aptr = 0;
		dv->area.addr = sv->addr + (sv->step * chan->samples_per_frag * f) / 8;
		dv->area.first = sv->first;
		dv->area.step = sv->step;
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

	snd_pcm_mmap_commit_samples(data->slave, SND_PCM_CHANNEL_PLAYBACK, samples);
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
	snd_pcm_mmap_commit_samples(data->slave, SND_PCM_CHANNEL_CAPTURE, samples);
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

		if (data->control)
			snd_pcm_munmap(data->slave, plugin->channel);
		result = snd_pcm_mmap(data->slave, plugin->channel, &data->control, (void **)&data->buffer);
		if (result < 0)
			return result;
		setup = &data->slave->chan[plugin->channel].setup;

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
