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
#include <errno.h>
#include <math.h>
#include "pcm_local.h"

typedef struct {
	snd_pcm_t *pcm;
	unsigned int channels_count;
	int close_slave;
} snd_pcm_multi_slave_t;

typedef struct {
	int slave_idx;
	unsigned int slave_channel;
} snd_pcm_multi_channel_t;

typedef struct {
	size_t slaves_count;
	snd_pcm_multi_slave_t *slaves;
	size_t channels_count;
	snd_pcm_multi_channel_t *channels;
	int xfer_mode;
} snd_pcm_multi_t;

static int snd_pcm_multi_close(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	size_t i;
	int ret = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		int err;
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		if (slave->close_slave)
			err = snd_pcm_close(slave->pcm);
		else
			err = snd_pcm_unlink(slave->pcm);
		if (err < 0)
			ret = err;
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
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *slave_0 = multi->slaves[0].pcm;
	return snd_pcm_async(slave_0, sig, pid);
}

static int snd_pcm_multi_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	snd_pcm_multi_t *multi = pcm->private;
	int err;
	snd_pcm_t *slave_0 = multi->slaves[0].pcm;
	/* FIXME */
	err = snd_pcm_info(slave_0, info);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_multi_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t *info)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	int err;
	snd_pcm_t *slave_0 = multi->slaves[0].pcm;
	unsigned int req_mask = info->req_mask;
	unsigned int channels = info->req.format.channels;
	if ((req_mask & SND_PCM_PARAMS_CHANNELS) &&
	    channels != multi->channels_count) {
		info->req.fail_mask |= SND_PCM_PARAMS_CHANNELS;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	info->req_mask |= SND_PCM_PARAMS_CHANNELS;
	info->req.format.channels = multi->slaves[0].channels_count;
	err = snd_pcm_params_info(slave_0, info);
	info->req_mask = req_mask;
	info->req.format.channels = channels;
	if (err < 0)
		return err;
	info->min_channels = multi->channels_count;
	info->max_channels = multi->channels_count;
	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave_i = multi->slaves[i].pcm;
		snd_pcm_params_info_t info_i;
		info_i = *info;
		info_i.req_mask |= SND_PCM_PARAMS_CHANNELS;
		info_i.req.format.channels = multi->slaves[i].channels_count;
		err = snd_pcm_params_info(slave_i, &info_i);
		if (err < 0)
			return err;
		info->formats &= info_i.formats;
		info->rates &= info_i.rates;
		if (info_i.min_rate > info->min_rate)
			info->min_rate = info_i.min_rate;
		if (info_i.max_rate < info->max_rate)
			info->max_rate = info_i.max_rate;
		if (info_i.buffer_size < info->buffer_size)
			info->buffer_size = info_i.buffer_size;
		if (info_i.min_fragment_size > info->min_fragment_size)
			info->min_fragment_size = info_i.min_fragment_size;
		if (info_i.max_fragment_size < info->max_fragment_size)
			info->max_fragment_size = info_i.max_fragment_size;
		if (info_i.min_fragments > info->min_fragments)
			info->min_fragments = info_i.min_fragments;
		if (info_i.max_fragments < info->max_fragments)
			info->max_fragments = info_i.max_fragments;
		info->flags &= info_i.flags;
	}
	if (info->flags & SND_PCM_INFO_INTERLEAVED) {
		if (multi->slaves_count > 0) {
			info->flags &= ~SND_PCM_INFO_INTERLEAVED;
			info->flags |= SND_PCM_INFO_COMPLEX;
		}
	} else if (!(info->flags & SND_PCM_INFO_NONINTERLEAVED))
		info->flags |= SND_PCM_INFO_COMPLEX;
	info->req_mask = req_mask;
	return 0;
}

