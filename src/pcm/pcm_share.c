/*
 *  PCM - Share
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
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <pthread.h>
#include "pcm_local.h"
#include "list.h"

static LIST_HEAD(slaves);
static pthread_mutex_t slaves_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	struct list_head clients;
	struct list_head list;
	snd_pcm_t *pcm;
	size_t channels_count;
	size_t open_count;
	size_t setup_count;
	size_t prepared_count;
	size_t running_count;
	size_t safety_threshold;
	pthread_t thread;
	pthread_mutex_t mutex;
} snd_pcm_share_slave_t;

typedef struct {
	struct list_head list;
	snd_pcm_t *pcm;
	snd_pcm_share_slave_t *slave;
	size_t channels_count;
	int *slave_channels;
	int xfer_mode;
	int xrun_mode;
	int async_sig;
	pid_t async_pid;
	struct timeval trigger_time;
	size_t draining_silence;
	snd_pcm_mmap_control_t mmap_control;
	snd_pcm_mmap_status_t mmap_status;
	int ready;
	int client_socket;
	int slave_socket;
	void *stopped_data;
} snd_pcm_share_t;


static void snd_pcm_share_interrupt(snd_pcm_share_slave_t *slave)
{
	struct list_head *i;
	pthread_mutex_lock(&slave->mutex);
	/* Update poll status */
	for (i = slave->clients.next; i != &slave->clients; i = i->next) {
		snd_pcm_share_t *share = list_entry(i, snd_pcm_share_t, list);
		snd_pcm_t *pcm = share->pcm;
		int ready;
		switch (share->mmap_status.state) {
		case SND_PCM_STATE_DRAINING:
			if (pcm->stream == SND_PCM_STREAM_CAPTURE)
				ready = 1;
			else {
				if (pcm->mode & SND_PCM_ASYNC)
					kill(share->async_pid, share->async_sig);
				ready = 0;
			}
			break;
		case SND_PCM_STATE_RUNNING:
			if (pcm->mode & SND_PCM_ASYNC)
				kill(share->async_pid, share->async_sig);
			ready = (snd_pcm_mmap_avail(pcm) >= pcm->setup.avail_min);
			break;
		default:
			ready = 1;
		}
		if (ready != share->ready) {
			char buf[1];
			if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
				if (ready)
					read(share->slave_socket, buf, 1);
				else
					write(share->client_socket, buf, 1);
			} else {
				if (ready)
					write(share->slave_socket, buf, 1);
				else
					read(share->client_socket, buf, 1);
			}
			share->ready = ready;
		}
	}
	pthread_mutex_unlock(&slave->mutex);
}


void sigio_handler(int sig ATTRIBUTE_UNUSED)
{
}

void *snd_pcm_share_slave_thread(void *data)
{
	snd_pcm_share_slave_t *slave = data;
	int err;
	struct sigaction act;
	err = snd_pcm_async(slave->pcm, SIGIO, 0);
	assert(err == 0);
	act.sa_handler = sigio_handler;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGIO);
	act.sa_flags = 0;
	err = sigaction(SIGIO, &act, NULL);
	assert(err == 0);
	while (1) {
		pause();
		snd_pcm_share_interrupt(slave);
	}
	return NULL;
}

/* Warning: take the mutex before to call this */
static void snd_pcm_share_stop(snd_pcm_t *pcm, int state)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	share->mmap_status.state = state;
	gettimeofday(&share->trigger_time, 0);
	slave->prepared_count--;
	slave->running_count--;
	if (slave->running_count == 0) {
		int err = snd_pcm_drop(slave->pcm);
		assert(err >= 0);
	}
	if (pcm->stream == SND_PCM_STREAM_CAPTURE) {
		snd_pcm_areas_copy(pcm->running_areas, 0,
				   pcm->stopped_areas, 0,
				   pcm->setup.format.channels, pcm->setup.buffer_size,
				   pcm->setup.format.sfmt);
	}
	share->mmap_status.hw_ptr = slave->pcm->mmap_status->hw_ptr;
	pcm->mmap_status = &share->mmap_status;
}

