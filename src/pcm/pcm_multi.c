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
#include <sys/uio.h>
#include "pcm_local.h"

typedef struct {
	snd_pcm_t *handle;
	unsigned int channels_total;
	int close_slave;
} snd_pcm_multi_slave_t;

typedef struct {
	unsigned int client_channel;
	unsigned int slave;
	unsigned int slave_channel;
} snd_pcm_multi_bind_t;

typedef struct {
	size_t slaves_count;
	snd_pcm_multi_slave_t *slaves;
	size_t bindings_count;
	snd_pcm_multi_bind_t *bindings;
	size_t channels_count;
	int xfer_mode, mmap_shape;
} snd_pcm_multi_t;

static int snd_pcm_multi_close(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	int ret = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		int err;
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		if (slave->close_slave) {
			err = snd_pcm_close(slave->handle);
			if (err < 0)
				ret = err;
		} else
			snd_pcm_unlink(slave->handle);
	}
	free(multi->slaves);
	free(multi->bindings);
	free(multi);
	return ret;
}

static int snd_pcm_multi_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_nonblock(handle, nonblock);
}

static int snd_pcm_multi_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	snd_pcm_multi_t *multi = pcm->private;
	int err;
	snd_pcm_t *handle_0 = multi->slaves[0].handle;
	/* FIXME */
	err = snd_pcm_info(handle_0, info);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_multi_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t *info)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	int err;
	snd_pcm_t *handle_0 = multi->slaves[0].handle;
	unsigned int old_mask = info->req_mask;
	info->req_mask &= ~(SND_PCM_PARAMS_CHANNELS |
			    SND_PCM_PARAMS_MMAP_SHAPE |
			    SND_PCM_PARAMS_XFER_MODE);
	err = snd_pcm_params_info(handle_0, info);
	if (err < 0)
		return err;
	info->min_channels = multi->channels_count;
	info->max_channels = multi->channels_count;
	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle_i = multi->slaves[i].handle;
		snd_pcm_params_info_t info_i;
		info_i = *info;
		info_i.req_mask |= SND_PCM_PARAMS_CHANNELS;
		info_i.req.format.channels = multi->slaves[i].channels_total;
		err = snd_pcm_params_info(handle_i, &info_i);
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
	info->req_mask = old_mask;
	return 0;
}

static int snd_pcm_multi_params(snd_pcm_t *pcm, snd_pcm_params_t *params)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	snd_pcm_params_t p;
	if (params->format.channels != multi->channels_count)
		return -EINVAL;
	multi->xfer_mode = params->xfer_mode;
	multi->mmap_shape = params->mmap_shape;
	p = *params;
	for (i = 0; i < multi->slaves_count; ++i) {
		int err;
		snd_pcm_t *handle = multi->slaves[i].handle;
		if (handle->mmap_data) {
			err = snd_pcm_munmap_data(handle);
			if (err < 0)
				return err;
		}
		p.xfer_mode = SND_PCM_XFER_UNSPECIFIED;
		p.mmap_shape = SND_PCM_MMAP_UNSPECIFIED;
		p.format.channels = multi->slaves[i].channels_total;
#if 1
		p.xrun_max = ~0;
#endif
		err = snd_pcm_params(handle, &p);
		if (err < 0) {
			params->fail_mask = p.fail_mask;
			params->fail_reason = p.fail_reason;
			return err;
		}
	}
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_mmap_data(handle, NULL);
		if (err < 0)
			return err;
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
			snd_pcm_areas_silence(handle->mmap_areas, 0, handle->setup.format.channels, 
					      handle->setup.buffer_size, handle->setup.format.sfmt);
	}
	return 0;
}

static int snd_pcm_multi_setup(snd_pcm_t *pcm, snd_pcm_setup_t *setup)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	int err;
	size_t frames_alloc;
	err = snd_pcm_setup(multi->slaves[0].handle, setup);
	if (err < 0)
		return err;
	frames_alloc = multi->slaves[0].handle->setup.frag_size;
	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_setup_t s;
		snd_pcm_t *sh = multi->slaves[i].handle;
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
	if (multi->mmap_shape != SND_PCM_MMAP_UNSPECIFIED &&
	    multi->mmap_shape != setup->mmap_shape)
		return -EINVAL;
	return 0;
}

static int snd_pcm_multi_status(snd_pcm_t *pcm, snd_pcm_status_t *status)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_status(handle, status);
}

