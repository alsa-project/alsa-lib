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
#include <sys/shm.h>
#include <pthread.h>
#include "pcm_local.h"
#include "list.h"

static LIST_HEAD(slaves);
static pthread_mutex_t slaves_mutex = PTHREAD_MUTEX_INITIALIZER;
char *slaves_mutex_holder;

#define _S(x) #x
#define S(x) _S(x)

#if 1
#define Pthread_mutex_lock(mutex) pthread_mutex_lock(mutex)
#define Pthread_mutex_unlock(mutex) pthread_mutex_unlock(mutex)
#else
#define Pthread_mutex_lock(mutex) \
do { \
  int err = pthread_mutex_trylock(mutex); \
  if (err == EBUSY) { \
    fprintf(stderr, "lock " #mutex " is busy (%s): waiting in " __FUNCTION__ "\n", *(mutex##_holder)); \
    pthread_mutex_lock(mutex); \
    fprintf(stderr, "... got\n"); \
  } \
  *(mutex##_holder) = __FUNCTION__; \
} while (0)

#define Pthread_mutex_unlock(mutex) \
do { \
  *(mutex##_holder) = 0; \
  pthread_mutex_unlock(mutex); \
} while (0)
#endif

typedef struct {
	struct list_head clients;
	struct list_head list;
	snd_pcm_t *pcm;
	int sformat;
	int srate;
	size_t channels_count;
	size_t open_count;
	size_t setup_count;
	size_t mmap_count;
	size_t prepared_count;
	size_t running_count;
	size_t safety_threshold;
	size_t silence_frames;
	size_t hw_ptr;
	int poll[2];
	int polling;
	pthread_t thread;
	pthread_mutex_t mutex;
	char *mutex_holder;
	pthread_cond_t poll_cond;
} snd_pcm_share_slave_t;

typedef struct {
	struct list_head list;
	snd_pcm_t *pcm;
	snd_pcm_share_slave_t *slave;
	size_t channels_count;
	int *slave_channels;
	int xfer_mode;
	int xrun_mode;
	size_t avail_min;
	int async_sig;
	pid_t async_pid;
	int drain_silenced;
	struct timeval trigger_time;
	int state;
	size_t hw_ptr;
	size_t appl_ptr;
	int ready;
	int client_socket;
	int slave_socket;
	void *stopped_data;
} snd_pcm_share_t;

static void _snd_pcm_share_stop(snd_pcm_t *pcm, int state);

static size_t snd_pcm_share_slave_avail(snd_pcm_share_slave_t *slave)
{
	ssize_t avail;
	snd_pcm_t *pcm = slave->pcm;
  	avail = slave->hw_ptr - *pcm->appl_ptr;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		avail += pcm->setup.buffer_size;
	if (avail < 0)
		avail += pcm->setup.boundary;
	return avail;
}

/* Warning: take the mutex before to call this */
/* Return number of frames to mmap_forward the slave */
static size_t _snd_pcm_share_slave_forward(snd_pcm_share_slave_t *slave)
{
	struct list_head *i;
	size_t buffer_size, boundary;
	size_t slave_appl_ptr;
	ssize_t frames, safety_frames;
	ssize_t min_frames, max_frames;
	size_t avail, slave_avail;
	size_t slave_hw_avail;
	slave_avail = snd_pcm_share_slave_avail(slave);
	boundary = slave->pcm->setup.boundary;
	buffer_size = slave->pcm->setup.buffer_size;
	min_frames = slave_avail;
	max_frames = 0;
	slave_appl_ptr = *slave->pcm->appl_ptr;
	for (i = slave->clients.next; i != &slave->clients; i = i->next) {
		snd_pcm_share_t *share = list_entry(i, snd_pcm_share_t, list);
		snd_pcm_t *pcm = share->pcm;
		switch (share->state) {
		case SND_PCM_STATE_RUNNING:
			break;
		case SND_PCM_STATE_DRAINING:
			if (pcm->stream != SND_PCM_STREAM_PLAYBACK)
				continue;
			break;
		default:
			continue;
		}
		avail = snd_pcm_mmap_avail(pcm);
		frames = slave_avail - avail;
		if (frames > max_frames)
			max_frames = frames;
		if (share->state != SND_PCM_STATE_RUNNING)
			continue;
		if (frames < min_frames)
			min_frames = frames;
	}
	if (max_frames == 0)
		return 0;
	frames = min_frames;
	/* Slave xrun prevention */
	slave_hw_avail = buffer_size - slave_avail;
	safety_frames = slave->safety_threshold - slave_hw_avail;
	if (safety_frames > 0 &&
	    frames < safety_frames) {
		/* Avoid to pass over the last */
		if (max_frames < safety_frames)
			frames = max_frames;
		else
			frames = safety_frames;
	}
	if (frames < 0)
		return 0;
	return frames;
}


/* 
   - stop PCM on xrun
   - update poll status
   - draining silencing
   - return distance in frames to next event
*/
static size_t _snd_pcm_share_missing(snd_pcm_t *pcm, int slave_xrun)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	snd_pcm_t *spcm = slave->pcm;
	size_t buffer_size = spcm->setup.buffer_size;
	int ready = 1, running = 0;
	size_t avail = 0, slave_avail;
	ssize_t hw_avail;
	size_t missing = INT_MAX;
	ssize_t ready_missing;
	//	printf("state=%d hw_ptr=%d appl_ptr=%d slave appl_ptr=%d safety=%d silence=%d\n", share->state, slave->hw_ptr, share->appl_ptr, *slave->pcm->appl_ptr, slave->safety_threshold, slave->silence_frames);
	switch (share->state) {
	case SND_PCM_STATE_RUNNING:
		break;
	case SND_PCM_STATE_DRAINING:
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
			break;
		/* Fall through */
	default:
		return INT_MAX;
	}
	if (slave_xrun && pcm->setup.xrun_mode != SND_PCM_XRUN_NONE) {
		_snd_pcm_share_stop(pcm, SND_PCM_STATE_XRUN);
		goto update_poll;
	}
	share->hw_ptr = slave->hw_ptr;
	avail = snd_pcm_mmap_avail(pcm);
	hw_avail = buffer_size - avail;
	slave_avail = snd_pcm_share_slave_avail(slave);
	if (avail < slave_avail) {
		/* Some frames need still to be transferred */
		ssize_t slave_hw_avail = buffer_size - slave_avail;
		ssize_t safety_missing = slave_hw_avail - slave->safety_threshold;
		if (safety_missing < 0) {
			ssize_t err;
			ssize_t frames = slave_avail - avail;
			if (-safety_missing <= frames) {
				frames = -safety_missing;
				missing = 1;
			}
			err = snd_pcm_mmap_forward(spcm, frames);
			assert(err == frames);
			slave_avail -= frames;
		} else {
			if (safety_missing == 0)
				missing = 1;
			else
				missing = safety_missing;
		}
	}
	switch (share->state) {
	case SND_PCM_STATE_DRAINING:
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
			if (hw_avail <= 0) {
				_snd_pcm_share_stop(pcm, SND_PCM_STATE_SETUP);
				break;
			}
			if ((size_t)hw_avail < missing)
				missing = hw_avail;
			running = 1;
			ready = 0;
		}
		break;
	case SND_PCM_STATE_RUNNING:
		if (pcm->setup.xrun_mode != SND_PCM_XRUN_NONE) {
			if (hw_avail <= 0) {
				_snd_pcm_share_stop(pcm, SND_PCM_STATE_XRUN);
				break;
			}
			if ((size_t)hw_avail < missing)
				missing = hw_avail;
		}
		ready_missing = share->avail_min - avail;
		if (ready_missing > 0) {
			ready = 0;
			if ((size_t)ready_missing < missing)
				missing = ready_missing;
		}
		running = 1;
		break;
	}

 update_poll:
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
	if (!running)
		return INT_MAX;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK &&
	    share->state == SND_PCM_STATE_DRAINING &&
	    !share->drain_silenced) {
		/* drain silencing */
		if (avail >= slave->silence_frames) {
			size_t offset = share->appl_ptr % buffer_size;
			size_t xfer = 0;
			size_t size = slave->silence_frames;
			while (xfer < size) {
				size_t frames = size - xfer;
				size_t cont = buffer_size - offset;
				if (cont < frames)
					frames = cont;
				snd_pcm_areas_silence(pcm->running_areas, offset, pcm->setup.format.channels, frames, pcm->setup.format.sfmt);
				offset += frames;
				if (offset >= buffer_size)
					offset = 0;
				xfer += frames;
			}
			share->drain_silenced = 1;
		} else {
			size_t silence_missing;
			silence_missing = slave->silence_frames - avail;
			if (silence_missing < missing)
				missing = silence_missing;
		}
	}
	//	printf("missing=%d\n", missing);
	return missing;
}