/* Warning: take the mutex before to call this */
static void snd_pcm_share_slave_forward(snd_pcm_share_slave_t *slave)
{
	struct list_head *i;
	size_t buffer_size, boundary;
	size_t slave_appl_ptr;
	ssize_t frames, safety_frames;
	size_t min_frames, max_frames;
	ssize_t avail;
	int err;
	avail = snd_pcm_avail_update(slave->pcm);
	if (avail == 0)
		return;
	assert(avail > 0);
	boundary = slave->pcm->setup.boundary;
	buffer_size = slave->pcm->setup.buffer_size;
	min_frames = buffer_size;
	max_frames = 0;
	slave_appl_ptr = slave->pcm->mmap_control->appl_ptr;
	for (i = slave->clients.next; i != &slave->clients; i = i->next) {
		snd_pcm_share_t *share = list_entry(i, snd_pcm_share_t, list);
		snd_pcm_t *pcm = share->pcm;
		switch (share->mmap_status.state) {
		case SND_PCM_STATE_RUNNING:
			break;
		case SND_PCM_STATE_DRAINING:
		{
			size_t a, offset;
			if (pcm->stream != SND_PCM_STREAM_PLAYBACK)
				continue;
			a = snd_pcm_mmap_avail(pcm);
			frames = a - share->draining_silence;
			offset = snd_pcm_mmap_offset(pcm);
			offset += share->draining_silence;
			if (offset >= buffer_size)
				offset -= buffer_size;
			while (frames > 0) {
				size_t f = buffer_size - offset;
				if (f > (size_t) frames)
					f = frames;
				snd_pcm_areas_silence(pcm->running_areas, offset,
						      pcm->setup.format.channels, 
						      f, pcm->setup.format.sfmt);
				offset += f;
				if (offset == buffer_size)
					offset = 0;
				frames -= f;
			}
			share->draining_silence = a;
			if (a == buffer_size)
				snd_pcm_share_stop(pcm, SND_PCM_STATE_SETUP);
			break;
		}
		default:
			continue;
		}
		frames = share->mmap_control.appl_ptr - slave_appl_ptr;
		if (frames > (ssize_t)buffer_size)
			frames -= pcm->setup.boundary;
		else if (frames < -(ssize_t)pcm->setup.buffer_size)
			frames += pcm->setup.boundary;
		if (frames < 0) {
			if (snd_pcm_mmap_hw_avail(pcm) <= 0 &&
			    pcm->setup.xrun_mode != SND_PCM_XRUN_NONE)
				snd_pcm_share_stop(pcm, SND_PCM_STATE_XRUN);
			continue;
		}
		if ((size_t)frames < min_frames)
			min_frames = frames;
		if ((size_t)frames > max_frames)
			max_frames = frames;
	}
	if (max_frames == 0)
		return;
	frames = min_frames;
	if (frames > avail)
		frames = avail;
	/* Slave xrun prevention */
	safety_frames = slave->safety_threshold - snd_pcm_mmap_hw_avail(slave->pcm);
	if (safety_frames > 0 &&
	    frames < (ssize_t)safety_frames) {
		/* Avoid to pass over the last */
		if (max_frames < (size_t)safety_frames)
			frames = max_frames;
		else
			frames = safety_frames;
	}
	if (frames > 0) {
		err = snd_pcm_mmap_forward(slave->pcm, frames);
		assert(err == frames);
	}
}

static int snd_pcm_share_close(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	if (share->mmap_status.state == SND_PCM_STATE_RUNNING) {
		if (pcm->mode & SND_PCM_NONBLOCK)
			snd_pcm_drop(pcm);
		else
			snd_pcm_drain(pcm);
	}
	pthread_mutex_lock(&slave->mutex);
	if (pcm->valid_setup)
		slave->setup_count--;
	slave->open_count--;
	if (slave->open_count == 0) {
		pthread_kill(slave->thread, SIGTERM);
		err = snd_pcm_close(slave->pcm);
		list_del(&slave->list);
		pthread_mutex_destroy(&slave->mutex);
		free(slave);
	}
	list_del(&share->list);
	pthread_mutex_unlock(&slave->mutex);
	close(share->client_socket);
	close(share->slave_socket);
	free(share->slave_channels);
	free(share);
	return err;
}

static int snd_pcm_share_nonblock(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int nonblock ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_share_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_share_t *share = pcm->private;
	if (sig)
		share->async_sig = sig;
	else
		share->async_sig = SIGIO;
	if (pid)
		share->async_pid = pid;
	else
		share->async_pid = getpid();
	return -ENOSYS;
}

