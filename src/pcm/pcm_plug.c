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

void *snd_pcm_plug_buf_alloc(snd_pcm_t *pcm, int channel, size_t size)
{
	int idx;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	struct snd_pcm_plug_chan *plugchan = &plug->chan[channel];

	for (idx = 0; idx < 2; idx++) {
		if (plugchan->alloc_lock[idx])
			continue;
		if (plugchan->alloc_size[idx] >= size) {
			plugchan->alloc_lock[idx] = 1;
			return plugchan->alloc_ptr[idx];
		}
	}
	for (idx = 0; idx < 2; idx++) {
		if (plugchan->alloc_lock[idx])
			continue;
		if (plugchan->alloc_ptr[idx] != NULL)
			free(plugchan->alloc_ptr[idx]);
		plugchan->alloc_ptr[idx] = malloc(size);
		if (plugchan->alloc_ptr[idx] == NULL)
			return NULL;
		plugchan->alloc_size[idx] = size;
		plugchan->alloc_lock[idx] = 1;
		return plugchan->alloc_ptr[idx];
	}
	return NULL;
}

void snd_pcm_plug_buf_unlock(snd_pcm_t *pcm, int channel, void *ptr)
{
	int idx;

	snd_pcm_plug_t *plug;
	struct snd_pcm_plug_chan *plugchan;

	if (!ptr)
		return;
	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[channel];

	for (idx = 0; idx < 2; idx++) {
		if (plugchan->alloc_ptr[idx] == ptr) {
			plugchan->alloc_lock[idx] = 0;
			return;
		}
	}
}

/* snd_pcm_plugin externs */

int snd_pcm_plugin_insert(snd_pcm_plugin_t *plugin)
{
	snd_pcm_plug_t *plug;
	struct snd_pcm_plug_chan *plugchan;
	snd_pcm_t *pcm;
	if (!plugin)
		return -EFAULT;
	pcm = plugin->handle;
	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[plugin->channel];
	plugin->next = plugchan->first;
	plugin->prev = NULL;
	if (plugchan->first) {
		plugchan->first->prev = plugin;
		plugchan->first = plugin;
	} else {
		plugchan->last =
		plugchan->first = plugin;
	}
	return 0;
}

int snd_pcm_plugin_append(snd_pcm_plugin_t *plugin)
{
	snd_pcm_plug_t *plug;
	struct snd_pcm_plug_chan *plugchan;
	snd_pcm_t *pcm;
	if (!plugin)
		return -EFAULT;
	pcm = plugin->handle;
	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[plugin->channel];

	plugin->next = NULL;
	plugin->prev = plugchan->last;
	if (plugchan->last) {
		plugchan->last->next = plugin;
		plugchan->last = plugin;
	} else {
		plugchan->last =
		plugchan->first = plugin;
	}
	return 0;
}

#if 0
int snd_pcm_plugin_remove_to(snd_pcm_plugin_t *plugin)
{
	snd_pcm_plugin_t *plugin1, *plugin1_prev;
	snd_pcm_plug_t *plug;
	snd_pcm_t *pcm;
	struct snd_pcm_plug_chan *plugchan;
	if (!plugin)
		return -EFAULT;
	pcm = plugin->handle;

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[plugin->channel];

	plugin1 = plugin;
	while (plugin1->prev)
		plugin1 = plugin1->prev;
	if (plugchan->first != plugin1)
		return -EINVAL;
	plugchan->first = plugin;
	plugin1 = plugin->prev;
	plugin->prev = NULL;
	while (plugin1) {
		plugin1_prev = plugin1->prev;
		snd_pcm_plugin_free(plugin1);
		plugin1 = plugin1_prev;
	}
	return 0;
}

int snd_pcm_plug_remove_first(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plugin_t *plugin;
	snd_pcm_plug_t *plug;
	struct snd_pcm_plug_chan *plugchan;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[channel];

	plugin = plugchan->first;
	if (plugin->next) {
		plugin = plugin->next;
	} else {
		return snd_pcm_plug_clear(pcm, channel);
	}
	return snd_pcm_plugin_remove_to(plugin);
}
#endif

/* snd_pcm_plug externs */

