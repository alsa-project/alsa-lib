/*
 *  PCM Plug-In Interface
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
#include <math.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include "pcm_local.h"

static void *snd_pcm_plugin_buf_alloc(snd_pcm_t *pcm, int channel, size_t size);
static void snd_pcm_plugin_buf_free(snd_pcm_t *pcm, int channel, void *ptr);

snd_pcm_plugin_t *snd_pcm_plugin_build(snd_pcm_plugin_handle_t *handle,
				       const char *name,
				       snd_pcm_format_t *src_format,
				       snd_pcm_format_t *dst_format,
				       int extra)
{
	snd_pcm_plugin_t *plugin;
	int voices = 0;
	
	if (extra < 0)
		return NULL;
	if (src_format)
		voices = src_format->voices;
	if (dst_format && dst_format->voices > voices)
		voices = dst_format->voices;
	plugin = (snd_pcm_plugin_t *)calloc(1, sizeof(*plugin) + voices * sizeof(snd_pcm_plugin_voice_t) + extra);
	if (plugin == NULL)
		return NULL;
	plugin->name = name ? strdup(name) : NULL;
	if (src_format) {
		memcpy(&plugin->src_format, src_format, sizeof(snd_pcm_format_t));
		if ((plugin->src_width = snd_pcm_format_physical_width(src_format->format)) < 0)
			return NULL;
	}
	if (dst_format) {
		memcpy(&plugin->dst_format, dst_format, sizeof(snd_pcm_format_t));
		if ((plugin->dst_width = snd_pcm_format_physical_width(dst_format->format)) < 0)
			return NULL;
	}
	plugin->handle = handle;
	plugin->voices = (snd_pcm_plugin_voice_t *)((char *)plugin + sizeof(*plugin));
	plugin->extra_data = (char *)plugin->voices + voices * sizeof(snd_pcm_plugin_voice_t);
	plugin->src_voices = snd_pcm_plugin_src_voices;
	plugin->dst_voices = snd_pcm_plugin_dst_voices;
	return plugin;
}

int snd_pcm_plugin_free(snd_pcm_plugin_t *plugin)
{
	if (plugin) {
		if (plugin->private_free)
			plugin->private_free(plugin, plugin->private_data);
		if (plugin->name)
			free(plugin->name);
		free(plugin);
	}
	return 0;
}

int snd_pcm_plugin_clear(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plugin_t *plugin, *plugin_next;
	struct snd_pcm_plug *plug;
	int idx;
	
	if (!pcm)
		return -EINVAL;
	plugin = pcm->chan[channel].plug.first;
	pcm->chan[channel].plug.first = NULL;
	pcm->chan[channel].plug.last = NULL;
	while (plugin) {
		plugin_next = plugin->next;
		snd_pcm_plugin_free(plugin);
		plugin = plugin_next;
	}
	plug = &pcm->chan[channel].plug;
	for (idx = 0; idx < 2; idx++) {
		if (plug->alloc_ptr[idx])
			free(plug->alloc_ptr[idx]);
		plug->alloc_ptr[idx] = 0;
		plug->alloc_size[idx] = 0;
		plug->alloc_lock[idx] = 0;
	}
	return 0;
}

int snd_pcm_plugin_insert(snd_pcm_t *pcm, int channel, snd_pcm_plugin_t *plugin)
{
	if (!pcm || channel < 0 || channel > 1 || !plugin)
		return -EINVAL;
	plugin->next = pcm->chan[channel].plug.first;
	plugin->prev = NULL;
	if (pcm->chan[channel].plug.first) {
		pcm->chan[channel].plug.first->prev = plugin;
		pcm->chan[channel].plug.first = plugin;
	} else {
		pcm->chan[channel].plug.last =
		pcm->chan[channel].plug.first = plugin;
	}
	return 0;
}

int snd_pcm_plugin_append(snd_pcm_t *pcm, int channel, snd_pcm_plugin_t *plugin)
{
	if (!pcm || channel < 0 || channel > 1 || !plugin)
		return -EINVAL;
	plugin->next = NULL;
	plugin->prev = pcm->chan[channel].plug.last;
	if (pcm->chan[channel].plug.last) {
		pcm->chan[channel].plug.last->next = plugin;
		pcm->chan[channel].plug.last = plugin;
	} else {
		pcm->chan[channel].plug.last =
		pcm->chan[channel].plug.first = plugin;
	}
	return 0;
}

int snd_pcm_plugin_remove_to(snd_pcm_t *pcm, int channel, snd_pcm_plugin_t *plugin)
{
	snd_pcm_plugin_t *plugin1, *plugin1_prev;

	if (!pcm || channel < 0 || channel > 1 || !plugin || !plugin->prev)
		return -EINVAL;
	plugin1 = plugin;
	while (plugin1->prev)
		plugin1 = plugin1->prev;
	if (pcm->chan[channel].plug.first != plugin1)
		return -EINVAL;
	pcm->chan[channel].plug.first = plugin;
	plugin1 = plugin->prev;
	plugin->prev = NULL;
	while (plugin1) {
		plugin1_prev = plugin1->prev;
		snd_pcm_plugin_free(plugin1);
		plugin1 = plugin1_prev;
	}
	return 0;
}

int snd_pcm_plugin_remove_first(snd_pcm_t *pcm, int channel)
{
	snd_pcm_plugin_t *plugin;

	plugin = pcm->chan[channel].plug.first;
	if (plugin->next) {
		plugin = plugin->next;
	} else {
		return snd_pcm_plugin_clear(pcm, channel);
	}
	return snd_pcm_plugin_remove_to(pcm, channel, plugin);
}

snd_pcm_plugin_t *snd_pcm_plugin_first(snd_pcm_t *pcm, int channel)
{
	if (!pcm || channel < 0 || channel > 1)
		return NULL;
	return pcm->chan[channel].plug.first;
}

snd_pcm_plugin_t *snd_pcm_plugin_last(snd_pcm_t *pcm, int channel)
{
	if (!pcm || channel < 0 || channel > 1)
		return NULL;
	return pcm->chan[channel].plug.last;
}


double snd_pcm_plugin_transfer_ratio(snd_pcm_t *pcm, int channel)
{
	ssize_t transfer;

	transfer = snd_pcm_plugin_client_size(pcm, channel, 1000000);
	if (transfer < 0)
		return 0;
	return (double)transfer / (double)1000000;
}

double snd_pcm_plugin_hardware_ratio(snd_pcm_t *pcm, int channel)
{
	ssize_t hardware;

	hardware = snd_pcm_plugin_hardware_size(pcm, channel, 1000000);
	if (hardware < 0)
		return 0;
	return (double)hardware / (double)1000000;
}

/*
 *
 */

