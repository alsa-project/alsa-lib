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
	void *buffer;
#if 0
	char *silence;
#endif
} mmap_t;


static ssize_t mmap_src_channels(snd_pcm_plugin_t *plugin,
			       size_t frames,
			       snd_pcm_plugin_channel_t **channels)
{
	mmap_t *data;
        snd_pcm_plugin_channel_t *sv;
	snd_pcm_channel_area_t *dv;
	snd_pcm_t *stream;
	snd_pcm_setup_t *setup;
	size_t pos;
	int ready;
	unsigned int channel;

	assert(plugin && channels);
	data = (mmap_t *)plugin->extra_data;
	stream = plugin->plug->slave;

	setup = &stream->setup;
	if (snd_pcm_mmap_state(stream) < SND_PCM_STATE_PREPARED)
		return -EBADFD;

	ready = snd_pcm_mmap_ready(stream);
	if (ready < 0)
		return ready;
	if (!ready) {
		struct pollfd pfd;
		if (snd_pcm_mmap_state(stream) != SND_PCM_STATE_RUNNING)
			return -EPIPE;
		if (stream->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		pfd.fd = snd_pcm_file_descriptor(stream);
		pfd.events = POLLOUT | POLLERR;
		ready = poll(&pfd, 1, 10000);
		if (ready < 0)
			return ready;
		if (ready == 0 || (pfd.revents & POLLERR))
			return -EPIPE;
		assert(snd_pcm_mmap_ready(stream));
	}
	pos = snd_pcm_mmap_frames_offset(stream);
	assert(pos % setup->frames_align == 0);

	sv = plugin->buf_channels;
	dv = stream->channels;
	*channels = sv;
	for (channel = 0; channel < plugin->src_format.channels; ++channel) {
		sv->enabled = 1;
#if 0
		sv->wanted = !data->silence[channel * setup->frags + f];
#else
		sv->wanted = 1;
#endif
		sv->area.addr = dv->addr + dv->step * pos / 8;
		sv->area.first = dv->first;
		sv->area.step = dv->step;
		++sv;
		++dv;
	}
	return snd_pcm_mmap_frames_xfer(stream, frames);
}

static ssize_t mmap_dst_channels(snd_pcm_plugin_t *plugin,
				 size_t frames,
				 snd_pcm_plugin_channel_t **channels)
{
	mmap_t *data;
	int err;
	unsigned int channel;
        snd_pcm_plugin_channel_t *dv;
	snd_pcm_channel_area_t *sv;
	snd_pcm_t *stream;
	snd_pcm_setup_t *setup;
	size_t pos;
	int ready;

	assert(plugin && channels);
	data = (mmap_t *)plugin->extra_data;
	stream = plugin->plug->slave;

	setup = &stream->setup;
	if (snd_pcm_mmap_state(stream) < SND_PCM_STATE_PREPARED)
		return -EBADFD;
	if (snd_pcm_mmap_state(stream) == SND_PCM_STATE_PREPARED &&
	    stream->setup.start_mode == SND_PCM_START_DATA) {
		err = snd_pcm_go(stream);
		if (err < 0)
			return err;
	}
	ready = snd_pcm_mmap_ready(stream);
	if (ready < 0)
		return ready;
	if (!ready) {
		struct pollfd pfd;
		if (snd_pcm_mmap_state(stream) != SND_PCM_STATE_RUNNING)
			return -EPIPE;
		if (stream->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		pfd.fd = snd_pcm_file_descriptor(stream);
		pfd.events = POLLIN | POLLERR;
		ready = poll(&pfd, 1, 10000);
		if (ready < 0)
			return ready;
		if (ready == 0 || (pfd.revents & POLLERR))
			return -EPIPE;
		assert(snd_pcm_mmap_ready(stream));
	}
	pos = snd_pcm_mmap_frames_offset(stream);
	assert(pos % setup->frames_align == 0);

	sv = stream->channels;
	dv = plugin->buf_channels;
	*channels = dv;
	for (channel = 0; channel < plugin->dst_format.channels; ++channel) {
		dv->enabled = 1;
		dv->wanted = 0;
		dv->area.addr = sv->addr + sv->step * pos / 8;
		dv->area.first = sv->first;
		dv->area.step = sv->step;
		++sv;
		++dv;
	}
	return snd_pcm_mmap_frames_xfer(stream, frames);
}

static ssize_t mmap_playback_transfer(snd_pcm_plugin_t *plugin,
				      const snd_pcm_plugin_channel_t *src_channels,
				      snd_pcm_plugin_channel_t *dst_channels UNUSED,
				      size_t frames)
{
	mmap_t *data;
	snd_pcm_setup_t *setup;
	snd_pcm_t *stream;
	int err;

	assert(plugin && plugin->prev);
	assert(src_channels);
	data = (mmap_t *)plugin->extra_data;
	stream = plugin->plug->slave;
	setup = &stream->setup;

#if 0
	for (channel = 0; channel < plugin->src_format.channels; channel++) {
		if (src_channels[channel].enabled)
			data->silence[channel * setup->frags + f] = 0;
	}
#endif

	snd_pcm_frame_data(stream, frames);
	if (snd_pcm_mmap_state(stream) == SND_PCM_STATE_PREPARED &&
	    (setup->start_mode == SND_PCM_START_DATA ||
	     (setup->start_mode == SND_PCM_START_FULL &&
	      !snd_pcm_mmap_ready(stream)))) {
		err = snd_pcm_go(stream);
		if (err < 0)
			return err;
	}
	return frames;
}
 
static ssize_t mmap_capture_transfer(snd_pcm_plugin_t *plugin,
				     const snd_pcm_plugin_channel_t *src_channels UNUSED,
				     snd_pcm_plugin_channel_t *dst_channels UNUSED,
				     size_t frames)
{
	mmap_t *data;
	snd_pcm_t *stream;

	assert(plugin && plugin->next);
	data = (mmap_t *)plugin->extra_data;
	stream = plugin->plug->slave;

	/* FIXME: not here the increment */
	snd_pcm_frame_data(stream, frames);

	return frames;
}
 
static int mmap_action(snd_pcm_plugin_t *plugin,
		       snd_pcm_plugin_action_t action,
		       unsigned long udata UNUSED)
{
	struct mmap_private_data *data;
	snd_pcm_t *stream;

	assert(plugin);
	stream = plugin->plug->slave;
	data = (mmap_t *)plugin->extra_data;
	if (action == INIT) {
		snd_pcm_setup_t *setup;
		int result;

		if (data->buffer) {
			snd_pcm_munmap(stream);
			data->buffer = 0;
		}
		result = snd_pcm_mmap(stream, NULL, NULL, (void **)&data->buffer);
		if (result < 0)
			return result;
		setup = &stream->setup;

#if 0
		if (plugin->stream == SND_PCM_STREAM_PLAYBACK) {
			data->silence = malloc(setup->frags * setup->format.channels);
			memset(data->silence, 0, setup->frags * setup->format.channels);
		} else
			data->silence = 0;
#endif
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
#if 0
	if (data->silence)
		free(data->silence);
#endif
	if (data->buffer)
		snd_pcm_munmap(plugin->plug->slave);
}
 
int snd_pcm_plugin_build_mmap(snd_pcm_plug_t *plug,
			      snd_pcm_format_t *format,
			      snd_pcm_plugin_t **r_plugin)
{
	int err;
	mmap_t *data;
	snd_pcm_plugin_t *plugin;

	assert(r_plugin);
	*r_plugin = NULL;
	assert(plug);
	err = snd_pcm_plugin_build(plug, "I/O mmap",
				   format, format,
				   sizeof(mmap_t) + sizeof(snd_pcm_plugin_channel_t) * format->channels,
				   &plugin);
	if (err < 0)
		return err;
	data = (mmap_t *)plugin->extra_data;
	if (plug->handle->stream == SND_PCM_STREAM_PLAYBACK) {
		plugin->client_channels = mmap_src_channels;
		plugin->transfer = mmap_playback_transfer;
	} else {
		plugin->client_channels = mmap_dst_channels;
		plugin->transfer = mmap_capture_transfer;
	}
	plugin->action = mmap_action;
	plugin->private_free = mmap_free;
	*r_plugin = plugin;
	return 0;
}