int snd_pcm_plug_clear(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plugin_t *plugin, *plugin_next;
	snd_pcm_plug_t *plug;
	struct snd_pcm_plug_chan *plugchan;
	int idx;
	
	if (!pcm)
		return -EINVAL;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[channel];
	plugin = plugchan->first;
	plugchan->first = NULL;
	plugchan->last = NULL;
	while (plugin) {
		plugin_next = plugin->next;
		snd_pcm_plugin_free(plugin);
		plugin = plugin_next;
	}
	for (idx = 0; idx < 2; idx++) {
		if (plugchan->alloc_ptr[idx]) {
			free(plugchan->alloc_ptr[idx]);
			plugchan->alloc_ptr[idx] = 0;
		}
		plugchan->alloc_size[idx] = 0;
		plugchan->alloc_lock[idx] = 0;
	}
	return 0;
}

snd_pcm_plugin_t *snd_pcm_plug_first(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plug_t *plug;
	struct snd_pcm_plug_chan *plugchan;
	if (!pcm)
		return NULL;
	if (channel < 0 || channel > 1)
		return NULL;
	if (!pcm->chan[channel].open)
		return NULL;

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[channel];

	return plugchan->first;
}

snd_pcm_plugin_t *snd_pcm_plug_last(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plug_t *plug;
	struct snd_pcm_plug_chan *plugchan;
	if (!pcm)
		return NULL;
	if (channel < 0 || channel > 1)
		return NULL;
	if (!pcm->chan[channel].open)
		return NULL;

	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[channel];

	return plugchan->last;
}

int snd_pcm_plug_direct(snd_pcm_t *pcm, int channel)
{
	return snd_pcm_plug_first(pcm, channel) == NULL;
}

#if 0
double snd_pcm_plug_client_ratio(snd_pcm_t *pcm, int channel)
{
	ssize_t client;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;

	client = snd_pcm_plug_client_size(pcm, channel, 1000000);
	if (client < 0)
		return 0;
	return (double)client / (double)1000000;
}

double snd_pcm_plug_slave_ratio(snd_pcm_t *pcm, int channel)
{
	ssize_t slave;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;

	slave = snd_pcm_plug_slave_size(pcm, channel, 1000000);
	if (slave < 0)
		return 0;
	return (double)slave / (double)1000000;
}
#endif

/*
 *
 */

static int snd_pcm_plug_channel_close(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	snd_pcm_plug_clear(pcm, channel);
	if (plug->close_slave)
		return snd_pcm_channel_close(plug->slave, channel);
	return 0;
}

static int snd_pcm_plug_channel_nonblock(snd_pcm_t *pcm, int channel, int nonblock)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_channel_nonblock(plug->slave, channel, nonblock);
}

static int snd_pcm_plug_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_info(plug->slave, info);
}

