/*
 *  PCM - Plug
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
  
#include "pcm_local.h"
#include "pcm_plugin.h"
#include "interval.h"

typedef struct {
	snd_pcm_t *req_slave;
	int close_slave;
	snd_pcm_t *slave;
	ttable_entry_t *ttable;
	unsigned int tt_ssize, tt_cused, tt_sused;
} snd_pcm_plug_t;

static int snd_pcm_plug_close(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	int err, result = 0;
	if (plug->ttable)
		free(plug->ttable);
	if (plug->slave != plug->req_slave) {
		err = snd_pcm_close(plug->slave);
		if (err < 0)
			result = err;
	}
	if (plug->close_slave) {
		err = snd_pcm_close(plug->req_slave);
		if (err < 0)
			result = err;
	}
	free(plug);
	return result;
}

static int snd_pcm_plug_card(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_card(plug->slave);
}

static int snd_pcm_plug_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_nonblock(plug->slave, nonblock);
}

static int snd_pcm_plug_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_async(plug->slave, sig, pid);
}

static int snd_pcm_plug_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	int err;
	
	if ((err = snd_pcm_info(slave, info)) < 0)
		return err;
	return 0;
}

static unsigned int linear_preferred_formats[] = {
#ifdef SND_LITTLE_ENDIAN
	SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_U16_LE,
	SND_PCM_FORMAT_S16_BE,
	SND_PCM_FORMAT_U16_BE,
#else
	SND_PCM_FORMAT_S16_BE,
	SND_PCM_FORMAT_U16_BE,
	SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_U16_LE,
#endif
#ifdef SND_LITTLE_ENDIAN
	SND_PCM_FORMAT_S24_LE,
	SND_PCM_FORMAT_U24_LE,
	SND_PCM_FORMAT_S24_BE,
	SND_PCM_FORMAT_U24_BE,
#else
	SND_PCM_FORMAT_S24_BE,
	SND_PCM_FORMAT_U24_BE,
	SND_PCM_FORMAT_S24_LE,
	SND_PCM_FORMAT_U24_LE,
#endif
#ifdef SND_LITTLE_ENDIAN
	SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_U32_LE,
	SND_PCM_FORMAT_S32_BE,
	SND_PCM_FORMAT_U32_BE,
#else
	SND_PCM_FORMAT_S32_BE,
	SND_PCM_FORMAT_U32_BE,
	SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_U32_LE,
#endif
	SND_PCM_FORMAT_S8,
	SND_PCM_FORMAT_U8
};

static unsigned int nonlinear_preferred_formats[] = {
	SND_PCM_FORMAT_MU_LAW,
	SND_PCM_FORMAT_A_LAW,
	SND_PCM_FORMAT_IMA_ADPCM,
};

static int snd_pcm_plug_slave_format(int format, const mask_t *format_mask)
{
	int w, u, e, wid, w1, dw;
	mask_t *lin = alloca(mask_sizeof());
	if (mask_test(format_mask, format))
		return format;
	mask_load(lin, SND_PCM_FMTBIT_LINEAR);
	if (!mask_test(lin, format)) {
		unsigned int i;
		switch (format) {
		case SND_PCM_FORMAT_MU_LAW:
		case SND_PCM_FORMAT_A_LAW:
		case SND_PCM_FORMAT_IMA_ADPCM:
			for (i = 0; i < sizeof(linear_preferred_formats) / sizeof(linear_preferred_formats[0]); ++i) {
				unsigned int f = linear_preferred_formats[i];
				if (mask_test(format_mask, f))
					return f;
			}
			/* Fall through */
		default:
			return -EINVAL;
		}

	}
	mask_intersect(lin, format_mask);
	if (mask_empty(lin)) {
		unsigned int i;
		for (i = 0; i < sizeof(nonlinear_preferred_formats) / sizeof(nonlinear_preferred_formats[0]); ++i) {
			unsigned int f = nonlinear_preferred_formats[i];
			if (mask_test(format_mask, f))
				return f;
		}
		return -EINVAL;
	}
	w = snd_pcm_format_width(format);
	u = snd_pcm_format_unsigned(format);
	e = snd_pcm_format_big_endian(format);
	w1 = w;
	dw = 8;
	for (wid = 0; wid < 4; ++wid) {
		int end, e1 = e;
		for (end = 0; end < 2; ++end) {
			int sgn, u1 = u;
			for (sgn = 0; sgn < 2; ++sgn) {
				int f;
				f = snd_pcm_build_linear_format(w1, u1, e1);
				if (f >= 0 && mask_test(format_mask, f))
					return f;
				u1 = !u1;
			}
			e1 = !e1;
		}
		if (w1 < 32)
			w1 += dw;
		else {
			w1 = w - 8;
			dw = -8;
		}
	}
	return -EINVAL;
}

