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


unsigned int snd_pcm_plug_formats(unsigned int formats)
{
	int fmts = (SND_PCM_LINEAR_FORMATS | SND_PCM_FMT_MU_LAW |
		    SND_PCM_FMT_A_LAW | SND_PCM_FMT_IMA_ADPCM);
	if (formats & fmts)
		formats |= fmts;
	return formats;
}

static int preferred_formats[] = {
	SND_PCM_SFMT_S16_LE,
	SND_PCM_SFMT_S16_BE,
	SND_PCM_SFMT_U16_LE,
	SND_PCM_SFMT_U16_BE,
	SND_PCM_SFMT_S24_LE,
	SND_PCM_SFMT_S24_BE,
	SND_PCM_SFMT_U24_LE,
	SND_PCM_SFMT_U24_BE,
	SND_PCM_SFMT_S32_LE,
	SND_PCM_SFMT_S32_BE,
	SND_PCM_SFMT_U32_LE,
	SND_PCM_SFMT_U32_BE,
	SND_PCM_SFMT_S8,
	SND_PCM_SFMT_U8
};

static int snd_pcm_plug_slave_fmt(int format,
				  snd_pcm_params_info_t *slave_info)
{
	if ((snd_pcm_plug_formats(slave_info->formats) & (1 << format)) == 0)
		return -EINVAL;
	if (snd_pcm_format_linear(format)) {
		int width = snd_pcm_format_width(format);
		int unsignd = snd_pcm_format_unsigned(format);
		int big = snd_pcm_format_big_endian(format);
		int format1;
		int wid, width1=width;
		int dwidth1 = 8;
		for (wid = 0; wid < 4; ++wid) {
			int end, big1 = big;
			for (end = 0; end < 2; ++end) {
				int sgn, unsignd1 = unsignd;
				for (sgn = 0; sgn < 2; ++sgn) {
					format1 = snd_pcm_build_linear_format(width1, unsignd1, big1);
					if (format1 >= 0 &&
					    slave_info->formats & (1 << format1))
						goto _found;
					unsignd1 = !unsignd1;
				}
				big1 = !big1;
			}
			if (width1 == 32) {
				dwidth1 = -dwidth1;
				width1 = width;
			}
			width1 += dwidth1;
		}
		return -EINVAL;
	_found:
		return format1;
	} else {
		unsigned int i;
		switch (format) {
		case SND_PCM_SFMT_MU_LAW:
		case SND_PCM_SFMT_A_LAW:
		case SND_PCM_SFMT_IMA_ADPCM:
			for (i = 0; i < sizeof(preferred_formats) / sizeof(preferred_formats[0]); ++i) {
				int format1 = preferred_formats[i];
				if (slave_info->formats & (1 << format1))
					return format1;
			}
		default:
			return -EINVAL;
		}
	}
}

struct {
	unsigned int rate;
	unsigned int flag;
} snd_pcm_rates[] = {
	{ 8000, SND_PCM_RATE_8000 },
	{ 11025, SND_PCM_RATE_11025 },
	{ 16000, SND_PCM_RATE_16000 },
	{ 22050, SND_PCM_RATE_22050 },
	{ 32000, SND_PCM_RATE_32000 },
	{ 44100, SND_PCM_RATE_44100 },
	{ 48000, SND_PCM_RATE_48000 },
	{ 88200, SND_PCM_RATE_88200 },
	{ 96000, SND_PCM_RATE_96000 },
	{ 176400, SND_PCM_RATE_176400 },
	{ 192000, SND_PCM_RATE_192000 }
};