int snd_pcm_plugin_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	int err;
	
	if ((err = snd_pcm_channel_info(pcm, info)) < 0)
		return err;
	info->formats = snd_pcm_plugin_formats(info->formats);
	info->min_rate = 4000;
	info->max_rate = 192000;
	info->min_voices = 1;
	info->max_voices = 32;
	info->rates = SND_PCM_RATE_CONTINUOUS | SND_PCM_RATE_8000_192000;
	info->buffer_size = snd_pcm_plugin_client_size(pcm, info->channel, info->buffer_size);
	info->min_fragment_size = snd_pcm_plugin_client_size(pcm, info->channel, info->min_fragment_size);
	info->max_fragment_size = snd_pcm_plugin_client_size(pcm, info->channel, info->max_fragment_size);
	info->fragment_align = snd_pcm_plugin_client_size(pcm, info->channel, info->fragment_align);
	info->fifo_size = snd_pcm_plugin_client_size(pcm, info->channel, info->fifo_size);
	info->transfer_block_size = snd_pcm_plugin_client_size(pcm, info->channel, info->transfer_block_size);
	info->mmap_size = snd_pcm_plugin_client_size(pcm, info->channel, info->mmap_size);
	return 0;
}

static int snd_pcm_plugin_action(snd_pcm_t *pcm, int channel, int action,
				 unsigned long data)
{
	snd_pcm_plugin_t *plugin;
	int err;

	plugin = pcm->chan[channel].plug.first;
	while (plugin) {
		if (plugin->action) {
			if ((err = plugin->action(plugin, action, data))<0)
				return err;
		}
		plugin = plugin->next;
	}
	return 0;
}