static int snd_pcm_multi_mmap(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	size_t count = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		snd_pcm_setup_t *setup;
		int err = snd_pcm_mmap(slave);
		if (err < 0)
			return err;
		count += slave->mmap_info_count;
		setup = &slave->setup;
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
			snd_pcm_channel_area_t r[setup->format.channels];
			snd_pcm_channel_area_t s[setup->format.channels];
			err = snd_pcm_mmap_get_areas(slave, s, r);
			if (err < 0)
				return err;
			err = snd_pcm_areas_silence(s, 0, setup->format.channels, setup->buffer_size, setup->format.sfmt);
			if (err < 0)
				return err;
			err = snd_pcm_areas_silence(r, 0, setup->format.channels, setup->buffer_size, setup->format.sfmt);
			if (err < 0)
				return err;
		}
	}
	pcm->mmap_info_count = count;
	pcm->mmap_info = malloc(count * sizeof(*pcm->mmap_info));
	count = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		memcpy(&pcm->mmap_info[count], slave->mmap_info, slave->mmap_info_count * sizeof(*pcm->mmap_info));
		count += slave->mmap_info_count;
	}
	return 0;
}

static int snd_pcm_multi_munmap(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		int err = snd_pcm_munmap(slave);
		if (err < 0)
			return err;
	}
	pcm->mmap_info_count = 0;
	free(pcm->mmap_info);
	pcm->mmap_info = 0;
	return 0;
}
		
static int snd_pcm_multi_params(snd_pcm_t *pcm, snd_pcm_params_t *params)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	snd_pcm_params_t p;
	int err = 0;
	if (params->format.channels != multi->channels_count) {
		params->fail_mask = SND_PCM_PARAMS_CHANNELS;
		params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	p = *params;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		p.format.channels = multi->slaves[i].channels_count;
		err = snd_pcm_params(slave, &p);
		if (err < 0) {
			params->fail_mask = p.fail_mask;
			params->fail_reason = p.fail_reason;
			break;
		}
	}
	if (err == 0)
		multi->xfer_mode = params->xfer_mode;
	return err;
}

static int snd_pcm_multi_setup(snd_pcm_t *pcm, snd_pcm_setup_t *setup)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	int err;
	err = snd_pcm_setup(multi->slaves[0].pcm, setup);
	if (err < 0)
		return err;
	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_setup_t s;
		snd_pcm_t *sh = multi->slaves[i].pcm;
		err = snd_pcm_setup(sh, &s);
		if (err < 0)
			return err;
		if (setup->format.rate != s.format.rate)
			return -EINVAL;
		if (setup->buffer_size != s.buffer_size)
			return -EINVAL;
		if (setup->mmap_shape != SND_PCM_MMAP_NONINTERLEAVED ||
		    s.mmap_shape != SND_PCM_MMAP_NONINTERLEAVED)
			setup->mmap_shape = SND_PCM_MMAP_COMPLEX;
	}
	setup->format.channels = multi->channels_count;
	if (multi->xfer_mode == SND_PCM_XFER_UNSPECIFIED)
		setup->xfer_mode = SND_PCM_XFER_NONINTERLEAVED;
	else
		setup->xfer_mode = multi->xfer_mode;
	return 0;
}

static int snd_pcm_multi_status(snd_pcm_t *pcm, snd_pcm_status_t *status)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *slave = multi->slaves[0].pcm;
	return snd_pcm_status(slave, status);
}

static int snd_pcm_multi_state(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *slave = multi->slaves[0].pcm;
	return snd_pcm_state(slave);
}

static int snd_pcm_multi_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *slave = multi->slaves[0].pcm;
	return snd_pcm_delay(slave, delayp);
}

static ssize_t snd_pcm_multi_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *slave = multi->slaves[0].pcm;
	return snd_pcm_avail_update(slave);
}

static int snd_pcm_multi_prepare(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_prepare(multi->slaves[0].pcm);
}

static int snd_pcm_multi_start(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_start(multi->slaves[0].pcm);
}

static int snd_pcm_multi_drop(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_drop(multi->slaves[0].pcm);
}

static int snd_pcm_multi_drain(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_drain(multi->slaves[0].pcm);
}

static int snd_pcm_multi_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_pause(multi->slaves[0].pcm, enable);
}

