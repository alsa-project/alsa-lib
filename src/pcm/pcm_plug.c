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

/* snd_pcm_plug helpers */

void *snd_pcm_plug_buf_alloc(snd_pcm_t *pcm, int stream, size_t size)
{
	int idx;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	snd_pcm_plug_stream_t *plugstr = &plug->stream[stream];

	for (idx = 0; idx < 2; idx++) {
		if (plugstr->alloc_lock[idx])
			continue;
		if (plugstr->alloc_size[idx] >= size) {
			plugstr->alloc_lock[idx] = 1;
			return plugstr->alloc_ptr[idx];
		}
	}
	for (idx = 0; idx < 2; idx++) {
		if (plugstr->alloc_lock[idx])
			continue;
		if (plugstr->alloc_ptr[idx] != NULL)
			free(plugstr->alloc_ptr[idx]);
		plugstr->alloc_ptr[idx] = malloc(size);
		if (plugstr->alloc_ptr[idx] == NULL)
			return NULL;
		plugstr->alloc_size[idx] = size;
		plugstr->alloc_lock[idx] = 1;
		return plugstr->alloc_ptr[idx];
	}
	return NULL;
}

void snd_pcm_plug_buf_unlock(snd_pcm_t *pcm, int stream, void *ptr)
{
	int idx;

	snd_pcm_plug_t *plug;
	snd_pcm_plug_stream_t *plugstr;

	if (!ptr)
		return;
	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[stream];

	for (idx = 0; idx < 2; idx++) {
		if (plugstr->alloc_ptr[idx] == ptr) {
			plugstr->alloc_lock[idx] = 0;
			return;
		}
	}
}

/* snd_pcm_plugin externs */

int snd_pcm_plugin_insert(snd_pcm_plugin_t *plugin)
{
	snd_pcm_plug_t *plug;
	snd_pcm_plug_stream_t *plugstr;
	snd_pcm_t *pcm;
	assert(plugin);
	pcm = plugin->handle;
	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[plugin->stream];
	plugin->next = plugstr->first;
	plugin->prev = NULL;
	if (plugstr->first) {
		plugstr->first->prev = plugin;
		plugstr->first = plugin;
	} else {
		plugstr->last =
		plugstr->first = plugin;
	}
	return 0;
}

int snd_pcm_plugin_append(snd_pcm_plugin_t *plugin)
{
	snd_pcm_plug_t *plug;
	snd_pcm_plug_stream_t *plugstr;
	snd_pcm_t *pcm;
	assert(plugin);
	pcm = plugin->handle;
	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[plugin->stream];

	plugin->next = NULL;
	plugin->prev = plugstr->last;
	if (plugstr->last) {
		plugstr->last->next = plugin;
		plugstr->last = plugin;
	} else {
		plugstr->last =
		plugstr->first = plugin;
	}
	return 0;
}

#if 0
int snd_pcm_plugin_remove_to(snd_pcm_plugin_t *plugin)
{
	snd_pcm_plugin_t *plugin1, *plugin1_prev;
	snd_pcm_plug_t *plug;
	snd_pcm_t *pcm;
	snd_pcm_plug_stream_t *plugstr;
	assert(plugin);
	pcm = plugin->handle;

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[plugin->stream];

	plugin1 = plugin;
	while (plugin1->prev)
		plugin1 = plugin1->prev;
	if (plugstr->first != plugin1)
		return -EINVAL;
	plugstr->first = plugin;
	plugin1 = plugin->prev;
	plugin->prev = NULL;
	while (plugin1) {
		plugin1_prev = plugin1->prev;
		snd_pcm_plugin_free(plugin1);
		plugin1 = plugin1_prev;
	}
	return 0;
}

int snd_pcm_plug_remove_first(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plugin_t *plugin;
	snd_pcm_plug_t *plug;
	snd_pcm_plug_stream_t *plugstr;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	assert(pcm->stream[stream].open);

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[stream];

	plugin = plugstr->first;
	if (plugin->next) {
		plugin = plugin->next;
	} else {
		return snd_pcm_plug_clear(pcm, stream);
	}
	return snd_pcm_plugin_remove_to(plugin);
}
#endif