static int snd_pcm_multi_state(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_state(handle);
}

static int snd_pcm_multi_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_delay(handle, delayp);
}

static ssize_t snd_pcm_multi_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_avail_update(handle);
}

static int snd_pcm_multi_prepare(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_prepare(multi->slaves[0].handle);
}

static int snd_pcm_multi_start(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_start(multi->slaves[0].handle);
}

static int snd_pcm_multi_stop(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_stop(multi->slaves[0].handle);
}

static int snd_pcm_multi_drain(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_drain(multi->slaves[0].handle);
}

static int snd_pcm_multi_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_multi_t *multi = pcm->private;
	return snd_pcm_pause(multi->slaves[0].handle, enable);
}

static int snd_pcm_multi_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	int err;
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int channel = info->channel;
	unsigned int i;
	for (i = 0; i < multi->bindings_count; ++i) {
		if (multi->bindings[i].client_channel == channel) {
			info->channel = multi->bindings[i].slave_channel;
			err = snd_pcm_channel_info(multi->slaves[multi->bindings[i].slave].handle, info);
			info->channel = channel;
			return err;
		}
	}
	info->channel = channel;
	return -EINVAL;
}

static int snd_pcm_multi_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	int err;
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int channel = params->channel;
	unsigned int i;
	for (i = 0; i < multi->bindings_count; ++i) {
		if (multi->bindings[i].client_channel == channel) {
			params->channel = multi->bindings[i].slave_channel;
			err = snd_pcm_channel_params(multi->slaves[multi->bindings[i].slave].handle, params);
			params->channel = channel;
			return err;
		}
	}
	params->channel = channel;
	return -EINVAL;
}

static int snd_pcm_multi_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	int err;
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int channel = setup->channel;
	unsigned int i;
	for (i = 0; i < multi->bindings_count; ++i) {
		if (multi->bindings[i].client_channel == channel) {
			setup->channel = multi->bindings[i].slave_channel;
			err = snd_pcm_channel_setup(multi->slaves[multi->bindings[i].slave].handle, setup);
			setup->channel = channel;
			return err;
		}
	}
	memset(setup, 0, sizeof(*setup));
	setup->channel = channel;
	return 0;
}

static ssize_t snd_pcm_multi_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	size_t pos[multi->slaves_count];
	memset(pos, 0, sizeof(pos));
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle_i = multi->slaves[i].handle;
		ssize_t f = snd_pcm_rewind(handle_i, frames);
		if (f < 0)
			return f;
		pos[i] = f;
		frames = f;
	}
	/* Realign the pointers */
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle_i = multi->slaves[i].handle;
		size_t f = pos[i] - frames;
		if (f > 0)
			snd_pcm_mmap_appl_forward(handle_i, f);
	}
	return frames;
}

static int snd_pcm_multi_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	pcm->mmap_status = multi->slaves[0].handle->mmap_status;
	return 0;
}

static int snd_pcm_multi_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	pcm->mmap_control = multi->slaves[0].handle->mmap_control;
	return 0;
}

static int snd_pcm_multi_mmap_data(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_mmap_data(handle, 0);
		snd_pcm_setup_t *setup;
		if (err < 0)
			return err;
		setup = &handle->setup;
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
			snd_pcm_channel_area_t areas[setup->format.channels];
			err = snd_pcm_mmap_get_areas(handle, areas);
			if (err < 0)
				return err;
			err = snd_pcm_areas_silence(areas, 0, setup->format.channels, setup->buffer_size, setup->format.sfmt);
			if (err < 0)
				return err;
		}
	}
	pcm->mmap_data = multi->slaves[0].handle->mmap_data;
	return 0;
}

static int snd_pcm_multi_munmap_status(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}
		
static int snd_pcm_multi_munmap_control(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}
		
static int snd_pcm_multi_munmap_data(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;
	int ret = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_munmap_data(handle);
		if (err < 0)
			ret = err;
	}
	return ret;
}
		