static int snd_pcm_multi_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	snd_pcm_multi_t *multi = pcm->private;
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

static int snd_pcm_multi_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int channel = params->channel;
	snd_pcm_multi_channel_t *c = &multi->channels[channel];
	int err;
	if (c->slave_idx < 0)
		return -ENXIO;
	params->channel = c->slave_channel;
	err = snd_pcm_channel_params(multi->slaves[c->slave_idx].pcm, params);
	params->channel = channel;
	return err;
}

static int snd_pcm_multi_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int channel = setup->channel;
	snd_pcm_multi_channel_t *c = &multi->channels[channel];
	int err;
	if (c->slave_idx < 0)
		return -ENXIO;
	setup->channel = c->slave_channel;
	err = snd_pcm_channel_setup(multi->slaves[c->slave_idx].pcm, setup);
	setup->channel = channel;
	return err;
}

static ssize_t snd_pcm_multi_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	size_t pos[multi->slaves_count];
	memset(pos, 0, sizeof(pos));
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave_i = multi->slaves[i].pcm;
		ssize_t f = snd_pcm_rewind(slave_i, frames);
		if (f < 0)
			return f;
		pos[i] = f;
		frames = f;
	}
	/* Realign the pointers */
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave_i = multi->slaves[i].pcm;
		size_t f = pos[i] - frames;
		if (f > 0)
			snd_pcm_mmap_appl_forward(slave_i, f);
	}
	return frames;
}

static ssize_t snd_pcm_multi_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;

	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		ssize_t frames = snd_pcm_mmap_forward(slave, size);
		if (frames < 0)
			return frames;
		if (i == 0) {
			size = frames;
			continue;
		}
		if ((size_t) frames != size)
			return -EBADFD;
	}
	return size;
}

static int snd_pcm_multi_channels_mask(snd_pcm_t *pcm, bitset_t *cmask)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	bitset_t *cmasks[multi->slaves_count];
	int err;
	for (i = 0; i < multi->slaves_count; ++i)
		cmasks[i] = bitset_alloc(multi->slaves[i].channels_count);
	for (i = 0; i < multi->channels_count; ++i) {
		snd_pcm_multi_channel_t *b = &multi->channels[i];
		if (b->slave_idx < 0)
			continue;
		if (bitset_get(cmask, i))
			bitset_set(cmasks[b->slave_idx], b->slave_channel);
	}
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *slave = multi->slaves[i].pcm;
		err = snd_pcm_channels_mask(slave, cmasks[i]);
		if (err < 0) {
			for (i = 0; i <= multi->slaves_count; ++i)
				free(cmasks[i]);
			return err;
		}
	}
	bitset_zero(cmask, pcm->setup.format.channels);
	for (i = 0; i < multi->channels_count; ++i) {
		snd_pcm_multi_channel_t *b = &multi->channels[i];
		if (b->slave_idx < 0)
			continue;
		if (bitset_get(cmasks[b->slave_idx], b->slave_channel))
			bitset_set(cmask, i);
	}
	for (i = 0; i < multi->slaves_count; ++i)
		free(cmasks[i]);
	return 0;
}
		
int snd_pcm_multi_poll_descriptor(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *slave = multi->slaves[0].pcm;
	return snd_pcm_poll_descriptor(slave);
}

static void snd_pcm_multi_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int k;
	fprintf(fp, "Multi PCM\n");
	fprintf(fp, "\nChannel bindings:\n");
	for (k = 0; k < multi->channels_count; ++k) {
		snd_pcm_multi_channel_t *c = &multi->channels[k];
		if (c->slave_idx < 0)
			continue;
		fprintf(fp, "%d: slave %d, channel %d\n", 
			k, c->slave_idx, c->slave_channel);
	}
	if (pcm->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	for (k = 0; k < multi->slaves_count; ++k) {
		fprintf(fp, "\nSlave #%d: ", k);
		snd_pcm_dump(multi->slaves[k].pcm, fp);
	}
}

