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
#include "pcm_local.h"

static void *snd_pcm_plugin_buf_alloc(snd_pcm_t *pcm, size_t size);
static void snd_pcm_plugin_buf_free(snd_pcm_t *pcm, void *ptr);
static void *snd_pcm_plugin_ptr_alloc(snd_pcm_t *pcm, size_t size);

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
	int idx;
	
	if (!pcm)
		return -EINVAL;
	plugin = pcm->plugin_first[channel];
	pcm->plugin_first[channel] = NULL;
	pcm->plugin_last[channel] = NULL;
	while (plugin) {
		plugin_next = plugin->next;
		snd_pcm_plugin_free(plugin);
		plugin = plugin_next;
	}
	for (idx = 0; idx < 4; idx++) {
		if (pcm->plugin_alloc_ptr[idx])
			free(pcm->plugin_alloc_ptr[idx]);
		pcm->plugin_alloc_ptr[idx] = 0;
		pcm->plugin_alloc_size[idx] = 0;
		pcm->plugin_alloc_lock[idx] = 0;
	}
	if (pcm->plugin_alloc_xptr[channel])
		free(pcm->plugin_alloc_xptr[channel]);
	pcm->plugin_alloc_xptr[channel] = NULL;
	pcm->plugin_alloc_xsize[channel] = 0;
	return 0;
}

int snd_pcm_plugin_insert(snd_pcm_t *pcm, int channel, snd_pcm_plugin_t *plugin)
{
	if (!pcm || channel < 0 || channel > 1 || !plugin)
		return -EINVAL;
	plugin->next = pcm->plugin_first[channel];
	plugin->prev = NULL;
	if (pcm->plugin_first[channel]) {
		pcm->plugin_first[channel]->prev = plugin;
		pcm->plugin_first[channel] = plugin;
	} else {
		pcm->plugin_last[channel] =
		pcm->plugin_first[channel] = plugin;
	}
	return 0;
}

