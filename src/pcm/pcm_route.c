/*
 *  PCM - Linear conversion
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
  
#include <byteswap.h>
#include <math.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

/* The best possible hack to support missing optimization in gcc 2.7.2.3 */
#if ROUTE_PLUGIN_RESOLUTION & (ROUTE_PLUGIN_RESOLUTION - 1) != 0
#define div(a) a /= ROUTE_PLUGIN_RESOLUTION
#elif ROUTE_PLUGIN_RESOLUTION == 16
#define div(a) a >>= 4
#else
#error "Add some code here"
#endif

typedef struct {
	int channel;
	int as_int;
#if ROUTE_PLUGIN_FLOAT
	float as_float;
#endif
} ttable_src_t;

typedef struct ttable_dst ttable_dst_t;

typedef struct {
	enum {UINT32=0, UINT64=1, FLOAT=2} sum_idx;
	int get_idx;
	int put_idx;
	int conv_idx;
	int src_size;
	int dst_sfmt;
	size_t ndsts;
	ttable_dst_t *dsts;
} route_params_t;


typedef void (*route_f)(snd_pcm_channel_area_t *src_areas,
			size_t src_offset,
			snd_pcm_channel_area_t *dst_area,
			size_t dst_offset,
			size_t frames,
			ttable_dst_t *ttable,
			route_params_t *params);

struct ttable_dst {
	int att;	/* Attenuated */
	unsigned int nsrcs;
	ttable_src_t* srcs;
	route_f func;
};

typedef union {
	u_int32_t as_uint32;
	u_int64_t as_uint64;
#if ROUTE_PLUGIN_FLOAT
	float as_float;
#endif
} sum_t;

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	int req_sformat, req_schannels;
	int sformat;
	int cformat;
	int schannels;
	int cchannels;
	int cxfer_mode, cmmap_shape;
	route_params_t params;
} snd_pcm_route_t;


static void route1_zero(snd_pcm_channel_area_t *src_areas ATTRIBUTE_UNUSED,
			size_t src_offset ATTRIBUTE_UNUSED,
			snd_pcm_channel_area_t *dst_area,
			size_t dst_offset,
			size_t frames,
			ttable_dst_t* ttable ATTRIBUTE_UNUSED,
			route_params_t *params)
{
#if 0
	if (dst_area->wanted)
		snd_pcm_area_silence(dst_area, dst_offset, frames, params->dst_sfmt);
	dsts_area->enabled = 0;
#else
	snd_pcm_area_silence(dst_area, dst_offset, frames, params->dst_sfmt);
#endif
}

static void route1_one(snd_pcm_channel_area_t *src_areas,
		       size_t src_offset,
		       snd_pcm_channel_area_t *dst_area,
		       size_t dst_offset,
		       size_t frames,
		       ttable_dst_t* ttable,
		       route_params_t *params)
{
#define CONV_LABELS
#include "plugin_ops.h"
#undef CONV_LABELS
	void *conv;
	snd_pcm_channel_area_t *src_area = 0;
	unsigned int srcidx;
	char *src, *dst;
	int src_step, dst_step;
	for (srcidx = 0; srcidx < ttable->nsrcs; ++srcidx) {
		src_area = &src_areas[ttable->srcs[srcidx].channel];
		if (src_area->addr != NULL)
			break;
	}
	if (srcidx == ttable->nsrcs) {
		route1_zero(src_areas, src_offset, dst_area, dst_offset, frames, ttable, params);
		return;
	}
	
#if 0
	dst_area->enabled = 1;
#endif
	conv = conv_labels[params->conv_idx];
	src = snd_pcm_channel_area_addr(src_area, src_offset);
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	src_step = snd_pcm_channel_area_step(src_area);
	dst_step = snd_pcm_channel_area_step(dst_area);
	while (frames-- > 0) {
		goto *conv;
#define CONV_END after
#include "plugin_ops.h"
#undef CONV_END
	after:
		src += src_step;
		dst += dst_step;
	}
}

