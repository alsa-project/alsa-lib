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
#include <endian.h>
#include <byteswap.h>
#include "../pcm_local.h"
#endif

#define SHIFT	11
#define BITS	(1<<SHIFT)
#define MASK	(BITS-1)

/*
 *  Basic rate conversion plugin
 */

#define rate_voices(data) ((rate_voice_t *)((char *)data + sizeof(*data)))

typedef signed short (*take_sample_f)(void *ptr);
typedef void (*put_sample_f)(void *ptr, signed int val);
 
typedef struct {
	signed short last_S1;
	signed short last_S2;
} rate_voice_t;
 
typedef struct rate_private_data {
	snd_pcm_plugin_t *plugin;
	take_sample_f take;
	put_sample_f put;
	unsigned int pitch;
	unsigned int pos;
	ssize_t old_src_samples, old_dst_samples;
} rate_t;

static void rate_init(snd_pcm_plugin_t *plugin, rate_t *data)
{
	int voice;
	rate_voice_t *rvoices = rate_voices(data);

	data->pos = 0;
	for (voice = 0; plugin->src_format.voices; voice++) {
		rvoices[voice].last_S1 = 0;
		rvoices[voice].last_S2 = 0;
	}
}

#define RATE_TAKE_SAMPLE(name, type, val) \
static signed short rate_take_sample_##name(void *ptr) \
{ \
	signed int smp = *(type *)ptr; \
	return val; \
}

#define RATE_PUT_SAMPLE(name, type, val) \
static void rate_put_sample_##name(void *ptr, signed int smp) \
{ \
	*(type *)ptr = val; \
}

static void resample_expand(snd_pcm_plugin_t *plugin,
			    const snd_pcm_plugin_voice_t *src_voices,
			    const snd_pcm_plugin_voice_t *dst_voices,
			    int src_samples, int dst_samples)
{
	unsigned int pos;
	signed int val;
	signed short S1, S2;
	char *src, *dst;
	int voice;
	int src_step, dst_step;
	int src_samples1, dst_samples1;
	rate_t *data = (rate_t *)plugin->extra_data;
	rate_voice_t *rvoices = rate_voices(data);
	
	for (voice = 0; voice < plugin->src_format.voices; voice++, rvoices++) {
		pos = data->pos;
		S1 = rvoices->last_S1;
		S2 = rvoices->last_S2;
		if (src_voices[voice].addr == NULL)
			continue;
		src = (char *)src_voices[voice].addr + src_voices[voice].offset / 8;
		dst = (char *)dst_voices[voice].addr + src_voices[voice].offset / 8;
		src_step = src_voices[voice].next / 8;
		dst_step = dst_voices[voice].next / 8;
		src_samples1 = src_samples;
		dst_samples1 = dst_samples;
		if (pos & ~MASK) {
			pos &= MASK;
			S1 = S2;
			S2 = data->take(src);
			src += src_step;
			src_samples--;
		}
		while (dst_samples1-- > 0) {
			if (pos & ~MASK) {
				pos &= MASK;
				S1 = S2;
				if (src_samples1-- > 0) {
					S2 = data->take(src);
					src += src_step;
				}
			}
			val = S1 + ((S2 - S1) * (signed int)pos) / BITS;
			if (val < -32768)
				val = -32768;
			else if (val > 32767)
				val = 32767;
			data->put(dst, val);
			dst += dst_step;
			pos += data->pitch;
		}
		rvoices->last_S1 = S1;
		rvoices->last_S2 = S2;
		data->pos = pos;
	}
}

