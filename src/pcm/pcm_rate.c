/*
 *  PCM - Rate conversion
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
  
#include <limits.h>
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

#define DIV (1<<16)

typedef struct {
	int16_t sample;
	int sum;
	unsigned int pos;
} rate_state_t;
 
typedef size_t (*rate_f)(snd_pcm_channel_area_t *src_areas,
			 size_t src_offset, size_t src_frames,
			 snd_pcm_channel_area_t *dst_areas,
			 size_t dst_offset, size_t *dst_framesp,
			 size_t channels,
			 int getidx, int putidx,
			 unsigned int arg,
			 rate_state_t *states);

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	int get_idx;
	int put_idx;
	unsigned int pitch;
	rate_f func;
	int req_sformat;
	int req_srate;
	int sformat;
	int cformat;
	int srate;
	int crate;
	int cxfer_mode, cmmap_shape;
	rate_state_t *states;
} snd_pcm_rate_t;

static size_t resample_expand(snd_pcm_channel_area_t *src_areas,
			      size_t src_offset, size_t src_frames,
			      snd_pcm_channel_area_t *dst_areas,
			      size_t dst_offset, size_t *dst_framesp,
			      size_t channels,
			      int getidx, int putidx,
			      unsigned int get_threshold,
			      rate_state_t *states)
{
#define GET_S16_LABELS
#define PUT_S16_LABELS
#include "plugin_ops.h"
#undef GET_S16_LABELS
#undef PUT_S16_LABELS
	void *get = get_s16_labels[getidx];
	void *put = put_s16_labels[putidx];
	unsigned int channel;
	size_t src_frames1 = 0;
	size_t dst_frames1 = 0;
	size_t dst_frames = *dst_framesp;
	int16_t sample = 0;
	
	if (src_frames == 0 ||
	    dst_frames == 0)
		return 0;
	for (channel = 0; channel < channels; ++channel) {
		snd_pcm_channel_area_t *src_area = &src_areas[channel];
		snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
		char *src, *dst;
		int src_step, dst_step;
		int16_t old_sample = states->sample;
		unsigned int pos = states->pos;
#if 0
		if (!src_area->enabled) {
			if (dst_area->wanted)
				snd_pcm_area_silence(&dst_area->area, 0, dst_frames, plugin->dst_format.sfmt);
			dst_area->enabled = 0;
			continue;
		}
		dst_area->enabled = 1;
#endif
		src = snd_pcm_channel_area_addr(src_area, src_offset);
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		src_step = snd_pcm_channel_area_step(src_area);
		dst_step = snd_pcm_channel_area_step(dst_area);
		src_frames1 = 0;
		dst_frames1 = 0;
		while (dst_frames1 < dst_frames) {
			if (pos >= get_threshold) {
				int16_t new_sample;
				if (src_frames1 == src_frames)
					break;
				pos -= get_threshold;
				goto *get;
#define GET_S16_END after_get
#include "plugin_ops.h"
#undef GET_S16_END
			after_get:
				src += src_step;
				src_frames1++;
				new_sample = sample;
				sample = (old_sample * (DIV - pos) + new_sample * pos) / DIV;
				old_sample = new_sample;
			} else
				sample = old_sample;
			goto *put;
#define PUT_S16_END after_put
#include "plugin_ops.h"
#undef PUT_S16_END
		after_put:
			dst += dst_step;
			dst_frames1++;
			pos += DIV;
		}
		states->sample = old_sample;
		states->pos = pos;
		states++;
	}
	*dst_framesp = dst_frames1;
	return src_frames1;
}

static size_t resample_shrink(snd_pcm_channel_area_t *src_areas,
			      size_t src_offset, size_t src_frames,
			      snd_pcm_channel_area_t *dst_areas,
			      size_t dst_offset, size_t *dst_framesp,
			      size_t channels,
			      int getidx, int putidx,
			      unsigned int get_increment,
			      rate_state_t *states)
{
#define GET_S16_LABELS
#define PUT_S16_LABELS
#include "plugin_ops.h"
#undef GET_S16_LABELS
#undef PUT_S16_LABELS
	void *get = get_s16_labels[getidx];
	void *put = put_s16_labels[putidx];
	unsigned int channel;
	size_t src_frames1 = 0;
	size_t dst_frames1 = 0;
	size_t dst_frames = *dst_framesp;
	int16_t sample = 0;

	if (src_frames == 0 ||
	    dst_frames == 0)
		return 0;
	for (channel = 0; channel < channels; ++channel) {
		snd_pcm_channel_area_t *src_area = &src_areas[channel];
		snd_pcm_channel_area_t *dst_area = &dst_areas[channel];
		unsigned int pos;
		int sum;
		char *src, *dst;
		int src_step, dst_step;
		sum = states->sum;
		pos = states->pos;
#if 0
		if (!src_area->enabled) {
			if (dst_area->wanted)
				snd_pcm_area_silence(&dst_area->area, 0, dst_frames, plugin->dst_format.sfmt);
			dst_area->enabled = 0;
			continue;
		}
		dst_area->enabled = 1;
#endif
		src = snd_pcm_channel_area_addr(src_area, src_offset);
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
		src_step = snd_pcm_channel_area_step(src_area);
		dst_step = snd_pcm_channel_area_step(dst_area);
		src_frames1 = 0;
		dst_frames1 = 0;
		while (src_frames1 < src_frames) {
			
			goto *get;
#define GET_S16_END after_get
#include "plugin_ops.h"
#undef GET_S16_END
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
#define PUT_S16_END after_put
#include "plugin_ops.h"
#undef PUT_S16_END
			after_put:
				dst += dst_step;
				sum = s * pos;
				dst_frames1++;
				if (dst_frames1 == dst_frames)
					break;
			} else
				sum += sample * get_increment;
		}
		states->sum = sum;
		states->pos = pos;
		states++;
	}
	*dst_framesp = dst_frames1;
	return src_frames1;
}

static int snd_pcm_rate_close(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private;
	int err = 0;
	if (rate->plug.close_slave)
		err = snd_pcm_close(rate->plug.slave);
	if (rate->states)
		free(rate->states);
	free(rate);
	return 0;
}

static int snd_pcm_rate_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t * info)
{
	snd_pcm_rate_t *rate = pcm->private;
	unsigned int req_mask = info->req_mask;
	unsigned int sfmt = info->req.format.sfmt;
	unsigned int crate = info->req.format.rate;
	unsigned int srate;
	int err;
	if (req_mask & SND_PCM_PARAMS_SFMT &&
	    !snd_pcm_format_linear(sfmt)) {
		info->req.fail_mask = SND_PCM_PARAMS_SFMT;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	if (rate->req_sformat >= 0) {
		info->req_mask |= SND_PCM_PARAMS_SFMT;
		info->req.format.sfmt = rate->req_sformat;
	}
	info->req_mask |= SND_PCM_PARAMS_RATE;
	info->req_mask &= ~(SND_PCM_PARAMS_MMAP_SHAPE | 
			    SND_PCM_PARAMS_XFER_MODE);
	info->req.format.rate = rate->req_srate;
	err = snd_pcm_params_info(rate->plug.slave, info);
	info->req_mask = req_mask;
	info->req.format.sfmt = sfmt;
	info->req.format.rate = crate;
	if (err < 0)
		return err;
	if (req_mask & SND_PCM_PARAMS_SFMT)
		info->formats = 1 << sfmt;
	else
		info->formats = SND_PCM_LINEAR_FORMATS;
	if (!(req_mask & SND_PCM_PARAMS_RATE)) {
		info->min_rate = 4000;
		info->max_rate = 192000;
		return 0;
	}
	if (rate->req_srate - info->min_rate < info->max_rate - rate->req_srate)
		srate = info->min_rate;
	else
		srate = info->max_rate;
	info->min_rate = crate;
	info->max_rate = crate;
	if (info->buffer_size)
		info->buffer_size = muldiv64(info->buffer_size, crate, srate);
	if (info->min_fragment_size)
		info->min_fragment_size = muldiv64(info->min_fragment_size, crate, srate);
	if (info->max_fragment_size)
		info->max_fragment_size = muldiv64(info->max_fragment_size, crate, srate);
	if (info->fragment_align)
		info->fragment_align = muldiv64(info->fragment_align, crate, srate);
	info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	info->flags |= SND_PCM_INFO_INTERLEAVED | SND_PCM_INFO_NONINTERLEAVED;
	return 0;
}

static int snd_pcm_rate_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_rate_t *rate = pcm->private;
	snd_pcm_t *slave = rate->plug.slave;
	snd_pcm_params_t slave_params;
	snd_pcm_params_info_t slave_info;
	int srate, crate;
	int err;
	if (!snd_pcm_format_linear(params->format.sfmt)) {
		params->fail_mask = SND_PCM_PARAMS_SFMT;
		params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	slave_params = *params;
	rate->cformat = params->format.sfmt;
	rate->crate = crate = params->format.rate;
	rate->cxfer_mode = params->xfer_mode;
	rate->cmmap_shape = params->mmap_shape;

	memset(&slave_info, 0, sizeof(slave_info));
	slave_info.req = *params;
	if (rate->req_sformat >= 0) {
		slave_info.req.format.sfmt = rate->req_sformat;
		slave_params.format.sfmt = rate->req_sformat;
	}
	slave_info.req.format.rate = rate->req_srate;
	slave_info.req_mask = ~0;
	err = snd_pcm_params_info(slave, &slave_info);
	if (err < 0) {
		params->fail_mask = slave_info.req.fail_mask;
		params->fail_reason = slave_info.req.fail_reason;
		return err;
	}

	if (rate->req_srate - slave_info.min_rate < slave_info.max_rate - rate->req_srate)
		srate = slave_info.min_rate;
	else
		srate = slave_info.max_rate;

	slave_params.format.rate = srate;
	slave_params.avail_min = muldiv64(params->avail_min, srate, crate);
	slave_params.xfer_min = muldiv64(params->xfer_min, srate, crate);
	slave_params.buffer_size = muldiv64(params->buffer_size, srate, crate);
	slave_params.frag_size = muldiv64(params->frag_size, srate, crate);
	slave_params.xfer_align = muldiv64(params->xfer_align, srate, crate);
	/* FIXME: boundary? */
	slave_params.xfer_mode = SND_PCM_XFER_UNSPECIFIED;
	slave_params.mmap_shape = SND_PCM_MMAP_UNSPECIFIED;;
	err = snd_pcm_params_mmap(slave, &slave_params);
	params->fail_mask = slave_params.fail_mask;
	params->fail_reason = slave_params.fail_reason;
	return err;
}

