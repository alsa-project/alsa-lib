/*
 *  PCM Plug-In shared (kernel/library) code
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
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
  
#if 0
#define PLUGIN_DEBUG
#endif
#ifdef __KERNEL__
#include "../../include/driver.h"
#include "../../include/pcm.h"
#define snd_pcm_plug_first(handle, channel) ((handle)->runtime->oss.plugin_first)
#define snd_pcm_plug_last(handle, channel) ((handle)->runtime->oss.plugin_last)
#else
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include "pcm_local.h"
#endif

static int snd_pcm_plugin_src_voices_mask(snd_pcm_plugin_t *plugin,
					  bitset_t *dst_vmask,
					  bitset_t **src_vmask)
{
	bitset_t *vmask = plugin->src_vmask;
	bitset_copy(vmask, dst_vmask, plugin->src_format.voices);
	*src_vmask = vmask;
	return 0;
}

static int snd_pcm_plugin_dst_voices_mask(snd_pcm_plugin_t *plugin,
					  bitset_t *src_vmask,
					  bitset_t **dst_vmask)
{
	bitset_t *vmask = plugin->dst_vmask;
	bitset_copy(vmask, src_vmask, plugin->dst_format.voices);
	*dst_vmask = vmask;
	return 0;
}

static int snd_pcm_plugin_side_voices(snd_pcm_plugin_t *plugin,
				      int client_side,
				      size_t samples,
				      snd_pcm_plugin_voice_t **voices)
{
	char *ptr;
	int width;
	unsigned int voice;
	long size;
	snd_pcm_plugin_voice_t *v;
	snd_pcm_format_t *format;
	if ((plugin->channel == SND_PCM_CHANNEL_PLAYBACK && client_side) ||
	    (plugin->channel == SND_PCM_CHANNEL_CAPTURE && !client_side)) {
		format = &plugin->src_format;
		v = plugin->src_voices;
	} else {
		format = &plugin->dst_format;
		v = plugin->dst_voices;
	}

	*voices = v;
	if ((width = snd_pcm_format_physical_width(format->format)) < 0)
		return width;	
	size = format->voices * samples * width;
	if ((size % 8) != 0)
		return -EINVAL;
	size /= 8;
	ptr = (char *)snd_pcm_plug_buf_alloc(plugin->handle, plugin->channel, size);
	if (ptr == NULL)
		return -ENOMEM;
	if ((size % format->voices) != 0)
		return -EINVAL;
	size /= format->voices;
	for (voice = 0; voice < format->voices; voice++, v++) {
		v->enabled = 1;
		v->wanted = 0;
		v->aptr = ptr;
		if (format->interleave) {
			v->area.addr = ptr;
			v->area.first = voice * width;
			v->area.step = format->voices * width;
		} else {
			v->area.addr = ptr + (voice * size);
			v->area.first = 0;
			v->area.step = width;
		}
	}
	return 0;
}

int snd_pcm_plugin_client_voices(snd_pcm_plugin_t *plugin,
				 size_t samples,
				 snd_pcm_plugin_voice_t **voices)
{
	return snd_pcm_plugin_side_voices(plugin, 1, samples, voices);
}

int snd_pcm_plugin_slave_voices(snd_pcm_plugin_t *plugin,
				size_t samples,
				snd_pcm_plugin_voice_t **voices)
{
	return snd_pcm_plugin_side_voices(plugin, 0, samples, voices);
}


int snd_pcm_plugin_build(snd_pcm_plugin_handle_t *handle,
			 int channel,
			 const char *name,
			 snd_pcm_format_t *src_format,
			 snd_pcm_format_t *dst_format,
			 int extra,
			 snd_pcm_plugin_t **ret)
{
	snd_pcm_plugin_t *plugin;
	
	if (!handle)
		return -EFAULT;
	if (extra < 0)
		return -EINVAL;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!src_format || !dst_format)
		return -EFAULT;
	plugin = (snd_pcm_plugin_t *)calloc(1, sizeof(*plugin) + extra);
	if (plugin == NULL)
		return -ENOMEM;
	plugin->name = name ? strdup(name) : NULL;
	plugin->handle = handle;
	plugin->channel = channel;
	memcpy(&plugin->src_format, src_format, sizeof(snd_pcm_format_t));
	if ((plugin->src_width = snd_pcm_format_physical_width(src_format->format)) < 0)
		return -EINVAL;
	memcpy(&plugin->dst_format, dst_format, sizeof(snd_pcm_format_t));
	if ((plugin->dst_width = snd_pcm_format_physical_width(dst_format->format)) < 0)
		return -EINVAL;
	plugin->src_voices = calloc(src_format->voices, sizeof(snd_pcm_plugin_voice_t));
	if (plugin->src_voices == NULL) {
		free(plugin);
		return -ENOMEM;
	}
	plugin->dst_voices = calloc(dst_format->voices, sizeof(snd_pcm_plugin_voice_t));
	if (plugin->dst_voices == NULL) {
		free(plugin->src_voices);
		free(plugin);
		return -ENOMEM;
	}
	plugin->src_vmask = bitset_alloc(src_format->voices);
	if (plugin->src_vmask == NULL) {
		free(plugin->src_voices);
		free(plugin->dst_voices);
		free(plugin);
		return -ENOMEM;
	}
	plugin->dst_vmask = bitset_alloc(dst_format->voices);
	if (plugin->dst_vmask == NULL) {
		free(plugin->src_voices);
		free(plugin->dst_voices);
		free(plugin->src_vmask);
		free(plugin);
		return -ENOMEM;
	}
	plugin->client_voices = snd_pcm_plugin_client_voices;
	plugin->src_voices_mask = snd_pcm_plugin_src_voices_mask;
	plugin->dst_voices_mask = snd_pcm_plugin_dst_voices_mask;
	*ret = plugin;
	return 0;
}

int snd_pcm_plugin_free(snd_pcm_plugin_t *plugin)
{
	if (plugin) {
		if (plugin->private_free)
			plugin->private_free(plugin, plugin->private_data);
		if (plugin->name)
			free(plugin->name);
		free(plugin->src_voices);
		free(plugin->dst_voices);
		free(plugin->src_vmask);
		free(plugin->dst_vmask);
		free(plugin);
	}
	return 0;
}

ssize_t snd_pcm_plugin_src_samples_to_size(snd_pcm_plugin_t *plugin, size_t samples)
{
	ssize_t result;

	if (plugin == NULL)
		return -EFAULT;
	result = samples * plugin->src_format.voices * plugin->src_width;
	if (result % 8 != 0)
		return -EINVAL;
	return result / 8;
}

ssize_t snd_pcm_plugin_dst_samples_to_size(snd_pcm_plugin_t *plugin, size_t samples)
{
	ssize_t result;

	if (plugin == NULL)
		return -EFAULT;
	result = samples * plugin->dst_format.voices * plugin->dst_width;
	if (result % 8 != 0)
		return -EINVAL;
	return result / 8;
}

ssize_t snd_pcm_plugin_src_size_to_samples(snd_pcm_plugin_t *plugin, size_t size)
{
	ssize_t result;
	long tmp;

	if (plugin == NULL)
		return -EFAULT;
	result = size * 8;
	tmp = plugin->src_format.voices * plugin->src_width;
	if (result % tmp != 0)
		return -EINVAL;
	return result / tmp;
}

ssize_t snd_pcm_plugin_dst_size_to_samples(snd_pcm_plugin_t *plugin, size_t size)
{
	ssize_t result;
	long tmp;

	if (plugin == NULL)
		return -EFAULT;
	result = size * 8;
	tmp = plugin->dst_format.voices * plugin->dst_width;
	if (result % tmp != 0)
		return -EINVAL;
	return result / tmp;
}

ssize_t snd_pcm_plug_client_samples(snd_pcm_plugin_handle_t *handle, int channel, size_t drv_samples)
{
	snd_pcm_plugin_t *plugin, *plugin_prev, *plugin_next;
	
	if (handle == NULL)
		return -EFAULT;
	if (channel != SND_PCM_CHANNEL_PLAYBACK &&
	    channel != SND_PCM_CHANNEL_CAPTURE)
		return -EINVAL;
	if (drv_samples == 0)
		return 0;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_PLAYBACK);
		while (plugin && drv_samples > 0) {
			plugin_prev = plugin->prev;
			if (plugin->src_samples)
				drv_samples = plugin->src_samples(plugin, drv_samples);
			plugin = plugin_prev;
		}
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_CAPTURE);
		while (plugin && drv_samples > 0) {
			plugin_next = plugin->next;
			if (plugin->dst_samples)
				drv_samples = plugin->dst_samples(plugin, drv_samples);
			plugin = plugin_next;
		}
	}
	return drv_samples;
}

ssize_t snd_pcm_plug_slave_samples(snd_pcm_plugin_handle_t *handle, int channel, size_t clt_samples)
{
	snd_pcm_plugin_t *plugin, *plugin_prev, *plugin_next;
	ssize_t samples;
	
	if (handle == NULL)
		return -EFAULT;
	if (channel != SND_PCM_CHANNEL_PLAYBACK &&
	    channel != SND_PCM_CHANNEL_CAPTURE)
		return -EINVAL;
	if (clt_samples == 0)
		return 0;
	samples = clt_samples;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_PLAYBACK);
		while (plugin && samples > 0) {
			plugin_next = plugin->next;
			if (plugin->dst_samples) {
				samples = plugin->dst_samples(plugin, samples);
				if (samples < 0)
					return samples;
			}
			plugin = plugin_next;
		}
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_CAPTURE);
		while (plugin) {
			plugin_prev = plugin->prev;
			if (plugin->src_samples) {
				samples = plugin->src_samples(plugin, samples);
				if (samples < 0)
					return samples;
			}
			plugin = plugin_prev;
		}
	} 
	return samples;
}

ssize_t snd_pcm_plug_client_size(snd_pcm_plugin_handle_t *handle, int channel, size_t drv_size)
{
	snd_pcm_plugin_t *plugin;
	ssize_t result = 0;
	
	if (handle == NULL)
		return -EFAULT;
	if (channel != SND_PCM_CHANNEL_PLAYBACK &&
	    channel != SND_PCM_CHANNEL_CAPTURE)
		return -EINVAL;
	if (drv_size == 0)
		return 0;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_PLAYBACK);
		if (plugin == NULL)
			return drv_size;
		result = snd_pcm_plugin_dst_size_to_samples(plugin, drv_size);
		if (result < 0)
			return result;
		result = snd_pcm_plug_client_samples(handle, SND_PCM_CHANNEL_PLAYBACK, result);
		if (result < 0)
			return result;
		plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_PLAYBACK);
		result = snd_pcm_plugin_src_samples_to_size(plugin, result);
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_CAPTURE);
		if (plugin == NULL)
			return drv_size;
		result = snd_pcm_plugin_src_size_to_samples(plugin, drv_size);
		if (result < 0)
			return result;
		result = snd_pcm_plug_client_samples(handle, SND_PCM_CHANNEL_CAPTURE, result);
		if (result < 0)
			return result;
		plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_CAPTURE);
		result = snd_pcm_plugin_dst_samples_to_size(plugin, result);
	}
	return result;
}

ssize_t snd_pcm_plug_slave_size(snd_pcm_plugin_handle_t *handle, int channel, size_t clt_size)
{
	snd_pcm_plugin_t *plugin;
	ssize_t result = 0;
	
	if (handle == NULL)
		return -EFAULT;
	if (channel != SND_PCM_CHANNEL_PLAYBACK &&
	    channel != SND_PCM_CHANNEL_CAPTURE)
		return -EINVAL;
	if (clt_size == 0)
		return 0;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_PLAYBACK);
		if (plugin == NULL)
			return clt_size;
		result = snd_pcm_plugin_src_size_to_samples(plugin, clt_size);
		if (result < 0)
			return result;
		result = snd_pcm_plug_slave_samples(handle, SND_PCM_CHANNEL_PLAYBACK, result);
		if (result < 0)
			return result;
		plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_PLAYBACK);
		result = snd_pcm_plugin_dst_samples_to_size(plugin, result);
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_CAPTURE);
		if (plugin == NULL)
			return clt_size;
		result = snd_pcm_plugin_dst_size_to_samples(plugin, clt_size);
		if (result < 0)
			return result;
		result = snd_pcm_plug_slave_samples(handle, SND_PCM_CHANNEL_CAPTURE, result);
		if (result < 0)
			return result;
		plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_CAPTURE);
		result = snd_pcm_plugin_src_samples_to_size(plugin, result);
	} 
	return result;
}


unsigned int snd_pcm_plug_formats(unsigned int formats)
{
	int linfmts = (SND_PCM_FMT_U8 | SND_PCM_FMT_S8 |
		       SND_PCM_FMT_U16_LE | SND_PCM_FMT_S16_LE |
		       SND_PCM_FMT_U16_BE | SND_PCM_FMT_S16_BE |
		       SND_PCM_FMT_U24_LE | SND_PCM_FMT_S24_LE |
		       SND_PCM_FMT_U24_BE | SND_PCM_FMT_S24_BE |
		       SND_PCM_FMT_U32_LE | SND_PCM_FMT_S32_LE |
		       SND_PCM_FMT_U32_BE | SND_PCM_FMT_S32_BE);
	formats |= SND_PCM_FMT_MU_LAW;
#ifndef __KERNEL__
	formats |= SND_PCM_FMT_A_LAW | SND_PCM_FMT_IMA_ADPCM;
#endif
	
	if (formats & linfmts)
		formats |= linfmts;
	return formats;
}

static int preferred_formats[] = {
	SND_PCM_SFMT_S16_LE,
	SND_PCM_SFMT_S16_BE,
	SND_PCM_SFMT_U16_LE,
	SND_PCM_SFMT_U16_BE,
	SND_PCM_SFMT_S24_LE,
	SND_PCM_SFMT_S24_BE,
	SND_PCM_SFMT_U24_LE,
	SND_PCM_SFMT_U24_BE,
	SND_PCM_SFMT_S32_LE,
	SND_PCM_SFMT_S32_BE,
	SND_PCM_SFMT_U32_LE,
	SND_PCM_SFMT_U32_BE,
	SND_PCM_SFMT_S8,
	SND_PCM_SFMT_U8
};

int snd_pcm_plug_slave_params(snd_pcm_channel_params_t *params,
			      snd_pcm_channel_info_t *slave_info,
			      snd_pcm_channel_params_t *slave_params)
{
	memcpy(slave_params, params, sizeof(*slave_params));
	if ((slave_info->formats & (1 << params->format.format)) == 0) {
		int format = params->format.format;
		if ((snd_pcm_plug_formats(slave_info->formats) & (1 << format)) == 0)
			return -EINVAL;
		if (snd_pcm_format_linear(format)) {
			int width = snd_pcm_format_width(format);
			int unsignd = snd_pcm_format_unsigned(format);
			int big = snd_pcm_format_big_endian(format);
			int format1;
			int wid, width1=width;
			int dwidth1 = 8;
			for (wid = 0; wid < 4; ++wid) {
				int end, big1 = big;
				for (end = 0; end < 2; ++end) {
					int sgn, unsignd1 = unsignd;
					for (sgn = 0; sgn < 2; ++sgn) {
						format1 = snd_pcm_build_linear_format(width1, unsignd1, big1);
						if (format1 >= 0 &&
						    slave_info->formats & (1 << format1))
							goto _found;
						unsignd1 = !unsignd1;
					}
					big1 = !big1;
				}
				if (width1 == 32) {
					dwidth1 = -dwidth1;
					width1 = width;
				}
				width1 += dwidth1;
			}
			return -EINVAL;
		_found:
			slave_params->format.format = format1;
		} else {
			unsigned int i;
			switch (format) {
			case SND_PCM_SFMT_MU_LAW:
#ifndef __KERNEL__
			case SND_PCM_SFMT_A_LAW:
			case SND_PCM_SFMT_IMA_ADPCM:
#endif
				for (i = 0; i < sizeof(preferred_formats) / sizeof(preferred_formats[0]); ++i) {
					int format1 = preferred_formats[i];
					if (slave_info->formats & (1 << format1)) {
						slave_params->format.format = format1;
						break;
					}
				}
				if (i == sizeof(preferred_formats)/sizeof(preferred_formats[0]))
					return -EINVAL;
				break;
			default:
				return -EINVAL;
			}
		}
	}

	/* voices */
      	if (params->format.voices < slave_info->min_voices ||
      	    params->format.voices > slave_info->max_voices) {
		unsigned int dst_voices = params->format.voices < slave_info->min_voices ?
				 slave_info->min_voices : slave_info->max_voices;
		slave_params->format.voices = dst_voices;
	}

	/* rate */
        if (params->format.rate < slave_info->min_rate ||
            params->format.rate > slave_info->max_rate) {
        	unsigned int dst_rate = params->format.rate < slave_info->min_rate ?
        		       slave_info->min_rate : slave_info->max_rate;
		slave_params->format.rate = dst_rate;
	}

	/* interleave */
	if (!(slave_info->flags & SND_PCM_CHNINFO_INTERLEAVE))
		slave_params->format.interleave = 0;
	if (!(slave_info->flags & SND_PCM_CHNINFO_NONINTERLEAVE))
		slave_params->format.interleave = 1;
	return 0;
}

