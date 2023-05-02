/**
 * \file pcm/pcm_rate.c
 * \ingroup PCM_Plugins
 * \brief PCM Rate Plugin Interface
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2000-2004
 */
/*
 *  PCM - Rate conversion
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *                2004 by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <inttypes.h>
#include "bswap.h"
#include "pcm_local.h"
#include "pcm_plugin.h"
#include "pcm_rate.h"

#include "plugin_ops.h"

#if 0
#define DEBUG_REFINE
#endif

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_rate = "";
#endif

#ifndef DOC_HIDDEN

typedef struct _snd_pcm_rate snd_pcm_rate_t;

struct _snd_pcm_rate {
	snd_pcm_generic_t gen;
	snd_pcm_uframes_t appl_ptr, hw_ptr, last_slave_hw_ptr;
	snd_pcm_uframes_t last_commit_ptr;
	snd_pcm_uframes_t orig_avail_min;
	snd_pcm_sw_params_t sw_params;
	snd_pcm_format_t sformat;
	unsigned int srate;
	snd_pcm_channel_area_t *pareas;	/* areas for splitted period (rate pcm) */
	snd_pcm_channel_area_t *sareas;	/* areas for splitted period (slave pcm) */
	snd_pcm_rate_info_t info;
	void *open_func;
	void *obj;
	snd_pcm_rate_ops_t ops;
	unsigned int src_conv_idx;
	unsigned int dst_conv_idx;
	snd_pcm_channel_area_t *src_buf;
	snd_pcm_channel_area_t *dst_buf;
	int start_pending; /* start is triggered but not commited to slave */
	snd_htimestamp_t trigger_tstamp;
	unsigned int plugin_version;
	unsigned int rate_min, rate_max;
	snd_pcm_format_t orig_in_format;
	snd_pcm_format_t orig_out_format;
	uint64_t in_formats;
	uint64_t out_formats;
	unsigned int format_flags;
};

#define SND_PCM_RATE_PLUGIN_VERSION_OLD	0x010001	/* old rate plugin */
#endif /* DOC_HIDDEN */

/* allocate a channel area and a temporary buffer for the given size */
static snd_pcm_channel_area_t *
rate_alloc_tmp_buf(snd_pcm_format_t format,
		   unsigned int channels, unsigned int frames)
{
	snd_pcm_channel_area_t *ap;
	int width = snd_pcm_format_physical_width(format);
	unsigned int i;

	ap = malloc(sizeof(*ap) * channels);
	if (!ap)
		return NULL;
	ap->addr = malloc(frames * channels * width / 8);
	if (!ap->addr) {
		free(ap);
		return NULL;
	}

	/* set up in interleaved format */
	for (i = 0; i < channels; i++) {
		ap[i].addr = ap[0].addr + (i * width) / 8;
		ap[i].first = 0;
		ap[i].step = width * channels;
	}

	return ap;
}

static void rate_free_tmp_buf(snd_pcm_channel_area_t **ptr)
{
	snd_pcm_channel_area_t *c = *ptr;

	if (c) {
		free(c->addr);
		free(c);
		*ptr = NULL;
	}
}

static int snd_pcm_rate_hw_refine_cprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err;
	snd_pcm_access_mask_t access_mask = { SND_PCM_ACCBIT_SHM };
	snd_pcm_format_mask_t format_mask = { SND_PCM_FMTBIT_LINEAR };
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_ACCESS,
					 &access_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_FORMAT,
					 &format_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_set_subformat(params, SND_PCM_SUBFORMAT_STD);
	if (err < 0)
		return err;
	if (rate->rate_min) {
		err = _snd_pcm_hw_param_set_min(params, SND_PCM_HW_PARAM_RATE,
						rate->rate_min, 0);
		if (err < 0)
			return err;
	}
	if (rate->rate_max) {
		err = _snd_pcm_hw_param_set_max(params, SND_PCM_HW_PARAM_RATE,
						rate->rate_max, 0);
		if (err < 0)
			return err;
	}
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_rate_hw_refine_sprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *sparams)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_access_mask_t saccess_mask = { SND_PCM_ACCBIT_MMAP };
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_set_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				   &saccess_mask);
	if (rate->sformat != SND_PCM_FORMAT_UNKNOWN) {
		_snd_pcm_hw_params_set_format(sparams, rate->sformat);
		_snd_pcm_hw_params_set_subformat(sparams, SND_PCM_SUBFORMAT_STD);
	}
	_snd_pcm_hw_param_set_minmax(sparams, SND_PCM_HW_PARAM_RATE,
				     rate->srate, 0, rate->srate + 1, -1);
	return 0;
}

static int snd_pcm_rate_hw_refine_schange(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
					  snd_pcm_hw_params_t *sparams)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_interval_t t, buffer_size;
	const snd_interval_t *srate, *crate;
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_CHANNELS |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	if (rate->sformat == SND_PCM_FORMAT_UNKNOWN)
		links |= (SND_PCM_HW_PARBIT_FORMAT |
			  SND_PCM_HW_PARBIT_SUBFORMAT |
			  SND_PCM_HW_PARBIT_SAMPLE_BITS |
			  SND_PCM_HW_PARBIT_FRAME_BITS);
	snd_interval_copy(&buffer_size, snd_pcm_hw_param_get_interval(params, SND_PCM_HW_PARAM_BUFFER_SIZE));
	snd_interval_unfloor(&buffer_size);
	crate = snd_pcm_hw_param_get_interval(params, SND_PCM_HW_PARAM_RATE);
	srate = snd_pcm_hw_param_get_interval(sparams, SND_PCM_HW_PARAM_RATE);
	snd_interval_muldiv(&buffer_size, srate, crate, &t);
	err = _snd_pcm_hw_param_set_interval(sparams, SND_PCM_HW_PARAM_BUFFER_SIZE, &t);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_refine(sparams, links, params);
	if (err < 0)
		return err;
	return 0;
}
	
static int snd_pcm_rate_hw_refine_cchange(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
					  snd_pcm_hw_params_t *sparams)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_interval_t t;
#ifdef DEBUG_REFINE
	snd_output_t *out;
