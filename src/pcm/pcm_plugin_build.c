/*
 *  PCM Plug-In shared (kernel/library) code
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
  
#if 0
#define PLUGIN_DEBUG
#endif
#ifdef __KERNEL__
#include "../include/driver.h"
#include "../include/pcm.h"
#define snd_pcm_plugin_first(pb, channel) ((pb)->oss.plugin_first)
#define snd_pcm_plugin_last(pb, channel) ((pb)->oss.plugin_last)
#define snd_pcm_plugin_append(pb, channel, plugin) snd_pcm_oss_plugin_append(pb, plugin)
#define my_calloc(size) snd_kcalloc(size, GFP_KERNEL)
#define my_free(ptr) snd_kfree(ptr)
#define my_strdup(str) snd_kmalloc_strdup(str, GFP_KERNEL)
#else
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "pcm_local.h"
#define my_calloc(size) calloc(1, size)
#define my_free(ptr) free(ptr)
#define my_strdup(str) strdup(str)
#endif

ssize_t snd_pcm_plugin_src_samples_to_size(snd_pcm_plugin_t *plugin, size_t samples)
{
	ssize_t result;

	if (plugin == NULL)
		return -EINVAL;
	result = samples * plugin->src_format.voices * plugin->src_width;
#if 0
	if ((result % 8) != 0)
		return -EINVAL;
#endif
	return result / 8;
}

ssize_t snd_pcm_plugin_dst_samples_to_size(snd_pcm_plugin_t *plugin, size_t samples)
{
	ssize_t result;

	if (plugin == NULL)
		return -EINVAL;
	result = samples * plugin->dst_format.voices * plugin->dst_width;
#if 0
	if ((result % 8) != 0)
		return -EINVAL;
#endif
	return result / 8;
}

ssize_t snd_pcm_plugin_src_size_to_samples(snd_pcm_plugin_t *plugin, size_t size)
{
	ssize_t result;
	long tmp;

	if (plugin == NULL)
		return -EINVAL;
	result = size * 8;
	tmp = plugin->src_format.voices * plugin->src_width;
#if 0
	if ((result % tmp) != 0)
		return -EINVAL;
#endif
	return result / tmp;
}

ssize_t snd_pcm_plugin_dst_size_to_samples(snd_pcm_plugin_t *plugin, size_t size)
{
	ssize_t result;
	long tmp;

	if (plugin == NULL)
		return -EINVAL;
	result = size * 8;
	tmp = plugin->dst_format.voices * plugin->dst_width;
#if 0
	if ((result % tmp) != 0)
		return -EINVAL;
#endif
	return result / tmp;
}

ssize_t snd_pcm_plugin_client_samples(snd_pcm_plugin_handle_t *pb, int channel, size_t drv_samples)
{
	snd_pcm_plugin_t *plugin, *plugin_prev, *plugin_next;
	
	if (pb == NULL || (channel != SND_PCM_CHANNEL_PLAYBACK &&
			   channel != SND_PCM_CHANNEL_CAPTURE))
		return -EINVAL;
	if (drv_samples == 0)
		return 0;
	if (drv_samples < 0)
		return -EINVAL;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plugin_last(pb, SND_PCM_CHANNEL_PLAYBACK);
		while (plugin && drv_samples > 0) {
			plugin_prev = plugin->prev;
			if (plugin->src_samples)
				drv_samples = plugin->src_samples(plugin, drv_samples);
			plugin = plugin_prev;
		}
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plugin_first(pb, SND_PCM_CHANNEL_CAPTURE);
		while (plugin && drv_samples > 0) {
			plugin_next = plugin->next;
			if (plugin->dst_samples)
				drv_samples = plugin->dst_samples(plugin, drv_samples);
			plugin = plugin_next;
		}
	}
	return drv_samples;
}

ssize_t snd_pcm_plugin_hardware_samples(snd_pcm_plugin_handle_t *pb, int channel, size_t clt_samples)
{
	snd_pcm_plugin_t *plugin, *plugin_prev, *plugin_next;
	
	if (pb == NULL || (channel != SND_PCM_CHANNEL_PLAYBACK &&
			   channel != SND_PCM_CHANNEL_CAPTURE))
		return -EINVAL;
	if (clt_samples == 0)
		return 0;
	if (clt_samples < 0)
		return -EINVAL;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plugin_first(pb, SND_PCM_CHANNEL_PLAYBACK);
		while (plugin && clt_samples > 0) {
			plugin_next = plugin->next;
			if (plugin->dst_samples)
				clt_samples = plugin->dst_samples(plugin, clt_samples);
			plugin = plugin_next;
		}
		if (clt_samples < 0)
			return clt_samples;
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plugin_last(pb, SND_PCM_CHANNEL_CAPTURE);
		while (plugin) {
			plugin_prev = plugin->prev;
			if (plugin->src_samples)
				clt_samples = plugin->src_samples(plugin, clt_samples);
			plugin = plugin_prev;
		}
	} 
	return clt_samples;
}

ssize_t snd_pcm_plugin_client_size(snd_pcm_plugin_handle_t *pb, int channel, size_t drv_size)
{
	snd_pcm_plugin_t *plugin;
	ssize_t result = 0;
	
	if (pb == NULL || (channel != SND_PCM_CHANNEL_PLAYBACK &&
			   channel != SND_PCM_CHANNEL_CAPTURE))
		return -EINVAL;
	if (drv_size == 0)
		return 0;
	if (drv_size < 0)
		return -EINVAL;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plugin_last(pb, SND_PCM_CHANNEL_PLAYBACK);
		if (plugin == NULL)
			return drv_size;
		result = snd_pcm_plugin_dst_size_to_samples(plugin, drv_size);
		result = snd_pcm_plugin_client_samples(pb, SND_PCM_CHANNEL_PLAYBACK, result);
		if (result < 0)
			return result;
		plugin = snd_pcm_plugin_first(pb, SND_PCM_CHANNEL_PLAYBACK);
		result = snd_pcm_plugin_src_samples_to_size(plugin, result);
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plugin_first(pb, SND_PCM_CHANNEL_CAPTURE);
		if (plugin == NULL)
			return drv_size;
		result = snd_pcm_plugin_src_size_to_samples(plugin, drv_size);
		result = snd_pcm_plugin_client_samples(pb, SND_PCM_CHANNEL_PLAYBACK, result);
		plugin = snd_pcm_plugin_last(pb, SND_PCM_CHANNEL_CAPTURE);
		result = snd_pcm_plugin_dst_samples_to_size(plugin, result);
	}
	return result;
}

ssize_t snd_pcm_plugin_hardware_size(snd_pcm_plugin_handle_t *pb, int channel, size_t clt_size)
{
	snd_pcm_plugin_t *plugin;
	ssize_t result = 0;
	
	if (pb == NULL || (channel != SND_PCM_CHANNEL_PLAYBACK &&
			   channel != SND_PCM_CHANNEL_CAPTURE))
		return -EINVAL;
	if (clt_size == 0)
		return 0;
	if (clt_size < 0)
		return -EINVAL;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		plugin = snd_pcm_plugin_first(pb, SND_PCM_CHANNEL_PLAYBACK);
		if (plugin == NULL)
			return clt_size;
		result = snd_pcm_plugin_src_size_to_samples(plugin, clt_size);
		result = snd_pcm_plugin_hardware_samples(pb, SND_PCM_CHANNEL_PLAYBACK, result);
		if (result < 0)
			return result;
		plugin = snd_pcm_plugin_last(pb, SND_PCM_CHANNEL_PLAYBACK);
		result = snd_pcm_plugin_dst_samples_to_size(plugin, result);
	} else if (channel == SND_PCM_CHANNEL_CAPTURE) {
		plugin = snd_pcm_plugin_last(pb, SND_PCM_CHANNEL_CAPTURE);
		if (plugin == NULL)
			return clt_size;
		result = snd_pcm_plugin_dst_size_to_samples(plugin, clt_size);
		result = snd_pcm_plugin_hardware_samples(pb, SND_PCM_CHANNEL_PLAYBACK, result);
		if (result < 0)
			return result;
		plugin = snd_pcm_plugin_first(pb, SND_PCM_CHANNEL_CAPTURE);
		result = snd_pcm_plugin_src_samples_to_size(plugin, result);
	} 
	return result;
}


unsigned int snd_pcm_plugin_formats(unsigned int formats)
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

int snd_pcm_plugin_hwparams(snd_pcm_channel_params_t *params,
			    snd_pcm_channel_info_t *hwinfo,
			    snd_pcm_channel_params_t *hwparams)
{
	memcpy(hwparams, params, sizeof(*hwparams));
	if ((hwinfo->formats & (1 << params->format.format)) == 0) {
		int format = params->format.format;
		if ((snd_pcm_plugin_formats(hwinfo->formats) & (1 << format)) == 0)
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
						    hwinfo->formats & (1 << format1))
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
			hwparams->format.format = format1;
		} else {
			int i;
			switch (format) {
			case SND_PCM_SFMT_MU_LAW:
#ifndef __KERNEL__
			case SND_PCM_SFMT_A_LAW:
			case SND_PCM_SFMT_IMA_ADPCM:
#endif
				for (i = 0; i < sizeof(preferred_formats) / sizeof(preferred_formats[0]); ++i) {
					int format1 = preferred_formats[i];
					if (hwinfo->formats & (1 << format1)) {
						hwparams->format.format = format1;
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
      	if (params->format.voices < hwinfo->min_voices ||
      	    params->format.voices > hwinfo->max_voices) {
		int dst_voices = params->format.voices < hwinfo->min_voices ?
				 hwinfo->min_voices : hwinfo->max_voices;
		if ((params->format.rate < hwinfo->min_rate ||
		     params->format.rate > hwinfo->max_rate) &&
		    dst_voices > 2)
			dst_voices = 2;
		hwparams->format.voices = dst_voices;
	}

	/* rate */
        if (params->format.rate < hwinfo->min_rate ||
            params->format.rate > hwinfo->max_rate) {
        	int dst_rate = params->format.rate < hwinfo->min_rate ?
        		       hwinfo->min_rate : hwinfo->max_rate;
		hwparams->format.rate = dst_rate;
	}

	/* interleave */
	if (!(hwinfo->flags & SND_PCM_CHNINFO_INTERLEAVE))
		hwparams->format.interleave = 0;
	if (!(hwinfo->flags & SND_PCM_CHNINFO_NONINTERLEAVE))
		hwparams->format.interleave = 1;
	return 0;
}