static int snd_pcm_share_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	snd_pcm_share_t *share = pcm->private;
	return snd_pcm_info(share->slave->pcm, info);
}

static int snd_pcm_share_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t *info)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	unsigned int req_mask = info->req_mask;
	unsigned int channels = info->req.format.channels;
	if ((req_mask & SND_PCM_PARAMS_CHANNELS) &&
	    channels != share->channels_count) {
		info->req.fail_mask |= SND_PCM_PARAMS_CHANNELS;
		info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		return -EINVAL;
	}
	info->req_mask |= SND_PCM_PARAMS_CHANNELS;
	info->req.format.channels = slave->channels_count;
	err = snd_pcm_params_info(slave->pcm, info);
	info->req.format.channels = channels;
	info->req_mask = req_mask;
	if (slave->setup_count > 1 || 
	    (slave->setup_count == 1 && !pcm->valid_setup)) {
		snd_pcm_setup_t *s = &slave->pcm->setup;
		if ((req_mask & SND_PCM_PARAMS_SFMT) &&
		    info->req.format.sfmt != s->format.sfmt) {
			info->req.fail_mask |= SND_PCM_PARAMS_SFMT;
			info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
			return -EINVAL;
		}
		info->formats = 1 << s->format.sfmt;
		info->rates = SND_PCM_RATE_CONTINUOUS;
		info->min_rate = info->max_rate = s->format.rate;
		info->buffer_size = s->buffer_size;
		info->min_fragment_size = info->max_fragment_size = s->frag_size;
		info->min_fragments = info->max_fragments = s->frags;
		info->fragment_align = s->frag_size;
		info->req.fail_mask = 0;
	}

	info->min_channels = info->max_channels = share->channels_count;
	if (info->flags & SND_PCM_INFO_INTERLEAVED) {
		info->flags &= ~SND_PCM_INFO_INTERLEAVED;
		info->flags |= SND_PCM_INFO_COMPLEX;
	}
	return err;
}

static int snd_pcm_share_params(snd_pcm_t *pcm, snd_pcm_params_t *params)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	unsigned int channels = params->format.channels;
	int err = 0;
	if (channels != share->channels_count) {
		params->fail_mask = SND_PCM_PARAMS_CHANNELS;
		params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
		ERR("channels requested (%d) differs from configuration (%d)", channels, share->channels_count);
		return -EINVAL;
	}
	share->xfer_mode = params->xfer_mode;
	share->xrun_mode = params->xrun_mode;
	pthread_mutex_lock(&slave->mutex);
	if (slave->setup_count > 1 || 
	    (slave->setup_count == 1 && !pcm->valid_setup)) {
		snd_pcm_setup_t *s = &slave->pcm->setup;
		if (params->format.sfmt != s->format.sfmt) {
			ERR("slave is already running with different format");
			params->fail_mask |= SND_PCM_PARAMS_SFMT;
		}
		if (params->fail_mask) {
			params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
			err = -EINVAL;
			goto _end;
		}
	} else {
		snd_pcm_params_t sp = *params;
		if (slave->pcm->mmap_data) {
			err = snd_pcm_munmap_data(slave->pcm);
			if (err < 0)
				goto _end;
		}
		sp.xfer_mode = SND_PCM_XFER_UNSPECIFIED;
		sp.xrun_mode = SND_PCM_XRUN_NONE;
		sp.format.channels = slave->channels_count;
		err = snd_pcm_params(slave->pcm, &sp);
		snd_pcm_mmap_data(slave->pcm, NULL);
		if (err < 0)
			goto _end;
		if (slave->pcm->stream == SND_PCM_STREAM_PLAYBACK)
			snd_pcm_areas_silence(slave->pcm->running_areas, 0, slave->pcm->setup.format.channels, slave->pcm->setup.buffer_size, slave->pcm->setup.format.sfmt);
	}
	share->mmap_status.state = SND_PCM_STATE_SETUP;
	slave->setup_count++;
 _end:
	pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_setup(snd_pcm_t *pcm, snd_pcm_setup_t *setup)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err;
	err = snd_pcm_setup(slave->pcm, setup);
	if (err < 0)
		return err;
	setup->xrun_mode = share->xrun_mode;
	setup->format.channels = share->channels_count;
	if (share->xfer_mode == SND_PCM_XFER_UNSPECIFIED)
		setup->xfer_mode = SND_PCM_XFER_NONINTERLEAVED;
	else
		setup->xfer_mode = share->xfer_mode;
	if (setup->mmap_shape != SND_PCM_MMAP_INTERLEAVED)
		setup->mmap_shape = SND_PCM_MMAP_COMPLEX;
	return 0;
}

