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


static int preferred_formats[] = {
	SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_S16_BE,
	SND_PCM_FORMAT_U16_LE,
	SND_PCM_FORMAT_U16_BE,
	SND_PCM_FORMAT_S24_LE,
	SND_PCM_FORMAT_S24_BE,
	SND_PCM_FORMAT_U24_LE,
	SND_PCM_FORMAT_U24_BE,
	SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_S32_BE,
	SND_PCM_FORMAT_U32_LE,
	SND_PCM_FORMAT_U32_BE,
	SND_PCM_FORMAT_S8,
	SND_PCM_FORMAT_U8
};

static int snd_pcm_plug_slave_fmt(int format, unsigned int format_mask)
{
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
					    format_mask & (1U << format1))
						return format1;
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
		return ffs(format_mask) - 1;
	} else {
		unsigned int i;
		switch (format) {
		case SND_PCM_FORMAT_MU_LAW:
		case SND_PCM_FORMAT_A_LAW:
		case SND_PCM_FORMAT_IMA_ADPCM:
			for (i = 0; i < sizeof(preferred_formats) / sizeof(preferred_formats[0]); ++i) {
				int format1 = preferred_formats[i];
				if (format_mask & (1U << format1))
					return format1;
			}
		default:
			return ffs(format_mask) - 1;
		}
	}
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

static int snd_pcm_plug_hw_info(snd_pcm_t *pcm, snd_pcm_hw_info_t *info)
{
	int err;
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	snd_pcm_hw_info_t sinfo, i;
	snd_pcm_hw_params_t sparams;
	unsigned int rate_min, rate_max;
	unsigned int channels_min, channels_max;
	unsigned int format, format_mask;
	size_t fragment_size_min, fragment_size_max;
	
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

	if (info->rate_min < 4000)
		info->rate_min = 4000;
	if (info->rate_max > 192000)
		info->rate_max = 192000;
	if (info->rate_max < info->rate_min)
		return -EINVAL;

	if (info->channels_min < 1)
		info->channels_min = 1;
	if (info->channels_max > 1024)
		info->channels_max = 1024;
	if (info->channels_max < info->channels_min)
		return -EINVAL;

	if (info->fragment_size_max > 1024 * 1024)
		info->fragment_size_max = 1024 * 1024;
	if (info->fragment_size_max < info->fragment_size_min)
		return -EINVAL;

	sinfo.access_mask = SND_PCM_ACCBIT_MMAP;
	sinfo.format_mask = (SND_PCM_FMTBIT_LINEAR | SND_PCM_FMTBIT_MU_LAW |
			     SND_PCM_FMTBIT_A_LAW | SND_PCM_FMTBIT_IMA_ADPCM);
	sinfo.subformat_mask = SND_PCM_SUBFMTBIT_STD;
	sinfo.channels_min = 1;
	sinfo.channels_max = 1024;
	sinfo.rate_min = 4000;
	sinfo.rate_max = 192000;
	sinfo.fragments_min = 1;
	sinfo.fragments_max = UINT_MAX;
	sinfo.fragment_size_min = 1;
	sinfo.fragment_size_max = ULONG_MAX;

	/* Formats */
	err = snd_pcm_hw_info(slave, &sinfo);
	if (err < 0) {
		*info = i;
		return err;
	}
	format_mask = 0;
	for (format = 0; format < SND_PCM_FORMAT_LAST; ++format) {
		if (!(info->format_mask & (1 << format)))
			continue;
		err = snd_pcm_plug_slave_fmt(format, sinfo.format_mask);
		if (err < 0)
			info->format_mask &= ~(1 << format);
		else
			format_mask |= (1 << err);
	}
	sinfo.format_mask = format_mask;

	/* Rate (and fragment_size) */
	i = sinfo;
	sparams.rate = info->rate_min;
	err = snd_pcm_hw_info_rulesv(slave, &i, &sparams,
				    SND_PCM_RULE_REL_NEAR | SND_PCM_HW_PARAM_RATE,
				    -1);
	if (err < 0) {
		*info = i;
		return err;
	}
	rate_min = i.rate_min;
	if (info->rate_max != info->rate_min) {
		i = sinfo;
		sparams.rate = info->rate_max;
		err = snd_pcm_hw_info_rulesv(slave, &i, &sparams,
					     SND_PCM_RULE_REL_NEAR | SND_PCM_HW_PARAM_RATE,
					     -1);
		if (err < 0) {
			*info = i;
			return err;
		}
		rate_max = i.rate_min;
	} else
		rate_max = rate_min;
	sinfo.rate_min = rate_min;
	sinfo.rate_max = rate_max;

	/* Channels */
	i = sinfo;
	sparams.channels = info->channels_min;
	err = snd_pcm_hw_info_rulesv(slave, &i, &sparams,
				     SND_PCM_RULE_REL_NEAR | SND_PCM_HW_PARAM_CHANNELS,
				     -1);
	if (err < 0) {
		*info = i;
		return err;
	}
	channels_min = i.channels_min;
	if (info->channels_max != info->channels_min) {
		i = sinfo;
		sparams.channels = info->channels_max;
		err = snd_pcm_hw_info_rulesv(slave, &i, &sparams,
					     SND_PCM_RULE_REL_NEAR | SND_PCM_HW_PARAM_CHANNELS,
					     -1);
		if (err < 0) {
			*info = i;
			return err;
		}
		channels_max = i.channels_min;
	} else
		channels_max = channels_min;
	sinfo.channels_min = channels_min;
	sinfo.channels_max = channels_max;

	sinfo.fragments_min = info->fragments_min;
	sinfo.fragments_max = info->fragments_max;
	sinfo.fragment_size_min = muldiv_down(info->fragment_size_min, sinfo.rate_min, info->rate_max);
	sinfo.fragment_size_max = muldiv_up(info->fragment_size_max, sinfo.rate_max, info->rate_min);
	err = snd_pcm_hw_info(slave, &sinfo);
	if (err < 0) {
		*info = sinfo;
		return err;
	}

	info->subformat_mask = sinfo.subformat_mask;
	info->fragments_min = sinfo.fragments_min;
	info->fragments_max = sinfo.fragments_max;

	fragment_size_min = muldiv_down(sinfo.fragment_size_min, info->rate_min, sinfo.rate_max);
	fragment_size_max = muldiv_up(sinfo.fragment_size_max, info->rate_max, sinfo.rate_min);
	if (fragment_size_min > info->fragment_size_min)
		info->fragment_size_min = fragment_size_min;
	if (fragment_size_max < info->fragment_size_max)
		info->fragment_size_max = fragment_size_max;
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
	} else {
		/* No conversion is needed */
		if (clt->format == slv->format &&
		    clt->rate == slv->rate &&
		    clt->channels == clt->channels)
			return 0;
	}
	if (snd_pcm_format_linear(slv->format)) {
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
		snd_pcm_plug_change_format
	};
	snd_pcm_hw_params_t p = *slave;
	unsigned int k = 0;
	while (1) {
		snd_pcm_t *new;
		int err;
		if (client->format == p.format &&
		    client->channels == p.channels &&
		    client->rate == p.rate)
			return 0;
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
	assert(0);
	return 0;
}