int snd_pcm_plug_format(snd_pcm_plugin_handle_t *handle, 
			snd_pcm_channel_params_t *params, 
			snd_pcm_channel_params_t *slave_params)
{
	snd_pcm_channel_params_t tmpparams;
	snd_pcm_channel_params_t dstparams;
	snd_pcm_channel_params_t *srcparams;
	snd_pcm_plugin_t *plugin;
	int err;
	
	switch (params->channel) {
	case SND_PCM_CHANNEL_PLAYBACK:
		memcpy(&dstparams, slave_params, sizeof(*slave_params));
		srcparams = slave_params;
		memcpy(srcparams, params, sizeof(*params));
		break;
	case SND_PCM_CHANNEL_CAPTURE:
		memcpy(&dstparams, params, sizeof(*params));
		srcparams = params;
		memcpy(srcparams, slave_params, sizeof(*slave_params));
		break;
	default:
		return -EINVAL;
	}
	memcpy(&tmpparams, srcparams, sizeof(*srcparams));
		
	pdprintf("srcparams: interleave=%i, format=%i, rate=%i, voices=%i\n", 
		 srcparams->format.interleave,
		 srcparams->format.format,
		 srcparams->format.rate,
		 srcparams->format.voices);
	pdprintf("dstparams: interleave=%i, format=%i, rate=%i, voices=%i\n", 
		 dstparams.format.interleave,
		 dstparams.format.format,
		 dstparams.format.rate,
		 dstparams.format.voices);

	if (srcparams->format.voices == 1)
		srcparams->format.interleave = dstparams.format.interleave;

	/* Format change (linearization) */
	if ((srcparams->format.format != dstparams.format.format ||
	     srcparams->format.rate != dstparams.format.rate ||
	     srcparams->format.voices != dstparams.format.voices) &&
	    !snd_pcm_format_linear(srcparams->format.format)) {
		if (snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		else
			tmpparams.format.format = SND_PCM_SFMT_S16;
		tmpparams.format.interleave = dstparams.format.interleave;
		switch (srcparams->format.format) {
		case SND_PCM_SFMT_MU_LAW:
			err = snd_pcm_plugin_build_mulaw(handle,
							 params->channel,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
			break;
#ifndef __KERNEL__
		case SND_PCM_SFMT_A_LAW:
			err = snd_pcm_plugin_build_alaw(handle,
							params->channel,
							&srcparams->format,
							&tmpparams.format,
							&plugin);
			break;
		case SND_PCM_SFMT_IMA_ADPCM:
			err = snd_pcm_plugin_build_adpcm(handle,
							 params->channel,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
			break;
#endif
		default:
			return -EINVAL;
		}
		pdprintf("params format change: src=%i, dst=%i returns %i\n", srcparams->format.format, tmpparams.format.format, err);
		if (err < 0)
			return err;
		err = snd_pcm_plugin_append(plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
	}

	/* voices reduction */
	if (srcparams->format.voices > dstparams.format.voices) {
		int sv = srcparams->format.voices;
		int dv = dstparams.format.voices;
		route_ttable_entry_t *ttable = calloc(1, dv*sv*sizeof(*ttable));
#if 1
		if (sv == 2 && dv == 1) {
			ttable[0] = HALF;
			ttable[1] = HALF;
		} else
#endif
		{
			int v;
			for (v = 0; v < dv; ++v)
				ttable[v * sv + v] = FULL;
		}
		tmpparams.format.voices = dstparams.format.voices;
		tmpparams.format.interleave = dstparams.format.interleave;
		if (srcparams->format.rate == dstparams.format.rate &&
		    snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		err = snd_pcm_plugin_build_route(handle,
						 params->channel,
						 &srcparams->format,
						 &tmpparams.format,
						 ttable,
						 &plugin);
		free(ttable);
		pdprintf("params voices reduction: src=%i, dst=%i returns %i\n", srcparams->format.voices, tmpparams.format.voices, err);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		err = snd_pcm_plugin_append(plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
	}

	/* rate resampling */
	if (srcparams->format.rate != dstparams.format.rate) {
		tmpparams.format.rate = dstparams.format.rate;
		tmpparams.format.interleave = dstparams.format.interleave;
		if (srcparams->format.voices == dstparams.format.voices &&
		    snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
        	err = snd_pcm_plugin_build_rate(handle,
						params->channel,
        					&srcparams->format,
						&tmpparams.format,
						&plugin);
		pdprintf("params rate down resampling: src=%i, dst=%i returns %i\n", srcparams->format.rate, tmpparams.format.rate, err);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}      					    
		err = snd_pcm_plugin_append(plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
        }

	/* voices extension  */
	if (srcparams->format.voices < dstparams.format.voices) {
		int sv = srcparams->format.voices;
		int dv = dstparams.format.voices;
		route_ttable_entry_t *ttable = calloc(1, dv * sv * sizeof(*ttable));
#if 0
		{
			int v;
			for (v = 0; v < sv; ++v)
				ttable[v * sv + v] = FULL;
		}
#else
		{
			/* Playback is spreaded on all voices */
			int vd, vs;
			for (vd = 0, vs = 0; vd < dv; ++vd) {
				ttable[vd * sv + vs] = FULL;
				vs++;
				if (vs == sv)
					vs = 0;
			}
		}
#endif
		tmpparams.format.voices = dstparams.format.voices;
		tmpparams.format.interleave = dstparams.format.interleave;
		if (snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		err = snd_pcm_plugin_build_route(handle,
						 params->channel,
						 &srcparams->format,
						 &tmpparams.format,
						 ttable,
						 &plugin);
		free(ttable);
		pdprintf("params voices extension: src=%i, dst=%i returns %i\n", srcparams->format.voices, tmpparams.format.voices, err);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}      					    
		err = snd_pcm_plugin_append(plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
	}

	/* format change */
	if (srcparams->format.format != dstparams.format.format) {
		tmpparams.format.format = dstparams.format.format;
		tmpparams.format.interleave = dstparams.format.interleave;
		if (tmpparams.format.format == SND_PCM_SFMT_MU_LAW) {
			err = snd_pcm_plugin_build_mulaw(handle,
							 params->channel,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
		}
#ifndef __KERNEL__
		else if (tmpparams.format.format == SND_PCM_SFMT_A_LAW) {
			err = snd_pcm_plugin_build_alaw(handle,
							params->channel,
							&srcparams->format,
							&tmpparams.format,
							&plugin);
		}
		else if (tmpparams.format.format == SND_PCM_SFMT_IMA_ADPCM) {
			err = snd_pcm_plugin_build_adpcm(handle,
							 params->channel,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
		}
#endif
		else if (snd_pcm_format_linear(srcparams->format.format) &&
			 snd_pcm_format_linear(tmpparams.format.format)) {
			err = snd_pcm_plugin_build_linear(handle,
							  params->channel,
							  &srcparams->format,
							  &tmpparams.format,
							  &plugin);
		}
		else
			return -EINVAL;
		pdprintf("params format change: src=%i, dst=%i returns %i\n", srcparams->format.format, tmpparams.format.format, err);
		if (err < 0)
			return err;
		err = snd_pcm_plugin_append(plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
	}

	/* interleave */
	if (srcparams->format.interleave != dstparams.format.interleave) {
		tmpparams.format.interleave = dstparams.format.interleave;
		err = snd_pcm_plugin_build_copy(handle,
						params->channel,
						&srcparams->format,
						&tmpparams.format,
						&plugin);
		pdprintf("interleave change: src=%i, dst=%i returns %i\n", srcparams->format.interleave, tmpparams.format.interleave, err);
		if (err < 0)
			return err;
		err = snd_pcm_plugin_append(plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
	}

	return 0;
}

ssize_t snd_pcm_plug_client_voices_buf(snd_pcm_plugin_handle_t *handle,
				       int channel,
				       char *buf,
				       size_t count,
				       snd_pcm_plugin_voice_t **voices)
{
	snd_pcm_plugin_t *plugin;
	snd_pcm_plugin_voice_t *v;
	snd_pcm_format_t *format;
	int width, nvoices, voice;

	if (buf == NULL)
		return -EINVAL;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plug_first(handle, channel);
		format = &plugin->src_format;
		v = plugin->src_voices;
	}
	else {
		plugin = snd_pcm_plug_last(handle, channel);
		format = &plugin->dst_format;
		v = plugin->dst_voices;
	}
	*voices = v;
	if ((width = snd_pcm_format_physical_width(format->format)) < 0)
		return width;
	if ((count * 8) % width != 0)
		return -EINVAL;
	nvoices = format->voices;
	if (format->interleave ||
	    format->voices == 1) {
		for (voice = 0; voice < nvoices; voice++, v++) {
			v->enabled = 1;
			v->wanted = (channel == SND_PCM_CHANNEL_CAPTURE);
			v->aptr = NULL;
			v->area.addr = buf;
			v->area.first = voice * width;
			v->area.step = nvoices * width;
		}
		return count;
	} else
		return -EINVAL;
}

ssize_t snd_pcm_plug_client_voices_iovec(snd_pcm_plugin_handle_t *handle,
					 int channel,
					 const struct iovec *vector,
					 unsigned long count,
					 snd_pcm_plugin_voice_t **voices)
{
	snd_pcm_plugin_t *plugin;
	snd_pcm_plugin_voice_t *v;
	snd_pcm_format_t *format;
	int width;
	unsigned int nvoices, voice;

	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plug_first(handle, channel);
		format = &plugin->src_format;
		v = plugin->src_voices;
	}
	else {
		plugin = snd_pcm_plug_last(handle, channel);
		format = &plugin->dst_format;
		v = plugin->dst_voices;
	}
	*voices = v;
	if ((width = snd_pcm_format_physical_width(format->format)) < 0)
		return width;
	nvoices = format->voices;
	if (format->interleave) {
		if (count != 1 || vector->iov_base == NULL ||
		    (vector->iov_len * 8) % width != 0)
			return -EINVAL;
		
		for (voice = 0; voice < nvoices; voice++, v++) {
			v->enabled = 1;
			v->wanted = (channel == SND_PCM_CHANNEL_CAPTURE);
			v->aptr = NULL;
			v->area.addr = vector->iov_base;
			v->area.first = voice * width;
			v->area.step = nvoices * width;
		}
		return vector->iov_len;
	} else {
		size_t len;
		if (count != nvoices)
			return -EINVAL;
		len = vector->iov_len;
		if ((len * 8) % width != 0)
			return -EINVAL;
		for (voice = 0; voice < nvoices; voice++, v++, vector++) {
			if (vector->iov_len != len)
				return -EINVAL;
			v->enabled = (vector->iov_base != NULL);
			v->wanted = (v->enabled && (channel == SND_PCM_CHANNEL_CAPTURE));
			v->aptr = NULL;
			v->area.addr = vector->iov_base;
			v->area.first = 0;
			v->area.step = width;
		}
		return len * nvoices;
	}
}

int snd_pcm_plug_playback_voices_mask(snd_pcm_plugin_handle_t *handle,
				      bitset_t *client_vmask)
{
#ifndef __KERNEL__
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &handle->private;
#endif
	snd_pcm_plugin_t *plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_PLAYBACK);
	if (plugin == NULL) {
#ifndef __KERNEL__
		return snd_pcm_voices_mask(plug->slave, SND_PCM_CHANNEL_PLAYBACK, client_vmask);
#else
		return 0;
#endif
	} else {
		int svoices = plugin->dst_format.voices;
		bitset_t bs[bitset_size(svoices)];
		bitset_t *srcmask;
		bitset_t *dstmask = bs;
		int err;
		bitset_one(dstmask, svoices);
#ifndef __KERNEL__
		err = snd_pcm_voices_mask(plug->slave, SND_PCM_CHANNEL_PLAYBACK, dstmask);
		if (err < 0)
			return err;
#endif
		if (plugin == NULL) {
			bitset_and(client_vmask, dstmask, svoices);
			return 0;
		}
		while (1) {
			err = plugin->src_voices_mask(plugin, dstmask, &srcmask);
			if (err < 0)
				return err;
			dstmask = srcmask;
			if (plugin->prev == NULL)
				break;
			plugin = plugin->prev;
		}
		bitset_and(client_vmask, dstmask, plugin->src_format.voices);
		return 0;
	}
}

int snd_pcm_plug_capture_voices_mask(snd_pcm_plugin_handle_t *handle,
				     bitset_t *client_vmask)
{
#ifndef __KERNEL__
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &handle->private;
#endif
	snd_pcm_plugin_t *plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_CAPTURE);
	if (plugin == NULL) {
#ifndef __KERNEL__
		return snd_pcm_voices_mask(plug->slave, SND_PCM_CHANNEL_CAPTURE, client_vmask);
#else
		return 0;
#endif
	} else {
		int svoices = plugin->src_format.voices;
		bitset_t bs[bitset_size(svoices)];
		bitset_t *srcmask = bs;
		bitset_t *dstmask;
		int err;
		bitset_one(srcmask, svoices);
#ifndef __KERNEL__
		err = snd_pcm_voices_mask(plug->slave, SND_PCM_CHANNEL_CAPTURE, srcmask);
		if (err < 0)
			return err;
#endif
		while (1) {
			err = plugin->dst_voices_mask(plugin, srcmask, &dstmask);
			if (err < 0)
				return err;
			srcmask = dstmask;
			if (plugin->next == NULL)
				break;
			plugin = plugin->next;
		}
		bitset_and(client_vmask, srcmask, plugin->dst_format.voices);
		return 0;
	}
}

static int snd_pcm_plug_playback_disable_useless_voices(snd_pcm_plugin_handle_t *handle,
							snd_pcm_plugin_voice_t *src_voices)
{
	snd_pcm_plugin_t *plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_PLAYBACK);
	unsigned int nvoices = plugin->src_format.voices;
	bitset_t bs[bitset_size(nvoices)];
	bitset_t *srcmask = bs;
	int err;
	unsigned int voice;
	for (voice = 0; voice < nvoices; voice++) {
		if (src_voices[voice].enabled)
			bitset_set(srcmask, voice);
		else
			bitset_reset(srcmask, voice);
	}
	err = snd_pcm_plug_playback_voices_mask(handle, srcmask);
	if (err < 0)
		return err;
	for (voice = 0; voice < nvoices; voice++) {
		if (!bitset_get(srcmask, voice))
			src_voices[voice].enabled = 0;
	}
	return 0;
}

static int snd_pcm_plug_capture_disable_useless_voices(snd_pcm_plugin_handle_t *handle,
						       snd_pcm_plugin_voice_t *src_voices,
						       snd_pcm_plugin_voice_t *client_voices)
{
#ifndef __KERNEL__
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &handle->private;
#endif
	snd_pcm_plugin_t *plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_CAPTURE);
	unsigned int nvoices = plugin->dst_format.voices;
	bitset_t bs[bitset_size(nvoices)];
	bitset_t *dstmask = bs;
	bitset_t *srcmask;
	int err;
	unsigned int voice;
	for (voice = 0; voice < nvoices; voice++) {
		if (client_voices[voice].enabled)
			bitset_set(dstmask, voice);
		else
			bitset_reset(dstmask, voice);
	}
	while (plugin) {
		err = plugin->src_voices_mask(plugin, dstmask, &srcmask);
		if (err < 0)
			return err;
		dstmask = srcmask;
		plugin = plugin->prev;
	}
#ifndef __KERNEL__
	err = snd_pcm_voices_mask(plug->slave, SND_PCM_CHANNEL_CAPTURE, dstmask);
	if (err < 0)
		return err;
#endif
	plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_CAPTURE);
	nvoices = plugin->src_format.voices;
	for (voice = 0; voice < nvoices; voice++) {
		if (!bitset_get(dstmask, voice))
			src_voices[voice].enabled = 0;
	}
	return 0;
}

ssize_t snd_pcm_plug_write_transfer(snd_pcm_plugin_handle_t *handle, snd_pcm_plugin_voice_t *src_voices, size_t size)
{
	snd_pcm_plugin_t *plugin, *next;
	snd_pcm_plugin_voice_t *dst_voices;
	ssize_t samples;
	int err;

	if ((err = snd_pcm_plug_playback_disable_useless_voices(handle, src_voices)) < 0)
		return err;
	
	plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_PLAYBACK);
	samples = snd_pcm_plugin_src_size_to_samples(plugin, size);
	if (samples < 0)
		return samples;
	while (plugin && samples > 0) {
		if ((next = plugin->next) != NULL) {
			ssize_t samples1 = samples;
			if (plugin->dst_samples)
				samples1 = plugin->dst_samples(plugin, samples);
			if ((err = next->client_voices(next, samples1, &dst_voices)) < 0) {
				snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_PLAYBACK, src_voices->aptr);
				return err;
			}
		} else {
			if ((err = snd_pcm_plugin_slave_voices(plugin, samples, &dst_voices)) < 0)
				return err;
		}
		pdprintf("write plugin: %s, %i\n", plugin->name, samples);
		if ((samples = plugin->transfer(plugin, src_voices, dst_voices, samples)) < 0) {
			snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_PLAYBACK, src_voices->aptr);
			snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_PLAYBACK, dst_voices->aptr);
			return samples;
		}
		snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_PLAYBACK, src_voices->aptr);
		src_voices = dst_voices;
		plugin = next;
	}
	snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_PLAYBACK, src_voices->aptr);
	samples = snd_pcm_plug_client_samples(handle, SND_PCM_CHANNEL_PLAYBACK, samples);
	if (samples < 0)
		return samples;
	return snd_pcm_plugin_src_samples_to_size(snd_pcm_plug_first(handle, SND_PCM_CHANNEL_PLAYBACK), samples);
}

