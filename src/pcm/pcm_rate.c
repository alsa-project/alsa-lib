/**
 * \file pcm/pcm_rate.c
 * \ingroup PCM_Plugins
 * \brief PCM Rate Plugin Interface
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2000-2004
 */
/*
 *  PCM - Rate conversion
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *                2004 by Jaroslav Kysela <perex@suse.cz>
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <inttypes.h>
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"
#include "iatomic.h"

#if 0
#define DEBUG_REFINE
#endif

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_rate = "";
#endif

#ifndef DOC_HIDDEN

/* LINEAR_DIV needs to be large enough to handle resampling from 192000 -> 8000 */
#define LINEAR_DIV_SHIFT 19
#define LINEAR_DIV (1<<LINEAR_DIV_SHIFT)

enum rate_type {
	RATE_TYPE_LINEAR,		/* linear interpolation */
	RATE_TYPE_BANDLIMIT,		/* bandlimited interpolation */
	RATE_TYPE_POLYPHASE,		/* polyphase resampling */
};

typedef struct _snd_pcm_rate snd_pcm_rate_t;

typedef void (*rate_f)(const snd_pcm_channel_area_t *dst_areas,
		       snd_pcm_uframes_t dst_offset,
		       snd_pcm_uframes_t dst_frames,
		       const snd_pcm_channel_area_t *src_areas,
		       snd_pcm_uframes_t src_offset,
		       snd_pcm_uframes_t src_frames,
		       unsigned int channels,
		       snd_pcm_rate_t *rate);

struct _snd_pcm_rate {
	snd_pcm_t *slave;
	int close_slave;
	snd_atomic_write_t watom;
	snd_pcm_uframes_t appl_ptr, hw_ptr;
	snd_pcm_uframes_t orig_avail_min;
	snd_pcm_sw_params_t sw_params;
	enum rate_type type;
	unsigned int get_idx;
	unsigned int put_idx;
	unsigned int pitch;
	unsigned int pitch_shift;	/* for expand interpolation */
	rate_f func;
	snd_pcm_format_t sformat;
	unsigned int srate;
	snd_pcm_channel_area_t *pareas;	/* areas for splitted period (rate pcm) */
	snd_pcm_channel_area_t *sareas;	/* areas for splitted period (slave pcm) */
};

static void snd_pcm_rate_expand(const snd_pcm_channel_area_t *dst_areas,
				snd_pcm_uframes_t dst_offset, snd_pcm_uframes_t dst_frames,
				const snd_pcm_channel_area_t *src_areas,
				snd_pcm_uframes_t src_offset, snd_pcm_uframes_t src_frames,
				unsigned int channels,
				snd_pcm_rate_t *rate)
{
#define GET16_LABELS
#define PUT16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
#undef PUT16_LABELS
	void *get = get16_labels[rate->get_idx];
	void *put = put16_labels[rate->put_idx];
	unsigned int get_threshold = rate->pitch;
	unsigned int channel;
	snd_pcm_uframes_t src_frames1;
	snd_pcm_uframes_t dst_frames1;
	int16_t sample = 0;
	
	for (channel = 0; channel < channels; ++channel) {
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
		const char *src;
		char *dst;
		int src_step, dst_step;
		int16_t old_sample = 0;
		int16_t new_sample = 0;
		int old_weight, new_weight;
		unsigned int pos = 0;
		int init;
		src = snd_pcm_channel_area_addr(src_area, src_offset);
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		src_step = snd_pcm_channel_area_step(src_area);
		dst_step = snd_pcm_channel_area_step(dst_area);
		src_frames1 = 0;
		dst_frames1 = 0;
		init = 1;
		while (dst_frames1 < dst_frames) {
			if (pos >= get_threshold) {
				src += src_step;
				src_frames1++;
				if (src_frames1 < src_frames) {
					old_sample = new_sample;
					goto *get;
#define GET16_END after_get
#include "plugin_ops.h"
#undef GET16_END
				after_get:
					new_sample = sample;
					if (init) {
						init = 0;
						continue;
					}
				}
				pos -= get_threshold;
			}
			new_weight = (pos << (16 - rate->pitch_shift)) / (get_threshold >> rate->pitch_shift);
			old_weight = 0x10000 - new_weight;
			sample = (old_sample * old_weight + new_sample * new_weight) >> 16;
			goto *put;
#define PUT16_END after_put
#include "plugin_ops.h"
#undef PUT16_END
		after_put:
			dst += dst_step;
			dst_frames1++;
			pos += LINEAR_DIV;
		} 
	}
}

/* optimized version for S16 format */
static void snd_pcm_rate_expand_s16(const snd_pcm_channel_area_t *dst_areas,
				    snd_pcm_uframes_t dst_offset, snd_pcm_uframes_t dst_frames,
				    const snd_pcm_channel_area_t *src_areas,
				    snd_pcm_uframes_t src_offset, snd_pcm_uframes_t src_frames,
				    unsigned int channels,
				    snd_pcm_rate_t *rate)
{
	unsigned int channel;
	snd_pcm_uframes_t src_frames1;
	snd_pcm_uframes_t dst_frames1;
	unsigned int get_threshold = rate->pitch;
	
	for (channel = 0; channel < channels; ++channel) {
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
		const int16_t *src;
		int16_t *dst;
		int src_step, dst_step;
		int16_t old_sample;
		int16_t new_sample;
		int old_weight, new_weight;
		unsigned int pos;
		src = snd_pcm_channel_area_addr(src_area, src_offset);
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		src_step = snd_pcm_channel_area_step(src_area) >> 1;
		dst_step = snd_pcm_channel_area_step(dst_area) >> 1;
		src_frames1 = 0;
		dst_frames1 = 0;
		old_sample = new_sample = *src;
		pos = get_threshold;
		while (dst_frames1 < dst_frames) {
			if (pos >= get_threshold) {
				pos -= get_threshold;
				src += src_step;
				src_frames1++;
				if (src_frames1 < src_frames) {
					old_sample = new_sample;
					new_sample = *src;
				}
			}
			new_weight = (pos << (16 - rate->pitch_shift)) / (get_threshold >> rate->pitch_shift);
			old_weight = 0x10000 - new_weight;
			*dst = (old_sample * old_weight + new_sample * new_weight) >> 16;
			dst += dst_step;
			dst_frames1++;
			pos += LINEAR_DIV;
		} 
	}
}