#endif
	const snd_interval_t *sbuffer_size, *buffer_size;
	const snd_interval_t *srate, *crate;
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_CHANNELS |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	if (rate->sformat == SND_PCM_FORMAT_UNKNOWN)
		links |= (SND_PCM_HW_PARBIT_FORMAT |
			  SND_PCM_HW_PARBIT_SUBFORMAT |
			  SND_PCM_HW_PARBIT_SAMPLE_BITS |
			  SND_PCM_HW_PARBIT_FRAME_BITS);
	sbuffer_size = snd_pcm_hw_param_get_interval(sparams, SND_PCM_HW_PARAM_BUFFER_SIZE);
	crate = snd_pcm_hw_param_get_interval(params, SND_PCM_HW_PARAM_RATE);
	srate = snd_pcm_hw_param_get_interval(sparams, SND_PCM_HW_PARAM_RATE);
	snd_interval_muldiv(sbuffer_size, crate, srate, &t);
	snd_interval_floor(&t);
	err = _snd_pcm_hw_param_set_interval(params, SND_PCM_HW_PARAM_BUFFER_SIZE, &t);
	if (err < 0)
		return err;
	buffer_size = snd_pcm_hw_param_get_interval(params, SND_PCM_HW_PARAM_BUFFER_SIZE);
	/*
	 * this condition probably needs more work:
	 *   in case when the buffer_size is known and we are looking
	 *   for best period_size, we should prefer situation when
	 *   (buffer_size / period_size) * period_size == buffer_size
	 */
	if (snd_interval_single(buffer_size) && buffer_size->integer) {
		snd_interval_t *period_size;
		period_size = (snd_interval_t *)snd_pcm_hw_param_get_interval(params, SND_PCM_HW_PARAM_PERIOD_SIZE);
		if (!snd_interval_checkempty(period_size) &&
		    period_size->openmin && period_size->openmax &&
		    period_size->min + 1 == period_size->max) {
			if (period_size->min > 0 && (buffer_size->min / period_size->min) * period_size->min == buffer_size->min) {
		    		snd_interval_set_value(period_size, period_size->min);
		    	} else if ((buffer_size->max / period_size->max) * period_size->max == buffer_size->max) {
		    		snd_interval_set_value(period_size, period_size->max);
		    	}
		}
	}
#ifdef DEBUG_REFINE
	snd_output_stdio_attach(&out, stderr, 0);
	snd_output_printf(out, "REFINE (params):\n");
	snd_pcm_hw_params_dump(params, out);
	snd_output_printf(out, "REFINE (slave params):\n");
	snd_pcm_hw_params_dump(sparams, out);
	snd_output_close(out);
#endif
	err = _snd_pcm_hw_params_refine(params, links, sparams);
#ifdef DEBUG_REFINE
	snd_output_stdio_attach(&out, stderr, 0);
	snd_output_printf(out, "********************\n");
	snd_output_printf(out, "REFINE (params) (%i):\n", err);
	snd_pcm_hw_params_dump(params, out);
	snd_output_printf(out, "REFINE (slave params):\n");
	snd_pcm_hw_params_dump(sparams, out);
	snd_output_close(out);
#endif
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_rate_hw_refine(snd_pcm_t *pcm, 
				  snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_rate_hw_refine_cprepare,
				       snd_pcm_rate_hw_refine_cchange,
				       snd_pcm_rate_hw_refine_sprepare,
				       snd_pcm_rate_hw_refine_schange,
				       snd_pcm_generic_hw_refine);
}

/* evaluate the best matching available format to the given format */
static int get_best_format(uint64_t mask, snd_pcm_format_t orig)
{
	int pwidth = snd_pcm_format_physical_width(orig);
	int width = snd_pcm_format_width(orig);
	int signd = snd_pcm_format_signed(orig);
	int best_score = -1;
	int match = -1;
	int f, score;

	for (f = 0; f <= SND_PCM_FORMAT_LAST; f++) {
		if (!(mask & (1ULL << f)))
			continue;
		score = 0;
		if (snd_pcm_format_linear(f)) {
			if (snd_pcm_format_physical_width(f) == pwidth)
				score++;
			if (snd_pcm_format_physical_width(f) >= pwidth)
				score++;
			if (snd_pcm_format_width(f) == width)
				score++;
			if (snd_pcm_format_signed(f) == signd)
				score++;
		}
		if (score > best_score) {
			match = f;
			best_score = score;
		}
	}

	return match;
}

/* set up the input and output formats from the available lists */
static int choose_preferred_format(snd_pcm_rate_t *rate)
{
	uint64_t in_mask = rate->in_formats;
	uint64_t out_mask = rate->out_formats;
	int in, out;

	if (!in_mask || !out_mask)
		return 0;

	if (rate->orig_in_format == rate->orig_out_format)
		if (in_mask & out_mask & (1ULL << rate->orig_in_format))
			return 0; /* nothing changed */

 repeat:
	in = get_best_format(in_mask, rate->orig_in_format);
	out = get_best_format(out_mask, rate->orig_out_format);
	if (in < 0 || out < 0)
		return -ENOENT;

	if ((rate->format_flags & SND_PCM_RATE_FLAG_SYNC_FORMATS) &&
	    in != out) {
		if (out_mask & (1ULL << in))
			out = in;
		else if (in_mask & (1ULL << out))
			in = out;
		else {
			in_mask &= ~(1ULL << in);
			out_mask &= ~(1ULL << out);
			goto repeat;
		}
	}

	rate->info.in.format = in;
	rate->info.out.format = out;
	return 0;
}

static int snd_pcm_rate_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->gen.slave;
	snd_pcm_rate_side_info_t *sinfo, *cinfo;
	unsigned int channels, acc;
	int need_src_buf, need_dst_buf;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_rate_hw_refine_cchange,
					  snd_pcm_rate_hw_refine_sprepare,
					  snd_pcm_rate_hw_refine_schange,
					  snd_pcm_generic_hw_params);
	if (err < 0)
		return err;

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		cinfo = &rate->info.in;
		sinfo = &rate->info.out;
	} else {
		sinfo = &rate->info.in;
		cinfo = &rate->info.out;
	}
	err = INTERNAL(snd_pcm_hw_params_get_format)(params, &cinfo->format);
	if (err < 0)
		return err;
	err = INTERNAL(snd_pcm_hw_params_get_rate)(params, &cinfo->rate, 0);
	if (err < 0)
		return err;
	err = INTERNAL(snd_pcm_hw_params_get_period_size)(params, &cinfo->period_size, 0);
	if (err < 0)
		return err;
	err = INTERNAL(snd_pcm_hw_params_get_buffer_size)(params, &cinfo->buffer_size);
	if (err < 0)
		return err;
	err = INTERNAL(snd_pcm_hw_params_get_channels)(params, &channels);
	if (err < 0)
		return err;
	err = INTERNAL(snd_pcm_hw_params_get_access)(params, &acc);
	if (err < 0)
		return err;

	rate->info.channels = channels;
	sinfo->format = slave->format;
	sinfo->rate = slave->rate;
	sinfo->buffer_size = slave->buffer_size;
	sinfo->period_size = slave->period_size;

	if (CHECK_SANITY(rate->pareas)) {
		SNDMSG("rate plugin already in use");
		return -EBUSY;
	}

	rate->pareas = rate_alloc_tmp_buf(cinfo->format, channels,
					  cinfo->period_size);
	rate->sareas = rate_alloc_tmp_buf(sinfo->format, channels,
					  sinfo->period_size);
	if (!rate->pareas || !rate->sareas) {
		err = -ENOMEM;
		goto error_pareas;
	}

	rate->orig_in_format = rate->info.in.format;
	rate->orig_out_format = rate->info.out.format;
	if (choose_preferred_format(rate) < 0) {
		SNDERR("No matching format in rate plugin");
		err = -EINVAL;
		goto error_pareas;
	}

	err = rate->ops.init(rate->obj, &rate->info);
	if (err < 0)
		goto error_init;

	rate_free_tmp_buf(&rate->src_buf);
	rate_free_tmp_buf(&rate->dst_buf);

	need_src_buf = need_dst_buf = 0;

	if ((rate->format_flags & SND_PCM_RATE_FLAG_INTERLEAVED) &&
	    !(acc == SND_PCM_ACCESS_MMAP_INTERLEAVED ||
	      acc == SND_PCM_ACCESS_RW_INTERLEAVED)) {
		need_src_buf = need_dst_buf = 1;
	} else {
		if (rate->orig_in_format != rate->info.in.format)
			need_src_buf = 1;
		if (rate->orig_out_format != rate->info.out.format)
			need_dst_buf = 1;
	}

	if (need_src_buf) {
		rate->src_conv_idx =
			snd_pcm_linear_convert_index(rate->orig_in_format,
						     rate->info.in.format);
		rate->src_buf = rate_alloc_tmp_buf(rate->info.in.format,
						   channels, rate->info.in.period_size);
		if (!rate->src_buf) {
			err = -ENOMEM;
			goto error;
		}
	}

	if (need_dst_buf) {
		rate->dst_conv_idx =
			snd_pcm_linear_convert_index(rate->info.out.format,
						     rate->orig_out_format);
		rate->dst_buf = rate_alloc_tmp_buf(rate->info.out.format,
						   channels, rate->info.out.period_size);
		if (!rate->dst_buf) {
			err = -ENOMEM;
			goto error;
		}
	}

	return 0;

 error:
	rate_free_tmp_buf(&rate->src_buf);
	rate_free_tmp_buf(&rate->dst_buf);
 error_init:
	if (rate->ops.free)
		rate->ops.free(rate->obj);
 error_pareas:
	rate_free_tmp_buf(&rate->pareas);
	rate_free_tmp_buf(&rate->sareas);
	return err;
}

