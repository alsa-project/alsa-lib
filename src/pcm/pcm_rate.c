/**
 * \file pcm/pcm_rate.c
 * \ingroup PCM_Plugins
 * \brief PCM Rate Plugin Interface
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2000-2001
 */
/*
 *  PCM - Rate conversion
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
  
#include <limits.h>
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_rate = "";
#endif

#ifndef DOC_HIDDEN

#define DIV (1<<16)

enum rate_type {
	RATE_TYPE_LINEAR,		/* linear interpolation */
	RATE_TYPE_BANDLIMIT,		/* bandlimited interpolation */
	RATE_TYPE_POLYPHASE,		/* polyphase resampling */
};

typedef struct {
	union {
		struct {
			int init;
			int16_t old_sample, new_sample;
			int sum;
			unsigned int pos;
		} linear;
	} u;
} snd_pcm_rate_state_t;
 
typedef snd_pcm_uframes_t (*rate_f)(const snd_pcm_channel_area_t *dst_areas,
				    snd_pcm_uframes_t dst_offset,
				    snd_pcm_uframes_t *dst_framesp,
				    const snd_pcm_channel_area_t *src_areas,
				    snd_pcm_uframes_t src_offset,
				    snd_pcm_uframes_t src_frames,
				    unsigned int channels,
				    unsigned int getidx, unsigned int putidx,
				    unsigned int arg,
				    snd_pcm_rate_state_t *states);

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	enum rate_type type;
	unsigned int get_idx;
	unsigned int put_idx;
	unsigned int pitch;
	rate_f func;
	snd_pcm_format_t sformat;
	unsigned int srate;
	snd_pcm_rate_state_t *states;
} snd_pcm_rate_t;

static int16_t initial_sample(const char *src, unsigned int getidx)
{
#define GET16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
	void *get = get16_labels[getidx];
	int sample = 0;

	goto *get;
#define GET16_END after_get
#include "plugin_ops.h"
#undef GET16_END
      after_get:
	return sample;
}

static snd_pcm_uframes_t snd_pcm_rate_expand(const snd_pcm_channel_area_t *dst_areas,
					     snd_pcm_uframes_t dst_offset, snd_pcm_uframes_t *dst_framesp,
					     const snd_pcm_channel_area_t *src_areas,
					     snd_pcm_uframes_t src_offset, snd_pcm_uframes_t src_frames,
					     unsigned int channels,
					     unsigned int getidx, unsigned int putidx,
					     unsigned int get_threshold,
					     snd_pcm_rate_state_t *states)
{
#define GET16_LABELS
#define PUT16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
#undef PUT16_LABELS
	void *get = get16_labels[getidx];
	void *put = put16_labels[putidx];
	unsigned int channel;
	snd_pcm_uframes_t src_frames1 = 0;
	snd_pcm_uframes_t dst_frames1 = 0;
	snd_pcm_uframes_t dst_frames = *dst_framesp;
	int16_t sample = 0;
	
	if (src_frames == 0 ||
	    dst_frames == 0)
		return 0;
	for (channel = 0; channel < channels; ++channel) {
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
		const char *src;
		char *dst;
		int src_step, dst_step;
		int16_t old_sample = states->u.linear.old_sample;
		int16_t new_sample = states->u.linear.new_sample;
		unsigned int pos = states->u.linear.pos;
		src = snd_pcm_channel_area_addr(src_area, src_offset);
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		src_step = snd_pcm_channel_area_step(src_area);
		dst_step = snd_pcm_channel_area_step(dst_area);
		src_frames1 = 0;
		dst_frames1 = 0;
		if (! states->u.linear.init) {
			sample = initial_sample(src, getidx);
			old_sample = new_sample = sample;
			src += src_step;
			src_frames1++;
			states->u.linear.init = 2; /* get a new sample */
		}
		while (dst_frames1 < dst_frames) {
			if (states->u.linear.init == 2) {
				old_sample = new_sample;
				goto *get;
#define GET16_END after_get
#include "plugin_ops.h"
#undef GET16_END
			after_get:
				new_sample = sample;
				states->u.linear.init = 1;
			}
			sample = (((int64_t)old_sample * (int64_t)(get_threshold - pos)) + ((int64_t)new_sample * pos)) / get_threshold;
			goto *put;
#define PUT16_END after_put
#include "plugin_ops.h"
#undef PUT16_END
		after_put:
			dst += dst_step;
			dst_frames1++;
			pos += DIV;
			if (pos >= get_threshold) {
				pos -= get_threshold;
				src += src_step;
				src_frames1++;
				states->u.linear.init = 2; /* get a new sample */
				if (src_frames1 >= src_frames)
					break;
			}
		} 
		states->u.linear.old_sample = old_sample;
		states->u.linear.new_sample = new_sample;
		states->u.linear.pos = pos;
		states++;
	}
	*dst_framesp = dst_frames1;
	return src_frames1;
}