#define SND_PCM_FMTBIT_PLUG (SND_PCM_FMTBIT_LINEAR | \
			     (1 << SND_PCM_FORMAT_MU_LAW) | \
			     (1 << SND_PCM_FORMAT_A_LAW) | \
			     (1 << SND_PCM_FORMAT_IMA_ADPCM))


/* Accumulate to params->cmask */
/* Reset sparams->cmask */
static int snd_pcm_plug_hw_link(snd_pcm_hw_params_t *params,
				snd_pcm_hw_params_t *sparams,
				snd_pcm_t *slave,
				unsigned long private ATTRIBUTE_UNUSED)
{
	int rate_always, channels_always, format_always;
	int rate_never, channels_never, format_never;
	unsigned int links = (SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	const mask_t *format_mask, *sformat_mask;
	mask_t *fmt_mask = alloca(mask_sizeof());
	mask_t *sfmt_mask = alloca(mask_sizeof());
	mask_t *access_mask = alloca(mask_sizeof());
	int err;
	unsigned int format;
	unsigned int scmask = sparams->cmask;
	snd_pcm_hw_param_near_copy(slave, sparams, SND_PCM_HW_PARAM_RATE,
				   params);
	scmask |= sparams->cmask;
	snd_pcm_hw_param_near_copy(slave, sparams, SND_PCM_HW_PARAM_CHANNELS,
				   params);
	scmask |= sparams->cmask;
	format_mask = snd_pcm_hw_param_value_mask(params,
						   SND_PCM_HW_PARAM_FORMAT);
	sformat_mask = snd_pcm_hw_param_value_mask(sparams,
						    SND_PCM_HW_PARAM_FORMAT);
	mask_none(sfmt_mask);
	mask_none(fmt_mask);
	for (format = 0; format <= SND_PCM_FORMAT_LAST; format++) {
		int f;
		if (!mask_test(format_mask, format))
			continue;
		if (mask_test(sformat_mask, format))
			f = format;
		else {
			f = snd_pcm_plug_slave_format(format, sformat_mask);
			if (f < 0)
				continue;
		}
		mask_set(sfmt_mask, f);
		mask_set(fmt_mask, format);
	}

	err = _snd_pcm_hw_param_mask(params, 
				     SND_PCM_HW_PARAM_FORMAT, fmt_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_mask(sparams,
				     SND_PCM_HW_PARAM_FORMAT, sfmt_mask);
	assert(err >= 0);

	format_always = snd_pcm_hw_param_always_eq(params,
						   SND_PCM_HW_PARAM_FORMAT,
						   sparams);
	format_never = (!format_always &&
			snd_pcm_hw_param_never_eq(params,
						  SND_PCM_HW_PARAM_FORMAT,
						  sparams));
		
	channels_always = snd_pcm_hw_param_always_eq(params,
						     SND_PCM_HW_PARAM_CHANNELS,
						     sparams);
	channels_never = (!channels_always &&
			  snd_pcm_hw_param_never_eq(params,
						    SND_PCM_HW_PARAM_CHANNELS,
						    sparams));
		
	rate_always = snd_pcm_hw_param_always_eq(params,
						 SND_PCM_HW_PARAM_RATE,
						 sparams);
	rate_never = (!rate_always &&
		      snd_pcm_hw_param_never_eq(params,
						SND_PCM_HW_PARAM_RATE,
						sparams));

	if (rate_always)
		links |= (SND_PCM_HW_PARBIT_RATE |
			  SND_PCM_HW_PARBIT_PERIOD_SIZE |
			  SND_PCM_HW_PARBIT_PERIODS |
			  SND_PCM_HW_PARBIT_BUFFER_TIME |
			  SND_PCM_HW_PARBIT_BUFFER_SIZE);
	else {
		interval_t t;
		const interval_t *sbuffer_size, *buffer_size;
		const interval_t *srate, *rate;
		buffer_size = snd_pcm_hw_param_value_interval(params, SND_PCM_HW_PARAM_BUFFER_SIZE);
		sbuffer_size = snd_pcm_hw_param_value_interval(sparams, SND_PCM_HW_PARAM_BUFFER_SIZE);
		rate = snd_pcm_hw_param_value_interval(params, SND_PCM_HW_PARAM_RATE);
		srate = snd_pcm_hw_param_value_interval(sparams, SND_PCM_HW_PARAM_RATE);
		interval_muldiv(sbuffer_size, rate, srate, &t);
		interval_round(&t);
		err = _snd_pcm_hw_param_refine_interval(params, SND_PCM_HW_PARAM_BUFFER_SIZE, &t);
		if (err < 0)
			return err;
		interval_muldiv(buffer_size, srate, rate, &t);
		interval_round(&t);
		err = _snd_pcm_hw_param_refine_interval(sparams, SND_PCM_HW_PARAM_BUFFER_SIZE, &t);
		assert(err >= 0);
		scmask |= sparams->cmask;
	}
	if (channels_always)
		links |= SND_PCM_HW_PARBIT_CHANNELS;
	if (format_always) {
		links |= (SND_PCM_HW_PARBIT_FORMAT |
			  SND_PCM_HW_PARBIT_SUBFORMAT |
			  SND_PCM_HW_PARBIT_SAMPLE_BITS);
		if (channels_always) {
			links |= SND_PCM_HW_PARBIT_FRAME_BITS;
			if (rate_always) 
				links |= (SND_PCM_HW_PARBIT_PERIOD_BYTES |
					  SND_PCM_HW_PARBIT_BUFFER_BYTES);
		}
	}

	mask_load(access_mask, SND_PCM_ACCBIT_PLUGIN);
	if (format_never || channels_never || rate_never) {
		mask_t *mmap_mask = alloca(mask_sizeof());
		mask_load(mmap_mask, SND_PCM_ACCBIT_MMAP);
		_snd_pcm_hw_param_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				       mmap_mask);
	} else
		mask_union(access_mask, snd_pcm_hw_param_value_mask(sparams, SND_PCM_HW_PARAM_ACCESS));
	_snd_pcm_hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS,
			       access_mask);
	sparams->cmask |= scmask;
	return snd_pcm_generic_hw_link(params, sparams, slave, links);
}

