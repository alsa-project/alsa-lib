/*
 *  Rate conversion Plug-In
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
  
#ifdef __KERNEL__
#include "../../include/driver.h"
#include "../../include/pcm.h"
#include "../../include/pcm_plugin.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include "../pcm_local.h"
#endif

#define SHIFT	11
#define BITS	(1<<SHIFT)
#define MASK	(BITS-1)

/*
 *  Basic rate conversion plugin
 */

typedef struct {
	signed short last_S1;
	signed short last_S2;
} rate_channel_t;
 
typedef void (*rate_f)(snd_pcm_plugin_t *plugin,
		       const snd_pcm_plugin_channel_t *src_channels,
		       snd_pcm_plugin_channel_t *dst_channels,
		       int src_frames, int dst_frames);

typedef struct rate_private_data {
	unsigned int pitch;
	unsigned int pos;
	rate_f func;
	int get, put;
	ssize_t old_src_frames, old_dst_frames;
	rate_channel_t channels[0];
} rate_t;

static void rate_init(snd_pcm_plugin_t *plugin)
{
	unsigned int channel;
	rate_t *data = (rate_t *)plugin->extra_data;
	data->pos = 0;
	for (channel = 0; channel < plugin->src_format.channels; channel++) {
		data->channels[channel].last_S1 = 0;
		data->channels[channel].last_S2 = 0;
	}
}

static void resample_expand(snd_pcm_plugin_t *plugin,
			    const snd_pcm_plugin_channel_t *src_channels,
			    snd_pcm_plugin_channel_t *dst_channels,
			    int src_frames, int dst_frames)
{
	unsigned int pos = 0;
	signed int val;
	signed short S1, S2;
	char *src, *dst;
	unsigned int channel;
	int src_step, dst_step;
	int src_frames1, dst_frames1;
	rate_t *data = (rate_t *)plugin->extra_data;
	rate_channel_t *rchannels = data->channels;

#define GET_S16_LABELS
#define PUT_S16_LABELS
#include "plugin_ops.h"
#undef GET_S16_LABELS
#undef PUT_S16_LABELS
	void *get = get_s16_labels[data->get];
	void *put = put_s16_labels[data->put];
	void *get_s16_end = 0;
	signed short sample = 0;
#define GET_S16_END *get_s16_end
#include "plugin_ops.h"
#undef GET_S16_END
	
	for (channel = 0; channel < plugin->src_format.channels; channel++, rchannels++) {
		pos = data->pos;
		S1 = rchannels->last_S1;
		S2 = rchannels->last_S2;
		if (!src_channels[channel].enabled) {
			if (dst_channels[channel].wanted)
				snd_pcm_area_silence(&dst_channels[channel].area, 0, dst_frames, plugin->dst_format.format);
			dst_channels[channel].enabled = 0;
			continue;
		}
		dst_channels[channel].enabled = 1;
		src = (char *)src_channels[channel].area.addr + src_channels[channel].area.first / 8;
		dst = (char *)dst_channels[channel].area.addr + dst_channels[channel].area.first / 8;
		src_step = src_channels[channel].area.step / 8;
		dst_step = dst_channels[channel].area.step / 8;
		src_frames1 = src_frames;
		dst_frames1 = dst_frames;
		if (pos & ~MASK) {
			get_s16_end = &&after_get1;
			goto *get;
		after_get1:
			pos &= MASK;
			S1 = S2;
			S2 = sample;
			src += src_step;
			src_frames--;
		}
		while (dst_frames1-- > 0) {
			if (pos & ~MASK) {
				pos &= MASK;
				S1 = S2;
				if (src_frames1-- > 0) {
					get_s16_end = &&after_get2;
					goto *get;
				after_get2:
					S2 = sample;
					src += src_step;
				}
			}
			val = S1 + ((S2 - S1) * (signed int)pos) / BITS;
			if (val < -32768)
				val = -32768;
			else if (val > 32767)
				val = 32767;
			sample = val;
			goto *put;
#define PUT_S16_END after_put
#include "plugin_ops.h"
#undef PUT_S16_END
		after_put:
			dst += dst_step;
			pos += data->pitch;
		}
		rchannels->last_S1 = S1;
		rchannels->last_S2 = S2;
		rchannels++;
	}
	data->pos = pos;
}