static int snd_pcm_plug_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	struct snd_pcm_chan *chan;
	
	if ((err = snd_pcm_channel_info(plug->slave, info)) < 0)
		return err;
	info->formats = snd_pcm_plug_formats(info->formats);
	info->min_rate = 4000;
	info->max_rate = 192000;
	info->min_voices = 1;
	info->max_voices = 32;
	info->rates = SND_PCM_RATE_CONTINUOUS | SND_PCM_RATE_8000_192000;
	info->flags |= SND_PCM_CHNINFO_INTERLEAVE | SND_PCM_CHNINFO_NONINTERLEAVE;

	chan = &pcm->chan[info->channel];
	if (pcm->chan[info->channel].valid_setup) {
		info->buffer_size = snd_pcm_plug_client_size(pcm, info->channel, info->buffer_size);
		info->min_fragment_size = snd_pcm_plug_client_size(pcm, info->channel, info->min_fragment_size);
		info->max_fragment_size = snd_pcm_plug_client_size(pcm, info->channel, info->max_fragment_size);
		info->fragment_align = snd_pcm_plug_client_size(pcm, info->channel, info->fragment_align);
		info->fifo_size = snd_pcm_plug_client_size(pcm, info->channel, info->fifo_size);
		info->transfer_block_size = snd_pcm_plug_client_size(pcm, info->channel, info->transfer_block_size);
		if (chan->setup.mode == SND_PCM_MODE_BLOCK)
			info->mmap_size = chan->setup.buffer_size;
		else
			info->mmap_size = snd_pcm_plug_client_size(pcm, info->channel, info->mmap_size);
	}
	if (!snd_pcm_plug_direct(pcm, info->channel))
		info->flags &= ~(SND_PCM_CHNINFO_MMAP | SND_PCM_CHNINFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_plug_action(snd_pcm_t *pcm, int channel, int action,
			       unsigned long data)
{
	snd_pcm_plugin_t *plugin;
	int err;
	snd_pcm_plug_t *plug;
	struct snd_pcm_plug_chan *plugchan;
	plug = (snd_pcm_plug_t*) &pcm->private;
	plugchan = &plug->chan[channel];

	plugin = plugchan->first;
	while (plugin) {
		if (plugin->action) {
			if ((err = plugin->action(plugin, action, data))<0)
				return err;
		}
		plugin = plugin->next;
	}
	return 0;
}

static int snd_pcm_plug_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	snd_pcm_channel_params_t slave_params, params1;
	snd_pcm_channel_info_t slave_info;
	snd_pcm_plugin_t *plugin;
	snd_pcm_plug_t *plug;
	int err;
	int channel = params->channel;
	
	plug = (snd_pcm_plug_t*) &pcm->private;

	/*
	 *  try to decide, if a conversion is required
         */

	memset(&slave_info, 0, sizeof(slave_info));
	slave_info.channel = channel;
	if ((err = snd_pcm_channel_info(plug->slave, &slave_info)) < 0) {
		snd_pcm_plug_clear(pcm, channel);
		return err;
	}

	if ((err = snd_pcm_plug_slave_params(params, &slave_info, &slave_params)) < 0)
		return err;


	snd_pcm_plug_clear(pcm, channel);

	/* add necessary plugins */
	memcpy(&params1, params, sizeof(*params));
	if ((err = snd_pcm_plug_format(pcm, &params1, &slave_params)) < 0)
		return err;

	if (snd_pcm_plug_direct(pcm, channel))
		return snd_pcm_channel_params(plug->slave, params);

	/*
	 *  I/O plugins
	 */

	if (slave_info.flags & SND_PCM_CHNINFO_MMAP) {
		pdprintf("params mmap plugin\n");
		err = snd_pcm_plugin_build_mmap(pcm, channel, plug->slave, &slave_params.format, &plugin);
	} else {
		pdprintf("params I/O plugin\n");
		err = snd_pcm_plugin_build_io(pcm, channel, plug->slave, &slave_params.format, &plugin);
	}
	if (err < 0)
		return err;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		err = snd_pcm_plugin_append(plugin);
	} else {
		err = snd_pcm_plugin_insert(plugin);
	}
	if (err < 0) {
		snd_pcm_plugin_free(plugin);
		return err;
	}

	/* compute right sizes */
	slave_params.buffer_size = snd_pcm_plug_slave_size(pcm, channel, slave_params.buffer_size);
	slave_params.frag_size = snd_pcm_plug_slave_size(pcm, channel, slave_params.frag_size);
	slave_params.bytes_fill_max = snd_pcm_plug_slave_size(pcm, channel, slave_params.bytes_fill_max);
	slave_params.bytes_min = snd_pcm_plug_slave_size(pcm, channel, slave_params.bytes_min);
	slave_params.bytes_xrun_max = snd_pcm_plug_slave_size(pcm, channel, slave_params.bytes_xrun_max);
	slave_params.bytes_align = snd_pcm_plug_slave_size(pcm, channel, slave_params.bytes_align);

	pdprintf("params requested params: format = %i, rate = %i, voices = %i\n", slave_params.format.format, slave_params.format.rate, slave_params.format.voices);
	err = snd_pcm_channel_params(plug->slave, &slave_params);
	if (err < 0)
		return err;

	err = snd_pcm_plug_action(pcm, channel, INIT, 0);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_plug_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	struct snd_pcm_plug_chan *plugchan;

	err = snd_pcm_channel_setup(plug->slave, setup);
	if (err < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, setup->channel))
		return 0;
	setup->byte_boundary /= setup->frag_size;
	setup->frag_size = snd_pcm_plug_client_size(pcm, setup->channel, setup->frag_size);
	setup->byte_boundary *= setup->frag_size;
	setup->buffer_size = setup->frags * setup->frag_size;
	setup->bytes_min = snd_pcm_plug_client_size(pcm, setup->channel, setup->bytes_min);
	setup->bytes_align = snd_pcm_plug_client_size(pcm, setup->channel, setup->bytes_align);
	setup->bytes_xrun_max = snd_pcm_plug_client_size(pcm, setup->channel, setup->bytes_xrun_max);
	setup->bytes_fill_max = snd_pcm_plug_client_size(pcm, setup->channel, setup->bytes_fill_max);

	plugchan = &plug->chan[setup->channel];
	if (setup->channel == SND_PCM_CHANNEL_PLAYBACK)
		setup->format = plugchan->first->src_format;
	else
		setup->format = plugchan->last->dst_format;
	return 0;	
}