static int snd_pcm_plug_slave_rate(unsigned int rate,
				   snd_pcm_params_info_t *slave_info)
{
        if (rate <= slave_info->min_rate)
		return slave_info->min_rate;
	else if (rate >= slave_info->max_rate)
		return slave_info->max_rate;
	else if (!(slave_info->rates & (SND_PCM_RATE_CONTINUOUS |
					SND_PCM_RATE_KNOT))) {
		unsigned int k;
		unsigned int rate1 = 0, rate2 = 0;
		int delta1, delta2;
		for (k = 0; k < sizeof(snd_pcm_rates) / 
			     sizeof(snd_pcm_rates[0]); ++k) {
			if (!(snd_pcm_rates[k].flag & slave_info->rates))
				continue;
			if (snd_pcm_rates[k].rate < rate) {
				rate1 = snd_pcm_rates[k].rate;
			} else if (snd_pcm_rates[k].rate >= rate) {
				rate2 = snd_pcm_rates[k].rate;
				break;
			}
		}
		if (rate1 == 0)
			return rate2;
		if (rate2 == 0)
			return rate1;
		delta1 = rate - rate1;
		delta2 = rate2 - rate;
		if (delta1 < delta2)
			return rate1;
		else
			return rate2;
	}
	return rate;
}

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

static int snd_pcm_plug_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	int err;
	
	if ((err = snd_pcm_info(slave, info)) < 0)
		return err;
	return 0;
}