static int snd_pcm_rate_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;

	rate_free_tmp_buf(&rate->pareas);
	rate_free_tmp_buf(&rate->sareas);
	if (rate->ops.free)
		rate->ops.free(rate->obj);
	rate_free_tmp_buf(&rate->src_buf);
	rate_free_tmp_buf(&rate->dst_buf);
	return snd_pcm_hw_free(rate->gen.slave);
}

static void recalc(snd_pcm_t *pcm, snd_pcm_uframes_t *val)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->gen.slave;
	unsigned long div;

	if (*val == pcm->buffer_size) {
		*val = slave->buffer_size;
	} else {
		div = *val / pcm->period_size;
		if (div * pcm->period_size == *val)
			*val = div * slave->period_size;
		else
			*val = muldiv_near(*val, slave->period_size, pcm->period_size);
	}
}

static int snd_pcm_rate_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->gen.slave;
	snd_pcm_sw_params_t *sparams;
	snd_pcm_uframes_t boundary1, boundary2, sboundary;
	int err;

	sparams = &rate->sw_params;
	err = snd_pcm_sw_params_current(slave, sparams);
	if (err < 0)
		return err;
	sboundary = sparams->boundary;
	*sparams = *params;
	boundary1 = pcm->buffer_size;
	boundary2 = slave->buffer_size;
	while (boundary1 * 2 <= LONG_MAX - pcm->buffer_size &&
	       boundary2 * 2 <= LONG_MAX - slave->buffer_size) {
		boundary1 *= 2;
		boundary2 *= 2;
	}
	params->boundary = boundary1;
	sparams->boundary = sboundary;

	if (rate->ops.adjust_pitch)
		rate->ops.adjust_pitch(rate->obj, &rate->info);

	recalc(pcm, &sparams->avail_min);
	rate->orig_avail_min = sparams->avail_min;
	recalc(pcm, &sparams->start_threshold);
	if (sparams->avail_min < 1) sparams->avail_min = 1;
	if (sparams->start_threshold <= slave->buffer_size) {
		if (sparams->start_threshold > (slave->buffer_size / sparams->avail_min) * sparams->avail_min)
			sparams->start_threshold = (slave->buffer_size / sparams->avail_min) * sparams->avail_min;
	}
	if (sparams->stop_threshold >= params->boundary) {
		sparams->stop_threshold = sparams->boundary;
	} else {
		recalc(pcm, &sparams->stop_threshold);
	}
	recalc(pcm, &sparams->silence_threshold);
	if (sparams->silence_size >= params->boundary) {
		sparams->silence_size = sparams->boundary;
	} else {
		recalc(pcm, &sparams->silence_size);
	}
	return snd_pcm_sw_params(slave, sparams);
}

static int snd_pcm_rate_init(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;

	if (rate->ops.reset)
		rate->ops.reset(rate->obj);
	rate->last_commit_ptr = 0;
	rate->start_pending = 0;
	return 0;
}

static void do_convert(const snd_pcm_channel_area_t *dst_areas,
		       snd_pcm_uframes_t dst_offset, unsigned int dst_frames,
		       const snd_pcm_channel_area_t *src_areas,
		       snd_pcm_uframes_t src_offset, unsigned int src_frames,
		       unsigned int channels,
		       snd_pcm_rate_t *rate)
{
	const snd_pcm_channel_area_t *out_areas;
	snd_pcm_uframes_t out_offset;

	if (rate->dst_buf) {
		out_areas = rate->dst_buf;
		out_offset = 0;
	} else {
		out_areas = dst_areas;
		out_offset = dst_offset;
	}

	if (rate->src_buf) {
		snd_pcm_linear_convert(rate->src_buf, 0,
				       src_areas, src_offset,
				       channels, src_frames,
				       rate->src_conv_idx);
		src_areas = rate->src_buf;
		src_offset = 0;
	}

	if (rate->ops.convert)
		rate->ops.convert(rate->obj, out_areas, out_offset, dst_frames,
				   src_areas, src_offset, src_frames);
	else
		rate->ops.convert_s16(rate->obj,
				      snd_pcm_channel_area_addr(out_areas, out_offset),
				      dst_frames,
				      snd_pcm_channel_area_addr(src_areas, src_offset),
				      src_frames);
	if (rate->dst_buf)
		snd_pcm_linear_convert(dst_areas, dst_offset,
				       rate->dst_buf, 0,
				       channels, dst_frames,
				       rate->dst_conv_idx);
}

static inline void
snd_pcm_rate_write_areas1(snd_pcm_t *pcm,
			 const snd_pcm_channel_area_t *areas,
			 snd_pcm_uframes_t offset,
			 const snd_pcm_channel_area_t *slave_areas,
			 snd_pcm_uframes_t slave_offset)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	do_convert(slave_areas, slave_offset, rate->gen.slave->period_size,
		   areas, offset, pcm->period_size,
		   pcm->channels, rate);
}

static inline void
snd_pcm_rate_read_areas1(snd_pcm_t *pcm,
			 const snd_pcm_channel_area_t *areas,
			 snd_pcm_uframes_t offset,
			 const snd_pcm_channel_area_t *slave_areas,
			 snd_pcm_uframes_t slave_offset)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	do_convert(areas, offset, pcm->period_size,
		   slave_areas, slave_offset, rate->gen.slave->period_size,
		   pcm->channels, rate);
}