static size_t _snd_pcm_share_slave_missing(snd_pcm_share_slave_t *slave)
{
	size_t missing = INT_MAX;
	struct list_head *i;
	ssize_t avail = snd_pcm_avail_update(slave->pcm);
	int slave_xrun = (avail == -EPIPE);
	slave->hw_ptr = *slave->pcm->hw_ptr;
	for (i = slave->clients.next; i != &slave->clients; i = i->next) {
		snd_pcm_share_t *share = list_entry(i, snd_pcm_share_t, list);
		snd_pcm_t *pcm = share->pcm;
		size_t m = _snd_pcm_share_missing(pcm, slave_xrun);
		if (m < missing)
			missing = m;
	}
	return missing;
}

void *snd_pcm_share_slave_thread(void *data)
{
	snd_pcm_share_slave_t *slave = data;
	snd_pcm_t *spcm = slave->pcm;
	struct pollfd pfd[2];
	int err;
	pfd[0].fd = slave->poll[0];
	pfd[0].events = POLLIN;
	pfd[1].fd = snd_pcm_poll_descriptor(spcm);
	pfd[1].events = POLLIN | POLLOUT;
	Pthread_mutex_lock(&slave->mutex);
	err = pipe(slave->poll);
	assert(err >= 0);
	while (slave->open_count > 0) {
		size_t missing;
		//		printf("begin min_missing\n");
		missing = _snd_pcm_share_slave_missing(slave);
		//		printf("min_missing=%d\n", missing);
		if (missing < INT_MAX) {
			size_t hw_ptr;
			ssize_t avail_min;
			hw_ptr = slave->hw_ptr + missing;
			hw_ptr += spcm->setup.frag_size - 1;
			if (hw_ptr >= spcm->setup.boundary)
				hw_ptr -= spcm->setup.boundary;
			hw_ptr -= hw_ptr % spcm->setup.frag_size;
			avail_min = hw_ptr - *spcm->appl_ptr;
			if (spcm->stream == SND_PCM_STREAM_PLAYBACK)
				avail_min += spcm->setup.buffer_size;
			if (avail_min < 0)
				avail_min += spcm->setup.boundary;
			// printf("avail_min=%d\n", avail_min);
			if ((size_t)avail_min != spcm->setup.avail_min)
				snd_pcm_set_avail_min(spcm, avail_min);
			slave->polling = 1;
			Pthread_mutex_unlock(&slave->mutex);
			err = poll(pfd, 2, -1);
			Pthread_mutex_lock(&slave->mutex);
			if (pfd[0].revents & POLLIN) {
				char buf[1];
				read(pfd[0].fd, buf, 1);
			}
		} else {
			slave->polling = 0;
			pthread_cond_wait(&slave->poll_cond, &slave->mutex);
		}
	}
	Pthread_mutex_unlock(&slave->mutex);
	return NULL;
}