static int snd_pcm_share_status(snd_pcm_t *pcm, snd_pcm_status_t *status)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	ssize_t sd = 0, d = 0;
	pthread_mutex_lock(&slave->mutex);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		status->avail = snd_pcm_mmap_playback_avail(pcm);
		if (share->mmap_status.state != SND_PCM_STATE_RUNNING &&
		    share->mmap_status.state != SND_PCM_STATE_DRAINING)
			goto _notrunning;
		d = pcm->setup.buffer_size - status->avail;
	} else {
		status->avail = snd_pcm_mmap_capture_avail(pcm);
		if (share->mmap_status.state != SND_PCM_STATE_RUNNING)
			goto _notrunning;
		d = status->avail;
	}
	err = snd_pcm_delay(slave->pcm, &sd);
	if (err < 0)
		goto _end;
 _notrunning:
	status->delay = sd + d;
	status->state = share->mmap_status.state;
	status->trigger_time = share->trigger_time;
 _end:
	pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_state(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	return share->mmap_status.state;
}

static int snd_pcm_share_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	ssize_t sd;
	pthread_mutex_lock(&slave->mutex);
	switch (share->mmap_status.state) {
	case SND_PCM_STATE_XRUN:
		err = -EPIPE;
		goto _end;
	case SND_PCM_STATE_RUNNING:
		break;
	case SND_PCM_STATE_DRAINING:
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
			break;
		/* Fall through */
	default:
		err = -EBADFD;
		goto _end;
	}
	err = snd_pcm_delay(slave->pcm, &sd);
	if (err < 0)
		goto _end;
	*delayp = sd + snd_pcm_mmap_delay(pcm);
 _end:
	pthread_mutex_unlock(&slave->mutex);
	return 0;
}

static ssize_t snd_pcm_share_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	ssize_t ret = 0;
	pthread_mutex_lock(&slave->mutex);
	ret = snd_pcm_avail_update(slave->pcm);
	if (ret == -EPIPE) {
	  	struct list_head *i;
		for (i = slave->clients.next; i != &slave->clients; i = i->next) {
			snd_pcm_share_t *share = list_entry(i, snd_pcm_share_t, list);
			snd_pcm_t *pcm = share->pcm;
			if (share->mmap_status.state == SND_PCM_STATE_RUNNING &&
			    pcm->setup.xrun_mode != SND_PCM_XRUN_NONE)
				snd_pcm_share_stop(pcm, SND_PCM_STATE_XRUN);
		}
	}
	pthread_mutex_unlock(&slave->mutex);
	if (ret >= 0) {
		ret = snd_pcm_mmap_avail(pcm);
		if ((size_t)ret > pcm->setup.buffer_size)
			return -EPIPE;
	}
	return ret;
}

static ssize_t snd_pcm_share_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	ssize_t ret = 0;
	ssize_t frames;
	pthread_mutex_lock(&slave->mutex);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK &&
	    share->mmap_status.state == SND_PCM_STATE_RUNNING) {
		frames = slave->pcm->mmap_control->appl_ptr - share->mmap_control.appl_ptr;
		if (frames > (ssize_t)pcm->setup.buffer_size)
			frames -= pcm->setup.boundary;
		else if (frames < -(ssize_t)pcm->setup.buffer_size)
			frames += pcm->setup.boundary;
		if (frames > 0) {
			/* Latecomer PCM */
			ret = snd_pcm_rewind(slave->pcm, frames);
			if (ret < 0) {
				pthread_mutex_unlock(&slave->mutex);
				return ret;
			}
			size += ret;
		}
	}
	snd_pcm_mmap_appl_forward(pcm, size);
	snd_pcm_share_slave_forward(share->slave);
	pthread_mutex_unlock(&slave->mutex);
	return size;
}