/* snd_pcm_plug externs */

int snd_pcm_plug_clear(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plugin_t *plugin, *plugin_next;
	snd_pcm_plug_t *plug;
	snd_pcm_plug_stream_t *plugstr;
	int idx;
	
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	assert(pcm->stream[stream].open);

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[stream];
	plugin = plugstr->first;
	plugstr->first = NULL;
	plugstr->last = NULL;
	while (plugin) {
		plugin_next = plugin->next;
		snd_pcm_plugin_free(plugin);
		plugin = plugin_next;
	}
	for (idx = 0; idx < 2; idx++) {
		if (plugstr->alloc_ptr[idx]) {
			free(plugstr->alloc_ptr[idx]);
			plugstr->alloc_ptr[idx] = 0;
		}
		plugstr->alloc_size[idx] = 0;
		plugstr->alloc_lock[idx] = 0;
	}
	return 0;
}

snd_pcm_plugin_t *snd_pcm_plug_first(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plug_t *plug;
	snd_pcm_plug_stream_t *plugstr;
	if (!pcm)
		return NULL;
	if (stream < 0 || stream > 1)
		return NULL;
	if (!pcm->stream[stream].open)
		return NULL;

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[stream];

	return plugstr->first;
}

snd_pcm_plugin_t *snd_pcm_plug_last(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plug_t *plug;
	snd_pcm_plug_stream_t *plugstr;
	if (!pcm)
		return NULL;
	if (stream < 0 || stream > 1)
		return NULL;
	if (!pcm->stream[stream].open)
		return NULL;

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[stream];

	return plugstr->last;
}

int snd_pcm_plug_direct(snd_pcm_t *pcm, int stream)
{
	return snd_pcm_plug_first(pcm, stream) == NULL;
}

/*
 *
 */

static int snd_pcm_plug_stream_close(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	snd_pcm_plug_clear(pcm, stream);
	if (plug->close_slave)
		return snd_pcm_stream_close(plug->slave, stream);
	return 0;
}

static int snd_pcm_plug_stream_nonblock(snd_pcm_t *pcm, int stream, int nonblock)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_stream_nonblock(plug->slave, stream, nonblock);
}

static int snd_pcm_plug_info(snd_pcm_t *pcm, int stream UNUSED, snd_pcm_info_t * info)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_info(plug->slave, info);
}

