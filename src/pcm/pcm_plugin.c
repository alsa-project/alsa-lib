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

ssize_t snd_pcm_plugin_transfer_size(snd_pcm_t *pcm, int channel, size_t drv_size)
{
	snd_pcm_plugin_t *plugin, *plugin_prev, *plugin_next;
	
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	if (drv_size == 0)
		return 0;
	if (drv_size < 0)
		return -EINVAL;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plugin_last(pcm, channel);
		while (plugin) {
			plugin_prev = plugin->prev;
			if (plugin->src_size)
				drv_size = plugin->src_size(plugin, drv_size);
			plugin = plugin_prev;
		}
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plugin_first(pcm, channel);
		while (plugin) {
			plugin_next = plugin->next;
			if (plugin->dst_size)
				drv_size = plugin->dst_size(plugin, drv_size);
			plugin = plugin_next;
		}
	}
	return drv_size;
}

ssize_t snd_pcm_plugin_hardware_size(snd_pcm_t *pcm, int channel, size_t trf_size)
{
	snd_pcm_plugin_t *plugin, *plugin_prev, *plugin_next;
	
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	if (trf_size == 0)
		return 0;
	if (trf_size < 0)
		return -EINVAL;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plugin_first(pcm, channel);
		while (plugin) {
			plugin_next = plugin->next;
			if (plugin->dst_size)
				trf_size = plugin->dst_size(plugin, trf_size);
			plugin = plugin_next;
		}
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plugin_last(pcm, channel);
		while (plugin) {
			plugin_prev = plugin->prev;
			if (plugin->src_size)
				trf_size = plugin->src_size(plugin, trf_size);
			plugin = plugin_prev;
		}
	} 
	return trf_size;
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

static unsigned int snd_pcm_plugin_formats(snd_pcm_t *pcm, unsigned int formats)
{
	formats |= SND_PCM_FMT_MU_LAW;
	if (formats & (SND_PCM_FMT_U8|SND_PCM_FMT_S8|
		       SND_PCM_FMT_U16_LE|SND_PCM_FMT_S16_LE))
		formats |= SND_PCM_FMT_U8|SND_PCM_FMT_S8|
			   SND_PCM_FMT_U16_LE|SND_PCM_FMT_S16_LE;
	return formats;
}

int snd_pcm_plugin_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	int err;
	
	if ((err = snd_pcm_channel_info(pcm, info)) < 0)
		return err;
	info->formats = snd_pcm_plugin_formats(pcm, info->formats);
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

static void swap_formats(int channel, int *src, int *dst)
{
	int tmp;

	if (channel == SND_PCM_CHANNEL_PLAYBACK)
		return;
	tmp = *src;
	*src = *dst;
	*dst = tmp;	
}

#define CONVERT_RATIO(dest, ratio) dest = (int)((double)dest * ratio)