static int snd_pcm_share_prepare(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	pthread_mutex_lock(&slave->mutex);
	if (slave->prepared_count == 0) {
		err = snd_pcm_prepare(slave->pcm);
		if (err < 0)
			goto _end;
	}
	slave->prepared_count++;
	share->mmap_status.hw_ptr = 0;
	share->mmap_control.appl_ptr = 0;
	share->mmap_status.state = SND_PCM_STATE_PREPARED;
 _end:
	pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_start(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	if (share->mmap_status.state != SND_PCM_STATE_PREPARED)
		return -EBADFD;
	pthread_mutex_lock(&slave->mutex);
	share->mmap_status.state = SND_PCM_STATE_RUNNING;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		size_t hw_avail = snd_pcm_mmap_playback_hw_avail(pcm);
		if (hw_avail == 0) {
			err = -EPIPE;
			goto _end;
		}
		if (slave->running_count) {
			ssize_t sd;
			err = snd_pcm_delay(slave->pcm, &sd);
			if (err < 0)
				goto _end;
			err = snd_pcm_rewind(slave->pcm, sd);
			if (err < 0)
				goto _end;
		}
		assert(share->mmap_status.hw_ptr == 0);
		/* Use the same mmap_status of slave */
		pcm->mmap_status = slave->pcm->mmap_status;
		share->mmap_control.appl_ptr = slave->pcm->mmap_control->appl_ptr;
		snd_pcm_areas_copy(pcm->stopped_areas, 0,
				   pcm->running_areas, snd_pcm_mmap_offset(pcm),
				   pcm->setup.format.channels, hw_avail,
				   pcm->setup.format.sfmt);
		snd_pcm_mmap_forward(pcm, pcm->setup.buffer_size);
	}
	if (slave->running_count == 0) {
		err = snd_pcm_start(slave->pcm);
		if (err < 0)
			goto _end;
	}
	slave->running_count++;
	gettimeofday(&share->trigger_time, 0);
 _end:
	pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_drop(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	pthread_mutex_lock(&slave->mutex);
	switch (share->mmap_status.state) {
	case SND_PCM_STATE_OPEN:
		err = -EBADFD;
		goto _end;
	case SND_PCM_STATE_SETUP:
		goto _end;
	case SND_PCM_STATE_DRAINING:
		if (pcm->stream == SND_PCM_STREAM_CAPTURE) {
			share->mmap_status.state = SND_PCM_STATE_SETUP;
			break;
		}
		/* Fall through */
	case SND_PCM_STATE_RUNNING:
		snd_pcm_share_stop(pcm, SND_PCM_STATE_SETUP);
		break;
	case SND_PCM_STATE_PREPARED:
	case SND_PCM_STATE_XRUN:
		share->mmap_status.state = SND_PCM_STATE_SETUP;
		break;
	}
	
	if (slave->running_count > 0 &&
	    pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		ssize_t delay;
		err = snd_pcm_delay(pcm, &delay);
		if (err < 0)
			goto _end;
		if (delay > 0) {
			err = snd_pcm_rewind(pcm, delay);
			if (err < 0)
				goto _end;
		}
		snd_pcm_areas_silence(pcm->running_areas, 0, pcm->setup.format.channels,
				      pcm->setup.buffer_size, pcm->setup.format.sfmt);
		snd_pcm_mmap_forward(pcm, pcm->setup.buffer_size);
	}
	share->mmap_control.appl_ptr = share->mmap_status.hw_ptr = 0;
 _end:
	pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_drain(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	pthread_mutex_lock(&slave->mutex);
	switch (share->mmap_status.state) {
	case SND_PCM_STATE_OPEN:
		err = -EBADFD;
		goto _end;
	case SND_PCM_STATE_PREPARED:
		share->mmap_status.state = SND_PCM_STATE_SETUP;
		break;
	case SND_PCM_STATE_SETUP:
	case SND_PCM_STATE_DRAINING:
		goto _end;
	case SND_PCM_STATE_XRUN:
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
			share->mmap_status.state = SND_PCM_STATE_SETUP;
			break;
		}
		/* Fall through */
	case SND_PCM_STATE_RUNNING:
		if (snd_pcm_mmap_avail(pcm) <= 0) {
			share->mmap_status.state = SND_PCM_STATE_SETUP;
			break;
		}
		share->draining_silence = 0;
		share->mmap_status.state = SND_PCM_STATE_DRAINING;
		break;
	}
 _end:
	pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_pause(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int enable ATTRIBUTE_UNUSED)
{
	return -ENOSYS;
}

static int snd_pcm_share_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	unsigned int channel = info->channel;
	int c = share->slave_channels[channel];
	int err;
	info->channel = c;
	err = snd_pcm_channel_info(slave->pcm, info);
	info->channel = channel;
	return err;
}

static int snd_pcm_share_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	unsigned int channel = params->channel;
	int c = share->slave_channels[channel];
	int err;
	params->channel = c;
	err = snd_pcm_channel_params(slave->pcm, params);
	params->channel = channel;
	return err;
}