static int snd_pcm_plug_channel_status(snd_pcm_t *pcm, snd_pcm_channel_status_t *status)
{
	int err;
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;

	err = snd_pcm_channel_status(plug->slave, status);
	if (err < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, status->channel))
		return 0;

	/* FIXME: may overflow */
	status->byte_io = snd_pcm_plug_client_size(pcm, status->channel, status->byte_io);
	status->byte_data = snd_pcm_plug_client_size(pcm, status->channel, status->byte_data);
	status->bytes_used = snd_pcm_plug_client_size(pcm, status->channel, status->bytes_used);
	return 0;	
}

static int snd_pcm_plug_channel_update(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;
	err = snd_pcm_channel_update(plug->slave, channel);
	if (err < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, channel))
		return 0;
#if 0
	/* To think more about that */
	if ((err = snd_pcm_plug_action(pcm, channel, UPDATE, 0))<0)
		return err;
#endif
	return 0;
}

static int snd_pcm_plug_channel_prepare(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;
	err = snd_pcm_channel_prepare(plug->slave, channel);
	if (err < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, channel))
		return 0;
	if ((err = snd_pcm_plug_action(pcm, channel, PREPARE, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_channel_go(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_channel_go(plug->slave, channel);
}

static int snd_pcm_plug_sync_go(snd_pcm_t *pcm, snd_pcm_sync_t *sync)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_sync_go(plug->slave, sync);
}

static int snd_pcm_plug_channel_drain(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;

	if ((err = snd_pcm_channel_drain(plug->slave, channel)) < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, channel))
		return 0;
	if ((err = snd_pcm_plug_action(pcm, channel, DRAIN, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_channel_flush(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;

	if ((err = snd_pcm_channel_flush(plug->slave, channel)) < 0)
		return err;
	if (snd_pcm_plug_direct(pcm, channel))
		return 0;
	if ((err = snd_pcm_plug_action(pcm, channel, FLUSH, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_channel_pause(snd_pcm_t *pcm, int channel, int enable)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int err;
	
	if ((err = snd_pcm_channel_pause(plug->slave, channel, enable)) < 0)
		return err;
	if ((err = snd_pcm_plug_action(pcm, channel, PAUSE, 0))<0)
		return err;
	return 0;
}

static int snd_pcm_plug_voice_setup(snd_pcm_t *pcm, int channel, snd_pcm_voice_setup_t *setup)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	struct snd_pcm_chan *chan;
	unsigned int voice;
	int width;
	size_t size;

	if (snd_pcm_plug_direct(pcm, channel))
		return snd_pcm_voice_setup(plug->slave, channel, setup);

        voice = setup->voice;
        memset(setup, 0, sizeof(*setup));
        setup->voice = voice;
	chan = &pcm->chan[channel];
	if (!chan->mmap_data) {
		setup->area.addr = 0;
		return 0;
	}
	if (voice >= chan->setup.format.voices)
		return -EINVAL;

	width = snd_pcm_format_physical_width(chan->setup.format.format);
        if (width < 0)
                return width;
	size = chan->mmap_data_size;
	if (chan->setup.format.interleave) {
                setup->area.addr = chan->mmap_data;
                setup->area.first = chan->sample_width;
                setup->area.step = chan->bits_per_sample;
        } else {
                size /= chan->setup.format.voices;
                setup->area.addr = chan->mmap_data + setup->voice * size;
                setup->area.first = 0;
                setup->area.step = width;
	}
	return 0;
}

ssize_t snd_pcm_plug_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	unsigned int k, step, voices;
	int size = 0;
	if (snd_pcm_plug_direct(pcm, SND_PCM_CHANNEL_PLAYBACK))
		return snd_pcm_writev(plug->slave, vector, count);
	voices = chan->setup.format.voices;
	if (chan->setup.format.interleave)
		step = 1;
	else {
		step = voices;
		if (count % voices != 0)
			return -EINVAL;
	}
	for (k = 0; k < count; k += step, vector += step) {
		snd_pcm_plugin_voice_t *voices;
		int expected, ret;
		expected = snd_pcm_plug_client_voices_iovec(pcm, SND_PCM_CHANNEL_PLAYBACK, vector, count, &voices);
		if (expected < 0)
			return expected;
		ret = snd_pcm_plug_write_transfer(pcm, voices, expected);
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
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	unsigned int k, step, voices;
	int size = 0;
	if (snd_pcm_plug_direct(pcm, SND_PCM_CHANNEL_CAPTURE))
		return snd_pcm_readv(plug->slave, vector, count);
	voices = chan->setup.format.voices;
	if (chan->setup.format.interleave)
		step = 1;
	else {
		step = voices;
		if (count % voices != 0)
			return -EINVAL;
	}
	for (k = 0; k < count; k += step) {
		snd_pcm_plugin_voice_t *voices;
		int expected, ret;
		expected = snd_pcm_plug_client_voices_iovec(pcm, SND_PCM_CHANNEL_CAPTURE, vector, count, &voices);
		if (expected < 0)
			return expected;
		ret = snd_pcm_plug_read_transfer(pcm, voices, expected);
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
	snd_pcm_plugin_voice_t *voices;

	if (snd_pcm_plug_direct(pcm, SND_PCM_CHANNEL_PLAYBACK))
		return snd_pcm_write(plug->slave, buf, count);
	expected = snd_pcm_plug_client_voices_buf(pcm, SND_PCM_CHANNEL_PLAYBACK, (char *)buf, count, &voices);
	if (expected < 0)
		return expected;
	 return snd_pcm_plug_write_transfer(pcm, voices, expected);
}

ssize_t snd_pcm_plug_read(snd_pcm_t *pcm, void *buf, size_t count)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	int expected;
	snd_pcm_plugin_voice_t *voices;

	if (snd_pcm_plug_direct(pcm, SND_PCM_CHANNEL_CAPTURE))
		return snd_pcm_read(plug->slave, buf, count);
	expected = snd_pcm_plug_client_voices_buf(pcm, SND_PCM_CHANNEL_CAPTURE, buf, count, &voices);
	if (expected < 0)
		return expected;
	return snd_pcm_plug_read_transfer(pcm, voices, expected);
}

static int snd_pcm_plug_mmap_control(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t **control, size_t csize UNUSED)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, channel))
		return snd_pcm_mmap_control(plug->slave, channel, control);
	return -EBADFD;
}

static int snd_pcm_plug_mmap_data(snd_pcm_t *pcm, int channel, void **buffer, size_t bsize UNUSED)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, channel))
		return snd_pcm_mmap_data(plug->slave, channel, buffer);
	return -EBADFD;
}

static int snd_pcm_plug_munmap_control(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t *control UNUSED, size_t csize UNUSED)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, channel))
		return snd_pcm_munmap_control(plug->slave, channel);
	return -EBADFD;
}
		
