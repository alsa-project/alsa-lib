/*
 *  PCM - Meter plugin
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
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
  
#include <byteswap.h>
#include <time.h>
#include "pcm_meter.h"

#define FREQUENCY 50

typedef struct _snd_pcm_meter_s16 {
	snd_pcm_adpcm_state_t *adpcm_states;
	unsigned int index;
	snd_pcm_uframes_t old;
} snd_pcm_meter_s16_t;

int s16_open(snd_pcm_meter_scope_t *scope)
{
	snd_pcm_meter_t *meter = scope->pcm->private_data;
	snd_pcm_t *spcm = meter->slave;
	snd_pcm_channel_area_t *a;
	unsigned int c;
	snd_pcm_meter_s16_t *s16;
	int index;
	if (spcm->format == SND_PCM_FORMAT_S16 &&
	    spcm->access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED) {
		meter->buf16 = (int16_t *) meter->buf;
		return -EINVAL;
	}
	switch (spcm->format) {
	case SND_PCM_FORMAT_A_LAW:
	case SND_PCM_FORMAT_MU_LAW:
	case SND_PCM_FORMAT_IMA_ADPCM:
		index = snd_pcm_linear_put_index(SND_PCM_FORMAT_S16, SND_PCM_FORMAT_S16);
		break;
	case SND_PCM_FORMAT_S8:
	case SND_PCM_FORMAT_S16_LE:
	case SND_PCM_FORMAT_S16_BE:
	case SND_PCM_FORMAT_S24_LE:
	case SND_PCM_FORMAT_S24_BE:
	case SND_PCM_FORMAT_S32_LE:
	case SND_PCM_FORMAT_S32_BE:
	case SND_PCM_FORMAT_U8:
	case SND_PCM_FORMAT_U16_LE:
	case SND_PCM_FORMAT_U16_BE:
	case SND_PCM_FORMAT_U24_LE:
	case SND_PCM_FORMAT_U24_BE:
	case SND_PCM_FORMAT_U32_LE:
	case SND_PCM_FORMAT_U32_BE:
		index = snd_pcm_linear_convert_index(spcm->format, SND_PCM_FORMAT_S16);
		break;
	default:
		return -EINVAL;
	}
	s16 = calloc(1, sizeof(*s16));
	if (!s16)
		return -ENOMEM;
	s16->index = index;
	if (spcm->format == SND_PCM_FORMAT_IMA_ADPCM) {
		s16->adpcm_states = calloc(spcm->channels, sizeof(*s16->adpcm_states));
		if (!s16->adpcm_states) {
			free(s16);
			return -ENOMEM;
		}
	}
	meter->buf16 = malloc(meter->buf_size * 2 * spcm->channels);
	if (!meter->buf16) {
		if (s16->adpcm_states)
			free(s16->adpcm_states);
		free(s16);
		return -ENOMEM;
	}
	a = calloc(spcm->channels, sizeof(*a));
	if (!a) {
		free(meter->buf16);
		if (s16->adpcm_states)
			free(s16->adpcm_states);
		free(s16);
		return -ENOMEM;
	}
	meter->buf16_areas = a;
	for (c = 0; c < spcm->channels; c++, a++) {
		a->addr = meter->buf16 + c * meter->buf_size;
		a->first = 0;
		a->step = 16;
	}
	scope->private_data = s16;
	return 0;
}

void s16_close(snd_pcm_meter_scope_t *scope)
{
	snd_pcm_meter_t *meter = scope->pcm->private_data;
	snd_pcm_meter_s16_t *s16 = scope->private_data;
	if (s16->adpcm_states)
		free(s16->adpcm_states);
	free(s16);
	free(meter->buf16);
	meter->buf16 = NULL;
	free(meter->buf16_areas);
}

void s16_start(snd_pcm_meter_scope_t *scope ATTRIBUTE_UNUSED)
{
}

void s16_stop(snd_pcm_meter_scope_t *scope ATTRIBUTE_UNUSED)
{
}

void s16_update(snd_pcm_meter_scope_t *scope)
{
	snd_pcm_meter_t *meter = scope->pcm->private_data;
	snd_pcm_meter_s16_t *s16 = scope->private_data;
	snd_pcm_t *spcm = meter->slave;
	snd_pcm_sframes_t size;
	snd_pcm_uframes_t offset;
	size = meter->now - s16->old;
	if (size < 0)
		size += spcm->boundary;
	offset = s16->old % meter->buf_size;
	while (size > 0) {
		snd_pcm_uframes_t frames = size;
		snd_pcm_uframes_t cont = meter->buf_size - offset;
		if (frames > cont)
			frames = cont;
		switch (spcm->format) {
		case SND_PCM_FORMAT_A_LAW:
			snd_pcm_alaw_decode(meter->buf16_areas, offset,
					    meter->buf_areas, offset,
					    spcm->channels, frames,
					    s16->index);
			break;
		case SND_PCM_FORMAT_MU_LAW:
			snd_pcm_mulaw_decode(meter->buf16_areas, offset,
					     meter->buf_areas, offset,
					     spcm->channels, frames,
					     s16->index);
			break;
		case SND_PCM_FORMAT_IMA_ADPCM:
			snd_pcm_adpcm_decode(meter->buf16_areas, offset,
					     meter->buf_areas, offset,
					     spcm->channels, frames,
					     s16->index,
					     s16->adpcm_states);
			break;
		default:
			snd_pcm_linear_convert(meter->buf16_areas, offset,
					       meter->buf_areas, offset,
					       spcm->channels, frames,
					       s16->index);
			break;
		}
		if (frames == cont)
			offset = 0;
		else
			offset += frames;
		size -= frames;
	}
	s16->old = meter->now;
}

void s16_reset(snd_pcm_meter_scope_t *scope)
{
	snd_pcm_meter_t *meter = scope->pcm->private_data;
	snd_pcm_meter_s16_t *s16 = scope->private_data;
	s16->old = meter->now;
}

snd_pcm_meter_scope_t s16_scope = {
	name: "s16",
	open: s16_open,
	start: s16_start,
	stop: s16_stop,
	update: s16_update,
	reset: s16_reset,
	close: s16_close,
	pcm: NULL,
	active: 0,
	list: { 0, 0 },
	private_data: NULL,
};

void snd_pcm_meter_add_scope(snd_pcm_t *pcm, snd_pcm_meter_scope_t *scope)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	scope->pcm = pcm;
	list_add_tail(&scope->list, &meter->scopes);
}

void snd_pcm_meter_add_frames(snd_pcm_t *pcm,
			      const snd_pcm_channel_area_t *areas,
			      snd_pcm_uframes_t ptr,
			      snd_pcm_uframes_t frames)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	while (frames > 0) {
		snd_pcm_uframes_t n = frames;
		snd_pcm_uframes_t dst_offset = ptr % meter->buf_size;
		snd_pcm_uframes_t src_offset = ptr % pcm->buffer_size;
		snd_pcm_uframes_t dst_cont = meter->buf_size - dst_offset;
		snd_pcm_uframes_t src_cont = pcm->buffer_size - src_offset;
		if (n > dst_cont)
			n = dst_cont;
		if (n > src_cont)
			n = src_cont;
		snd_pcm_areas_copy(meter->buf_areas, dst_offset, 
				   areas, src_offset,
				   pcm->channels, n, pcm->format);
		frames -= n;
		ptr += n;
		if (ptr == pcm->boundary)
			ptr = 0;
	}
}

static void snd_pcm_meter_update_main(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	snd_pcm_sframes_t frames;
	snd_pcm_uframes_t rptr, old_rptr;
	const snd_pcm_channel_area_t *areas;
	int locked;
	locked = (pthread_mutex_trylock(&meter->update_mutex) >= 0);
	areas = snd_pcm_mmap_areas(pcm);
	rptr = *pcm->hw_ptr;
	old_rptr = meter->rptr;
	meter->rptr = rptr;
	frames = rptr - old_rptr;
	if (frames < 0)
		frames += pcm->boundary;
	if (frames > 0) {
		assert((snd_pcm_uframes_t) frames <= pcm->buffer_size);
		snd_pcm_meter_add_frames(pcm, areas, old_rptr, frames);
	}
	if (locked)
		pthread_mutex_unlock(&meter->update_mutex);
}

static int snd_pcm_meter_update_scope(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	snd_pcm_sframes_t frames;
	snd_pcm_uframes_t rptr, old_rptr;
	const snd_pcm_channel_area_t *areas;
	int reset = 0;
	/* Wait main thread */
	pthread_mutex_lock(&meter->update_mutex);
	areas = snd_pcm_mmap_areas(pcm);
 _again:
	rptr = *pcm->hw_ptr;
	old_rptr = meter->rptr;
	rmb();
	if (atomic_read(&meter->reset)) {
		reset = 1;
		atomic_dec(&meter->reset);
		goto _again;
	}
	meter->rptr = rptr;
	frames = rptr - old_rptr;
	if (frames < 0)
		frames += pcm->boundary;
	if (frames > 0) {
		assert((snd_pcm_uframes_t) frames <= pcm->buffer_size);
		snd_pcm_meter_add_frames(pcm, areas, old_rptr, frames);
	}
	pthread_mutex_unlock(&meter->update_mutex);
	return reset;
}

