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
#define snd_pcm_plug_first(handle, stream) ((handle)->runtime->oss.plugin_first)
#define snd_pcm_plug_last(handle, stream) ((handle)->runtime->oss.plugin_last)
#else
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include "pcm_local.h"
#endif

static int snd_pcm_plugin_src_channels_mask(snd_pcm_plugin_t *plugin,
					  bitset_t *dst_vmask,
					  bitset_t **src_vmask)
{
	bitset_t *vmask = plugin->src_vmask;
	bitset_copy(vmask, dst_vmask, plugin->src_format.channels);
	*src_vmask = vmask;
	return 0;
}

static int snd_pcm_plugin_dst_channels_mask(snd_pcm_plugin_t *plugin,
					  bitset_t *src_vmask,
					  bitset_t **dst_vmask)
{
	bitset_t *vmask = plugin->dst_vmask;
	bitset_copy(vmask, src_vmask, plugin->dst_format.channels);
	*dst_vmask = vmask;
	return 0;
}

static ssize_t snd_pcm_plugin_side_channels(snd_pcm_plugin_t *plugin,
					  int client_side,
					  size_t frames,
					  snd_pcm_plugin_channel_t **channels)
{
	char *ptr;
	int width;
	unsigned int channel;
	long size;
	snd_pcm_plugin_channel_t *v;
	snd_pcm_format_t *format;
	if ((plugin->stream == SND_PCM_STREAM_PLAYBACK && client_side) ||
	    (plugin->stream == SND_PCM_STREAM_CAPTURE && !client_side)) {
		format = &plugin->src_format;
		v = plugin->src_channels;
	} else {
		format = &plugin->dst_format;
		v = plugin->dst_channels;
	}

	*channels = v;
	if ((width = snd_pcm_format_physical_width(format->format)) < 0)
		return width;	
	size = frames * format->channels * width;
	assert(size % 8 == 0);
	size /= 8;
	ptr = (char *)snd_pcm_plug_buf_alloc(plugin->handle, plugin->stream, size);
	if (ptr == NULL)
		return -ENOMEM;
	assert(size % format->channels == 0);
	size /= format->channels;
	for (channel = 0; channel < format->channels; channel++, v++) {
		v->enabled = 1;
		v->wanted = 0;
		v->aptr = ptr;
		if (format->interleave) {
			v->area.addr = ptr;
			v->area.first = channel * width;
			v->area.step = format->channels * width;
		} else {
			v->area.addr = ptr + (channel * size);
			v->area.first = 0;
			v->area.step = width;
		}
	}
	return frames;
}

ssize_t snd_pcm_plugin_client_channels(snd_pcm_plugin_t *plugin,
				     size_t frames,
				     snd_pcm_plugin_channel_t **channels)
{
	return snd_pcm_plugin_side_channels(plugin, 1, frames, channels);
}

ssize_t snd_pcm_plugin_slave_channels(snd_pcm_plugin_t *plugin,
				    size_t frames,
				    snd_pcm_plugin_channel_t **channels)
{
	return snd_pcm_plugin_side_channels(plugin, 0, frames, channels);
}