static void snd_pcm_rate_shrink(const snd_pcm_channel_area_t *dst_areas,
				snd_pcm_uframes_t dst_offset, snd_pcm_uframes_t dst_frames,
				const snd_pcm_channel_area_t *src_areas,
				snd_pcm_uframes_t src_offset, snd_pcm_uframes_t src_frames,
				unsigned int channels,
				snd_pcm_rate_t *rate)
{
#define GET16_LABELS
#define PUT16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
#undef PUT16_LABELS
	void *get = get16_labels[rate->get_idx];
	void *put = put16_labels[rate->put_idx];
	unsigned int get_increment = rate->pitch;
	unsigned int channel;
	snd_pcm_uframes_t src_frames1;
	snd_pcm_uframes_t dst_frames1;
	int16_t sample = 0;
	unsigned int pos;

	for (channel = 0; channel < channels; ++channel) {
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
		const char *src;
		char *dst;
		int src_step, dst_step;
		int16_t old_sample = 0;
		int16_t new_sample = 0;
		int old_weight, new_weight;
		pos = LINEAR_DIV - get_increment; /* Force first sample to be copied */
		src = snd_pcm_channel_area_addr(src_area, src_offset);
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		src_step = snd_pcm_channel_area_step(src_area);
		dst_step = snd_pcm_channel_area_step(dst_area);
		src_frames1 = 0;
		dst_frames1 = 0;
		while (src_frames1 < src_frames) {
			
			goto *get;
#define GET16_END after_get
#include "plugin_ops.h"
#undef GET16_END
		after_get:
			new_sample = sample;
			src += src_step;
			src_frames1++;
			pos += get_increment;
			if (pos >= LINEAR_DIV) {
				pos -= LINEAR_DIV;
				old_weight = (pos << (32 - LINEAR_DIV_SHIFT)) / (get_increment >> (LINEAR_DIV_SHIFT - 16));
				new_weight = 0x10000 - old_weight;
				sample = (old_sample * old_weight + new_sample * new_weight) >> 16;
				goto *put;
#define PUT16_END after_put
#include "plugin_ops.h"
#undef PUT16_END
			after_put:
				dst += dst_step;
				dst_frames1++;
				if (CHECK_SANITY(dst_frames1 > dst_frames)) {
					SNDERR("dst_frames overflow");
					break;
				}
			}
			old_sample = new_sample;
		}
	}
}

/* optimized version for S16 format */
static void snd_pcm_rate_shrink_s16(const snd_pcm_channel_area_t *dst_areas,
				    snd_pcm_uframes_t dst_offset, snd_pcm_uframes_t dst_frames,
				    const snd_pcm_channel_area_t *src_areas,
				    snd_pcm_uframes_t src_offset, snd_pcm_uframes_t src_frames,
				    unsigned int channels,
				    snd_pcm_rate_t *rate)
{
	unsigned int get_increment = rate->pitch;
	unsigned int channel;
	snd_pcm_uframes_t src_frames1;
	snd_pcm_uframes_t dst_frames1;
	unsigned int pos = 0;

	for (channel = 0; channel < channels; ++channel) {
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
		const int16_t *src;
		int16_t *dst;
		int src_step, dst_step;
		int16_t old_sample = 0;
		int16_t new_sample = 0;
		int old_weight, new_weight;
		pos = LINEAR_DIV - get_increment; /* Force first sample to be copied */
		src = snd_pcm_channel_area_addr(src_area, src_offset);
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		src_step = snd_pcm_channel_area_step(src_area) >> 1;
		dst_step = snd_pcm_channel_area_step(dst_area) >> 1 ;
		src_frames1 = 0;
		dst_frames1 = 0;
		while (src_frames1 < src_frames) {
			
			new_sample = *src;
			src += src_step;
			src_frames1++;
			pos += get_increment;
			if (pos >= LINEAR_DIV) {
				pos -= LINEAR_DIV;
				old_weight = (pos << (32 - LINEAR_DIV_SHIFT)) / (get_increment >> (LINEAR_DIV_SHIFT - 16));
				new_weight = 0x10000 - old_weight;
				*dst = (old_sample * old_weight + new_sample * new_weight) >> 16;
				dst += dst_step;
				dst_frames1++;
				if (CHECK_SANITY(dst_frames1 > dst_frames)) {
					SNDERR("dst_frames overflow");
					break;
				}
			}
			old_sample = new_sample;
		}
	}
}

#endif /* DOC_HIDDEN */