static ssize_t snd_pcm_multi_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int i;

	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		ssize_t frames = snd_pcm_mmap_forward(handle, size);
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
		cmasks[i] = bitset_alloc(multi->slaves[i].channels_total);
	for (i = 0; i < multi->bindings_count; ++i) {
		snd_pcm_multi_bind_t *b = &multi->bindings[i];
		if (bitset_get(cmask, b->client_channel))
			bitset_set(cmasks[b->slave], b->slave_channel);
	}
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		err = snd_pcm_channels_mask(handle, cmasks[i]);
		if (err < 0) {
			for (i = 0; i <= multi->slaves_count; ++i)
				free(cmasks[i]);
			return err;
		}
	}
	bitset_zero(cmask, pcm->setup.format.channels);
	for (i = 0; i < multi->bindings_count; ++i) {
		snd_pcm_multi_bind_t *b = &multi->bindings[i];
		if (bitset_get(cmasks[b->slave], b->slave_channel))
			bitset_set(cmask, b->client_channel);
	}
	for (i = 0; i < multi->slaves_count; ++i)
		free(cmasks[i]);
	return 0;
}
		
int snd_pcm_multi_poll_descriptor(snd_pcm_t *pcm)
{
	snd_pcm_multi_t *multi = pcm->private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_poll_descriptor(handle);
}

static void snd_pcm_multi_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_multi_t *multi = pcm->private;
	unsigned int k;
	fprintf(fp, "Multi PCM\n");
	if (pcm->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	for (k = 0; k < multi->slaves_count; ++k) {
		fprintf(fp, "\nSlave #%d: ", k);
		snd_pcm_dump(multi->slaves[k].handle, fp);
	}
	fprintf(fp, "\nBindings:\n");
	for (k = 0; k < multi->bindings_count; ++k) {
		fprintf(fp, "Channel #%d: slave %d[%d]\n", 
			multi->bindings[k].client_channel,
			multi->bindings[k].slave,
			multi->bindings[k].slave_channel);
	}
}

struct snd_pcm_ops snd_pcm_multi_ops = {
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
	mmap_status: snd_pcm_multi_mmap_status,
	mmap_control: snd_pcm_multi_mmap_control,
	mmap_data: snd_pcm_multi_mmap_data,
	munmap_status: snd_pcm_multi_munmap_status,
	munmap_control: snd_pcm_multi_munmap_control,
	munmap_data: snd_pcm_multi_munmap_data,
};

struct snd_pcm_fast_ops snd_pcm_multi_fast_ops = {
	status: snd_pcm_multi_status,
	state: snd_pcm_multi_state,
	delay: snd_pcm_multi_delay,
	prepare: snd_pcm_multi_prepare,
	start: snd_pcm_multi_start,
	stop: snd_pcm_multi_stop,
	drain: snd_pcm_multi_drain,
	pause: snd_pcm_multi_pause,
	writei: snd_pcm_mmap_writei,
	writen: snd_pcm_mmap_writen,
	readi: snd_pcm_mmap_readi,
	readn: snd_pcm_mmap_readn,
	rewind: snd_pcm_multi_rewind,
	poll_descriptor: snd_pcm_multi_poll_descriptor,
	channels_mask: snd_pcm_multi_channels_mask,
	avail_update: snd_pcm_multi_avail_update,
	mmap_forward: snd_pcm_multi_mmap_forward,
};