ssize_t snd_pcm_plug_read_transfer(snd_pcm_plugin_handle_t *handle, snd_pcm_plugin_voice_t *dst_voices_final, size_t size)
{
	snd_pcm_plugin_t *plugin, *next;
	snd_pcm_plugin_voice_t *src_voices, *dst_voices;
	ssize_t samples;
	int err;

	plugin = snd_pcm_plug_last(handle, SND_PCM_CHANNEL_CAPTURE);
	samples = snd_pcm_plugin_dst_size_to_samples(plugin, size);
	if (samples < 0)
		return samples;
	samples = snd_pcm_plug_slave_samples(handle, SND_PCM_CHANNEL_CAPTURE, samples);
	if (samples < 0)
		return samples;

	plugin = snd_pcm_plug_first(handle, SND_PCM_CHANNEL_CAPTURE);
	if ((err = snd_pcm_plugin_slave_voices(plugin, samples, &src_voices)) < 0)
		return err;
	if ((err = snd_pcm_plug_capture_disable_useless_voices(handle, src_voices, dst_voices_final) < 0))
		return err;
	
	while (plugin && samples > 0) {
		if ((next = plugin->next) != NULL) {
			if ((err = plugin->client_voices(plugin, samples, &dst_voices)) < 0) {
				snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_CAPTURE, src_voices->aptr);
				return err;
			}
		} else {
			dst_voices = dst_voices_final;
		}
		pdprintf("read plugin: %s, %i\n", plugin->name, samples);
		if ((samples = plugin->transfer(plugin, src_voices, dst_voices, samples)) < 0) {
			snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_CAPTURE, src_voices->aptr);
			snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_CAPTURE, dst_voices->aptr);
			return samples;
		}
