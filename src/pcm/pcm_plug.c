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
#include <limits.h>
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

void snd_pcm_plugin_dump(snd_pcm_plugin_t *plugin, FILE *fp)
{
	fprintf(fp, "----------- %s\n", plugin->name);
	fprintf(fp, "Buffer: %d frames\n", plugin->buf_frames);
	if (plugin->src_format.interleave != plugin->dst_format.interleave) {
		if (plugin->src_format.interleave)
			fprintf(fp, "Interleaved -> Non interleaved\n");
		else
			fprintf(fp, "Non interleaved -> Interleaved\n");
	}
	if (plugin->src_format.channels != plugin->dst_format.channels) {
		fprintf(fp, "Channels: %d -> %d\n", 
			plugin->src_format.channels, 
			plugin->dst_format.channels);
	}
	if (plugin->src_format.format != plugin->dst_format.format) {
		fprintf(fp, "Format: %s -> %s\n", 
			snd_pcm_format_name(plugin->src_format.format),
			snd_pcm_format_name(plugin->dst_format.format));
	}
	if (plugin->src_format.rate != plugin->dst_format.rate) {
		fprintf(fp, "Rate: %d -> %d\n", 
			plugin->src_format.rate, 
			plugin->dst_format.rate);
	}
	if (plugin->dump)
		plugin->dump(plugin, fp);
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
	info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	info->flags |= SND_PCM_INFO_INTERLEAVE | SND_PCM_INFO_NONINTERLEAVE;
	return 0;
}

static int snd_pcm_plug_params_info(void *private, snd_pcm_params_info_t *info)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_params_info_t slave_info;
	int rate;
	int slave_format, slave_rate;
	unsigned int slave_channels;

	memset(&info->formats, 0, (char*)(info + 1) - (char*) &info->formats);
	info->req.fail_reason = 0;
	info->req.fail_mask = 0;

	if (info->req_mask & SND_PCM_PARAMS_RATE) {
		info->min_rate = info->req.format.rate;
		info->max_rate = info->req.format.rate;
	} else {
		info->min_rate = 4000;
		info->max_rate = 192000;
	}
	/* ??? */
	info->rates = SND_PCM_RATE_CONTINUOUS | SND_PCM_RATE_8000_192000;
	if (info->req_mask & SND_PCM_PARAMS_CHANNELS) {
		info->min_channels = info->req.format.channels;
		info->max_channels = info->req.format.channels;
	} else {
		info->min_channels = 1;
		info->max_channels = 32;
	}

	memset(&slave_info, 0, sizeof(slave_info));
	if ((err = snd_pcm_params_info(plug->slave, &slave_info)) < 0)
		return err;

	if (info->req_mask & SND_PCM_PARAMS_FORMAT) 
		info->formats = 1 << info->req.format.format;
	else
		info->formats = snd_pcm_plug_formats(slave_info.formats);

	info->min_fragments = slave_info.min_fragments;
	info->max_fragments = slave_info.max_fragments;
	
	if (!(info->req_mask & SND_PCM_PARAMS_FORMAT))
		return 0;
	slave_format = snd_pcm_plug_slave_fmt(info->req.format.format, &slave_info);
	if (slave_format < 0) {
		info->req.fail_mask = SND_PCM_PARAMS_FORMAT;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}

	if (!(info->req_mask & SND_PCM_PARAMS_RATE))
		return 0;
	slave_rate = snd_pcm_plug_slave_rate(info->req.format.rate, &slave_info);
	if (slave_rate < 0) {
		info->req.fail_mask = SND_PCM_PARAMS_RATE;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}

	if (!(info->req_mask & SND_PCM_PARAMS_CHANNELS))
		return 0;
	slave_channels = info->req.format.rate;
	if (slave_channels < info->min_channels)
		slave_channels = info->min_channels;
	else if (slave_channels > info->max_channels)
		slave_channels = info->max_channels;

	slave_info.req_mask = (SND_PCM_PARAMS_FORMAT |
			       SND_PCM_PARAMS_CHANNELS |
			       SND_PCM_PARAMS_RATE);
	slave_info.req.format.format = info->req.format.format;
	slave_info.req.format.channels = info->req.format.channels;
	slave_info.req.format.rate = info->req.format.rate;
	if ((err = snd_pcm_params_info(plug->slave, &slave_info)) < 0) {
		info->req.fail_mask = slave_info.req.fail_mask;
		info->req.fail_reason = slave_info.req.fail_reason;
		return err;
	}
	rate = info->req.format.rate;
	info->buffer_size = slave_info.buffer_size * rate / slave_rate;
	info->min_fragment_size = slave_info.min_fragment_size * rate / slave_rate;
	info->max_fragment_size = slave_info.max_fragment_size * rate / slave_rate;
	info->fragment_align = slave_info.fragment_align * rate / slave_rate;
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
	setup->mmap_bytes = 0;
	if (plug->handle->stream == SND_PCM_STREAM_PLAYBACK)
		setup->format = plug->first->src_format;
	else
		setup->format = plug->last->dst_format;
	/* FIXME: this is not exact */
	setup->rate_master = setup->format.rate;
	setup->rate_divisor = 1;
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
  