static int snd_pcm_plug_hw_refine1(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
				   snd_pcm_hw_params_t *sparams)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	unsigned int cmask, lcmask;
	int err;
	
	cmask = params->cmask;
	params->cmask = 0;
	err = _snd_pcm_hw_param_min(params, SND_PCM_HW_PARAM_CHANNELS, 1, 0);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_max(params, SND_PCM_HW_PARAM_CHANNELS, 1024, 0);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_min(params, SND_PCM_HW_PARAM_RATE, RATE_MIN, 0);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_max(params, SND_PCM_HW_PARAM_RATE, RATE_MAX, 0);
	if (err < 0)
		return err;
	lcmask = params->cmask;
	params->cmask |= cmask;

	_snd_pcm_hw_params_any(sparams);
	err = snd_pcm_hw_refine2(params, sparams,
				 snd_pcm_plug_hw_link, slave, 0);
	params->cmask |= lcmask;
	if (err < 0)
		return err;
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}



static int snd_pcm_plug_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_hw_params_t sparams;
	int err;
#if 0
	fprintf(stderr, "Enter: client =\n");
	snd_pcm_hw_params_dump(params, stderr);
#endif
	err = snd_pcm_plug_hw_refine1(pcm, params, &sparams);
#if 0
	fprintf(stderr, "Exit: client =\n");
	snd_pcm_hw_params_dump(params, stderr);
	fprintf(stderr, "Exit: slave =\n");
	snd_pcm_hw_params_dump(&sparams, stderr);
#endif
	return err;
}

static void snd_pcm_plug_clear(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	/* Clear old plugins */
	if (plug->slave != slave) {
		snd_pcm_close(plug->slave);
		plug->slave = slave;
		pcm->fast_ops = slave->fast_ops;
		pcm->fast_op_arg = slave->fast_op_arg;
	}
}

typedef struct {
	unsigned int access;
	unsigned int format;
	unsigned int channels;
	unsigned int rate;
} snd_pcm_plug_params_t;