static int snd_pcm_plug_stream_info(snd_pcm_t *pcm, snd_pcm_stream_info_t *info)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	snd_pcm_stream_t *str;
	
	if ((err = snd_pcm_stream_info(plug->slave, info)) < 0)
		return err;
	info->formats = snd_pcm_plug_formats(info->formats);
	info->min_rate = 4000;
	info->max_rate = 192000;
	info->min_channels = 1;
	info->max_channels = 32;
	info->rates = SND_PCM_RATE_CONTINUOUS | SND_PCM_RATE_8000_192000;
	info->flags |= SND_PCM_STREAM_INFO_INTERLEAVE | SND_PCM_STREAM_INFO_NONINTERLEAVE;

	str = &pcm->stream[info->stream];
	if (pcm->stream[info->stream].valid_setup) {
		info->buffer_size = snd_pcm_plug_client_size(pcm, info->stream, info->buffer_size);
		info->min_fragment_size = snd_pcm_plug_client_size(pcm, info->stream, info->min_fragment_size);
		info->max_fragment_size = snd_pcm_plug_client_size(pcm, info->stream, info->max_fragment_size);
		info->fragment_align = snd_pcm_plug_client_size(pcm, info->stream, info->fragment_align);
		info->fifo_size = snd_pcm_plug_client_size(pcm, info->stream, info->fifo_size);
		info->transfer_block_size = snd_pcm_plug_client_size(pcm, info->stream, info->transfer_block_size);
		if (str->setup.mode == SND_PCM_MODE_FRAGMENT)
			info->mmap_size = str->setup.buffer_size;
		else
			info->mmap_size = snd_pcm_plug_client_size(pcm, info->stream, info->mmap_size);
	}
	if (!snd_pcm_plug_direct(pcm, info->stream))
		info->flags &= ~(SND_PCM_STREAM_INFO_MMAP | SND_PCM_STREAM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_plug_action(snd_pcm_t *pcm, int stream, int action,
			       unsigned long data)
{
	snd_pcm_plugin_t *plugin;
	int err;
	snd_pcm_plug_t *plug;
	snd_pcm_plug_stream_t *plugstr;
	plug = (snd_pcm_plug_t*) &pcm->private;
	plugstr = &plug->stream[stream];

	plugin = plugstr->first;
	while (plugin) {
		if (plugin->action) {
			if ((err = plugin->action(plugin, action, data))<0)
				return err;
		}
		plugin = plugin->next;
	}
	return 0;
}

static int snd_pcm_plug_stream_params(snd_pcm_t *pcm, snd_pcm_stream_params_t *params)
{
	snd_pcm_stream_params_t slave_params, params1;
	snd_pcm_stream_info_t slave_info;
	snd_pcm_plugin_t *plugin;
	snd_pcm_plug_t *plug;
	size_t bytes_per_frame;
	int err;
	int stream = params->stream;
	
	plug = (snd_pcm_plug_t*) &pcm->private;

	/*
	 *  try to decide, if a conversion is required
         */

	memset(&slave_info, 0, sizeof(slave_info));
	slave_info.stream = stream;
	if ((err = snd_pcm_stream_info(plug->slave, &slave_info)) < 0) {
		snd_pcm_plug_clear(pcm, stream);
		return err;
	}

	if ((err = snd_pcm_plug_slave_params(params, &slave_info, &slave_params)) < 0)
		return err;


	snd_pcm_plug_clear(pcm, stream);

	/* add necessary plugins */
	memcpy(&params1, params, sizeof(*params));
	if ((err = snd_pcm_plug_format(pcm, &params1, &slave_params)) < 0)
		return err;

	if (snd_pcm_plug_direct(pcm, stream))
		return snd_pcm_stream_params(plug->slave, params);

	/* compute right sizes */
	bytes_per_frame = snd_pcm_format_size(params->format.format, params->format.channels);
	if (bytes_per_frame == 0)
		bytes_per_frame = 1;
	params1.frag_size -= params1.frag_size % bytes_per_frame;
	slave_params.frag_size = snd_pcm_plug_slave_size(pcm, stream, params1.frag_size);
	params1.buffer_size -= params1.buffer_size % bytes_per_frame;
	slave_params.buffer_size = snd_pcm_plug_slave_size(pcm, stream, params1.buffer_size);
	params1.bytes_fill_max -= params1.bytes_fill_max % bytes_per_frame;
	slave_params.bytes_fill_max = snd_pcm_plug_slave_size(pcm, stream, params1.bytes_fill_max);
	params1.bytes_min -= params1.bytes_min % bytes_per_frame;
	slave_params.bytes_min = snd_pcm_plug_slave_size(pcm, stream, params1.bytes_min);
	params1.bytes_xrun_max -= params1.bytes_xrun_max % bytes_per_frame;
	slave_params.bytes_xrun_max = snd_pcm_plug_slave_size(pcm, stream, params1.bytes_xrun_max);
	params1.bytes_align -= params1.bytes_align % bytes_per_frame;
	slave_params.bytes_align = snd_pcm_plug_slave_size(pcm, stream, params1.bytes_align);
	if (slave_params.byte_boundary == 0 || slave_params.byte_boundary > INT_MAX)
		slave_params.byte_boundary = INT_MAX;
	slave_params.byte_boundary /= params->buffer_size;
	if (slave_params.byte_boundary > INT_MAX / slave_params.buffer_size)
		slave_params.byte_boundary = INT_MAX;
	else
		slave_params.byte_boundary *= slave_params.buffer_size;

	/*
	 *  I/O plugins
	 */

	if (slave_info.flags & SND_PCM_STREAM_INFO_MMAP) {
		pdprintf("params mmap plugin\n");
		err = snd_pcm_plugin_build_mmap(pcm, stream, plug->slave, &slave_params.format, &plugin);
	} else {
		pdprintf("params I/O plugin\n");
		err = snd_pcm_plugin_build_io(pcm, stream, plug->slave, &slave_params.format, &plugin);
	}
	if (err < 0)
		return err;
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		err = snd_pcm_plugin_append(plugin);
	} else {
		err = snd_pcm_plugin_insert(plugin);
	}
	if (err < 0) {
		snd_pcm_plugin_free(plugin);
		return err;
	}

	pdprintf("params requested params: format = %i, rate = %i, channels = %i\n", slave_params.format.format, slave_params.format.rate, slave_params.format.channels);
	err = snd_pcm_stream_params(plug->slave, &slave_params);
	if (err < 0)
		return err;

	err = snd_pcm_plug_action(pcm, stream, INIT, 0);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_plug_stream_setup(snd_pcm_t *pcm, snd_pcm_stream_setup_t *setup)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	snd_pcm_plug_stream_t *plugstr;

	err = snd_pcm_stream_setup(plug->slave, setup);
	if (err < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, setup->stream))
		return 0;
	setup->byte_boundary /= setup->frag_size;
	setup->frag_size = snd_pcm_plug_client_size(pcm, setup->stream, setup->frag_size);
	setup->byte_boundary *= setup->frag_size;
	setup->buffer_size = setup->frags * setup->frag_size;
	setup->bytes_min = snd_pcm_plug_client_size(pcm, setup->stream, setup->bytes_min);
	setup->bytes_align = snd_pcm_plug_client_size(pcm, setup->stream, setup->bytes_align);
	setup->bytes_xrun_max = snd_pcm_plug_client_size(pcm, setup->stream, setup->bytes_xrun_max);
	setup->bytes_fill_max = snd_pcm_plug_client_size(pcm, setup->stream, setup->bytes_fill_max);

	plugstr = &plug->stream[setup->stream];
	if (setup->stream == SND_PCM_STREAM_PLAYBACK)
		setup->format = plugstr->first->src_format;
	else
		setup->format = plugstr->last->dst_format;
	return 0;	
}