static void resample_shrink(snd_pcm_plugin_t *plugin,
			    const snd_pcm_plugin_voice_t *src_voices,
			    const snd_pcm_plugin_voice_t *dst_voices,
			    int src_samples, int dst_samples)
{
	unsigned int pos;
	signed int val;
	signed short S1, S2;
	char *src, *dst;
	int voice;
	int src_step, dst_step;
	int src_samples1, dst_samples1;
	rate_t *data = (rate_t *)plugin->extra_data;
	rate_voice_t *rvoices = rate_voices(data);
	
	for (voice = 0; voice < plugin->src_format.voices; ++voice) {
		pos = data->pos;
		S1 = rvoices->last_S1;
		S2 = rvoices->last_S2;
		if (src_voices[voice].addr == NULL)
			continue;
		src = (char *)src_voices[voice].addr + src_voices[voice].offset / 8;
		dst = (char *)dst_voices[voice].addr + src_voices[voice].offset / 8;
		src_step = src_voices[voice].next / 8;
		dst_step = dst_voices[voice].next / 8;
		src_samples1 = src_samples;
		dst_samples1 = dst_samples;
		while (dst_samples1 > 0) {
			S1 = S2;
			if (src_samples1-- > 0) {
				S2 = data->take(src);
				src += src_step;
			}
			if (pos & ~MASK) {
				pos &= MASK;
				val = S1 + ((S2 - S1) * (signed int)pos) / BITS;
				if (val < -32768)
					val = -32768;
				else if (val > 32767)
					val = 32767;
				data->put(dst, val);
				dst += dst_step;
				dst_samples1--;
			}
			pos += data->pitch;
		}
		rvoices->last_S1 = S1;
		rvoices->last_S2 = S2;
		data->pos = pos;
	}
}

static ssize_t rate_src_samples(snd_pcm_plugin_t *plugin, size_t samples)
{
	rate_t *data;
	ssize_t res;

	if (plugin == NULL || samples <= 0)
		return -EINVAL;
	data = (rate_t *)plugin->extra_data;
	if (plugin->src_format.rate < plugin->dst_format.rate) {
		res = (((samples * data->pitch) + (BITS/2)) >> SHIFT);
	} else {
		res = (((samples << SHIFT) + (data->pitch / 2)) / data->pitch);		
	}
	if (data->old_src_samples > 0) {
		ssize_t samples1 = samples, res1 = data->old_dst_samples;
		while (data->old_src_samples < samples1) {
			samples1 >>= 1;
			res1 <<= 1;
		}
		while (data->old_src_samples > samples1) {
			samples1 <<= 1;
			res1 >>= 1;
		}
		if (data->old_src_samples == samples1)
			return res1;
	}
	data->old_src_samples = samples;
	data->old_dst_samples = res;
	return res;
}

static ssize_t rate_dst_samples(snd_pcm_plugin_t *plugin, size_t samples)
{
	rate_t *data;
	ssize_t res;

	if (plugin == NULL || samples <= 0)
		return -EINVAL;
	data = (rate_t *)plugin->extra_data;
	if (plugin->src_format.rate < plugin->dst_format.rate) {
		res = (((samples << SHIFT) + (data->pitch / 2)) / data->pitch);
	} else {
		res = (((samples * data->pitch) + (BITS/2)) >> SHIFT);
	}
	if (data->old_dst_samples > 0) {
		ssize_t samples1 = samples, res1 = data->old_src_samples;
		while (data->old_dst_samples < samples1) {
			samples1 >>= 1;
			res1 <<= 1;
		}
		while (data->old_dst_samples > samples1) {
			samples1 <<= 1;
			res1 >>= 1;
		}
		if (data->old_dst_samples == samples1)
			return res1;
	}
	data->old_dst_samples = samples;
	data->old_src_samples = res;
	return res;
}

static ssize_t rate_transfer(snd_pcm_plugin_t *plugin,
			     const snd_pcm_plugin_voice_t *src_voices,
			     const snd_pcm_plugin_voice_t *dst_voices,
			     size_t samples)
{
	size_t dst_samples;

	if (plugin == NULL || src_voices == NULL || src_voices == NULL || samples < 0)
		return -EINVAL;
	if (samples == 0)
		return 0;
	dst_samples = rate_dst_samples(plugin, samples);
	if (plugin->src_format.rate < plugin->dst_format.rate) {
		resample_expand(plugin, src_voices, dst_voices, samples, dst_samples);
	} else {
		resample_shrink(plugin, src_voices, dst_voices, samples, dst_samples);
	}
	return dst_samples;
}

