/**
 * \file pcm/pcm_ioplug.c
 * \ingroup Plugin_SDK
 * \brief I/O Plugin SDK
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 2005
 */
/*
 *  PCM - External I/O Plugin SDK
 *  Copyright (c) 2005 by Takashi Iwai <tiwai@suse.de>
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
  
#include "pcm_local.h"
#include "pcm_ioplug.h"

/* hw_params */
struct ioplug_parm {
	unsigned int min, max;
	unsigned int num_list;
	unsigned int *list;
	unsigned int active: 1;
	unsigned int integer: 1;
};

typedef struct snd_pcm_ioplug_priv {
	snd_pcm_ioplug_t *data;
	struct ioplug_parm params[SND_PCM_IOPLUG_HW_PARAMS];
	unsigned int last_hw;
	snd_pcm_uframes_t avail_max;
	snd_htimestamp_t trigger_tstamp;
} ioplug_priv_t;

/* update the hw pointer */
static void snd_pcm_ioplug_hw_ptr_update(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;
	snd_pcm_sframes_t hw;

	hw = io->data->callback->pointer(io->data);
	if (hw >= 0) {
		unsigned int delta;
		if ((unsigned int)hw >= io->last_hw)
			delta = hw - io->last_hw;
		else
			delta = pcm->buffer_size + hw - io->last_hw;
		io->data->hw_ptr += delta;
		io->last_hw = hw;
	} else
		io->data->state = SNDRV_PCM_STATE_XRUN;
}

static int snd_pcm_ioplug_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	memset(info, 0, sizeof(*info));
	info->stream = pcm->stream;
	info->card = -1;
	if (pcm->name) {
		strncpy(info->id, pcm->name, sizeof(info->id));
		strncpy(info->name, pcm->name, sizeof(info->name));
		strncpy(info->subname, pcm->name, sizeof(info->subname));
	}
	info->subdevices_count = 1;
	return 0;
}

static int snd_pcm_ioplug_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	return snd_pcm_channel_info_shm(pcm, info, -1);
}

static int snd_pcm_ioplug_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	ioplug_priv_t *io = pcm->private_data;

	memset(status, 0, sizeof(*status));
	snd_pcm_ioplug_hw_ptr_update(pcm);
	status->state = io->data->state;
	status->trigger_tstamp = io->trigger_tstamp;
	status->avail = snd_pcm_mmap_avail(pcm);
	status->avail_max = io->avail_max;
	return 0;
}

static snd_pcm_state_t snd_pcm_ioplug_state(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;
	return io->data->state;
}

static int snd_pcm_ioplug_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_ioplug_hw_ptr_update(pcm);
	return 0;
}

static int snd_pcm_ioplug_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_ioplug_hw_ptr_update(pcm);
	*delayp = snd_pcm_mmap_hw_avail(pcm); 
	return 0;
}

static int snd_pcm_ioplug_reset(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;

	io->data->appl_ptr = 0;
	io->data->hw_ptr = 0;
	io->last_hw = 0;
	io->avail_max = 0;
	return 0;
}

static int snd_pcm_ioplug_prepare(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;

	io->data->state = SND_PCM_STATE_PREPARED;
	snd_pcm_ioplug_reset(pcm);
	if (io->data->callback->prepare)
		return io->data->callback->prepare(io->data);
	return 0;
}

static inline snd_mask_t *hw_param_mask(snd_pcm_hw_params_t *params,
					snd_pcm_hw_param_t var)
{
	return &params->masks[var - SND_PCM_HW_PARAM_FIRST_MASK];
}

static inline snd_interval_t *hw_param_interval(snd_pcm_hw_params_t *params,
						snd_pcm_hw_param_t var)
{
	return &params->intervals[var - SND_PCM_HW_PARAM_FIRST_INTERVAL];
}

static int hw_params_type[SND_PCM_IOPLUG_HW_PARAMS] = {
	[SND_PCM_IOPLUG_HW_ACCESS] = SND_PCM_HW_PARAM_ACCESS,
	[SND_PCM_IOPLUG_HW_FORMAT] = SND_PCM_HW_PARAM_FORMAT,
	[SND_PCM_IOPLUG_HW_CHANNELS] = SND_PCM_HW_PARAM_CHANNELS,
	[SND_PCM_IOPLUG_HW_RATE] = SND_PCM_HW_PARAM_RATE,
	[SND_PCM_IOPLUG_HW_PERIOD_BYTES] = SND_PCM_HW_PARAM_PERIOD_BYTES,
	[SND_PCM_IOPLUG_HW_BUFFER_BYTES] = SND_PCM_HW_PARAM_BUFFER_BYTES,
	[SND_PCM_IOPLUG_HW_PERIODS] = SND_PCM_HW_PARAM_PERIODS,
};