static int snd_pcm_plug_stream_status(snd_pcm_t *pcm, snd_pcm_stream_status_t *status)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;

	err = snd_pcm_stream_status(plug->slave, status);
	if (err < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, status->stream))
		return 0;

	status->byte_io = snd_pcm_plug_client_size(pcm, status->stream, status->byte_io);
	status->byte_data = snd_pcm_plug_client_size(pcm, status->stream, status->byte_data);
	status->bytes_avail = snd_pcm_plug_client_size(pcm, status->stream, status->bytes_avail);
	status->bytes_avail_max = snd_pcm_plug_client_size(pcm, status->stream, status->bytes_avail_max);
	return 0;	
}

static int snd_pcm_plug_stream_state(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_stream_state(plug->slave, stream);
}

static int snd_pcm_plug_stream_byte_io(snd_pcm_t *pcm, int stream, int update)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_stream_byte_io(plug->slave, stream, update);
}

static int snd_pcm_plug_stream_prepare(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;
	err = snd_pcm_stream_prepare(plug->slave, stream);
	if (err < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, stream))
		return 0;
	if ((err = snd_pcm_plug_action(pcm, stream, PREPARE, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_stream_go(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_stream_go(plug->slave, stream);
}

static int snd_pcm_plug_sync_go(snd_pcm_t *pcm, int stream UNUSED, snd_pcm_sync_t *sync)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_sync_go(plug->slave, sync);
}

static int snd_pcm_plug_stream_drain(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;

	if ((err = snd_pcm_stream_drain(plug->slave, stream)) < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, stream))
		return 0;
	if ((err = snd_pcm_plug_action(pcm, stream, DRAIN, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_stream_flush(snd_pcm_t *pcm, int stream)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;

	if ((err = snd_pcm_stream_flush(plug->slave, stream)) < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, stream))
		return 0;
	if ((err = snd_pcm_plug_action(pcm, stream, FLUSH, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_stream_pause(snd_pcm_t *pcm, int stream, int enable)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;
	
	if ((err = snd_pcm_stream_pause(plug->slave, stream, enable)) < 0)
		return err;
	if ((err = snd_pcm_plug_action(pcm, stream, PAUSE, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_channel_setup(snd_pcm_t *pcm, int stream, snd_pcm_channel_setup_t *setup)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;

	if (snd_pcm_plug_direct(pcm, stream))
		return snd_pcm_channel_setup(plug->slave, stream, setup);
	/* FIXME: non mmap setups */
	return -ENXIO;
}

static ssize_t snd_pcm_plug_stream_seek(snd_pcm_t *pcm, int stream, off_t offset)
{
	ssize_t ret;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, stream))
		return snd_pcm_stream_seek(plug->slave, stream, offset);
	if (offset < 0) {
		offset = snd_pcm_plug_slave_size(pcm, stream, -offset);
		if (offset < 0)
			return offset;
		offset = -offset;
	} else {
		offset = snd_pcm_plug_slave_size(pcm, stream, offset);
		if (offset < 0)
			return offset;
	}
	ret = snd_pcm_stream_seek(plug->slave, stream, offset);
	if (ret < 0)
		return ret;
	return snd_pcm_plug_client_size(pcm, stream, ret);
}
  
ssize_t snd_pcm_plug_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	unsigned int k, step, channels;
	int size = 0;
	if (snd_pcm_plug_direct(pcm, SND_PCM_STREAM_PLAYBACK))
		return snd_pcm_writev(plug->slave, vector, count);
	channels = str->setup.format.channels;
	if (str->setup.format.interleave)
		step = 1;
	else {
		step = channels;
		assert(count % channels == 0);
	}
	for (k = 0; k < count; k += step, vector += step) {
		snd_pcm_plugin_channel_t *channels;
		int expected, ret;
		expected = snd_pcm_plug_client_channels_iovec(pcm, SND_PCM_STREAM_PLAYBACK, vector, count, &channels);
		if (expected < 0)
			return expected;
		ret = snd_pcm_plug_write_transfer(pcm, channels, expected);
		if (ret < 0) {
			if (size > 0)
				return size;
			return ret;
		}
		size += ret;
		if (ret != expected)
			return size;
	}
	return size;
}

ssize_t snd_pcm_plug_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	unsigned int k, step, channels;
	int size = 0;
	if (snd_pcm_plug_direct(pcm, SND_PCM_STREAM_CAPTURE))
		return snd_pcm_readv(plug->slave, vector, count);
	channels = str->setup.format.channels;
	if (str->setup.format.interleave)
		step = 1;
	else {
		step = channels;
		assert(count % channels == 0);
	}
	for (k = 0; k < count; k += step) {
		snd_pcm_plugin_channel_t *channels;
		int expected, ret;
		expected = snd_pcm_plug_client_channels_iovec(pcm, SND_PCM_STREAM_CAPTURE, vector, count, &channels);
		if (expected < 0)
			return expected;
		ret = snd_pcm_plug_read_transfer(pcm, channels, expected);
		if (ret < 0) {
			if (size > 0)
				return size;
			return ret;
		}
		size += ret;
		if (ret != expected)
			return size;
	}
	return size;
}

ssize_t snd_pcm_plug_write(snd_pcm_t *pcm, const void *buf, size_t count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int expected;
	snd_pcm_plugin_channel_t *channels;

	if (snd_pcm_plug_direct(pcm, SND_PCM_STREAM_PLAYBACK))
		return snd_pcm_write(plug->slave, buf, count);
	expected = snd_pcm_plug_client_channels_buf(pcm, SND_PCM_STREAM_PLAYBACK, (char *)buf, count, &channels);
	if (expected < 0)
		return expected;
	 return snd_pcm_plug_write_transfer(pcm, channels, expected);
}

ssize_t snd_pcm_plug_read(snd_pcm_t *pcm, void *buf, size_t count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int expected;
	snd_pcm_plugin_channel_t *channels;

	if (snd_pcm_plug_direct(pcm, SND_PCM_STREAM_CAPTURE))
		return snd_pcm_read(plug->slave, buf, count);
	expected = snd_pcm_plug_client_channels_buf(pcm, SND_PCM_STREAM_CAPTURE, buf, count, &channels);
	if (expected < 0)
		return expected;
	return snd_pcm_plug_read_transfer(pcm, channels, expected);
}

static int snd_pcm_plug_mmap_control(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t **control, size_t csize UNUSED)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, stream))
		return snd_pcm_mmap_control(plug->slave, stream, control);
	return -EBADFD;
}

static int snd_pcm_plug_mmap_data(snd_pcm_t *pcm, int stream, void **buffer, size_t bsize UNUSED)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, stream))
		return snd_pcm_mmap_data(plug->slave, stream, buffer);
	return -EBADFD;
}