static int snd_pcm_share_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	unsigned int channel = setup->channel;
	int c = share->slave_channels[channel];
	int err;
	setup->channel = c;
	err = snd_pcm_channel_setup(slave->pcm, setup);
	setup->channel = channel;
	return err;
}

static ssize_t snd_pcm_share_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int ret = -EBADFD;
	ssize_t n;
	switch (share->mmap_status.state) {
	case SND_PCM_STATE_RUNNING:
		break;
	case SND_PCM_STATE_PREPARED:
		if (pcm->stream != SND_PCM_STREAM_PLAYBACK)
			return -EBADFD;
		break;
	case SND_PCM_STATE_DRAINING:
		if (pcm->stream != SND_PCM_STREAM_CAPTURE)
			return -EBADFD;
		break;
	case SND_PCM_STATE_XRUN:
		return -EPIPE;
	default:
		goto _err;
	}
	n = snd_pcm_mmap_hw_avail(pcm);
	assert(n >= 0);
	if (n > 0) {
		if ((size_t)n > frames)
			n = frames;
		frames -= n;
	}
	if (share->mmap_status.state == SND_PCM_STATE_RUNNING &&
	    frames > 0) {
		pthread_mutex_lock(&slave->mutex);
		ret = snd_pcm_rewind(slave->pcm, frames);
		pthread_mutex_unlock(&slave->mutex);
		if (ret < 0) {
			if (n <= 0)
				return ret;
			goto _end;
		}
		n += ret;
	}
 _end:
	snd_pcm_mmap_appl_backward(pcm, n);
	ret = n;
 _err:
	return ret;
}

static int snd_pcm_share_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	pcm->mmap_status = &share->mmap_status;
	return 0;
}

static int snd_pcm_share_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	pcm->mmap_control = &share->mmap_control;
	return 0;
}

static int snd_pcm_share_mmap_data(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	pcm->mmap_data = slave->pcm->mmap_data;
	return 0;
}

static int snd_pcm_share_munmap_status(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}
		
static int snd_pcm_share_munmap_control(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}
		
static int snd_pcm_share_munmap_data(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}
		
static int snd_pcm_share_channels_mask(snd_pcm_t *pcm, bitset_t *cmask)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	unsigned int i;
	bitset_t m[bitset_size(slave->channels_count)];
	int err = snd_pcm_channels_mask(slave->pcm, m);
	if (err < 0)
		return err;
	for (i = 0; i < share->channels_count; ++i) {
		if (!bitset_get(m, share->slave_channels[i]))
			bitset_reset(cmask, i);
	}
	return 0;
}
		
int snd_pcm_share_poll_descriptor(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	return share->client_socket;
}

static void snd_pcm_share_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	unsigned int k;
	fprintf(fp, "Share PCM\n");
	fprintf(fp, "\nChannel bindings:\n");
	for (k = 0; k < share->channels_count; ++k)
		fprintf(fp, "%d: %d\n", k, share->slave_channels[k]);
	if (pcm->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(slave->pcm, fp);
}