static int ioplug_mask_refine(snd_mask_t *mask, struct ioplug_parm *parm)
{
	snd_mask_t bits;
	unsigned int i;

	memset(&bits, 0, sizeof(bits));
	for (i = 0; i < parm->num_list; i++)
		bits.bits[parm->list[i] / 32] |= 1U << (parm->list[i] % 32);
	return snd_mask_refine(mask, &bits);
}

static int snd_interval_list(snd_interval_t *ival, int num_list, unsigned int *list)
{
	int imin, imax;
	int changed = 0;

	if (snd_interval_empty(ival))
		return -ENOENT;
	for (imin = 0; imin < num_list; imin++) {
		if (ival->min == list[imin] && ! ival->openmin)
			break;
		if (ival->min <= list[imin]) {
			ival->min = list[imin];
			ival->openmin = 0;
			changed = 1;
			break;
		}
	}
	if (imin >= num_list)
		return -EINVAL;
	for (imax = num_list - 1; imax >= imin; imax--) {
		if (ival->max == list[imax] && ! ival->openmax)
			break;
		if (ival->max >= list[imax]) {
			ival->max = list[imax];
			ival->openmax = 0;
			changed = 1;
			break;
		}
	}
	if (imax < imin)
		return -EINVAL;
	return changed;
}

static int ioplug_interval_refine(ioplug_priv_t *io, snd_pcm_hw_params_t *params, int type)
{
	struct ioplug_parm *parm = &io->params[type];
	snd_interval_t *ival;

	if (! parm->active)
		return 0;
	ival = hw_param_interval(params, hw_params_type[type]);
	ival->integer |= parm->integer;
	if (parm->num_list) {
		return snd_interval_list(ival, parm->num_list, parm->list);
	} else if (parm->min || parm->max) {
		snd_interval_t t;
		memset(&t, 0, sizeof(t));
		snd_interval_set_minmax(&t, parm->min, parm->max);
		t.integer = ival->integer;
		return snd_interval_refine(ival, &t);
	}
	return 0;
}

/* x = a * b */
static int rule_mul(snd_pcm_hw_params_t *params, int x, int a, int b)
{
	snd_interval_t t;

	snd_interval_mul(hw_param_interval(params, a),
			 hw_param_interval(params, b), &t);
	return snd_interval_refine(hw_param_interval(params, x), &t);
}

/* x = a / b */
static int rule_div(snd_pcm_hw_params_t *params, int x, int a, int b)
{
	snd_interval_t t;

	snd_interval_div(hw_param_interval(params, a),
			 hw_param_interval(params, b), &t);
	return snd_interval_refine(hw_param_interval(params, x), &t);
}

/* x = a * b / k */
static int rule_muldivk(snd_pcm_hw_params_t *params, int x, int a, int b, int k)
{
	snd_interval_t t;

	snd_interval_muldivk(hw_param_interval(params, a),
			     hw_param_interval(params, b), k, &t);
	return snd_interval_refine(hw_param_interval(params, x), &t);
}

/* x = a * k / b */
static int rule_mulkdiv(snd_pcm_hw_params_t *params, int x, int a, int k, int b)
{
	snd_interval_t t;

	snd_interval_mulkdiv(hw_param_interval(params, a), k,
			     hw_param_interval(params, b), &t);
	return snd_interval_refine(hw_param_interval(params, x), &t);
}

#if 0
static void dump_parm(snd_pcm_hw_params_t *params)
{
	snd_output_t *log;
	snd_output_stdio_attach(&log, stderr, 0);
	snd_pcm_hw_params_dump(params, log);
	snd_output_close(log);
}
#endif