static int snd_pcm_plug_change_rate(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_plug_params_t *clt, snd_pcm_plug_params_t *slv)
{
	snd_pcm_plug_t *plug = pcm->private;
	int err;
	assert(snd_pcm_format_linear(slv->format));
	if (clt->rate == slv->rate)
		return 0;
	err = snd_pcm_rate_open(new, NULL, slv->format, slv->rate, plug->slave, plug->slave != plug->req_slave);
	if (err < 0)
		return err;
	slv->access = clt->access;
	slv->rate = clt->rate;
	if (snd_pcm_format_linear(clt->format))
		slv->format = clt->format;
	return 1;
}

static int snd_pcm_plug_change_channels(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_plug_params_t *clt, snd_pcm_plug_params_t *slv)
{
	snd_pcm_plug_t *plug = pcm->private;
	unsigned int tt_ssize, tt_cused, tt_sused;
	ttable_entry_t *ttable;
	int err;
	assert(snd_pcm_format_linear(slv->format));
	if (clt->channels == slv->channels)
		return 0;
	if (clt->rate != slv->rate &&
	    clt->channels > slv->channels)
		return 0;
	     
	ttable = plug->ttable;
	if (ttable) {
		tt_ssize = plug->tt_ssize;
		tt_cused = plug->tt_cused;
		tt_sused = plug->tt_sused;
	} else {
		unsigned int k;
		unsigned int c = 0, s = 0;
		int n;
		tt_ssize = slv->channels;
		tt_cused = clt->channels;
		tt_sused = slv->channels;
		ttable = alloca(tt_cused * tt_sused * sizeof(*ttable));
		for (k = 0; k < tt_cused * tt_sused; ++k)
			ttable[k] = 0;
		if (clt->channels > slv->channels) {
			n = clt->channels;
		} else {
			n = slv->channels;
		}
		while (n-- > 0) {
			ttable_entry_t v = FULL;
			if (pcm->stream == SND_PCM_STREAM_PLAYBACK &&
			    clt->channels > slv->channels) {
				int srcs = clt->channels / slv->channels;
				if (s < clt->channels % slv->channels)
					srcs++;
				v /= srcs;
			} else if (pcm->stream == SND_PCM_STREAM_CAPTURE &&
				   slv->channels > clt->channels) {
				int srcs = slv->channels / clt->channels;
				if (s < slv->channels % clt->channels)
					srcs++;
				v /= srcs;
			}
			ttable[c * tt_ssize + s] = v;
			if (++c == clt->channels)
				c = 0;
			if (++s == slv->channels)
				s = 0;
		}
	}
	err = snd_pcm_route_open(new, NULL, slv->format, slv->channels, ttable, tt_ssize, tt_cused, tt_sused, plug->slave, plug->slave != plug->req_slave);
	if (err < 0)
		return err;
	slv->channels = clt->channels;
	slv->access = clt->access;
	if (snd_pcm_format_linear(clt->format))
		slv->format = clt->format;
	return 1;
}

static int snd_pcm_plug_change_format(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_plug_params_t *clt, snd_pcm_plug_params_t *slv)
{
	snd_pcm_plug_t *plug = pcm->private;
	int err, cfmt;
	int (*f)(snd_pcm_t **pcm, char *name, int sformat, snd_pcm_t *slave, int close_slave);
	if (snd_pcm_format_linear(slv->format)) {
		/* Conversion is done in another plugin */
		if (clt->format == slv->format ||
		    clt->rate != slv->rate ||
		    clt->channels != slv->channels)
			return 0;
		cfmt = clt->format;
		switch (clt->format) {
		case SND_PCM_FORMAT_MU_LAW:
			f = snd_pcm_mulaw_open;
			break;
		case SND_PCM_FORMAT_A_LAW:
			f = snd_pcm_alaw_open;
			break;
		case SND_PCM_FORMAT_IMA_ADPCM:
			f = snd_pcm_adpcm_open;
			break;
		default:
			assert(snd_pcm_format_linear(clt->format));
			f = snd_pcm_linear_open;
			break;
		}
	} else {
		/* No conversion is needed */
		if (clt->format == slv->format &&
		    clt->rate == slv->rate &&
		    clt->channels == clt->channels)
			return 0;
		switch (slv->format) {
		case SND_PCM_FORMAT_MU_LAW:
			f = snd_pcm_mulaw_open;
			break;
		case SND_PCM_FORMAT_A_LAW:
			f = snd_pcm_alaw_open;
			break;
		case SND_PCM_FORMAT_IMA_ADPCM:
			f = snd_pcm_adpcm_open;
			break;
		default:
			assert(0);
			return -EINVAL;
		}
		if (snd_pcm_format_linear(clt->format))
			cfmt = clt->format;
		else
			cfmt = SND_PCM_FORMAT_S16;
	}
	err = f(new, NULL, slv->format, plug->slave, plug->slave != plug->req_slave);
	if (err < 0)
		return err;
	slv->format = cfmt;
	slv->access = clt->access;
	return 1;
}