int snd_pcm_plugin_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	snd_pcm_channel_params_t hwparams, params1;
	snd_pcm_channel_info_t hwinfo;
	snd_pcm_plugin_t *plugin;
	struct snd_pcm_plug *plug;
	int err;
	
	if (!pcm || !params || params->channel < 0 || params->channel > 1)
		return -EINVAL;

	plug = &pcm->chan[params->channel].plug;
	if (plug->mmap_data)
		return -EBADFD;

	/*
	 *  try to decide, if a conversion is required
         */

	memset(&hwinfo, 0, sizeof(hwinfo));
	hwinfo.channel = params->channel;
	if ((err = snd_pcm_channel_info(pcm, &hwinfo)) < 0) {
		snd_pcm_plugin_clear(pcm, params->channel);
		return err;
	}

	if ((err = snd_pcm_plugin_hwparams(params, &hwinfo, &hwparams)) < 0)
		return err;


	snd_pcm_plugin_clear(pcm, params->channel);

	/* add necessary plugins */
	memcpy(&params1, params, sizeof(*params));
	if ((err = snd_pcm_plugin_format(pcm, &params1, &hwparams, &hwinfo)) < 0)
		return err;

	if (snd_pcm_plugin_first(pcm, params->channel) == NULL)
		return snd_pcm_channel_params(pcm, params);

	/*
	 *  I/O plugins
	 */

	if (params->mode == SND_PCM_MODE_STREAM) {
		pdprintf("params stream plugin\n");
		err = snd_pcm_plugin_build_stream(pcm, params->channel, &hwparams.format, &plugin);
	} else if (params->mode == SND_PCM_MODE_BLOCK) {
		if (hwinfo.flags & SND_PCM_CHNINFO_MMAP) {
			pdprintf("params mmap plugin\n");
			err = snd_pcm_plugin_build_mmap(pcm, params->channel, &hwparams.format, &plugin);
		} else {
			pdprintf("params block plugin\n");
			err = snd_pcm_plugin_build_block(pcm, params->channel, &hwparams.format, &plugin);
		}
	} else {
		return -EINVAL;
	}
	if (err < 0)
		return err;
	if (params->channel == SND_PCM_CHANNEL_PLAYBACK) {
		err = snd_pcm_plugin_append(pcm, params->channel, plugin);
	} else {
		err = snd_pcm_plugin_insert(pcm, params->channel, plugin);
	}
	if (err < 0) {
		snd_pcm_plugin_free(plugin);
		return err;
	}

	/* compute right sizes */
	if (params->mode == SND_PCM_MODE_STREAM) {
		pdprintf("params queue_size = %i\n", hwparams.buf.stream.queue_size);
		hwparams.buf.stream.queue_size = snd_pcm_plugin_hardware_size(pcm, hwparams.channel, hwparams.buf.stream.queue_size);
		hwparams.buf.stream.max_fill = snd_pcm_plugin_hardware_size(pcm, hwparams.channel, hwparams.buf.stream.max_fill);
		pdprintf("params queue_size = %i\n", hwparams.buf.stream.queue_size);
	} else if (params->mode == SND_PCM_MODE_BLOCK) {
		pdprintf("params frag_size = %i\n", hwparams.buf.block.frag_size);
		hwparams.buf.block.frag_size = snd_pcm_plugin_hardware_size(pcm, hwparams.channel, hwparams.buf.block.frag_size);
		pdprintf("params frag_size = %i\n", hwparams.buf.block.frag_size);
	} else {
		return -EINVAL;
	}
	pdprintf("params requested params: format = %i, rate = %i, voices = %i\n", hwparams.format.format, hwparams.format.rate, hwparams.format.voices);
	err = snd_pcm_channel_params(pcm, &hwparams);
	if (err < 0)
		return err;

	plug->setup_is_valid = 0;
	memset(&plug->setup, 0, sizeof(snd_pcm_channel_setup_t));
	plug->setup.channel = params->channel;
	if ((err = snd_pcm_plugin_setup(pcm, &plug->setup))<0)
		return err;
	err = snd_pcm_plugin_action(pcm, hwparams.channel, INIT, (long)&hwparams);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_plugin_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	int err;
	struct snd_pcm_plug *plug;
	
	if (!pcm || !setup || setup->channel < 0 || setup->channel > 1)
		return -EINVAL;
	plug = &pcm->chan[setup->channel].plug;
	if (plug->first == NULL)
		return snd_pcm_channel_setup(pcm, setup);
	if (plug->setup_is_valid) {
		memcpy(setup, &plug->setup, sizeof(*setup));
		return 0;
	}
	err = snd_pcm_channel_setup(pcm, setup);
	if (err < 0)
		return err;
	if (setup->mode == SND_PCM_MODE_STREAM) {
		pdprintf("params setup: queue_size = %i\n", setup->buf.stream.queue_size);
		setup->buf.stream.queue_size = snd_pcm_plugin_client_size(pcm, setup->channel, setup->buf.stream.queue_size);
		pdprintf("params setup: queue_size = %i\n", setup->buf.stream.queue_size);
	} else if (setup->mode == SND_PCM_MODE_BLOCK) {
		pdprintf("params setup: frag_size = %i\n", setup->buf.block.frag_size);
		setup->buf.block.frag_size = snd_pcm_plugin_client_size(pcm, setup->channel, setup->buf.block.frag_size);
		pdprintf("params setup: frag_size = %i\n", setup->buf.block.frag_size);
	} else {
		return -EINVAL;
	}
	if (setup->channel == SND_PCM_CHANNEL_PLAYBACK)
		setup->format = plug->first->src_format;
	else
		setup->format = plug->last->dst_format;
	memcpy(&plug->setup, setup, sizeof(*setup));
	plug->setup_is_valid = 1;
	return 0;	
}

int snd_pcm_plugin_status(snd_pcm_t *pcm, snd_pcm_channel_status_t *status)
{
	int err;
	
	if (!pcm || !status || status->channel < 0 || status->channel > 1)
		return -EINVAL;
	err = snd_pcm_channel_status(pcm, status);
	if (err < 0)
		return err;
	/* FIXME: scount may overflow */
	status->scount = snd_pcm_plugin_client_size(pcm, status->channel, status->scount);
	status->count = snd_pcm_plugin_client_size(pcm, status->channel, status->count);
	status->free = snd_pcm_plugin_client_size(pcm, status->channel, status->free);
	return 0;	
}

static void mmap_clear(struct snd_pcm_plug *plug)
{
	int idx;
	snd_pcm_mmap_fragment_t *f;

	f = plug->mmap_control->fragments;
	for (idx = 0; idx < plug->setup.buf.block.frags; idx++) {
		f->data = 0;
		f->io = 0;
		f->res[0] = 0;
		f->res[1] = 0;
		f++;
	}
	plug->mmap_control->status.frag_io = 0;
	plug->mmap_control->status.block = 0;
	plug->mmap_control->status.expblock = 0;
}