/* refine *_TIME and *_SIZE, then update *_BYTES */
static int refine_time_and_size(snd_pcm_hw_params_t *params,
				int time, int size, int bytes)
{
	int err, change1 = 0;

	/* size = time * rate / 1000000 */
	err = rule_muldivk(params, size, time,
			   SND_PCM_HW_PARAM_RATE, 1000000);
	if (err < 0)
		return err;
	change1 |= err;

	/* bytes = size * framebits / 8 */
	err = rule_muldivk(params, bytes, size,
			   SND_PCM_HW_PARAM_FRAME_BITS, 8);
	if (err < 0)
		return err;
	change1 |= err;
	return change1;
}

/* refine *_TIME and *_SIZE from *_BYTES */
static int refine_back_time_and_size(snd_pcm_hw_params_t *params,
				     int time, int size, int bytes)
{
	int err;

	/* size = bytes * 8 / framebits */
	err = rule_mulkdiv(params, size, bytes, 8, SND_PCM_HW_PARAM_FRAME_BITS);
	if (err < 0)
		return err;
	/* time = size * 1000000 / rate */
	err = rule_mulkdiv(params, time, size, 1000000, SND_PCM_HW_PARAM_RATE);
	if (err < 0)
		return err;
	return 0;
}


static int snd_pcm_ioplug_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	int change = 0, change1, change2, err;
	ioplug_priv_t *io = pcm->private_data;
	int i;

	/* access, format */
	for (i = SND_PCM_IOPLUG_HW_ACCESS; i <= SND_PCM_IOPLUG_HW_FORMAT; i++) {
		err = ioplug_mask_refine(hw_param_mask(params, hw_params_type[i]),
				 &io->params[i]);
		if (err < 0)
			return err;
		change |= err;
	}
	/* channels, rate */
	for (; i <= SND_PCM_IOPLUG_HW_RATE; i++) {
		err = ioplug_interval_refine(io, params, i);
		if (err < 0)
			return err;
		change |= err;
	}

	if (params->rmask & ((1 << SND_PCM_HW_PARAM_ACCESS) |
			     (1 << SND_PCM_HW_PARAM_FORMAT) |
			     (1 << SND_PCM_HW_PARAM_SUBFORMAT) |
			     (1 << SND_PCM_HW_PARAM_CHANNELS) |
			     (1 << SND_PCM_HW_PARAM_RATE))) {
		err = snd_pcm_hw_refine_soft(pcm, params);
		if (err < 0)
			return err;
		change |= err;
	}

	change1 = refine_time_and_size(params, SND_PCM_HW_PARAM_PERIOD_TIME,
				       SND_PCM_HW_PARAM_PERIOD_SIZE,
				       SND_PCM_HW_PARAM_PERIOD_BYTES);
	if (change1 < 0)
		return change1;
	err = ioplug_interval_refine(io, params, SND_PCM_IOPLUG_HW_PERIOD_BYTES);
	if (err < 0)
		return err;
	change1 |= err;
	if (change1) {
		change |= change1;
		err = refine_back_time_and_size(params, SND_PCM_HW_PARAM_PERIOD_TIME,
						SND_PCM_HW_PARAM_PERIOD_SIZE,
						SND_PCM_HW_PARAM_PERIOD_BYTES);
		if (err < 0)
			return err;
	}

	change1 = refine_time_and_size(params, SND_PCM_HW_PARAM_BUFFER_TIME,
				       SND_PCM_HW_PARAM_BUFFER_SIZE,
				       SND_PCM_HW_PARAM_BUFFER_BYTES);
	if (change1 < 0)
		return change1;
	change |= change1;

	do {
		change2 = 0;
		err = ioplug_interval_refine(io, params, SND_PCM_IOPLUG_HW_BUFFER_BYTES);
		if (err < 0)
			return err;
		change2 |= err;
		/* periods = buffer_bytes / periods */
		err = rule_div(params, SND_PCM_HW_PARAM_PERIODS,
			       SND_PCM_HW_PARAM_BUFFER_BYTES,
			       SND_PCM_HW_PARAM_PERIOD_BYTES);
		if (err < 0)
			return err;
		change2 |= err;
		err = ioplug_interval_refine(io, params, SND_PCM_IOPLUG_HW_PERIODS);
		if (err < 0)
			return err;
		change2 |= err;
		/* buffer_bytes = periods * period_bytes */
		err = rule_mul(params, SND_PCM_HW_PARAM_BUFFER_BYTES,
			       SND_PCM_HW_PARAM_PERIOD_BYTES,
			       SND_PCM_HW_PARAM_PERIODS);
		if (err < 0)
			return err;
		change2 |= err;
		change1 |= change2;
	} while (change2);
	change |= change1;

	if (change1) {
		err = refine_back_time_and_size(params, SND_PCM_HW_PARAM_BUFFER_TIME,
						SND_PCM_HW_PARAM_BUFFER_SIZE,
						SND_PCM_HW_PARAM_BUFFER_BYTES);
		if (err < 0)
			return err;
	}