int snd_pcm_plugin_append(snd_pcm_t *pcm, int channel, snd_pcm_plugin_t *plugin)
{
	if (!pcm || channel < 0 || channel > 1 || !plugin)
		return -EINVAL;
	plugin->next = NULL;
	plugin->prev = pcm->plugin_last[channel];
	if (pcm->plugin_last[channel]) {
		pcm->plugin_last[channel]->next = plugin;
		pcm->plugin_last[channel] = plugin;
	} else {
		pcm->plugin_last[channel] =
		pcm->plugin_first[channel] = plugin;
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
	if (pcm->plugin_first[channel] != plugin1)
		return -EINVAL;
	pcm->plugin_first[channel] = plugin;
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

	plugin = snd_pcm_plugin_first(pcm, channel);
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
	return pcm->plugin_first[channel];
}

snd_pcm_plugin_t *snd_pcm_plugin_last(snd_pcm_t *pcm, int channel)
{
	if (!pcm || channel < 0 || channel > 1)
		return NULL;
	return pcm->plugin_last[channel];
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
	info->rates = SND_PCM_RATE_8000_48000;
	return 0;
}

static int snd_pcm_plugin_action(snd_pcm_t *pcm, int channel, int action,
				 unsigned long data)
{
	snd_pcm_plugin_t *plugin;
	int err;

	plugin = pcm->plugin_first[channel];
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
	int err;
	
	if (!pcm || !params || params->channel < 0 || params->channel > 1)
		return -EINVAL;

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
	err = snd_pcm_plugin_action(pcm, hwparams.channel, INIT, (long)&hwparams);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_plugin_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	int err;
	
	if (!pcm || !setup || setup->channel < 0 || setup->channel > 1)
		return -EINVAL;
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
	return 0;	
}

int snd_pcm_plugin_status(snd_pcm_t *pcm, snd_pcm_channel_status_t *status)
{
	double ratio;
	int err;
	
	if (!pcm || !status || status->channel < 0 || status->channel > 1)
		return -EINVAL;
	err = snd_pcm_channel_status(pcm, status);
	if (err < 0)
		return err;
	ratio = snd_pcm_plugin_transfer_ratio(pcm, status->channel);
	if (ratio <= 0)
		return -EINVAL;
	/* FIXME: scount may overflow */
	status->scount = snd_pcm_plugin_client_size(pcm, status->channel, status->scount);
	status->count = snd_pcm_plugin_client_size(pcm, status->channel, status->count);
	status->free = snd_pcm_plugin_client_size(pcm, status->channel, status->free);
	return 0;	
}

int snd_pcm_plugin_prepare(snd_pcm_t *pcm, int channel)
{
	int err;

	if ((err = snd_pcm_plugin_action(pcm, channel, PREPARE, 0))<0)
		return err;
	return snd_pcm_channel_prepare(pcm, channel);
}

int snd_pcm_plugin_playback_drain(snd_pcm_t *pcm)
{
	int err;

	if ((err = snd_pcm_plugin_action(pcm, SND_PCM_CHANNEL_PLAYBACK, DRAIN, 0))<0)
		return err;
	return snd_pcm_playback_drain(pcm);
}

int snd_pcm_plugin_flush(snd_pcm_t *pcm, int channel)
{
	int err;

	pdprintf("flush\n");
	if ((err = snd_pcm_plugin_action(pcm, channel, FLUSH, 0))<0)
		return err;
	return snd_pcm_channel_flush(pcm, channel);
}

ssize_t snd_pcm_plugin_transfer_size(snd_pcm_t *pcm, int channel)
{
	ssize_t result;

	if ((result = snd_pcm_transfer_size(pcm, channel)) < 0)
		return result;
	return snd_pcm_plugin_client_size(pcm, channel, result);
}

int snd_pcm_plugin_pointer(snd_pcm_t *pcm, int channel, void **ptr, size_t *size)
{
	snd_pcm_plugin_t *plugin = NULL;
	snd_pcm_plugin_voice_t *voices;
	size_t samples;
	int width;

	if (!ptr || !size || *size < 1)
		return -EINVAL;
	*ptr = NULL;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	if ((*size = snd_pcm_plugin_transfer_size(pcm, channel)) < 0)
		return *size;
	if (channel == SND_PCM_CHANNEL_PLAYBACK && plugin->src_voices) {
		plugin = pcm->plugin_first[channel];
		if (!plugin)
			goto __skip;
		if (!plugin->src_format.interleave)
			goto __skip;
		if ((width = snd_pcm_format_width(plugin->src_format.format)) < 0)
			return width;
		samples = *size * width;
		if ((samples % (plugin->src_format.voices * 8)) != 0)
			return -EINVAL;
		samples /= (plugin->src_format.voices * 8);
		pcm->plugin_alloc_xchannel = SND_PCM_CHANNEL_PLAYBACK;
		if (plugin->src_voices(plugin, &voices, samples,
				       snd_pcm_plugin_ptr_alloc) < 0)
			goto __skip;
		*ptr = voices->addr;
		return 0;
	} else if (channel == SND_PCM_CHANNEL_CAPTURE && plugin->dst_voices) {
		plugin = pcm->plugin_last[channel];
		if (!plugin)
			goto __skip;
		if (plugin->dst_format.interleave)
			goto __skip;
		if ((width = snd_pcm_format_width(plugin->dst_format.format)) < 0)
			return width;
		samples = *size * width;
		if ((samples % (plugin->dst_format.voices * 8)) != 0)
			return -EINVAL;
		samples /= (plugin->src_format.voices * 8);
		pcm->plugin_alloc_xchannel = SND_PCM_CHANNEL_CAPTURE;
		if (plugin->dst_voices(plugin, &voices, *size,
				       snd_pcm_plugin_ptr_alloc) < 0)
			goto __skip;
		*ptr = voices->addr;
		return 0;
	}
      __skip:
      	*ptr = snd_pcm_plugin_ptr_alloc(pcm, *size);
      	if (*ptr == NULL)
      		return -ENOMEM;
	return 0;
}

ssize_t snd_pcm_plugin_write(snd_pcm_t *pcm, const void *buffer, size_t count)
{
	snd_pcm_plugin_t *plugin;

	if ((plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_PLAYBACK)) == NULL)
		return snd_pcm_write(pcm, buffer, count);
	if (plugin->src_format.interleave) {
		struct iovec vec;
		vec.iov_base = (void *)buffer;
		vec.iov_len = count;
		return snd_pcm_plugin_writev(pcm, &vec, 1);
	} else {
		int idx, voices = plugin->src_format.voices;
		int size = count / voices;
		struct iovec vec[voices];
		for (idx = 0; idx < voices; idx++) {
			vec[idx].iov_base = (char *)buffer + (size * idx);
			vec[idx].iov_len = size;
		}
		return snd_pcm_plugin_writev(pcm, vec, voices);
	}
}