static void snd_pcm_plugin_status_change(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan;
	struct snd_pcm_plug *plug;
	snd_pcm_channel_status_t status;
	int newstatus;

	chan = &pcm->chan[channel];
	plug = &chan->plug;
	if (!plug->mmap_data)
		return;
	if (chan->mmap_control) {
		newstatus = chan->mmap_control->status.status;
	} else {
		status.channel = channel;
		if (snd_pcm_channel_status(pcm, &status) < 0)
			newstatus = SND_PCM_STATUS_NOTREADY;
		else
			newstatus = status.status;
	}
	if (plug->mmap_control->status.status == SND_PCM_STATUS_RUNNING &&
	    newstatus != SND_PCM_STATUS_RUNNING) {
		mmap_clear(plug);
	}
	plug->mmap_control->status.status = newstatus;
}

int snd_pcm_plugin_prepare(snd_pcm_t *pcm, int channel)
{
	int err;

	if ((err = snd_pcm_plugin_action(pcm, channel, PREPARE, 0))<0)
		return err;
	if ((err = snd_pcm_channel_prepare(pcm, channel)) < 0)
		return err;
	snd_pcm_plugin_status_change(pcm, channel);
	return 0;
}

int snd_pcm_plugin_go(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan;
	struct snd_pcm_plug *plug;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	plug = &chan->plug;
	if (plug->first == NULL)
		return snd_pcm_channel_go(pcm, channel);
	if (plug->mmap_control) {
		if (plug->mmap_control->status.status != SND_PCM_STATUS_PREPARED)
			return -EBADFD;
		if (channel == SND_PCM_CHANNEL_PLAYBACK) {
			if (!plug->mmap_control->fragments[0].data)
				return -EIO;
		} else {
			if (plug->mmap_control->fragments[0].data)
				return -EIO;
		}
		plug->mmap_control->status.status = SND_PCM_STATUS_RUNNING;
	}
	return 0;
}

int snd_pcm_plugin_sync_go(snd_pcm_t *pcm, snd_pcm_sync_t *sync)
{
	int err;
	if (!pcm || !sync)
		return -EINVAL;
	if (snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_PLAYBACK) ||
	    snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_CAPTURE)) {
		if ((err = snd_pcm_plugin_go(pcm, SND_PCM_CHANNEL_PLAYBACK)) < 0)
			return err;
		return snd_pcm_plugin_go(pcm, SND_PCM_CHANNEL_CAPTURE);
	}
	return snd_pcm_sync_go(pcm, sync);
}

int snd_pcm_plugin_playback_drain(snd_pcm_t *pcm)
{
	int err;

	if ((err = snd_pcm_plugin_action(pcm, SND_PCM_CHANNEL_PLAYBACK, DRAIN, 0))<0)
		return err;
	if ((err = snd_pcm_playback_drain(pcm)) < 0)
		return err;
	snd_pcm_plugin_status_change(pcm, SND_PCM_CHANNEL_PLAYBACK);
	return 0;
}

int snd_pcm_plugin_flush(snd_pcm_t *pcm, int channel)
{
	int err;

	pdprintf("flush\n");
	if ((err = snd_pcm_plugin_action(pcm, channel, FLUSH, 0))<0)
		return err;
	if ((err = snd_pcm_channel_flush(pcm, channel)) < 0)
		return err;
	snd_pcm_plugin_status_change(pcm, channel);
	return 0;
}

int snd_pcm_plugin_pause(snd_pcm_t *pcm, int enable)
{
	int err;
	
	if ((err = snd_pcm_playback_pause(pcm, enable)) < 0)
		return err;
	snd_pcm_plugin_status_change(pcm, SND_PCM_CHANNEL_PLAYBACK);
	return 0;
}

ssize_t snd_pcm_plugin_transfer_size(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_plug *plug;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	plug = &pcm->chan[channel].plug;
	if (plug->first == NULL)
		return snd_pcm_plugin_transfer_size(pcm, channel);
	if (!plug->setup_is_valid)
		return -EBADFD;
	if (plug->setup.mode != SND_PCM_MODE_BLOCK)
		return -EBADFD;
	return plug->setup.buf.block.frag_size;
}

int snd_pcm_plugin_voice_setup(snd_pcm_t *pcm, int channel, snd_pcm_voice_setup_t *setup)
{
	int voice, width, size;
	struct snd_pcm_plug* plug;
	
	if (!pcm || !setup)
		return -EINVAL;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	voice = setup->voice;
	memset(setup, 0, sizeof(*setup));
	setup->voice = voice;
	if (voice < 0)
		return -EINVAL;
	plug = &pcm->chan[channel].plug;
	if (plug->first == NULL)
		return snd_pcm_voice_setup(pcm, channel, setup);
	if (!plug->mmap_control) {
		setup->addr = -1;
		return 0;
	}
	if (voice >= plug->setup.format.voices)
		return -EINVAL;

	width = snd_pcm_format_physical_width(plug->setup.format.format);
        if (width < 0)
                return width;
	size = plug->mmap_size;
	if (plug->setup.format.interleave) {
                setup->addr = 0;
                setup->first = voice * width;
                setup->step = plug->setup.format.voices * width;
        } else {
                size /= plug->setup.format.voices;
                setup->addr = setup->voice * size;
                setup->first = 0;
                setup->step = width;
	}
	return 0;
}