static snd_pcm_sframes_t snd_pcm_rate_client_frames(snd_pcm_t *pcm, snd_pcm_sframes_t frames)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	if (frames == 0)
		return 0;
	/* Round toward zero */
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return muldiv_near(frames, LINEAR_DIV, rate->pitch);
	else
		return muldiv_near(frames, rate->pitch, LINEAR_DIV);
}

static snd_pcm_sframes_t snd_pcm_rate_slave_frames(snd_pcm_t *pcm, snd_pcm_sframes_t frames)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	/* Round toward zero */
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return muldiv_near(frames, rate->pitch, LINEAR_DIV);
	else
		return muldiv_near(frames, LINEAR_DIV, rate->pitch);
}

static int snd_pcm_rate_hw_refine_cprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params)
{
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
	err = _snd_pcm_hw_param_set_min(params,
					SND_PCM_HW_PARAM_RATE, SND_PCM_PLUGIN_RATE_MIN, 0);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_set_max(params,
					SND_PCM_HW_PARAM_RATE, SND_PCM_PLUGIN_RATE_MAX, 0);
	if (err < 0)
		return err;
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
		    	if ((buffer_size->min / period_size->min) * period_size->min == buffer_size->min) {
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

static int snd_pcm_rate_hw_refine_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_hw_refine(rate->slave, params);
}

static int snd_pcm_rate_hw_params_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return _snd_pcm_hw_params(rate->slave, params);
}

static int snd_pcm_rate_hw_refine(snd_pcm_t *pcm, 
				  snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_rate_hw_refine_cprepare,
				       snd_pcm_rate_hw_refine_cchange,
				       snd_pcm_rate_hw_refine_sprepare,
				       snd_pcm_rate_hw_refine_schange,
				       snd_pcm_rate_hw_refine_slave);
}

static int snd_pcm_rate_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->slave;
	snd_pcm_format_t src_format, dst_format, pformat, sformat;
	unsigned int src_rate, dst_rate, channels, pwidth, swidth, chn;
	snd_pcm_uframes_t period_size, buffer_size;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_rate_hw_refine_cchange,
					  snd_pcm_rate_hw_refine_sprepare,
					  snd_pcm_rate_hw_refine_schange,
					  snd_pcm_rate_hw_params_slave);
	if (err < 0)
		return err;

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		err = INTERNAL(snd_pcm_hw_params_get_format)(params, &src_format);
		if (err < 0)
			return err;
		pformat = src_format;
		dst_format = slave->format;
		sformat = dst_format;
		dst_rate = slave->rate;
		err = INTERNAL(snd_pcm_hw_params_get_rate)(params, &src_rate, 0);
	} else {
		sformat = src_format = slave->format;
		err = INTERNAL(snd_pcm_hw_params_get_format)(params, &dst_format);
		if (err < 0)
			return err;
		pformat = dst_format;
		src_rate = slave->rate;
		err = INTERNAL(snd_pcm_hw_params_get_rate)(params, &dst_rate, 0);
	}
	if (err < 0)
		return err;
	err = INTERNAL(snd_pcm_hw_params_get_period_size)(params, &period_size, 0);
	if (err < 0)
		return err;
	err = INTERNAL(snd_pcm_hw_params_get_buffer_size)(params, &buffer_size);
	if (err < 0)
		return err;
	err = INTERNAL(snd_pcm_hw_params_get_channels)(params, &channels);
	if (err < 0)
		return err;
	rate->get_idx = snd_pcm_linear_get_index(src_format, SND_PCM_FORMAT_S16);
	rate->put_idx = snd_pcm_linear_put_index(SND_PCM_FORMAT_S16, dst_format);
	if (src_rate < dst_rate) {
		if (src_format == dst_format && src_format == SND_PCM_FORMAT_S16)
			rate->func = snd_pcm_rate_expand_s16;
		else
			rate->func = snd_pcm_rate_expand;
		/* pitch is get_threshold */
	} else {
		if (src_format == dst_format && src_format == SND_PCM_FORMAT_S16)
			rate->func = snd_pcm_rate_shrink_s16;
		else
			rate->func = snd_pcm_rate_shrink;
		/* pitch is get_increment */
	}
	rate->pitch = (((u_int64_t)dst_rate * LINEAR_DIV) + (src_rate / 2)) / src_rate;
	if (CHECK_SANITY(rate->pareas)) {
		SNDMSG("rate plugin already in use");
		return -EBUSY;
	}
	if ((buffer_size / period_size) * period_size == buffer_size &&
	    (slave->buffer_size / slave->period_size) * slave->period_size == slave->buffer_size)
		return 0;
	rate->pareas = malloc(2 * channels * sizeof(*rate->pareas));
	if (rate->pareas == NULL)
		return -ENOMEM;
	pwidth = snd_pcm_format_physical_width(pformat);
	swidth = snd_pcm_format_physical_width(sformat);
	rate->pareas[0].addr = malloc(((pwidth * channels * period_size) / 8) +
				      ((swidth * channels * slave->period_size) / 8));
	if (rate->pareas[0].addr == NULL) {
		free(rate->pareas);
		return -ENOMEM;
	}
	rate->sareas = rate->pareas + channels;
	rate->sareas[0].addr = (char *)rate->pareas[0].addr + ((pwidth * channels * period_size) / 8);
	for (chn = 0; chn < channels; chn++) {
		rate->pareas[chn].addr = rate->pareas[0].addr + (pwidth * chn * period_size) / 8;
		rate->pareas[chn].first = 0;
		rate->pareas[chn].step = pwidth;
		rate->sareas[chn].addr = rate->sareas[0].addr + (swidth * chn * slave->period_size) / 8;
		rate->sareas[chn].first = 0;
		rate->sareas[chn].step = swidth;
	}
	return 0;
}