ssize_t snd_pcm_plugin_read(snd_pcm_t *pcm, void *buffer, size_t count)
{
	snd_pcm_plugin_t *plugin;

	if ((plugin = snd_pcm_plugin_last(pcm, SND_PCM_CHANNEL_CAPTURE)) == NULL)
		return snd_pcm_write(pcm, buffer, count);
	if (plugin->dst_format.interleave) {
		struct iovec vec;
		vec.iov_base = buffer;
		vec.iov_len = count;
		return snd_pcm_plugin_readv(pcm, &vec, 1);
	} else {
		int idx, voices = plugin->dst_format.voices;
		int size = count / voices;
		struct iovec vec[voices];
		for (idx = 0; idx < voices; idx++) {
			vec[idx].iov_base = (char *)buffer + (size * idx);
			vec[idx].iov_len = size;
		}
		return snd_pcm_plugin_readv(pcm, vec, voices);
	}
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
			v->offset = voice * width;
			v->next = cvoices * width;
		}
	} else {
		if (count != cvoices)
			return -EINVAL;
		for (voice = 0; voice < cvoices; voice++, v++) {
			v->aptr = NULL;
			v->addr = vector[voice].iov_base;
			v->offset = 0;
			v->next = width;
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

ssize_t snd_pcm_plugin_writev(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	snd_pcm_plugin_t *plugin, *next;
	snd_pcm_plugin_voice_t *src_voices, *dst_voices;
	size_t samples;
	ssize_t size;
	int idx, err;

	if ((plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_PLAYBACK)) == NULL)
		return snd_pcm_writev(pcm, vector, count);
	if ((err = snd_pcm_plugin_load_src_vector(plugin, &src_voices, vector, count)) < 0)
		return err;
	size = 0;
	for (idx = 0; idx < count; idx++)
		size += vector[idx].iov_len;
	size = snd_pcm_plugin_src_size_to_samples(plugin, size);
	if (size < 0)
		return size;
	samples = size;
	while (plugin) {
		if ((next = plugin->next) != NULL) {
			if (next->src_voices) {
				if ((err = next->src_voices(next, &dst_voices, samples, snd_pcm_plugin_buf_alloc)) < 0) {
					snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
					return err;
				}
			} else {
				if ((err = snd_pcm_plugin_src_voices(next, &dst_voices, samples)) < 0) {
					snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
					return err;
				}
			}
		} else {
			dst_voices = NULL;
		}
		pdprintf("write plugin: %s, %i\n", plugin->name, samples);
		if ((size = plugin->transfer(plugin, src_voices, dst_voices, samples))<0) {
			snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
			if (dst_voices)
				snd_pcm_plugin_buf_free(pcm, dst_voices->aptr);
			return size;
		}
		snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
		plugin = plugin->next;
		src_voices = dst_voices;
		samples = size;
	}
	samples = snd_pcm_plugin_client_samples(pcm, SND_PCM_CHANNEL_PLAYBACK, samples);
	size = snd_pcm_plugin_src_samples_to_size(pcm->plugin_first[SND_PCM_CHANNEL_PLAYBACK], samples);
	if (size < 0)
		return size;
	pdprintf("writev result = %i\n", size);
	return size;
}

ssize_t snd_pcm_plugin_readv(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	snd_pcm_plugin_t *plugin, *next;
	snd_pcm_plugin_voice_t *src_voices = NULL, *dst_voices;
	size_t samples;
	ssize_t size;
	int idx, err;

	if ((plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_CAPTURE)) == NULL)
		return snd_pcm_readv(pcm, vector, count);
	if (vector == NULL)
		return -EINVAL;
	size = 0;
	for (idx = 0; idx < count; idx++)
		size += vector[idx].iov_len;
	if (size < 0)
		return size;
	samples = snd_pcm_plugin_dst_size_to_samples(pcm->plugin_last[SND_PCM_CHANNEL_CAPTURE], size);
	samples = snd_pcm_plugin_hardware_samples(pcm, SND_PCM_CHANNEL_CAPTURE, samples);
	while (plugin && samples > 0) {
		if ((next = plugin->next) != NULL) {
			if (plugin->dst_voices) {
				if ((err = plugin->dst_voices(plugin, &dst_voices, samples, snd_pcm_plugin_buf_alloc)) < 0) {
					if (src_voices)
						snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
					return err;
				}
			} else {
				if ((err = snd_pcm_plugin_dst_voices(plugin, &dst_voices, samples)) < 0) {
					if (src_voices)
						snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
					return err;
				}
			}
		} else {
			if ((err = snd_pcm_plugin_load_dst_vector(plugin, &dst_voices, vector, count)) < 0) {
				if (src_voices)
					snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
				return err;
			}
		}
		pdprintf("read plugin: %s, %i\n", plugin->name, samples);
		if ((size = plugin->transfer(plugin, src_voices, dst_voices, samples))<0) {
			if (src_voices)
				snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
			snd_pcm_plugin_buf_free(pcm, dst_voices->aptr);
			return size;
		}
		if (src_voices)
			snd_pcm_plugin_buf_free(pcm, src_voices->aptr);
		plugin = plugin->next;
		src_voices = dst_voices;
		samples = size;
	}
	snd_pcm_plugin_buf_free(pcm, dst_voices->aptr);
	size = snd_pcm_plugin_dst_samples_to_size(pcm->plugin_last[SND_PCM_CHANNEL_CAPTURE], samples);
	pdprintf("readv result = %i\n", size);
	return size;
}