static int snd_pcm_plugin_load_vector(snd_pcm_plugin_t *plugin,
				      snd_pcm_plugin_voice_t **voices,
				      const struct iovec *vector,
				      int count,
				      snd_pcm_format_t *format)
{
	snd_pcm_plugin_voice_t *v = plugin->voices;
	int width, cvoices, voice;

	*voices = NULL;
	if ((width = snd_pcm_format_width(format->format)) < 0)
		return width;
	cvoices = format->voices;
	if (format->interleave) {
		if (count != 1)
			return -EINVAL;
		for (voice = 0; voice < cvoices; voice++, v++) {
			v->aptr = NULL;
			if ((v->addr = vector->iov_base) == NULL)
				return -EINVAL;
			v->first = voice * width;
			v->step = cvoices * width;
		}
	} else {
		if (count != cvoices)
			return -EINVAL;
		for (voice = 0; voice < cvoices; voice++, v++) {
			v->aptr = NULL;
			v->addr = vector[voice].iov_base;
			v->first = 0;
			v->step = width;
		}		
	}
	*voices = plugin->voices;
	return 0;
}

static inline int snd_pcm_plugin_load_src_vector(snd_pcm_plugin_t *plugin,
					  snd_pcm_plugin_voice_t **voices,
					  const struct iovec *vector,
					  int count)
{
	return snd_pcm_plugin_load_vector(plugin, voices, vector, count, &plugin->src_format);
}

static inline int snd_pcm_plugin_load_dst_vector(snd_pcm_plugin_t *plugin,
					  snd_pcm_plugin_voice_t **voices,
					  const struct iovec *vector,
					  int count)
{
	return snd_pcm_plugin_load_vector(plugin, voices, vector, count, &plugin->dst_format);
}

static ssize_t snd_pcm_plugin_writev1(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	snd_pcm_plugin_t *plugin, *next;
	snd_pcm_plugin_voice_t *src_voices, *dst_voices;
	ssize_t samples;
	ssize_t size;
	int idx, err;

	plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_PLAYBACK);
	if ((err = snd_pcm_plugin_load_src_vector(plugin, &src_voices, vector, count)) < 0)
		return err;
	size = 0;
	for (idx = 0; idx < count; idx++)
		size += vector[idx].iov_len;
	samples = snd_pcm_plugin_src_size_to_samples(plugin, size);
	if (samples < 0)
		return samples;
	while (plugin) {
		if ((next = plugin->next) != NULL) {
			ssize_t samples1 = samples;
			if (plugin->dst_samples)
				samples1 = plugin->dst_samples(plugin, samples);
			if ((err = next->src_voices(next, &dst_voices, samples1)) < 0) {
				snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_PLAYBACK, src_voices->aptr);
				return err;
			}
		} else {
			dst_voices = NULL;
		}
		pdprintf("write plugin: %s, %i\n", plugin->name, samples);
		if ((samples = plugin->transfer(plugin, src_voices, dst_voices, samples)) < 0) {
			snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_PLAYBACK, src_voices->aptr);
			if (dst_voices)
				snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_PLAYBACK, dst_voices->aptr);
			return samples;
		}
		snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_PLAYBACK, src_voices->aptr);
		plugin = plugin->next;
		src_voices = dst_voices;
	}
	samples = snd_pcm_plugin_client_samples(pcm, SND_PCM_CHANNEL_PLAYBACK, samples);
	size = snd_pcm_plugin_src_samples_to_size(snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_PLAYBACK), samples);
	if (size < 0)
		return size;
	pdprintf("writev result = %i\n", size);
	return size;
}

ssize_t snd_pcm_plugin_writev(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	snd_pcm_plugin_t *plugin;
	int k, step, voices;
	int size = 0;
	if (vector == NULL)
		return -EFAULT;
	plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_PLAYBACK);
	if (plugin == NULL)
		return snd_pcm_readv(pcm, vector, count);
	voices = plugin->src_format.voices;
	if (plugin->src_format.interleave)
		step = 1;
	else {
		step = voices;
		if (count % voices != 0)
			return -EINVAL;
	}
	for (k = 0; k < count; k += step) {
		int expected = 0;
		int j;
		int ret = snd_pcm_plugin_writev1(pcm, vector, step);
		if (ret < 0) {
			if (size > 0)
				return size;
			return ret;
		}
		size += ret;
		for (j = 0; j < step; ++j) {
			expected += vector->iov_len;
			++vector;
		}
		if (ret != expected)
			return size;
	}
	return size;
}