static void _snd_pcm_share_update(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	snd_pcm_t *spcm = slave->pcm;
	size_t missing;
	ssize_t avail = snd_pcm_avail_update(spcm);
	slave->hw_ptr = *slave->pcm->hw_ptr;
	missing = _snd_pcm_share_missing(pcm, avail == -EPIPE);
	if (!slave->polling) {
		pthread_cond_signal(&slave->poll_cond);
		return;
	}
	if (missing < INT_MAX) {
		size_t hw_ptr;
		ssize_t avail_min;
		hw_ptr = slave->hw_ptr + missing;
		hw_ptr += spcm->setup.frag_size - 1;
		if (hw_ptr >= spcm->setup.boundary)
			hw_ptr -= spcm->setup.boundary;
		hw_ptr -= hw_ptr % spcm->setup.frag_size;
		avail_min = hw_ptr - *spcm->appl_ptr;
		if (spcm->stream == SND_PCM_STREAM_PLAYBACK)
			avail_min += spcm->setup.buffer_size;
		if (avail_min < 0)
			avail_min += spcm->setup.boundary;
		if ((size_t)avail_min < spcm->setup.avail_min)
			snd_pcm_set_avail_min(spcm, avail_min);
	}
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
	if (slave->sformat >= 0) {
		if ((req_mask & SND_PCM_PARAMS_SFMT) &&
		    info->req.format.sfmt != slave->sformat) {
			info->req.fail_mask |= SND_PCM_PARAMS_SFMT;
			info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
			return -EINVAL;
		}
		info->req.format.sfmt = slave->sformat;
		info->req_mask |= SND_PCM_PARAMS_SFMT;
	}
	if (slave->srate >= 0) {
		info->req.format.rate = slave->srate;
		info->req_mask |= SND_PCM_PARAMS_RATE;
	}

	info->req_mask |= SND_PCM_PARAMS_CHANNELS;
	info->req.format.channels = slave->channels_count;
	err = snd_pcm_params_info(slave->pcm, info);
	info->req.format.channels = channels;
	info->req_mask = req_mask;
	Pthread_mutex_lock(&slave->mutex);
	if (slave->setup_count > 1 || 
	    (slave->setup_count == 1 && !pcm->valid_setup)) {
		snd_pcm_setup_t *s = &slave->pcm->setup;
		if ((req_mask & SND_PCM_PARAMS_SFMT) &&
		    info->req.format.sfmt != s->format.sfmt) {
			info->req.fail_mask |= SND_PCM_PARAMS_SFMT;
			info->req.fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
			err = -EINVAL;
			goto _end;
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
 _end:
	Pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_mmap(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	snd_pcm_mmap_info_t *i;
	size_t count;
	int err = 0;
	Pthread_mutex_lock(&slave->mutex);
	if (slave->mmap_count == 0) {
		err = snd_pcm_mmap(slave->pcm);
		if (err < 0)
			goto _end;
		if (slave->pcm->stream == SND_PCM_STREAM_PLAYBACK)
			snd_pcm_areas_silence(slave->pcm->running_areas, 0, slave->pcm->setup.format.channels, slave->pcm->setup.buffer_size, slave->pcm->setup.format.sfmt);
	}
	slave->mmap_count++;
	count = slave->pcm->mmap_info_count;
	i = malloc((count + 1) * sizeof(*i));
	if (!i) {
		err = -ENOMEM;
		goto _end;
	}
	err = snd_pcm_alloc_user_mmap(pcm, i);
	if (err < 0) {
		free(i);
		return err;
	}
	share->stopped_data = i->addr;
	memcpy(i + 1, slave->pcm->mmap_info, count * sizeof(*pcm->mmap_info));
	pcm->mmap_info_count = count + 1;
	pcm->mmap_info = i;
 _end:
	Pthread_mutex_unlock(&slave->mutex);
	return 0;
}

static int snd_pcm_share_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	snd_pcm_mmap_info_t *i = pcm->mmap_info;
	int err = 0;
	Pthread_mutex_lock(&slave->mutex);
	slave->mmap_count--;
	if (slave->mmap_count == 0) {
		err = snd_pcm_munmap(slave->pcm);
		if (err < 0)
			goto _end;
	}
	err = snd_pcm_free_mmap(pcm, i);
	if (err < 0)
		goto _end;
	free(i);
	pcm->mmap_info_count = 0;
	pcm->mmap_info = 0;
 _end:
	Pthread_mutex_unlock(&slave->mutex);
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
		ERR("channels requested (%d) differs from configuration (%ld)", channels, (long)share->channels_count);
		return -EINVAL;
	}
	share->xfer_mode = params->xfer_mode;
	share->xrun_mode = params->xrun_mode;
	share->avail_min = params->avail_min;
	Pthread_mutex_lock(&slave->mutex);
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
		snd_pcm_setup_t *ss;
		if (slave->sformat >= 0 &&
		    params->format.sfmt != slave->sformat) {
			params->fail_mask = SND_PCM_PARAMS_SFMT;
			params->fail_reason = SND_PCM_PARAMS_FAIL_INVAL;
			ERR("format requested (%d) differs from configuration (%d)", params->format.sfmt, slave->sformat);
			err = -EINVAL;
			goto _end;
		}
		if (slave->srate >= 0)
			sp.format.rate = slave->srate;
		sp.xfer_mode = SND_PCM_XFER_UNSPECIFIED;
		sp.xrun_mode = SND_PCM_XRUN_NONE;
		sp.format.channels = slave->channels_count;
		err = snd_pcm_params_mmap(slave->pcm, &sp);
		if (err < 0)
			goto _end;
		ss = &slave->pcm->setup;
		/* >= 30 ms */
		slave->safety_threshold = ss->format.rate * 30 / 1000;
		slave->safety_threshold += ss->frag_size - 1;
		slave->safety_threshold -= slave->safety_threshold % ss->frag_size;
		slave->silence_frames = slave->safety_threshold;
	}
	share->state = SND_PCM_STATE_SETUP;
	slave->setup_count++;
 _end:
	Pthread_mutex_unlock(&slave->mutex);
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
	if (share->avail_min > setup->buffer_size)
		share->avail_min = setup->buffer_size;
	setup->avail_min = share->avail_min;
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
	Pthread_mutex_lock(&slave->mutex);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		status->avail = snd_pcm_mmap_playback_avail(pcm);
		if (share->state != SND_PCM_STATE_RUNNING &&
		    share->state != SND_PCM_STATE_DRAINING)
			goto _notrunning;
		d = pcm->setup.buffer_size - status->avail;
	} else {
		status->avail = snd_pcm_mmap_capture_avail(pcm);
		if (share->state != SND_PCM_STATE_RUNNING)
			goto _notrunning;
		d = status->avail;
	}
	err = snd_pcm_delay(slave->pcm, &sd);
	if (err < 0)
		goto _end;
 _notrunning:
	status->delay = sd + d;
	status->state = share->state;
	status->trigger_time = share->trigger_time;
 _end:
	Pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_state(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	return share->state;
}

