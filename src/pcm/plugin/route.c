/*
 *  Attenuated route Plug-In
 *  Copyright (c) 2000 by Abramo Bagnara <abbagnara@racine.ra.it>
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
#define my_calloc(size) snd_kmalloc(size, GFP_KERNEL)
#define my_free(ptr) snd_kfree(ptr)
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include <math.h>
#include "../pcm_local.h"
#define my_calloc(size) calloc(1, size)
#define my_free(ptr) free(ptr)
#endif

typedef struct ttable_dst ttable_dst_t;
typedef struct route_private_data route_t;

typedef void (*route_voice_f)(snd_pcm_plugin_t *plugin,
			      const snd_pcm_plugin_voice_t *src_voices,
			      const snd_pcm_plugin_voice_t *dst_voice,
			      ttable_dst_t* ttable, size_t samples);

typedef struct {
	int voice;
	int as_int;
#ifndef __KERNEL
	float as_float;
#endif
} ttable_src_t;

struct ttable_dst {
	int att;	/* Attenuated */
	int nsrcs;
	ttable_src_t* srcs;
	route_voice_f func;
};

struct route_private_data {
	enum {INT32=0, INT64=1, FLOAT=2} sum_type;
	int get, put;
	int copy;
	int src_sample_size;
	ttable_dst_t ttable[0];
};

typedef union {
	int32_t as_int32;
	int64_t as_int64;
#ifndef __KERNEL__
	float as_float;
#endif
} sum_t;


