/*
 *  PCM - Null plugin
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
  
#include <byteswap.h>
#include <limits.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

typedef struct {
	snd_pcm_setup_t setup;
	snd_timestamp_t trigger_time;
	int state;
	size_t appl_ptr;
	size_t hw_ptr;
	int poll_fd;
} snd_pcm_null_t;

static int snd_pcm_null_close(snd_pcm_t *pcm)
{
	snd_pcm_null_t *null = pcm->private;
	close(null->poll_fd);
	free(null);
	return 0;
}

static int snd_pcm_null_nonblock(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int nonblock ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_null_async(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int sig ATTRIBUTE_UNUSED, pid_t pid ATTRIBUTE_UNUSED)
{
	return -ENOSYS;
}

static int snd_pcm_null_info(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_info_t * info)
{
	memset(info, 0, sizeof(*info));
	/* FIXME */
	return 0;
}

static int snd_pcm_null_channel_info(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_channel_info_t * info)
{
	int channel = info->channel;
	memset(info, 0, sizeof(*info));
	info->channel = channel;
	return 0;
}

static int snd_pcm_null_channel_params(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_channel_params_t * params ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_null_channel_setup(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_channel_setup_t * setup)
{
	int channel = setup->channel;
	memset(setup, 0, sizeof(*setup));
	setup->channel = channel;
	return 0;
}

static int snd_pcm_null_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_null_t *null = pcm->private;
	memset(status, 0, sizeof(*status));
	status->state = null->state;
	status->trigger_time = null->trigger_time;
	gettimeofday(&status->tstamp, 0);
	status->avail = pcm->setup.buffer_size;
	status->avail_max = status->avail;
	return 0;
}

static int snd_pcm_null_state(snd_pcm_t *pcm)
{
	snd_pcm_null_t *null = pcm->private;
	return null->state;
}

static int snd_pcm_null_delay(snd_pcm_t *pcm ATTRIBUTE_UNUSED, ssize_t *delayp)
{
	*delayp = 0;
	return 0;
}

static int snd_pcm_null_prepare(snd_pcm_t *pcm)
{
	snd_pcm_null_t *null = pcm->private;
	null->state = SND_PCM_STATE_PREPARED;
	null->appl_ptr = 0;
	null->hw_ptr = 0;
	return 0;
}

static int snd_pcm_null_start(snd_pcm_t *pcm)
{
	snd_pcm_null_t *null = pcm->private;
	assert(null->state == SND_PCM_STATE_PREPARED);
	null->state = SND_PCM_STATE_RUNNING;
	if (pcm->stream == SND_PCM_STREAM_CAPTURE)
		snd_pcm_mmap_appl_forward(pcm, pcm->setup.buffer_size);
	return 0;
}

static int snd_pcm_null_drop(snd_pcm_t *pcm)
{
	snd_pcm_null_t *null = pcm->private;
	assert(null->state != SND_PCM_STATE_OPEN);
	null->state = SND_PCM_STATE_SETUP;
	return 0;
}

static int snd_pcm_null_drain(snd_pcm_t *pcm)
{
	snd_pcm_null_t *null = pcm->private;
	assert(null->state != SND_PCM_STATE_OPEN);
	null->state = SND_PCM_STATE_SETUP;
	return 0;
}

static int snd_pcm_null_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_null_t *null = pcm->private;
	if (enable) {
		if (null->state != SND_PCM_STATE_RUNNING)
			return -EBADFD;
	} else if (null->state != SND_PCM_STATE_PAUSED)
		return -EBADFD;
	null->state = SND_PCM_STATE_PAUSED;
	return 0;
}

static ssize_t snd_pcm_null_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_null_t *null = pcm->private;
	switch (null->state) {
	case SND_PCM_STATE_PREPARED:
	case SND_PCM_STATE_RUNNING:
		snd_pcm_mmap_appl_backward(pcm, frames);
		snd_pcm_mmap_hw_backward(pcm, frames);
		return frames;
	default:
		return -EBADFD;
	}
}

static ssize_t snd_pcm_null_fwd(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_null_t *null = pcm->private;
	switch (null->state) {
	case SND_PCM_STATE_PREPARED:
	case SND_PCM_STATE_RUNNING:
		snd_pcm_mmap_appl_forward(pcm, size);
		snd_pcm_mmap_hw_forward(pcm, size);
		return size;
	default:
		return -EBADFD;
	}
}