static ssize_t snd_pcm_plugin_readv1(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	snd_pcm_plugin_t *plugin, *next;
	snd_pcm_plugin_voice_t *src_voices = NULL, *dst_voices;
	ssize_t samples;
	ssize_t size;
	int idx, err;

	plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_CAPTURE);
	size = 0;
	for (idx = 0; idx < count; idx++)
		size += vector[idx].iov_len;
	if (size < 0)
		return size;
	samples = snd_pcm_plugin_dst_size_to_samples(snd_pcm_plugin_last(pcm, SND_PCM_CHANNEL_CAPTURE), size);
	samples = snd_pcm_plugin_hardware_samples(pcm, SND_PCM_CHANNEL_CAPTURE, samples);
	while (plugin && samples > 0) {
		if ((next = plugin->next) != NULL) {
			if ((err = plugin->dst_voices(plugin, &dst_voices, samples)) < 0) {
				if (src_voices)
					snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_CAPTURE, src_voices->aptr);
				return err;
			}
		} else {
			if ((err = snd_pcm_plugin_load_dst_vector(plugin, &dst_voices, vector, count)) < 0) {
				if (src_voices)
					snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_CAPTURE, src_voices->aptr);
				return err;
			}
		}
		pdprintf("read plugin: %s, %i\n", plugin->name, samples);
		if ((samples = plugin->transfer(plugin, src_voices, dst_voices, samples)) < 0) {
			if (src_voices)
				snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_CAPTURE, src_voices->aptr);
			snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_CAPTURE, dst_voices->aptr);
			return samples;
		}
		if (src_voices)
			snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_CAPTURE, src_voices->aptr);
		plugin = plugin->next;
		src_voices = dst_voices;
	}
	snd_pcm_plugin_buf_free(pcm, SND_PCM_CHANNEL_CAPTURE, dst_voices->aptr);
	size = snd_pcm_plugin_dst_samples_to_size(snd_pcm_plugin_last(pcm, SND_PCM_CHANNEL_CAPTURE), samples);
	pdprintf("readv result = %i\n", size);
	return size;
}

ssize_t snd_pcm_plugin_readv(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	snd_pcm_plugin_t *plugin;
	int k, step, voices;
	int size = 0;
	if (vector == NULL)
		return -EFAULT;
	plugin = snd_pcm_plugin_last(pcm, SND_PCM_CHANNEL_CAPTURE);
	if (plugin == NULL)
		return snd_pcm_readv(pcm, vector, count);
	voices = plugin->dst_format.voices;
	if (plugin->dst_format.interleave)
		step = 1;
	else {
		step = voices;
		if (count % voices != 0)
			return -EINVAL;
	}
	for (k = 0; k < count; k += step) {
		int expected = 0;
		int j;
		int ret = snd_pcm_plugin_readv1(pcm, vector, step);
		if (ret < 0) {
			if (size > 0)
				return size;
			return ret;
		}
		size += ret;
		for (j = 0; j < step; ++j) {
			expected += vector->iov_len;
			++vector;
		}
		if (ret != expected)
			return size;
	}
	return size;
}

ssize_t snd_pcm_plugin_write(snd_pcm_t *pcm, const void *buffer, size_t count)
{
	snd_pcm_plugin_t *plugin;
	int voices;

	if ((plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_PLAYBACK)) == NULL)
		return snd_pcm_write(pcm, buffer, count);
	voices = plugin->src_format.voices;
	if (count % voices != 0)
		return -EINVAL;
	if (plugin->src_format.interleave) {
		struct iovec vec;
		vec.iov_base = (void *)buffer;
		vec.iov_len = count;
		return snd_pcm_plugin_writev1(pcm, &vec, 1);
	} else {
		int idx;
		int size = count / voices;
		struct iovec vec[voices];
		for (idx = 0; idx < voices; idx++) {
			vec[idx].iov_base = (char *)buffer + (size * idx);
			vec[idx].iov_len = size;
		}
		return snd_pcm_plugin_writev1(pcm, vec, voices);
	}
}

ssize_t snd_pcm_plugin_read(snd_pcm_t *pcm, void *buffer, size_t count)
{
	snd_pcm_plugin_t *plugin;
	int voices;

	if ((plugin = snd_pcm_plugin_last(pcm, SND_PCM_CHANNEL_CAPTURE)) == NULL)
		return snd_pcm_read(pcm, buffer, count);
	voices = plugin->dst_format.voices;
	if (count % voices != 0)
		return -EINVAL;
	if (plugin->dst_format.interleave) {
		struct iovec vec;
		vec.iov_base = buffer;
		vec.iov_len = count;
		return snd_pcm_plugin_readv1(pcm, &vec, 1);
	} else {
		int idx;
		int size = count / voices;
		struct iovec vec[voices];
		for (idx = 0; idx < voices; idx++) {
			vec[idx].iov_base = (char *)buffer + (size * idx);
			vec[idx].iov_len = size;
		}
		return snd_pcm_plugin_readv1(pcm, vec, voices);
	}
}