#ifdef __KERNEL__
#define FULL ROUTE_PLUGIN_RESOLUTION
#define HALF ROUTE_PLUGIN_RESOLUTION / 2
typedef int ttable_entry_t;
#else
#define FULL 1.0
#define HALF 0.5
typedef float ttable_entry_t;
#endif

int snd_pcm_plugin_format(snd_pcm_plugin_handle_t *pb, 
			  snd_pcm_channel_params_t *params, 
			  snd_pcm_channel_params_t *hwparams,
			  snd_pcm_channel_info_t *hwinfo)
{
	snd_pcm_channel_params_t tmpparams;
	snd_pcm_channel_params_t dstparams;
	snd_pcm_channel_params_t *srcparams;
	snd_pcm_plugin_t *plugin;
	int err;
	
	switch (params->channel) {
	case SND_PCM_CHANNEL_PLAYBACK:
		memcpy(&dstparams, hwparams, sizeof(*hwparams));
		srcparams = hwparams;
		memcpy(srcparams, params, sizeof(*params));
		break;
	case SND_PCM_CHANNEL_CAPTURE:
		memcpy(&dstparams, params, sizeof(*params));
		srcparams = params;
		memcpy(srcparams, hwparams, sizeof(*hwparams));
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

	/* Format change (linearization) */
	if ((srcparams->format.format != dstparams.format.format ||
	     srcparams->format.rate != dstparams.format.rate ||
	     srcparams->format.voices != dstparams.format.voices) &&
	    !snd_pcm_format_linear(srcparams->format.format)) {
		if (snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		else
			tmpparams.format.format = SND_PCM_SFMT_S16;
		switch (srcparams->format.format) {
		case SND_PCM_SFMT_MU_LAW:
			err = snd_pcm_plugin_build_mulaw(pb,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
			break;
#ifndef __KERNEL__
		case SND_PCM_SFMT_A_LAW:
			err = snd_pcm_plugin_build_alaw(pb,
							&srcparams->format,
							&tmpparams.format,
							&plugin);
			break;
		case SND_PCM_SFMT_IMA_ADPCM:
			err = snd_pcm_plugin_build_adpcm(pb,
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
		err = snd_pcm_plugin_append(pb, params->channel, plugin);
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
		ttable_entry_t *ttable = my_calloc(dv*sv*sizeof(*ttable));
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
		if (srcparams->format.rate == dstparams.format.rate &&
		    snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		err = snd_pcm_plugin_build_route(pb, &srcparams->format,
						 &tmpparams.format,
						 ttable,
						 &plugin);
		my_free(ttable);
		pdprintf("params voices reduction: src=%i, dst=%i returns %i\n", srcparams->format.voices, tmpparams.format.voices, err);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		err = snd_pcm_plugin_append(pb, params->channel, plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
	}

	/* rate resampling */
	if (srcparams->format.rate != dstparams.format.rate) {
		tmpparams.format.rate = dstparams.format.rate;
		if (srcparams->format.voices == dstparams.format.voices &&
		    snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
        	err = snd_pcm_plugin_build_rate(pb,
        					&srcparams->format,
						&tmpparams.format,
						&plugin);
		pdprintf("params rate down resampling: src=%i, dst=%i returns %i\n", srcparams->format.rate, tmpparams.format.rate, err);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}      					    
		err = snd_pcm_plugin_append(pb, params->channel, plugin);
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
		ttable_entry_t *ttable = my_calloc(dv * sv * sizeof(*ttable));
#if 1
		if (sv == 1 && dv == 2) {
			ttable[0] = FULL;
			ttable[1] = FULL;
		} else
#endif
		{
			int v;
			for (v = 0; v < sv; ++v)
				ttable[v * sv + v] = FULL;
		}
		tmpparams.format.voices = dstparams.format.voices;
		if (snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		err = snd_pcm_plugin_build_route(pb,
						 &srcparams->format,
						 &tmpparams.format,
						 ttable,
						 &plugin);
		my_free(ttable);
		pdprintf("params voices extension: src=%i, dst=%i returns %i\n", srcparams->format.voices, tmpparams.format.voices, err);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}      					    
		err = snd_pcm_plugin_append(pb, params->channel, plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
	}

	/* format change */
	if (srcparams->format.format != dstparams.format.format) {
		tmpparams.format.format = dstparams.format.format;
		if (tmpparams.format.format == SND_PCM_SFMT_MU_LAW) {
			err = snd_pcm_plugin_build_mulaw(pb,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
		}
#ifndef __KERNEL__
		else if (tmpparams.format.format == SND_PCM_SFMT_A_LAW) {
			err = snd_pcm_plugin_build_alaw(pb,
							&srcparams->format,
							&tmpparams.format,
							&plugin);
		}
		else if (tmpparams.format.format == SND_PCM_SFMT_IMA_ADPCM) {
			err = snd_pcm_plugin_build_adpcm(pb,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
		}
#endif
		else if (snd_pcm_format_linear(srcparams->format.format) &&
			 snd_pcm_format_linear(tmpparams.format.format)) {
			err = snd_pcm_plugin_build_linear(pb,
							  &srcparams->format,
							  &tmpparams.format,
							  &plugin);
		}
		else
			return -EINVAL;
		pdprintf("params format change: src=%i, dst=%i returns %i\n", srcparams->format.format, tmpparams.format.format, err);
		if (err < 0)
			return err;
		err = snd_pcm_plugin_append(pb, params->channel, plugin);
		if (err < 0) {
			snd_pcm_plugin_free(plugin);
			return err;
		}
		srcparams->format = tmpparams.format;
	}

	return 0;
}