static void route1_many(snd_pcm_channel_area_t *src_areas,
			size_t src_offset,
			snd_pcm_channel_area_t *dst_area,
			size_t dst_offset,
			size_t frames,
			ttable_dst_t* ttable,
			route_params_t *params)
{
#define GET_U_LABELS
#define PUT_U32_LABELS
#include "plugin_ops.h"
#undef GET_U_LABELS
#undef PUT_U32_LABELS
	static void *zero_labels[3] = { &&zero_int32, &&zero_int64,
#if ROUTE_PLUGIN_FLOAT
				 &&zero_float
#endif
	};
	/* sum_type att */
	static void *add_labels[3 * 2] = { &&add_int32_noatt, &&add_int32_att,
				    &&add_int64_noatt, &&add_int64_att,
#if ROUTE_PLUGIN_FLOAT
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
#if ROUTE_PLUGIN_FLOAT
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
	void *zero, *get, *add, *norm, *put_u32;
	int nsrcs = ttable->nsrcs;
	char *dst;
	int dst_step;
	char *srcs[nsrcs];
	int src_steps[nsrcs];
	ttable_src_t src_tt[nsrcs];
	u_int32_t sample = 0;
	int srcidx, srcidx1 = 0;
	for (srcidx = 0; srcidx < nsrcs; ++srcidx) {
		snd_pcm_channel_area_t *src_area = &src_areas[ttable->srcs[srcidx].channel];
#if 0
		if (!src_area->enabled)
			continue;
#endif
		srcs[srcidx1] = snd_pcm_channel_area_addr(src_area, src_offset);
		src_steps[srcidx1] = snd_pcm_channel_area_step(src_area);
		src_tt[srcidx1] = ttable->srcs[srcidx];
		srcidx1++;
	}
	nsrcs = srcidx1;
	if (nsrcs == 0) {
		route1_zero(src_areas, src_offset, dst_area, dst_offset, frames, ttable, params);
		return;
	} else if (nsrcs == 1 && src_tt[0].as_int == ROUTE_PLUGIN_RESOLUTION) {
		route1_one(src_areas, src_offset, dst_area, dst_offset, frames, ttable, params);
		return;
	}

#if 0
	dst_area->enabled = 1;
#endif
	zero = zero_labels[params->sum_idx];
	get = get_u_labels[params->get_idx];
	add = add_labels[params->sum_idx * 2 + ttable->att];
	norm = norm_labels[params->sum_idx * 8 + ttable->att * 4 + 4 - params->src_size];
	put_u32 = put_u32_labels[params->put_idx];
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	dst_step = snd_pcm_channel_area_step(dst_area);

	while (frames-- > 0) {
		ttable_src_t *ttp = src_tt;
		sum_t sum;

		/* Zero sum */
		goto *zero;
	zero_int32:
		sum.as_uint32 = 0;
		goto zero_end;
	zero_int64: 
		sum.as_uint64 = 0;
		goto zero_end;
#if ROUTE_PLUGIN_FLOAT
	zero_float:
		sum.as_float = 0.0;
		goto zero_end;
#endif
	zero_end:
		for (srcidx = 0; srcidx < nsrcs; ++srcidx) {
			char *src = srcs[srcidx];
			
			/* Get sample */
			goto *get;
#define GET_U_END after_get
#include "plugin_ops.h"
#undef GET_U_END
		after_get:

			/* Sum */
			goto *add;
		add_int32_att:
			sum.as_uint32 += sample * ttp->as_int;
			goto after_sum;
		add_int32_noatt:
			if (ttp->as_int)
				sum.as_uint32 += sample;
			goto after_sum;
		add_int64_att:
			sum.as_uint64 += (u_int64_t) sample * ttp->as_int;
			goto after_sum;
		add_int64_noatt:
			if (ttp->as_int)
				sum.as_uint64 += sample;
			goto after_sum;
#if ROUTE_PLUGIN_FLOAT
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
		sum.as_uint64 = sum.as_uint32;
	norm_int64_8_att:
		sum.as_uint64 <<= 8;
	norm_int64_0_att:
		div(sum.as_uint64);
		goto norm_int;

	norm_int32_16_att:
		sum.as_uint64 = sum.as_uint32;
	norm_int64_16_att:
		sum.as_uint64 <<= 16;
		div(sum.as_uint64);
		goto norm_int;

	norm_int32_24_att:
		sum.as_uint64 = sum.as_uint32;
	norm_int64_24_att:
		sum.as_uint64 <<= 24;
		div(sum.as_uint64);
		goto norm_int;

	norm_int32_8_noatt:
		sum.as_uint64 = sum.as_uint32;
	norm_int64_8_noatt:
		sum.as_uint64 <<= 8;
		goto norm_int;

	norm_int32_16_noatt:
		sum.as_uint64 = sum.as_uint32;
	norm_int64_16_noatt:
		sum.as_uint64 <<= 16;
		goto norm_int;

	norm_int32_24_noatt:
		sum.as_uint64 = sum.as_uint32;
	norm_int64_24_noatt:
		sum.as_uint64 <<= 24;
		goto norm_int;

	norm_int64_0_noatt:
	norm_int:
		if (sum.as_uint64 > (u_int32_t)0xffffffff)
			sample = (u_int32_t)0xffffffff;
		else
			sample = sum.as_uint64;
		goto after_norm;

#if ROUTE_PLUGIN_FLOAT
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
		if (sum.as_float > (u_int32_t)0xffffffff)
			sample = (u_int32_t)0xffffffff;
		else
			sample = sum.as_float;
		goto after_norm;
#endif
	after_norm:
		
		/* Put sample */
		goto *put_u32;
#define PUT_U32_END after_put_u32
#include "plugin_ops.h"
#undef PUT_U32_END
	after_put_u32:
		
		dst += dst_step;
	}
}

static void route_transfer(snd_pcm_channel_area_t *src_areas,
			   size_t src_offset,
			   snd_pcm_channel_area_t *dst_areas,
			   size_t dst_offset,
			   size_t frames,
			   size_t dst_channels,
			   route_params_t *params)
{
	size_t dst_channel;
	ttable_dst_t *dstp;
	snd_pcm_channel_area_t *dst_area;

	dstp = params->dsts;
	dst_area = dst_areas;
	for (dst_channel = 0; dst_channel < dst_channels; ++dst_channel) {
		if (dst_channel >= params->ndsts)
			route1_zero(src_areas, src_offset, dst_area, dst_offset, frames, dstp, params);
		else
			dstp->func(src_areas, src_offset, dst_area, dst_offset, frames, dstp, params);
		dstp++;
		dst_area++;
	}
}

static int snd_pcm_route_close(snd_pcm_t *pcm)
{
	snd_pcm_route_t *route = pcm->private;
	route_params_t *params = &route->params;
	int err = 0;
	size_t dst_channel;
	if (route->plug.close_slave)
		err = snd_pcm_close(route->plug.slave);
	if (params->dsts) {
		for (dst_channel = 0; dst_channel < params->ndsts; ++dst_channel) {
			if (params->dsts[dst_channel].srcs != NULL)
				free(params->dsts[dst_channel].srcs);
		}
		free(params->dsts);
	}
	free(route);
	return 0;
}

static int snd_pcm_route_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t * info)
{
	snd_pcm_route_t *route = pcm->private;
	unsigned int req_mask = info->req_mask;
	unsigned int sfmt = info->req.format.sfmt;
	unsigned int channels = info->req.format.channels;
	int err;
	if (req_mask & SND_PCM_PARAMS_SFMT &&
	    !snd_pcm_format_linear(sfmt)) {
		info->req.fail_mask = SND_PCM_PARAMS_SFMT;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	if (route->req_sformat >= 0) {
		info->req_mask |= SND_PCM_PARAMS_SFMT;
		info->req.format.sfmt = route->req_sformat;
	}
	if (route->req_schannels >= 0) {
		info->req_mask |= SND_PCM_PARAMS_CHANNELS;
		info->req.format.channels = route->req_schannels;
	}
	info->req_mask &= ~(SND_PCM_PARAMS_MMAP_SHAPE | 
			    SND_PCM_PARAMS_XFER_MODE);
	err = snd_pcm_params_info(route->plug.slave, info);
	info->req_mask = req_mask;
	info->req.format.sfmt = sfmt;
	info->req.format.channels = channels;
	if (err < 0)
		return err;
	if (req_mask & SND_PCM_PARAMS_SFMT)
		info->formats = 1 << sfmt;
	else
		info->formats = SND_PCM_LINEAR_FORMATS;
	if (req_mask & SND_PCM_PARAMS_CHANNELS) {
		info->min_channels = channels;
		info->max_channels = channels;
	} else {
		info->min_channels = 1;
		info->max_channels = 1024;
	}
	info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	info->flags |= SND_PCM_INFO_INTERLEAVED | SND_PCM_INFO_NONINTERLEAVED;
	return err;
}

static int snd_pcm_route_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_route_t *route = pcm->private;
	snd_pcm_t *slave = route->plug.slave;
	int err;
	if (!snd_pcm_format_linear(params->format.sfmt)) {
		params->fail_mask = SND_PCM_PARAMS_SFMT;
		params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	route->cformat = params->format.sfmt;
	route->cchannels = params->format.channels;
	route->cxfer_mode = params->xfer_mode;
	route->cmmap_shape = params->mmap_shape;
	if (route->req_sformat >= 0)
		params->format.sfmt = route->req_sformat;
	if (route->req_schannels >= 0)
		params->format.channels = route->req_schannels;
	params->xfer_mode = SND_PCM_XFER_UNSPECIFIED;
	params->mmap_shape = SND_PCM_MMAP_UNSPECIFIED;;
	err = snd_pcm_params(slave, params);
	params->format.sfmt = route->cformat;
	params->format.channels = route->cchannels;
	params->xfer_mode = route->cxfer_mode;
	params->mmap_shape = route->cmmap_shape;
	return err;
}

static int snd_pcm_route_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_route_t *route = pcm->private;
	int src_format, dst_format;
	int err = snd_pcm_setup(route->plug.slave, setup);
	if (err < 0)
		return err;
	if (route->req_sformat >= 0)
		assert(route->req_sformat == setup->format.sfmt);
	route->sformat = setup->format.sfmt;
	route->schannels = setup->format.channels;
	if (route->cxfer_mode == SND_PCM_XFER_UNSPECIFIED)
		setup->xfer_mode = SND_PCM_XFER_NONINTERLEAVED;
	else
		setup->xfer_mode = route->cxfer_mode;
	if (route->cmmap_shape == SND_PCM_MMAP_UNSPECIFIED)
		setup->mmap_shape = SND_PCM_MMAP_NONINTERLEAVED;
	else
		setup->mmap_shape = route->cmmap_shape;
	setup->format.sfmt = route->cformat;
	setup->format.channels = route->cchannels;
	setup->mmap_bytes = 0;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		src_format = route->cformat;
		dst_format = route->sformat;
	} else {
		src_format = route->sformat;
		dst_format = route->cformat;
	}
	route->params.get_idx = getput_index(src_format);
	route->params.put_idx = getput_index(dst_format);
	route->params.conv_idx = conv_index(src_format, dst_format);
	route->params.src_size = snd_pcm_format_width(src_format) / 8;
	route->params.dst_sfmt = dst_format;
#if ROUTE_PLUGIN_FLOAT
	route->params.sum_idx = FLOAT;
#else
	if (src_size == 4)
		route->params.sum_idx = UINT64;
	else
		route->params.sum_idx = UINT32;
#endif
	return 0;
}