/*
 *  Plugin helpers
 */

static void *snd_pcm_plugin_buf_alloc(snd_pcm_t *pcm, int channel, size_t size)
{
	int idx;
	void *ptr;
	struct snd_pcm_plug *plug;

	if (pcm == NULL || size <= 0)
		return NULL;
	plug = &pcm->chan[channel].plug;
	for (idx = 0; idx < 2; idx++) {
		if (plug->alloc_lock[idx])
			continue;
		if (plug->alloc_ptr[idx] == NULL)
			continue;
		if (plug->alloc_size[idx] >= size) {
			plug->alloc_lock[idx] = 1;
			return plug->alloc_ptr[idx];
		}
	}
	for (idx = 0; idx < 2; idx++) {
		if (plug->alloc_lock[idx])
			continue;
		if (plug->alloc_ptr[idx] == NULL)
			continue;
		ptr = realloc(plug->alloc_ptr[idx], size);
		if (ptr == NULL)
			continue;
		plug->alloc_size[idx] = size;
		plug->alloc_lock[idx] = 1;
		return plug->alloc_ptr[idx] = ptr;
	}
	for (idx = 0; idx < 2; idx++) {
		if (plug->alloc_ptr[idx] != NULL)
			continue;
		ptr = malloc(size);
		if (ptr == NULL)
			continue;
		plug->alloc_size[idx] = size;
		plug->alloc_lock[idx] = 1;
		return plug->alloc_ptr[idx] = ptr;
	}
	return NULL;
}

static void snd_pcm_plugin_buf_free(snd_pcm_t *pcm, int channel, void *ptr)
{
	int idx;

	struct snd_pcm_plug *plug;

	if (pcm == NULL || ptr == NULL)
		return;
	plug = &pcm->chan[channel].plug;
	for (idx = 0; idx < 2; idx++) {
		if (plug->alloc_ptr[idx] == ptr) {
			plug->alloc_lock[idx] = 0;
			return;
		}
	}
}

static int snd_pcm_plugin_xvoices(snd_pcm_plugin_t *plugin,
				  int channel,
				  snd_pcm_plugin_voice_t **voices,
				  size_t samples,
				  snd_pcm_format_t *format)
{
	char *ptr;
	int width, voice;
	long size;
	snd_pcm_plugin_voice_t *v;
	
	*voices = NULL;
	if ((width = snd_pcm_format_physical_width(format->format)) < 0)
		return width;	
	size = format->voices * samples * width;
	if ((size % 8) != 0)
		return -EINVAL;
	size /= 8;
	ptr = (char *)snd_pcm_plugin_buf_alloc(plugin->handle, channel, size);
	if (ptr == NULL)
		return -ENOMEM;
	if ((size % format->voices) != 0)
		return -EINVAL;
	size /= format->voices;
	v = plugin->voices;
	for (voice = 0; voice < format->voices; voice++, v++) {
		v->aptr = ptr;
		if (format->interleave) {
			v->addr = ptr;
			v->first = voice * width;
			v->step = format->voices * width;
		} else {
			v->addr = ptr + (voice * size);
			v->first = 0;
			v->step = width;
		}
	}
	*voices = plugin->voices;
	return 0;
}

int snd_pcm_plugin_src_voices(snd_pcm_plugin_t *plugin,
			      snd_pcm_plugin_voice_t **voices,
			      size_t samples)
{
	return snd_pcm_plugin_xvoices(plugin, SND_PCM_CHANNEL_PLAYBACK, voices, samples, &plugin->src_format);
}

int snd_pcm_plugin_dst_voices(snd_pcm_plugin_t *plugin,
			      snd_pcm_plugin_voice_t **voices,
			      size_t samples)
{
	return snd_pcm_plugin_xvoices(plugin, SND_PCM_CHANNEL_CAPTURE, voices, samples, &plugin->dst_format);
}

static void *playback_mmap(void *data)
{
	snd_pcm_t *pcm = data;
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	struct snd_pcm_plug *plug = &chan->plug;
	snd_pcm_mmap_control_t *control;
	void *buffer;
	int frags;
	int frag_size, voice_size, voice_frag_size;
	int voices;
	control = plug->mmap_control;
	buffer = plug->mmap_data;
	frags = plug->setup.buf.block.frags;
	frag_size = plug->setup.buf.block.frag_size;
	voices = plug->setup.format.voices;
	voice_size = plug->mmap_size / voices;
	voice_frag_size = voice_size / frags;
	while (1) {
		int err;
		struct pollfd pfd;
		int frag = control->status.frag_io;
		if (plug->thread_stop)
			break;
		if (control->status.status != SND_PCM_STATUS_RUNNING &&
		    control->status.status != SND_PCM_STATUS_PREPARED) {
			usleep(100000);
			continue;
		}
		pfd.fd = chan->fd;
		pfd.events = POLLOUT;
		pfd.revents = 0;
		err = poll(&pfd, 1, -1);
		if (err < 0) {
			fprintf(stderr, "error on poll\n");
			continue;
		}
		if (!control->fragments[frag].data) {
			usleep(10000);
			continue;
		}
		/* NYI: status.block */
		control->fragments[frag].io = 1;
		if (plug->setup.format.interleave) {
			err = snd_pcm_plugin_write(pcm, buffer + frag * frag_size, frag_size);
		} else {
			struct iovec vector[voices];
			struct iovec *v = vector;
			int voice;
			for (voice = 0; voice < voices; ++voice) {
				v->iov_base = buffer + voice_size * voice + frag * voice_frag_size;
				v->iov_len = voice_frag_size;
				v++;
			}
			err = snd_pcm_plugin_writev(pcm, vector, voice_frag_size);
		}
		if (err <= 0) {
			control->fragments[frag].io = 0;
			snd_pcm_plugin_status_change(pcm, SND_PCM_CHANNEL_PLAYBACK);
			continue;
		}
		control->fragments[frag].io = 0;
		control->fragments[frag].data = 0;
		frag++;
		if (frag == frags)
			frag = 0;
		control->status.frag_io = frag;
	}
	return 0;
}