static inline void snd_pcm_rate_sync_hwptr0(snd_pcm_t *pcm, snd_pcm_uframes_t slave_hw_ptr)
{
	snd_pcm_rate_t *rate;
	snd_pcm_sframes_t slave_hw_ptr_diff;
	snd_pcm_sframes_t last_slave_hw_ptr_frac;

	if (pcm->stream != SND_PCM_STREAM_PLAYBACK)
		return;

	rate = pcm->private_data;
	slave_hw_ptr_diff = pcm_frame_diff(slave_hw_ptr, rate->last_slave_hw_ptr, rate->gen.slave->boundary);
	if (slave_hw_ptr_diff == 0)
		return;
	last_slave_hw_ptr_frac = rate->last_slave_hw_ptr % rate->gen.slave->period_size;
	/* While handling fraction part fo slave period, rounded value will be
	 * introduced by input_frames().
	 * To eliminate rounding issue on rate->hw_ptr, subtract last rounded
	 * value from rate->hw_ptr and add new rounded value of present
	 * slave_hw_ptr fraction part to rate->hw_ptr. Hence,
	 * rate->hw_ptr += [ (no. of updated slave periods * pcm rate period size) -
	 * 	fractional part of last_slave_hw_ptr rounded value +
	 * 	fractional part of updated slave hw ptr's rounded value ]
	 */
	rate->hw_ptr += (
			(((last_slave_hw_ptr_frac + slave_hw_ptr_diff) / rate->gen.slave->period_size) * pcm->period_size) -
			rate->ops.input_frames(rate->obj, last_slave_hw_ptr_frac) +
			rate->ops.input_frames(rate->obj, (last_slave_hw_ptr_frac + slave_hw_ptr_diff) % rate->gen.slave->period_size));
	rate->last_slave_hw_ptr = slave_hw_ptr;

	rate->hw_ptr %= pcm->boundary;
}

static inline void snd_pcm_rate_sync_hwptr(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_rate_sync_hwptr0(pcm, *rate->gen.slave->hw.ptr);
}

static int snd_pcm_rate_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err = snd_pcm_hwsync(rate->gen.slave);
	if (err < 0)
		return err;
	snd_pcm_rate_sync_hwptr(pcm);
	return 0;
}

static snd_pcm_uframes_t snd_pcm_rate_playback_internal_delay(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;

	return pcm_frame_diff(rate->appl_ptr, rate->last_commit_ptr, pcm->boundary);
}

static int snd_pcm_rate_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_sframes_t slave_delay;
	int err;

	snd_pcm_rate_hwsync(pcm);

	err = snd_pcm_delay(rate->gen.slave, &slave_delay);
	if (err < 0) {
		return err;
	}

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		*delayp = rate->ops.input_frames(rate->obj, slave_delay)
				+ snd_pcm_rate_playback_internal_delay(pcm);
	} else {
		*delayp = rate->ops.output_frames(rate->obj, slave_delay)
				+ snd_pcm_mmap_capture_delay(pcm);
	}
	return 0;
}

static int snd_pcm_rate_prepare(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err;

	err = snd_pcm_prepare(rate->gen.slave);
	if (err < 0)
		return err;
	*pcm->hw.ptr = 0;
	*pcm->appl.ptr = 0;
	rate->last_slave_hw_ptr = 0;
	err = snd_pcm_rate_init(pcm);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_rate_reset(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err;
	err = snd_pcm_reset(rate->gen.slave);
	if (err < 0)
		return err;
	*pcm->hw.ptr = 0;
	*pcm->appl.ptr = 0;
	rate->last_slave_hw_ptr = 0;
	err = snd_pcm_rate_init(pcm);
	if (err < 0)
		return err;
	return 0;
}

static snd_pcm_sframes_t snd_pcm_rate_rewindable(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static snd_pcm_sframes_t snd_pcm_rate_forwardable(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static snd_pcm_sframes_t snd_pcm_rate_rewind(snd_pcm_t *pcm ATTRIBUTE_UNUSED,
                                             snd_pcm_uframes_t frames ATTRIBUTE_UNUSED)
{
        return 0;
}

static snd_pcm_sframes_t snd_pcm_rate_forward(snd_pcm_t *pcm ATTRIBUTE_UNUSED,
                                              snd_pcm_uframes_t frames ATTRIBUTE_UNUSED)
{
        return 0;
}

static int snd_pcm_rate_commit_area(snd_pcm_t *pcm, snd_pcm_rate_t *rate,
				    snd_pcm_uframes_t appl_offset,
				    snd_pcm_uframes_t size ATTRIBUTE_UNUSED,
				    snd_pcm_uframes_t slave_size)
{
	snd_pcm_uframes_t cont = pcm->buffer_size - appl_offset;
	const snd_pcm_channel_area_t *areas;
	const snd_pcm_channel_area_t *slave_areas;
	snd_pcm_uframes_t slave_offset, xfer;
	snd_pcm_uframes_t slave_frames = ULONG_MAX;
	snd_pcm_sframes_t result;

	areas = snd_pcm_mmap_areas(pcm);
	/*
	 * Because snd_pcm_rate_write_areas1() below will convert a full source period
	 * then there had better be a full period available in the current buffer.
	 */
	if (cont >= pcm->period_size) {
		result = snd_pcm_mmap_begin(rate->gen.slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
		/*
		 * Because snd_pcm_rate_write_areas1() below will convert to a full slave period
		 * then there had better be a full slave period available in the slave buffer.
		 */
		if (slave_frames < rate->gen.slave->period_size) {
			snd_pcm_rate_write_areas1(pcm, areas, appl_offset, rate->sareas, 0);
			goto __partial;
		}
		snd_pcm_rate_write_areas1(pcm, areas, appl_offset,
					  slave_areas, slave_offset);
		/* Only commit the requested slave_size, even if more was actually converted */
		result = snd_pcm_mmap_commit(rate->gen.slave, slave_offset, slave_size);
		if (result < (snd_pcm_sframes_t)slave_size) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->gen.slave, result);
			if (result < 0)
				return result;
			return 0;
		}
	} else {
		snd_pcm_areas_copy(rate->pareas, 0,
				   areas, appl_offset,
				   pcm->channels, cont,
				   pcm->format);
		snd_pcm_areas_copy(rate->pareas, cont,
				   areas, 0,
				   pcm->channels, pcm->period_size - cont,
				   pcm->format);

		snd_pcm_rate_write_areas1(pcm, rate->pareas, 0, rate->sareas, 0);

		/* ok, commit first fragment */
		result = snd_pcm_mmap_begin(rate->gen.slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
	      __partial:
		cont = slave_frames;
		if (cont > slave_size)
			cont = slave_size;
		snd_pcm_areas_copy(slave_areas, slave_offset,
				   rate->sareas, 0,
				   pcm->channels, cont,
				   rate->gen.slave->format);
		result = snd_pcm_mmap_commit(rate->gen.slave, slave_offset, cont);
		if (result < (snd_pcm_sframes_t)cont) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->gen.slave, result);
			if (result < 0)
				return result;
			return 0;
		}
		xfer = cont;

		if (xfer == slave_size)
			goto commit_done;
		
		/* commit second fragment */
		cont = slave_size - cont;
		slave_frames = cont;
		result = snd_pcm_mmap_begin(rate->gen.slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
#if 0
		if (slave_offset) {
			SNDERR("non-zero slave_offset %ld", slave_offset);
			return -EIO;
		}
#endif
		snd_pcm_areas_copy(slave_areas, slave_offset,
				   rate->sareas, xfer,
				   pcm->channels, cont,
				   rate->gen.slave->format);
		result = snd_pcm_mmap_commit(rate->gen.slave, slave_offset, cont);
		if (result < (snd_pcm_sframes_t)cont) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->gen.slave, result + xfer);
			if (result < 0)
				return result;
			return 0;
		}
	}

 commit_done:
	if (rate->start_pending) {
		/* we have pending start-trigger.  let's issue it now */
		snd_pcm_start(rate->gen.slave);
		rate->start_pending = 0;
	}
	return 1;
}