static int snd_pcm_rate_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	if (rate->pareas) {
		free(rate->pareas[0].addr);
		free(rate->pareas);
		rate->pareas = NULL;
		rate->sareas = NULL;
	}
	return snd_pcm_hw_free(rate->slave);
}

static void recalc(snd_pcm_t *pcm, snd_pcm_uframes_t *val)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->slave;
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
	snd_pcm_t *slave = rate->slave;
	snd_pcm_sw_params_t *sparams;
	snd_pcm_uframes_t boundary1, boundary2;

	rate->sw_params = *params;
	sparams = &rate->sw_params;
	if ((rate->pitch >= LINEAR_DIV ? 1 : 0) ^ (pcm->stream == SND_PCM_STREAM_CAPTURE ? 1 : 0)) {
		boundary1 = pcm->buffer_size;
		boundary2 = slave->buffer_size;
		while (boundary2 * 2 <= LONG_MAX - slave->buffer_size) {
			boundary1 *= 2;
			boundary2 *= 2;
		}
	} else {
		boundary1 = pcm->buffer_size;
		boundary2 = slave->buffer_size;
		while (boundary1 * 2 <= LONG_MAX - pcm->buffer_size) {
			boundary1 *= 2;
			boundary2 *= 2;
		}
	}
	params->boundary = boundary1;
	sparams->boundary = boundary2;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		rate->pitch = (((u_int64_t)slave->period_size * LINEAR_DIV) + (pcm->period_size/2) ) / pcm->period_size;
		do {
			snd_pcm_uframes_t cframes,cframes_test;
			
			cframes = snd_pcm_rate_client_frames(pcm, slave->period_size );
			if (cframes == pcm->period_size )
				break;
			if (cframes > pcm->period_size ) {
				rate->pitch++;
				cframes_test = snd_pcm_rate_client_frames(pcm, slave->period_size );
				if (cframes_test < pcm->period_size ) {
					SNDERR("Unable to satisfy pitch condition (%i/%i - %li/%li)\n", slave->rate, pcm->rate, slave->period_size, pcm->period_size);
					return -EIO;
				}
			} else {
				rate->pitch--;
				cframes_test = snd_pcm_rate_client_frames(pcm, slave->period_size );
				if (cframes_test > pcm->period_size) {
					SNDERR("Unable to satisfy pitch condition (%i/%i - %li/%li)\n", slave->rate, pcm->rate, slave->period_size, pcm->period_size);
					return -EIO;
				}
			}
		} while (1);
		if ((snd_pcm_uframes_t)snd_pcm_rate_client_frames(pcm, slave->period_size ) != pcm->period_size) {
			SNDERR("invalid slave period_size %ld for pcm period_size %ld",
			       slave->period_size, pcm->period_size);
			return -EIO;
		}
	} else {
		rate->pitch = (((u_int64_t)pcm->period_size * LINEAR_DIV) + (slave->period_size/2) ) / slave->period_size;
		do {
			snd_pcm_uframes_t cframes;
			
			cframes = snd_pcm_rate_slave_frames(pcm, pcm->period_size );
			if (cframes == slave->period_size )
				break;
			if (cframes > slave->period_size ) {
				rate->pitch++;
				if ((snd_pcm_uframes_t)snd_pcm_rate_slave_frames(pcm, pcm->period_size ) < slave->period_size ) {
					SNDERR("Unable to satisfy pitch condition (%i/%i - %li/%li)\n", slave->rate, pcm->rate, slave->period_size, pcm->period_size);
					return -EIO;
				}
			} else {
				rate->pitch--;
				if ((snd_pcm_uframes_t)snd_pcm_rate_slave_frames(pcm, pcm->period_size) > slave->period_size ) {
					SNDERR("Unable to satisfy pitch condition (%i/%i - %li/%li)\n", slave->rate, pcm->rate, slave->period_size , pcm->period_size );
					return -EIO;
				}
			}
		} while (1);
		if ((snd_pcm_uframes_t)snd_pcm_rate_slave_frames(pcm, pcm->period_size ) != slave->period_size) {
			SNDERR("invalid pcm period_size %ld for slave period_size",
			       pcm->period_size, slave->period_size);
			return -EIO;
		}
	}
	if (rate->pitch >= LINEAR_DIV) {
		/* shift for expand linear interpolation */
		rate->pitch_shift = 0;
		while ((rate->pitch >> rate->pitch_shift) >= (1 << 16))
			rate->pitch_shift++;
	}
	recalc(pcm, &sparams->avail_min);
	rate->orig_avail_min = sparams->avail_min;
	recalc(pcm, &sparams->xfer_align);
	recalc(pcm, &sparams->start_threshold);
	if (sparams->avail_min < 1) sparams->avail_min = 1;
	if (sparams->xfer_align < 1) sparams->xfer_align = 1;
	if (sparams->start_threshold <= slave->buffer_size) {
		if (sparams->start_threshold > (slave->buffer_size / sparams->avail_min) * sparams->avail_min)
			sparams->start_threshold = (slave->buffer_size / sparams->avail_min) * sparams->avail_min;
		if (sparams->start_threshold > (slave->buffer_size / sparams->xfer_align) * sparams->xfer_align)
			sparams->start_threshold = (slave->buffer_size / sparams->xfer_align) * sparams->xfer_align;
	}
	if (sparams->stop_threshold >= sparams->boundary) {
		sparams->stop_threshold = sparams->boundary;
	} else {
		recalc(pcm, &sparams->stop_threshold);
	}
	recalc(pcm, &sparams->silence_threshold);
	recalc(pcm, &sparams->silence_size);
	return snd_pcm_sw_params(slave, sparams);
}