static void *snd_pcm_meter_thread(void *data)
{
	snd_pcm_t *pcm = data;
	snd_pcm_meter_t *meter = pcm->private_data;
	snd_pcm_t *spcm = meter->slave;
	struct list_head *pos;
	snd_pcm_meter_scope_t *scope;
	int err, reset;
	list_for_each(pos, &meter->scopes) {
		scope = list_entry(pos, snd_pcm_meter_scope_t, list);
		err = scope->open(scope);
		scope->active = (err >= 0);
	}
	while (!meter->closed) {
		snd_pcm_sframes_t now;
		snd_pcm_status_t status;
		int err;
		pthread_mutex_lock(&meter->running_mutex);
		err = snd_pcm_status(spcm, &status);
		assert(err >= 0);
		if (status.state != SND_PCM_STATE_RUNNING &&
		    (status.state != SND_PCM_STATE_DRAINING ||
		     spcm->stream != SND_PCM_STREAM_PLAYBACK)) {
			if (meter->running) {
				list_for_each(pos, &meter->scopes) {
					scope = list_entry(pos, snd_pcm_meter_scope_t, list);
					scope->stop(scope);
				}
				meter->running = 0;
			}
			pthread_cond_wait(&meter->running_cond,
					  &meter->running_mutex);
			pthread_mutex_unlock(&meter->running_mutex);
			continue;
		}
		pthread_mutex_unlock(&meter->running_mutex);
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
			now = status.appl_ptr - status.delay;
			if (now < 0)
				now += pcm->boundary;
		} else {
			now = status.appl_ptr + status.delay;
			if ((snd_pcm_uframes_t) now >= pcm->boundary)
				now -= pcm->boundary;
		}
		meter->now = now;
		if (pcm->stream == SND_PCM_STREAM_CAPTURE)
			reset = snd_pcm_meter_update_scope(pcm);
		else {
			reset = 0;
			while (atomic_read(&meter->reset)) {
				reset = 1;
				atomic_dec(&meter->reset);
			}
		}
		if (reset) {
			list_for_each(pos, &meter->scopes) {
				scope = list_entry(pos, snd_pcm_meter_scope_t, list);
				if (scope->active)
					scope->reset(scope);
			}
			continue;
		}
		if (!meter->running) {
			list_for_each(pos, &meter->scopes) {
				scope = list_entry(pos, snd_pcm_meter_scope_t, list);
				if (scope->active)
					scope->start(scope);
			}
			meter->running = 1;
		}
		list_for_each(pos, &meter->scopes) {
			scope = list_entry(pos, snd_pcm_meter_scope_t, list);
			if (scope->active)
				scope->update(scope);
		}
	        nanosleep(&meter->delay, NULL);
	}
	list_for_each(pos, &meter->scopes) {
		scope = list_entry(pos, snd_pcm_meter_scope_t, list);
		if (scope->active)
			scope->close(scope);
	}
	return NULL;
}