static int snd_pcm_route_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t * setup)
{
#if 0
	snd_pcm_plugin_t *plugin = pcm->private;
	int err;
	err = snd_pcm_channel_setup(plugin->slave, setup);
	if (err < 0)
		return err;
#endif
	if (!pcm->mmap_info)
		return 0;
	if (pcm->setup.mmap_shape == SND_PCM_MMAP_INTERLEAVED) {
		setup->running_area.addr = pcm->mmap_info->addr;
		setup->running_area.first = setup->channel * pcm->bits_per_sample;
		setup->running_area.step = pcm->bits_per_frame;
	} else {
		setup->running_area.addr = pcm->mmap_info->addr + setup->channel * pcm->setup.buffer_size * pcm->bits_per_sample / 8;
		setup->running_area.first = 0;
		setup->running_area.step = pcm->bits_per_sample;
	}
	setup->stopped_area = setup->running_area;
	return 0;
}

static ssize_t snd_pcm_route_write_areas(snd_pcm_t *pcm,
					 snd_pcm_channel_area_t *areas,
					 size_t offset,
					 size_t size,
					 size_t *slave_sizep)
{
	snd_pcm_route_t *route = pcm->private;
	snd_pcm_t *slave = route->plug.slave;
	size_t xfer = 0;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_playback_xfer(slave, size - xfer);
		route_transfer(areas, offset, 
			       snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
			       frames, route->schannels, &route->params);
		err = snd_pcm_mmap_forward(slave, frames);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		offset += err;
		xfer += err;
		snd_pcm_mmap_hw_forward(pcm, err);
	}
	if (xfer > 0) {
		if (slave_sizep)
			*slave_sizep = xfer;
		return xfer;
	}
	return err;
}