struct snd_pcm_ops snd_pcm_share_ops = {
	close: snd_pcm_share_close,
	info: snd_pcm_share_info,
	params_info: snd_pcm_share_params_info,
	params: snd_pcm_share_params,
	setup: snd_pcm_share_setup,
	channel_info: snd_pcm_share_channel_info,
	channel_params: snd_pcm_share_channel_params,
	channel_setup: snd_pcm_share_channel_setup,
	dump: snd_pcm_share_dump,
	nonblock: snd_pcm_share_nonblock,
	async: snd_pcm_share_async,
	mmap_status: snd_pcm_share_mmap_status,
	mmap_control: snd_pcm_share_mmap_control,
	mmap_data: snd_pcm_share_mmap_data,
	munmap_status: snd_pcm_share_munmap_status,
	munmap_control: snd_pcm_share_munmap_control,
	munmap_data: snd_pcm_share_munmap_data,
};

struct snd_pcm_fast_ops snd_pcm_share_fast_ops = {
	status: snd_pcm_share_status,
	state: snd_pcm_share_state,
	delay: snd_pcm_share_delay,
	prepare: snd_pcm_share_prepare,
	start: snd_pcm_share_start,
	drop: snd_pcm_share_drop,
	drain: snd_pcm_share_drain,
	pause: snd_pcm_share_pause,
	writei: snd_pcm_mmap_writei,
	writen: snd_pcm_mmap_writen,
	readi: snd_pcm_mmap_readi,
	readn: snd_pcm_mmap_readn,
	rewind: snd_pcm_share_rewind,
	poll_descriptor: snd_pcm_share_poll_descriptor,
	channels_mask: snd_pcm_share_channels_mask,
	avail_update: snd_pcm_share_avail_update,
	mmap_forward: snd_pcm_share_mmap_forward,
};

int snd_pcm_share_open(snd_pcm_t **handlep, char *name, char *sname,
		       size_t schannels_count,
		       size_t channels_count, int *channels_map,
		       int stream, int mode)
{
	snd_pcm_t *handle;
	snd_pcm_share_t *share;
	int err;
	struct list_head *i;
	char slave_map[32] = { 0 };
	unsigned int k;
	snd_pcm_share_slave_t *slave = NULL;
	int sd[2];

	assert(handlep);
	assert(channels_count > 0 && sname && channels_map);

	for (k = 0; k < channels_count; ++k) {
		if (channels_map[k] < 0 || channels_map[k] > 31) {
			ERR("Invalid slave channel (%d) in binding", channels_map[k]);
			return -EINVAL;
		}
		if (slave_map[channels_map[k]]) {
			ERR("Repeated slave channel (%d) in binding", channels_map[k]);
			return -EINVAL;
		}
		slave_map[channels_map[k]] = 1;
		assert((unsigned)channels_map[k] < schannels_count);
	}

	share = calloc(1, sizeof(snd_pcm_share_t));
	if (!share)
		return -ENOMEM;

	share->channels_count = channels_count;
	share->slave_channels = calloc(channels_count, sizeof(*share->slave_channels));
	if (!share->slave_channels) {
		free(share);
		return -ENOMEM;
	}
	memcpy(share->slave_channels, channels_map, channels_count * sizeof(*share->slave_channels));

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		free(share->slave_channels);
		free(share);
		return -ENOMEM;
	}
	err = socketpair(AF_LOCAL, SOCK_STREAM, 0, sd);
	if (err >= 0 && stream == SND_PCM_STREAM_PLAYBACK) {
		int bufsize = 1;
		err = setsockopt(sd[0], SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
		if (err >= 0) {
			struct pollfd pfd;
			pfd.fd = sd[0];
			pfd.events = POLLOUT;
			while ((err = poll(&pfd, 1, 0)) == 1) {
				char buf[1];
				err = write(sd[0], buf, 1);
				assert(err != 0);
				if (err != 1)
					break;
			}
		}
	}
	if (err < 0) {
		err = -errno;
		free(handle);
		free(share->slave_channels);
		free(share);
		return err;
	}

	pthread_mutex_lock(&slaves_mutex);
	for (i = slaves.next; i != &slaves; i = i->next) {
		snd_pcm_share_slave_t *s = list_entry(i, snd_pcm_share_slave_t, list);
		if (s->pcm->name && strcmp(s->pcm->name, sname)) {
			slave = s;
			break;
		}
	}
	if (!slave) {
		snd_pcm_t *spcm;
		err = snd_pcm_open(&spcm, sname, stream, mode);
		if (err < 0) {
			pthread_mutex_unlock(&slaves_mutex);
			close(sd[0]);
			close(sd[1]);
			free(handle);
			free(share->slave_channels);
			free(share);
			return err;
		}
		slave = calloc(1, sizeof(*slave));
		if (!slave) {
			pthread_mutex_unlock(&slaves_mutex);
			snd_pcm_close(spcm);
			close(sd[0]);
			close(sd[1]);
			free(handle);
			free(share->slave_channels);
			free(share);
			return err;
		}
		INIT_LIST_HEAD(&slave->clients);
		slave->pcm = spcm;
		slave->channels_count = schannels_count;
		pthread_mutex_init(&slave->mutex, NULL);
		list_add_tail(&slave->list, &slaves);
	}
	pthread_mutex_unlock(&slaves_mutex);

	pthread_mutex_lock(&slave->mutex);
	if (slave->open_count == 0) {
		err = pthread_create(&slave->thread, NULL, snd_pcm_share_slave_thread, slave);
		assert(err == 0);
	}
	slave->open_count++;
	list_add_tail(&share->list, &slave->clients);
	pthread_mutex_unlock(&slave->mutex);

	share->slave = slave;
	share->pcm = handle;
	share->client_socket = sd[0];
	share->slave_socket = sd[1];
	share->async_sig = SIGIO;
	share->async_pid = getpid();
	
	if (name)
		handle->name = strdup(name);
	handle->type = SND_PCM_TYPE_SHARE;
	handle->stream = stream;
	handle->mode = mode;
	handle->ops = &snd_pcm_share_ops;
	handle->op_arg = handle;
	handle->fast_ops = &snd_pcm_share_fast_ops;
	handle->fast_op_arg = handle;
	handle->private = share;
	err = snd_pcm_init(handle);
	if (err < 0) {
		snd_pcm_close(handle);
		return err;
	}
	*handlep = handle;
	return 0;
}