static int snd_pcm_plug_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t *info)
{
	int err;
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	snd_pcm_params_info_t slave_info;
	int sformat, srate;
	unsigned int schannels;
	int crate;

	info->req.fail_reason = 0;
	info->req.fail_mask = 0;

	if (info->req_mask & SND_PCM_PARAMS_RATE) {
		info->min_rate = info->req.format.rate;
		info->max_rate = info->req.format.rate;
	} else {
		info->min_rate = 4000;
		info->max_rate = 192000;
	}
	info->rates = SND_PCM_RATE_CONTINUOUS | SND_PCM_RATE_8000_192000;

	if (info->req_mask & SND_PCM_PARAMS_CHANNELS) {
		info->min_channels = info->req.format.channels;
		info->max_channels = info->req.format.channels;
	} else {
		info->min_channels = 1;
		info->max_channels = 32;
	}

	memset(&slave_info, 0, sizeof(slave_info));
	if ((err = snd_pcm_params_info(slave, &slave_info)) < 0)
		return err;

	info->flags = slave_info.flags;
	info->flags |= SND_PCM_INFO_INTERLEAVED | SND_PCM_INFO_NONINTERLEAVED;

	info->min_fragments = slave_info.min_fragments;
	info->max_fragments = slave_info.max_fragments;
	
	if (info->req_mask & SND_PCM_PARAMS_SFMT) 
		info->formats = 1 << info->req.format.sfmt;
	else {
		info->formats = snd_pcm_plug_formats(slave_info.formats);
		return 0;
	}

	sformat = snd_pcm_plug_slave_fmt(info->req.format.sfmt, &slave_info);
	if (sformat < 0) {
		info->req.fail_mask = SND_PCM_PARAMS_SFMT;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}

	if (!(info->req_mask & SND_PCM_PARAMS_RATE))
		return 0;
	crate = info->req.format.rate;
	srate = snd_pcm_plug_slave_rate(crate, &slave_info);
	if (srate < 0) {
		info->req.fail_mask = SND_PCM_PARAMS_RATE;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}

	if (!(info->req_mask & SND_PCM_PARAMS_CHANNELS))
		return 0;
	schannels = info->req.format.channels;
	if (schannels < info->min_channels)
		schannels = info->min_channels;
	else if (schannels > info->max_channels)
		schannels = info->max_channels;

	slave_info.req_mask = (SND_PCM_PARAMS_SFMT |
			       SND_PCM_PARAMS_CHANNELS |
			       SND_PCM_PARAMS_RATE);
	slave_info.req.format.sfmt = sformat;
	slave_info.req.format.channels = schannels;
	slave_info.req.format.rate = srate;
	if ((err = snd_pcm_params_info(slave, &slave_info)) < 0) {
		info->req.fail_mask = slave_info.req.fail_mask;
		info->req.fail_reason = slave_info.req.fail_reason;
		return err;
	}
	info->buffer_size = muldiv64(slave_info.buffer_size, crate, srate);
	info->min_fragment_size = muldiv64(slave_info.min_fragment_size, crate, srate);
	info->max_fragment_size = muldiv64(slave_info.max_fragment_size, crate, srate);
	info->fragment_align = muldiv64(slave_info.fragment_align, crate, srate);
	if (sformat != info->req.format.sfmt ||
	    (unsigned int) srate != info->req.format.rate ||
	    schannels != info->req.format.channels)
		info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
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

static int snd_pcm_plug_change_rate(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_format_t *clt, snd_pcm_format_t *slv)
{
	snd_pcm_plug_t *plug = pcm->private;
	int err;
	assert(snd_pcm_format_linear(slv->sfmt));
	if (clt->rate == slv->rate)
		return 0;
	err = snd_pcm_rate_open(new, slv->sfmt, slv->rate, plug->slave, plug->slave != plug->req_slave);
	if (err < 0)
		return err;
	slv->rate = clt->rate;
	if (snd_pcm_format_linear(clt->sfmt))
		slv->sfmt = clt->sfmt;
	return 1;
}

static int snd_pcm_plug_change_channels(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_format_t *clt, snd_pcm_format_t *slv)
{
	snd_pcm_plug_t *plug = pcm->private;
	unsigned int tt_ssize, tt_cused, tt_sused;
	ttable_entry_t *ttable;
	int err;
	assert(snd_pcm_format_linear(slv->sfmt));
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
	err = snd_pcm_route_open(new, slv->sfmt, slv->channels, ttable, tt_ssize, tt_cused, tt_sused, plug->slave, plug->slave != plug->req_slave);
	if (err < 0)
		return err;
	slv->channels = clt->channels;
	if (snd_pcm_format_linear(clt->sfmt))
		slv->sfmt = clt->sfmt;
	return 1;
}

static int snd_pcm_plug_change_format(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_format_t *clt, snd_pcm_format_t *slv)
{
	snd_pcm_plug_t *plug = pcm->private;
	int err, cfmt;
	int (*f)(snd_pcm_t **handle, int sformat, snd_pcm_t *slave, int close_slave);
	if (snd_pcm_format_linear(slv->sfmt)) {
		/* Conversion is done in another plugin */
		if (clt->sfmt == slv->sfmt ||
		    clt->rate != slv->rate ||
		    clt->channels != slv->channels)
			return 0;
	} else {
		/* No conversion is needed */
		if (clt->sfmt == slv->sfmt &&
		    clt->rate == slv->rate &&
		    clt->channels == clt->channels)
			return 0;
	}
	if (snd_pcm_format_linear(slv->sfmt)) {
		cfmt = clt->sfmt;
		switch (clt->sfmt) {
		case SND_PCM_SFMT_MU_LAW:
			f = snd_pcm_mulaw_open;
			break;
		case SND_PCM_SFMT_A_LAW:
			f = snd_pcm_alaw_open;
			break;
		case SND_PCM_SFMT_IMA_ADPCM:
			f = snd_pcm_adpcm_open;
			break;
		default:
			assert(snd_pcm_format_linear(clt->sfmt));
			f = snd_pcm_linear_open;
			break;
		}
	} else {
		switch (slv->sfmt) {
		case SND_PCM_SFMT_MU_LAW:
			f = snd_pcm_mulaw_open;
			break;
		case SND_PCM_SFMT_A_LAW:
			f = snd_pcm_alaw_open;
			break;
		case SND_PCM_SFMT_IMA_ADPCM:
			f = snd_pcm_adpcm_open;
			break;
		default:
			assert(0);
			return -EINVAL;
		}
		if (snd_pcm_format_linear(clt->sfmt))
			cfmt = clt->sfmt;
		else
			cfmt = SND_PCM_SFMT_S16;
	}
	err = f(new, slv->sfmt, plug->slave, plug->slave != plug->req_slave);
	if (err < 0)
		return err;
	slv->sfmt = cfmt;
	return 1;
}

static int snd_pcm_plug_insert_plugins(snd_pcm_t *pcm,
				       snd_pcm_format_t *client_fmt,
				       snd_pcm_format_t *slave_fmt)
{
	snd_pcm_plug_t *plug = pcm->private;
	int (*funcs[])(snd_pcm_t *pcm, snd_pcm_t **new, snd_pcm_format_t *s, snd_pcm_format_t *d) = {
		snd_pcm_plug_change_format,
		snd_pcm_plug_change_channels,
		snd_pcm_plug_change_rate,
		snd_pcm_plug_change_channels,
		snd_pcm_plug_change_format
	};
	snd_pcm_format_t sfmt = *slave_fmt;
	unsigned int k = 0;
	while (1) {
		snd_pcm_t *new;
		int err;
		if (client_fmt->sfmt == sfmt.sfmt &&
		    client_fmt->channels == sfmt.channels &&
		    client_fmt->rate == sfmt.rate)
			return 0;
		assert(k < sizeof(funcs)/sizeof(*funcs));
		err = funcs[k](pcm, &new, client_fmt, &sfmt);
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
	assert(0);
	return 0;
}

static int snd_pcm_plug_params(snd_pcm_t *pcm, snd_pcm_params_t *params)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	snd_pcm_format_t *slave_format, *format;
	snd_pcm_params_info_t slave_info;
	int srate;
	int err;
	
	memset(&slave_info, 0, sizeof(slave_info));
	err = snd_pcm_params_info(slave, &slave_info);
	assert(err >= 0);
	if (err < 0)
		return err;

	slave_info.req = *params;
	format = &params->format;
	slave_format = &slave_info.req.format;

	if ((slave_info.formats & (1 << format->sfmt)) == 0) {
		int slave_fmt = snd_pcm_plug_slave_fmt(format->sfmt, &slave_info);
		if (slave_fmt < 0) {
			params->fail_mask = SND_PCM_PARAMS_SFMT;
			params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
			return slave_fmt;
		}
		slave_format->sfmt = slave_fmt;
	}
	slave_info.req_mask |= SND_PCM_PARAMS_SFMT;

	if (slave_info.formats != 1U << slave_format->sfmt) {
		err = snd_pcm_params_info(slave, &slave_info);
		assert(err >= 0);
		if (err < 0)
			return err;
	}

      	if (format->channels < slave_info.min_channels)
		slave_format->channels = slave_info.min_channels;
	else if (format->channels > slave_info.max_channels)
		slave_format->channels = slave_info.max_channels;
	slave_info.req_mask |= SND_PCM_PARAMS_CHANNELS;
	err = snd_pcm_params_info(slave, &slave_info);
	assert(err >= 0);
	if (err < 0)
		return err;


	srate = snd_pcm_plug_slave_rate(format->rate, &slave_info);
	if (srate < 0) {
		params->fail_mask = SND_PCM_PARAMS_RATE;
		params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return srate;
	}
	slave_format->rate = srate;
	slave_info.req_mask |= SND_PCM_PARAMS_RATE;
	err = snd_pcm_params_info(slave, &slave_info);
	assert(err >= 0);
	if (err < 0)
		return err;

	if (slave_format->rate - slave_info.min_rate < slave_info.max_rate - slave_format->rate)
		slave_format->rate = slave_info.min_rate;
	else
		slave_format->rate = slave_info.max_rate;

	err = snd_pcm_plug_insert_plugins(pcm, format, slave_format);
	if (err < 0)
		return err;

	err = snd_pcm_params(plug->slave, params);
	if (err < 0)
		snd_pcm_plug_clear(pcm);

	assert(plug->req_slave->setup.format.sfmt == slave_format->sfmt);
	assert(plug->req_slave->setup.format.channels == slave_format->channels);
	assert(plug->req_slave->setup.format.rate == slave_format->rate);

	return err;
}

static int snd_pcm_plug_setup(snd_pcm_t *pcm, snd_pcm_setup_t *setup)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_setup(plug->slave, setup);
}

