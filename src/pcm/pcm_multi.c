/*
 *  PCM - Multi
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
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "pcm_local.h"

typedef struct {
	snd_pcm_t *pcm;
	unsigned int channels_count;
	int close_slave;
	int linked;
} snd_pcm_multi_slave_t;

typedef struct {
	int slave_idx;
	unsigned int slave_channel;
} snd_pcm_multi_channel_t;

typedef struct {
	unsigned int slaves_count;
	unsigned int master_slave;
	snd_pcm_multi_slave_t *slaves;
	unsigned int channels_count;
	snd_pcm_multi_channel_t *channels;
} snd_pcm_multi_t;

static int snd_pcm_multi_close(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int i;
	int ret = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		if (slave->close_slave) {
			int err = snd_pcm_close(slave->pcm);
			if (err < 0)
				ret = err;
		}
	}
	free(multi->slaves);
	free(multi->channels);
	free(multi);
	return ret;
}

static int snd_pcm_multi_nonblock(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int nonblock ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_multi_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_t *slave_0 = multi->slaves[0].pcm;
	return snd_pcm_async(slave_0, sig, pid);
}

static int snd_pcm_multi_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	int err, n;
	assert(info->subdevice < multi->slaves_count);
	n = info->subdevice;
	info->subdevice = 0;
	err = snd_pcm_info(multi->slaves[n].pcm, info);
	if (err < 0)
		return err;
	info->subdevices_count = multi->slaves_count;
	return 0;
}

static int snd_pcm_multi_hw_refine_cprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_access_mask_t access_mask;
	int err;
	snd_pcm_access_mask_any(&access_mask);
	snd_pcm_access_mask_reset(&access_mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_ACCESS,
					 &access_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_set(params, SND_PCM_HW_PARAM_CHANNELS,
				    multi->channels_count, 0);
	if (err < 0)
		return err;
	params->info = ~0U;
	return 0;
}

static int snd_pcm_multi_hw_refine_sprepare(snd_pcm_t *pcm, unsigned int slave_idx,
					    snd_pcm_hw_params_t *sparams)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_multi_slave_t *slave = &multi->slaves[slave_idx];
	snd_pcm_access_mask_t saccess_mask = { SND_PCM_ACCBIT_MMAP };
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_set_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				   &saccess_mask);
	_snd_pcm_hw_param_set(sparams, SND_PCM_HW_PARAM_CHANNELS,
			      slave->channels_count, 0);
	return 0;
}

static int snd_pcm_multi_hw_refine_schange(snd_pcm_t *pcm ATTRIBUTE_UNUSED,
					   unsigned int slave_idx ATTRIBUTE_UNUSED,
					   snd_pcm_hw_params_t *params,
					   snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_FORMAT |
			      SND_PCM_HW_PARBIT_SUBFORMAT |
			      SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	const snd_pcm_access_mask_t *access_mask = snd_pcm_hw_param_get_mask(params, SND_PCM_HW_PARAM_ACCESS);
	if (!snd_pcm_access_mask_test(access_mask, SND_PCM_ACCESS_RW_INTERLEAVED) &&
	    !snd_pcm_access_mask_test(access_mask, SND_PCM_ACCESS_RW_NONINTERLEAVED) &&
	    !snd_pcm_access_mask_test(access_mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED)) {
		snd_pcm_access_mask_t saccess_mask;
		snd_pcm_access_mask_any(&saccess_mask);
		snd_pcm_access_mask_reset(&saccess_mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
		err = _snd_pcm_hw_param_set_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
						 &saccess_mask);
		if (err < 0)
			return err;
	}
	err = _snd_pcm_hw_params_refine(sparams, links, params);
	if (err < 0)
		return err;
	return 0;
}
	
static int snd_pcm_multi_hw_refine_cchange(snd_pcm_t *pcm ATTRIBUTE_UNUSED,
					   unsigned int slave_idx ATTRIBUTE_UNUSED,
					   snd_pcm_hw_params_t *params,
					   snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_FORMAT |
			      SND_PCM_HW_PARBIT_SUBFORMAT |
			      SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	snd_pcm_access_mask_t access_mask;
	const snd_pcm_access_mask_t *saccess_mask = snd_pcm_hw_param_get_mask(sparams, SND_PCM_HW_PARAM_ACCESS);
	snd_pcm_access_mask_any(&access_mask);
	snd_pcm_access_mask_reset(&access_mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
	if (!snd_pcm_access_mask_test(saccess_mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED))
		snd_pcm_access_mask_reset(&access_mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
	if (!snd_pcm_access_mask_test(saccess_mask, SND_PCM_ACCESS_MMAP_COMPLEX) &&
	    !snd_pcm_access_mask_test(saccess_mask, SND_PCM_ACCESS_MMAP_INTERLEAVED))
		snd_pcm_access_mask_reset(&access_mask, SND_PCM_ACCESS_MMAP_COMPLEX);
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_ACCESS,
					 &access_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_refine(params, links, sparams);
	if (err < 0)
		return err;
	params->info &= sparams->info;
	return 0;
}

static int snd_pcm_multi_hw_refine_slave(snd_pcm_t *pcm,
					 unsigned int slave_idx,
					 snd_pcm_hw_params_t *sparams)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_t *slave = multi->slaves[slave_idx].pcm;
	return snd_pcm_hw_refine(slave, sparams);
}

static int snd_pcm_multi_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int k;
	snd_pcm_hw_params_t sparams[multi->slaves_count];
	int err;
	unsigned int cmask, changed;
	err = snd_pcm_multi_hw_refine_cprepare(pcm, params);
	if (err < 0)
		return err;
	for (k = 0; k < multi->slaves_count; ++k) {
		err = snd_pcm_multi_hw_refine_sprepare(pcm, k, &sparams[k]);
		if (err < 0) {
			SNDERR("Slave PCM #%d not useable", k);
			return err;
		}
	}
	do {
		cmask = params->cmask;
		params->cmask = 0;
		for (k = 0; k < multi->slaves_count; ++k) {
			err = snd_pcm_multi_hw_refine_schange(pcm, k, params, &sparams[k]);
			if (err >= 0)
				err = snd_pcm_multi_hw_refine_slave(pcm, k, &sparams[k]);
			if (err < 0) {
				snd_pcm_multi_hw_refine_cchange(pcm, k, params, &sparams[k]);
				return err;
			}
			err = snd_pcm_multi_hw_refine_cchange(pcm, k, params, &sparams[k]);
			if (err < 0)
				return err;
		}
		err = snd_pcm_hw_refine_soft(pcm, params);
		changed = params->cmask;
		params->cmask |= cmask;
		if (err < 0)
			return err;
	} while (changed);
	return 0;
}

static int snd_pcm_multi_hw_params_slave(snd_pcm_t *pcm,
					 unsigned int slave_idx,
					 snd_pcm_hw_params_t *sparams)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_t *slave = multi->slaves[slave_idx].pcm;
	int err = snd_pcm_hw_params(slave, sparams);
	if (err < 0)
		return err;
	err = snd_pcm_areas_silence(slave->running_areas, 0, slave->channels, slave->buffer_size, slave->format);
	if (err < 0)
		return err;
	if (slave->stopped_areas) {
		err = snd_pcm_areas_silence(slave->stopped_areas, 0, slave->channels, slave->buffer_size, slave->format);
		if (err < 0)
			return err;
	}
	return 0;
}

static int snd_pcm_multi_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int i;
	snd_pcm_hw_params_t sparams[multi->slaves_count];
	int err;
	for (i = 0; i < multi->slaves_count; ++i) {
		err = snd_pcm_multi_hw_refine_sprepare(pcm, i, &sparams[i]);
		assert(err >= 0);
		err = snd_pcm_multi_hw_refine_schange(pcm, i, params, &sparams[i]);
		assert(err >= 0);
		err = snd_pcm_multi_hw_params_slave(pcm, i, &sparams[i]);
		if (err < 0) {
			snd_pcm_multi_hw_refine_cchange(pcm, i, params, &sparams[i]);
			return err;
		}
	}
	multi->slaves[0].linked = 0;
	for (i = 1; i < multi->slaves_count; ++i) {
		err = snd_pcm_link(multi->slaves[0].pcm, multi->slaves[i].pcm);
		multi->slaves[i].linked = (err >= 0);
	}
	return 0;
}

static int snd_pcm_multi_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int i;
	int err = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		int e = snd_pcm_hw_free(slave);
		if (e < 0)
			err = e;
		if (!multi->slaves[i].linked)
			continue;
		multi->slaves[i].linked = 0;
		e = snd_pcm_unlink(slave);
		if (e < 0)
			err = e;
	}
	return err;
}

static int snd_pcm_multi_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int i;
	int err;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		err = snd_pcm_sw_params(slave, params);
		if (err < 0)
			return err;
	}
	return 0;
}

static int snd_pcm_multi_status(snd_pcm_t *pcm, snd_pcm_status_t *status)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_t *slave = multi->slaves[0].pcm;
	return snd_pcm_status(slave, status);
}

static snd_pcm_state_t snd_pcm_multi_state(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_t *slave = multi->slaves[0].pcm;
	return snd_pcm_state(slave);
}

static int snd_pcm_multi_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_t *slave = multi->slaves[0].pcm;
	return snd_pcm_delay(slave, delayp);
}

static snd_pcm_sframes_t snd_pcm_multi_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	snd_pcm_sframes_t ret = LONG_MAX;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_sframes_t avail;
		avail = snd_pcm_avail_update(multi->slaves[i].pcm);
		if (avail < 0)
			return avail;
		if (ret > avail)
			ret = avail;
	}
	return ret;
}

static int snd_pcm_multi_prepare(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	int err = 0;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		if (multi->slaves[i].linked)
			continue;
		err = snd_pcm_prepare(multi->slaves[i].pcm);
		if (err < 0)
			return err;
	}
	return err;
}

static int snd_pcm_multi_reset(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	int err = 0;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		if (multi->slaves[i].linked)
			continue;
		err = snd_pcm_reset(multi->slaves[i].pcm);
		if (err < 0)
			return err;
	}
	return err;
}

static int snd_pcm_multi_start(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	int err = 0;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		if (multi->slaves[i].linked)
			continue;
		err = snd_pcm_start(multi->slaves[i].pcm);
		if (err < 0)
			return err;
	}
	return err;
}

static int snd_pcm_multi_drop(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	int err = 0;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		if (multi->slaves[i].linked)
			continue;
		err = snd_pcm_drop(multi->slaves[i].pcm);
		if (err < 0)
			return err;
	}
	return err;
}

static int snd_pcm_multi_drain(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	int err = 0;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		if (multi->slaves[i].linked)
			continue;
		err = snd_pcm_drain(multi->slaves[i].pcm);
		if (err < 0)
			return err;
	}
	return err;
}

static int snd_pcm_multi_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	int err = 0;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		if (multi->slaves[i].linked)
			continue;
		err = snd_pcm_pause(multi->slaves[i].pcm, enable);
		if (err < 0)
			return err;
	}
	return err;
}

static int snd_pcm_multi_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int channel = info->channel;
	snd_pcm_multi_channel_t *c = &multi->channels[channel];
	int err;
	if (c->slave_idx < 0)
		return -ENXIO;
	info->channel = c->slave_channel;
	err = snd_pcm_channel_info(multi->slaves[c->slave_idx].pcm, info);
	info->channel = channel;
	return err;
}

static snd_pcm_sframes_t snd_pcm_multi_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int i;
	snd_pcm_uframes_t pos[multi->slaves_count];
	memset(pos, 0, sizeof(pos));
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave_i = multi->slaves[i].pcm;
		snd_pcm_sframes_t f = snd_pcm_rewind(slave_i, frames);
		if (f < 0)
			return f;
		pos[i] = f;
		frames = f;
	}
	/* Realign the pointers */
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave_i = multi->slaves[i].pcm;
		snd_pcm_uframes_t f = pos[i] - frames;
		if (f > 0)
			snd_pcm_mmap_commit(slave_i, snd_pcm_mmap_offset(slave_i), f);
	}
	return frames;
}