static int snd_pcm_rate_init(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	switch (rate->type) {
	case RATE_TYPE_LINEAR:
		break;
	default:
		assert(0);
	}
	return 0;
}

static inline int
snd_pcm_rate_write_areas1(snd_pcm_t *pcm,
			 const snd_pcm_channel_area_t *areas,
			 snd_pcm_uframes_t offset,
			 const snd_pcm_channel_area_t *slave_areas,
			 snd_pcm_uframes_t slave_offset)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	rate->func(slave_areas, slave_offset, rate->slave->period_size,
		   areas, offset, pcm->period_size,
		   pcm->channels, rate);
	return 0;
}

static inline int
snd_pcm_rate_read_areas1(snd_pcm_t *pcm,
			 const snd_pcm_channel_area_t *areas,
			 snd_pcm_uframes_t offset,
			 const snd_pcm_channel_area_t *slave_areas,
			 snd_pcm_uframes_t slave_offset)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	rate->func(areas, offset, pcm->period_size,
		   slave_areas, slave_offset, rate->slave->period_size,
		   pcm->channels, rate);
	return 0;
}

static inline snd_pcm_sframes_t snd_pcm_rate_move_applptr(snd_pcm_t *pcm, snd_pcm_sframes_t frames)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_uframes_t orig_appl_ptr, appl_ptr = rate->appl_ptr, slave_appl_ptr;
	snd_pcm_sframes_t diff, ndiff;
	snd_pcm_t *slave = rate->slave;

	orig_appl_ptr = rate->appl_ptr;
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	else
		snd_pcm_mmap_appl_backward(pcm, -frames);
	slave_appl_ptr =
		(appl_ptr / pcm->period_size) * rate->slave->period_size;
	diff = slave_appl_ptr - *slave->appl.ptr;
	if (diff < -(snd_pcm_sframes_t)(slave->boundary / 2)) {
		diff = (slave->boundary - *slave->appl.ptr) + slave_appl_ptr;
	} else if (diff > (snd_pcm_sframes_t)(slave->boundary / 2)) {
		diff = -((slave->boundary - slave_appl_ptr) + *slave->appl.ptr);
	}
	if (diff == 0)
		return frames;
	if (diff > 0) {
		ndiff = snd_pcm_forward(rate->slave, diff);
	} else {
		ndiff = snd_pcm_rewind(rate->slave, diff);
	}
	if (ndiff < 0)
		return diff;
	slave_appl_ptr = *slave->appl.ptr;
	rate->appl_ptr =
		(slave_appl_ptr / rate->slave->period_size) * pcm->period_size +
		snd_pcm_rate_client_frames(pcm, slave_appl_ptr % rate->slave->period_size) +
		orig_appl_ptr % pcm->period_size;
	diff = orig_appl_ptr - rate->appl_ptr;
	if (diff < -(snd_pcm_sframes_t)(slave->boundary / 2)) {
		diff = (slave->boundary - rate->appl_ptr) + orig_appl_ptr;
	} else if (diff > (snd_pcm_sframes_t)(slave->boundary / 2)) {
		diff = -((slave->boundary - orig_appl_ptr) + rate->appl_ptr);
	}
	if (frames < 0)
		return -diff;
	return diff;
}

static inline void snd_pcm_rate_sync_hwptr(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_uframes_t slave_hw_ptr = *rate->slave->hw.ptr;

	if (pcm->stream != SND_PCM_STREAM_PLAYBACK)
		return;
	rate->hw_ptr =
		(slave_hw_ptr / rate->slave->period_size) * pcm->period_size +
		snd_pcm_rate_client_frames(pcm, slave_hw_ptr % rate->slave->period_size);
}

static int snd_pcm_rate_close(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err = 0;
	if (rate->close_slave)
		err = snd_pcm_close(rate->slave);
	free(rate);
	return 0;
}

static int snd_pcm_rate_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_nonblock(rate->slave, nonblock);
}

static int snd_pcm_rate_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_async(rate->slave, sig, pid);
}

static int snd_pcm_rate_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_poll_descriptors_revents(rate->slave, pfds, nfds, revents);
}

static int snd_pcm_rate_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_info(rate->slave, info);
}

static int snd_pcm_rate_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	return snd_pcm_channel_info_shm(pcm, info, -1);
}

static snd_pcm_state_t snd_pcm_rate_state(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_state(rate->slave);
}

static int snd_pcm_rate_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err = snd_pcm_hwsync(rate->slave);
	if (err < 0)
		return err;
	snd_atomic_write_begin(&rate->watom);
	snd_pcm_rate_sync_hwptr(pcm);
	snd_atomic_write_end(&rate->watom);
	return 0;
}

static int snd_pcm_rate_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_rate_hwsync(pcm);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		*delayp = snd_pcm_mmap_playback_hw_avail(pcm);
	else
		*delayp = snd_pcm_mmap_capture_hw_avail(pcm);
	return 0;
}