static void resample_shrink(snd_pcm_plugin_t *plugin,
			    const snd_pcm_plugin_channel_t *src_channels,
			    snd_pcm_plugin_channel_t *dst_channels,
			    int src_frames, int dst_frames)
{
	unsigned int pos = 0;
	signed int val;
	signed short S1, S2;
	char *src, *dst;
	unsigned int channel;
	int src_step, dst_step;
	int src_frames1, dst_frames1;
	rate_t *data = (rate_t *)plugin->extra_data;
	rate_channel_t *rchannels = data->channels;
	
#define GET_S16_LABELS
#define PUT_S16_LABELS
#include "plugin_ops.h"
#undef GET_S16_LABELS
#undef PUT_S16_LABELS
	void *get = get_s16_labels[data->get];
	void *put = put_s16_labels[data->put];
	signed short sample = 0;

	for (channel = 0; channel < plugin->src_format.channels; ++channel) {
		pos = data->pos;
		S1 = rchannels->last_S1;
		S2 = rchannels->last_S2;
		if (!src_channels[channel].enabled) {
			if (dst_channels[channel].wanted)
				snd_pcm_area_silence(&dst_channels[channel].area, 0, dst_frames, plugin->dst_format.format);
			dst_channels[channel].enabled = 0;
			continue;
		}
		dst_channels[channel].enabled = 1;
		src = (char *)src_channels[channel].area.addr + src_channels[channel].area.first / 8;
		dst = (char *)dst_channels[channel].area.addr + dst_channels[channel].area.first / 8;
		src_step = src_channels[channel].area.step / 8;
		dst_step = dst_channels[channel].area.step / 8;
		src_frames1 = src_frames;
		dst_frames1 = dst_frames;
		while (dst_frames1 > 0) {
			S1 = S2;
			if (src_frames1-- > 0) {
				goto *get;
#define GET_S16_END after_get
#include "plugin_ops.h"
#undef GET_S16_END
			after_get:
				S2 = sample;
				src += src_step;
			}
			if (pos & ~MASK) {
				pos &= MASK;
				val = S1 + ((S2 - S1) * (signed int)pos) / BITS;
				if (val < -32768)
					val = -32768;
				else if (val > 32767)
					val = 32767;
				sample = val;
				goto *put;
#define PUT_S16_END after_put
#include "plugin_ops.h"
#undef PUT_S16_END
			after_put:
				dst += dst_step;
				dst_frames1--;
			}
			pos += data->pitch;
		}
		rchannels->last_S1 = S1;
		rchannels->last_S2 = S2;
		rchannels++;
	}
	data->pos = pos;
}

static ssize_t rate_src_frames(snd_pcm_plugin_t *plugin, size_t frames)
{
	rate_t *data;
	ssize_t res;

	assert(plugin);
	if (frames == 0)
		return 0;
	data = (rate_t *)plugin->extra_data;
	if (plugin->src_format.rate < plugin->dst_format.rate) {
		res = (((frames * data->pitch) + (BITS/2)) >> SHIFT);
	} else {
		res = (((frames << SHIFT) + (data->pitch / 2)) / data->pitch);		
	}
	if (data->old_src_frames > 0) {
		ssize_t frames1 = frames, res1 = data->old_dst_frames;
		while (data->old_src_frames < frames1) {
			frames1 >>= 1;
			res1 <<= 1;
		}
		while (data->old_src_frames > frames1) {
			frames1 <<= 1;
			res1 >>= 1;
		}
		if (data->old_src_frames == frames1)
			return res1;
	}
	data->old_src_frames = frames;
	data->old_dst_frames = res;
	return res;
}