static int snd_pcm_rate_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_rate_t *rate = pcm->private;
	int src_format, dst_format;
	int src_rate, dst_rate;
	int mul, div;
	int err = snd_pcm_setup(rate->plug.slave, setup);
	if (err < 0)
		return err;
	if (rate->req_sformat >= 0)
		assert(rate->req_sformat == setup->format.sfmt);
	rate->sformat = setup->format.sfmt;
	rate->srate = setup->format.rate;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		src_format = rate->cformat;
		dst_format = rate->sformat;
		src_rate = rate->crate;
		dst_rate = rate->srate;
	} else {
		src_format = rate->sformat;
		dst_format = rate->cformat;
		src_rate = rate->srate;
		dst_rate = rate->crate;
	}
	rate->get_idx = getput_index(src_format);
	rate->put_idx = getput_index(dst_format);
	if (src_rate < dst_rate) {
		rate->func = resample_expand;
		/* pitch is get_threshold */
	} else {
		rate->func = resample_shrink;
		/* pitch is get_increment */
	}
	rate->pitch = (((u_int64_t)dst_rate * DIV) + src_rate / 2) / src_rate;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		mul = DIV;
		div = rate->pitch;
	} else {
		mul = rate->pitch;
		div = DIV;
	}
	rate->crate = muldiv64(rate->srate, mul, div);
	if (rate->cxfer_mode == SND_PCM_XFER_UNSPECIFIED)
		setup->xfer_mode = SND_PCM_XFER_NONINTERLEAVED;
	else
		setup->xfer_mode = rate->cxfer_mode;
	if (rate->cmmap_shape == SND_PCM_MMAP_UNSPECIFIED)
		setup->mmap_shape = SND_PCM_MMAP_NONINTERLEAVED;
	else
		setup->mmap_shape = rate->cmmap_shape;
	setup->format.sfmt = rate->cformat;
	setup->format.rate = rate->crate;
	/* FIXME */
	setup->rate_master = rate->crate;
	setup->rate_divisor = 1;
	setup->mmap_bytes = 0;
	setup->avail_min = muldiv64(setup->avail_min, mul, div);
	setup->xfer_min = muldiv64(setup->xfer_min, mul, div);

	/* FIXME: the three above are not a lot sensible */
	setup->buffer_size = muldiv64(setup->buffer_size, mul, div);
	setup->frag_size = muldiv64(setup->frag_size, mul, div);
	setup->xfer_align = muldiv64(setup->xfer_align, mul, div);

	/* FIXME */
	setup->boundary = LONG_MAX - LONG_MAX % setup->buffer_size;

	if (rate->states)
		free(rate->states);
	rate->states = malloc(setup->format.channels * sizeof(*rate->states));
	return 0;
}