static ssize_t snd_pcm_route_read_areas(snd_pcm_t *pcm,
					 snd_pcm_channel_area_t *areas,
					 size_t offset,
					 size_t size,
					 size_t *slave_sizep)
{
	snd_pcm_route_t *route = pcm->private;
	snd_pcm_t *slave = route->plug.slave;
	size_t xfer = 0;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_capture_xfer(slave, size - xfer);
		route_transfer(snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
			       areas, offset, 
			       frames, route->cchannels, &route->params);
		err = snd_pcm_mmap_forward(slave, frames);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		offset += err;
		xfer += err;
		snd_pcm_mmap_hw_forward(pcm, err);
	}
	if (xfer > 0) {
		if (slave_sizep)
			*slave_sizep = xfer;
		return xfer;
	}
	return err;
}

static void snd_pcm_route_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_route_t *route = pcm->private;
	unsigned int dst;
	if (route->req_sformat < 0)
		fprintf(fp, "Route conversion PCM\n");
	else
		fprintf(fp, "Route conversion PCM (sformat=%s)\n", 
			snd_pcm_format_name(route->req_sformat));
	fputs("Transformation table:\n", fp);
	for (dst = 0; dst < route->params.ndsts; dst++) {
		ttable_dst_t *d = &route->params.dsts[dst];
		unsigned int src;
		if (d->nsrcs == 0)
			continue;
		fprintf(fp, "%d <- ", dst);
		src = 0;
		while (1) {
			ttable_src_t *s = &d->srcs[src];
			if (d->att)
				fprintf(fp, "%d*%g", s->channel, s->as_float);
			else
				fprintf(fp, "%d", s->channel);
			src++;
			if (src == d->nsrcs)
				break;
			fputs(" + ", fp);
		}
		putc('\n', fp);
	}
	if (pcm->valid_setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(route->plug.slave, fp);
}