static int snd_pcm_plug_change_access(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_plug_params_t *clt, snd_pcm_plug_params_t *slv)
{
	snd_pcm_plug_t *plug = pcm->private;
	int err;
	if (clt->access == slv->access)
		return 0;
	err = snd_pcm_copy_open(new, NULL, plug->slave, plug->slave != plug->req_slave);
	if (err < 0)
		return err;
	slv->access = clt->access;
	return 1;
}

static int snd_pcm_plug_insert_plugins(snd_pcm_t *pcm,
				       snd_pcm_plug_params_t *client,
				       snd_pcm_plug_params_t *slave)
{
	snd_pcm_plug_t *plug = pcm->private;
	int (*funcs[])(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_plug_params_t *s, snd_pcm_plug_params_t *d) = {
		snd_pcm_plug_change_format,
		snd_pcm_plug_change_channels,
		snd_pcm_plug_change_rate,
		snd_pcm_plug_change_channels,
		snd_pcm_plug_change_format,
		snd_pcm_plug_change_access
	};
	snd_pcm_plug_params_t p = *slave;
	unsigned int k = 0;
	while (client->format != p.format ||
	       client->channels != p.channels ||
	       client->rate != p.rate ||
	       client->access != p.access) {
		snd_pcm_t *new;
		int err;
		assert(k < sizeof(funcs)/sizeof(*funcs));
		err = funcs[k](pcm, &new, client, &p);
		if (err < 0) {
			snd_pcm_plug_clear(pcm);
			return err;
		}
		if (err) {
			plug->slave = new;
			pcm->fast_ops = new->fast_ops;
			pcm->fast_op_arg = new->fast_op_arg;
		}
		k++;
	}
	return 0;
}

static int snd_pcm_plug_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	snd_pcm_plug_params_t clt_params, slv_params;
	snd_pcm_hw_params_t sparams;
	int err = snd_pcm_plug_hw_refine1(pcm, params, &sparams);
	assert(err >= 0);

	clt_params.access = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_ACCESS, 0);
	clt_params.format = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_FORMAT, 0);
	clt_params.channels = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_CHANNELS, 0);
	clt_params.rate = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_RATE, 0);

	if (snd_pcm_hw_param_test(params, SND_PCM_HW_PARAM_ACCESS, clt_params.access))
		slv_params.access = clt_params.access;
	else
		slv_params.access = snd_pcm_hw_param_first(slave, &sparams, SND_PCM_HW_PARAM_ACCESS, 0);
	slv_params.format = snd_pcm_hw_param_value(&sparams, SND_PCM_HW_PARAM_FORMAT, 0);
	slv_params.channels = snd_pcm_hw_param_value(&sparams, SND_PCM_HW_PARAM_CHANNELS, 0);
	slv_params.rate = snd_pcm_hw_param_value(&sparams, SND_PCM_HW_PARAM_RATE, 0);

	snd_pcm_plug_clear(pcm);
	err = snd_pcm_plug_insert_plugins(pcm, &clt_params, &slv_params);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params(plug->slave, params);
	if (err < 0) {
		snd_pcm_plug_clear(pcm);
		return err;
	}
	pcm->hw_ptr = slave->hw_ptr;
	pcm->appl_ptr = slave->appl_ptr;
	return 0;
}

