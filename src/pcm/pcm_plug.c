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

static int snd_pcm_plug_hw_refine1(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
				   snd_pcm_hw_params_t *sparams)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	int err;
	const mask_t *format_mask, *sformat_mask;
	unsigned int rate_min, rate_max, srate_min, srate_max;
	unsigned int channels_min, channels_max, schannels_min, schannels_max;
	unsigned int format;
	int same_rate, same_channels, same_format;
	snd_pcm_hw_params_t tmp;
	unsigned int links = (SND_PCM_HW_PARBIT_FRAGMENT_LENGTH |
			      SND_PCM_HW_PARBIT_FRAGMENTS |
			      SND_PCM_HW_PARBIT_BUFFER_LENGTH);
	mask_t *tmp_mask = alloca(mask_sizeof());
	mask_t *accplug_mask = alloca(mask_sizeof());
	mask_t *mmap_mask = alloca(mask_sizeof());
	mask_t *fmtplug_mask = alloca(mask_sizeof());
	mask_load(accplug_mask, SND_PCM_ACCBIT_PLUGIN);
	mask_load(mmap_mask, SND_PCM_ACCBIT_MMAP);
	mask_load(fmtplug_mask, SND_PCM_FMTBIT_PLUG);
	
	err = _snd_pcm_hw_params_min(params, 1, SND_PCM_HW_PARAM_CHANNELS, 1);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_min(params, 1, SND_PCM_HW_PARAM_RATE, RATE_MIN);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_max(params, 1, SND_PCM_HW_PARAM_RATE, RATE_MAX);
	if (err < 0)
		return err;

	_snd_pcm_hw_params_any(sparams);
	err = snd_pcm_hw_refine2(params, sparams,
				 snd_pcm_hw_refine, slave, links);
	if (err < 0)
		return err;
	
	rate_min = snd_pcm_hw_params_value_min(params, SND_PCM_HW_PARAM_RATE);
	rate_max = snd_pcm_hw_params_value_max(params, SND_PCM_HW_PARAM_RATE);
	tmp = *sparams;
	srate_min = snd_pcm_hw_params_near(slave, &tmp,
					   SND_PCM_HW_PARAM_RATE, rate_min);
	if (srate_min < rate_max) {
		tmp = *sparams;
		srate_max = snd_pcm_hw_params_near(slave, &tmp,
						   SND_PCM_HW_PARAM_RATE, rate_max);
	} else 
		srate_max = srate_min;
	err = snd_pcm_hw_params_minmax(slave, sparams,
				       SND_PCM_HW_PARAM_RATE,
				       srate_min, srate_max);
	assert(err >= 0);

	channels_min = snd_pcm_hw_params_value_min(params, SND_PCM_HW_PARAM_CHANNELS);
	channels_max = snd_pcm_hw_params_value_max(params, SND_PCM_HW_PARAM_CHANNELS);
	tmp = *sparams;
	schannels_min = snd_pcm_hw_params_near(slave, &tmp,
					       SND_PCM_HW_PARAM_CHANNELS, channels_min);
	if (schannels_min < channels_max) {
		tmp = *sparams;
		schannels_max = snd_pcm_hw_params_near(slave, &tmp,
						       SND_PCM_HW_PARAM_CHANNELS, channels_max);
	} else 
		schannels_max = schannels_min;
	err = snd_pcm_hw_params_minmax(slave, sparams,
				       SND_PCM_HW_PARAM_CHANNELS,
				       schannels_min, schannels_max);
	assert(err >= 0);

	format_mask = snd_pcm_hw_params_value_mask(params,
						   SND_PCM_HW_PARAM_FORMAT);
	sformat_mask = snd_pcm_hw_params_value_mask(sparams,
						    SND_PCM_HW_PARAM_FORMAT);
	mask_none(tmp_mask);
	for (format = 0; format <= SND_PCM_FORMAT_LAST; format++) {
		int f;
		if (!mask_test(format_mask, format))
			continue;
		if (mask_test(sformat_mask, format))
			f = format;
		else {
			f = snd_pcm_plug_slave_format(format, sformat_mask);
			if (f < 0) {
				mask_t *m = alloca(mask_sizeof());
				mask_all(m);
				mask_reset(m, format);
				err = _snd_pcm_hw_params_mask(params, 1, SND_PCM_HW_PARAM_FORMAT, m);
				if (err < 0)
					return err;
				continue;
			}
		}
		mask_set(tmp_mask, f);
	}

	err = _snd_pcm_hw_params_mask(sparams, 0, 
				      SND_PCM_HW_PARAM_FORMAT, tmp_mask);
	assert(err >= 0);
	sformat_mask = snd_pcm_hw_params_value_mask(sparams,
						    SND_PCM_HW_PARAM_FORMAT);

	same_rate = (rate_min == rate_max && 
		     rate_min == srate_min &&
		     rate_min == srate_max);
	if (same_rate)
		links |= (SND_PCM_HW_PARBIT_RATE |
			  SND_PCM_HW_PARBIT_FRAGMENT_SIZE |
			  SND_PCM_HW_PARBIT_BUFFER_SIZE);
	same_channels = (channels_min == channels_max && 
			 channels_min == schannels_min &&
			 channels_min == schannels_max);
	if (same_channels)
		links |= SND_PCM_HW_PARBIT_CHANNELS;
	same_format = (mask_single(format_mask) && 
		       mask_eq(format_mask, sformat_mask));
	if (same_format) {
		links |= (SND_PCM_HW_PARBIT_FORMAT |
			  SND_PCM_HW_PARBIT_SUBFORMAT |
			  SND_PCM_HW_PARBIT_SAMPLE_BITS);
		if (same_channels) {
			links |= SND_PCM_HW_PARBIT_FRAME_BITS;
			if (same_rate) 
				links |= (SND_PCM_HW_PARBIT_FRAGMENT_BYTES |
					  SND_PCM_HW_PARBIT_BUFFER_BYTES);
		}
	}

	if (same_rate && same_channels && same_format) {
		const mask_t *access_mask = snd_pcm_hw_params_value_mask(params, SND_PCM_HW_PARAM_ACCESS);
		mask_copy(tmp_mask, snd_pcm_hw_params_value_mask(sparams, SND_PCM_HW_PARAM_ACCESS));
		mask_intersect(tmp_mask, access_mask);
		if (!mask_empty(tmp_mask))
			_snd_pcm_hw_params_mask(sparams, 0,
						SND_PCM_HW_PARAM_ACCESS,
						access_mask);
		else
		  _snd_pcm_hw_params_mask(sparams, 0, SND_PCM_HW_PARAM_ACCESS,
					  mmap_mask);
	} else {
		err = _snd_pcm_hw_params_mask(params, 1,
					      SND_PCM_HW_PARAM_ACCESS,
					      accplug_mask);
		if (err < 0)
			return err;
		err = _snd_pcm_hw_params_mask(params, 1,
					      SND_PCM_HW_PARAM_FORMAT,
					      fmtplug_mask);
		if (err < 0)
			return err;
		_snd_pcm_hw_params_mask(sparams, 0, SND_PCM_HW_PARAM_ACCESS,
					mmap_mask);
		_snd_pcm_hw_params_mask(sparams, 0, SND_PCM_HW_PARAM_FORMAT,
					fmtplug_mask);
	}
	err = snd_pcm_hw_refine2(params, sparams,
				 snd_pcm_hw_refine, slave, links);
	if (err < 0)
		return err;
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}