static int _snd_pcm_share_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	ssize_t sd;
	switch (share->state) {
	case SND_PCM_STATE_XRUN:
		return -EPIPE;
	case SND_PCM_STATE_RUNNING:
		break;
	case SND_PCM_STATE_DRAINING:
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
			break;
		/* Fall through */
	default:
		return -EBADFD;
	}
	err = snd_pcm_delay(slave->pcm, &sd);
	if (err < 0)
		return err;
	*delayp = sd + snd_pcm_mmap_delay(pcm);
	return 0;
}

static int snd_pcm_share_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err;
	Pthread_mutex_lock(&slave->mutex);
	err = _snd_pcm_share_delay(pcm, delayp);
	Pthread_mutex_unlock(&slave->mutex);
	return err;
}

static ssize_t snd_pcm_share_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	ssize_t avail;
	Pthread_mutex_lock(&slave->mutex);
	if (share->state == SND_PCM_STATE_RUNNING) {
		avail = snd_pcm_avail_update(slave->pcm);
		if (avail < 0) {
			Pthread_mutex_unlock(&slave->mutex);
			return avail;
		}
		share->hw_ptr = *slave->pcm->hw_ptr;
	}
	Pthread_mutex_unlock(&slave->mutex);
	avail = snd_pcm_mmap_avail(pcm);
	if ((size_t)avail > pcm->setup.buffer_size)
		return -EPIPE;
	return avail;
}