void zero_voice(snd_pcm_plugin_t *plugin,
		const snd_pcm_plugin_voice_t *dst_voice,
		size_t samples)
{
	char *dst = dst_voice->addr + dst_voice->first / 8;
	int dst_step = dst_voice->step / 8;
	switch (plugin->dst_width) {
	case 4: {
		int dstbit = dst_voice->first % 8;
		int dstbit_step = dst_voice->step % 8;
		while (samples-- > 0) {
			if (dstbit)
				*dst &= 0x0f;
			else
				*dst &= 0xf0;
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8:
		while (samples-- > 0) {
			*dst = 0;
			dst += dst_step;
		}
		break;
	case 16:
		while (samples-- > 0) {
			*(int16_t*)dst = 0;
			dst += dst_step;
		}
		break;
	case 32:
		while (samples-- > 0) {
			*(int32_t*)dst = 0;
			dst += dst_step;
		}
		break;
	case 64:
		while (samples-- > 0) {
			*(int64_t*)dst = 0;
			dst += dst_step;
		}
		break;
	}
}


static void route_to_voice_zero(snd_pcm_plugin_t *plugin,
				const snd_pcm_plugin_voice_t *src_voices,
				const snd_pcm_plugin_voice_t *dst_voice,
				ttable_dst_t* ttable, size_t samples)
{
//	null_voice(&dst_voices[voice]);
	zero_voice(plugin, dst_voice, samples);
}

static void route_to_voice_one(snd_pcm_plugin_t *plugin,
			       const snd_pcm_plugin_voice_t *src_voices,
			       const snd_pcm_plugin_voice_t *dst_voice,
			       ttable_dst_t* ttable, size_t samples)
{
#define COPY_LABELS
#include "plugin_ops.h"
#undef COPY_LABELS
	route_t *data = (route_t *)plugin->extra_data;
	void *copy;
	const snd_pcm_plugin_voice_t *src_voice = 0;
	int srcidx;
	char *src, *dst;
	int src_step, dst_step;
	for (srcidx = 0; srcidx < ttable->nsrcs; ++srcidx) {
		src_voice = &src_voices[ttable->srcs[srcidx].voice];
		if (src_voice->addr != NULL)
			break;
	}
	if (srcidx == ttable->nsrcs) {
		route_to_voice_zero(plugin, src_voices, dst_voice, ttable, samples);
		return;
	}

	copy = copy_labels[data->copy];
	src = src_voice->addr + src_voice->first / 8;
	src_step = src_voice->step / 8;
	dst = dst_voice->addr + dst_voice->first / 8;
	dst_step = dst_voice->step / 8;
	while (samples-- > 0) {
		goto *copy;
#define COPY_END after
#include "plugin_ops.h"
#undef COPY_END
	after:
		src += src_step;
		dst += dst_step;
	}
}

static void route_to_voice(snd_pcm_plugin_t *plugin,
			   const snd_pcm_plugin_voice_t *src_voices,
			   const snd_pcm_plugin_voice_t *dst_voice,
			   ttable_dst_t* ttable, size_t samples)
{
#define GET_LABELS
#define PUT32_LABELS
#include "plugin_ops.h"
#undef GET_LABELS
#undef PUT32_LABELS
	static void *zero_labels[3] = { &&zero_int32, &&zero_int64,
#ifndef __KERNEL__
				 &&zero_float
#endif
	};
	/* sum_type att */
	static void *add_labels[3 * 2] = { &&add_int32_noatt, &&add_int32_att,
				    &&add_int64_noatt, &&add_int64_att,
#ifndef __KERNEL__
				    &&add_float_noatt, &&add_float_att
#endif
	};
	/* sum_type att shift */
	static void *norm_labels[3 * 2 * 4] = { 0,
					 &&norm_int32_8_noatt,
					 &&norm_int32_16_noatt,
					 &&norm_int32_24_noatt,
					 0,
					 &&norm_int32_8_att,
					 &&norm_int32_16_att,
					 &&norm_int32_24_att,
					 &&norm_int64_0_noatt,
					 &&norm_int64_8_noatt,
					 &&norm_int64_16_noatt,
					 &&norm_int64_24_noatt,
					 &&norm_int64_0_att,
					 &&norm_int64_8_att,
					 &&norm_int64_16_att,
					 &&norm_int64_24_att,
#ifndef __KERNEL__
					 &&norm_float_0,
					 &&norm_float_8,
					 &&norm_float_16,
					 &&norm_float_24,
					 &&norm_float_0,
					 &&norm_float_8,
					 &&norm_float_16,
					 &&norm_float_24,
#endif
	};
	route_t *data = (route_t *)plugin->extra_data;
	void *zero, *get, *add, *norm, *put32;
	int nsrcs = ttable->nsrcs;
	char *dst;
	int dst_step;
	char *srcs[nsrcs];
	int src_steps[nsrcs];
	ttable_src_t src_tt[nsrcs];
	int32_t sample = 0;
	int srcidx, srcidx1 = 0;
	for (srcidx = 0; srcidx < nsrcs; ++srcidx) {
		const snd_pcm_plugin_voice_t *src_voice = &src_voices[ttable->srcs[srcidx].voice];
		if (src_voice->addr == NULL)
			continue;
		srcs[srcidx1] = src_voice->addr + src_voices->first / 8;
		src_steps[srcidx1] = src_voice->step / 8;
		src_tt[srcidx1] = ttable->srcs[srcidx];
		srcidx1++;
	}
	nsrcs = srcidx1;
	if (nsrcs == 0) {
		route_to_voice_zero(plugin, src_voices, dst_voice, ttable, samples);
		return;
	} else if (nsrcs == 1 && src_tt[0].as_int == ROUTE_PLUGIN_RESOLUTION) {
		route_to_voice_one(plugin, src_voices, dst_voice, ttable, samples);
		return;
	}

	zero = zero_labels[data->sum_type];
	get = get_labels[data->get];
	add = add_labels[data->sum_type * 2 + ttable->att];
	norm = norm_labels[data->sum_type * 8 + ttable->att * 4 + 4 - data->src_sample_size];
	put32 = put32_labels[data->put];
	dst = dst_voice->addr + dst_voice->first / 8;
	dst_step = dst_voice->step / 8;

	while (samples-- > 0) {
		ttable_src_t *ttp = src_tt;
		sum_t sum;

		/* Zero sum */
		goto *zero;
	zero_int32:
		sum.as_int32 = 0;
		goto zero_end;
	zero_int64: 
		sum.as_int64 = 0;
		goto zero_end;
#ifndef __KERNEL__
	zero_float:
		sum.as_float = 0.0;
		goto zero_end;
#endif
	zero_end:
		for (srcidx = 0; srcidx < nsrcs; ++srcidx) {
			char *src = srcs[srcidx];
			
			/* Get sample */
			goto *get;
#define GET_END after_get
#include "plugin_ops.h"
#undef GET_END
		after_get:

			/* Sum */
			goto *add;
		add_int32_att:
			sum.as_int32 += sample * ttp->as_int;
			goto after_sum;
		add_int32_noatt:
			if (ttp->as_int)
				sum.as_int32 += sample;
			goto after_sum;
		add_int64_att:
			sum.as_int64 += (int64_t) sample * ttp->as_int;
			goto after_sum;
		add_int64_noatt:
			if (ttp->as_int)
				sum.as_int64 += sample;
			goto after_sum;
#ifndef __KERNEL__
		add_float_att:
			sum.as_float += sample * ttp->as_float;
			goto after_sum;
		add_float_noatt:
			if (ttp->as_int)
				sum.as_float += sample;
			goto after_sum;
#endif
		after_sum:
			srcs[srcidx] += src_steps[srcidx];
			ttp++;
		}
		
		/* Normalization */
		goto *norm;
	norm_int32_8_att:
		sum.as_int64 = sum.as_int32;
		sum.as_int64 *= (1 << 8) / ROUTE_PLUGIN_RESOLUTION;
		goto after_norm;
	norm_int32_16_att:
		sum.as_int64 = sum.as_int32;
		sum.as_int64 *= (1 << 16) / ROUTE_PLUGIN_RESOLUTION;
		goto after_norm;
	norm_int32_24_att:
		sum.as_int64 = sum.as_int32;
		sum.as_int64 *= (1 << 24) / ROUTE_PLUGIN_RESOLUTION;
		goto norm_int;
	norm_int64_0_att:
		sum.as_int64 /= ROUTE_PLUGIN_RESOLUTION;
		goto norm_int;
	norm_int64_8_att:
		sum.as_int64 *= (1 << 8) / ROUTE_PLUGIN_RESOLUTION;
		goto norm_int;
	norm_int64_16_att:
		sum.as_int64 *= (1 << 16) / ROUTE_PLUGIN_RESOLUTION;
		goto norm_int;
	norm_int64_24_att:
		sum.as_int64 *= (1 << 24) / ROUTE_PLUGIN_RESOLUTION;
		goto norm_int;
	norm_int32_8_noatt:
		sum.as_int64 = sum.as_int32;
		sum.as_int64 *= (1 << 8);
		goto after_norm;
	norm_int32_16_noatt:
		sum.as_int64 = sum.as_int32;
		sum.as_int64 *= (1 << 16);
		goto after_norm;
	norm_int32_24_noatt:
		sum.as_int64 = sum.as_int32;
		sum.as_int64 *= (1 << 24);
		goto norm_int;
	norm_int64_0_noatt:
		goto norm_int;
	norm_int64_8_noatt:
		sum.as_int64 *= (1 << 8);
		goto norm_int;
	norm_int64_16_noatt:
		sum.as_int64 *= (1 << 16);
		goto norm_int;
	norm_int64_24_noatt:
		sum.as_int64 *= (1 << 24);
		goto norm_int;
	norm_int:
		if (sum.as_int64 < (int32_t)0x80000000)
			sample = (int32_t)0x80000000;
		else if (sum.as_int64 > 0x7fffffffLL) {
			sample = 0x7fffffff;
		}
		else
			sample = sum.as_int64;
		goto after_norm;
#ifndef __KERNEL__
	norm_float_8:
		sum.as_float *= 1 << 8;
		goto norm_float;
	norm_float_16:
		sum.as_float *= 1 << 16;
		goto norm_float;
	norm_float_24:
		sum.as_float *= 1 << 24;
		goto norm_float;
	norm_float_0:
	norm_float:
		sum.as_float = floor(sum.as_float + 0.5);
		if (sum.as_float < (int32_t)0x80000000)
			sample = (int32_t)0x80000000;
		else if (sum.as_float > 0x7fffffff)
			sample = 0x7fffffff;
		else
			sample = sum.as_float;
		goto after_norm;
#endif
	after_norm:
		
		/* Put sample */
		goto *put32;
#define PUT32_END after_put32
#include "plugin_ops.h"
#undef PUT32_END
	after_put32:
		
		dst += dst_step;
	}
}

#ifdef __KERNEL__
#define FULL ROUTE_PLUGIN_RESOLUTION
typedef int src_ttable_entry_t;
#else
#define FULL 1.0 
typedef float src_ttable_entry_t;
#endif

static void route_free(snd_pcm_plugin_t *plugin, void* private_data)
{
	route_t *data = (route_t *)plugin->extra_data;
	int dst_voice;
	for (dst_voice = 0; dst_voice < plugin->dst_format.voices; ++dst_voice) {
		if (data->ttable[dst_voice].srcs != NULL)
			my_free(data->ttable[dst_voice].srcs);
	}
}

static int route_load_ttable(snd_pcm_plugin_t *plugin, 
			     const src_ttable_entry_t* src_ttable)
{
	route_t *data;
	int src_voice, dst_voice;
	const src_ttable_entry_t *sptr;
	ttable_dst_t *dptr;
	if (src_ttable == NULL)
		return 0;
	data = (route_t *)plugin->extra_data;
	dptr = data->ttable;
	sptr = src_ttable;
	plugin->private_free = route_free;
	for (dst_voice = 0; dst_voice < plugin->dst_format.voices; ++dst_voice) {
		src_ttable_entry_t t = 0;
		int att = 0;
		int nsrcs = 0;
		ttable_src_t srcs[plugin->src_format.voices];
		for (src_voice = 0; src_voice < plugin->src_format.voices; ++src_voice) {
			if (*sptr < 0 || *sptr > FULL)
				return -EINVAL;
			if (*sptr != 0) {
				srcs[nsrcs].voice = src_voice;
#ifdef __KERNEL__
				srcs[nsrcs].as_int = *sptr;
#else
				/* Also in user space for non attenuated */
				srcs[nsrcs].as_int = (*sptr == FULL ? ROUTE_PLUGIN_RESOLUTION : 0);
				srcs[nsrcs].as_float = *sptr;
#endif
				if (*sptr != FULL)
					att = 1;
				t += *sptr;
				nsrcs++;
			}
			sptr++;
		}
#if 0
		if (t > FULL)
			return -EINVAL;
#endif
		dptr->att = att;
		dptr->nsrcs = nsrcs;
		switch (nsrcs) {
		case 0:
			dptr->func = route_to_voice_zero;
			break;
		case 1:
			dptr->func = route_to_voice_one;
			break;
		default:
			dptr->func = route_to_voice;
			break;
		}
		dptr->srcs = my_calloc(sizeof(*srcs) * nsrcs);
		memcpy(dptr->srcs, srcs, sizeof(*srcs) * nsrcs);
		dptr++;
	}
	return 0;
}

static ssize_t route_transfer(snd_pcm_plugin_t *plugin,
			      const snd_pcm_plugin_voice_t *src_voices,
			      const snd_pcm_plugin_voice_t *dst_voices,
			      size_t samples)
{
	route_t *data;
	int src_nvoices, dst_nvoices;
	int src_voice, dst_voice;
	ttable_dst_t *ttp;
	const snd_pcm_plugin_voice_t *dvp;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL)
		return -EFAULT;
	if (samples < 0)
		return -EINVAL;
	if (samples == 0)
		return 0;
	data = (route_t *)plugin->extra_data;

	src_nvoices = plugin->src_format.voices;
	for (src_voice = 0; src_voice < src_nvoices; ++src_voice) {
		if (src_voices[src_voice].first % 8 != 0 || 
		    src_voices[src_voice].step % 8 != 0)
			return -EINVAL;
	}

	dst_nvoices = plugin->dst_format.voices;
	for (dst_voice = 0; dst_voice < dst_nvoices; ++dst_voice) {
		if (dst_voices[dst_voice].first % 8 != 0 || 
		    dst_voices[dst_voice].step % 8 != 0)
			return -EINVAL;
	}

	ttp = data->ttable;
	dvp = dst_voices;
	for (dst_voice = 0; dst_voice < dst_nvoices; ++dst_voice) {
		ttp->func(plugin, src_voices, dvp, ttp, samples);
		dvp++;
		ttp++;
	}
	return samples;
}

int getput_index(int format)
{
	int sign, width, endian;
	sign = !snd_pcm_format_signed(format);
	width = snd_pcm_format_width(format) / 8 - 1;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	endian = snd_pcm_format_big_endian(format);
#elif __BYTE_ORDER == __BIG_ENDIAN
	endian = snd_pcm_format_little_endian(format);
#else
#error "Unsupported endian..."
#endif
	if (endian < 0)
		endian = 0;
	return width * 4 + endian * 2 + sign;
}

int snd_pcm_plugin_build_route(snd_pcm_plugin_handle_t *handle,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       src_ttable_entry_t *ttable,
			       snd_pcm_plugin_t **r_plugin)
{
	route_t *data;
	snd_pcm_plugin_t *plugin;
	int err;

	if (!r_plugin)
		return -EFAULT;
	*r_plugin = NULL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (!(snd_pcm_format_linear(src_format->format) &&
	      snd_pcm_format_linear(dst_format->format)))
		return -EINVAL;

	plugin = snd_pcm_plugin_build(handle,
				      "attenuated route conversion",
				      src_format,
				      dst_format,
				      sizeof(route_t) + sizeof(data->ttable[0]) * dst_format->voices);
	if (plugin == NULL)
		return -ENOMEM;

	data = (route_t *) plugin->extra_data;

	data->get = getput_index(src_format->format);
	data->put = getput_index(dst_format->format);
	data->copy = copy_index(src_format->format, dst_format->format);

#ifdef __KERNEL__
	if (snd_pcm_format_width(src_format->format) == 32)
		data->sum_type = INT64;
	else
		data->sum_type = INT32;
#else
	data->sum_type = FLOAT;
#endif
	data->src_sample_size = snd_pcm_format_width(src_format->format) / 8;

	if ((err = route_load_ttable(plugin, ttable)) < 0) {
		snd_pcm_plugin_free(plugin);
		return err;
	}
	plugin->transfer = route_transfer;
	*r_plugin = plugin;
	return 0;
}
