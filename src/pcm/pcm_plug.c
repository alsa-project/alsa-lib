/*
 *  PCM - Plug
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
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/uio.h>
#include "pcm_local.h"

/* snd_pcm_plugin externs */

int snd_pcm_plugin_insert(snd_pcm_plugin_t *plugin)
{
	snd_pcm_plug_t *plug;
	assert(plugin);
	plug = plugin->plug;
	plugin->next = plug->first;
	plugin->prev = NULL;
	if (plug->first) {
		plug->first->prev = plugin;
		plug->first = plugin;
	} else {
		plug->last =
		plug->first = plugin;
	}
	return 0;
}

int snd_pcm_plugin_append(snd_pcm_plugin_t *plugin)
{
	snd_pcm_plug_t *plug;
	assert(plugin);
	plug = plugin->plug;
	plugin->next = NULL;
	plugin->prev = plug->last;
	if (plug->last) {
		plug->last->next = plugin;
		plug->last = plugin;
	} else {
		plug->last =
		plug->first = plugin;
	}
	return 0;
}

/* snd_pcm_plug externs */

int snd_pcm_plug_clear(snd_pcm_plug_t *plug)
{
	snd_pcm_plugin_t *plugin, *plugin_next;
	
	assert(plug);

	plugin = plug->first;
	plug->first = NULL;
	plug->last = NULL;
	while (plugin) {
		plugin_next = plugin->next;
		snd_pcm_plugin_free(plugin);
		plugin = plugin_next;
	}
	return 0;
}

snd_pcm_plugin_t *snd_pcm_plug_first(snd_pcm_plug_t *plug)
{
	assert(plug);
	return plug->first;
}

snd_pcm_plugin_t *snd_pcm_plug_last(snd_pcm_plug_t *plug)
{
	assert(plug);
	return plug->last;
}

/*
 *
 */

static int snd_pcm_plug_close(void *private)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_plug_clear(plug);
	free(plug->handle->ops);
	if (plug->close_slave)
		return snd_pcm_close(plug->slave);
	free(private);
	return 0;
}

static int snd_pcm_plug_nonblock(void *private, int nonblock)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	return snd_pcm_nonblock(plug->slave, nonblock);
}

static int snd_pcm_plug_info(void *private, snd_pcm_info_t *info)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	
	if ((err = snd_pcm_info(plug->slave, info)) < 0)
		return err;
	info->formats = snd_pcm_plug_formats(info->formats);
	info->min_rate = 4000;
	info->max_rate = 192000;
	info->min_channels = 1;
	info->max_channels = 32;
	info->rates = SND_PCM_RATE_CONTINUOUS | SND_PCM_RATE_8000_192000;
	info->flags |= SND_PCM_INFO_INTERLEAVE | SND_PCM_INFO_NONINTERLEAVE;

	if (plug->slave->valid_setup) {
		info->buffer_size = snd_pcm_plug_client_size(plug, info->buffer_size);
		info->min_fragment_size = snd_pcm_plug_client_size(plug, info->min_fragment_size);
		info->max_fragment_size = snd_pcm_plug_client_size(plug, info->max_fragment_size);
		info->fragment_align = snd_pcm_plug_client_size(plug, info->fragment_align);
		info->fifo_size = snd_pcm_plug_client_size(plug, info->fifo_size);
		info->transfer_block_size = snd_pcm_plug_client_size(plug, info->transfer_block_size);
	}
	info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_plug_action(snd_pcm_plug_t *plug, int action,
			       unsigned long data)
{
	int err;
	snd_pcm_plugin_t *plugin = plug->first;
	while (plugin) {
		if (plugin->action) {
			if ((err = plugin->action(plugin, action, data))<0)
				return err;
		}
		plugin = plugin->next;
	}
	return 0;
}