static int snd_pcm_plug_munmap_control(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t *control UNUSED, size_t csize UNUSED)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, stream))
		return snd_pcm_munmap_control(plug->slave, stream);
	return -EBADFD;
}
		
static int snd_pcm_plug_munmap_data(snd_pcm_t *pcm, int stream, void *buffer UNUSED, size_t size UNUSED)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, stream))
		return snd_pcm_munmap_data(plug->slave, stream);
	return -EBADFD;
}
		
static int snd_pcm_plug_channels_mask(snd_pcm_t *pcm, int stream,
				    bitset_t *client_vmask)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, stream))
		return snd_pcm_channels_mask(plug->slave, stream, client_vmask);
	if (stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_plug_playback_channels_mask(pcm, client_vmask);
	else
		return snd_pcm_plug_capture_channels_mask(pcm, client_vmask);
}

int snd_pcm_plug_file_descriptor(snd_pcm_t* pcm, int stream)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_file_descriptor(plug->slave, stream);
}

struct snd_pcm_ops snd_pcm_plug_ops = {
	stream_close: snd_pcm_plug_stream_close,
	stream_nonblock: snd_pcm_plug_stream_nonblock,
	info: snd_pcm_plug_info,
	stream_info: snd_pcm_plug_stream_info,
	stream_params: snd_pcm_plug_stream_params,
	stream_setup: snd_pcm_plug_stream_setup,
	channel_setup: snd_pcm_plug_channel_setup,
	stream_status: snd_pcm_plug_stream_status,
	stream_byte_io: snd_pcm_plug_stream_byte_io,
	stream_state: snd_pcm_plug_stream_state,
	stream_prepare: snd_pcm_plug_stream_prepare,
	stream_go: snd_pcm_plug_stream_go,
	sync_go: snd_pcm_plug_sync_go,
	stream_drain: snd_pcm_plug_stream_drain,
	stream_flush: snd_pcm_plug_stream_flush,
	stream_pause: snd_pcm_plug_stream_pause,
	stream_seek: snd_pcm_plug_stream_seek,
	write: snd_pcm_plug_write,
	writev: snd_pcm_plug_writev,
	read: snd_pcm_plug_read,
	readv: snd_pcm_plug_readv,
	mmap_control: snd_pcm_plug_mmap_control,
	mmap_data: snd_pcm_plug_mmap_data,
	munmap_control: snd_pcm_plug_munmap_control,
	munmap_data: snd_pcm_plug_munmap_data,
	file_descriptor: snd_pcm_plug_file_descriptor,
	channels_mask: snd_pcm_plug_channels_mask,
};

int snd_pcm_plug_connect(snd_pcm_t **handle, snd_pcm_t *slave, int mode, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_plug_t *plug;
	int err;
	err = snd_pcm_abstract_open(handle, mode, SND_PCM_TYPE_PLUG, sizeof(snd_pcm_plug_t));
	if (err < 0) {
		if (close_slave)
			snd_pcm_close(slave);
		return err;
	}
	pcm = *handle;
	pcm->ops = &snd_pcm_plug_ops;
        plug = (snd_pcm_plug_t*) &pcm->private;
	plug->slave = slave;
	plug->close_slave = close_slave;
	return 0;
}

int snd_pcm_plug_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int mode)
{
	snd_pcm_t *slave;
	int err;
	err = snd_pcm_open_subdevice(&slave, card, device, subdevice, mode);
	if (err < 0)
		return err;
	return snd_pcm_plug_connect(handle, slave, mode, 1);
}

int snd_pcm_plug_open(snd_pcm_t **handle, int card, int device, int mode)
{
	return snd_pcm_plug_open_subdevice(handle, card, device, -1, mode);
}