static int snd_pcm_meter_close(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	int err = 0;
	pthread_mutex_destroy(&meter->update_mutex);
	pthread_mutex_destroy(&meter->running_mutex);
	pthread_cond_destroy(&meter->running_cond);
	if (meter->close_slave)
		err = snd_pcm_close(meter->slave);
	free(meter);
	return err;
}

static int snd_pcm_meter_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_nonblock(meter->slave, nonblock);
}

static int snd_pcm_meter_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_async(meter->slave, sig, pid);
}

static int snd_pcm_meter_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_info(meter->slave, info);
}

static int snd_pcm_meter_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_channel_info(meter->slave, info);
}

static int snd_pcm_meter_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_status(meter->slave, status);
}

static snd_pcm_state_t snd_pcm_meter_state(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_state(meter->slave);
}

static int snd_pcm_meter_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_delay(meter->slave, delayp);
}

static int snd_pcm_meter_prepare(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	int err;
	atomic_inc(&meter->reset);
	err = snd_pcm_prepare(meter->slave);
	if (err >= 0) {
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
			meter->rptr = *pcm->appl_ptr;
		else
			meter->rptr = *pcm->hw_ptr;
	}
	return err;
}

static int snd_pcm_meter_reset(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	int err = snd_pcm_reset(meter->slave);
	if (err >= 0) {
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
			meter->rptr = *pcm->appl_ptr;
	}
	return err;
}