static int snd_pcm_rate_commit_next_period(snd_pcm_t *pcm, snd_pcm_uframes_t appl_offset)
{
	snd_pcm_rate_t *rate = pcm->private_data;

	return snd_pcm_rate_commit_area(pcm, rate, appl_offset, pcm->period_size,
					rate->gen.slave->period_size);
}

static int snd_pcm_rate_grab_next_period(snd_pcm_t *pcm, snd_pcm_uframes_t hw_offset)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_uframes_t cont = pcm->buffer_size - hw_offset;
	const snd_pcm_channel_area_t *areas;
	const snd_pcm_channel_area_t *slave_areas;
	snd_pcm_uframes_t slave_offset, xfer;
	snd_pcm_uframes_t slave_frames = ULONG_MAX;
	snd_pcm_sframes_t result;

	areas = snd_pcm_mmap_areas(pcm);
	if (cont >= pcm->period_size) {
		result = snd_pcm_mmap_begin(rate->gen.slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
		if (slave_frames < rate->gen.slave->period_size)
			goto __partial;
		snd_pcm_rate_read_areas1(pcm, areas, hw_offset,
					 slave_areas, slave_offset);
		result = snd_pcm_mmap_commit(rate->gen.slave, slave_offset, rate->gen.slave->period_size);
		if (result < (snd_pcm_sframes_t)rate->gen.slave->period_size) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->gen.slave, result);
			if (result < 0)
				return result;
			return 0;
		}
	} else {
		/* ok, grab first fragment */
		result = snd_pcm_mmap_begin(rate->gen.slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
	      __partial:
		cont = slave_frames;
		if (cont > rate->gen.slave->period_size)
			cont = rate->gen.slave->period_size;
		snd_pcm_areas_copy(rate->sareas, 0,
				   slave_areas, slave_offset,
				   pcm->channels, cont,
				   rate->gen.slave->format);
		result = snd_pcm_mmap_commit(rate->gen.slave, slave_offset, cont);
		if (result < (snd_pcm_sframes_t)cont) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->gen.slave, result);
			if (result < 0)
				return result;
			return 0;
		}
		xfer = cont;

		if (xfer == rate->gen.slave->period_size)
			goto __transfer;

		/* grab second fragment */
		cont = rate->gen.slave->period_size - cont;
		slave_frames = cont;
		result = snd_pcm_mmap_begin(rate->gen.slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
#if 0
		if (slave_offset) {
			SNDERR("non-zero slave_offset %ld", slave_offset);
			return -EIO;
		}
#endif
		snd_pcm_areas_copy(rate->sareas, xfer,
		                   slave_areas, slave_offset,
				   pcm->channels, cont,
				   rate->gen.slave->format);
		result = snd_pcm_mmap_commit(rate->gen.slave, slave_offset, cont);
		if (result < (snd_pcm_sframes_t)cont) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->gen.slave, result + xfer);
			if (result < 0)
				return result;
			return 0;
		}

	      __transfer:
		cont = pcm->buffer_size - hw_offset;
		if (cont >= pcm->period_size) {
			snd_pcm_rate_read_areas1(pcm, areas, hw_offset,
						 rate->sareas, 0);
		} else {
			snd_pcm_rate_read_areas1(pcm,
						 rate->pareas, 0,
						 rate->sareas, 0);
			snd_pcm_areas_copy(areas, hw_offset,
					   rate->pareas, 0,
					   pcm->channels, cont,
					   pcm->format);
			snd_pcm_areas_copy(areas, 0,
					   rate->pareas, cont,
					   pcm->channels, pcm->period_size - cont,
					   pcm->format);
		}
	}
	return 1;
}

static int snd_pcm_rate_sync_playback_area(snd_pcm_t *pcm, snd_pcm_uframes_t appl_ptr)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->gen.slave;
	snd_pcm_uframes_t xfer;
	snd_pcm_sframes_t slave_size;
	int err;

	slave_size = snd_pcm_avail_update(slave);
	if (slave_size < 0)
		return slave_size;

	xfer = pcm_frame_diff(appl_ptr, rate->last_commit_ptr, pcm->boundary);
	while (xfer >= pcm->period_size &&
	       (snd_pcm_uframes_t)slave_size >= rate->gen.slave->period_size) {
		err = snd_pcm_rate_commit_next_period(pcm, rate->last_commit_ptr % pcm->buffer_size);
		if (err == 0)
			break;
		if (err < 0)
			return err;
		xfer -= pcm->period_size;
		slave_size -= rate->gen.slave->period_size;
		rate->last_commit_ptr += pcm->period_size;
		if (rate->last_commit_ptr >= pcm->boundary)
			rate->last_commit_ptr -= pcm->boundary;
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_rate_mmap_commit(snd_pcm_t *pcm,
						  snd_pcm_uframes_t offset ATTRIBUTE_UNUSED,
						  snd_pcm_uframes_t size)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err;

	if (size == 0)
		return 0;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		err = snd_pcm_rate_sync_playback_area(pcm, rate->appl_ptr + size);
		if (err < 0)
			return err;
	}
	snd_pcm_mmap_appl_forward(pcm, size);
	return size;
}

static snd_pcm_sframes_t snd_pcm_rate_avail_update_capture(snd_pcm_t *pcm,
							   snd_pcm_sframes_t slave_size)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->gen.slave;
	snd_pcm_uframes_t xfer, hw_offset, size;
	
	xfer = snd_pcm_mmap_capture_avail(pcm);
	size = pcm->buffer_size - xfer;
	hw_offset = snd_pcm_mmap_hw_offset(pcm);
	while (size >= pcm->period_size &&
	       (snd_pcm_uframes_t)slave_size >= slave->period_size) {
		int err = snd_pcm_rate_grab_next_period(pcm, hw_offset);
		if (err < 0)
			return err;
		if (err == 0)
			return (snd_pcm_sframes_t)xfer;
		xfer += pcm->period_size;
		size -= pcm->period_size;
		slave_size -= slave->period_size;
		hw_offset += pcm->period_size;
		hw_offset %= pcm->buffer_size;
		snd_pcm_mmap_hw_forward(pcm, pcm->period_size);
	}
	return (snd_pcm_sframes_t)xfer;
}