static int snd_pcm_plug_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_hw_params_t sparams;
	return snd_pcm_plug_hw_refine1(pcm, params, &sparams);
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

	clt_params.access = snd_pcm_hw_params_value(params, SND_PCM_HW_PARAM_ACCESS);
	clt_params.format = snd_pcm_hw_params_value(params, SND_PCM_HW_PARAM_FORMAT);
	clt_params.channels = snd_pcm_hw_params_value(params, SND_PCM_HW_PARAM_CHANNELS);
	clt_params.rate = snd_pcm_hw_params_value(params, SND_PCM_HW_PARAM_RATE);

	slv_params.access = snd_pcm_hw_params_first(slave, &sparams, SND_PCM_HW_PARAM_ACCESS);
	slv_params.format = snd_pcm_hw_params_value(&sparams, SND_PCM_HW_PARAM_FORMAT);
	slv_params.channels = snd_pcm_hw_params_value(&sparams, SND_PCM_HW_PARAM_CHANNELS);
	slv_params.rate = snd_pcm_hw_params_value(&sparams, SND_PCM_HW_PARAM_RATE);

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
	size_t avail_min, xfer_min, xfer_align;
	int err;
	avail_min = params->avail_min;
	xfer_min = params->xfer_min;
	xfer_align = params->xfer_align;
	params->avail_min = muldiv_near(params->avail_min, slave->rate, pcm->rate);
	params->xfer_min = muldiv_near(params->xfer_min, slave->rate, pcm->rate);
	params->xfer_align = muldiv_near(params->xfer_align, slave->rate, pcm->rate);
	err = snd_pcm_sw_params(slave, params);
	params->avail_min = avail_min;
	params->xfer_min = xfer_min;
	params->xfer_align = xfer_align;
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
				