int _snd_pcm_share_open(snd_pcm_t **pcmp, char *name, snd_config_t *conf,
			int stream, int mode)
{
	snd_config_iterator_t i;
	char *sname = NULL;
	snd_config_t *binding = NULL;
	int err;
	unsigned int idx;
	int *channels_map;
	size_t channels_count = 0;
	long schannels_count = -1;
	size_t schannel_max = 0;
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
				ERR("Invalid type for sname");
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "schannels") == 0) {
			err = snd_config_integer_get(n, &schannels_count);
			if (err < 0) {
				ERR("Invalid type for schannels");
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "binding") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				ERR("Invalid type for binding");
				return -EINVAL;
			}
			binding = n;
			continue;
		}
		ERR("Unknown field: %s", n->id);
		return -EINVAL;
	}
	if (!sname) {
		ERR("Missing sname");
		return -EINVAL;
	}
	if (!binding) {
		ERR("Missing binding");
		return -EINVAL;
	}
	snd_config_foreach(i, binding) {
		int cchannel = -1;
		char *p;
		snd_config_t *n = snd_config_entry(i);
		errno = 0;
		cchannel = strtol(n->id, &p, 10);
		if (errno || *p || cchannel < 0) {
			ERR("Invalid client channel in binding: %s", n->id);
			return -EINVAL;
		}
		if ((unsigned)cchannel > channels_count)
			channels_count = cchannel + 1;
	}
	if (channels_count == 0) {
		ERR("No bindings defined");
		return -EINVAL;
	}
	channels_map = calloc(channels_count, sizeof(*channels_map));
	for (idx = 0; idx < channels_count; ++idx)
		channels_map[idx] = -1;

	snd_config_foreach(i, binding) {
		snd_config_t *n = snd_config_entry(i);
		long cchannel;
		long schannel = -1;
		cchannel = strtol(n->id, 0, 10);
		err = snd_config_integer_get(n, &schannel);
		if (err < 0)
			goto _free;
		assert(schannels_count <= 0 || schannel < schannels_count);
		channels_map[cchannel] = schannel;
		if ((unsigned)schannel > schannel_max)
			schannel_max = schannel;
	}
	if (schannels_count <= 0)
		schannels_count = schannel_max + 1;
	    err = snd_pcm_share_open(pcmp, name, sname, schannels_count,
				 channels_count, channels_map, stream, mode);
_free:
	free(channels_map);
	return err;
}