static void *capture_mmap(void *data)
{
	snd_pcm_t *pcm = data;
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	struct snd_pcm_plug *plug = &chan->plug;
	snd_pcm_mmap_control_t *control;
	void *buffer;
	int frag, frags;
	int frag_size, voice_size, voice_frag_size;
	int voices;
	control = plug->mmap_control;
	buffer = plug->mmap_data;
	frag = 0;
	frags = plug->setup.buf.block.frags;
	frag_size = plug->setup.buf.block.frag_size;
	voices = plug->setup.format.voices;
	voice_size = plug->mmap_size / voices;
	voice_frag_size = voice_size / frags;
	while (1) {
		int err;
		struct pollfd pfd;
		if (plug->thread_stop)
			break;
		if (control->status.status != SND_PCM_STATUS_RUNNING) {
			usleep(100000);
			continue;
		}
		pfd.fd = chan->fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		err = poll(&pfd, 1, -1);
		if (err < 0) {
			printf("error on poll\n");
			continue;
		}
		if (control->fragments[frag].data) {
			usleep(10000);
			continue;
		}
		/* NYI: status.block */
		control->status.frag_io = frag;
		control->fragments[frag].io = 1;
		if (plug->setup.format.interleave) {
			err = snd_pcm_plugin_read(pcm, buffer + frag * frag_size, frag_size);
		} else {
			struct iovec vector[voices];
			struct iovec *v = vector;
			int voice;
			for (voice = 0; voice < voices; ++voice) {
				v->iov_base = buffer + voice_size * voice + frag * voice_frag_size;
				v->iov_len = voice_frag_size;
				v++;
			}
			err = snd_pcm_plugin_readv(pcm, vector, voice_frag_size);
		}
		if (err < 0) {
			snd_pcm_plugin_status_change(pcm, SND_PCM_CHANNEL_CAPTURE);
			continue;
		}
		control->fragments[frag].io = 0;
		control->fragments[frag].data = 1;
		frag++;
		if (frag == frags)
			frag = 0;
	}
	return 0;
}

int snd_pcm_plugin_mmap(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t **control, void **data)
{
	int err;

	struct snd_pcm_plug *plug;
	if (!pcm || channel < 0 || channel > 1 || !control || !data)
		return -EINVAL;

	plug = &pcm->chan[channel].plug;
	if (plug->first == NULL)
		return snd_pcm_mmap(pcm, channel, control, data);
	if (!plug->setup_is_valid)
		return -EBADFD;
	if (plug->setup.mode != SND_PCM_MODE_BLOCK)
		return -EINVAL;

	if (plug->mmap_data) {
		*control = plug->mmap_control;
		*data = plug->mmap_data;
		return 0;
	}

	plug->mmap_control = malloc(sizeof(snd_pcm_mmap_control_t));
	plug->mmap_size = plug->setup.buf.block.frag_size * plug->setup.buf.block.frags;
	plug->mmap_data = malloc(plug->mmap_size);

	mmap_clear(plug);

	*control = plug->mmap_control;
	*data = plug->mmap_data;

	plug->thread_stop = 0;
	if (channel == SND_PCM_CHANNEL_PLAYBACK)
		err = pthread_create(&plug->thread, NULL, playback_mmap, pcm);
	else
		err = pthread_create(&plug->thread, NULL, capture_mmap, pcm);
	if (err < 0) {
		snd_pcm_plugin_munmap(pcm, channel);
		return err;
	}
	return 0;
}

int snd_pcm_plugin_munmap(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_plug *plug;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	plug = &pcm->chan[channel].plug;
	if (plug->first == NULL)
		return snd_pcm_munmap(pcm, channel);
	if (plug->mmap_data) {
		plug->thread_stop = 1;
		pthread_join(plug->thread, NULL);
		free(plug->mmap_control);
		plug->mmap_control = 0;
		free(plug->mmap_data);
		plug->mmap_data = 0;
	}
	return 0;
}