static ssize_t snd_pcm_null_writei(snd_pcm_t *pcm, const void *buffer ATTRIBUTE_UNUSED, size_t size)
{
	snd_pcm_null_t *null = pcm->private;
	if (null->state == SND_PCM_STATE_PREPARED &&
	    pcm->setup.start_mode != SND_PCM_START_EXPLICIT) {
		null->state = SND_PCM_STATE_RUNNING;
	}
	return snd_pcm_null_fwd(pcm, size);
}

static ssize_t snd_pcm_null_writen(snd_pcm_t *pcm, void **bufs ATTRIBUTE_UNUSED, size_t size)
{
	snd_pcm_null_t *null = pcm->private;
	if (null->state == SND_PCM_STATE_PREPARED &&
	    pcm->setup.start_mode != SND_PCM_START_EXPLICIT) {
		null->state = SND_PCM_STATE_RUNNING;
	}
	return snd_pcm_null_fwd(pcm, size);
}

static ssize_t snd_pcm_null_readi(snd_pcm_t *pcm, void *buffer ATTRIBUTE_UNUSED, size_t size)
{
	snd_pcm_null_t *null = pcm->private;
	if (null->state == SND_PCM_STATE_PREPARED &&
	    pcm->setup.start_mode != SND_PCM_START_EXPLICIT) {
		null->state = SND_PCM_STATE_RUNNING;
		snd_pcm_mmap_hw_forward(pcm, pcm->setup.buffer_size);
	}
	return snd_pcm_null_fwd(pcm, size);
}

static ssize_t snd_pcm_null_readn(snd_pcm_t *pcm, void **bufs ATTRIBUTE_UNUSED, size_t size)
{
	snd_pcm_null_t *null = pcm->private;
	if (null->state == SND_PCM_STATE_PREPARED &&
	    pcm->setup.start_mode != SND_PCM_START_EXPLICIT) {
		null->state = SND_PCM_STATE_RUNNING;
		snd_pcm_mmap_hw_forward(pcm, pcm->setup.buffer_size);
	}
	return snd_pcm_null_fwd(pcm, size);
}

static ssize_t snd_pcm_null_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	return snd_pcm_null_fwd(pcm, size);
}

static ssize_t snd_pcm_null_avail_update(snd_pcm_t *pcm)
{
	return pcm->setup.buffer_size;
}

static int snd_pcm_null_set_avail_min(snd_pcm_t *pcm, size_t frames)
{
	pcm->setup.buffer_size = frames;
	return 0;
}

static int snd_pcm_null_mmap(snd_pcm_t *pcm)
{
	snd_pcm_mmap_info_t *i;
	int err;
	i = calloc(1, sizeof(*i));
	if (!i)
		return -ENOMEM;
	err = snd_pcm_alloc_user_mmap(pcm, i);
	if (err < 0) {
		free(i);
		return err;
	}
	pcm->mmap_info = i;
	pcm->mmap_info_count = 1;
	return 0;
}

static int snd_pcm_null_munmap(snd_pcm_t *pcm)
{
	int err = snd_pcm_free_mmap(pcm, pcm->mmap_info);
	if (err < 0)
		return err;
	free(pcm->mmap_info);
	pcm->mmap_info_count = 0;
	pcm->mmap_info = 0;
	return 0;
}

static int snd_pcm_null_params_info(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_params_info_t * info)
{
	int sizes = ((info->req_mask & SND_PCM_PARAMS_SFMT) &&
		     (info->req_mask & SND_PCM_PARAMS_CHANNELS));
	info->flags = SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID |
	  SND_PCM_INFO_INTERLEAVED | SND_PCM_INFO_NONINTERLEAVED |
	  SND_PCM_INFO_PAUSE;
	info->formats = ~0;
	info->rates = SND_PCM_RATE_CONTINUOUS | SND_PCM_RATE_8000_192000;
	info->min_rate = 4000;
	info->max_rate = 192000;
	info->min_channels = 1;
	info->max_channels = 32;
	info->min_fragments = 1;
	info->max_fragments = 1024 * 1024;
	if (sizes) {
		info->buffer_size = 1024 * 1024;
		info->min_fragment_size = 1;
		info->max_fragment_size = 1024 * 1024;
		info->fragment_align = 1;
	}
	return 0;
}