static int snd_pcm_plug_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_channel_info(plug->slave, info);
}

static int snd_pcm_plug_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_channel_params(plug->slave, params);
}

static int snd_pcm_plug_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_channel_setup(plug->slave, setup);
}

static int snd_pcm_plug_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_mmap_status(plug->slave, NULL);
}

static int snd_pcm_plug_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_mmap_control(plug->slave, NULL);
}

static int snd_pcm_plug_mmap_data(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_mmap_data(plug->slave, NULL);
}

static int snd_pcm_plug_munmap_status(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_munmap_status(plug->slave);
}
		
static int snd_pcm_plug_munmap_control(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_munmap_control(plug->slave);
}
		
static int snd_pcm_plug_munmap_data(snd_pcm_t *pcm)
{
	snd_pcm_plug_t *plug = pcm->private;
	return snd_pcm_munmap_data(plug->slave);
}
		
static void snd_pcm_plug_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_plug_t *plug = pcm->private;
	fprintf(fp, "Plug PCM: ");
	snd_pcm_dump(plug->slave, fp);
}

struct snd_pcm_ops snd_pcm_plug_ops = {
	close: snd_pcm_plug_close,
	info: snd_pcm_plug_info,
	params_info: snd_pcm_plug_params_info,
	params: snd_pcm_plug_params,
	setup: snd_pcm_plug_setup,
	channel_info: snd_pcm_plug_channel_info,
	channel_params: snd_pcm_plug_channel_params,
	channel_setup: snd_pcm_plug_channel_setup,
	dump: snd_pcm_plug_dump,
	nonblock: snd_pcm_plug_nonblock,
	mmap_status: snd_pcm_plug_mmap_status,
	mmap_control: snd_pcm_plug_mmap_control,
	mmap_data: snd_pcm_plug_mmap_data,
	munmap_status: snd_pcm_plug_munmap_status,
	munmap_control: snd_pcm_plug_munmap_control,
	munmap_data: snd_pcm_plug_munmap_data,
};