ssize_t snd_pcm_plug_writev(void *private, snd_timestamp_t tstamp UNUSED, const struct iovec *vector, unsigned long count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_t *handle = plug->handle;
	unsigned int k, step;
	size_t result = 0;
	assert(plug->frames_alloc);
	if (handle->setup.format.interleave)
		step = 1;
	else
		step = handle->setup.format.channels;
	for (k = 0; k < count; k += step) {
		snd_pcm_plugin_channel_t *channels;
		ssize_t frames;
		frames = snd_pcm_plug_client_channels_iovec(plug, vector, step, &channels);
		if (frames < 0) {
			if (result > 0)
				return result;
			return frames;
		}
		while (1) {
			unsigned int c;
			ssize_t ret;
			size_t frames1 = frames;
			if (frames1 > plug->frames_alloc)
				frames1 = plug->frames_alloc;
			ret = snd_pcm_plug_write_transfer(plug, channels, frames1);
			if (ret < 0) {
				if (result > 0)
					return result;
				return ret;
			}
			result += ret;
			frames -= ret;
			if (frames == 0)
				break;
			for (c = 0; c < handle->setup.format.channels; ++c)
				channels[c].area.addr += ret * channels[c].area.step / 8;
		}
		vector += step;
	}
	return result;
}

ssize_t snd_pcm_plug_readv(void *private, snd_timestamp_t tstamp UNUSED, const struct iovec *vector, unsigned long count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_t *handle = plug->handle;
	unsigned int k, step;
	size_t result = 0;
	assert(plug->frames_alloc);
	if (handle->setup.format.interleave)
		step = 1;
	else
		step = handle->setup.format.channels;
	for (k = 0; k < count; k += step) {
		snd_pcm_plugin_channel_t *channels;
		ssize_t frames;
		frames = snd_pcm_plug_client_channels_iovec(plug, vector, step, &channels);
		if (frames < 0) {
			if (result > 0)
				return result;
			return frames;
		}
		while (1) {
			unsigned int c;
			ssize_t ret;
			size_t frames1 = frames;
			if (frames1 > plug->frames_alloc)
				frames1 = plug->frames_alloc;
			ret = snd_pcm_plug_read_transfer(plug, channels, frames1);
			if (ret < 0) {
				if (result > 0)
					return result;
				return ret;
			}
			result += ret;
			frames -= ret;
			if (frames == 0)
				break;
			for (c = 0; c < handle->setup.format.channels; ++c)
				channels[c].area.addr += ret * channels[c].area.step / 8;
		}
		vector += step;
	}
	return result;
}

ssize_t snd_pcm_plug_write(void *private, snd_timestamp_t tstamp UNUSED, const void *buf, size_t count)
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

ssize_t snd_pcm_plug_read(void *private, snd_timestamp_t tstamp UNUSED, void *buf, size_t count)
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

static void snd_pcm_plug_dump(void *private, FILE *fp)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) private;
	snd_pcm_t *handle = plug->handle;
	snd_pcm_plugin_t *plugin;
	fprintf(fp, "Plug PCM\n");
	if (handle->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(handle, fp);
	}
	fprintf(fp, "\nPlugins:\n");
	plugin = plug->first;
	while (plugin) {
		snd_pcm_plugin_dump(plugin, fp);
		plugin = plugin->next;
	}
	fprintf(fp, "\n");
}

static int snd_pcm_plug_params(void *private, snd_pcm_params_t *params);