static snd_pcm_sframes_t snd_pcm_rate_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_sframes_t slave_size;

	slave_size = snd_pcm_avail_update(rate->gen.slave);
	if (slave_size < 0)
		return slave_size;

	if (pcm->stream == SND_PCM_STREAM_CAPTURE)
		return snd_pcm_rate_avail_update_capture(pcm, slave_size);

	snd_pcm_rate_sync_hwptr(pcm);
	snd_pcm_rate_sync_playback_area(pcm, rate->appl_ptr);
	return snd_pcm_mmap_avail(pcm);
}

static int snd_pcm_rate_htimestamp(snd_pcm_t *pcm,
				   snd_pcm_uframes_t *avail,
				   snd_htimestamp_t *tstamp)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_sframes_t avail1;
	snd_pcm_uframes_t tmp;
	int ok = 0, err;

	while (1) {
		/* the position is from this plugin itself */
		avail1 = snd_pcm_avail_update(pcm);
		if (avail1 < 0)
			return avail1;
		if (ok && (snd_pcm_uframes_t)avail1 == *avail)
			break;
		*avail = avail1;
		/* timestamp is taken from the slave PCM */
		err = snd_pcm_htimestamp(rate->gen.slave, &tmp, tstamp);
		if (err < 0)
			return err;
		ok = 1;
	}
	return 0;
}

static int snd_pcm_rate_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		/* Try to sync as much as possible */
		snd_pcm_rate_hwsync(pcm);
		snd_pcm_rate_sync_playback_area(pcm, rate->appl_ptr);
	}
	return snd_pcm_poll_descriptors_revents(rate->gen.slave, pfds, nfds, revents);
}

/* locking */
static int snd_pcm_rate_drain(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		/* commit the remaining fraction (if any) */
		snd_pcm_uframes_t size, ofs, saved_avail_min;
		snd_pcm_sw_params_t sw_params;
		int commit_err = 0;

		__snd_pcm_lock(pcm);
		/* temporarily set avail_min to one */
		sw_params = rate->sw_params;
		saved_avail_min = sw_params.avail_min;
		sw_params.avail_min = 1;
		snd_pcm_sw_params(rate->gen.slave, &sw_params);

		size = pcm_frame_diff(rate->appl_ptr, rate->last_commit_ptr, pcm->boundary);
		ofs = rate->last_commit_ptr % pcm->buffer_size;
		while (size > 0) {
			snd_pcm_uframes_t psize, spsize;
			int err;

			err = __snd_pcm_wait_in_lock(rate->gen.slave, SND_PCM_WAIT_DRAIN);
			if (err < 0)
				break;
			if (size > pcm->period_size) {
				psize = pcm->period_size;
				spsize = rate->gen.slave->period_size;
			} else {
				psize = size;
				spsize = rate->ops.output_frames(rate->obj, size);
				if (! spsize)
					break;
			}
			commit_err = snd_pcm_rate_commit_area(pcm, rate, ofs,
						 psize, spsize);
			if (commit_err == 1) {
				rate->last_commit_ptr += psize;
				if (rate->last_commit_ptr >= pcm->boundary)
					rate->last_commit_ptr -= pcm->boundary;
			} else if (commit_err == 0) {
				if (pcm->mode & SND_PCM_NONBLOCK) {
					commit_err = -EAGAIN;
					break;
				}
				continue;
			} else
				break;

			ofs = (ofs + psize) % pcm->buffer_size;
			size -= psize;
		}
		sw_params.avail_min = saved_avail_min;
		snd_pcm_sw_params(rate->gen.slave, &sw_params);
		__snd_pcm_unlock(pcm);
		if (commit_err < 0)
			return commit_err;
	}
	return snd_pcm_drain(rate->gen.slave);
}

static snd_pcm_state_t snd_pcm_rate_state(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	if (rate->start_pending) /* pseudo-state */
		return SND_PCM_STATE_RUNNING;
	return snd_pcm_state(rate->gen.slave);
}


static int snd_pcm_rate_start(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_sframes_t avail;
		
	if (pcm->stream == SND_PCM_STREAM_CAPTURE)
		return snd_pcm_start(rate->gen.slave);

	if (snd_pcm_state(rate->gen.slave) != SND_PCM_STATE_PREPARED)
		return -EBADFD;

	gettimestamp(&rate->trigger_tstamp, pcm->tstamp_type);

	avail = snd_pcm_mmap_playback_hw_avail(rate->gen.slave);
	if (avail < 0) /* can't happen on healthy drivers */
		return -EBADFD;

	if (avail == 0) {
		/* postpone the trigger since we have no data committed yet */
		rate->start_pending = 1;
		return 0;
	}
	rate->start_pending = 0;
	return snd_pcm_start(rate->gen.slave);
}

static int snd_pcm_rate_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_sframes_t err;

	err = snd_pcm_status(rate->gen.slave, status);
	if (err < 0)
		return err;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		if (rate->start_pending)
			status->state = SND_PCM_STATE_RUNNING;
		status->trigger_tstamp = rate->trigger_tstamp;
	}
	snd_pcm_rate_sync_hwptr0(pcm, status->hw_ptr);
	status->appl_ptr = *pcm->appl.ptr;
	status->hw_ptr = *pcm->hw.ptr;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		status->delay = rate->ops.input_frames(rate->obj, status->delay)
					+ snd_pcm_rate_playback_internal_delay(pcm);
		status->avail = snd_pcm_mmap_playback_avail(pcm);
		status->avail_max = rate->ops.input_frames(rate->obj, status->avail_max);
	} else {
		status->delay = rate->ops.output_frames(rate->obj, status->delay)
					+ snd_pcm_mmap_capture_delay(pcm);
		status->avail = snd_pcm_mmap_capture_avail(pcm);
		status->avail_max = rate->ops.output_frames(rate->obj, status->avail_max);
	}
	return 0;
}

static void snd_pcm_rate_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	if (rate->sformat == SND_PCM_FORMAT_UNKNOWN)
		snd_output_printf(out, "Rate conversion PCM (%d)\n", 
			rate->srate);
	else
		snd_output_printf(out, "Rate conversion PCM (%d, sformat=%s)\n", 
			rate->srate,
			snd_pcm_format_name(rate->sformat));
	if (rate->ops.dump)
		rate->ops.dump(rate->obj, out);
	snd_output_printf(out, "Protocol version: %x\n", rate->plugin_version);
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(rate->gen.slave, out);
}

static int snd_pcm_rate_close(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;

	if (rate->ops.close)
		rate->ops.close(rate->obj);
	if (rate->open_func)
		snd_dlobj_cache_put(rate->open_func);
	return snd_pcm_generic_close(pcm);
}

