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

static int snd_pcm_plug_slave_format(int format, unsigned int format_mask)
{
	int w, u, e, wid, w1, dw;
	if (format_mask & (1 << format))
		return format;
	if (!((1 << format) & SND_PCM_FMTBIT_LINEAR)) {
		unsigned int i;
		switch (format) {
		case SND_PCM_FORMAT_MU_LAW:
		case SND_PCM_FORMAT_A_LAW:
		case SND_PCM_FORMAT_IMA_ADPCM:
			for (i = 0; i < sizeof(linear_preferred_formats) / sizeof(linear_preferred_formats[0]); ++i) {
				unsigned int f = linear_preferred_formats[i];
				if (format_mask & (1 << f))
					return f;
			}
			/* Fall through */
		default:
			return -EINVAL;
		}

	}
	if (!(format_mask & SND_PCM_FMTBIT_LINEAR)) {
		unsigned int i;
		for (i = 0; i < sizeof(nonlinear_preferred_formats) / sizeof(nonlinear_preferred_formats[0]); ++i) {
			unsigned int f = nonlinear_preferred_formats[i];
			if (format_mask & (1 << f))
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
				if (f >= 0 && format_mask & (1 << f))
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

static int snd_pcm_plug_hw_info(snd_pcm_t *pcm, snd_pcm_hw_info_t *info)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	snd_pcm_hw_info_t sinfo;
	int rate_min, rate_max;
	int channels_min, channels_max;
	unsigned int format, format_mask;
	int err;
	
	info->access_mask &= (SND_PCM_ACCBIT_MMAP_INTERLEAVED | 
			      SND_PCM_ACCBIT_RW_INTERLEAVED |
			      SND_PCM_ACCBIT_MMAP_NONINTERLEAVED | 
			      SND_PCM_ACCBIT_RW_NONINTERLEAVED);
	if (info->access_mask == 0)
		return -EINVAL;

	info->format_mask &= (SND_PCM_FMTBIT_LINEAR | SND_PCM_FMTBIT_MU_LAW |
			      SND_PCM_FMTBIT_A_LAW | SND_PCM_FMTBIT_IMA_ADPCM);
	if (info->format_mask == 0)
		return -EINVAL;

	info->subformat_mask &= SND_PCM_SUBFMTBIT_STD;
	if (info->subformat_mask == 0)
		return -EINVAL;

	if (info->rate_min < RATE_MIN)
		info->rate_min = RATE_MIN;
	if (info->rate_max > RATE_MAX)
		info->rate_max = RATE_MAX;
	if (info->rate_max < info->rate_min)
		return -EINVAL;

	if (info->channels_min < 1)
		info->channels_min = 1;
	if (info->channels_max < info->channels_min)
		return -EINVAL;

	sinfo = *info;
	sinfo.access_mask = SND_PCM_ACCBIT_MMAP;
	sinfo.format_mask = (SND_PCM_FMTBIT_LINEAR | SND_PCM_FMTBIT_MU_LAW |
			     SND_PCM_FMTBIT_A_LAW | SND_PCM_FMTBIT_IMA_ADPCM);
	sinfo.channels_min = 1;
	sinfo.channels_max = UINT_MAX;
	sinfo.rate_min = RATE_MIN;
	sinfo.rate_max = RATE_MAX;

	err = snd_pcm_hw_info(slave, &sinfo);
	if (err < 0)
		goto _err;
	
	rate_min = snd_pcm_hw_info_par_nearest_next(&sinfo,
						    SND_PCM_HW_INFO_RATE,
						    info->rate_min, -1,
						    slave);
	assert(rate_min >= 0);
	if ((int)info->rate_max - rate_min > 1) {
		rate_max = snd_pcm_hw_info_par_nearest_next(&sinfo,
							    SND_PCM_HW_INFO_RATE,
							    info->rate_max, -1,
							    slave);
		assert(rate_max >= 0);
	} else
		rate_max = rate_min;
	sinfo.rate_min = rate_min;
	sinfo.rate_max = rate_max;

	err = snd_pcm_hw_info(slave, &sinfo);
	assert(err >= 0);

	channels_min = snd_pcm_hw_info_par_nearest_next(&sinfo,
						    SND_PCM_HW_INFO_CHANNELS,
						    info->channels_min, -1,
						    slave);
	assert(channels_min >= 0);
	if ((int)info->channels_max - channels_min > 1) {
		channels_max = snd_pcm_hw_info_par_nearest_next(&sinfo,
							    SND_PCM_HW_INFO_CHANNELS,
							    info->channels_max, -1,
							    slave);
		assert(channels_max >= 0);
	} else
		channels_max = channels_min;
	sinfo.channels_min = channels_min;
	sinfo.channels_max = channels_max;

	err = snd_pcm_hw_info(slave, &sinfo);
	assert(err >= 0);

	format_mask = 0;
	for (format = 0; format <= SND_PCM_FORMAT_LAST; format++) {
		int f;
		if (!(info->format_mask & (1 << format)))
			continue;
		f = snd_pcm_plug_slave_format(format, sinfo.format_mask);
		assert(f >= 0);
		format_mask |= (1 << f);
	}
	sinfo.format_mask = format_mask;

	err = snd_pcm_hw_info(slave, &sinfo);
	assert(err >= 0);

 _err:
	info->subformat_mask = sinfo.subformat_mask;
	info->fragment_length_min = sinfo.fragment_length_min;
	info->fragment_length_max = sinfo.fragment_length_max;
	info->fragments_min = sinfo.fragments_min;
	info->fragments_max = sinfo.fragments_max;
	info->buffer_length_min = sinfo.buffer_length_min;
	info->buffer_length_max = sinfo.buffer_length_max;
	
	if (err < 0)
		return err;
	info->info = sinfo.info & ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	snd_pcm_hw_info_complete(info);
	return 0;
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

static int snd_pcm_plug_change_rate(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_hw_params_t *clt, snd_pcm_hw_params_t *slv)
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

static int snd_pcm_plug_change_channels(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_hw_params_t *clt, snd_pcm_hw_params_t *slv)
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

static int snd_pcm_plug_change_format(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_hw_params_t *clt, snd_pcm_hw_params_t *slv)
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

static int snd_pcm_plug_change_access(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_hw_params_t *clt, snd_pcm_hw_params_t *slv)
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
				       snd_pcm_hw_params_t *client,
				       snd_pcm_hw_params_t *slave)
{
	snd_pcm_plug_t *plug = pcm->private;
	int (*funcs[])(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_hw_params_t *s, snd_pcm_hw_params_t *d) = {
		snd_pcm_plug_change_format,
		snd_pcm_plug_change_channels,
		snd_pcm_plug_change_rate,
		snd_pcm_plug_change_channels,
		snd_pcm_plug_change_format,
		snd_pcm_plug_change_access
	};
	snd_pcm_hw_params_t p = *slave;
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
	snd_pcm_hw_info_t sinfo;
	snd_pcm_hw_params_t sparams;
	int format;
	int err;

	sparams = *params;
	snd_pcm_hw_params_to_info(&sparams, &sinfo);
	sinfo.access_mask = ~0U;
	sinfo.format_mask = (SND_PCM_FMTBIT_LINEAR | SND_PCM_FMTBIT_MU_LAW |
			     SND_PCM_FMTBIT_A_LAW | SND_PCM_FMTBIT_IMA_ADPCM);
	sinfo.subformat_mask = SND_PCM_SUBFMTBIT_STD;
	sinfo.channels_min = 1;
	sinfo.channels_max = UINT_MAX;
	sinfo.rate_min = RATE_MIN;
	sinfo.rate_max = RATE_MAX;

	err = snd_pcm_hw_info(slave, &sinfo);
	if (err < 0)
		return err;

	err = snd_pcm_hw_info_par_nearest_next(&sinfo,
					       SND_PCM_HW_INFO_RATE,
					       params->rate, -1,
					       slave);
	if (err < 0)
		return err;
	sinfo.rate_min = sinfo.rate_max = err;

	err = snd_pcm_hw_info(slave, &sinfo);
	assert(err >= 0);

	err = snd_pcm_hw_info_par_nearest_next(&sinfo,
					       SND_PCM_HW_INFO_CHANNELS,
					       params->channels, -1,
					       slave);
	if (err < 0)
		return err;
	sinfo.channels_min = sinfo.channels_max = err;

	err = snd_pcm_hw_info(slave, &sinfo);
	assert(err >= 0);

	format = snd_pcm_plug_slave_format(params->format, sinfo.format_mask);
	assert(format >= 0);

	sparams.format = format;
	assert(sinfo.rate_min == sinfo.rate_max);
	sparams.rate = sinfo.rate_min;
	assert(sinfo.channels_min == sinfo.channels_max);
	sparams.channels = sinfo.channels_min;

	snd_pcm_plug_clear(pcm);
	if (params->format != sparams.format ||
	    params->channels != sparams.channels ||
	    params->rate != sparams.rate ||
	    !(sinfo.access_mask & (1 << params->access))) {
		sinfo.access_mask &= SND_PCM_ACCBIT_MMAP;
		assert(sinfo.access_mask);
		sparams.access = ffs(sinfo.access_mask) - 1;
		err = snd_pcm_plug_insert_plugins(pcm, params, &sparams);
		if (err < 0)
			return err;
	}

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

static int snd_pcm_plug_dig_info(snd_pcm_t *pcm, snd_pcm_dig_info_t * info)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_dig_info(plug->slave, info);
}

static int snd_pcm_plug_dig_params(snd_pcm_t *pcm, snd_pcm_dig_params_t * params)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_dig_params(plug->slave, params);
}


static int snd_pcm_plug_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_channel_info(plug->slave, info);
}

static int snd_pcm_plug_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
#if 0
	snd_pcm_plugin_t *plug = pcm->private;
	return snd_pcm_mmap(plug->slave);
#endif
	return 0;
}

static int snd_pcm_plug_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
#if 0
	snd_pcm_plugin_t *plug = pcm->private;
	return snd_pcm_munmap(plug->slave);
#endif
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
	info: snd_pcm_plug_info,
	hw_info: snd_pcm_plug_hw_info,
	hw_params: snd_pcm_plug_hw_params,
	sw_params: snd_pcm_plug_sw_params,
	dig_info: snd_pcm_plug_dig_info,
	dig_params: snd_pcm_plug_dig_params,
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
				