static int snd_pcm_plug_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	snd_pcm_uframes_t avail_min, xfer_align, silence_threshold, silence_size;
	int err;
	avail_min = params->avail_min;
	xfer_align = params->xfer_align;
	silence_threshold = params->silence_threshold;
	silence_size = params->silence_size;
	params->avail_min = muldiv_near(params->avail_min, slave->rate, pcm->rate);
	params->xfer_align = muldiv_near(params->xfer_align, slave->rate, pcm->rate);
	params->silence_threshold = muldiv_near(params->silence_threshold, slave->rate, pcm->rate);
	params->silence_size = muldiv_near(params->silence_size, slave->rate, pcm->rate);
	err = snd_pcm_sw_params(slave, params);
	params->avail_min = avail_min;
	params->xfer_align = xfer_align;
	params->silence_threshold = silence_threshold;
	params->silence_size = silence_size;
	params->boundary = LONG_MAX - pcm->buffer_size * 2 - LONG_MAX % pcm->buffer_size;
	return err;
}

static int snd_pcm_plug_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_channel_info(plug->slave, info);
}

static int snd_pcm_plug_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_plug_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_plug_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_plug_t *plug = pcm->private;
	fprintf(fp, "Plug PCM: ");
	snd_pcm_dump(plug->slave, fp);
}

snd_pcm_ops_t snd_pcm_plug_ops = {
	close: snd_pcm_plug_close,
	card: snd_pcm_plug_card,
	info: snd_pcm_plug_info,
	hw_refine: snd_pcm_plug_hw_refine,
	hw_params: snd_pcm_plug_hw_params,
	sw_params: snd_pcm_plug_sw_params,
	channel_info: snd_pcm_plug_channel_info,
	dump: snd_pcm_plug_dump,
	nonblock: snd_pcm_plug_nonblock,
	async: snd_pcm_plug_async,
	mmap: snd_pcm_plug_mmap,
	munmap: snd_pcm_plug_munmap,
};

int snd_pcm_plug_open(snd_pcm_t **pcmp,
		      char *name,
		      ttable_entry_t *ttable,
		      unsigned int tt_ssize,
		      unsigned int tt_cused, unsigned int tt_sused,
		      snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_plug_t *plug;
	assert(pcmp && slave);
	plug = calloc(1, sizeof(snd_pcm_plug_t));
	if (!plug)
		return -ENOMEM;
	plug->slave = plug->req_slave = slave;
	plug->close_slave = close_slave;
	plug->ttable = ttable;
	plug->tt_ssize = tt_ssize;
	plug->tt_cused = tt_cused;
	plug->tt_sused = tt_sused;
	
	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(plug);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_PLUG;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_plug_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = slave->fast_ops;
	pcm->fast_op_arg = slave->fast_op_arg;
	pcm->private = plug;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = slave->hw_ptr;
	pcm->appl_ptr = slave->appl_ptr;
	*pcmp = pcm;

	return 0;
}

int snd_pcm_plug_open_hw(snd_pcm_t **pcmp, char *name, int card, int device, int subdevice, int stream, int mode)
{
	snd_pcm_t *slave;
	int err;
	err = snd_pcm_hw_open(&slave, NULL, card, device, subdevice, stream, mode);
	if (err < 0)
		return err;
	return snd_pcm_plug_open(pcmp, name, 0, 0, 0, 0, slave, 1);
}

#define MAX_CHANNELS 32

int _snd_pcm_plug_open(snd_pcm_t **pcmp, char *name,
		       snd_config_t *conf, 
		       int stream, int mode)
{
	snd_config_iterator_t i;
	char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	snd_config_t *tt = NULL;
	ttable_entry_t *ttable = NULL;
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
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "ttable") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			tt = n;
			continue;
		}
		ERR("Unknown field %s", n->id);
		return -EINVAL;
	}
	if (!sname) {
		ERR("sname is not defined");
		return -EINVAL;
	}
	if (tt) {
		ttable = malloc(MAX_CHANNELS * MAX_CHANNELS * sizeof(*ttable));
		err = snd_pcm_route_load_ttable(tt, ttable, MAX_CHANNELS, MAX_CHANNELS,
						&cused, &sused, -1);
		if (err < 0)
			return err;
	}
		
	/* This is needed cause snd_config_update may destroy config */
	sname = strdup(sname);
	if (!sname)
		return  -ENOMEM;
	err = snd_pcm_open(&spcm, sname, stream, mode);
	free(sname);
	if (err < 0)
		return err;
	err = snd_pcm_plug_open(pcmp, name, ttable, MAX_CHANNELS, cused, sused, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				