static snd_pcm_uframes_t snd_pcm_rate_shrink(const snd_pcm_channel_area_t *dst_areas,
					     snd_pcm_uframes_t dst_offset, snd_pcm_uframes_t *dst_framesp,
					     const snd_pcm_channel_area_t *src_areas,
					     snd_pcm_uframes_t src_offset, snd_pcm_uframes_t src_frames,
					     unsigned int channels,
					     unsigned int getidx, unsigned int putidx,
					     unsigned int get_increment,
					     snd_pcm_rate_state_t *states)
{
#define GET16_LABELS
#define PUT16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
#undef PUT16_LABELS
	void *get = get16_labels[getidx];
	void *put = put16_labels[putidx];
	unsigned int channel;
	snd_pcm_uframes_t src_frames1 = 0;
	snd_pcm_uframes_t dst_frames1 = 0;
	snd_pcm_uframes_t dst_frames = *dst_framesp;
	int16_t sample = 0;

	if (src_frames == 0 ||
	    dst_frames == 0)
		return 0;
	for (channel = 0; channel < channels; ++channel) {
		const snd_pcm_channel_area_t *src_area = &src_areas[channel];
		const snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
		unsigned int pos;
		int sum;
		const char *src;
		char *dst;
		int src_step, dst_step;
		sum = states->u.linear.sum;
		pos = states->u.linear.pos;
		states->u.linear.init = 0;
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
			src += src_step;
			src_frames1++;
			pos += get_increment;
			if (pos >= DIV) {
				int s = sample;
				pos -= DIV;
				sum += s * (get_increment - pos);
				sum /= DIV;
				sample = sum;
				goto *put;
#define PUT16_END after_put
#include "plugin_ops.h"
#undef PUT16_END
			after_put:
				dst += dst_step;
				sum = s * pos;
				dst_frames1++;
				if (dst_frames1 == dst_frames)
					break;
			} else
				sum += sample * get_increment;
		}
		states->u.linear.sum = sum;
		states->u.linear.pos = pos;
		states++;
	}
	*dst_framesp = dst_frames1;
	return src_frames1;
}

#endif /* DOC_HIDDEN */

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
	const snd_interval_t *sbuffer_size;
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
	err = _snd_pcm_hw_params_refine(params, links, sparams);
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
				       snd_pcm_plugin_hw_refine_slave);
}

static int snd_pcm_rate_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->plug.slave;
	snd_pcm_format_t src_format, dst_format;
	unsigned int src_rate, dst_rate;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_rate_hw_refine_cchange,
					  snd_pcm_rate_hw_refine_sprepare,
					  snd_pcm_rate_hw_refine_schange,
					  snd_pcm_plugin_hw_params_slave);
	if (err < 0)
		return err;

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		err = INTERNAL(snd_pcm_hw_params_get_format)(params, &src_format);
		if (err < 0)
			return err;
		dst_format = slave->format;
		err = INTERNAL(snd_pcm_hw_params_get_rate)(params, &src_rate, 0);
		dst_rate = slave->rate;
	} else {
		src_format = slave->format;
		err = INTERNAL(snd_pcm_hw_params_get_format)(params, &dst_format);
		if (err < 0)
			return err;
		src_rate = slave->rate;
		err = INTERNAL(snd_pcm_hw_params_get_rate)(params, &dst_rate, 0);
	}
	if (err < 0)
		return err;
	rate->get_idx = snd_pcm_linear_get_index(src_format, SND_PCM_FORMAT_S16);
	rate->put_idx = snd_pcm_linear_put_index(SND_PCM_FORMAT_S16, dst_format);
	if (src_rate < dst_rate) {
		rate->func = snd_pcm_rate_expand;
		/* pitch is get_threshold */
	} else {
		rate->func = snd_pcm_rate_shrink;
		/* pitch is get_increment */
	}
	rate->pitch = (((u_int64_t)dst_rate * DIV) + src_rate / 2) / src_rate;
	assert(!rate->states);
	rate->states = malloc(rate->plug.slave->channels * sizeof(*rate->states));
	return 0;
}

static int snd_pcm_rate_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	if (rate->states) {
		free(rate->states);
		rate->states = NULL;
	}
	return snd_pcm_hw_free(rate->plug.slave);
}

static void recalc(snd_pcm_t *pcm, snd_pcm_uframes_t *val)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->plug.slave;
	unsigned long div;

	if (*val == pcm->buffer_size) {
		*val = slave->buffer_size;
	} else {
		div = *val / pcm->period_size;
		if (div * pcm->period_size == *val)
			*val = div * slave->period_size;
		else
			*val = muldiv_near(*val, slave->rate, pcm->rate);
	}
}

static int snd_pcm_rate_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	snd_pcm_t *slave = rate->plug.slave;
	snd_pcm_sw_params_t sparams;
	snd_pcm_uframes_t boundary1, boundary2;

	sparams = *params;
	if ((rate->pitch >= DIV ? 1 : 0) ^ (pcm->stream == SND_PCM_STREAM_CAPTURE ? 1 : 0)) {
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
	sparams.boundary = boundary2;
#if 0
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		rate->pitch = (((u_int64_t)boundary2 * DIV) + boundary1 / 2) / boundary1;
	} else {
		rate->pitch = (((u_int64_t)boundary1 * DIV) + boundary2 / 2) / boundary2;
	}