static ssize_t rate_dst_frames(snd_pcm_plugin_t *plugin, size_t frames)
{
	rate_t *data;
	ssize_t res;

	assert(plugin);
	if (frames == 0)
		return 0;
	data = (rate_t *)plugin->extra_data;
	if (plugin->src_format.rate < plugin->dst_format.rate) {
		res = (((frames << SHIFT) + (data->pitch / 2)) / data->pitch);
	} else {
		res = (((frames * data->pitch) + (BITS/2)) >> SHIFT);
	}
	if (data->old_dst_frames > 0) {
		ssize_t frames1 = frames, res1 = data->old_src_frames;
		while (data->old_dst_frames < frames1) {
			frames1 >>= 1;
			res1 <<= 1;
		}
		while (data->old_dst_frames > frames1) {
			frames1 <<= 1;
			res1 >>= 1;
		}
		if (data->old_dst_frames == frames1)
			return res1;
	}
	data->old_dst_frames = frames;
	data->old_src_frames = res;
	return res;
}

static ssize_t rate_transfer(snd_pcm_plugin_t *plugin,
			     const snd_pcm_plugin_channel_t *src_channels,
			     snd_pcm_plugin_channel_t *dst_channels,
			     size_t frames)
{
	size_t dst_frames;
	unsigned int channel;
	rate_t *data;

	assert(plugin && src_channels && dst_channels);
	if (frames == 0)
		return 0;
	for (channel = 0; channel < plugin->src_format.channels; channel++) {
		assert(src_channels[channel].area.first % 8 == 0 &&
		       src_channels[channel].area.step % 8 == 0);
		assert(dst_channels[channel].area.first % 8 == 0 &&
		       dst_channels[channel].area.step % 8 == 0);
	}

	dst_frames = rate_dst_frames(plugin, frames);
	data = (rate_t *)plugin->extra_data;
	data->func(plugin, src_channels, dst_channels, frames, dst_frames);
	return dst_frames;
}

static int rate_action(snd_pcm_plugin_t *plugin,
		       snd_pcm_plugin_action_t action,
		       unsigned long udata UNUSED)
{
	assert(plugin);
	switch (action) {
	case INIT:
	case PREPARE:
	case DRAIN:
	case FLUSH:
		rate_init(plugin);
		break;
	default:
		break;
	}
	return 0;	/* silenty ignore other actions */
}

int snd_pcm_plugin_build_rate(snd_pcm_plugin_handle_t *handle,
			      int stream,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin)
{
	int err;
	rate_t *data;
	snd_pcm_plugin_t *plugin;

	assert(r_plugin);
	*r_plugin = NULL;

	assert(src_format->channels == dst_format->channels);
	assert(src_format->channels > 0);
	assert(snd_pcm_format_linear(src_format->format) > 0);
	assert(snd_pcm_format_linear(dst_format->format) > 0);
	assert(src_format->rate != dst_format->rate);

	err = snd_pcm_plugin_build(handle, stream,
				   "rate conversion",
				   src_format,
				   dst_format,
				   sizeof(rate_t) + src_format->channels * sizeof(rate_channel_t),
				   &plugin);
	if (err < 0)
		return err;
	data = (rate_t *)plugin->extra_data;
	data->get = getput_index(src_format->format);
	data->put = getput_index(dst_format->format);

	if (src_format->rate < dst_format->rate) {
		data->pitch = ((src_format->rate << SHIFT) + (dst_format->rate >> 1)) / dst_format->rate;
		data->func = resample_expand;
	} else {
		data->pitch = ((dst_format->rate << SHIFT) + (src_format->rate >> 1)) / src_format->rate;
		data->func = resample_shrink;
	}
	data->pos = 0;
	rate_init(plugin);
	data->old_src_frames = data->old_dst_frames = 0;
	plugin->transfer = rate_transfer;
	plugin->src_frames = rate_src_frames;
	plugin->dst_frames = rate_dst_frames;
	plugin->action = rate_action;
	*r_plugin = plugin;
	return 0;
}