static int snd_pcm_plug_setup(void *private, snd_pcm_setup_t *setup)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;

	err = snd_pcm_setup(plug->slave, setup);
	if (err < 0)
		return err;
	if (!plug->first)
		return 0;
	setup->frame_boundary /= setup->frag_size;
	setup->frag_size = snd_pcm_plug_client_size(plug, setup->frag_size);
	setup->frame_boundary *= setup->frag_size;
	setup->buffer_size = setup->frags * setup->frag_size;
	setup->frames_min = snd_pcm_plug_client_size(plug, setup->frames_min);
	setup->frames_align = snd_pcm_plug_client_size(plug, setup->frames_align);
	setup->frames_xrun_max = snd_pcm_plug_client_size(plug, setup->frames_xrun_max);
	setup->frames_fill_max = snd_pcm_plug_client_size(plug, setup->frames_fill_max);
	setup->mmap_size = 0;
	if (plug->handle->stream == SND_PCM_STREAM_PLAYBACK)
		setup->format = plug->first->src_format;
	else
		setup->format = plug->last->dst_format;
	err = snd_pcm_plug_alloc(plug, setup->frag_size);
	if (err < 0)
		return err;
	return 0;	
}

static int snd_pcm_plug_status(void *private, snd_pcm_status_t *status)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;

	err = snd_pcm_status(plug->slave, status);
	if (err < 0)
		return err;

	status->frame_io = snd_pcm_plug_client_size(plug, status->frame_io);
	status->frame_data = snd_pcm_plug_client_size(plug, status->frame_data);
	status->frames_avail = snd_pcm_plug_client_size(plug, status->frames_avail);
	status->frames_avail_max = snd_pcm_plug_client_size(plug, status->frames_avail_max);
	return 0;	
}

static int snd_pcm_plug_state(void *private)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	return snd_pcm_state(plug->slave);
}

static int snd_pcm_plug_frame_io(void *private, int update)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	ssize_t frame_io = snd_pcm_frame_io(plug->slave, update);
	if (frame_io < 0)
		return frame_io;
	return snd_pcm_plug_client_size(plug, frame_io);
}