static int snd_pcm_rate_prepare(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err;

	snd_atomic_write_begin(&rate->watom);
	err = snd_pcm_prepare(rate->slave);
	if (err < 0) {
		snd_atomic_write_end(&rate->watom);
		return err;
	}
	*pcm->hw.ptr = 0;
	*pcm->appl.ptr = 0;
	snd_atomic_write_end(&rate->watom);
	err = snd_pcm_rate_init(pcm);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_rate_reset(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	int err;
	snd_atomic_write_begin(&rate->watom);
	err = snd_pcm_reset(rate->slave);
	if (err < 0) {
		snd_atomic_write_end(&rate->watom);
		return err;
	}
	*pcm->hw.ptr = 0;
	*pcm->appl.ptr = 0;
	snd_atomic_write_end(&rate->watom);
	err = snd_pcm_rate_init(pcm);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_rate_start(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_start(rate->slave);
}

static int snd_pcm_rate_drop(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_drop(rate->slave);
}

static int snd_pcm_rate_drain(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_drain(rate->slave);
}

static int snd_pcm_rate_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_pause(rate->slave, enable);
}

static snd_pcm_sframes_t snd_pcm_rate_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_sframes_t n = snd_pcm_mmap_hw_avail(pcm);

	if ((snd_pcm_uframes_t)n > frames)
		frames = n;
	if (frames == 0)
		return 0;
	
	snd_atomic_write_begin(&rate->watom);
	n = snd_pcm_rate_move_applptr(pcm, -frames);
	snd_atomic_write_end(&rate->watom);
	return n;
}

static snd_pcm_sframes_t snd_pcm_rate_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_sframes_t n = snd_pcm_mmap_avail(pcm);

	if ((snd_pcm_uframes_t)n > frames)
		frames = n;
	if (frames == 0)
		return 0;
	
	snd_atomic_write_begin(&rate->watom);
	n = snd_pcm_rate_move_applptr(pcm, frames);
	snd_atomic_write_end(&rate->watom);
	return n;
}

static int snd_pcm_rate_resume(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return snd_pcm_resume(rate->slave);
}

static int snd_pcm_rate_poll_ask(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_uframes_t avail_min;
	int err;

	if (rate->slave->fast_ops->poll_ask) {
		err = rate->slave->fast_ops->poll_ask(rate->slave->fast_op_arg);
		if (err < 0)
			return err;
	}
	avail_min = rate->appl_ptr % pcm->period_size;
	if (avail_min > 0) {
		recalc(pcm, &avail_min);
		if (avail_min < rate->slave->buffer_size &&
		    avail_min != rate->slave->period_size)
			avail_min++;	/* 1st small little rounding correction */
		if (avail_min < rate->slave->buffer_size &&
		    avail_min != rate->slave->period_size)
			avail_min++;	/* 2nd small little rounding correction */
		avail_min += rate->orig_avail_min;
	} else {
		avail_min = rate->orig_avail_min;
	}
	if (rate->sw_params.avail_min == avail_min)
		return 0;
	rate->sw_params.avail_min = avail_min;
	return snd_pcm_sw_params(rate->slave, &rate->sw_params);
}