static int snd_pcm_meter_start(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	int err;
	pthread_mutex_lock(&meter->running_mutex);
	err = snd_pcm_start(meter->slave);
	if (err >= 0)
		pthread_cond_signal(&meter->running_cond);
	pthread_mutex_unlock(&meter->running_mutex);
	return err;
}

static int snd_pcm_meter_drop(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_drop(meter->slave);
}

static int snd_pcm_meter_drain(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_drain(meter->slave);
}

static int snd_pcm_meter_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_pause(meter->slave, enable);
}

static snd_pcm_sframes_t snd_pcm_meter_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	snd_pcm_sframes_t err = snd_pcm_rewind(meter->slave, frames);
	if (err > 0 && pcm->stream == SND_PCM_STREAM_PLAYBACK)
		meter->rptr = *pcm->appl_ptr;
	return err;
}

static snd_pcm_sframes_t snd_pcm_meter_mmap_forward(snd_pcm_t *pcm, snd_pcm_uframes_t size)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	snd_pcm_uframes_t old_rptr = *pcm->appl_ptr;
	snd_pcm_sframes_t result = snd_pcm_mmap_forward(meter->slave, size);
	if (result <= 0)
		return result;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		snd_pcm_meter_add_frames(pcm, snd_pcm_mmap_areas(pcm), old_rptr, result);
		meter->rptr = *pcm->appl_ptr;
	}
	return result;
}

static snd_pcm_sframes_t snd_pcm_meter_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	snd_pcm_sframes_t result = snd_pcm_avail_update(meter->slave);
	if (result <= 0)
		return result;
	if (pcm->stream == SND_PCM_STREAM_CAPTURE)
		snd_pcm_meter_update_main(pcm);
	return result;
}

static int snd_pcm_meter_hw_refine_cprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params)
{
	int err;
	snd_pcm_access_mask_t access_mask = { SND_PCM_ACCBIT_PLUGIN };
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_ACCESS,
					 &access_mask);
	if (err < 0)
		return err;
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_meter_hw_refine_sprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *sparams)
{
	snd_pcm_access_mask_t saccess_mask = { SND_PCM_ACCBIT_MMAP };
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_set_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				   &saccess_mask);
	return 0;
}

static int snd_pcm_meter_hw_refine_schange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
					  snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = ~SND_PCM_HW_PARBIT_ACCESS;
	err = _snd_pcm_hw_params_refine(sparams, links, params);
	if (err < 0)
		return err;
	return 0;
}
	
static int snd_pcm_meter_hw_refine_cchange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
					  snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = ~SND_PCM_HW_PARBIT_ACCESS;
	err = _snd_pcm_hw_params_refine(params, links, sparams);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_meter_hw_refine_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_hw_refine(meter->slave, params);
}

int snd_pcm_meter_hw_params_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return _snd_pcm_hw_params(meter->slave, params);
}

static int snd_pcm_meter_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_meter_hw_refine_cprepare,
				       snd_pcm_meter_hw_refine_cchange,
				       snd_pcm_meter_hw_refine_sprepare,
				       snd_pcm_meter_hw_refine_schange,
				       snd_pcm_meter_hw_refine_slave);
}

static int snd_pcm_meter_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	unsigned int channel;
	snd_pcm_t *slave = meter->slave;
	size_t buf_size_bytes;
	int err;
	err = snd_pcm_hw_params_slave(pcm, params,
				      snd_pcm_meter_hw_refine_cchange,
				      snd_pcm_meter_hw_refine_sprepare,
				      snd_pcm_meter_hw_refine_schange,
				      snd_pcm_meter_hw_params_slave);
	if (err < 0)
		return err;
	/* more than 1 second of buffer */
	meter->buf_size = slave->buffer_size;
	while (meter->buf_size < slave->rate)
		meter->buf_size *= 2;
	buf_size_bytes = snd_pcm_frames_to_bytes(slave, meter->buf_size);
	assert(!meter->buf);
	meter->buf = malloc(buf_size_bytes);
	meter->buf_areas = malloc(sizeof(*meter->buf_areas) * slave->channels);
	for (channel = 0; channel < slave->channels; ++channel) {
		snd_pcm_channel_area_t *a = &meter->buf_areas[channel];
		a->addr = meter->buf + buf_size_bytes / slave->channels * channel;
		a->first = 0;
		a->step = slave->sample_bits;
	}
	meter->closed = 0;
	err = pthread_create(&meter->thread, NULL, snd_pcm_meter_thread, pcm);
	assert(err == 0);
	return 0;
}