struct snd_pcm_ops snd_pcm_plug_ops = {
	close: snd_pcm_plug_close,
	nonblock: snd_pcm_plug_nonblock,
	info: snd_pcm_plug_info,
	params_info: snd_pcm_plug_params_info,
	params: snd_pcm_plug_params,
	setup: snd_pcm_plug_setup,
	channel_setup: snd_pcm_plug_channel_setup,
	status: snd_pcm_plug_status,
	frame_io: snd_pcm_plug_frame_io,
	state: snd_pcm_plug_state,
	prepare: snd_pcm_plug_prepare,
	go: snd_pcm_plug_go,
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
	dump: snd_pcm_plug_dump,
};

static void snd_pcm_plug_slave_params(snd_pcm_plug_t *plug,
				      snd_pcm_params_t *params,
				      snd_pcm_params_t *slave_params)
{
	/* compute right sizes */
	slave_params->frag_size = snd_pcm_plug_slave_size(plug, params->frag_size);
	slave_params->buffer_size = snd_pcm_plug_slave_size(plug, params->buffer_size);
	slave_params->frames_fill_max = snd_pcm_plug_slave_size(plug, params->frames_fill_max);
	slave_params->frames_min = snd_pcm_plug_slave_size(plug, params->frames_min);
	slave_params->frames_xrun_max = snd_pcm_plug_slave_size(plug, params->frames_xrun_max);
	slave_params->frames_align = snd_pcm_plug_slave_size(plug, params->frames_align);
	if (slave_params->frame_boundary == 0 || slave_params->frame_boundary > INT_MAX)
		slave_params->frame_boundary = INT_MAX;
	assert(params->buffer_size > 0);
	slave_params->frame_boundary /= params->buffer_size;
	if (slave_params->frame_boundary > INT_MAX / slave_params->buffer_size)
		slave_params->frame_boundary = INT_MAX;
	else
		slave_params->frame_boundary *= slave_params->buffer_size;
}



static int snd_pcm_plug_params(void *private, snd_pcm_params_t *params)
{
	snd_pcm_params_t slave_params;
	snd_pcm_info_t slave_info;
	snd_pcm_format_t *req_format, *real_format, format1;
	snd_pcm_params_info_t slave_params_info;
	snd_pcm_plugin_t *plugin;
	snd_pcm_plug_t *plug;
	int err;
	int first = 1;
	
	plug = (snd_pcm_plug_t*) private;

	/*
	 *  try to decide, if a conversion is required
         */

	memset(&slave_info, 0, sizeof(slave_info));
	if ((err = snd_pcm_info(plug->slave, &slave_info)) < 0) {
		snd_pcm_plug_clear(plug);
		return err;
	}
	memset(&slave_params_info, 0, sizeof(slave_params_info));
	if ((err = snd_pcm_params_info(plug->slave, &slave_params_info)) < 0) {
		snd_pcm_plug_clear(plug);
		return err;
	}

	slave_params = *params;
	if ((err = snd_pcm_plug_slave_format(&params->format, &slave_info, &slave_params_info, &slave_params.format)) < 0)
		return err;

 retry:
	/* add necessary plugins */
	format1 = params->format;
	snd_pcm_plug_clear(plug);
	if ((err = snd_pcm_plug_format_plugins(plug, &format1, 
					       &slave_params.format)) < 0)
		return err;

	/* compute right sizes */
	snd_pcm_plug_slave_params(plug, params, &slave_params);

	pdprintf("params requested params: format = %i, rate = %i, channels = %i\n", slave_params.format.format, slave_params.format.rate, slave_params.format.channels);

	err = snd_pcm_params(plug->slave, &slave_params);
	if (err < 0) {
		params->fail_mask = slave_params.fail_mask;
		params->fail_reason = slave_params.fail_reason;
		return err;
	}
	req_format = &slave_params.format;
	real_format = &plug->slave->setup.format;
	if (real_format->interleave != req_format->interleave ||
	    real_format->format != req_format->format ||
	    real_format->rate != req_format->rate ||
	    real_format->channels != req_format->channels) {
		assert(first);
		slave_params.format = *real_format;
		first = 0;
		goto retry;
	}

	if (!plug->first) {
		*plug->handle->ops = *plug->slave->ops;
		plug->handle->ops->params = snd_pcm_plug_params;
		plug->handle->ops->setup = snd_pcm_plug_setup;
		plug->handle->ops->info = snd_pcm_plug_info;
		plug->handle->ops->params_info = snd_pcm_plug_params_info;
		plug->handle->op_arg = plug->slave->op_arg;
		return 0;
	}

	*plug->handle->ops = snd_pcm_plug_ops;
	plug->handle->op_arg = plug;

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
	handle->ops->params_info = snd_pcm_plug_params_info;
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