/* Call it with mutex held */
static ssize_t _snd_pcm_share_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	ssize_t ret = 0;
	ssize_t frames;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK &&
	    share->state == SND_PCM_STATE_RUNNING) {
		frames = *slave->pcm->appl_ptr - share->appl_ptr;
		if (frames > (ssize_t)pcm->setup.buffer_size)
			frames -= pcm->setup.boundary;
		else if (frames < -(ssize_t)pcm->setup.buffer_size)
			frames += pcm->setup.boundary;
		if (frames > 0) {
			/* Latecomer PCM */
			ret = snd_pcm_rewind(slave->pcm, frames);
			if (ret < 0)
				return ret;
		}
	}
	snd_pcm_mmap_appl_forward(pcm, size);
	if (share->state == SND_PCM_STATE_RUNNING) {
		ssize_t frames = _snd_pcm_share_slave_forward(slave);
		if (frames > 0) {
			ssize_t err;
			err = snd_pcm_mmap_forward(slave->pcm, frames);
			assert(err == frames);
		}
		_snd_pcm_share_update(pcm);
	}
	return size;
}

static ssize_t snd_pcm_share_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	ssize_t ret;
	Pthread_mutex_lock(&slave->mutex);
	ret = _snd_pcm_share_mmap_forward(pcm, size);
	Pthread_mutex_unlock(&slave->mutex);
	return ret;
}