#if 0
	fprintf(stderr, "XXX\n");
	dump_parm(params);
#endif
	return change;
}

static int snd_pcm_ioplug_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	ioplug_priv_t *io = pcm->private_data;
	int err;

	INTERNAL(snd_pcm_hw_params_get_access)(params, &io->data->access);
	INTERNAL(snd_pcm_hw_params_get_format)(params, &io->data->format);
	INTERNAL(snd_pcm_hw_params_get_channels)(params, &io->data->channels);
	INTERNAL(snd_pcm_hw_params_get_rate)(params, &io->data->rate, 0);
	INTERNAL(snd_pcm_hw_params_get_period_size)(params, &io->data->period_size, 0);
	INTERNAL(snd_pcm_hw_params_get_buffer_size)(params, &io->data->buffer_size);
	if (io->data->callback->hw_params) {
		err = io->data->callback->hw_params(io->data, params);
		if (err < 0)
			return err;
		INTERNAL(snd_pcm_hw_params_get_access)(params, &io->data->access);
		INTERNAL(snd_pcm_hw_params_get_format)(params, &io->data->format);
		INTERNAL(snd_pcm_hw_params_get_channels)(params, &io->data->channels);
		INTERNAL(snd_pcm_hw_params_get_rate)(params, &io->data->rate, 0);
		INTERNAL(snd_pcm_hw_params_get_period_size)(params, &io->data->period_size, 0);
		INTERNAL(snd_pcm_hw_params_get_buffer_size)(params, &io->data->buffer_size);
	}
	return 0;
}

static int snd_pcm_ioplug_hw_free(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;

	if (io->data->callback->hw_free)
		return io->data->callback->hw_free(io->data);
	return 0;
}

static int snd_pcm_ioplug_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	ioplug_priv_t *io = pcm->private_data;

	if (io->data->callback->sw_params)
		return io->data->callback->sw_params(io->data, params);
	return 0;
}


static int snd_pcm_ioplug_start(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;
	struct timeval tv;
	int err;
	
	if (io->data->state != SND_PCM_STATE_PREPARED)
		return -EBUSY;

	err = io->data->callback->start(io->data);
	if (err < 0)
		return err;

	gettimeofday(&tv, 0);
	io->trigger_tstamp.tv_sec = tv.tv_sec;
	io->trigger_tstamp.tv_nsec = tv.tv_usec * 1000L;
	io->data->state = SND_PCM_STATE_RUNNING;

	return 0;
}

static int snd_pcm_ioplug_drop(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;
	struct timeval tv;

	if (io->data->state == SND_PCM_STATE_OPEN)
		return -EBADFD;

	io->data->callback->stop(io->data);

	gettimeofday(&tv, 0);
	io->trigger_tstamp.tv_sec = tv.tv_sec;
	io->trigger_tstamp.tv_nsec = tv.tv_usec * 1000L;
	io->data->state = SND_PCM_STATE_SETUP;

	return 0;
}

static int snd_pcm_ioplug_drain(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;

	if (io->data->state == SND_PCM_STATE_OPEN)
		return -EBADFD;
	if (io->data->callback->drain)
		io->data->callback->drain(io->data);
	return snd_pcm_ioplug_drop(pcm);
}

static int snd_pcm_ioplug_pause(snd_pcm_t *pcm, int enable)
{
	ioplug_priv_t *io = pcm->private_data;
	static snd_pcm_state_t states[2] = {
		SND_PCM_STATE_PAUSED, SND_PCM_STATE_RUNNING
	};
	int prev, err;

	prev = !enable;
	enable = !prev;
	if (io->data->state != states[prev])
		return -EBADFD;
	if (io->data->callback->pause) {
		err = io->data->callback->pause(io->data, enable);
		if (err < 0)
			return err;
	}
	io->data->state = states[enable];
	return 0;
}

static snd_pcm_sframes_t snd_pcm_ioplug_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_mmap_appl_backward(pcm, frames);
	return frames;
}