/*
 *  Plugin helpers
 */

static void *snd_pcm_plugin_buf_alloc(snd_pcm_t *pcm, size_t size)
{
	int idx;
	void *ptr;

	if (pcm == NULL || size <= 0)
		return NULL;
	for (idx = 0; idx < 4; idx++) {
		if (pcm->plugin_alloc_lock[idx])
			continue;
		if (pcm->plugin_alloc_ptr[idx] == NULL)
			continue;
		if (pcm->plugin_alloc_size[idx] >= size) {
			pcm->plugin_alloc_lock[idx] = 1;
			return pcm->plugin_alloc_ptr[idx];
		}
	}
	for (idx = 0; idx < 4; idx++) {
		if (pcm->plugin_alloc_lock[idx])
			continue;
		if (pcm->plugin_alloc_ptr[idx] == NULL)
			continue;
		ptr = realloc(pcm->plugin_alloc_ptr[idx], size);
		if (ptr == NULL)
			continue;
		pcm->plugin_alloc_size[idx] = size;
		pcm->plugin_alloc_lock[idx] = 1;
		return pcm->plugin_alloc_ptr[idx] = ptr;
	}
	for (idx = 0; idx < 4; idx++) {
		if (pcm->plugin_alloc_ptr[idx] != NULL)
			continue;
		ptr = malloc(size);
		if (ptr == NULL)
			continue;
		pcm->plugin_alloc_size[idx] = size;
		pcm->plugin_alloc_lock[idx] = 1;
		return pcm->plugin_alloc_ptr[idx] = ptr;
	}
	return NULL;
}

static void snd_pcm_plugin_buf_free(snd_pcm_t *pcm, void *ptr)
{
	int idx;

	if (pcm == NULL || ptr == NULL)
		return;
	for (idx = 0; idx < 4; idx++) {
		if (pcm->plugin_alloc_ptr[idx] == ptr) {
			pcm->plugin_alloc_lock[idx] = 0;
			return;
		}
	}
}

static void *snd_pcm_plugin_ptr_alloc(snd_pcm_t *pcm, size_t size)
{
	void *ptr;
	int channel = pcm->plugin_alloc_xchannel;

	if (pcm->plugin_alloc_xptr[channel]) {
		if (pcm->plugin_alloc_xsize[channel] >= size)
			return pcm->plugin_alloc_xptr[channel];
		ptr = realloc(pcm->plugin_alloc_xptr[channel], size);
	} else {
		ptr = malloc(size);
	}
	if (ptr == NULL)
		return NULL;
	pcm->plugin_alloc_xptr[channel] = (char *)ptr;
	pcm->plugin_alloc_xsize[channel] = size;			
	return ptr;
}

static int snd_pcm_plugin_xvoices(snd_pcm_plugin_t *plugin,
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
	ptr = (char *)snd_pcm_plugin_buf_alloc(plugin->handle, size);
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
			v->offset = voice * width;
			v->next = format->voices * width;
		} else {
			v->addr = ptr + (voice * size);
			v->offset = 0;
			v->next = width;
		}
	}
	*voices = plugin->voices;
	return 0;
}

int snd_pcm_plugin_src_voices(snd_pcm_plugin_t *plugin,
			      snd_pcm_plugin_voice_t **voices,
			      size_t samples)
{
	return snd_pcm_plugin_xvoices(plugin, voices, samples, &plugin->src_format);
}

int snd_pcm_plugin_dst_voices(snd_pcm_plugin_t *plugin,
			      snd_pcm_plugin_voice_t **voices,
			      size_t samples)
{
	return snd_pcm_plugin_xvoices(plugin, voices, samples, &plugin->dst_format);
}