/**
 * \brief Convert rate pcm frames to corresponding rate slave pcm frames
 * \param pcm PCM handle
 * \param frames Frames to be converted to slave frames
 * \retval Corresponding slave frames
*/
static snd_pcm_uframes_t snd_pcm_rate_slave_frames(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_uframes_t sframes;
	snd_pcm_rate_t *rate = pcm->private_data;

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		sframes = rate->ops.output_frames(rate->obj, frames);
	else
		sframes = rate->ops.input_frames(rate->obj, frames);

	return sframes;
}

static int snd_pcm_rate_may_wait_for_avail_min(snd_pcm_t *pcm,
					       snd_pcm_uframes_t avail)
{
	return snd_pcm_plugin_may_wait_for_avail_min_conv(pcm, avail,
							  snd_pcm_rate_slave_frames);
}

static const snd_pcm_fast_ops_t snd_pcm_rate_fast_ops = {
	.status = snd_pcm_rate_status,
	.state = snd_pcm_rate_state,
	.hwsync = snd_pcm_rate_hwsync,
	.delay = snd_pcm_rate_delay,
	.prepare = snd_pcm_rate_prepare,
	.reset = snd_pcm_rate_reset,
	.start = snd_pcm_rate_start,
	.drop = snd_pcm_generic_drop,
	.drain = snd_pcm_rate_drain,
	.pause = snd_pcm_generic_pause,
	.rewindable = snd_pcm_rate_rewindable,
	.rewind = snd_pcm_rate_rewind,
	.forwardable = snd_pcm_rate_forwardable,
	.forward = snd_pcm_rate_forward,
	.resume = snd_pcm_generic_resume,
	.writei = snd_pcm_mmap_writei,
	.writen = snd_pcm_mmap_writen,
	.readi = snd_pcm_mmap_readi,
	.readn = snd_pcm_mmap_readn,
	.avail_update = snd_pcm_rate_avail_update,
	.mmap_commit = snd_pcm_rate_mmap_commit,
	.htimestamp = snd_pcm_rate_htimestamp,
	.poll_descriptors_count = snd_pcm_generic_poll_descriptors_count,
	.poll_descriptors = snd_pcm_generic_poll_descriptors,
	.poll_revents = snd_pcm_rate_poll_revents,
	.may_wait_for_avail_min = snd_pcm_rate_may_wait_for_avail_min,
};

static const snd_pcm_ops_t snd_pcm_rate_ops = {
	.close = snd_pcm_rate_close,
	.info = snd_pcm_generic_info,
	.hw_refine = snd_pcm_rate_hw_refine,
	.hw_params = snd_pcm_rate_hw_params,
	.hw_free = snd_pcm_rate_hw_free,
	.sw_params = snd_pcm_rate_sw_params,
	.channel_info = snd_pcm_generic_channel_info,
	.dump = snd_pcm_rate_dump,
	.nonblock = snd_pcm_generic_nonblock,
	.async = snd_pcm_generic_async,
	.mmap = snd_pcm_generic_mmap,
	.munmap = snd_pcm_generic_munmap,
	.query_chmaps = snd_pcm_generic_query_chmaps,
	.get_chmap = snd_pcm_generic_get_chmap,
	.set_chmap = snd_pcm_generic_set_chmap,
};

/**
 * \brief Get a default converter string
 * \param root Root configuration node
 * \retval A const config item if found, or NULL
 */
const snd_config_t *snd_pcm_rate_get_default_converter(snd_config_t *root)
{
	snd_config_t *n;
	/* look for default definition */
	if (snd_config_search(root, "defaults.pcm.rate_converter", &n) >= 0)
		return n;
	return NULL;
}

static void rate_initial_setup(snd_pcm_rate_t *rate)
{
	if (rate->plugin_version == SND_PCM_RATE_PLUGIN_VERSION)
		rate->plugin_version = rate->ops.version;

	if (rate->plugin_version >= 0x010002 &&
	    rate->ops.get_supported_rates)
		rate->ops.get_supported_rates(rate->obj,
					      &rate->rate_min,
					      &rate->rate_max);

	if (rate->plugin_version >= 0x010003 &&
	    rate->ops.get_supported_formats) {
		rate->ops.get_supported_formats(rate->obj,
						&rate->in_formats,
						&rate->out_formats,
						&rate->format_flags);
	} else if (!rate->ops.convert && rate->ops.convert_s16) {
		rate->in_formats = rate->out_formats =
			1ULL << SND_PCM_FORMAT_S16;
		rate->format_flags = SND_PCM_RATE_FLAG_INTERLEAVED;
	}
}

#ifdef PIC
static int is_builtin_plugin(const char *type)
{
	return strcmp(type, "linear") == 0;
}

static const char *const default_rate_plugins[] = {
	"speexrate", "linear", NULL
};

static int rate_open_func(snd_pcm_rate_t *rate, const char *type, const snd_config_t *converter_conf, int verbose)
{
	char open_name[64], open_conf_name[64], lib_name[64], *lib = NULL;
	snd_pcm_rate_open_func_t open_func;
	snd_pcm_rate_open_conf_func_t open_conf_func;
	int err;

	snprintf(open_name, sizeof(open_name), "_snd_pcm_rate_%s_open", type);
	snprintf(open_conf_name, sizeof(open_conf_name), "_snd_pcm_rate_%s_open_conf", type);
	if (!is_builtin_plugin(type)) {
		snprintf(lib_name, sizeof(lib_name),
				 "libasound_module_rate_%s.so", type);
		lib = lib_name;
	}

	open_conf_func = snd_dlobj_cache_get(lib, open_conf_name, NULL, verbose && converter_conf != NULL);
	if (open_conf_func) {
		err = open_conf_func(SND_PCM_RATE_PLUGIN_VERSION,
				     &rate->obj, &rate->ops, converter_conf);
		if (!err) {
			rate->open_func = open_conf_func;
			return 0;
		} else {
			snd_dlobj_cache_put(open_conf_func);
			return err;
		}
	}

	open_func = snd_dlobj_cache_get(lib, open_name, NULL, verbose);
	if (!open_func)
		return -ENOENT;

	rate->open_func = open_func;

	err = open_func(SND_PCM_RATE_PLUGIN_VERSION, &rate->obj, &rate->ops);
	if (!err)
		return 0;

	/* try to open with the old protocol version */
	rate->plugin_version = SND_PCM_RATE_PLUGIN_VERSION_OLD;
	err = open_func(SND_PCM_RATE_PLUGIN_VERSION_OLD,
			&rate->obj, &rate->ops);
	if (!err)
		return 0;

	snd_dlobj_cache_put(open_func);
	rate->open_func = NULL;
	return err;
}
#endif

/*
 * If the conf is an array of alternatives then the id of
 * the first element will be "0" (or maybe NULL). Otherwise assume it is
 * a structure.
 */