static snd_pcm_sframes_t snd_pcm_ioplug_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

static int snd_pcm_ioplug_resume(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;

	if (io->data->callback->resume)
		io->data->callback->resume(io->data);
	return 0;
}

static snd_pcm_sframes_t ioplug_priv_transfer_areas(snd_pcm_t *pcm,
						       const snd_pcm_channel_area_t *areas,
						       snd_pcm_uframes_t offset,
						       snd_pcm_uframes_t size)
{
	ioplug_priv_t *io = pcm->private_data;
	snd_pcm_sframes_t result;
		
	if (! size)
		return 0;
	if (io->data->callback->transfer)
		result = io->data->callback->transfer(io->data, areas, offset, size);
	else
		result = size;
	if (result > 0)
		snd_pcm_mmap_appl_forward(pcm, result);
	return result;
}

static snd_pcm_sframes_t snd_pcm_ioplug_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	if (pcm->mmap_rw)
		return snd_pcm_mmap_writei(pcm, buffer, size);
	else {
		snd_pcm_channel_area_t areas[pcm->channels];
		snd_pcm_areas_from_buf(pcm, areas, (void*)buffer);
		return snd_pcm_write_areas(pcm, areas, 0, size, 
					   ioplug_priv_transfer_areas);
	}
}

static snd_pcm_sframes_t snd_pcm_ioplug_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	if (pcm->mmap_rw)
		return snd_pcm_mmap_writen(pcm, bufs, size);
	else {
		snd_pcm_channel_area_t areas[pcm->channels];
		snd_pcm_areas_from_bufs(pcm, areas, bufs);
		return snd_pcm_write_areas(pcm, areas, 0, size,
					   ioplug_priv_transfer_areas);
	}
}

static snd_pcm_sframes_t snd_pcm_ioplug_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	if (pcm->mmap_rw)
		return snd_pcm_mmap_readi(pcm, buffer, size);
	else {
		snd_pcm_channel_area_t areas[pcm->channels];
		snd_pcm_areas_from_buf(pcm, areas, buffer);
		return snd_pcm_read_areas(pcm, areas, 0, size,
					  ioplug_priv_transfer_areas);
	}
}

static snd_pcm_sframes_t snd_pcm_ioplug_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	if (pcm->mmap_rw)
		return snd_pcm_mmap_readn(pcm, bufs, size);
	else {
		snd_pcm_channel_area_t areas[pcm->channels];
		snd_pcm_areas_from_bufs(pcm, areas, bufs);
		return snd_pcm_read_areas(pcm, areas, 0, size,
					  ioplug_priv_transfer_areas);
	}
}

static snd_pcm_sframes_t snd_pcm_ioplug_mmap_commit(snd_pcm_t *pcm,
						    snd_pcm_uframes_t offset,
						    snd_pcm_uframes_t size)
{
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK &&
	    pcm->access != SND_PCM_ACCESS_RW_INTERLEAVED &&
	    pcm->access != SND_PCM_ACCESS_RW_NONINTERLEAVED) {
		const snd_pcm_channel_area_t *areas;
		snd_pcm_uframes_t ofs, frames = size;

		snd_pcm_mmap_begin(pcm, &areas, &ofs, &frames);
		if (ofs != offset)
			return -EIO;
		return ioplug_priv_transfer_areas(pcm, areas, offset, frames);
	}

	snd_pcm_mmap_appl_forward(pcm, size);
	return size;
}

static snd_pcm_sframes_t snd_pcm_ioplug_avail_update(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;
	snd_pcm_uframes_t avail;

	snd_pcm_ioplug_hw_ptr_update(pcm);
	if (pcm->stream == SND_PCM_STREAM_CAPTURE &&
	    pcm->access != SND_PCM_ACCESS_RW_INTERLEAVED &&
	    pcm->access != SND_PCM_ACCESS_RW_NONINTERLEAVED) {
		if (io->data->callback->transfer) {
			const snd_pcm_channel_area_t *areas;
			snd_pcm_uframes_t offset, size = UINT_MAX;
			snd_pcm_sframes_t result;

			snd_pcm_mmap_begin(pcm, &areas, &offset, &size);
			result = io->data->callback->transfer(io->data, areas, offset, size);
			if (result < 0)
				return result;
		}
	}
	avail = snd_pcm_mmap_avail(pcm);
	if (avail > io->avail_max)
		io->avail_max = avail;
	return (snd_pcm_sframes_t)avail;
}