static int snd_pcm_rate_commit_next_period(snd_pcm_t *pcm, snd_pcm_uframes_t appl_offset)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_uframes_t cont = pcm->buffer_size - appl_offset;
	const snd_pcm_channel_area_t *areas;
	const snd_pcm_channel_area_t *slave_areas;
	snd_pcm_uframes_t slave_offset, xfer;
	snd_pcm_uframes_t slave_frames = ULONG_MAX;
	snd_pcm_sframes_t result;

	areas = snd_pcm_mmap_areas(pcm);
	if (cont >= pcm->period_size) {
		result = snd_pcm_mmap_begin(rate->slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
		if (slave_frames < rate->slave->period_size) {
			snd_pcm_rate_write_areas1(pcm, areas, appl_offset, rate->sareas, 0);
			goto __partial;
		}
		snd_pcm_rate_write_areas1(pcm, areas, appl_offset,
					  slave_areas, slave_offset);
		result = snd_pcm_mmap_commit(rate->slave, slave_offset, rate->slave->period_size);
		if (result < (snd_pcm_sframes_t)rate->slave->period_size) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->slave, result);
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
		result = snd_pcm_mmap_begin(rate->slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
	      __partial:
		xfer = 0;
		cont = rate->slave->buffer_size - slave_offset;
		if (cont > rate->slave->period_size)
			cont = rate->slave->period_size;
		snd_pcm_areas_copy(slave_areas, slave_offset,
				   rate->sareas, 0,
				   pcm->channels, cont,
				   rate->slave->format);
		result = snd_pcm_mmap_commit(rate->slave, slave_offset, cont);
		if (result < (snd_pcm_sframes_t)cont) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->slave, result);
			if (result < 0)
				return result;
			return 0;
		}
		xfer = cont;

		if (xfer == rate->slave->period_size)
			return 1;
		
		/* commit second fragment */
		cont = rate->slave->period_size - cont;
		slave_frames = cont;
		result = snd_pcm_mmap_begin(rate->slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
		if (slave_offset) {
			SNDERR("non-zero slave_offset %ld", slave_offset);
			return -EIO;
		}
		snd_pcm_areas_copy(slave_areas, slave_offset,
				   rate->sareas, xfer,
				   pcm->channels, cont,
				   rate->slave->format);
		result = snd_pcm_mmap_commit(rate->slave, slave_offset, cont);
		if (result < (snd_pcm_sframes_t)cont) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->slave, result + xfer);
			if (result < 0)
				return result;
			return 0;
		}
	}
	return 1;
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
		result = snd_pcm_mmap_begin(rate->slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
		if (slave_frames < rate->slave->period_size)
			goto __partial;
		snd_pcm_rate_read_areas1(pcm, areas, hw_offset,
					 slave_areas, slave_offset);
		result = snd_pcm_mmap_commit(rate->slave, slave_offset, rate->slave->period_size);
		if (result < (snd_pcm_sframes_t)rate->slave->period_size) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->slave, result);
			if (result < 0)
				return result;
			return 0;
		}
	} else {
		/* ok, grab first fragment */
		result = snd_pcm_mmap_begin(rate->slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
	      __partial:
		xfer = 0;
		cont = rate->slave->buffer_size - slave_offset;
		if (cont > rate->slave->period_size)
			cont = rate->slave->period_size;
		snd_pcm_areas_copy(rate->sareas, 0,
				   slave_areas, slave_offset,
				   pcm->channels, cont,
				   rate->slave->format);
		result = snd_pcm_mmap_commit(rate->slave, slave_offset, cont);
		if (result < (snd_pcm_sframes_t)cont) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->slave, result);
			if (result < 0)
				return result;
			return 0;
		}
		xfer = cont;

		if (xfer == rate->slave->period_size)
			goto __transfer;

		/* grab second fragment */
		cont = rate->slave->period_size - cont;
		slave_frames = cont;
		result = snd_pcm_mmap_begin(rate->slave, &slave_areas, &slave_offset, &slave_frames);
		if (result < 0)
			return result;
		if (slave_offset) {
			SNDERR("non-zero slave_offset %ld", slave_offset);
			return -EIO;
		}
		snd_pcm_areas_copy(rate->sareas, xfer,
		                   slave_areas, slave_offset,
				   pcm->channels, cont,
				   rate->slave->format);
		result = snd_pcm_mmap_commit(rate->slave, slave_offset, cont);
		if (result < (snd_pcm_sframes_t)cont) {
			if (result < 0)
				return result;
			result = snd_pcm_rewind(rate->slave, result + xfer);
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

static snd_pcm_sframes_t snd_pcm_rate_mmap_commit(snd_pcm_t *pcm,
						  snd_pcm_uframes_t offset ATTRIBUTE_UNUSED,
						  snd_pcm_uframes_t size)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->slave;
	snd_pcm_uframes_t appl_offset, xfer;
	snd_pcm_sframes_t slave_size;
	int err;

	if (size == 0)
		return 0;
	if (pcm->stream == SND_PCM_STREAM_CAPTURE) {
		snd_atomic_write_begin(&rate->watom);
		snd_pcm_mmap_appl_forward(pcm, size);
		snd_atomic_write_end(&rate->watom);
		return size;
	}
	slave_size = snd_pcm_avail_update(slave);
	if (slave_size < 0)
		return slave_size;
	xfer = rate->appl_ptr % pcm->period_size;
	appl_offset = (rate->appl_ptr - xfer) % pcm->buffer_size;
	xfer = pcm->period_size - xfer;
	if (xfer >= size) {
		if (xfer == size && (snd_pcm_uframes_t)slave_size >= rate->slave->period_size) {
			err = snd_pcm_rate_commit_next_period(pcm, appl_offset);
			if (err < 0)
				return err;
			if (err == 0)
				return 0;
		}
		snd_atomic_write_begin(&rate->watom);
		snd_pcm_mmap_appl_forward(pcm, size);
		snd_atomic_write_end(&rate->watom);
		return size;
	} else {
		if ((snd_pcm_uframes_t)slave_size >= rate->slave->period_size) {
			err = snd_pcm_rate_commit_next_period(pcm, appl_offset);
			if (err < 0)
				return err;
			if (err == 0)
				return 0;
		}
		snd_atomic_write_begin(&rate->watom);
		snd_pcm_mmap_appl_forward(pcm, xfer);
		snd_atomic_write_end(&rate->watom);
		appl_offset += pcm->period_size;
		appl_offset %= pcm->buffer_size;
		size -= xfer;
		slave_size -= rate->slave->period_size;
	}
	while ((snd_pcm_uframes_t)size >= pcm->period_size &&
	       (snd_pcm_uframes_t)slave_size >= rate->slave->period_size) {
		err = snd_pcm_rate_commit_next_period(pcm, appl_offset);
		if (err == 0)
			return xfer;
		if (err < 0)
			return xfer > 0 ? (snd_pcm_sframes_t)xfer : err;
		xfer += pcm->period_size;
		size -= pcm->period_size;
		slave_size -= rate->slave->period_size;
		appl_offset += pcm->period_size;
		appl_offset %= pcm->buffer_size;
		snd_atomic_write_begin(&rate->watom);
		snd_pcm_mmap_appl_forward(pcm, pcm->period_size);
		snd_atomic_write_end(&rate->watom);
	}
	size %= pcm->period_size;
	if (size > 0) {
		snd_atomic_write_begin(&rate->watom);
		snd_pcm_mmap_appl_forward(pcm, size);
		snd_atomic_write_end(&rate->watom);
		xfer += size;
	}
	return xfer;
}