static int snd_pcm_rate_init(snd_pcm_t *pcm)
{
	snd_pcm_rate_t *rate = pcm->private;
	unsigned int k;
	for (k = 0; k < pcm->setup.format.channels; ++k) {
		rate->states[k].sum = 0;
		rate->states[k].sample = 0;
		if (rate->func == resample_expand) {
			/* Get a sample on entry */
			rate->states[k].pos = rate->pitch + DIV;
		} else {
			rate->states[k].pos = 0;
		}
	}
	return 0;
}

static ssize_t snd_pcm_rate_write_areas(snd_pcm_t *pcm,
					snd_pcm_channel_area_t *areas,
					size_t client_offset,
					size_t client_size,
					size_t *slave_sizep)
{
	snd_pcm_rate_t *rate = pcm->private;
	snd_pcm_t *slave = rate->plug.slave;
	size_t client_xfer = 0;
	size_t slave_xfer = 0;
	ssize_t err = 0;
	size_t slave_size;
	if (slave_sizep)
		slave_size = *slave_sizep;
	else
		slave_size = INT_MAX;
	assert(client_size > 0 && slave_size > 0);
	while (client_xfer < client_size &&
	       slave_xfer < slave_size) {
		size_t src_frames, dst_frames;
		src_frames = client_size - client_xfer;
		dst_frames = snd_pcm_mmap_playback_xfer(slave, slave_size - slave_xfer);
		src_frames = rate->func(areas, client_offset, src_frames,
					snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
					&dst_frames, 
					pcm->setup.format.channels,
					rate->get_idx, rate->put_idx,
					rate->pitch, rate->states);
		err = snd_pcm_mmap_forward(slave, dst_frames);
		if (err < 0)
			break;
		assert((size_t)err == dst_frames);
		client_offset += src_frames;
		client_xfer += src_frames;
		snd_pcm_mmap_hw_forward(pcm, src_frames);
		slave_xfer += dst_frames;
	}
	if (client_xfer > 0 || slave_xfer > 0) {
		if (slave_sizep)
			*slave_sizep = slave_xfer;
		return client_xfer;
	}
	return err;
}