#if 0
		{
		  unsigned int voice;
		  for (voice = 0; voice < plugin->src_format.voices; ++voice) {
		    fprintf(stderr, "%d%d ", src_voices[voice].enabled, src_voices[voice].wanted);
		  }
		  fprintf(stderr, " -> ");
		  for (voice = 0; voice < plugin->dst_format.voices; ++voice) {
		    fprintf(stderr, "%d%d ", dst_voices[voice].enabled, dst_voices[voice].wanted);
		  }
		  fprintf(stderr, "\n");
		}
#endif
		snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_CAPTURE, src_voices->aptr);
		plugin = next;
		src_voices = dst_voices;
	}
	snd_pcm_plug_buf_unlock(handle, SND_PCM_CHANNEL_CAPTURE, src_voices->aptr);
	return snd_pcm_plugin_dst_samples_to_size(snd_pcm_plug_last(handle, SND_PCM_CHANNEL_CAPTURE), samples);
}

int snd_pcm_area_silence(const snd_pcm_voice_area_t *dst_area, size_t dst_offset,
			  size_t samples, int format)
{
	/* FIXME: sub byte resolution and odd dst_offset */
	char *dst;
	unsigned int dst_step;
	int width;
	u_int64_t silence;
	if (!dst_area->addr)
		return 0;
	dst = dst_area->addr + (dst_area->first + dst_area->step * dst_offset) / 8;
	width = snd_pcm_format_physical_width(format);
	silence = snd_pcm_format_silence_64(format);
	if (dst_area->step == (unsigned int) width) {
		size_t dwords = samples * width / 64;
		samples -= dwords * 64 / width;
		while (dwords-- > 0)
			*((u_int64_t*)dst)++ = silence;
		if (samples == 0)
			return 0;
	}
	dst_step = dst_area->step / 8;
	switch (width) {
	case 4: {
		u_int8_t s0 = silence & 0xf0;
		u_int8_t s1 = silence & 0x0f;
		int dstbit = dst_area->first % 8;
		int dstbit_step = dst_area->step % 8;
		while (samples-- > 0) {
			if (dstbit) {
				*dst &= 0xf0;
				*dst |= s1;
			} else {
				*dst &= 0x0f;
				*dst |= s0;
			}
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8: {
		u_int8_t sil = silence;
		while (samples-- > 0) {
			*dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 16: {
		u_int16_t sil = silence;
		while (samples-- > 0) {
			*(u_int16_t*)dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 32: {
		u_int32_t sil = silence;
		while (samples-- > 0) {
			*(u_int32_t*)dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 64: {
		while (samples-- > 0) {
			*(u_int64_t*)dst = silence;
			dst += dst_step;
		}
		break;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

int snd_pcm_areas_silence(const snd_pcm_voice_area_t *dst_areas, size_t dst_offset,
			  size_t vcount, size_t samples, int format)
{
	int width = snd_pcm_format_physical_width(format);
	while (vcount > 0) {
		void *addr = dst_areas->addr;
		unsigned int step = dst_areas->step;
		const snd_pcm_voice_area_t *begin = dst_areas;
		int vc = vcount;
		unsigned int v = 0;
		int err;
		while (1) {
			vc--;
			v++;
			dst_areas++;
			if (vc == 0 ||
			    dst_areas->addr != addr ||
			    dst_areas->step != step ||
			    dst_areas->first != dst_areas[-1].first + width)
				break;
		}
		if (v > 1 && v * width == step) {
			/* Collapse the areas */
			snd_pcm_voice_area_t d;
			d.addr = begin->addr;
			d.first = begin->first;
			d.step = width;
			err = snd_pcm_area_silence(&d, dst_offset * v, samples * v, format);
			vcount -= v;
		} else {
			err = snd_pcm_area_silence(begin, dst_offset, samples, format);
			dst_areas = begin + 1;
			vcount--;
		}
		if (err < 0)
			return err;
	}
	return 0;
}


int snd_pcm_area_copy(const snd_pcm_voice_area_t *src_area, size_t src_offset,
		      const snd_pcm_voice_area_t *dst_area, size_t dst_offset,
		      size_t samples, int format)
{
	/* FIXME: sub byte resolution and odd dst_offset */
	char *src, *dst;
	int width;
	int src_step, dst_step;
	src = src_area->addr + (src_area->first + src_area->step * src_offset) / 8;
	if (!src_area->addr)
		return snd_pcm_area_silence(dst_area, dst_offset, samples, format);
	dst = dst_area->addr + (dst_area->first + dst_area->step * dst_offset) / 8;
	if (!dst_area->addr)
		return 0;
	width = snd_pcm_format_physical_width(format);
	if (src_area->step == (unsigned int) width &&
	    dst_area->step == (unsigned int) width) {
		size_t bytes = samples * width / 8;
		samples -= bytes * 8 / width;
		memcpy(dst, src, bytes);
		if (samples == 0)
			return 0;
	}
	src_step = src_area->step / 8;
	dst_step = dst_area->step / 8;
	switch (width) {
	case 4: {
		int srcbit = src_area->first % 8;
		int srcbit_step = src_area->step % 8;
		int dstbit = dst_area->first % 8;
		int dstbit_step = dst_area->step % 8;
		while (samples-- > 0) {
			unsigned char srcval;
			if (srcbit)
				srcval = *src & 0x0f;
			else
				srcval = *src & 0xf0;
			if (dstbit)
				*dst &= 0xf0;
			else
				*dst &= 0x0f;
			*dst |= srcval;
			src += src_step;
			srcbit += srcbit_step;
			if (srcbit == 8) {
				src++;
				srcbit = 0;
			}
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8: {
		while (samples-- > 0) {
			*dst = *src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 16: {
		while (samples-- > 0) {
			*(u_int16_t*)dst = *(u_int16_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 32: {
		while (samples-- > 0) {
			*(u_int32_t*)dst = *(u_int32_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 64: {
		while (samples-- > 0) {
			*(u_int64_t*)dst = *(u_int64_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

int snd_pcm_areas_copy(const snd_pcm_voice_area_t *src_areas, size_t src_offset,
		       const snd_pcm_voice_area_t *dst_areas, size_t dst_offset,
		       size_t vcount, size_t samples, int format)
{
	int width = snd_pcm_format_physical_width(format);
	while (vcount > 0) {
		unsigned int step = src_areas->step;
		void *src_addr = src_areas->addr;
		const snd_pcm_voice_area_t *src_start = src_areas;
		void *dst_addr = dst_areas->addr;
		const snd_pcm_voice_area_t *dst_start = dst_areas;
		int vc = vcount;
		unsigned int v = 0;
		while (dst_areas->step == step) {
			vc--;
			v++;
			src_areas++;
			dst_areas++;
			if (vc == 0 ||
			    src_areas->step != step ||
			    src_areas->addr != src_addr ||
			    dst_areas->addr != dst_addr ||
			    src_areas->first != src_areas[-1].first + width ||
			    dst_areas->first != dst_areas[-1].first + width)
				break;
		}
		if (v > 1 && v * width == step) {
			/* Collapse the areas */
			snd_pcm_voice_area_t s, d;
			s.addr = src_start->addr;
			s.first = src_start->first;
			s.step = width;
			d.addr = dst_start->addr;
			d.first = dst_start->first;
			d.step = width;
			snd_pcm_area_copy(&s, src_offset * v, &d, dst_offset * v, samples * v, format);
			vcount -= v;
		} else {
			snd_pcm_area_copy(src_start, src_offset, dst_start, dst_offset, samples, format);
			src_areas = src_start + 1;
			dst_areas = dst_start + 1;
			vcount--;
		}
	}
	return 0;
}
