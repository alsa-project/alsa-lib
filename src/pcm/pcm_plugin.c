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
#include "pcm_local.h"

snd_pcm_plugin_t *snd_pcm_plugin_build(const char *name, int extra)
{
	snd_pcm_plugin_t *plugin;
	
	if (extra < 0)
		return NULL;
	plugin = (snd_pcm_plugin_t *)calloc(1, sizeof(*plugin) + extra);
	if (plugin == NULL)
		return NULL;
	plugin->name = name ? strdup(name) : NULL;
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

	transfer = snd_pcm_plugin_transfer_size(pcm, channel, 1000000);
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

static int snd_pcm_plugin_action(snd_pcm_t *pcm, int channel, int action)
{
	snd_pcm_plugin_t *plugin;
	int err;

	plugin = pcm->plugin_first[channel];
	while (plugin) {
		if (plugin->action) {
			if ((err = plugin->action(plugin, action))<0)
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

	/*  add necessary plugins */
	memcpy(&params1, params, sizeof(*params));
	if ((err = snd_pcm_plugin_format(pcm, &params1, &hwparams, &hwinfo)) < 0)
		return err;

	/*
	 *  I/O plugins
	 */

	if (hwinfo.mode == SND_PCM_MODE_STREAM) {
		pdprintf("params stream plugin\n");
		err = snd_pcm_plugin_build_stream(pcm, params->channel, &plugin);
	} else if (hwinfo.mode == SND_PCM_MODE_BLOCK) {
		if (hwinfo.flags & SND_PCM_CHNINFO_MMAP) {
			pdprintf("params mmap plugin\n");
			err = snd_pcm_plugin_build_mmap(pcm, params->channel, &plugin);
		} else {
			pdprintf("params block plugin\n");
			err = snd_pcm_plugin_build_block(pcm, params->channel, &plugin);
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
	err = snd_pcm_plugin_action(pcm, hwparams.channel, INIT);
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
		setup->buf.stream.queue_size = snd_pcm_plugin_transfer_size(pcm, setup->channel, setup->buf.stream.queue_size);
		pdprintf("params setup: queue_size = %i\n", setup->buf.stream.queue_size);
	} else if (setup->mode == SND_PCM_MODE_BLOCK) {
		pdprintf("params setup: frag_size = %i\n", setup->buf.block.frag_size);
		setup->buf.block.frag_size = snd_pcm_plugin_transfer_size(pcm, setup->channel, setup->buf.block.frag_size);
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
	status->scount = snd_pcm_plugin_transfer_size(pcm, status->channel, status->scount);
	status->count = snd_pcm_plugin_transfer_size(pcm, status->channel, status->count);
	status->free = snd_pcm_plugin_transfer_size(pcm, status->channel, status->free);
	return 0;	
}

int snd_pcm_plugin_prepare(snd_pcm_t *pcm, int channel)
{
	int err;

	if ((err = snd_pcm_plugin_action(pcm, channel, PREPARE))<0)
		return err;
	return snd_pcm_channel_prepare(pcm, channel);
}

int snd_pcm_plugin_drain_playback(snd_pcm_t *pcm)
{
	int err;

	if ((err = snd_pcm_plugin_action(pcm, SND_PCM_CHANNEL_PLAYBACK, DRAIN))<0)
		return err;
	return snd_pcm_drain_playback(pcm);
}

int snd_pcm_plugin_flush(snd_pcm_t *pcm, int channel)
{
	int err;

	pdprintf("flush\n");
	if ((err = snd_pcm_plugin_action(pcm, channel, FLUSH))<0)
		return err;
	return snd_pcm_flush_channel(pcm, channel);
}

int snd_pcm_plugin_pointer(snd_pcm_t *pcm, int channel, void **ptr, size_t *size)
{
	snd_pcm_plugin_t *plugin;
	int err;

	if (!ptr || !size)
		return -EINVAL;
	*ptr = NULL;
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	plugin = pcm->plugin_first[channel];
	if (!plugin)
		return -EINVAL;
	if (plugin->transfer_src_ptr) {
		err = plugin->transfer_src_ptr(plugin, (char **)ptr, size);
		if (err >= 0)
			return 0;
	}
	if (pcm->plugin_alloc_xptr[channel]) {
		if (pcm->plugin_alloc_xsize[channel] >= *size) {
			*ptr = (char *)pcm->plugin_alloc_xptr[channel];
			return 0;
		}
		*ptr = (char *)realloc(pcm->plugin_alloc_xptr[channel], *size);
	} else {
		*ptr = (char *)malloc(*size);
		if (*ptr != NULL)
			pcm->plugin_alloc_xsize[channel] = *size;			
	}
	if (*ptr == NULL)
		return -ENOMEM;
	pcm->plugin_alloc_xptr[channel] = *ptr;
	return 0;
}

static void *snd_pcm_plugin_malloc(snd_pcm_t *pcm, long size)
{
	int idx;
	void *ptr;

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

static int snd_pcm_plugin_alloc_unlock(snd_pcm_t *pcm, void *ptr)
{
	int idx;

	for (idx = 0; idx < 4; idx++) {
		if (pcm->plugin_alloc_ptr[idx] == ptr) {
			pcm->plugin_alloc_lock[idx] = 0;
			return 0;
		}
	}
	return -ENOENT;
}

ssize_t snd_pcm_plugin_write(snd_pcm_t *pcm, const void *buffer, size_t count)
{
	snd_pcm_plugin_t *plugin, *next;
	char *dst_ptr, *dst_ptr1 = NULL, *src_ptr, *src_ptr1 = NULL;
	size_t dst_size, src_size;
	ssize_t size = 0, result = 0;
	int err;

	if ((plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_PLAYBACK)) == NULL)
		return snd_pcm_write(pcm, buffer, count);
	src_ptr = (char *)buffer;
	dst_size = src_size = count;
	while (plugin) {
		next = plugin->next;
		if (plugin->dst_size) {
			dst_size = plugin->dst_size(plugin, dst_size);
			if (dst_size < 0) {
				result = dst_size;
				goto __free;
			}
		}
		if (next != NULL) {
			if (next->transfer_src_ptr) {
				if ((err = next->transfer_src_ptr(next, &dst_ptr, &dst_size)) < 0) {
					if (dst_ptr == NULL)
						goto __alloc;
					result = err;
					goto __free;
				}
			} else {
			      __alloc:
				dst_ptr = dst_ptr1 = (char *)snd_pcm_plugin_malloc(pcm, dst_size);
				if (dst_ptr == NULL) {
					result = -ENOMEM;
					goto __free;
				}
			}
		} else {
			dst_ptr = src_ptr;
			dst_size = src_size;
		}
		pdprintf("write plugin: %s, %i, %i\n", plugin->name, src_size, dst_size);
		if ((size = plugin->transfer(plugin, src_ptr, src_size,
						     dst_ptr, dst_size))<0) {
			result = size;
			goto __free;
		}
		if (src_ptr1)
			snd_pcm_plugin_alloc_unlock(pcm, src_ptr1);
		plugin = next;
		src_ptr = dst_ptr;
		src_ptr1 = dst_ptr1;
		dst_ptr1 = NULL;
		src_size = dst_size = size;
	}
	result = snd_pcm_plugin_transfer_size(pcm, SND_PCM_CHANNEL_PLAYBACK, size);
	pdprintf("size = %i, result = %i, count = %i\n", size, result, count);
      __free:
      	if (dst_ptr1)
      		snd_pcm_plugin_alloc_unlock(pcm, dst_ptr1);
      	if (src_ptr1)
      		snd_pcm_plugin_alloc_unlock(pcm, src_ptr1);
	return result;
}

ssize_t snd_pcm_plugin_read(snd_pcm_t *pcm, void *buffer, size_t count)
{
	snd_pcm_plugin_t *plugin, *next;
	char *dst_ptr, *dst_ptr1 = NULL, *src_ptr, *src_ptr1 = NULL;
	size_t dst_size, src_size;
	ssize_t size = 0, result = 0;
	int err;

	if ((plugin = snd_pcm_plugin_first(pcm, SND_PCM_CHANNEL_CAPTURE)) == NULL)
		return snd_pcm_read(pcm, buffer, count);
	src_ptr = NULL;
	src_size = 0;
	dst_size = snd_pcm_plugin_hardware_size(pcm, SND_PCM_CHANNEL_CAPTURE, count);
	if (dst_size < 0)
		return dst_size;
	while (plugin) {
		next = plugin->next;
		if (plugin->dst_size) {
			dst_size = plugin->dst_size(plugin, dst_size);
			if (dst_size < 0) {
				result = dst_size;
				goto __free;
			}
		}
		if (next != NULL) {
			if (next->transfer_src_ptr) {
				if ((err = next->transfer_src_ptr(next, &dst_ptr, &dst_size)) < 0) {
					if (dst_ptr == NULL)
						goto __alloc;
					result = err;
					goto __free;
				}
			} else {
			      __alloc:
				dst_ptr = dst_ptr1 = (char *)snd_pcm_plugin_malloc(pcm, dst_size);
				if (dst_ptr == NULL) {
					result = -ENOMEM;
					goto __free;
				}
			}
		} else {
			dst_ptr = buffer;
		}
		pdprintf("read plugin: %s, %i, %i\n", plugin->name, src_size, dst_size);
		if ((size = plugin->transfer(plugin, src_ptr, src_size,
						     dst_ptr, dst_size))<0) {
			result = size;
			goto __free;
		}
		if (dst_ptr1)
			snd_pcm_plugin_alloc_unlock(pcm, dst_ptr1);
		plugin = plugin->next;
		src_ptr = dst_ptr;
		src_ptr1 = dst_ptr1;
		dst_ptr1 = NULL;
		src_size = dst_size = size;
	}
	result = size;
      __free:
      	if (dst_ptr1)
      		snd_pcm_plugin_alloc_unlock(pcm, dst_ptr1);
      	if (src_ptr1)
      		snd_pcm_plugin_alloc_unlock(pcm, src_ptr1);
	return result;
}