int snd_pcm_plugin_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	double ratio;
	snd_pcm_channel_params_t sparams;
	snd_pcm_channel_info_t sinfo;
	snd_pcm_plugin_t *plugin;
	int err, srcfmt, dstfmt;
	
	if (!pcm || !params || params->channel < 0 || params->channel > 1)
		return -EINVAL;
	memcpy(&sparams, params, sizeof(sparams));

	/*
	 *  try to decide, if a conversion is required
         */

	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.channel = params->channel;
	if ((err = snd_pcm_channel_info(pcm, &sinfo)) < 0) {
		snd_pcm_plugin_clear(pcm, params->channel);
		return err;
	}
	if ((sinfo.formats & (1 << sparams.format.format)) == 0) {
		if ((snd_pcm_plugin_formats(pcm, sinfo.formats) & (1 << sparams.format.format)) == 0)
			return -EINVAL;
		switch (sparams.format.format) {
		case SND_PCM_SFMT_U8:
			if (sinfo.formats & SND_PCM_FMT_S8) {
				sparams.format.format = SND_PCM_SFMT_S8;
			} else if (sinfo.formats & SND_PCM_FMT_U16_LE) {
				sparams.format.format = SND_PCM_SFMT_U16_LE;
			} else if (sinfo.formats & SND_PCM_FMT_S16_LE) {
				sparams.format.format = SND_PCM_SFMT_S16_LE;
			} else {
				return -EINVAL;
			}
			break;
		case SND_PCM_SFMT_S8:
			if (sinfo.formats & SND_PCM_FMT_U8) {
				sparams.format.format = SND_PCM_SFMT_U8;
			} else if (sinfo.formats & SND_PCM_FMT_S16_LE) {
				sparams.format.format = SND_PCM_SFMT_S16_LE;
			} else if (sinfo.formats & SND_PCM_FMT_U16_LE) {
				sparams.format.format = SND_PCM_SFMT_U16_LE;
			} else {
				return -EINVAL;
			}
			break;
		case SND_PCM_SFMT_S16_LE:
			if (sinfo.formats & SND_PCM_FMT_U16_LE) {
				sparams.format.format = SND_PCM_SFMT_U16_LE;
			} else if (sinfo.formats & SND_PCM_FMT_S8) {
				sparams.format.format = SND_PCM_SFMT_S8;
			} else if (sinfo.formats & SND_PCM_FMT_U8) {
				sparams.format.format = SND_PCM_SFMT_U8;
			} else {
				return -EINVAL;
			}
			break;
		case SND_PCM_SFMT_U16_LE:
			if (sinfo.formats & SND_PCM_FMT_S16_LE) {
				sparams.format.format = SND_PCM_SFMT_S16_LE;
			} else if (sinfo.formats & SND_PCM_FMT_U8) {
				sparams.format.format = SND_PCM_SFMT_U8;
			} else if (sinfo.formats & SND_PCM_FMT_S8) {
				sparams.format.format = SND_PCM_SFMT_S8;
			} else {
				return -EINVAL;
			}
			break;
		case SND_PCM_SFMT_MU_LAW:
			if (sinfo.formats & SND_PCM_FMT_S16_LE) {
				sparams.format.format = SND_PCM_SFMT_S16_LE;
			} else if (sinfo.formats & SND_PCM_FMT_U16_LE) {
				sparams.format.format = SND_PCM_SFMT_U16_LE;
			} else if (sinfo.formats & SND_PCM_FMT_S8) {
				sparams.format.format = SND_PCM_SFMT_S8;
			} else if (sinfo.formats & SND_PCM_FMT_U8) {
				sparams.format.format = SND_PCM_SFMT_U8;
			} else {
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	/*
	 *  add necessary plugins
	 */

	snd_pcm_plugin_clear(pcm, params->channel);

      	if (sparams.format.voices < sinfo.min_voices ||
      	    sparams.format.voices > sinfo.max_voices) {
		int src_voices = sparams.format.voices;
		int dst_voices = sparams.format.voices < sinfo.min_voices ?
				 sinfo.min_voices : sinfo.max_voices;
		if (sparams.format.rate < sinfo.min_rate ||
		    sparams.format.rate > sinfo.max_rate)
			dst_voices = 2;
		sparams.format.voices = dst_voices;
		swap_formats(params->channel, &src_voices, &dst_voices);
      		err = snd_pcm_plugin_build_voices(params->format.format, src_voices,
						  params->format.format, dst_voices,
						  &plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}      					    
		err = snd_pcm_plugin_append(pcm, params->channel, plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
      	}

      	/*
      	 *  formats
      	 */
      
	if (params->format.format == sparams.format.format)
		goto __format_skip;
	/* build additional plugins for conversion */
	switch (params->format.format) {
	case SND_PCM_SFMT_MU_LAW:
		srcfmt = SND_PCM_SFMT_MU_LAW;
		dstfmt = sparams.format.format;
		swap_formats(params->channel, &srcfmt, &dstfmt);
		err = snd_pcm_plugin_build_mulaw(srcfmt, dstfmt, &plugin);
		break;
	default:
		srcfmt = params->format.format;
		dstfmt = sparams.format.format;
		swap_formats(params->channel, &srcfmt, &dstfmt);
		err = snd_pcm_plugin_build_linear(srcfmt, dstfmt, &plugin);
	}
	if (err < 0)
		return err;
	err = snd_pcm_plugin_append(pcm, params->channel, plugin);
	if (err < 0) {
		snd_pcm_plugin_free(plugin);
		return err;
	}

      __format_skip:

	/*
	 *  rate
	 */

        if (sparams.format.rate < sinfo.min_rate ||
            sparams.format.rate > sinfo.max_rate) {
        	int src_rate = sparams.format.rate;
        	int dst_rate = sparams.format.rate < sinfo.min_rate ?
        		       sinfo.min_rate : sinfo.max_rate;
		sparams.format.rate = dst_rate;
		swap_formats(params->channel, &src_rate, &dst_rate);
        	err = snd_pcm_plugin_build_rate(sparams.format.format, src_rate, sparams.format.voices,
        					sparams.format.format, dst_rate, sparams.format.voices,
        					&plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}      					    
		err = snd_pcm_plugin_append(pcm, params->channel, plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
        }
      
	/*
	 *  interleave
         */
      
	if (params->format.voices > 1 && sinfo.mode == SND_PCM_MODE_BLOCK) {
		int src_interleave, dst_interleave;

		if (params->format.interleave) {
			src_interleave = dst_interleave = 1;
			if (!(sinfo.flags & SND_PCM_CHNINFO_INTERLEAVE))
				dst_interleave = 0;
		} else {
			src_interleave = dst_interleave = 0;
			if (!(sinfo.flags & SND_PCM_CHNINFO_NONINTERLEAVE))
				dst_interleave = 1;
		}
		if (src_interleave != dst_interleave) {
			sparams.format.interleave = dst_interleave;
			swap_formats(params->channel, &src_interleave, &dst_interleave);
			err = snd_pcm_plugin_build_interleave(src_interleave, dst_interleave, sparams.format.format, &plugin);
			if (err < 0)
				return err;
			err = snd_pcm_plugin_append(pcm, params->channel, plugin);
			if (err < 0) {
				snd_pcm_plugin_free(plugin);
				return err;
			}
		}
	}

	/*
	 *  I/O plugins
	 */

	if (sinfo.mode == SND_PCM_MODE_STREAM) {
		err = snd_pcm_plugin_build_stream(pcm, params->channel, &plugin);
	} else if (sinfo.mode == SND_PCM_MODE_BLOCK) {
		if (sinfo.flags & SND_PCM_CHNINFO_MMAP) {
			err = snd_pcm_plugin_build_mmap(pcm, params->channel, &plugin);
		} else {
			err = snd_pcm_plugin_build_block(pcm, params->channel, &plugin);
		}
	} else {
		return -EINVAL;
	}
	if (err < 0)
		return err;
	err = snd_pcm_plugin_append(pcm, params->channel, plugin);
	if (err < 0) {
		snd_pcm_plugin_free(plugin);
		return err;
	}

	/*
	 *  ratio
	 */

	ratio = snd_pcm_plugin_hardware_ratio(pcm, params->channel);
	if (ratio <= 0)
		return -EINVAL;
	if (params->mode == SND_PCM_MODE_STREAM) {
		CONVERT_RATIO(sparams.buf.stream.queue_size, ratio);
		CONVERT_RATIO(sparams.buf.stream.max_fill, ratio);
	} else if (params->mode == SND_PCM_MODE_BLOCK) {
		CONVERT_RATIO(sparams.buf.block.frag_size, ratio);
	} else {
		return -EINVAL;
	}
	err = snd_pcm_channel_params(pcm, &sparams);
	if (err < 0)
		return err;
	err = snd_pcm_plugin_action(pcm, sparams.channel, INIT);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_plugin_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	double ratio;
	int err;
	
	if (!pcm || !setup || setup->channel < 0 || setup->channel > 1)
		return -EINVAL;
	err = snd_pcm_channel_setup(pcm, setup);
	if (err < 0)
		return err;
	ratio = snd_pcm_plugin_transfer_ratio(pcm, setup->channel);
	if (ratio <= 0)
		return -EINVAL;
	if (setup->mode == SND_PCM_MODE_STREAM) {
		CONVERT_RATIO(setup->buf.stream.queue_size, ratio);
	} else if (setup->mode == SND_PCM_MODE_BLOCK) {
		CONVERT_RATIO(setup->buf.block.frag_size, ratio);
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
	CONVERT_RATIO(status->scount, ratio);
	CONVERT_RATIO(status->count, ratio);
	CONVERT_RATIO(status->free, ratio);
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
		if ((size = plugin->transfer(plugin, src_ptr, src_size,
						     dst_ptr, dst_size))<0) {
			result = size;
			goto __free;
		}
		if (src_ptr1)
			snd_pcm_plugin_alloc_unlock(pcm, src_ptr1);
		plugin = plugin->next;
		src_ptr = dst_ptr;
		src_ptr1 = dst_ptr1;
		dst_ptr1 = NULL;
		src_size = dst_size = size;
	}
	result = snd_pcm_plugin_transfer_size(pcm, SND_PCM_CHANNEL_PLAYBACK, size);
      __free:
      	if (dst_ptr1)
      		snd_pcm_plugin_alloc_unlock(pcm, dst_ptr1);
      	if (src_ptr1)
      		snd_pcm_plugin_alloc_unlock(pcm, src_ptr1);
	return result;
}

ssize_t snd_pcm_plugin_read(snd_pcm_t *pcm, void *buffer, size_t count)
{
	snd_pcm_plugin_t *plugin, *prev;
	char *dst_ptr, *dst_ptr1 = NULL, *src_ptr, *src_ptr1 = NULL;
	size_t dst_size, src_size;
	ssize_t size = 0, result = 0;
	int err;

	if ((plugin = snd_pcm_plugin_last(pcm, SND_PCM_CHANNEL_CAPTURE)) == NULL)
		return snd_pcm_read(pcm, buffer, count);
	src_ptr = NULL;
	src_size = 0;
	dst_size = snd_pcm_plugin_hardware_size(pcm, SND_PCM_CHANNEL_CAPTURE, count);
	if (dst_size < 0)
		return dst_size;
	while (plugin) {
		prev = plugin->prev;
		if (plugin->dst_size) {
			dst_size = plugin->dst_size(plugin, dst_size);
			if (dst_size < 0) {
				result = dst_size;
				goto __free;
			}
		}
		if (prev != NULL) {
			if (prev->transfer_src_ptr) {
				if ((err = prev->transfer_src_ptr(prev, &dst_ptr, &dst_size)) < 0) {
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
		if ((size = plugin->transfer(plugin, src_ptr, src_size,
						     dst_ptr, dst_size))<0) {
			result = size;
			goto __free;
		}
		if (dst_ptr1)
			snd_pcm_plugin_alloc_unlock(pcm, dst_ptr1);
		plugin = plugin->prev;
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