static int snd_pcm_ioplug_nonblock(snd_pcm_t *pcm, int nonblock)
{
	ioplug_priv_t *io = pcm->private_data;

	io->data->nonblock = nonblock;
	return 0;
}

static int snd_pcm_ioplug_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	ioplug_priv_t *io = pcm->private_data;

	if (io->data->callback->poll_revents)
		return io->data->callback->poll_revents(io->data, pfds, nfds, revents);
	else
		*revents = pfds->revents;
	return 0;
}

static int snd_pcm_ioplug_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_ioplug_async(snd_pcm_t *pcm ATTRIBUTE_UNUSED,
				int sig ATTRIBUTE_UNUSED,
				pid_t pid ATTRIBUTE_UNUSED)
{
	return -ENOSYS;
}

static int snd_pcm_ioplug_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_ioplug_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	ioplug_priv_t *io = pcm->private_data;

	if (io->data->callback->dump)
		io->data->callback->dump(io->data, out);
	else {
		if (io->data->name)
			snd_output_printf(out, "%s\n", io->data->name);
		else
			snd_output_printf(out, "IO-PCM Plugin\n");
		if (pcm->setup) {
			snd_output_printf(out, "Its setup is:\n");
			snd_pcm_dump_setup(pcm, out);
		}
	}
}

static void clear_io_params(ioplug_priv_t *io)
{
	int i;
	for (i = 0; i < SND_PCM_IOPLUG_HW_PARAMS; i++) {
		free(io->params[i].list);
		memset(&io->params[i], 0, sizeof(io->params[i]));
	}
}

static int snd_pcm_ioplug_close(snd_pcm_t *pcm)
{
	ioplug_priv_t *io = pcm->private_data;

	clear_io_params(io);
	if (io->data->callback->close)
		io->data->callback->close(io->data);
	free(io);

	return 0;
}

static snd_pcm_ops_t snd_pcm_ioplug_ops = {
	.close = snd_pcm_ioplug_close,
	.nonblock = snd_pcm_ioplug_nonblock,
	.async = snd_pcm_ioplug_async,
	.poll_revents = snd_pcm_ioplug_poll_revents,
	.info = snd_pcm_ioplug_info,
	.hw_refine = snd_pcm_ioplug_hw_refine,
	.hw_params = snd_pcm_ioplug_hw_params,
	.hw_free = snd_pcm_ioplug_hw_free,
	.sw_params = snd_pcm_ioplug_sw_params,
	.channel_info = snd_pcm_ioplug_channel_info,
	.dump = snd_pcm_ioplug_dump,
	.mmap = snd_pcm_ioplug_mmap,
	.munmap = snd_pcm_ioplug_munmap,
};

static snd_pcm_fast_ops_t snd_pcm_ioplug_fast_ops = {
	.status = snd_pcm_ioplug_status,
	.prepare = snd_pcm_ioplug_prepare,
	.reset = snd_pcm_ioplug_reset,
	.start = snd_pcm_ioplug_start,
	.drop = snd_pcm_ioplug_drop,
	.drain = snd_pcm_ioplug_drain,
	.pause = snd_pcm_ioplug_pause,
	.state = snd_pcm_ioplug_state,
	.hwsync = snd_pcm_ioplug_hwsync,
	.delay = snd_pcm_ioplug_delay,
	.resume = snd_pcm_ioplug_resume,
	.poll_ask = NULL,
	.link_fd = NULL,
	.link = NULL,
	.unlink = NULL,
	.rewind = snd_pcm_ioplug_rewind,
	.forward = snd_pcm_ioplug_forward,
	.writei = snd_pcm_ioplug_writei,
	.writen = snd_pcm_ioplug_writen,
	.readi = snd_pcm_ioplug_readi,
	.readn = snd_pcm_ioplug_readn,
	.avail_update = snd_pcm_ioplug_avail_update,
	.mmap_commit = snd_pcm_ioplug_mmap_commit,
};

static int ioplug_set_parm_minmax(struct ioplug_parm *parm, unsigned int min, unsigned int max)
{
	parm->num_list = 0;
	free(parm->list);
	parm->list = NULL;
	parm->min = min;
	parm->max = max;
	parm->active = 1;
	return 0;
}