int snd_pcm_plugin_build(snd_pcm_plugin_handle_t *handle,
			 int stream,
			 const char *name,
			 snd_pcm_format_t *src_format,
			 snd_pcm_format_t *dst_format,
			 size_t extra,
			 snd_pcm_plugin_t **ret)
{
	snd_pcm_plugin_t *plugin;
	
	assert(handle);
	assert(stream >= 0 && stream <= 1);
	assert(src_format && dst_format);
	plugin = (snd_pcm_plugin_t *)calloc(1, sizeof(*plugin) + extra);
	if (plugin == NULL)
		return -ENOMEM;
	plugin->name = name ? strdup(name) : NULL;
	plugin->handle = handle;
	plugin->stream = stream;
	plugin->src_format = *src_format;
	plugin->src_width = snd_pcm_format_physical_width(src_format->format);
	assert(plugin->src_width > 0);
	plugin->dst_format = *dst_format;
	plugin->dst_width = snd_pcm_format_physical_width(dst_format->format);
	assert(plugin->dst_width > 0);
	plugin->src_channels = calloc(src_format->channels, sizeof(snd_pcm_plugin_channel_t));
	if (plugin->src_channels == NULL) {
		free(plugin);
		return -ENOMEM;
	}
	plugin->dst_channels = calloc(dst_format->channels, sizeof(snd_pcm_plugin_channel_t));
	if (plugin->dst_channels == NULL) {
		free(plugin->src_channels);
		free(plugin);
		return -ENOMEM;
	}
	plugin->src_vmask = bitset_alloc(src_format->channels);
	if (plugin->src_vmask == NULL) {
		free(plugin->src_channels);
		free(plugin->dst_channels);
		free(plugin);
		return -ENOMEM;
	}
	plugin->dst_vmask = bitset_alloc(dst_format->channels);
	if (plugin->dst_vmask == NULL) {
		free(plugin->src_channels);
		free(plugin->dst_channels);
		free(plugin->src_vmask);
		free(plugin);
		return -ENOMEM;
	}
	plugin->client_channels = snd_pcm_plugin_client_channels;
	plugin->src_channels_mask = snd_pcm_plugin_src_channels_mask;
	plugin->dst_channels_mask = snd_pcm_plugin_dst_channels_mask;
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
		free(plugin->src_channels);
		free(plugin->dst_channels);
		free(plugin->src_vmask);
		free(plugin->dst_vmask);
		free(plugin);
	}
	return 0;
}

ssize_t snd_pcm_plug_client_size(snd_pcm_plugin_handle_t *handle, int stream, size_t drv_frames)
{
	snd_pcm_plugin_t *plugin, *plugin_prev, *plugin_next;
	
	assert(handle);
	if (drv_frames == 0)
		return 0;
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		plugin = snd_pcm_plug_last(handle, SND_PCM_STREAM_PLAYBACK);
		while (plugin && drv_frames > 0) {
			plugin_prev = plugin->prev;
			if (plugin->src_frames)
				drv_frames = plugin->src_frames(plugin, drv_frames);
			plugin = plugin_prev;
		}
	} else if (stream == SND_PCM_STREAM_CAPTURE) {
		plugin = snd_pcm_plug_first(handle, SND_PCM_STREAM_CAPTURE);
		while (plugin && drv_frames > 0) {
			plugin_next = plugin->next;
			if (plugin->dst_frames)
				drv_frames = plugin->dst_frames(plugin, drv_frames);
			plugin = plugin_next;
		}
	} else
		assert(0);
	return drv_frames;
}

ssize_t snd_pcm_plug_slave_size(snd_pcm_plugin_handle_t *handle, int stream, size_t clt_frames)
{
	snd_pcm_plugin_t *plugin, *plugin_prev, *plugin_next;
	ssize_t frames;
	
	assert(handle);
	if (clt_frames == 0)
		return 0;
	frames = clt_frames;
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		plugin = snd_pcm_plug_first(handle, SND_PCM_STREAM_PLAYBACK);
		while (plugin && frames > 0) {
			plugin_next = plugin->next;
			if (plugin->dst_frames) {
				frames = plugin->dst_frames(plugin, frames);
				if (frames < 0)
					return frames;
			}
			plugin = plugin_next;
		}
	} else if (stream == SND_PCM_STREAM_CAPTURE) {
		plugin = snd_pcm_plug_last(handle, SND_PCM_STREAM_CAPTURE);
		while (plugin) {
			plugin_prev = plugin->prev;
			if (plugin->src_frames) {
				frames = plugin->src_frames(plugin, frames);
				if (frames < 0)
					return frames;
			}
			plugin = plugin_prev;
		}
	} else
		assert(0);
	return frames;
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

int snd_pcm_plug_slave_params(snd_pcm_stream_params_t *params,
			      snd_pcm_stream_info_t *slave_info,
			      snd_pcm_stream_params_t *slave_params)
{
	*slave_params = *params;
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

	/* channels */
      	if (params->format.channels < slave_info->min_channels ||
      	    params->format.channels > slave_info->max_channels) {
		unsigned int dst_channels = params->format.channels < slave_info->min_channels ?
				 slave_info->min_channels : slave_info->max_channels;
		slave_params->format.channels = dst_channels;
	}

	/* rate */
        if (params->format.rate < slave_info->min_rate ||
            params->format.rate > slave_info->max_rate) {
        	unsigned int dst_rate = params->format.rate < slave_info->min_rate ?
        		       slave_info->min_rate : slave_info->max_rate;
		slave_params->format.rate = dst_rate;
	}

	/* interleave */
	if (!(slave_info->flags & SND_PCM_STREAM_INFO_INTERLEAVE))
		slave_params->format.interleave = 0;
	if (!(slave_info->flags & SND_PCM_STREAM_INFO_NONINTERLEAVE))
		slave_params->format.interleave = 1;
	return 0;
}