static snd_pcm_sframes_t snd_pcm_rate_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->slave;
	snd_pcm_uframes_t slave_size;

	slave_size = snd_pcm_avail_update(slave);
	if (pcm->stream == SND_PCM_STREAM_CAPTURE)
		goto _capture;
	snd_atomic_write_begin(&rate->watom);
	snd_pcm_rate_sync_hwptr(pcm);
	snd_atomic_write_end(&rate->watom);
	return snd_pcm_mmap_avail(pcm);
 _capture: {
	snd_pcm_uframes_t xfer, hw_offset, size;
	
	xfer = snd_pcm_mmap_capture_avail(pcm);
	size = pcm->buffer_size - xfer;
	hw_offset = snd_pcm_mmap_hw_offset(pcm);
	while (size >= pcm->period_size &&
	       slave_size >= rate->slave->period_size) {
		int err = snd_pcm_rate_grab_next_period(pcm, hw_offset);
		if (err < 0)
			return err;
		if (err == 0)
			return (snd_pcm_sframes_t)xfer;
		xfer += pcm->period_size;
		size -= pcm->period_size;
		slave_size -= rate->slave->period_size;
		hw_offset += pcm->period_size;
		hw_offset %= pcm->buffer_size;
		snd_pcm_mmap_hw_forward(pcm, pcm->period_size);
	}
	return (snd_pcm_sframes_t)xfer;
 }
}

static int snd_pcm_rate_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_rate_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_rate_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_sframes_t err;
	snd_atomic_read_t ratom;
	snd_atomic_read_init(&ratom, &rate->watom);
 _again:
	snd_atomic_read_begin(&ratom);
	err = snd_pcm_status(rate->slave, status);
	if (err < 0) {
		snd_atomic_read_ok(&ratom);
		return err;
	}
	snd_pcm_rate_sync_hwptr(pcm);
	status->appl_ptr = *pcm->appl.ptr;
	status->hw_ptr = *pcm->hw.ptr;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		status->delay = snd_pcm_mmap_playback_hw_avail(pcm);
		status->avail = snd_pcm_mmap_playback_avail(pcm);
	} else {
		status->delay = snd_pcm_mmap_capture_hw_avail(pcm);
		status->avail = snd_pcm_mmap_capture_avail(pcm);
	}
	if (!snd_atomic_read_ok(&ratom)) {
		snd_atomic_read_wait(&ratom);
		goto _again;
	}
	status->avail_max = snd_pcm_rate_client_frames(pcm, (snd_pcm_sframes_t) status->avail_max);
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
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(rate->slave, out);
}

static snd_pcm_fast_ops_t snd_pcm_rate_fast_ops = {
	.status = snd_pcm_rate_status,
	.state = snd_pcm_rate_state,
	.hwsync = snd_pcm_rate_hwsync,
	.delay = snd_pcm_rate_delay,
	.prepare = snd_pcm_rate_prepare,
	.reset = snd_pcm_rate_reset,
	.start = snd_pcm_rate_start,
	.drop = snd_pcm_rate_drop,
	.drain = snd_pcm_rate_drain,
	.pause = snd_pcm_rate_pause,
	.rewind = snd_pcm_rate_rewind,
	.forward = snd_pcm_rate_forward,
	.resume = snd_pcm_rate_resume,
	.poll_ask = snd_pcm_rate_poll_ask,
	.writei = snd_pcm_mmap_writei,
	.writen = snd_pcm_mmap_writen,
	.readi = snd_pcm_mmap_readi,
	.readn = snd_pcm_mmap_readn,
	.avail_update = snd_pcm_rate_avail_update,
	.mmap_commit = snd_pcm_rate_mmap_commit,
};

static snd_pcm_ops_t snd_pcm_rate_ops = {
	.close = snd_pcm_rate_close,
	.info = snd_pcm_rate_info,
	.hw_refine = snd_pcm_rate_hw_refine,
	.hw_params = snd_pcm_rate_hw_params,
	.hw_free = snd_pcm_rate_hw_free,
	.sw_params = snd_pcm_rate_sw_params,
	.channel_info = snd_pcm_rate_channel_info,
	.dump = snd_pcm_rate_dump,
	.nonblock = snd_pcm_rate_nonblock,
	.async = snd_pcm_rate_async,
	.poll_revents = snd_pcm_rate_poll_revents,
	.mmap = snd_pcm_rate_mmap,
	.munmap = snd_pcm_rate_munmap,
};


/**
 * \brief Creates a new rate PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param sformat Slave format
 * \param srate Slave rate
 * \param slave Slave PCM handle
 * \param close_slave When set, the slave PCM handle is closed with copy PCM
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_rate_open(snd_pcm_t **pcmp, const char *name, snd_pcm_format_t sformat, unsigned int srate, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_rate_t *rate;
	int err;
	assert(pcmp && slave);
	if (sformat != SND_PCM_FORMAT_UNKNOWN &&
	    snd_pcm_format_linear(sformat) != 1)
		return -EINVAL;
	rate = calloc(1, sizeof(snd_pcm_rate_t));
	if (!rate) {
		return -ENOMEM;
	}
	rate->slave = slave;
	rate->close_slave = close_slave;
	rate->type = RATE_TYPE_LINEAR;
	rate->srate = srate;
	rate->sformat = sformat;
	snd_atomic_write_init(&rate->watom);

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_RATE, name, slave->stream, slave->mode);
	if (err < 0) {
		free(rate);
		return err;
	}
	pcm->ops = &snd_pcm_rate_ops;
	pcm->fast_ops = &snd_pcm_rate_fast_ops;
	pcm->private_data = rate;
	pcm->poll_fd = slave->poll_fd;
	pcm->poll_events = slave->poll_events;
	pcm->mmap_rw = 1;
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
	err = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
	snd_config_delete(sconf);
	if (err < 0)
		return err;
	err = snd_pcm_rate_open(pcmp, name, 
				sformat, (unsigned int) srate, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_rate_open, SND_PCM_DLSYM_VERSION);
#endif