static int snd_pcm_share_prepare(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	Pthread_mutex_lock(&slave->mutex);
	if (slave->prepared_count == 0) {
		err = snd_pcm_prepare(slave->pcm);
		if (err < 0)
			goto _end;
	}
	slave->prepared_count++;
	share->hw_ptr = 0;
	share->appl_ptr = 0;
	share->state = SND_PCM_STATE_PREPARED;
 _end:
	Pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_start(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	if (share->state != SND_PCM_STATE_PREPARED)
		return -EBADFD;
	Pthread_mutex_lock(&slave->mutex);
	share->state = SND_PCM_STATE_RUNNING;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		size_t hw_avail = snd_pcm_mmap_playback_hw_avail(pcm);
		size_t xfer = 0;
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
		assert(share->hw_ptr == 0);
		share->hw_ptr = *slave->pcm->hw_ptr;
		share->appl_ptr = *slave->pcm->appl_ptr;
		while (xfer < hw_avail) {
			size_t frames = hw_avail - xfer;
			size_t offset = snd_pcm_mmap_offset(pcm);
			size_t cont = pcm->setup.buffer_size - offset;
			if (cont < frames)
				frames = cont;
			snd_pcm_areas_copy(pcm->stopped_areas, xfer,
					   pcm->running_areas, offset,
					   pcm->setup.format.channels, frames,
					   pcm->setup.format.sfmt);
			xfer += frames;
		}
		snd_pcm_mmap_appl_forward(pcm, hw_avail);
		if (slave->running_count == 0)
			snd_pcm_mmap_forward(slave->pcm, hw_avail);
	}
	if (slave->running_count == 0) {
		err = snd_pcm_start(slave->pcm);
		if (err < 0)
			goto _end;
	}
	slave->running_count++;
	_snd_pcm_share_update(pcm);
	gettimeofday(&share->trigger_time, 0);
 _end:
	Pthread_mutex_unlock(&slave->mutex);
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
	if (err < 0)
		return err;
	if (!pcm->mmap_info)
		return 0;
	switch (pcm->setup.mmap_shape) {
	case SND_PCM_MMAP_INTERLEAVED:
	case SND_PCM_MMAP_COMPLEX:
		setup->stopped_area.addr = share->stopped_data;
		setup->stopped_area.first = channel * pcm->bits_per_sample;
		setup->stopped_area.step = pcm->bits_per_frame;
		break;
	case SND_PCM_MMAP_NONINTERLEAVED:
		setup->stopped_area.addr = share->stopped_data + c * pcm->setup.buffer_size * pcm->bits_per_sample / 8;
		setup->stopped_area.first = 0;
		setup->stopped_area.step = pcm->bits_per_sample;
		break;
	default:
		assert(0);
	}
	return 0;
}

static ssize_t _snd_pcm_share_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	ssize_t n;
	switch (share->state) {
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
		return -EBADFD;
	}
	n = snd_pcm_mmap_hw_avail(pcm);
	assert(n >= 0);
	if (n > 0) {
		if ((size_t)n > frames)
			n = frames;
		frames -= n;
	}
	if (share->state == SND_PCM_STATE_RUNNING &&
	    frames > 0) {
		int ret = snd_pcm_rewind(slave->pcm, frames);
		if (ret < 0)
			return ret;
		n += ret;
	}
	snd_pcm_mmap_appl_backward(pcm, n);
	_snd_pcm_share_update(pcm);
	return n;
}

static ssize_t snd_pcm_share_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	ssize_t ret;
	Pthread_mutex_lock(&slave->mutex);
	ret = _snd_pcm_share_rewind(pcm, frames);
	Pthread_mutex_unlock(&slave->mutex);
	return ret;
}

static int snd_pcm_share_set_avail_min(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	Pthread_mutex_lock(&slave->mutex);
	pcm->setup.avail_min = frames;
	share->avail_min = frames;
	_snd_pcm_share_update(pcm);
	Pthread_mutex_unlock(&slave->mutex);
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
		
/* Warning: take the mutex before to call this */
static void _snd_pcm_share_stop(snd_pcm_t *pcm, int state)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	if (!pcm->mmap_info) {
		/* PCM closing already begun in the main thread */
		return;
	}
	gettimeofday(&share->trigger_time, 0);
	if (pcm->stream == SND_PCM_STREAM_CAPTURE) {
		snd_pcm_areas_copy(pcm->running_areas, 0,
				   pcm->stopped_areas, 0,
				   pcm->setup.format.channels, pcm->setup.buffer_size,
				   pcm->setup.format.sfmt);
	} else if (slave->running_count > 1) {
		int err;
		ssize_t delay;
		snd_pcm_areas_silence(pcm->running_areas, 0, pcm->setup.format.channels,
				      pcm->setup.buffer_size, pcm->setup.format.sfmt);
		err = snd_pcm_delay(slave->pcm, &delay);
		if (err >= 0 && delay > 0)
			snd_pcm_rewind(slave->pcm, delay);
		share->drain_silenced = 0;
	}
	share->state = state;
	slave->prepared_count--;
	slave->running_count--;
	if (slave->running_count == 0) {
		int err = snd_pcm_drop(slave->pcm);
		assert(err >= 0);
	}
}