static int val_compar(const void *ap, const void *bp)
{
	return *(const unsigned int *)ap - *(const unsigned int *)bp;
}

static int ioplug_set_parm_list(struct ioplug_parm *parm, unsigned int num_list, const unsigned int *list)
{
	unsigned int *new_list;

	new_list = malloc(sizeof(*new_list) * num_list);
	if (new_list == NULL)
		return -ENOMEM;
	memcpy(new_list, list, sizeof(*new_list) * num_list);
	qsort(new_list, num_list, sizeof(*new_list), val_compar);

	free(parm->list);
	parm->num_list = num_list;
	parm->list = new_list;
	parm->active = 1;
	return 0;
}

void snd_pcm_ioplug_params_reset(snd_pcm_ioplug_t *ioplug)
{
	ioplug_priv_t *io = ioplug->pcm->private_data;
	clear_io_params(io);
}

int snd_pcm_ioplug_set_param_list(snd_pcm_ioplug_t *ioplug, int type, unsigned int num_list, const unsigned int *list)
{
	ioplug_priv_t *io = ioplug->pcm->private_data;
	if (type < 0 && type >= SND_PCM_IOPLUG_HW_PARAMS) {
		SNDERR("IOPLUG: invalid parameter type %d", type);
		return -EINVAL;
	}
	if (type == SND_PCM_IOPLUG_HW_PERIODS)
		io->params[type].integer = 1;
	return ioplug_set_parm_list(&io->params[type], num_list, list);
}

int snd_pcm_ioplug_set_param_minmax(snd_pcm_ioplug_t *ioplug, int type, unsigned int min, unsigned int max)
{
	ioplug_priv_t *io = ioplug->pcm->private_data;
	if (type < 0 && type >= SND_PCM_IOPLUG_HW_PARAMS) {
		SNDERR("IOPLUG: invalid parameter type %d", type);
		return -EINVAL;
	}
	if (type == SND_PCM_IOPLUG_HW_ACCESS || type == SND_PCM_IOPLUG_HW_FORMAT) {
		SNDERR("IOPLUG: invalid parameter type %d", type);
		return -EINVAL;
	}
	if (type == SND_PCM_IOPLUG_HW_PERIODS)
		io->params[type].integer = 1;
	return ioplug_set_parm_minmax(&io->params[type], min, max);
}

int snd_pcm_ioplug_reinit_status(snd_pcm_ioplug_t *ioplug)
{
	ioplug->pcm->poll_fd = ioplug->poll_fd;
	ioplug->pcm->poll_events = ioplug->poll_events;
	ioplug->pcm->mmap_rw = ioplug->mmap_rw;
	return 0;
}

const snd_pcm_channel_area_t *snd_pcm_ioplug_mmap_areas(snd_pcm_ioplug_t *ioplug)
{
	if (ioplug->mmap_rw)
		return snd_pcm_mmap_areas(ioplug->pcm);
	return NULL;
}

int snd_pcm_ioplug_create(snd_pcm_ioplug_t *ioplug, const char *name,
			  snd_pcm_stream_t stream, int mode)
{
	ioplug_priv_t *io;
	int err;
	snd_pcm_t *pcm;
	snd_pcm_access_t def_access = SND_PCM_ACCESS_RW_INTERLEAVED;
	snd_pcm_format_t def_format = SND_PCM_FORMAT_S16;

	assert(ioplug && ioplug->callback);
	assert(ioplug->callback->start &&
	       ioplug->callback->stop &&
	       ioplug->callback->pointer);

	io = calloc(1, sizeof(*io));
	if (! io)
		return -ENOMEM;

	io->data = ioplug;
	ioplug->state = SND_PCM_STATE_OPEN;
	ioplug->stream = stream;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_IOPLUG, name, stream, mode);
	if (err < 0) {
		free(io);
		return err;
	}

	ioplug->pcm = pcm;
	pcm->ops = &snd_pcm_ioplug_ops;
	pcm->fast_ops = &snd_pcm_ioplug_fast_ops;
	pcm->private_data = io;

	snd_pcm_set_hw_ptr(pcm, &ioplug->hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &ioplug->appl_ptr, -1, 0);

	snd_pcm_ioplug_reinit_status(ioplug);

	return 0;
}

int snd_pcm_ioplug_delete(snd_pcm_ioplug_t *ioplug)
{
	return snd_pcm_close(ioplug->pcm);
}