int snd_pcm_multi_create(snd_pcm_t **handlep, size_t slaves_count,
			 snd_pcm_t **slaves_handle, size_t *schannels_count,
			 size_t bindings_count,  unsigned int *bindings_cchannel,
			 unsigned int *bindings_slave, unsigned int *bindings_schannel,
			 int close_slaves)
{
	snd_pcm_t *handle;
	snd_pcm_multi_t *multi;
	size_t channels = 0;
	unsigned int i;
	int err;
	int stream;
	char client_map[32] = { 0 };
	char slave_map[32][32] = { { 0 } };

	assert(handlep);
	assert(slaves_count > 0 && slaves_handle && schannels_count);
	assert(bindings_count > 0 && bindings_slave && bindings_cchannel && bindings_schannel);

	multi = calloc(1, sizeof(snd_pcm_multi_t));
	if (!multi) {
		return -ENOMEM;
	}

	stream = slaves_handle[0]->stream;
	
	multi->slaves_count = slaves_count;
	multi->slaves = calloc(slaves_count, sizeof(*multi->slaves));
	multi->bindings_count = bindings_count;
	multi->bindings = calloc(bindings_count, sizeof(*multi->bindings));
	for (i = 0; i < slaves_count; ++i) {
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		assert(slaves_handle[i]->stream == stream);
		slave->handle = slaves_handle[i];
		slave->channels_total = schannels_count[i];
		slave->close_slave = close_slaves;
		if (i != 0)
			snd_pcm_link(slaves_handle[i-1], slaves_handle[i]);
	}
	for (i = 0; i < bindings_count; ++i) {
		snd_pcm_multi_bind_t *bind = &multi->bindings[i];
		assert(bindings_slave[i] < slaves_count);
		assert(bindings_schannel[i] < schannels_count[bindings_slave[i]]);
		bind->client_channel = bindings_cchannel[i];
		bind->slave = bindings_slave[i];
		bind->slave_channel = bindings_schannel[i];
		assert(!slave_map[bindings_slave[i]][bindings_schannel[i]]);
		slave_map[bindings_slave[i]][bindings_schannel[i]] = 1;
		assert(!client_map[bindings_cchannel[i]]);
		client_map[bindings_cchannel[i]] = 1;
		if (bindings_cchannel[i] >= channels)
			channels = bindings_cchannel[i] + 1;
	}
	multi->channels_count = channels;

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		free(multi);
		return -ENOMEM;
	}
	handle->type = SND_PCM_TYPE_MULTI;
	handle->stream = stream;
	handle->mode = multi->slaves[0].handle->mode;
	handle->ops = &snd_pcm_multi_ops;
	handle->op_arg = handle;
	handle->fast_ops = &snd_pcm_multi_fast_ops;
	handle->fast_op_arg = handle;
	handle->private = multi;
	err = snd_pcm_init(handle);
	if (err < 0) {
		snd_pcm_close(handle);
		return err;
	}
	*handlep = handle;
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
	unsigned int *bindings_cchannel = NULL;
	unsigned int *bindings_slave = NULL;
	unsigned int *bindings_schannel = NULL;
	size_t slaves_count = 0;
	size_t bindings_count = 0;
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
		++bindings_count;
	}
	slaves_id = calloc(slaves_count, sizeof(*slaves_id));
	slaves_name = calloc(slaves_count, sizeof(*slaves_name));
	slaves_pcm = calloc(slaves_count, sizeof(*slaves_pcm));
	slaves_channels = calloc(slaves_count, sizeof(*slaves_channels));
	bindings_cchannel = calloc(bindings_count, sizeof(*bindings_cchannel));
	bindings_slave = calloc(bindings_count, sizeof(*bindings_slave));
	bindings_schannel = calloc(bindings_count, sizeof(*bindings_schannel));
	idx = 0;
	snd_config_foreach(i, slave) {
		snd_config_t *m = snd_config_entry(i);
		char *pcm = NULL;
		long channels = -1;
		slaves_id[idx] = snd_config_id(m);
		snd_config_foreach(j, m) {
			snd_config_t *n = snd_config_entry(j);
			if (strcmp(n->id, "comment") == 0)
				continue;
			if (strcmp(n->id, "pcm") == 0) {
				err = snd_config_string_get(n, &pcm);
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
		if (!pcm || channels < 0) {
			err = -EINVAL;
			goto _free;
		}
		slaves_name[idx] = strdup(pcm);
		slaves_channels[idx] = channels;
		++idx;
	}

	idx = 0;
	snd_config_foreach(i, binding) {
		snd_config_t *m = snd_config_entry(i);
		long cchannel = -1, schannel = -1;
		int slave = -1;
		long val;
		char *str;
		snd_config_foreach(j, m) {
			snd_config_t *n = snd_config_entry(j);
			if (strcmp(n->id, "comment") == 0)
				continue;
			if (strcmp(n->id, "client_channel") == 0) {
				err = snd_config_integer_get(n, &cchannel);
				if (err < 0)
					goto _free;
				continue;
			}
			if (strcmp(n->id, "slave") == 0) {
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
			if (strcmp(n->id, "slave_channel") == 0) {
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
		bindings_cchannel[idx] = cchannel;
		bindings_slave[idx] = slave;
		bindings_schannel[idx] = schannel;
		++idx;
	}
	
	for (idx = 0; idx < slaves_count; ++idx) {
		err = snd_pcm_open(&slaves_pcm[idx], slaves_name[idx], stream, mode);
		if (err < 0)
			goto _free;
	}
	err = snd_pcm_multi_create(pcmp, slaves_count, slaves_pcm,
				   slaves_channels,
				   bindings_count, bindings_cchannel,
				   bindings_slave, bindings_schannel,
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
	if (bindings_cchannel)
		free(bindings_cchannel);
	if (bindings_slave)
		free(bindings_slave);
	if (bindings_schannel)
		free(bindings_schannel);
	return err;
}