static int snd_pcm_null_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_null_t *null = pcm->private;
	snd_pcm_setup_t *s = &null->setup;
	int w = snd_pcm_format_width(s->format.sfmt);
	if (w < 0) {
		params->fail_mask = SND_PCM_PARAMS_SFMT;
		return -EINVAL;
	}
	s->msbits = w;
	s->format = params->format;
	s->start_mode = params->start_mode;
	s->ready_mode = params->ready_mode;
	s->avail_min = params->avail_min;
	s->xfer_mode = params->xfer_mode;
	s->xfer_min = params->xfer_min;
	s->xfer_align = params->xfer_align;
	s->xrun_mode = params->xrun_mode;
	s->mmap_shape = params->mmap_shape;
	s->frag_size = params->frag_size;
	s->frags = s->buffer_size / s->frag_size;
	if (s->frags < 1)
		s->frags = 1;
	s->buffer_size = s->frag_size * s->frags;
	s->boundary = LONG_MAX - LONG_MAX % s->buffer_size;
	s->time = params->time;
	s->rate_master = s->format.rate;
	s->rate_divisor = 1;
	s->mmap_bytes = 0;
	s->fifo_size = 1;
	return 0;
}

static int snd_pcm_null_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_null_t *null = pcm->private;
	*setup = null->setup;
	return 0;
}

static void snd_pcm_null_dump(snd_pcm_t *pcm, FILE *fp)
{
	fprintf(fp, "Null PCM\n");
	if (pcm->valid_setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
}

snd_pcm_ops_t snd_pcm_null_ops = {
	close: snd_pcm_null_close,
	info: snd_pcm_null_info,
	params_info: snd_pcm_null_params_info,
	params: snd_pcm_null_params,
	setup: snd_pcm_null_setup,
	channel_info: snd_pcm_null_channel_info,
	channel_params: snd_pcm_null_channel_params,
	channel_setup: snd_pcm_null_channel_setup,
	dump: snd_pcm_null_dump,
	nonblock: snd_pcm_null_nonblock,
	async: snd_pcm_null_async,
	mmap: snd_pcm_null_mmap,
	munmap: snd_pcm_null_munmap,
};

snd_pcm_fast_ops_t snd_pcm_null_fast_ops = {
	status: snd_pcm_null_status,
	state: snd_pcm_null_state,
	delay: snd_pcm_null_delay,
	prepare: snd_pcm_null_prepare,
	start: snd_pcm_null_start,
	drop: snd_pcm_null_drop,
	drain: snd_pcm_null_drain,
	pause: snd_pcm_null_pause,
	rewind: snd_pcm_null_rewind,
	writei: snd_pcm_null_writei,
	writen: snd_pcm_null_writen,
	readi: snd_pcm_null_readi,
	readn: snd_pcm_null_readn,
	avail_update: snd_pcm_null_avail_update,
	mmap_forward: snd_pcm_null_mmap_forward,
	set_avail_min: snd_pcm_null_set_avail_min,
};

int snd_pcm_null_open(snd_pcm_t **pcmp, char *name, int stream, int mode)
{
	snd_pcm_t *pcm;
	snd_pcm_null_t *null;
	int fd;
	assert(pcmp);
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		fd = open("/dev/null", O_WRONLY);
		if (fd < 0) {
			SYSERR("Cannot open /dev/null");
			return -errno;
		}
	} else {
		fd = open("/dev/full", O_RDONLY);
		if (fd < 0) {
			SYSERR("Cannot open /dev/full");
			return -errno;
		}
	}
	null = calloc(1, sizeof(snd_pcm_null_t));
	if (!null) {
		close(fd);
		return -ENOMEM;
	}
	null->poll_fd = fd;
	null->state = SND_PCM_STATE_OPEN;
	
	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		close(fd);
		free(null);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_NULL;
	pcm->stream = stream;
	pcm->mode = mode;
	pcm->ops = &snd_pcm_null_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_null_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = null;
	pcm->poll_fd = fd;
	pcm->hw_ptr = &null->hw_ptr;
	pcm->appl_ptr = &null->appl_ptr;
	*pcmp = pcm;

	return 0;
}

int _snd_pcm_null_open(snd_pcm_t **pcmp, char *name,
		       snd_config_t *conf, 
		       int stream, int mode)
{
	snd_config_iterator_t i;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		return -EINVAL;
	}
	return snd_pcm_null_open(pcmp, name, stream, mode);
}