snd_pcm_ops_t snd_pcm_route_ops = {
	close: snd_pcm_route_close,
	info: snd_pcm_plugin_info,
	params_info: snd_pcm_route_params_info,
	params: snd_pcm_route_params,
	setup: snd_pcm_route_setup,
	channel_info: snd_pcm_plugin_channel_info,
	channel_params: snd_pcm_plugin_channel_params,
	channel_setup: snd_pcm_route_channel_setup,
	dump: snd_pcm_route_dump,
	nonblock: snd_pcm_plugin_nonblock,
	async: snd_pcm_plugin_async,
	mmap: snd_pcm_plugin_mmap,
	munmap: snd_pcm_plugin_munmap,
};

int route_load_ttable(route_params_t *params, int stream,
		      unsigned int tt_ssize,
		      ttable_entry_t *ttable,
		      unsigned int tt_cused, unsigned int tt_sused)
{
	unsigned int src_channel, dst_channel;
	ttable_dst_t *dptr;
	unsigned int sused, dused, smul, dmul;
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		sused = tt_cused;
		dused = tt_sused;
		smul = tt_ssize;
		dmul = 1;
	} else {
		sused = tt_sused;
		dused = tt_cused;
		smul = 1;
		dmul = tt_ssize;
	}
	params->ndsts = dused;
	dptr = calloc(dused, sizeof(*params->dsts));
	if (!dptr)
		return -ENOMEM;
	params->dsts = dptr;
	for (dst_channel = 0; dst_channel < dused; ++dst_channel) {
		ttable_entry_t t = 0;
		int att = 0;
		int nsrcs = 0;
		ttable_src_t srcs[sused];
		for (src_channel = 0; src_channel < sused; ++src_channel) {
			ttable_entry_t v;
			v = ttable[src_channel * smul + dst_channel * dmul];
			assert(v >= 0 && v <= FULL);
			if (v != 0) {
				srcs[nsrcs].channel = src_channel;
#if ROUTE_PLUGIN_FLOAT
				/* Also in user space for non attenuated */
				srcs[nsrcs].as_int = (v == FULL ? ROUTE_PLUGIN_RESOLUTION : 0);
				srcs[nsrcs].as_float = v;
#else
				srcs[nsrcs].as_int = v;
#endif
				if (v != FULL)
					att = 1;
				t += v;
				nsrcs++;
			}
		}
#if 0
		assert(t <= FULL);
#endif
		dptr->att = att;
		dptr->nsrcs = nsrcs;
		if (nsrcs == 0)
			dptr->func = route1_zero;
		else if (nsrcs == 1 && !att)
			dptr->func = route1_one;
		else
			dptr->func = route1_many;
		if (nsrcs > 0) {
			dptr->srcs = calloc(nsrcs, sizeof(*srcs));
			if (!dptr->srcs)
				return -ENOMEM;
			memcpy(dptr->srcs, srcs, sizeof(*srcs) * nsrcs);
		} else
			dptr->srcs = 0;
		dptr++;
	}
	return 0;
}