static int snd_pcm_meter_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	int err;
	meter->closed = 1;
	pthread_mutex_lock(&meter->running_mutex);
	pthread_cond_signal(&meter->running_cond);
	pthread_mutex_unlock(&meter->running_mutex);
	err = pthread_join(meter->thread, 0);
	assert(err == 0);
	if (meter->buf) {
		free(meter->buf);
		free(meter->buf_areas);
		meter->buf = 0;
		meter->buf_areas = 0;
	}
	return snd_pcm_hw_free(meter->slave);
}

static int snd_pcm_meter_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	return snd_pcm_sw_params(meter->slave, params);
}

static int snd_pcm_meter_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_meter_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_meter_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_meter_t *meter = pcm->private_data;
	snd_output_printf(out, "Meter PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(meter->slave, out);
}

snd_pcm_ops_t snd_pcm_meter_ops = {
	close: snd_pcm_meter_close,
	info: snd_pcm_meter_info,
	hw_refine: snd_pcm_meter_hw_refine,
	hw_params: snd_pcm_meter_hw_params,
	hw_free: snd_pcm_meter_hw_free,
	sw_params: snd_pcm_meter_sw_params,
	channel_info: snd_pcm_meter_channel_info,
	dump: snd_pcm_meter_dump,
	nonblock: snd_pcm_meter_nonblock,
	async: snd_pcm_meter_async,
	mmap: snd_pcm_meter_mmap,
	munmap: snd_pcm_meter_munmap,
};

snd_pcm_fast_ops_t snd_pcm_meter_fast_ops = {
	status: snd_pcm_meter_status,
	state: snd_pcm_meter_state,
	delay: snd_pcm_meter_delay,
	prepare: snd_pcm_meter_prepare,
	reset: snd_pcm_meter_reset,
	start: snd_pcm_meter_start,
	drop: snd_pcm_meter_drop,
	drain: snd_pcm_meter_drain,
	pause: snd_pcm_meter_pause,
	rewind: snd_pcm_meter_rewind,
	writei: snd_pcm_mmap_writei,
	writen: snd_pcm_mmap_writen,
	readi: snd_pcm_mmap_readi,
	readn: snd_pcm_mmap_readn,
	avail_update: snd_pcm_meter_avail_update,
	mmap_forward: snd_pcm_meter_mmap_forward,
};

int snd_pcm_meter_open(snd_pcm_t **pcmp, const char *name, unsigned int frequency,
		       snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_meter_t *meter;
	assert(pcmp);
	meter = calloc(1, sizeof(snd_pcm_meter_t));
	if (!meter)
		return -ENOMEM;
	meter->slave = slave;
	meter->close_slave = close_slave;
	meter->delay.tv_sec = 0;
	meter->delay.tv_nsec = 1000000000 / frequency;
	INIT_LIST_HEAD(&meter->scopes);

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(meter);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_METER;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->mmap_rw = 1;
	pcm->ops = &snd_pcm_meter_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_meter_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private_data = meter;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = slave->hw_ptr;
	pcm->appl_ptr = slave->appl_ptr;
	*pcmp = pcm;

	pthread_mutex_init(&meter->update_mutex, NULL);
	pthread_mutex_init(&meter->running_mutex, NULL);
	pthread_cond_init(&meter->running_cond, NULL);
#if 1
	snd_pcm_meter_add_scope(pcm, &s16_scope);
#endif
	return 0;
}

int _snd_pcm_meter_open(snd_pcm_t **pcmp, const char *name,
		       snd_config_t *conf, 
		       snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	const char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	long frequency = -1;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "type") == 0)
			continue;
		if (strcmp(id, "sname") == 0) {
			err = snd_config_get_string(n, &sname);
			if (err < 0) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(id, "frequency") == 0) {
			err = snd_config_get_integer(n, &frequency);
			if (err < 0) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!sname) {
		SNDERR("sname is not defined");
		return -EINVAL;
	}

	/* This is needed cause snd_config_update may destroy config */
	sname = strdup(sname);
	if (!sname)
		return  -ENOMEM;
	err = snd_pcm_open(&spcm, sname, stream, mode);
	free((void *) sname);
	if (err < 0)
		return err;
	err = snd_pcm_meter_open(pcmp, name, frequency < 0 ? FREQUENCY : frequency, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