static int snd_pcm_share_drain(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	Pthread_mutex_lock(&slave->mutex);
	switch (share->state) {
	case SND_PCM_STATE_OPEN:
		err = -EBADFD;
		goto _end;
	case SND_PCM_STATE_PREPARED:
		share->state = SND_PCM_STATE_SETUP;
		goto _end;
	case SND_PCM_STATE_SETUP:
		goto _end;
	}
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		switch (share->state) {
		case SND_PCM_STATE_XRUN:
			share->state = SND_PCM_STATE_SETUP;
			goto _end;
		case SND_PCM_STATE_DRAINING:
		case SND_PCM_STATE_RUNNING:
			share->state = SND_PCM_STATE_DRAINING;
			_snd_pcm_share_update(pcm);
			Pthread_mutex_unlock(&slave->mutex);
			if (!(pcm->mode & SND_PCM_NONBLOCK))
				snd_pcm_wait(pcm, -1);
			return 0;
		}
	} else {
		switch (share->state) {
		case SND_PCM_STATE_RUNNING:
			_snd_pcm_share_stop(pcm, SND_PCM_STATE_DRAINING);
			_snd_pcm_share_update(pcm);
			/* Fall through */
		case SND_PCM_STATE_XRUN:
		case SND_PCM_STATE_DRAINING:
			if (snd_pcm_mmap_capture_avail(pcm) <= 0)
				share->state = SND_PCM_STATE_SETUP;
			else
				share->state = SND_PCM_STATE_DRAINING;
			break;
		}
	}
 _end:
	Pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_drop(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	Pthread_mutex_lock(&slave->mutex);
	switch (share->state) {
	case SND_PCM_STATE_OPEN:
		err = -EBADFD;
		goto _end;
	case SND_PCM_STATE_SETUP:
		break;
	case SND_PCM_STATE_DRAINING:
		if (pcm->stream == SND_PCM_STREAM_CAPTURE) {
			share->state = SND_PCM_STATE_SETUP;
			break;
		}
		/* Fall through */
	case SND_PCM_STATE_RUNNING:
		_snd_pcm_share_stop(pcm, SND_PCM_STATE_SETUP);
		_snd_pcm_share_update(pcm);
		break;
	case SND_PCM_STATE_PREPARED:
	case SND_PCM_STATE_XRUN:
		share->state = SND_PCM_STATE_SETUP;
		break;
	}
	
	share->appl_ptr = share->hw_ptr = 0;
 _end:
	Pthread_mutex_unlock(&slave->mutex);
	return err;
}