int snd_pcm_route_open(snd_pcm_t **pcmp, char *name,
		       int sformat, unsigned int schannels,
		       ttable_entry_t *ttable,
		       unsigned int tt_ssize,
		       unsigned int tt_cused, unsigned int tt_sused,
		       snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_route_t *route;
	int err;
	assert(pcmp && slave && ttable);
	if (sformat >= 0 && snd_pcm_format_linear(sformat) != 1)
		return -EINVAL;
	route = calloc(1, sizeof(snd_pcm_route_t));
	if (!route) {
		return -ENOMEM;
	}
	route->req_sformat = sformat;
	route->req_schannels = schannels;
	route->plug.read = snd_pcm_route_read_areas;
	route->plug.write = snd_pcm_route_write_areas;
	route->plug.slave = slave;
	route->plug.close_slave = close_slave;

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(route);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_ROUTE;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_route_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = route;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = &route->plug.hw_ptr;
	pcm->appl_ptr = &route->plug.appl_ptr;
	err = route_load_ttable(&route->params, pcm->stream, tt_ssize, ttable, tt_cused, tt_sused);
	if (err < 0) {
		snd_pcm_close(pcm);
		return err;
	}
	*pcmp = pcm;

	return 0;
}

int snd_pcm_route_load_ttable(snd_config_t *tt, ttable_entry_t *ttable,
			      unsigned int tt_csize, unsigned int tt_ssize,
			      unsigned int *tt_cused, unsigned int *tt_sused,
			      int schannels)
{
	int cused = -1;
	int sused = -1;
	snd_config_iterator_t i;
	unsigned int k;
	for (k = 0; k < tt_csize * tt_ssize; ++k)
		ttable[k] = 0.0;
	snd_config_foreach(i, tt) {
		snd_config_t *in = snd_config_entry(i);
		snd_config_iterator_t j;
		char *p;
		long cchannel;
		errno = 0;
		cchannel = strtol(in->id, &p, 10);
		if (errno || *p || 
		    cchannel < 0 || (unsigned int) cchannel > tt_csize)
			return -EINVAL;
		if (snd_config_type(in) != SND_CONFIG_TYPE_COMPOUND)
			return -EINVAL;
		snd_config_foreach(j, in) {
			snd_config_t *jn = snd_config_entry(j);
			double value;
			long schannel;
			int err;
			errno = 0;
			schannel = strtol(jn->id, &p, 10);
			if (errno || *p || 
			    schannel < 0 || (unsigned int) schannel > tt_ssize || 
			    (schannels > 0 && schannel >= schannels))
				return -EINVAL;
			err = snd_config_real_get(jn, &value);
			if (err < 0) {
				long v;
				err = snd_config_integer_get(jn, &v);
				if (err < 0)
					return -EINVAL;
				value = v;
			}
			ttable[cchannel * tt_ssize + schannel] = value;
			if (schannel > sused)
				sused = schannel;
		}
		if (cchannel > cused)
			cused = cchannel;
	}
	*tt_sused = sused + 1;
	*tt_cused = cused + 1;
	return 0;
}

