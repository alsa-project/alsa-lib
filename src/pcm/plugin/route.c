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
#define bswap_16(x) __swab16((x))
#define bswap_32(x) __swab32((x))
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

#define ROUTE_PLUGIN_RESOLUTION 16

struct route_private_data;

typedef void (*route_f)(struct route_private_data *data,
			 void *src_ptr, void *dst_ptr,
			 size_t src_size, size_t dst_size);

struct route_private_data {
	int sample_size;	/* Bytes per sample */
	route_f func;
	int interleave;
	int format;
	int src_voices;
	int dst_voices;
	int ttable[0];
};

static void route_noop(struct route_private_data *data,
			void *src_ptr, void *dst_ptr,
			size_t src_size, size_t dst_size)
{
	memcpy(dst_ptr, src_ptr, src_size);
}

static void route_i_silence(struct route_private_data *data,
			     void *src_ptr, void *dst_ptr,
			     size_t src_size, size_t dst_size)
{
	char *src = src_ptr;
	char *dst = dst_ptr;
	int sil = snd_pcm_format_silence(data->format);
	int src_step = data->sample_size * data->src_voices;
	int dst_step = data->sample_size * data->dst_voices;
	char *end = dst + dst_size;
	if (data->src_voices < data->dst_voices) {
		while (dst < end) {
			int i;
			for (i = 0; i < src_step; ++i)
				*dst++ = *src++;
			for (; i < dst_step; ++i)
				*dst++ = sil;
		}
	} else {
		while (dst < end) {
			int i;
			for (i = 0; i < dst_step; ++i)
				*dst++ = *src++;
			src += src_step - dst_step;
		}
	}
}

static void route_n_silence(struct route_private_data *data,
			     void *src_ptr, void *dst_ptr,
			     size_t src_size, size_t dst_size)
{
	if (src_size < dst_size) {
		memcpy(dst_ptr, src_ptr, src_size);
		memset(dst_ptr + src_size, snd_pcm_format_silence(data->format), dst_size - src_size);
	} else {
		memcpy(dst_ptr, src_ptr, dst_size);
	}
}

#define ROUTE_I_FUNC(name, type, ttype, toh, hto, mul, div) \
static void name(struct route_private_data *data, \
		 void *src_ptr, void *dst_ptr, \
		 size_t src_size, size_t dst_size) \
{ \
	type *src = src_ptr; \
	type *dst = dst_ptr; \
	int src_voices = data->src_voices; \
	int dst_voices = data->dst_voices; \
	int *ttable = data->ttable; \
	size_t samples = src_size / (src_voices * data->sample_size); \
	while (samples-- > 0) { \
		int dst_voice; \
		int *ttp = ttable; \
		for (dst_voice = 0; dst_voice < dst_voices; ++dst_voice) { \
			int src_voice; \
			type *s = src; \
			ttype t = 0; \
			for (src_voice = 0; src_voice < src_voices; ++src_voice) { \
				ttype v = toh(*s); \
				t += mul(v, *ttp); \
				s++; \
				ttp++; \
			} \
			t = div(t, ROUTE_PLUGIN_RESOLUTION); \
			*dst++ = hto(t); \
		} \
		src += src_voices; \
	} \
}

#define ROUTE_N_FUNC(name, type, ttype, toh, hto, mul, div) \
static void name(struct route_private_data *data, \
		 void *src_ptr, void *dst_ptr, \
		 size_t src_size, size_t dst_size) \
{ \
	type *src = src_ptr; \
	type *dst = dst_ptr; \
	int src_voices = data->src_voices; \
	int dst_voices = data->dst_voices; \
	int *ttable = data->ttable; \
	size_t samples = src_size / (src_voices * data->sample_size); \
	int dst_voice; \
	for (dst_voice = 0; dst_voice < dst_voices; ++dst_voice) { \
		int *ttp = ttable + dst_voice * src_voices; \
		size_t samples1 = samples; \
		while (samples1-- > 0) { \
			int *ttp1 = ttp; \
			type *s = src; \
			ttype t = 0; \
			int src_voice; \
			for (src_voice = 0; src_voice < src_voices; ++src_voice) { \
				ttype v = toh(*s); \
				t += mul(v, *ttp1); \
				s += samples; \
				ttp1++; \
			} \
			t = div(t, ROUTE_PLUGIN_RESOLUTION); \
			*dst++ = hto(t); \
			src++; \
		} \
	} \
}