snd_pcm_ops_t snd_pcm_multi_ops = {
	close: snd_pcm_multi_close,
	info: snd_pcm_multi_info,
	params_info: snd_pcm_multi_params_info,
	params: snd_pcm_multi_params,
	setup: snd_pcm_multi_setup,
	channel_info: snd_pcm_multi_channel_info,
	channel_params: snd_pcm_multi_channel_params,
	channel_setup: snd_pcm_multi_channel_setup,
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
	start: snd_pcm_multi_start,
	drop: snd_pcm_multi_drop,
	drain: snd_pcm_multi_drain,
	pause: snd_pcm_multi_pause,
	writei: snd_pcm_mmap_writei,
	writen: snd_pcm_mmap_writen,
	readi: snd_pcm_mmap_readi,
	readn: snd_pcm_mmap_readn,
	rewind: snd_pcm_multi_rewind,
	channels_mask: snd_pcm_multi_channels_mask,
	avail_update: snd_pcm_multi_avail_update,
	mmap_forward: snd_pcm_multi_mmap_forward,
};

int snd_pcm_multi_open(snd_pcm_t **pcmp, char *name,
		       size_t slaves_count,
		       snd_pcm_t **slaves_pcm, size_t *schannels_count,
		       size_t channels_count,
		       int *sidxs, unsigned int *schannels,
		       int close_slaves)
{
	snd_pcm_t *pcm;
	snd_pcm_multi_t *multi;
	unsigned int i;
	int stream;
	char slave_map[32][32] = { { 0 } };

	assert(pcmp);
	assert(slaves_count > 0 && slaves_pcm && schannels_count);
	assert(channels_count > 0 && sidxs && schannels);

	multi = calloc(1, sizeof(snd_pcm_multi_t));
	if (!multi) {
		return -ENOMEM;
	}

	stream = slaves_pcm[0]->stream;
	
	multi->slaves_count = slaves_count;
	multi->slaves = calloc(slaves_count, sizeof(*multi->slaves));
	multi->channels_count = channels_count;
	multi->channels = calloc(channels_count, sizeof(*multi->channels));
	for (i = 0; i < slaves_count; ++i) {
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		assert(slaves_pcm[i]->stream == stream);
		slave->pcm = slaves_pcm[i];
		slave->channels_count = schannels_count[i];
		slave->close_slave = close_slaves;
		if (i != 0)
			snd_pcm_link(slaves_pcm[i-1], slaves_pcm[i]);
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
	pcm->mmap_auto = 1;
	pcm->ops = &snd_pcm_multi_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_multi_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = multi;
	pcm->poll_fd = multi->slaves[0].pcm->poll_fd;
	pcm->hw_ptr = multi->slaves[0].pcm->hw_ptr;
	pcm->appl_ptr = multi->slaves[0].pcm->appl_ptr;
	*pcmp = pcm;
	return 0;
}

int _snd_pcm_multi_open(snd_pcm_t **pcmp, char *name, snd_config_t *conf,
			int stream, int mode)
{
	snd_config_iterator_t i, j;
	snd_config_t *slave = NULL;
	snd_config_t *binding = NULL;
	int err;
	unsigned int idx;
	char **slaves_id = NULL;
	char **slaves_name = NULL;
	snd_pcm_t **slaves_pcm = NULL;
	size_t *slaves_channels = NULL;
	unsigned int *channels_sidx = NULL;
	unsigned int *channels_schannel = NULL;
	size_t slaves_count = 0;
	size_t channels_count = 0;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "slave") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_COMPOUND)
				return -EINVAL;
			slave = n;
			continue;
		}
		if (strcmp(n->id, "binding") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_COMPOUND)
				return -EINVAL;
			binding = n;
			continue;
		}
		return -EINVAL;
	}
	if (!slave || !binding)
		return -EINVAL;
	snd_config_foreach(i, slave) {
		++slaves_count;
	}
	snd_config_foreach(i, binding) {
		int cchannel = -1;
		char *p;
		snd_config_t *m = snd_config_entry(i);
		errno = 0;
		cchannel = strtol(m->id, &p, 10);
		if (errno || *p || cchannel < 0)
			return -EINVAL;
		if ((unsigned)cchannel >= channels_count)
			channels_count = cchannel + 1;
	}
	if (channels_count == 0)
		return -EINVAL;
	slaves_id = calloc(slaves_count, sizeof(*slaves_id));
	slaves_name = calloc(slaves_count, sizeof(*slaves_name));
	slaves_pcm = calloc(slaves_count, sizeof(*slaves_pcm));
	slaves_channels = calloc(slaves_count, sizeof(*slaves_channels));
	channels_sidx = calloc(channels_count, sizeof(*channels_sidx));
	channels_schannel = calloc(channels_count, sizeof(*channels_schannel));
	idx = 0;
	for (idx = 0; idx < channels_count; ++idx)
		channels_sidx[idx] = -1;
	snd_config_foreach(i, slave) {
		snd_config_t *m = snd_config_entry(i);
		char *name = NULL;
		long channels = -1;
		slaves_id[idx] = snd_config_id(m);
		snd_config_foreach(j, m) {
			snd_config_t *n = snd_config_entry(j);
			if (strcmp(n->id, "comment") == 0)
				continue;
			if (strcmp(n->id, "name") == 0) {
				err = snd_config_string_get(n, &name);
				if (err < 0)
					goto _free;
				continue;
			}
			if (strcmp(n->id, "channels") == 0) {
				err = snd_config_integer_get(n, &channels);
				if (err < 0)
					goto _free;
				continue;
			}
			err = -EINVAL;
			goto _free;
		}
		if (!name || channels < 0) {
			err = -EINVAL;
			goto _free;
		}
		slaves_name[idx] = strdup(name);
		slaves_channels[idx] = channels;
		++idx;
	}

	snd_config_foreach(i, binding) {
		snd_config_t *m = snd_config_entry(i);
		long cchannel = -1;
		long schannel = -1;
		int slave = -1;
		long val;
		char *str;
		cchannel = strtol(m->id, 0, 10);
		snd_config_foreach(j, m) {
			snd_config_t *n = snd_config_entry(j);
			if (strcmp(n->id, "comment") == 0)
				continue;
			if (strcmp(n->id, "sidx") == 0) {
				char buf[32];
				unsigned int k;
				err = snd_config_string_get(n, &str);
				if (err < 0) {
					err = snd_config_integer_get(n, &val);
					if (err < 0)
						goto _free;
					sprintf(buf, "%ld", val);
					str = buf;
				}
				for (k = 0; k < slaves_count; ++k) {
					if (strcmp(slaves_id[k], str) == 0)
						slave = k;
				}
				continue;
			}
			if (strcmp(n->id, "schannel") == 0) {
				err = snd_config_integer_get(n, &schannel);
				if (err < 0)
					goto _free;
				continue;
			}
			err = -EINVAL;
			goto _free;
		}
		if (cchannel < 0 || slave < 0 || schannel < 0) {
			err = -EINVAL;
			goto _free;
		}
		if ((size_t)slave >= slaves_count) {
			err = -EINVAL;
			goto _free;
		}
		if ((unsigned int) schannel >= slaves_channels[slave]) {
			err = -EINVAL;
			goto _free;
		}
		channels_sidx[cchannel] = slave;
		channels_schannel[cchannel] = schannel;
	}
	
	for (idx = 0; idx < slaves_count; ++idx) {
		err = snd_pcm_open(&slaves_pcm[idx], slaves_name[idx], stream, mode);
		if (err < 0)
			goto _free;
	}
	err = snd_pcm_multi_open(pcmp, name, slaves_count, slaves_pcm,
				 slaves_channels,
				 channels_count,
				 channels_sidx, channels_schannel,
				 1);
_free:
	if (err < 0) {
		for (idx = 0; idx < slaves_count; ++idx) {
			if (slaves_pcm[idx])
				snd_pcm_close(slaves_pcm[idx]);
			if (slaves_name[idx])
				free(slaves_name[idx]);
		}
	}
	if (slaves_name)
		free(slaves_name);
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