#define MAX_CHANNELS 32

int _snd_pcm_route_open(snd_pcm_t **pcmp, char *name,
			snd_config_t *conf, 
			int stream, int mode)
{
	snd_config_iterator_t i;
	char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	int sformat = -1;
	long schannels = -1;
	snd_config_t *tt = NULL;
	ttable_entry_t ttable[MAX_CHANNELS*MAX_CHANNELS];
	unsigned int cused, sused;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "sname") == 0) {
			err = snd_config_string_get(n, &sname);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "sformat") == 0) {
			char *f;
			err = snd_config_string_get(n, &f);
			if (err < 0)
				return -EINVAL;
			sformat = snd_pcm_format_value(f);
			if (sformat < 0)
				return -EINVAL;
			if (snd_pcm_format_linear(sformat) != 1)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "schannels") == 0) {
			err = snd_config_integer_get(n, &schannels);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "ttable") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_COMPOUND)
				return -EINVAL;
			tt = n;
			continue;
		}
		return -EINVAL;
	}
	if (!sname || !tt)
		return -EINVAL;

	err = snd_pcm_route_load_ttable(tt, ttable, MAX_CHANNELS, MAX_CHANNELS,
					&cused, &sused, schannels);
	if (err < 0)
		return err;

	/* This is needed cause snd_config_update may destroy config */
	sname = strdup(sname);
	if (!sname)
		return  -ENOMEM;
	err = snd_pcm_open(&spcm, sname, stream, mode);
	free(sname);
	if (err < 0)
		return err;
	err = snd_pcm_route_open(pcmp, name, sformat, schannels,
				 ttable, MAX_CHANNELS,
				 cused, sused,
				 spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				