int snd_pcm_plug_format(snd_pcm_plugin_handle_t *handle, 
			snd_pcm_stream_params_t *params, 
			snd_pcm_stream_params_t *slave_params)
{
	snd_pcm_stream_params_t tmpparams;
	snd_pcm_stream_params_t dstparams;
	snd_pcm_stream_params_t *srcparams;
	snd_pcm_plugin_t *plugin;
	int err;
	
	switch (params->stream) {
	case SND_PCM_STREAM_PLAYBACK:
		dstparams = *slave_params;
		srcparams = slave_params;
		*srcparams = *params;
		break;
	case SND_PCM_STREAM_CAPTURE:
		dstparams = *params;
		srcparams = params;
		*srcparams = *slave_params;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	tmpparams = *srcparams;
		
	pdprintf("srcparams: interleave=%i, format=%i, rate=%i, channels=%i\n", 
		 srcparams->format.interleave,
		 srcparams->format.format,
		 srcparams->format.rate,
		 srcparams->format.channels);
	pdprintf("dstparams: interleave=%i, format=%i, rate=%i, channels=%i\n", 
		 dstparams.format.interleave,
		 dstparams.format.format,
		 dstparams.format.rate,
		 dstparams.format.channels);

	if (srcparams->format.channels == 1)
		srcparams->format.interleave = dstparams.format.interleave;

	/* Format change (linearization) */
	if ((srcparams->format.format != dstparams.format.format ||
	     srcparams->format.rate != dstparams.format.rate ||
	     srcparams->format.channels != dstparams.format.channels) &&
	    !snd_pcm_format_linear(srcparams->format.format)) {
		if (snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		else
			tmpparams.format.format = SND_PCM_SFMT_S16;
		tmpparams.format.interleave = dstparams.format.interleave;
		switch (srcparams->format.format) {
		case SND_PCM_SFMT_MU_LAW:
			err = snd_pcm_plugin_build_mulaw(handle,
							 params->stream,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
			break;
#ifndef __KERNEL__
		case SND_PCM_SFMT_A_LAW:
			err = snd_pcm_plugin_build_alaw(handle,
							params->stream,
							&srcparams->format,
							&tmpparams.format,
							&plugin);
			break;
		case SND_PCM_SFMT_IMA_ADPCM:
			err = snd_pcm_plugin_build_adpcm(handle,
							 params->stream,
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

	/* channels reduction */
	if (srcparams->format.channels > dstparams.format.channels) {
		int sv = srcparams->format.channels;
		int dv = dstparams.format.channels;
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
		tmpparams.format.channels = dstparams.format.channels;
		tmpparams.format.interleave = dstparams.format.interleave;
		if (srcparams->format.rate == dstparams.format.rate &&
		    snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		err = snd_pcm_plugin_build_route(handle,
						 params->stream,
						 &srcparams->format,
						 &tmpparams.format,
						 ttable,
						 &plugin);
		free(ttable);
		pdprintf("params channels reduction: src=%i, dst=%i returns %i\n", srcparams->format.channels, tmpparams.format.channels, err);
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
		if (srcparams->format.channels == dstparams.format.channels &&
		    snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
        	err = snd_pcm_plugin_build_rate(handle,
						params->stream,
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

	/* channels extension  */
	if (srcparams->format.channels < dstparams.format.channels) {
		int sv = srcparams->format.channels;
		int dv = dstparams.format.channels;
		route_ttable_entry_t *ttable = calloc(1, dv * sv * sizeof(*ttable));
#if 0
		{
			int v;
			for (v = 0; v < sv; ++v)
				ttable[v * sv + v] = FULL;
		}
#else
		{
			/* Playback is spreaded on all channels */
			int vd, vs;
			for (vd = 0, vs = 0; vd < dv; ++vd) {
				ttable[vd * sv + vs] = FULL;
				vs++;
				if (vs == sv)
					vs = 0;
			}
		}
#endif
		tmpparams.format.channels = dstparams.format.channels;
		tmpparams.format.interleave = dstparams.format.interleave;
		if (snd_pcm_format_linear(dstparams.format.format))
			tmpparams.format.format = dstparams.format.format;
		err = snd_pcm_plugin_build_route(handle,
						 params->stream,
						 &srcparams->format,
						 &tmpparams.format,
						 ttable,
						 &plugin);
		free(ttable);
		pdprintf("params channels extension: src=%i, dst=%i returns %i\n", srcparams->format.channels, tmpparams.format.channels, err);
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
							 params->stream,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
		}
#ifndef __KERNEL__
		else if (tmpparams.format.format == SND_PCM_SFMT_A_LAW) {
			err = snd_pcm_plugin_build_alaw(handle,
							params->stream,
							&srcparams->format,
							&tmpparams.format,
							&plugin);
		}
		else if (tmpparams.format.format == SND_PCM_SFMT_IMA_ADPCM) {
			err = snd_pcm_plugin_build_adpcm(handle,
							 params->stream,
							 &srcparams->format,
							 &tmpparams.format,
							 &plugin);
		}
#endif
		else if (snd_pcm_format_linear(srcparams->format.format) &&
			 snd_pcm_format_linear(tmpparams.format.format)) {
			err = snd_pcm_plugin_build_linear(handle,
							  params->stream,
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
						params->stream,
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

ssize_t snd_pcm_plug_client_channels_buf(snd_pcm_plugin_handle_t *handle,
					 int stream,
					 char *buf,
					 size_t count,
					 snd_pcm_plugin_channel_t **channels)
{
	snd_pcm_plugin_t *plugin;
	snd_pcm_plugin_channel_t *v;
	snd_pcm_format_t *format;
	int width, nchannels, channel;

	assert(buf);
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		plugin = snd_pcm_plug_first(handle, stream);
		format = &plugin->src_format;
		v = plugin->src_channels;
	}
	else {
		plugin = snd_pcm_plug_last(handle, stream);
		format = &plugin->dst_format;
		v = plugin->dst_channels;
	}
	*channels = v;
	if ((width = snd_pcm_format_physical_width(format->format)) < 0)
		return width;
	nchannels = format->channels;
	assert(format->interleave || format->channels == 1);
	for (channel = 0; channel < nchannels; channel++, v++) {
		v->enabled = 1;
		v->wanted = (stream == SND_PCM_STREAM_CAPTURE);
		v->aptr = NULL;
		v->area.addr = buf;
		v->area.first = channel * width;
		v->area.step = nchannels * width;
	}
	return count;
}

ssize_t snd_pcm_plug_client_channels_iovec(snd_pcm_plugin_handle_t *handle,
					   int stream,
					   const struct iovec *vector,
					   unsigned long count,
					   snd_pcm_plugin_channel_t **channels)
{
	snd_pcm_plugin_t *plugin;
	snd_pcm_plugin_channel_t *v;
	snd_pcm_format_t *format;
	int width;
	unsigned int nchannels, channel;

	if (stream == SND_PCM_STREAM_PLAYBACK) {
		plugin = snd_pcm_plug_first(handle, stream);
		format = &plugin->src_format;
		v = plugin->src_channels;
	}
	else {
		plugin = snd_pcm_plug_last(handle, stream);
		format = &plugin->dst_format;
		v = plugin->dst_channels;
	}
	*channels = v;
	if ((width = snd_pcm_format_physical_width(format->format)) < 0)
		return width;
	nchannels = format->channels;
	if (format->interleave) {
		assert(count == 1 && vector->iov_base);
		
		for (channel = 0; channel < nchannels; channel++, v++) {
			v->enabled = 1;
			v->wanted = (stream == SND_PCM_STREAM_CAPTURE);
			v->aptr = NULL;
			v->area.addr = vector->iov_base;
			v->area.first = channel * width;
			v->area.step = nchannels * width;
		}
		return vector->iov_len;
	} else {
		size_t len;
		assert(count == nchannels);
		len = vector->iov_len;
		for (channel = 0; channel < nchannels; channel++, v++, vector++) {
			assert(vector->iov_len == len);
			v->enabled = (vector->iov_base != NULL);
			v->wanted = (v->enabled && (stream == SND_PCM_STREAM_CAPTURE));
			v->aptr = NULL;
			v->area.addr = vector->iov_base;
			v->area.first = 0;
			v->area.step = width;
		}
		return len;
	}
}

int snd_pcm_plug_playback_channels_mask(snd_pcm_plugin_handle_t *handle,
					bitset_t *client_vmask)
{
#ifndef __KERNEL__
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &handle->private;
#endif
	snd_pcm_plugin_t *plugin = snd_pcm_plug_last(handle, SND_PCM_STREAM_PLAYBACK);
	if (plugin == NULL) {
#ifndef __KERNEL__
		return snd_pcm_channels_mask(plug->slave, SND_PCM_STREAM_PLAYBACK, client_vmask);
#else
		return 0;
#endif
	} else {
		int schannels = plugin->dst_format.channels;
		bitset_t bs[bitset_size(schannels)];
		bitset_t *srcmask;
		bitset_t *dstmask = bs;
		int err;
		bitset_one(dstmask, schannels);
#ifndef __KERNEL__
		err = snd_pcm_channels_mask(plug->slave, SND_PCM_STREAM_PLAYBACK, dstmask);
		if (err < 0)
			return err;
#endif
		if (plugin == NULL) {
			bitset_and(client_vmask, dstmask, schannels);
			return 0;
		}
		while (1) {
			err = plugin->src_channels_mask(plugin, dstmask, &srcmask);
			if (err < 0)
				return err;
			dstmask = srcmask;
			if (plugin->prev == NULL)
				break;
			plugin = plugin->prev;
		}
		bitset_and(client_vmask, dstmask, plugin->src_format.channels);
		return 0;
	}
}

int snd_pcm_plug_capture_channels_mask(snd_pcm_plugin_handle_t *handle,
				       bitset_t *client_vmask)
{
#ifndef __KERNEL__
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &handle->private;
#endif
	snd_pcm_plugin_t *plugin = snd_pcm_plug_first(handle, SND_PCM_STREAM_CAPTURE);
	if (plugin == NULL) {
#ifndef __KERNEL__
		return snd_pcm_channels_mask(plug->slave, SND_PCM_STREAM_CAPTURE, client_vmask);
#else
		return 0;
#endif
	} else {
		int schannels = plugin->src_format.channels;
		bitset_t bs[bitset_size(schannels)];
		bitset_t *srcmask = bs;
		bitset_t *dstmask;
		int err;
		bitset_one(srcmask, schannels);
#ifndef __KERNEL__
		err = snd_pcm_channels_mask(plug->slave, SND_PCM_STREAM_CAPTURE, srcmask);
		if (err < 0)
			return err;
#endif
		while (1) {
			err = plugin->dst_channels_mask(plugin, srcmask, &dstmask);
			if (err < 0)
				return err;
			srcmask = dstmask;
			if (plugin->next == NULL)
				break;
			plugin = plugin->next;
		}
		bitset_and(client_vmask, srcmask, plugin->dst_format.channels);
		return 0;
	}
}

static int snd_pcm_plug_playback_disable_useless_channels(snd_pcm_plugin_handle_t *handle,
							  snd_pcm_plugin_channel_t *src_channels)
{
	snd_pcm_plugin_t *plugin = snd_pcm_plug_first(handle, SND_PCM_STREAM_PLAYBACK);
	unsigned int nchannels = plugin->src_format.channels;
	bitset_t bs[bitset_size(nchannels)];
	bitset_t *srcmask = bs;
	int err;
	unsigned int channel;
	for (channel = 0; channel < nchannels; channel++) {
		if (src_channels[channel].enabled)
			bitset_set(srcmask, channel);
		else
			bitset_reset(srcmask, channel);
	}
	err = snd_pcm_plug_playback_channels_mask(handle, srcmask);
	if (err < 0)
		return err;
	for (channel = 0; channel < nchannels; channel++) {
		if (!bitset_get(srcmask, channel))
			src_channels[channel].enabled = 0;
	}
	return 0;
}

static int snd_pcm_plug_capture_disable_useless_channels(snd_pcm_plugin_handle_t *handle,
							 snd_pcm_plugin_channel_t *src_channels,
							 snd_pcm_plugin_channel_t *client_channels)
{
#ifndef __KERNEL__
	snd_pcm_plug_t *plug = (snd_pcm_plug_t*) &handle->private;
#endif
	snd_pcm_plugin_t *plugin = snd_pcm_plug_last(handle, SND_PCM_STREAM_CAPTURE);
	unsigned int nchannels = plugin->dst_format.channels;
	bitset_t bs[bitset_size(nchannels)];
	bitset_t *dstmask = bs;
	bitset_t *srcmask;
	int err;
	unsigned int channel;
	for (channel = 0; channel < nchannels; channel++) {
		if (client_channels[channel].enabled)
			bitset_set(dstmask, channel);
		else
			bitset_reset(dstmask, channel);
	}
	while (plugin) {
		err = plugin->src_channels_mask(plugin, dstmask, &srcmask);
		if (err < 0)
			return err;
		dstmask = srcmask;
		plugin = plugin->prev;
	}
#ifndef __KERNEL__
	err = snd_pcm_channels_mask(plug->slave, SND_PCM_STREAM_CAPTURE, dstmask);
	if (err < 0)
		return err;
#endif
	plugin = snd_pcm_plug_first(handle, SND_PCM_STREAM_CAPTURE);
	nchannels = plugin->src_format.channels;
	for (channel = 0; channel < nchannels; channel++) {
		if (!bitset_get(dstmask, channel))
			src_channels[channel].enabled = 0;
	}
	return 0;
}

ssize_t snd_pcm_plug_write_transfer(snd_pcm_plugin_handle_t *handle, snd_pcm_plugin_channel_t *src_channels, size_t size)
{
	snd_pcm_plugin_t *plugin, *next;
	snd_pcm_plugin_channel_t *dst_channels;
	int err;
	ssize_t frames = size;

	if ((err = snd_pcm_plug_playback_disable_useless_channels(handle, src_channels)) < 0)
		return err;
	
	plugin = snd_pcm_plug_first(handle, SND_PCM_STREAM_PLAYBACK);
	while (plugin && frames > 0) {
		if ((next = plugin->next) != NULL) {
			ssize_t frames1 = frames;
			if (plugin->dst_frames)
				frames1 = plugin->dst_frames(plugin, frames);
			if ((err = next->client_channels(next, frames1, &dst_channels)) < 0) {
				snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_PLAYBACK, src_channels->aptr);
				return err;
			}
			if (err != frames1) {
				frames = err;
				if (plugin->src_frames)
					frames = plugin->src_frames(plugin, frames1);
			}
		} else {
			if ((err = snd_pcm_plugin_slave_channels(plugin, frames, &dst_channels)) < 0)
				return err;
		}
		pdprintf("write plugin: %s, %i\n", plugin->name, frames);
		if ((frames = plugin->transfer(plugin, src_channels, dst_channels, frames)) < 0) {
			snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_PLAYBACK, src_channels->aptr);
			snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_PLAYBACK, dst_channels->aptr);
			return frames;
		}
		snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_PLAYBACK, src_channels->aptr);
		src_channels = dst_channels;
		plugin = next;
	}
	snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_PLAYBACK, src_channels->aptr);
	return snd_pcm_plug_client_size(handle, SND_PCM_STREAM_PLAYBACK, frames);
}

ssize_t snd_pcm_plug_read_transfer(snd_pcm_plugin_handle_t *handle, snd_pcm_plugin_channel_t *dst_channels_final, size_t size)
{
	snd_pcm_plugin_t *plugin, *next;
	snd_pcm_plugin_channel_t *src_channels, *dst_channels;
	ssize_t frames = size;
	int err;

	plugin = snd_pcm_plug_last(handle, SND_PCM_STREAM_CAPTURE);
	frames = snd_pcm_plug_slave_size(handle, SND_PCM_STREAM_CAPTURE, frames);
	if (frames < 0)
		return frames;

	plugin = snd_pcm_plug_first(handle, SND_PCM_STREAM_CAPTURE);
	if ((err = snd_pcm_plugin_slave_channels(plugin, frames, &src_channels)) < 0)
		return err;
	if ((err = snd_pcm_plug_capture_disable_useless_channels(handle, src_channels, dst_channels_final) < 0))
		return err;
	
	while (plugin && frames > 0) {
		if ((next = plugin->next) != NULL) {
			if ((err = plugin->client_channels(plugin, frames, &dst_channels)) < 0) {
				snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_CAPTURE, src_channels->aptr);
				return err;
			}
			frames = err;
		} else {
			dst_channels = dst_channels_final;
		}
		pdprintf("read plugin: %s, %i\n", plugin->name, frames);
		if ((frames = plugin->transfer(plugin, src_channels, dst_channels, frames)) < 0) {
			snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_CAPTURE, src_channels->aptr);
			snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_CAPTURE, dst_channels->aptr);
			return frames;
		}
#if 0
		{
		  unsigned int channel;
		  for (channel = 0; channel < plugin->src_format.channels; ++channel) {
		    fprintf(stderr, "%d%d ", src_channels[channel].enabled, src_channels[channel].wanted);
		  }
		  fprintf(stderr, " -> ");
		  for (channel = 0; channel < plugin->dst_format.channels; ++channel) {
		    fprintf(stderr, "%d%d ", dst_channels[channel].enabled, dst_channels[channel].wanted);
		  }
		  fprintf(stderr, "\n");
		}
#endif
		snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_CAPTURE, src_channels->aptr);
		plugin = next;
		src_channels = dst_channels;
	}
	snd_pcm_plug_buf_unlock(handle, SND_PCM_STREAM_CAPTURE, src_channels->aptr);
	return frames;
}

int snd_pcm_area_silence(const snd_pcm_channel_area_t *dst_area, size_t dst_offset,
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
		assert(0);
	}
	return 0;
}

int snd_pcm_areas_silence(const snd_pcm_channel_area_t *dst_areas, size_t dst_offset,
			  size_t vcount, size_t frames, int format)
{
	int width = snd_pcm_format_physical_width(format);
	while (vcount > 0) {
		void *addr = dst_areas->addr;
		unsigned int step = dst_areas->step;
		const snd_pcm_channel_area_t *begin = dst_areas;
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
			snd_pcm_channel_area_t d;
			d.addr = begin->addr;
			d.first = begin->first;
			d.step = width;
			err = snd_pcm_area_silence(&d, dst_offset * v, frames * v, format);
			vcount -= v;
		} else {
			err = snd_pcm_area_silence(begin, dst_offset, frames, format);
			dst_areas = begin + 1;
			vcount--;
		}
		if (err < 0)
			return err;
	}
	return 0;
}


int snd_pcm_area_copy(const snd_pcm_channel_area_t *src_area, size_t src_offset,
		      const snd_pcm_channel_area_t *dst_area, size_t dst_offset,
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
		assert(0);
	}
	return 0;
}

int snd_pcm_areas_copy(const snd_pcm_channel_area_t *src_areas, size_t src_offset,
		       const snd_pcm_channel_area_t *dst_areas, size_t dst_offset,
		       size_t vcount, size_t frames, int format)
{
	int width = snd_pcm_format_physical_width(format);
	while (vcount > 0) {
		unsigned int step = src_areas->step;
		void *src_addr = src_areas->addr;
		const snd_pcm_channel_area_t *src_start = src_areas;
		void *dst_addr = dst_areas->addr;
		const snd_pcm_channel_area_t *dst_start = dst_areas;
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
			snd_pcm_channel_area_t s, d;
			s.addr = src_start->addr;
			s.first = src_start->first;
			s.step = width;
			d.addr = dst_start->addr;
			d.first = dst_start->first;
			d.step = width;
			snd_pcm_area_copy(&s, src_offset * v, &d, dst_offset * v, frames * v, format);
			vcount -= v;
		} else {
			snd_pcm_area_copy(src_start, src_offset, dst_start, dst_offset, frames, format);
			src_areas = src_start + 1;
			dst_areas = dst_start + 1;
			vcount--;
		}
	}
	return 0;
}