static ssize_t snd_pcm_rate_read_areas(snd_pcm_t *pcm,
				       snd_pcm_channel_area_t *areas,
				       size_t client_offset,
				       size_t client_size,
				       size_t *slave_sizep)

{
	snd_pcm_rate_t *rate = pcm->private;
	snd_pcm_t *slave = rate->plug.slave;
	size_t client_xfer = 0;
	size_t slave_xfer = 0;
	ssize_t err = 0;
	size_t slave_size;
	if (slave_sizep)
		slave_size = *slave_sizep;
	else
		slave_size = INT_MAX;
	assert(client_size > 0 && slave_size > 0);
	while (client_xfer < client_size &&
	       slave_xfer < slave_size) {
		size_t src_frames, dst_frames;
		dst_frames = client_size - client_xfer;
		src_frames = snd_pcm_mmap_capture_xfer(slave, slave_size - slave_xfer);
		src_frames = rate->func(snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
					src_frames,
					areas, client_offset, &dst_frames,
					pcm->setup.format.channels,
					rate->get_idx, rate->put_idx,
					rate->pitch, rate->states);
		err = snd_pcm_mmap_forward(slave, src_frames);
		if (err < 0)
			break;
		assert((size_t)err == src_frames);
		client_offset += dst_frames;
		client_xfer += dst_frames;
		snd_pcm_mmap_hw_forward(pcm, dst_frames);
		slave_xfer += src_frames;
	}
	if (client_xfer > 0 || slave_xfer > 0) {
		if (slave_sizep)
			*slave_sizep = slave_xfer;
		return client_xfer;
	}
	return err;
}