#define none(p) (p)
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16toh none
#define htole16 none
#define le32toh none
#define htole32 none
#define be16toh bswap_16
#define htobe16 bswap_16
#define be32toh bswap_32
#define htobe32 bswap_32
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le16toh bswap_16
#define htole16 bswap_16
#define le32toh bswap_32
#define htole32 bswap_32
#define be16toh none
#define htobe16 none
#define be32toh none
#define htobe32 none
#else
#error "Unsupported endian..."
#endif

#define mul(a,b) ((a)*(b))
#define div(a,b) ((a)/(b))
#define nmul(a,b) ((b)?(a):0)
#define ndiv(a,b) (a)

#define FUNCS(name, type, ttype, toh, hto) \
ROUTE_I_FUNC(aroute_i_##name, type, ttype, toh, hto, mul, div); \
ROUTE_N_FUNC(aroute_n_##name, type, ttype, toh, hto, mul, div); \
ROUTE_I_FUNC(nroute_i_##name, type, ttype, toh, hto, nmul, ndiv); \
ROUTE_N_FUNC(nroute_n_##name, type, ttype, toh, hto, nmul, ndiv);


FUNCS(s8,	int8_t,		int,	none,	none);
FUNCS(u8,	u_int8_t,	int,	none,	none);
FUNCS(s16_le,	int16_t,	int,	le16toh,htole16);
FUNCS(u16_le,	u_int16_t,	int,	le16toh,htole16);
FUNCS(s16_be,	int16_t,	int,	be16toh,htobe16);
FUNCS(u16_be,	u_int16_t,	int,	be16toh,htobe16);
FUNCS(s24_le,	int32_t,	int,	le32toh,htole32);
FUNCS(u24_le,	u_int32_t,	int,	le32toh,htole32);
FUNCS(s24_be,	int32_t,	int,	be32toh,htobe32);
FUNCS(u24_be,	u_int32_t,	int,	be32toh,htobe32);
FUNCS(s32_le,	int32_t,	int64_t,le32toh,htole32);
FUNCS(u32_le,	u_int32_t,	int64_t,le32toh,htole32);
FUNCS(s32_be,	int32_t,	int64_t,be32toh,htobe32);
FUNCS(u32_be,	u_int32_t,	int64_t,be32toh,htobe32);


static route_f aroute_i[] = {
	[SND_PCM_SFMT_S8]     aroute_i_s8,
	[SND_PCM_SFMT_U8]     aroute_i_u8,
	[SND_PCM_SFMT_S16_LE] aroute_i_s16_le,
	[SND_PCM_SFMT_S16_BE] aroute_i_s16_be,
	[SND_PCM_SFMT_U16_LE] aroute_i_u16_le,
	[SND_PCM_SFMT_U16_BE] aroute_i_u16_be,
	[SND_PCM_SFMT_S24_LE] aroute_i_s24_le,
	[SND_PCM_SFMT_S24_BE] aroute_i_s24_be,
	[SND_PCM_SFMT_U24_LE] aroute_i_u24_le,
	[SND_PCM_SFMT_U24_BE] aroute_i_u24_be,
	[SND_PCM_SFMT_S32_LE] aroute_i_s32_le,
	[SND_PCM_SFMT_S32_BE] aroute_i_s32_be,
	[SND_PCM_SFMT_U32_LE] aroute_i_u32_le,
	[SND_PCM_SFMT_U32_BE] aroute_i_u32_be
};

static route_f aroute_n[] = {
	[SND_PCM_SFMT_S8]     aroute_n_s8,
	[SND_PCM_SFMT_U8]     aroute_n_u8,
	[SND_PCM_SFMT_S16_LE] aroute_n_s16_le,
	[SND_PCM_SFMT_S16_BE] aroute_n_s16_be,
	[SND_PCM_SFMT_U16_LE] aroute_n_u16_le,
	[SND_PCM_SFMT_U16_BE] aroute_n_u16_be,
	[SND_PCM_SFMT_S24_LE] aroute_n_s24_le,
	[SND_PCM_SFMT_S24_BE] aroute_n_s24_be,
	[SND_PCM_SFMT_U24_LE] aroute_n_u24_le,
	[SND_PCM_SFMT_U24_BE] aroute_n_u24_be,
	[SND_PCM_SFMT_S32_LE] aroute_n_s32_le,
	[SND_PCM_SFMT_S32_BE] aroute_n_s32_be,
	[SND_PCM_SFMT_U32_LE] aroute_n_u32_le,
	[SND_PCM_SFMT_U32_BE] aroute_n_u32_be
};

static route_f nroute_i[] = {
	[SND_PCM_SFMT_S8]     nroute_i_s8,
	[SND_PCM_SFMT_U8]     nroute_i_u8,
	[SND_PCM_SFMT_S16_LE] nroute_i_s16_le,
	[SND_PCM_SFMT_S16_BE] nroute_i_s16_be,
	[SND_PCM_SFMT_U16_LE] nroute_i_u16_le,
	[SND_PCM_SFMT_U16_BE] nroute_i_u16_be,
	[SND_PCM_SFMT_S24_LE] nroute_i_s24_le,
	[SND_PCM_SFMT_S24_BE] nroute_i_s24_be,
	[SND_PCM_SFMT_U24_LE] nroute_i_u24_le,
	[SND_PCM_SFMT_U24_BE] nroute_i_u24_be,
	[SND_PCM_SFMT_S32_LE] nroute_i_s32_le,
	[SND_PCM_SFMT_S32_BE] nroute_i_s32_be,
	[SND_PCM_SFMT_U32_LE] nroute_i_u32_le,
	[SND_PCM_SFMT_U32_BE] nroute_i_u32_be
};

static route_f nroute_n[] = {
	[SND_PCM_SFMT_S8]     nroute_n_s8,
	[SND_PCM_SFMT_U8]     nroute_n_u8,
	[SND_PCM_SFMT_S16_LE] nroute_n_s16_le,
	[SND_PCM_SFMT_S16_BE] nroute_n_s16_be,
	[SND_PCM_SFMT_U16_LE] nroute_n_u16_le,
	[SND_PCM_SFMT_U16_BE] nroute_n_u16_be,
	[SND_PCM_SFMT_S24_LE] nroute_n_s24_le,
	[SND_PCM_SFMT_S24_BE] nroute_n_s24_be,
	[SND_PCM_SFMT_U24_LE] nroute_n_u24_le,
	[SND_PCM_SFMT_U24_BE] nroute_n_u24_be,
	[SND_PCM_SFMT_S32_LE] nroute_n_s32_le,
	[SND_PCM_SFMT_S32_BE] nroute_n_s32_be,
	[SND_PCM_SFMT_U32_LE] nroute_n_u32_le,
	[SND_PCM_SFMT_U32_BE] nroute_n_u32_be
};

static int route_load_ttable(struct route_private_data *data, 
			      const int *src_ttable)
{
	int src_voice, dst_voice;
	const int *sptr;
	int *dptr;
	int noop = 1;
	int noatt = 1;
	if (src_ttable == NULL)
		return 0;
	dptr = data->ttable;
	sptr = src_ttable;
	for (dst_voice = 0; dst_voice < data->dst_voices; ++dst_voice) {
		int t = 0;
		for (src_voice = 0; src_voice < data->src_voices; ++src_voice) {
			if (*sptr < 0 || *sptr > ROUTE_PLUGIN_RESOLUTION)
				return -EINVAL;
			if (*sptr != 0 && *sptr != ROUTE_PLUGIN_RESOLUTION)
				noatt = 0;
			if (src_voice == dst_voice) {
				if (*sptr != ROUTE_PLUGIN_RESOLUTION)
					noop = 0;
			}
			else {
				if (*sptr != 0)
					noop = 0;
			}
			t += *sptr;
			*dptr++ = *sptr++;
		}
#if 0
		if (t > ROUTE_PLUGIN_RESOLUTION)
			return -EINVAL;
#endif
	}
	if (noop) {
		if (data->src_voices == data->dst_voices)
			data->func = route_noop;
		else if (data->interleave)
			data->func = route_i_silence;
		else 
			data->func = route_n_silence;
		return 0;
	}
	if (noatt) {
		if (data->interleave)
			data->func = nroute_i[data->format];
		else
			data->func = nroute_n[data->format];
	} else {
		if (data->interleave)
			data->func = aroute_i[data->format];
		else
			data->func = aroute_n[data->format];
	}
	return 0;
}

static ssize_t route_transfer(snd_pcm_plugin_t *plugin,
			     char *src_ptr, size_t src_size,
			     char *dst_ptr, size_t dst_size)
{
	struct route_private_data *data;
	if (plugin == NULL || src_ptr == NULL || src_size < 0 ||
	                      dst_ptr == NULL || dst_size < 0)
		return -EINVAL;
	if (src_size == 0)
		return 0;
	data = (struct route_private_data *)snd_pcm_plugin_extra_data(plugin);
	data->func(data, src_ptr, dst_ptr, src_size, dst_size);
	return dst_size;
}

static ssize_t route_src_size(snd_pcm_plugin_t *plugin, size_t size)
{
        struct route_private_data *data;

        if (plugin == NULL || size <= 0)
                return -EINVAL;
        data = (struct route_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (!plugin || size <= 0)
		return -EINVAL;
	return (size * data->src_voices) / data->dst_voices;
}

static ssize_t route_dst_size(snd_pcm_plugin_t *plugin, size_t size)
{
        struct route_private_data *data;

        if (plugin == NULL || size <= 0)
                return -EINVAL;
        data = (struct route_private_data *)snd_pcm_plugin_extra_data(plugin);
	if (!plugin || size <= 0)
		return -EINVAL;
	return (size * data->dst_voices) / data->src_voices;
}

int snd_pcm_plugin_build_route(snd_pcm_format_t *src_format,
				snd_pcm_format_t *dst_format,
				int *ttable,
				snd_pcm_plugin_t **r_plugin)
{
	struct route_private_data *data;
	snd_pcm_plugin_t *plugin;
	int size;
	int err;

	if (!r_plugin)
		return -EINVAL;
	*r_plugin = NULL;
	if (src_format->interleave != dst_format->interleave)
		return -EINVAL;
	if (!dst_format->interleave)
		return -EINVAL;
	if (src_format->format != dst_format->format)
		return -EINVAL;
	if (!snd_pcm_format_linear(src_format->format))
		return -EINVAL;
	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices < 1 || dst_format->voices < 1)
		return -EINVAL;
	size = snd_pcm_format_size(src_format->format, 1);
	if (size < 0)
		return -EINVAL;
	plugin = snd_pcm_plugin_build("Volume/balance conversion",
				      sizeof(struct route_private_data) + 
				      sizeof(data->ttable[0]) * src_format->voices * dst_format->voices);
	if (plugin == NULL)
		return -ENOMEM;
	data = (struct route_private_data *)snd_pcm_plugin_extra_data(plugin);

	data->src_voices = src_format->voices;
	data->dst_voices = dst_format->voices;
	data->format = src_format->format;
	data->interleave = src_format->interleave;
	data->sample_size = size;
	if ((err = route_load_ttable(data, ttable)) < 0) {
		snd_pcm_plugin_free(plugin);
		return err;
	}
	plugin->transfer = route_transfer;
	plugin->src_size = route_src_size;
	plugin->dst_size = route_dst_size;
	*r_plugin = plugin;
	return 0;
}