static int is_string_array(const snd_config_t *conf)
{
	snd_config_iterator_t i;

	if (snd_config_get_type(conf) != SND_CONFIG_TYPE_COMPOUND)
		return 0;

	i = snd_config_iterator_first(conf);
	if (i && i != snd_config_iterator_end(conf)) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			return 0;
		if (id && strcmp(id, "0") != 0)
			return 0;
	}

	return 1;
}

/**
 * \brief Creates a new rate PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param sformat Slave format
 * \param srate Slave rate
 * \param converter SRC type string node
 * \param slave Slave PCM handle
 * \param close_slave When set, the slave PCM handle is closed with copy PCM
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_rate_open(snd_pcm_t **pcmp, const char *name,
		      snd_pcm_format_t sformat, unsigned int srate,
		      const snd_config_t *converter,
		      snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_rate_t *rate;
	const char *type = NULL;
	int err;
#ifndef PIC
	snd_pcm_rate_open_func_t open_func;
	extern int SND_PCM_RATE_PLUGIN_ENTRY(linear) (unsigned int version, void **objp, snd_pcm_rate_ops_t *ops);
#endif

	assert(pcmp && slave);
	if (sformat != SND_PCM_FORMAT_UNKNOWN &&
	    snd_pcm_format_linear(sformat) != 1)
		return -EINVAL;
	rate = calloc(1, sizeof(snd_pcm_rate_t));
	if (!rate) {
		return -ENOMEM;
	}
	rate->gen.slave = slave;
	rate->gen.close_slave = close_slave;
	rate->srate = srate;
	rate->sformat = sformat;

	rate->rate_min = SND_PCM_PLUGIN_RATE_MIN;
	rate->rate_max = SND_PCM_PLUGIN_RATE_MAX;
	rate->plugin_version = SND_PCM_RATE_PLUGIN_VERSION;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_RATE, name, slave->stream, slave->mode);
	if (err < 0) {
		free(rate);
		return err;
	}

#ifdef PIC
	err = -ENOENT;
	if (!converter) {
		const char *const *types;
		for (types = default_rate_plugins; *types; types++) {
			err = rate_open_func(rate, *types, NULL, 0);
			if (!err) {
				type = *types;
				break;
			}
		}
	} else if (!snd_config_get_string(converter, &type))
		err = rate_open_func(rate, type, NULL, 1);
	else if (is_string_array(converter)) {
		snd_config_iterator_t i, next;
		snd_config_for_each(i, next, converter) {
			snd_config_t *n = snd_config_iterator_entry(i);
			if (snd_config_get_string(n, &type) < 0)
				break;
			err = rate_open_func(rate, type, NULL, 0);
			if (!err)
				break;
		}
	} else if (snd_config_get_type(converter) == SND_CONFIG_TYPE_COMPOUND) {
		snd_config_iterator_t i, next;
		snd_config_for_each(i, next, converter) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			if (snd_config_get_id(n, &id) < 0)
				continue;
			if (strcmp(id, "name") != 0)
				continue;
			snd_config_get_string(n, &type);
			break;
		}
		if (!type) {
			SNDERR("No name given for rate converter");
			snd_pcm_free(pcm);
			free(rate);
			return -EINVAL;
		}
		err = rate_open_func(rate, type, converter, 1);
	} else {
		SNDERR("Invalid type for rate converter");
		snd_pcm_free(pcm);
		free(rate);
		return -EINVAL;
	}
	if (err < 0) {
		SNDERR("Cannot find rate converter");
		snd_pcm_free(pcm);
		free(rate);
		return -ENOENT;
	}
#else
	type = "linear";
	open_func = SND_PCM_RATE_PLUGIN_ENTRY(linear);
	err = open_func(SND_PCM_RATE_PLUGIN_VERSION, &rate->obj, &rate->ops);
	if (err < 0) {
		snd_pcm_free(pcm);
		free(rate);
		return err;
	}
#endif

	if (! rate->ops.init || ! (rate->ops.convert || rate->ops.convert_s16) ||
	    ! rate->ops.input_frames || ! rate->ops.output_frames) {
		SNDERR("Inproper rate plugin %s initialization", type);
		snd_pcm_free(pcm);
		free(rate);
		return err;
	}

	rate_initial_setup(rate);

	pcm->ops = &snd_pcm_rate_ops;
	pcm->fast_ops = &snd_pcm_rate_fast_ops;
	pcm->private_data = rate;
	pcm->poll_fd = slave->poll_fd;
	pcm->poll_events = slave->poll_events;
	pcm->mmap_rw = 1;
	pcm->tstamp_type = slave->tstamp_type;
	snd_pcm_set_hw_ptr(pcm, &rate->hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &rate->appl_ptr, -1, 0);
	*pcmp = pcm;

	return 0;
}

/*! \page pcm_plugins

\section pcm_plugins_rate Plugin: Rate

This plugin converts a stream rate. The input and output formats must be linear.

\code
pcm.name {
	type rate               # Rate PCM
        slave STR               # Slave name
        # or
        slave {                 # Slave definition
                pcm STR         # Slave PCM name
                # or
                pcm { }         # Slave PCM definition
                rate INT        # Slave rate
                [format STR]    # Slave format
        }
	converter STR			# optional
	# or
	converter [ STR1 STR2 ... ]	# optional
				# Converter type, default is taken from
				# defaults.pcm.rate_converter
	# or
	converter {		# optional
		name STR	# Convertor type
		xxx yyy		# optional convertor-specific configuration
	}
}
\endcode

\subsection pcm_plugins_rate_funcref Function reference

<UL>
  <LI>snd_pcm_rate_open()
  <LI>_snd_pcm_rate_open()
</UL>

*/

/**
 * \brief Creates a new rate PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with rate PCM description
 * \param stream Stream type
 * \param mode Stream mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_rate_open(snd_pcm_t **pcmp, const char *name,
		       snd_config_t *root, snd_config_t *conf, 
		       snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	int err;
	snd_pcm_t *spcm;
	snd_config_t *slave = NULL, *sconf;
	snd_pcm_format_t sformat = SND_PCM_FORMAT_UNKNOWN;
	int srate = -1;
	const snd_config_t *converter = NULL;

	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "slave") == 0) {
			slave = n;
			continue;
		}
		if (strcmp(id, "converter") == 0) {
			converter = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}

	err = snd_pcm_slave_conf(root, slave, &sconf, 2,
				 SND_PCM_HW_PARAM_FORMAT, 0, &sformat,
				 SND_PCM_HW_PARAM_RATE, SCONF_MANDATORY, &srate);
	if (err < 0)
		return err;
	if (sformat != SND_PCM_FORMAT_UNKNOWN &&
	    snd_pcm_format_linear(sformat) != 1) {
	    	snd_config_delete(sconf);
		SNDERR("slave format is not linear");
		return -EINVAL;
	}
	err = snd_pcm_open_slave(&spcm, root, sconf, stream, mode, conf);
	snd_config_delete(sconf);
	if (err < 0)
		return err;
	err = snd_pcm_rate_open(pcmp, name, sformat, (unsigned int) srate,
				converter, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_rate_open, SND_PCM_DLSYM_VERSION);
#endif