#endif
	recalc(pcm, &sparams.avail_min);
	recalc(pcm, &sparams.xfer_align);
	recalc(pcm, &sparams.start_threshold);
	if (sparams.stop_threshold >= sparams.boundary) {
		sparams.stop_threshold = sparams.boundary;
	} else {
		recalc(pcm, &sparams.stop_threshold);
	}
	recalc(pcm, &sparams.silence_threshold);
	recalc(pcm, &sparams.silence_size);
	return snd_pcm_sw_params(slave, &sparams);
}

static int snd_pcm_rate_init(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	unsigned int k;
	switch (rate->type) {
	case RATE_TYPE_LINEAR:
		for (k = 0; k < pcm->channels; ++k) {
			rate->states[k].u.linear.sum = 0;
			rate->states[k].u.linear.old_sample = 0;
			rate->states[k].u.linear.new_sample = 0;
			rate->states[k].u.linear.pos = 0;
			rate->states[k].u.linear.init = 0;
		}
		break;
	default:
		assert(0);
	}
	return 0;
}

static snd_pcm_uframes_t
snd_pcm_rate_write_areas(snd_pcm_t *pcm,
			 const snd_pcm_channel_area_t *areas,
			 snd_pcm_uframes_t offset,
			 snd_pcm_uframes_t size,
			 const snd_pcm_channel_area_t *slave_areas,
			 snd_pcm_uframes_t slave_offset,
			 snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	return rate->func(slave_areas, slave_offset, slave_sizep, 
			  areas, offset, size,
			  pcm->channels,
			  rate->get_idx, rate->put_idx,
			  rate->pitch, rate->states);
}

static snd_pcm_uframes_t
snd_pcm_rate_read_areas(snd_pcm_t *pcm,
			 const snd_pcm_channel_area_t *areas,
			 snd_pcm_uframes_t offset,
			 snd_pcm_uframes_t size,
			 const snd_pcm_channel_area_t *slave_areas,
			 snd_pcm_uframes_t slave_offset,
			 snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	*slave_sizep = rate->func(areas, offset, &size,
				  slave_areas, slave_offset, *slave_sizep,
				  pcm->channels,
				  rate->get_idx, rate->put_idx,
				  rate->pitch, rate->states);
	return size;
}

static snd_pcm_sframes_t snd_pcm_rate_client_frames(snd_pcm_t *pcm, snd_pcm_sframes_t frames)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	if (frames == 0)
		return 0;
	/* Round toward zero */
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return muldiv_down(frames, DIV, rate->pitch);
	else
		return muldiv_down(frames, rate->pitch, DIV);
}

static snd_pcm_sframes_t snd_pcm_rate_slave_frames(snd_pcm_t *pcm, snd_pcm_sframes_t frames)
{
	snd_pcm_rate_t *rate = pcm->private_data;
	/* Round toward zero */
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return muldiv_down(frames, rate->pitch, DIV);
	else
		return muldiv_down(frames, DIV, rate->pitch);
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
	snd_pcm_dump(rate->plug.slave, out);
}

static snd_pcm_ops_t snd_pcm_rate_ops = {
	.close = snd_pcm_plugin_close,
	.info = snd_pcm_plugin_info,
	.hw_refine = snd_pcm_rate_hw_refine,
	.hw_params = snd_pcm_rate_hw_params,
	.hw_free = snd_pcm_rate_hw_free,
	.sw_params = snd_pcm_rate_sw_params,
	.channel_info = snd_pcm_plugin_channel_info,
	.dump = snd_pcm_rate_dump,
	.nonblock = snd_pcm_plugin_nonblock,
	.async = snd_pcm_plugin_async,
	.poll_revents = snd_pcm_plugin_poll_revents,
	.mmap = snd_pcm_plugin_mmap,
	.munmap = snd_pcm_plugin_munmap,
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
	snd_pcm_plugin_init(&rate->plug);
	rate->type = RATE_TYPE_LINEAR;
	rate->srate = srate;
	rate->sformat = sformat;
	rate->plug.read = snd_pcm_rate_read_areas;
	rate->plug.write = snd_pcm_rate_write_areas;
	rate->plug.client_frames = snd_pcm_rate_client_frames;
	rate->plug.slave_frames = snd_pcm_rate_slave_frames;
	rate->plug.init = snd_pcm_rate_init;
	rate->plug.slave = slave;
	rate->plug.close_slave = close_slave;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_RATE, name, slave->stream, slave->mode);
	if (err < 0) {
		free(rate);
		return err;
	}
	pcm->ops = &snd_pcm_rate_ops;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->private_data = rate;
	pcm->poll_fd = slave->poll_fd;
	pcm->poll_events = slave->poll_events;
	snd_pcm_set_hw_ptr(pcm, &rate->plug.hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &rate->plug.appl_ptr, -1, 0);
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