int snd_pcm_plug_open(snd_pcm_t **handlep,
		      ttable_entry_t *ttable,
		      unsigned int tt_ssize,
		      unsigned int tt_cused, unsigned int tt_sused,
		      snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *handle;
	snd_pcm_plug_t *plug;
	int err;
	assert(handlep && slave);
	plug = calloc(1, sizeof(snd_pcm_plug_t));
	if (!plug)
		return -ENOMEM;
	plug->slave = plug->req_slave = slave;
	plug->close_slave = close_slave;
	plug->ttable = ttable;
	plug->tt_ssize = tt_ssize;
	plug->tt_cused = tt_cused;
	plug->tt_sused = tt_sused;
	
	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		free(plug);
		return -ENOMEM;
	}
	handle->type = SND_PCM_TYPE_PLUG;
	handle->stream = slave->stream;
	handle->ops = &snd_pcm_plug_ops;
	handle->op_arg = handle;
	handle->fast_ops = slave->fast_ops;
	handle->fast_op_arg = slave->fast_op_arg;
	handle->mode = slave->mode;
	handle->private = plug;
	err = snd_pcm_init(handle);
	if (err < 0) {
		snd_pcm_close(handle);
		return err;
	}
	*handlep = handle;

	return 0;
}

int snd_pcm_plug_open_subdevice(snd_pcm_t **handlep, int card, int device, int subdevice, int stream, int mode)
{
	snd_pcm_t *slave;
	int err;
	err = snd_pcm_hw_open_subdevice(&slave, card, device, subdevice, stream, mode);
	if (err < 0)
		return err;
	return snd_pcm_plug_open(handlep, 0, 0, 0, 0, slave, 1);
}

int snd_pcm_plug_open_card(snd_pcm_t **handlep, int card, int device, int stream, int mode)
{
	return snd_pcm_plug_open_subdevice(handlep, card, device, -1, stream, mode);
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
	if (!sname)
		return -EINVAL;
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
	err = snd_pcm_plug_open(pcmp, ttable, MAX_CHANNELS, cused, sused, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				