static int rate_action(snd_pcm_plugin_t *plugin,
		       snd_pcm_plugin_action_t action,
		       unsigned long udata)
{
	rate_t *data;

	if (plugin == NULL)
		return -EINVAL;
	data = (rate_t *)plugin->extra_data;
	switch (action) {
	case INIT:
	case PREPARE:
	case DRAIN:
	case FLUSH:
		rate_init(plugin, data);
		break;
	}
	return 0;	/* silenty ignore other actions */
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define my_little_swap16(x) (x)
#define my_little_swap32(x) (x)
#define my_big_swap16(x) bswap_16(x)
#define my_big_swap32(x) bswap_32(x)
#else
#define my_little_swap16(x) bswap_16(x)
#define my_little_swap32(x) bswap_32(x)
#define my_big_swap16(x) (x)
#define my_big_swap32(x) (x)
#endif

RATE_TAKE_SAMPLE(s8, int8_t, smp << 8)
RATE_TAKE_SAMPLE(u8, int8_t, (smp << 8) ^ 0x8000)
RATE_TAKE_SAMPLE(s16_le, int16_t, my_little_swap16(smp))
RATE_TAKE_SAMPLE(s16_be, int16_t, my_big_swap16(smp))
RATE_TAKE_SAMPLE(u16_le, int16_t, my_little_swap16(smp) ^ 0x8000)
RATE_TAKE_SAMPLE(u16_be, int16_t, my_big_swap16(smp) ^ 0x8000)
RATE_TAKE_SAMPLE(s24_le, int32_t, my_little_swap32(smp) >> 8)
RATE_TAKE_SAMPLE(s24_be, int32_t, my_big_swap32(smp) >> 8)
RATE_TAKE_SAMPLE(u24_le, int32_t, (my_little_swap32(smp) >> 8) ^ 0x8000)
RATE_TAKE_SAMPLE(u24_be, int32_t, (my_big_swap32(smp) >> 8) ^ 0x8000)
RATE_TAKE_SAMPLE(s32_le, int32_t, my_little_swap32(smp) >> 16)
RATE_TAKE_SAMPLE(s32_be, int32_t, my_big_swap32(smp) >> 16)
RATE_TAKE_SAMPLE(u32_le, int32_t, (my_little_swap32(smp) >> 16) ^ 0x8000)
RATE_TAKE_SAMPLE(u32_be, int32_t, (my_big_swap32(smp) >> 16) ^ 0x8000)

static take_sample_f rate_take_sample[] = {
	[SND_PCM_SFMT_S8]	rate_take_sample_s8,
	[SND_PCM_SFMT_U8]	rate_take_sample_u8,
	[SND_PCM_SFMT_S16_LE]	rate_take_sample_s16_le,
	[SND_PCM_SFMT_S16_BE]	rate_take_sample_s16_be,
	[SND_PCM_SFMT_U16_LE]	rate_take_sample_u16_le,
	[SND_PCM_SFMT_U16_BE]	rate_take_sample_u16_be,
	[SND_PCM_SFMT_S24_LE]	rate_take_sample_s24_le,
	[SND_PCM_SFMT_S24_BE]	rate_take_sample_s24_be,
	[SND_PCM_SFMT_U24_LE]	rate_take_sample_u24_le,
	[SND_PCM_SFMT_U24_BE]	rate_take_sample_u24_be,
	[SND_PCM_SFMT_S32_LE]	rate_take_sample_s32_le,
	[SND_PCM_SFMT_S32_BE]	rate_take_sample_s32_be,
	[SND_PCM_SFMT_U32_LE]	rate_take_sample_u32_le,
	[SND_PCM_SFMT_U32_BE]	rate_take_sample_u32_be
};

RATE_PUT_SAMPLE(s8, int8_t, smp >> 8)
RATE_PUT_SAMPLE(u8, int8_t, (smp >> 8) ^ 0x80)
RATE_PUT_SAMPLE(s16_le, int16_t, my_little_swap16(smp))
RATE_PUT_SAMPLE(s16_be, int16_t, my_big_swap16(smp))
RATE_PUT_SAMPLE(u16_le, int16_t, my_little_swap16(smp ^ 0x8000))
RATE_PUT_SAMPLE(u16_be, int16_t, my_big_swap16(smp ^ 0x8000))
RATE_PUT_SAMPLE(s24_le, int32_t, my_little_swap32(smp << 8))
RATE_PUT_SAMPLE(s24_be, int32_t, my_big_swap32(smp << 8))
RATE_PUT_SAMPLE(u24_le, int32_t, my_little_swap32((smp ^ 0x8000) >> 8))
RATE_PUT_SAMPLE(u24_be, int32_t, my_big_swap32((smp ^ 0x8000) >> 8))
RATE_PUT_SAMPLE(s32_le, int32_t, my_little_swap32(smp >> 16))
RATE_PUT_SAMPLE(s32_be, int32_t, my_big_swap32(smp >> 16))
RATE_PUT_SAMPLE(u32_le, int32_t, my_little_swap32((smp ^ 0x8000) >> 16))
RATE_PUT_SAMPLE(u32_be, int32_t, my_big_swap32((smp ^ 0x8000) >> 16))

static put_sample_f rate_put_sample[] = {
	[SND_PCM_SFMT_S8]	rate_put_sample_s8,
	[SND_PCM_SFMT_U8]	rate_put_sample_u8,
	[SND_PCM_SFMT_S16_LE]	rate_put_sample_s16_le,
	[SND_PCM_SFMT_S16_BE]	rate_put_sample_s16_be,
	[SND_PCM_SFMT_U16_LE]	rate_put_sample_u16_le,
	[SND_PCM_SFMT_U16_BE]	rate_put_sample_u16_be,
	[SND_PCM_SFMT_S24_LE]	rate_put_sample_s24_le,
	[SND_PCM_SFMT_S24_BE]	rate_put_sample_s24_be,
	[SND_PCM_SFMT_U24_LE]	rate_put_sample_u24_le,
	[SND_PCM_SFMT_U24_BE]	rate_put_sample_u24_be,
	[SND_PCM_SFMT_S32_LE]	rate_put_sample_s32_le,
	[SND_PCM_SFMT_S32_BE]	rate_put_sample_s32_be,
	[SND_PCM_SFMT_U32_LE]	rate_put_sample_u32_le,
	[SND_PCM_SFMT_U32_BE]	rate_put_sample_u32_be
};

int snd_pcm_plugin_build_rate(snd_pcm_plugin_handle_t *handle,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin)
{
	struct rate_private_data *data;
	snd_pcm_plugin_t *plugin;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->interleave != dst_format->interleave && 
	    src_format->voices > 1)
		return -EINVAL;
	if (!dst_format->interleave)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;
	if (dst_format->voices < 1)
		return -EINVAL;
	if (snd_pcm_format_linear(src_format->format) <= 0)
		return -EINVAL;
	if (snd_pcm_format_linear(dst_format->format) <= 0)
		return -EINVAL;
	if (src_format->rate == dst_format->rate)
		return -EINVAL;
	plugin = snd_pcm_plugin_build(handle,
				      "rate conversion",
				      src_format,
				      dst_format,
				      sizeof(rate_t) +
				        src_format->voices * sizeof(rate_voice_t));
	if (plugin == NULL)
		return -ENOMEM;
	data = (rate_t *)plugin->extra_data;
	data->plugin = plugin;
	data->take = rate_take_sample[src_format->format];
	data->put = rate_put_sample[dst_format->format];
	if (src_format->rate < dst_format->rate) {
		data->pitch = ((src_format->rate << SHIFT) + (dst_format->rate >> 1)) / dst_format->rate;
	} else {
		data->pitch = ((dst_format->rate << SHIFT) + (src_format->rate >> 1)) / src_format->rate;
	}
	data->pos = 0;
	rate_init(plugin, data);
	data->old_src_samples = data->old_dst_samples = 0;
	plugin->transfer = rate_transfer;
	plugin->src_samples = rate_src_samples;
	plugin->dst_samples = rate_dst_samples;
	plugin->action = rate_action;
	*r_plugin = plugin;
	return 0;
}