size_t snd_pcm_rate_client_frames(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_rate_t *rate = pcm->private;
	/* Round toward zero */
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return (int64_t)frames * DIV / rate->pitch;
	else
		return (int64_t)frames * rate->pitch / DIV;
}

static void snd_pcm_rate_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_rate_t *rate = pcm->private;
	if (rate->req_sformat < 0)
		fprintf(fp, "Rate conversion PCM (%d)\n", 
			rate->req_srate);
	else
		fprintf(fp, "Rate conversion PCM (%d, sformat=%s)\n", 
			rate->req_srate,
			snd_pcm_format_name(rate->req_sformat));
	if (pcm->valid_setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(rate->plug.slave, fp);
}

snd_pcm_ops_t snd_pcm_rate_ops = {
	close: snd_pcm_rate_close,
	info: snd_pcm_plugin_info,
	params_info: snd_pcm_rate_params_info,
	params: snd_pcm_rate_params,
	setup: snd_pcm_rate_setup,
	channel_info: snd_pcm_plugin_channel_info,
	channel_params: snd_pcm_plugin_channel_params,
	channel_setup: snd_pcm_plugin_channel_setup,
	dump: snd_pcm_rate_dump,
	nonblock: snd_pcm_plugin_nonblock,
	async: snd_pcm_plugin_async,
	mmap: snd_pcm_plugin_mmap,
	munmap: snd_pcm_plugin_munmap,
};

int snd_pcm_rate_open(snd_pcm_t **pcmp, char *name, int sformat, int srate, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_rate_t *rate;
	assert(pcmp && slave);
	if (sformat >= 0 && snd_pcm_format_linear(sformat) != 1)
		return -EINVAL;
	rate = calloc(1, sizeof(snd_pcm_rate_t));
	if (!rate) {
		return -ENOMEM;
	}
	rate->req_srate = srate;
	rate->req_sformat = sformat;
	rate->plug.read = snd_pcm_rate_read_areas;
	rate->plug.write = snd_pcm_rate_write_areas;
	rate->plug.client_frames = snd_pcm_rate_client_frames;
	rate->plug.init = snd_pcm_rate_init;
	rate->plug.slave = slave;
	rate->plug.close_slave = close_slave;

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(rate);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_RATE;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_rate_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = rate;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = &rate->plug.hw_ptr;
	pcm->appl_ptr = &rate->plug.appl_ptr;
	*pcmp = pcm;

	return 0;
}

int _snd_pcm_rate_open(snd_pcm_t **pcmp, char *name,
			 snd_config_t *conf, 
			 int stream, int mode)
{
	snd_config_iterator_t i;
	char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	int sformat = -1;
	long srate = -1;
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
		if (strcmp(n->id, "srate") == 0) {
			err = snd_config_integer_get(n, &srate);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		return -EINVAL;
	}
	if (!sname || !srate)
		return -EINVAL;
	/* This is needed cause snd_config_update may destroy config */
	sname = strdup(sname);
	if (!sname)
		return  -ENOMEM;
	err = snd_pcm_open(&spcm, sname, stream, mode);
	free(sname);
	if (err < 0)
		return err;
	err = snd_pcm_rate_open(pcmp, name, sformat, srate, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
				