static int snd_pcm_plug_prepare(void *private)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	int err;
	err = snd_pcm_prepare(plug->slave);
	if (err < 0)
		return err;
	if ((err = snd_pcm_plug_action(plug, PREPARE, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_go(void *private)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	return snd_pcm_go(plug->slave);
}

static int snd_pcm_plug_sync_go(void *private, snd_pcm_sync_t *sync)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	return snd_pcm_sync_go(plug->slave, sync);
}

static int snd_pcm_plug_drain(void *private)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	int err;

	if ((err = snd_pcm_drain(plug->slave)) < 0)
		return err;
	if ((err = snd_pcm_plug_action(plug, DRAIN, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_flush(void *private)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	int err;

	if ((err = snd_pcm_flush(plug->slave)) < 0)
		return err;
	if ((err = snd_pcm_plug_action(plug, FLUSH, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_pause(void *private, int enable)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	int err;
	
	if ((err = snd_pcm_pause(plug->slave, enable)) < 0)
		return err;
	if ((err = snd_pcm_plug_action(plug, PAUSE, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_channel_setup(void *private UNUSED, snd_pcm_channel_setup_t *setup UNUSED)
{
	/* FIXME: non mmap setups */
	return -ENXIO;
}

static ssize_t snd_pcm_plug_frame_data(void *private, off_t offset)
{
	ssize_t ret;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	if (offset < 0) {
		offset = snd_pcm_plug_slave_size(plug, -offset);
		if (offset < 0)
			return offset;
		offset = -offset;
	} else {
		offset = snd_pcm_plug_slave_size(plug, offset);
		if (offset < 0)
			return offset;
	}
	ret = snd_pcm_frame_data(plug->slave, offset);
	if (ret < 0)
		return ret;
	return snd_pcm_plug_client_size(plug, ret);
}
  
ssize_t snd_pcm_plug_writev(void *private, const struct iovec *vector, unsigned long count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_t *handle = plug->handle;
	unsigned int k, step, channels;
	size_t size = 0;
	assert(plug->frames_alloc);
	channels = handle->setup.format.channels;
	if (handle->setup.format.interleave)
		step = 1;
	else {
		step = channels;
		assert(count % channels == 0);
	}
	for (k = 0; k < count; k += step) {
		snd_pcm_plugin_channel_t *channels;
		ssize_t frames;
		frames = snd_pcm_plug_client_channels_iovec(plug, vector, count, &channels);
		if (frames < 0)
			return frames;
		while (1) {
			unsigned int c;
			ssize_t ret;
			size_t frames1 = frames;
			if (frames1 > plug->frames_alloc)
				frames1 = plug->frames_alloc;
			ret = snd_pcm_plug_write_transfer(plug, channels, frames1);
			if (ret < 0) {
				if (size > 0)
					return size;
				return ret;
			}
			size += ret;
			frames -= ret;
			if (frames == 0)
				break;
			for (c = 0; c < handle->setup.format.channels; ++c)
				channels[c].area.addr += ret * channels[c].area.step / 8;
		}
	}
	return size;
}

ssize_t snd_pcm_plug_readv(void *private, const struct iovec *vector, unsigned long count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_t *handle = plug->handle;
	unsigned int k, step, channels;
	size_t size = 0;
	assert(plug->frames_alloc);
	channels = handle->setup.format.channels;
	if (handle->setup.format.interleave)
		step = 1;
	else {
		step = channels;
		assert(count % channels == 0);
	}
	for (k = 0; k < count; k += step) {
		snd_pcm_plugin_channel_t *channels;
		ssize_t frames;
		frames = snd_pcm_plug_client_channels_iovec(plug, vector, count, &channels);
		if (frames < 0)
			return frames;
		while (1) {
			unsigned int c;
			ssize_t ret;
			size_t frames1 = frames;
			if (frames1 > plug->frames_alloc)
				frames1 = plug->frames_alloc;
			ret = snd_pcm_plug_read_transfer(plug, channels, frames1);
			if (ret < 0) {
				if (size > 0)
					return size;
				return ret;
			}
			size += ret;
			frames -= ret;
			if (frames == 0)
				break;
			for (c = 0; c < handle->setup.format.channels; ++c)
				channels[c].area.addr += ret * channels[c].area.step / 8;
		}
	}
	return size;
}

ssize_t snd_pcm_plug_write(void *private, const void *buf, size_t count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_t *handle = plug->handle;
	ssize_t frames;
	snd_pcm_plugin_channel_t *channels;
	size_t size = 0;
	assert(plug->frames_alloc);
	frames = snd_pcm_plug_client_channels_buf(plug, (char *)buf, count, &channels);
	if (frames < 0)
		return frames;

	while (1) {
		unsigned int c;
		ssize_t ret;
		size_t frames1 = frames;
		if (frames1 > plug->frames_alloc)
			frames1 = plug->frames_alloc;
		ret = snd_pcm_plug_write_transfer(plug, channels, frames1);
		if (ret < 0) {
			if (size > 0)
				return size;
			return ret;
		}
		size += ret;
		frames -= ret;
		if (frames == 0)
			break;
		for (c = 0; c < handle->setup.format.channels; ++c)
			channels[c].area.addr += ret * channels[c].area.step / 8;
	}
	return size;
}

ssize_t snd_pcm_plug_read(void *private, void *buf, size_t count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_t *handle = plug->handle;
	ssize_t frames;
	snd_pcm_plugin_channel_t *channels;
	size_t size = 0;
	assert(plug->frames_alloc);
	frames = snd_pcm_plug_client_channels_buf(plug, buf, count, &channels);
	if (frames < 0)
		return frames;

	while (1) {
		unsigned int c;
		ssize_t ret;
		size_t frames1 = frames;
		if (frames1 > plug->frames_alloc)
			frames1 = plug->frames_alloc;
		ret = snd_pcm_plug_read_transfer(plug, channels, frames1);
		if (ret < 0) {
			if (size > 0)
				return size;
			return ret;
		}
		size += ret;
		frames -= ret;
		if (frames == 0)
			break;
		for (c = 0; c < handle->setup.format.channels; ++c)
			channels[c].area.addr += ret * channels[c].area.step / 8;
	}
	return size;
}

static int snd_pcm_plug_mmap_status(void *private UNUSED, snd_pcm_mmap_status_t **status UNUSED)
{
	return -EBADFD;
}

static int snd_pcm_plug_mmap_control(void *private UNUSED, snd_pcm_mmap_control_t **control UNUSED)
{
	return -EBADFD;
}

static int snd_pcm_plug_mmap_data(void *private UNUSED, void **buffer UNUSED, size_t bsize UNUSED)
{
	return -EBADFD;
}

static int snd_pcm_plug_munmap_status(void *private UNUSED, snd_pcm_mmap_status_t *status UNUSED)
{
	return -EBADFD;
}
		
static int snd_pcm_plug_munmap_control(void *private UNUSED, snd_pcm_mmap_control_t *control UNUSED)
{
	return -EBADFD;
}
		
static int snd_pcm_plug_munmap_data(void *private UNUSED, void *buffer UNUSED, size_t size UNUSED)
{
	return -EBADFD;
}
		
static int snd_pcm_plug_channels_mask(void *private,
				    bitset_t *client_vmask)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	if (plug->handle->stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_plug_playback_channels_mask(plug, client_vmask);
	else
		return snd_pcm_plug_capture_channels_mask(plug, client_vmask);
}

int snd_pcm_plug_file_descriptor(void *private)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	return snd_pcm_file_descriptor(plug->slave);
}

static int snd_pcm_plug_params(void *private, snd_pcm_params_t *params);

struct snd_pcm_ops snd_pcm_plug_ops = {
	close: snd_pcm_plug_close,
	nonblock: snd_pcm_plug_nonblock,
	info: snd_pcm_plug_info,
	params: snd_pcm_plug_params,
	setup: snd_pcm_plug_setup,
	channel_setup: snd_pcm_plug_channel_setup,
	status: snd_pcm_plug_status,
	frame_io: snd_pcm_plug_frame_io,
	state: snd_pcm_plug_state,
	prepare: snd_pcm_plug_prepare,
	go: snd_pcm_plug_go,
	sync_go: snd_pcm_plug_sync_go,
	drain: snd_pcm_plug_drain,
	flush: snd_pcm_plug_flush,
	pause: snd_pcm_plug_pause,
	frame_data: snd_pcm_plug_frame_data,
	write: snd_pcm_plug_write,
	writev: snd_pcm_plug_writev,
	read: snd_pcm_plug_read,
	readv: snd_pcm_plug_readv,
	mmap_status: snd_pcm_plug_mmap_status,
	mmap_control: snd_pcm_plug_mmap_control,
	mmap_data: snd_pcm_plug_mmap_data,
	munmap_status: snd_pcm_plug_munmap_status,
	munmap_control: snd_pcm_plug_munmap_control,
	munmap_data: snd_pcm_plug_munmap_data,
	file_descriptor: snd_pcm_plug_file_descriptor,
	channels_mask: snd_pcm_plug_channels_mask,
};

static int snd_pcm_plug_params(void *private, snd_pcm_params_t *params)
{
	snd_pcm_params_t slave_params, params1;
	snd_pcm_info_t slave_info;
	snd_pcm_plugin_t *plugin;
	snd_pcm_plug_t *plug;
	int err;
	
	plug = (snd_pcm_plug_t*) private;

	/*
	 *  try to decide, if a conversion is required
         */

	memset(&slave_info, 0, sizeof(slave_info));
	slave_info.stream = plug->slave->stream;
	if ((err = snd_pcm_info(plug->slave, &slave_info)) < 0) {
		snd_pcm_plug_clear(plug);
		return err;
	}

	if ((err = snd_pcm_plug_slave_params(params, &slave_info, &slave_params)) < 0)
		return err;


	snd_pcm_plug_clear(plug);

	/* add necessary plugins */
	params1 = *params;
	if ((err = snd_pcm_plug_format(plug, &params1, &slave_params)) < 0)
		return err;

	if (!plug->first) {
		err = snd_pcm_params(plug->slave, params);
		if (err < 0)
			return err;
		*plug->handle->ops = *plug->slave->ops;
		plug->handle->ops->params = snd_pcm_plug_params;
		plug->handle->ops->setup = snd_pcm_plug_setup;
		plug->handle->ops->info = snd_pcm_plug_info;
		plug->handle->op_arg = plug->slave->op_arg;
		return 0;
	} else {
		*plug->handle->ops = snd_pcm_plug_ops;
		plug->handle->op_arg = plug;
	}

	/* compute right sizes */
	slave_params.frag_size = snd_pcm_plug_slave_size(plug, params1.frag_size);
	slave_params.buffer_size = snd_pcm_plug_slave_size(plug, params1.buffer_size);
	slave_params.frames_fill_max = snd_pcm_plug_slave_size(plug, params1.frames_fill_max);
	slave_params.frames_min = snd_pcm_plug_slave_size(plug, params1.frames_min);
	slave_params.frames_xrun_max = snd_pcm_plug_slave_size(plug, params1.frames_xrun_max);
	slave_params.frames_align = snd_pcm_plug_slave_size(plug, params1.frames_align);
	if (slave_params.frame_boundary == 0 || slave_params.frame_boundary > INT_MAX)
		slave_params.frame_boundary = INT_MAX;
	assert(params->buffer_size > 0);
	slave_params.frame_boundary /= params->buffer_size;
	if (slave_params.frame_boundary > INT_MAX / slave_params.buffer_size)
		slave_params.frame_boundary = INT_MAX;
	else
		slave_params.frame_boundary *= slave_params.buffer_size;

	/*
	 *  I/O plugins
	 */

	if (slave_info.flags & SND_PCM_INFO_MMAP) {
		pdprintf("params mmap plugin\n");
		err = snd_pcm_plugin_build_mmap(plug, &slave_params.format, &plugin);
	} else {
		pdprintf("params I/O plugin\n");
		err = snd_pcm_plugin_build_io(plug, &slave_params.format, &plugin);
	}
	if (err < 0)
		return err;
	if (plug->slave->stream == SND_PCM_STREAM_PLAYBACK) {
		err = snd_pcm_plugin_append(plugin);
	} else {
		err = snd_pcm_plugin_insert(plugin);
	}
	if (err < 0) {
		snd_pcm_plugin_free(plugin);
		return err;
	}

	pdprintf("params requested params: format = %i, rate = %i, channels = %i\n", slave_params.format.format, slave_params.format.rate, slave_params.format.channels);
	err = snd_pcm_params(plug->slave, &slave_params);
	if (err < 0)
		return err;

	err = snd_pcm_plug_action(plug, INIT, 0);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_plug_create(snd_pcm_t **handlep, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *handle;
	snd_pcm_plug_t *plug;
	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle)
		return -ENOMEM;
	plug = calloc(1, sizeof(snd_pcm_plug_t));
	if (!plug) {
		free(handle);
		return -ENOMEM;
	}
	plug->handle = handle;
	plug->slave = slave;
	plug->close_slave = close_slave;
	handle->type = SND_PCM_TYPE_PLUG;
	handle->stream = slave->stream;
	handle->ops = malloc(sizeof(*handle->ops));
	*handle->ops = *slave->ops;
	handle->ops->params = snd_pcm_plug_params;
	handle->ops->setup = snd_pcm_plug_setup;
	handle->ops->info = snd_pcm_plug_info;
	handle->op_arg = slave->op_arg;
	handle->mode = slave->mode;
	handle->private = plug;
	*handlep = handle;
	return 0;
}

int snd_pcm_plug_open_subdevice(snd_pcm_t **handlep, int card, int device, int subdevice, int stream, int mode)
{
	snd_pcm_t *slave;
	int err;
	err = snd_pcm_hw_open_subdevice(&slave, card, device, subdevice, stream, mode);
	if (err < 0)
		return err;
	return snd_pcm_plug_create(handlep, slave, 1);
}

int snd_pcm_plug_open(snd_pcm_t **handlep, int card, int device, int stream, int mode)
{
	return snd_pcm_plug_open_subdevice(handlep, card, device, -1, stream, mode);
}