static int snd_pcm_plug_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_plug_t *plug = pcm->private;
	snd_pcm_t *slave = plug->req_slave;
	snd_pcm_hw_info_t sinfo;
	snd_pcm_hw_params_t sparams;
	int err;
	sparams = *params;
	snd_pcm_hw_params_to_info(&sparams, &sinfo);
	sinfo.access_mask = SND_PCM_ACCBIT_MMAP;
	sinfo.format_mask = (SND_PCM_FMTBIT_LINEAR | SND_PCM_FMTBIT_MU_LAW |
			     SND_PCM_FMTBIT_A_LAW | SND_PCM_FMTBIT_IMA_ADPCM);
	sinfo.subformat_mask = SND_PCM_SUBFMTBIT_STD;
	sinfo.channels_min = 1;
	sinfo.channels_max = 1024;
	sinfo.rate_min = 4000;
	sinfo.rate_max = 192000;
	sinfo.fragment_size_min = 1;
	sinfo.fragment_size_max = ULONG_MAX;
	err = snd_pcm_hw_info_rulesv(slave, &sinfo, params,
				    SND_PCM_RULE_REL_NEAR | SND_PCM_HW_PARAM_RATE,
				    SND_PCM_RULE_REL_NEAR | SND_PCM_HW_PARAM_CHANNELS,
				     -1);
	if (err < 0) {
		snd_pcm_hw_info_to_params_fail(&sinfo, params);
		return err;
	}
	err = snd_pcm_plug_slave_fmt(sparams.format, sinfo.format_mask);
	if (err < 0)
		return err;
	sparams.format = err;
	sinfo.format_mask = 1U << err;
	sparams.fragment_size = muldiv_near(params->fragment_size, sparams.rate, params->rate);
	err = snd_pcm_hw_info_rulesv(slave, &sinfo, &sparams,
				     SND_PCM_RULE_REL_NEAR | SND_PCM_HW_PARAM_FRAGMENT_SIZE,
				     -1);
	if (err < 0) {
		snd_pcm_hw_info_to_params_fail(&sinfo, params);
		return err;
	}
	if (sinfo.access_mask & SND_PCM_ACCBIT_MMAP_INTERLEAVED)
		sparams.access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
	else if (sinfo.access_mask & SND_PCM_ACCBIT_MMAP_NONINTERLEAVED)
		sparams.access = SND_PCM_ACCESS_MMAP_NONINTERLEAVED;
	else
		assert(0);

	err = snd_pcm_plug_insert_plugins(pcm, params, &sparams);
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

static int snd_pcm_plug_mmap(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plug = pcm->private;
	return snd_pcm_mmap(plug->slave);
}

static int snd_pcm_plug_munmap(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plug = pcm->private;
	return snd_pcm_munmap(plug->slave);
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
	err = snd_pcm_plug_open(pcmp, name, ttable, MAX_CHANNELS, cused, sused, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				