static snd_pcm_sframes_t snd_pcm_multi_mmap_commit(snd_pcm_t *pcm,
						   snd_pcm_uframes_t offset,
						   snd_pcm_uframes_t size)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int i;

	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		snd_pcm_sframes_t frames = snd_pcm_mmap_commit(slave, offset, size);
		if (frames < 0)
			return frames;
		if (i == 0) {
			size = frames;
			continue;
		}
		if ((snd_pcm_uframes_t) frames != size)
			return -EBADFD;
	}
	return size;
}

static int snd_pcm_multi_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_multi_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_multi_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_multi_t *multi = pcm->private_data;
	unsigned int k;
	snd_output_printf(out, "Multi PCM\n");
	snd_output_printf(out, "\nChannel bindings:\n");
	for (k = 0; k < multi->channels_count; ++k) {
		snd_pcm_multi_channel_t *c = &multi->channels[k];
		if (c->slave_idx < 0)
			continue;
		snd_output_printf(out, "%d: slave %d, channel %d\n", 
			k, c->slave_idx, c->slave_channel);
	}
	if (pcm->setup) {
		snd_output_printf(out, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	for (k = 0; k < multi->slaves_count; ++k) {
		snd_output_printf(out, "\nSlave #%d: ", k);
		snd_pcm_dump(multi->slaves[k].pcm, out);
	}
}

snd_pcm_ops_t snd_pcm_multi_ops = {
	close: snd_pcm_multi_close,
	info: snd_pcm_multi_info,
	hw_refine: snd_pcm_multi_hw_refine,
	hw_params: snd_pcm_multi_hw_params,
	hw_free: snd_pcm_multi_hw_free,
	sw_params: snd_pcm_multi_sw_params,
	channel_info: snd_pcm_multi_channel_info,
	dump: snd_pcm_multi_dump,
	nonblock: snd_pcm_multi_nonblock,
	async: snd_pcm_multi_async,
	mmap: snd_pcm_multi_mmap,
	munmap: snd_pcm_multi_munmap,
};

snd_pcm_fast_ops_t snd_pcm_multi_fast_ops = {
	status: snd_pcm_multi_status,
	state: snd_pcm_multi_state,
	delay: snd_pcm_multi_delay,
	prepare: snd_pcm_multi_prepare,
	reset: snd_pcm_multi_reset,
	start: snd_pcm_multi_start,
	drop: snd_pcm_multi_drop,
	drain: snd_pcm_multi_drain,
	pause: snd_pcm_multi_pause,
	writei: snd_pcm_mmap_writei,
	writen: snd_pcm_mmap_writen,
	readi: snd_pcm_mmap_readi,
	readn: snd_pcm_mmap_readn,
	rewind: snd_pcm_multi_rewind,
	avail_update: snd_pcm_multi_avail_update,
	mmap_commit: snd_pcm_multi_mmap_commit,
};

int snd_pcm_multi_open(snd_pcm_t **pcmp, const char *name,
		       unsigned int slaves_count, unsigned int master_slave,
		       snd_pcm_t **slaves_pcm, unsigned int *schannels_count,
		       unsigned int channels_count,
		       int *sidxs, unsigned int *schannels,
		       int close_slaves)
{
	snd_pcm_t *pcm;
	snd_pcm_multi_t *multi;
	unsigned int i;
	snd_pcm_stream_t stream;
	char slave_map[32][32] = { { 0 } };

	assert(pcmp);
	assert(slaves_count > 0 && slaves_pcm && schannels_count);
	assert(channels_count > 0 && sidxs && schannels);
	assert(master_slave < slaves_count);

	multi = calloc(1, sizeof(snd_pcm_multi_t));
	if (!multi) {
		return -ENOMEM;
	}

	stream = slaves_pcm[0]->stream;
	
	multi->slaves_count = slaves_count;
	multi->master_slave = master_slave;
	multi->slaves = calloc(slaves_count, sizeof(*multi->slaves));
	multi->channels_count = channels_count;
	multi->channels = calloc(channels_count, sizeof(*multi->channels));
	for (i = 0; i < slaves_count; ++i) {
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		assert(slaves_pcm[i]->stream == stream);
		slave->pcm = slaves_pcm[i];
		slave->channels_count = schannels_count[i];
		slave->close_slave = close_slaves;
	}
	for (i = 0; i < channels_count; ++i) {
		snd_pcm_multi_channel_t *bind = &multi->channels[i];
		assert(sidxs[i] < (int)slaves_count);
		assert(schannels[i] < schannels_count[sidxs[i]]);
		bind->slave_idx = sidxs[i];
		bind->slave_channel = schannels[i];
		if (sidxs[i] < 0)
			continue;
		assert(!slave_map[sidxs[i]][schannels[i]]);
		slave_map[sidxs[i]][schannels[i]] = 1;
	}
	multi->channels_count = channels_count;

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(multi);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_MULTI;
	pcm->stream = stream;
	pcm->mode = multi->slaves[0].pcm->mode;
	pcm->mmap_rw = 1;
	pcm->ops = &snd_pcm_multi_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_multi_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private_data = multi;
	pcm->poll_fd = multi->slaves[master_slave].pcm->poll_fd;
	pcm->hw_ptr = multi->slaves[master_slave].pcm->hw_ptr;
	pcm->appl_ptr = multi->slaves[master_slave].pcm->appl_ptr;
	*pcmp = pcm;
	return 0;
}

int _snd_pcm_multi_open(snd_pcm_t **pcmp, const char *name,
			snd_config_t *root, snd_config_t *conf,
			snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, inext, j, jnext;
	snd_config_t *slaves = NULL;
	snd_config_t *bindings = NULL;
	int err;
	unsigned int idx;
	const char **slaves_id = NULL;
	snd_config_t **slaves_conf = NULL;
	snd_pcm_t **slaves_pcm = NULL;
	unsigned int *slaves_channels = NULL;
	int *channels_sidx = NULL;
	unsigned int *channels_schannel = NULL;
	unsigned int slaves_count = 0;
	long master_slave = 0;
	unsigned int channels_count = 0;
	snd_config_for_each(i, inext, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "slaves") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			slaves = n;
			continue;
		}
		if (strcmp(id, "bindings") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			bindings = n;
			continue;
		}
		if (strcmp(id, "master") == 0) {
			if (snd_config_get_integer(n, &((long)master_slave)) < 0) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slaves) {
		SNDERR("slaves is not defined");
		return -EINVAL;
	}
	if (!bindings) {
		SNDERR("bindings is not defined");
		return -EINVAL;
	}
	snd_config_for_each(i, inext, slaves) {
		++slaves_count;
	}
	if (master_slave < 0 || master_slave >= (long)slaves_count) {
		SNDERR("Master slave is out of range (0-%u)\n", slaves_count-1);
		return -EINVAL;
	}
	snd_config_for_each(i, inext, bindings) {
		long cchannel;
		snd_config_t *m = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(m);
		err = safe_strtol(id, &cchannel);
		if (err < 0 || cchannel < 0) {
			SNDERR("Invalid channel number: %s", id);
			return -EINVAL;
		}
		if ((unsigned long)cchannel >= channels_count)
			channels_count = cchannel + 1;
	}
	if (channels_count == 0) {
		SNDERR("No cannels defined");
		return -EINVAL;
	}
	slaves_id = calloc(slaves_count, sizeof(*slaves_id));
	slaves_conf = calloc(slaves_count, sizeof(*slaves_conf));
	slaves_pcm = calloc(slaves_count, sizeof(*slaves_pcm));
	slaves_channels = calloc(slaves_count, sizeof(*slaves_channels));
	channels_sidx = calloc(channels_count, sizeof(*channels_sidx));
	channels_schannel = calloc(channels_count, sizeof(*channels_schannel));
	idx = 0;
	for (idx = 0; idx < channels_count; ++idx)
		channels_sidx[idx] = -1;
	idx = 0;
	snd_config_for_each(i, inext, slaves) {
		snd_config_t *m = snd_config_iterator_entry(i);
		int channels;
		slaves_id[idx] = snd_config_get_id(m);
		err = snd_pcm_slave_conf(root, m, &slaves_conf[idx], 1,
					 SND_PCM_HW_PARAM_CHANNELS, 1, &channels);
		if (err < 0)
			goto _free;
		slaves_channels[idx] = channels;
		++idx;
	}

	snd_config_for_each(i, inext, bindings) {
		snd_config_t *m = snd_config_iterator_entry(i);
		long cchannel = -1;
		long schannel = -1;
		int slave = -1;
		long val;
		const char *str;
		const char *id = snd_config_get_id(m);
		err = safe_strtol(id, &cchannel);
		if (err < 0 || cchannel < 0) {
			SNDERR("Invalid channel number: %s", id);
			err = -EINVAL;
			goto _free;
		}
		snd_config_for_each(j, jnext, m) {
			snd_config_t *n = snd_config_iterator_entry(j);
			id = snd_config_get_id(n);
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "slave") == 0) {
				char buf[32];
				unsigned int k;
				err = snd_config_get_string(n, &str);
				if (err < 0) {
					err = snd_config_get_integer(n, &val);
					if (err < 0) {
						SNDERR("Invalid value for %s", id);
						goto _free;
					}
					sprintf(buf, "%ld", val);
					str = buf;
				}
				for (k = 0; k < slaves_count; ++k) {
					if (strcmp(slaves_id[k], str) == 0)
						slave = k;
				}
				continue;
			}
			if (strcmp(id, "channel") == 0) {
				err = snd_config_get_integer(n, &schannel);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _free;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
			err = -EINVAL;
			goto _free;
		}
		if (slave < 0 || (unsigned int)slave >= slaves_count) {
			SNDERR("Invalid or missing sidx");
			err = -EINVAL;
			goto _free;
		}
		if (schannel < 0 || 
		    (unsigned int) schannel >= slaves_channels[slave]) {
			SNDERR("Invalid or missing schannel");
			err = -EINVAL;
			goto _free;
		}
		channels_sidx[cchannel] = slave;
		channels_schannel[cchannel] = schannel;
	}
	
	for (idx = 0; idx < slaves_count; ++idx) {
		err = snd_pcm_open_slave(&slaves_pcm[idx], root, slaves_conf[idx], stream, mode);
		if (err < 0)
			goto _free;
	}
	err = snd_pcm_multi_open(pcmp, name, slaves_count, master_slave,
				 slaves_pcm, slaves_channels,
				 channels_count,
				 channels_sidx, channels_schannel,
				 1);
_free:
	if (err < 0) {
		for (idx = 0; idx < slaves_count; ++idx) {
			if (slaves_pcm[idx])
				snd_pcm_close(slaves_pcm[idx]);
		}
	}
	if (slaves_conf)
		free(slaves_conf);
	if (slaves_pcm)
		free(slaves_pcm);
	if (slaves_channels)
		free(slaves_channels);
	if (channels_sidx)
		free(channels_sidx);
	if (channels_schannel)
		free(channels_schannel);
	return err;
}