static int snd_pcm_share_close(snd_pcm_t *pcm)
{
	snd_pcm_share_t *share = pcm->private;
	snd_pcm_share_slave_t *slave = share->slave;
	int err = 0;
	Pthread_mutex_lock(&slaves_mutex);
	Pthread_mutex_lock(&slave->mutex);
	if (pcm->valid_setup)
		slave->setup_count--;
	slave->open_count--;
	if (slave->open_count == 0) {
		err = snd_pcm_close(slave->pcm);
		pthread_cond_signal(&slave->poll_cond);
		Pthread_mutex_unlock(&slave->mutex);
		err = pthread_join(slave->thread, 0);
		assert(err == 0);
		pthread_mutex_destroy(&slave->mutex);
		pthread_cond_destroy(&slave->poll_cond);
		list_del(&slave->list);
		free(slave);
		list_del(&share->list);
	} else {
		list_del(&share->list);
		Pthread_mutex_unlock(&slave->mutex);
	}
	Pthread_mutex_unlock(&slaves_mutex);
	close(share->client_socket);
	close(share->slave_socket);
	free(share->slave_channels);
	free(share);
	return err;
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

snd_pcm_ops_t snd_pcm_share_ops = {
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
	mmap: snd_pcm_share_mmap,
	munmap: snd_pcm_share_munmap,
};

snd_pcm_fast_ops_t snd_pcm_share_fast_ops = {
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
	channels_mask: snd_pcm_share_channels_mask,
	avail_update: snd_pcm_share_avail_update,
	mmap_forward: snd_pcm_share_mmap_forward,
	set_avail_min: snd_pcm_share_set_avail_min,
};

int snd_pcm_share_open(snd_pcm_t **pcmp, char *name, char *sname,
		       int sformat, int srate,
		       size_t schannels_count,
		       size_t channels_count, int *channels_map,
		       int stream, int mode)
{
	snd_pcm_t *pcm;
	snd_pcm_share_t *share;
	int err;
	struct list_head *i;
	char slave_map[32] = { 0 };
	unsigned int k;
	snd_pcm_share_slave_t *slave = NULL;
	int sd[2];

	assert(pcmp);
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

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(share->slave_channels);
		free(share);
		return -ENOMEM;
	}
	err = socketpair(AF_LOCAL, SOCK_STREAM, 0, sd);
	if (err < 0) {
		free(pcm);
		free(share->slave_channels);
		free(share);
		return -errno;
	}
		
	if (stream == SND_PCM_STREAM_PLAYBACK) {
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
		close(sd[0]);
		close(sd[1]);
		free(pcm);
		free(share->slave_channels);
		free(share);
		return err;
	}

	Pthread_mutex_lock(&slaves_mutex);
	for (i = slaves.next; i != &slaves; i = i->next) {
		snd_pcm_share_slave_t *s = list_entry(i, snd_pcm_share_slave_t, list);
		if (s->pcm->name && strcmp(s->pcm->name, sname) == 0) {
			slave = s;
			break;
		}
	}
	if (!slave) {
		snd_pcm_t *spcm;
		err = snd_pcm_open(&spcm, sname, stream, mode);
		if (err < 0) {
			Pthread_mutex_unlock(&slaves_mutex);
			close(sd[0]);
			close(sd[1]);
			free(pcm);
			free(share->slave_channels);
			free(share);
			return err;
		}
		slave = calloc(1, sizeof(*slave));
		if (!slave) {
			Pthread_mutex_unlock(&slaves_mutex);
			snd_pcm_close(spcm);
			close(sd[0]);
			close(sd[1]);
			free(pcm);
			free(share->slave_channels);
			free(share);
			return err;
		}
		INIT_LIST_HEAD(&slave->clients);
		slave->pcm = spcm;
		slave->channels_count = schannels_count;
		slave->sformat = sformat;
		slave->srate = srate;
		pthread_mutex_init(&slave->mutex, NULL);
		pthread_cond_init(&slave->poll_cond, NULL);
		list_add_tail(&slave->list, &slaves);
		Pthread_mutex_lock(&slave->mutex);
		err = pthread_create(&slave->thread, NULL, snd_pcm_share_slave_thread, slave);
		assert(err == 0);
		Pthread_mutex_unlock(&slaves_mutex);
	} else {
		Pthread_mutex_lock(&slave->mutex);
		Pthread_mutex_unlock(&slaves_mutex);
		for (i = slave->clients.next; i != &slave->clients; i = i->next) {
			snd_pcm_share_t *sh = list_entry(i, snd_pcm_share_t, list);
			unsigned int k;
			for (k = 0; k < sh->channels_count; ++k) {
				if (slave_map[sh->slave_channels[k]]) {
					ERR("Slave channel %d is already in use", sh->slave_channels[k]);
					Pthread_mutex_unlock(&slave->mutex);
					close(sd[0]);
					close(sd[1]);
					free(pcm);
					free(share->slave_channels);
					free(share);
					return -EBUSY;
				}
			}
		}
	}

	share->slave = slave;
	share->pcm = pcm;
	share->client_socket = sd[0];
	share->slave_socket = sd[1];
	share->async_sig = SIGIO;
	share->async_pid = getpid();
	
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_SHARE;
	pcm->stream = stream;
	pcm->mode = mode;
	pcm->mmap_auto = 1;
	pcm->ops = &snd_pcm_share_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_share_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = share;
	pcm->poll_fd = share->client_socket;
	pcm->hw_ptr = &share->hw_ptr;
	pcm->appl_ptr = &share->appl_ptr;

	slave->open_count++;
	list_add_tail(&share->list, &slave->clients);

	Pthread_mutex_unlock(&slave->mutex);

	*pcmp = pcm;
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
			if (err < 0) {
				ERR("Invalid type for sname");
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "sformat") == 0) {
			char *f;
			err = snd_config_string_get(n, &f);
			if (err < 0) {
				ERR("Invalid type for sformat");
				return -EINVAL;
			}
			sformat = snd_pcm_format_value(f);
			if (sformat < 0) {
				ERR("Unknown format %s", f);
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
		if (strcmp(n->id, "srate") == 0) {
			err = snd_config_integer_get(n, &srate);
			if (err < 0) {
				ERR("Invalid type for srate");
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
		ERR("sname is not defined");
		return -EINVAL;
	}
	if (!binding) {
		ERR("binding is not defined");
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
		if ((unsigned)cchannel >= channels_count)
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
	    err = snd_pcm_share_open(pcmp, name, sname, sformat, srate, 
				     schannels_count,
				     channels_count, channels_map, stream, mode);
_free:
	free(channels_map);
	return err;
}