static int snd_pcm_plug_munmap_data(snd_pcm_t *pcm, int channel, void *buffer UNUSED, size_t size UNUSED)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, channel))
		return snd_pcm_munmap_data(plug->slave, channel);
	return -EBADFD;
}
		
static int snd_pcm_plug_voices_mask(snd_pcm_t *pcm, int channel,
				    bitset_t *client_vmask)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	if (snd_pcm_plug_direct(pcm, channel))
		return snd_pcm_voices_mask(plug->slave, channel, client_vmask);
	if (channel == SND_PCM_CHANNEL_PLAYBACK)
		return snd_pcm_plug_playback_voices_mask(pcm, client_vmask);
	else
		return snd_pcm_plug_capture_voices_mask(pcm, client_vmask);
}

int snd_pcm_plug_file_descriptor(snd_pcm_t* pcm, int channel)
{
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &pcm->private;
	return snd_pcm_file_descriptor(plug->slave, channel);
}

struct snd_pcm_ops snd_pcm_plug_ops = {
	channel_close: snd_pcm_plug_channel_close,
	channel_nonblock: snd_pcm_plug_channel_nonblock,
	info: snd_pcm_plug_info,
	channel_info: snd_pcm_plug_channel_info,
	channel_params: snd_pcm_plug_channel_params,
	channel_setup: snd_pcm_plug_channel_setup,
	voice_setup: snd_pcm_plug_voice_setup,
	channel_status: snd_pcm_plug_channel_status,
	channel_update: snd_pcm_plug_channel_update,
	channel_prepare: snd_pcm_plug_channel_prepare,
	channel_go: snd_pcm_plug_channel_go,
	sync_go: snd_pcm_plug_sync_go,
	channel_drain: snd_pcm_plug_channel_drain,
	channel_flush: snd_pcm_plug_channel_flush,
	channel_pause: snd_pcm_plug_channel_pause,
	write: snd_pcm_plug_write,
	writev: snd_pcm_plug_writev,
	read: snd_pcm_plug_read,
	readv: snd_pcm_plug_readv,
	mmap_control: snd_pcm_plug_mmap_control,
	mmap_data: snd_pcm_plug_mmap_data,
	munmap_control: snd_pcm_plug_munmap_control,
	munmap_data: snd_pcm_plug_munmap_data,
	file_descriptor: snd_pcm_plug_file_descriptor,
	voices_mask: snd_pcm_plug_voices_mask,
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


