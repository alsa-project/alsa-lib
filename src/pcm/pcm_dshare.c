/**
 * \file pcm/pcm_dshare.c
 * \ingroup PCM_Plugins
 * \brief PCM Direct Sharing of Channels Plugin Interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 */
/*
 *  PCM - Direct Sharing of Channels
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
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
  
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include "pcm_direct.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_dshare = "";
#endif

static void do_silence(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	const snd_pcm_channel_area_t *dst_areas;
	unsigned int chn, dchn, channels;
	snd_pcm_format_t format;

	dst_areas = snd_pcm_mmap_areas(dshare->spcm);
	channels = dshare->channels;
	format = dshare->shmptr->s.format;
	for (chn = 0; chn < channels; chn++) {
		dchn = dshare->bindings ? dshare->bindings[chn] : chn;
		snd_pcm_area_silence(&dst_areas[dchn], 0, dshare->shmptr->s.buffer_size, format);
	}
}

static void share_areas(snd_pcm_direct_t *dshare,
		      const snd_pcm_channel_area_t *src_areas,
		      const snd_pcm_channel_area_t *dst_areas,
		      snd_pcm_uframes_t src_ofs,
		      snd_pcm_uframes_t dst_ofs,
		      snd_pcm_uframes_t size)
{
	unsigned int chn, dchn, channels;
	snd_pcm_format_t format;

	channels = dshare->channels;
	format = dshare->shmptr->s.format;
	if (dshare->interleaved) {
		unsigned int fbytes = snd_pcm_format_physical_width(format) / 8;
		memcpy(((char *)dst_areas[0].addr) + (dst_ofs * channels * fbytes),
		       ((char *)src_areas[0].addr) + (src_ofs * channels * fbytes),
		       size * channels * fbytes);
	} else {
		for (chn = 0; chn < channels; chn++) {
			dchn = dshare->bindings ? dshare->bindings[chn] : chn;
			snd_pcm_area_copy(&dst_areas[dchn], dst_ofs, &src_areas[chn], src_ofs, size, format);

		}
	}
}

/*
 *  synchronize shm ring buffer with hardware
 */
static void snd_pcm_dshare_sync_area(snd_pcm_t *pcm, snd_pcm_uframes_t size)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	snd_pcm_uframes_t appl_ptr, slave_appl_ptr, transfer;
	const snd_pcm_channel_area_t *src_areas, *dst_areas;
	
	/* get the start of update area */
	appl_ptr = dshare->appl_ptr - size;
	if (appl_ptr > pcm->boundary)
		appl_ptr += pcm->boundary;
	appl_ptr %= pcm->buffer_size;
	/* add sample areas here */
	src_areas = snd_pcm_mmap_areas(pcm);
	dst_areas = snd_pcm_mmap_areas(dshare->spcm);
	slave_appl_ptr = dshare->slave_appl_ptr % dshare->shmptr->s.buffer_size;
	dshare->slave_appl_ptr += size;
	dshare->slave_appl_ptr %= dshare->shmptr->s.boundary;
	while (size > 0) {
		transfer = appl_ptr + size > pcm->buffer_size ? pcm->buffer_size - appl_ptr : size;
		transfer = slave_appl_ptr + transfer > dshare->shmptr->s.buffer_size ? dshare->shmptr->s.buffer_size - slave_appl_ptr : transfer;
		size -= transfer;
		share_areas(dshare, src_areas, dst_areas, appl_ptr, slave_appl_ptr, transfer);
		slave_appl_ptr += transfer;
		slave_appl_ptr %= dshare->shmptr->s.buffer_size;
		appl_ptr += transfer;
		appl_ptr %= pcm->buffer_size;
	}
}

/*
 *  synchronize hardware pointer (hw_ptr) with ours
 */
static int snd_pcm_dshare_sync_ptr(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	snd_pcm_uframes_t slave_hw_ptr, old_slave_hw_ptr, avail;
	snd_pcm_sframes_t diff;
	
	switch (snd_pcm_state(dshare->spcm)) {
	case SND_PCM_STATE_DISCONNECTED:
		dshare->state = SNDRV_PCM_STATE_DISCONNECTED;
		return -ENOTTY;
	default:
		break;
	}
	if (dshare->slowptr)
		snd_pcm_hwsync(dshare->spcm);
	old_slave_hw_ptr = dshare->slave_hw_ptr;
	slave_hw_ptr = dshare->slave_hw_ptr = *dshare->spcm->hw.ptr;
	diff = slave_hw_ptr - old_slave_hw_ptr;
	if (diff == 0)		/* fast path */
		return 0;
	if (diff < 0) {
		slave_hw_ptr += dshare->shmptr->s.boundary;
		diff = slave_hw_ptr - old_slave_hw_ptr;
	}
	dshare->hw_ptr += diff;
	dshare->hw_ptr %= pcm->boundary;
	// printf("sync ptr diff = %li\n", diff);
	if (pcm->stop_threshold >= pcm->boundary)	/* don't care */
		return 0;
	if ((avail = snd_pcm_mmap_playback_avail(pcm)) >= pcm->stop_threshold) {
		struct timeval tv;
		gettimeofday(&tv, 0);
		dshare->trigger_tstamp.tv_sec = tv.tv_sec;
		dshare->trigger_tstamp.tv_nsec = tv.tv_usec * 1000L;
		dshare->state = SND_PCM_STATE_XRUN;
		dshare->avail_max = avail;
		return -EPIPE;
	}
	if (avail > dshare->avail_max)
		dshare->avail_max = avail;
	return 0;
}

/*
 *  plugin implementation
 */

static int snd_pcm_dshare_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	snd_pcm_state_t state;

	switch (dshare->state) {
	case SNDRV_PCM_STATE_DRAINING:
	case SNDRV_PCM_STATE_RUNNING:
		snd_pcm_dshare_sync_ptr(pcm);
		break;
	default:
		break;
	}
	memset(status, 0, sizeof(*status));
	state = snd_pcm_state(dshare->spcm);
	status->state = state == SND_PCM_STATE_RUNNING ? dshare->state : state;
	status->trigger_tstamp = dshare->trigger_tstamp;
	status->tstamp = snd_pcm_hw_fast_tstamp(dshare->spcm);
	status->avail = snd_pcm_mmap_playback_avail(pcm);
	status->avail_max = status->avail > dshare->avail_max ? status->avail : dshare->avail_max;
	dshare->avail_max = 0;
	return 0;
}

static snd_pcm_state_t snd_pcm_dshare_state(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	switch (snd_pcm_state(dshare->spcm)) {
	case SND_PCM_STATE_SUSPENDED:
		return -ESTRPIPE;
	case SND_PCM_STATE_DISCONNECTED:
		dshare->state = SNDRV_PCM_STATE_DISCONNECTED;
		return -ENOTTY;
	default:
		break;
	}
	return dshare->state;
}

static int snd_pcm_dshare_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	int err;
	
	switch (dshare->state) {
	case SNDRV_PCM_STATE_DRAINING:
	case SNDRV_PCM_STATE_RUNNING:
		err = snd_pcm_dshare_sync_ptr(pcm);
		if (err < 0)
			return err;
	case SNDRV_PCM_STATE_PREPARED:
	case SNDRV_PCM_STATE_SUSPENDED:
		*delayp = snd_pcm_mmap_playback_hw_avail(pcm);
		return 0;
	case SNDRV_PCM_STATE_XRUN:
		return -EPIPE;
	case SNDRV_PCM_STATE_DISCONNECTED:
		return -ENOTTY;
	default:
		return -EBADFD;
	}
}

static int snd_pcm_dshare_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;

	switch(dshare->state) {
	case SNDRV_PCM_STATE_DRAINING:
	case SNDRV_PCM_STATE_RUNNING:
		return snd_pcm_dshare_sync_ptr(pcm);
	case SNDRV_PCM_STATE_PREPARED:
	case SNDRV_PCM_STATE_SUSPENDED:
		return 0;
	case SNDRV_PCM_STATE_XRUN:
		return -EPIPE;
	case SNDRV_PCM_STATE_DISCONNECTED:
		return -ENOTTY;
	default:
		return -EBADFD;
	}
}

static int snd_pcm_dshare_prepare(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;

	snd_pcm_direct_check_interleave(dshare, pcm);
	// assert(pcm->boundary == dshare->shmptr->s.boundary);	/* for sure */
	dshare->state = SND_PCM_STATE_PREPARED;
	dshare->appl_ptr = 0;
	dshare->hw_ptr = 0;
	return 0;
}

static int snd_pcm_dshare_reset(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	dshare->hw_ptr %= pcm->period_size;
	dshare->appl_ptr = dshare->hw_ptr;
	dshare->slave_appl_ptr = dshare->slave_hw_ptr = *dshare->spcm->hw.ptr;
	return 0;
}

static int snd_pcm_dshare_start(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	snd_pcm_sframes_t avail;
	struct timeval tv;
	int err;
	
	if (dshare->state != SND_PCM_STATE_PREPARED)
		return -EBADFD;
	err = snd_timer_start(dshare->timer);
	if (err < 0)
		return err;
	dshare->state = SND_PCM_STATE_RUNNING;
	snd_pcm_hwsync(dshare->spcm);
	dshare->slave_appl_ptr = dshare->slave_hw_ptr = *dshare->spcm->hw.ptr;
	avail = snd_pcm_mmap_playback_hw_avail(pcm);
	if (avail < 0)
		return 0;
	if (avail > (snd_pcm_sframes_t)pcm->buffer_size)
		avail = pcm->buffer_size;
	snd_pcm_dshare_sync_area(pcm, avail);
	gettimeofday(&tv, 0);
	dshare->trigger_tstamp.tv_sec = tv.tv_sec;
	dshare->trigger_tstamp.tv_nsec = tv.tv_usec * 1000L;
	return 0;
}

static int snd_pcm_dshare_drop(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	if (dshare->state == SND_PCM_STATE_OPEN)
		return -EBADFD;
	snd_timer_stop(dshare->timer);
	do_silence(pcm);
	dshare->state = SND_PCM_STATE_SETUP;
	return 0;
}

static int snd_pcm_dshare_drain(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	snd_pcm_uframes_t stop_threshold;
	int err;

	if (dshare->state == SND_PCM_STATE_OPEN)
		return -EBADFD;
	stop_threshold = pcm->stop_threshold;
	if (pcm->stop_threshold > pcm->buffer_size)
		pcm->stop_threshold = pcm->buffer_size;
	while (dshare->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dshare_sync_ptr(pcm);
		if (err < 0)
			break;
		if (pcm->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		snd_pcm_wait(pcm, -1);
	}
	pcm->stop_threshold = stop_threshold;
	return snd_pcm_dshare_drop(pcm);
}

static int snd_pcm_dshare_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
        if (enable) {
		if (dshare->state != SND_PCM_STATE_RUNNING)
			return -EBADFD;
		dshare->state = SND_PCM_STATE_PAUSED;
		snd_timer_stop(dshare->timer);
		do_silence(pcm);
	} else {
		if (dshare->state != SND_PCM_STATE_PAUSED)
			return -EBADFD;
                dshare->state = SND_PCM_STATE_RUNNING;
                snd_timer_start(dshare->timer);
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dshare_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	/* FIXME: substract samples from the mix ring buffer, too? */
	snd_pcm_mmap_appl_backward(pcm, frames);
	return frames;
}

static snd_pcm_sframes_t snd_pcm_dshare_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_sframes_t avail;

	avail = snd_pcm_mmap_avail(pcm);
	if (avail < 0)
		return 0;
	if (frames > (snd_pcm_uframes_t)avail)
		frames = avail;
	snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

static int snd_pcm_dshare_resume(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	// snd_pcm_direct_t *dshare = pcm->private_data;
	// FIXME
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dshare_readi(snd_pcm_t *pcm ATTRIBUTE_UNUSED, void *buffer ATTRIBUTE_UNUSED, snd_pcm_uframes_t size ATTRIBUTE_UNUSED)
{
	return -ENODEV;
}

static snd_pcm_sframes_t snd_pcm_dshare_readn(snd_pcm_t *pcm ATTRIBUTE_UNUSED, void **bufs ATTRIBUTE_UNUSED, snd_pcm_uframes_t size ATTRIBUTE_UNUSED)
{
	return -ENODEV;
}

static int snd_pcm_dshare_close(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dshare = pcm->private_data;

	if (dshare->timer)
		snd_timer_close(dshare->timer);
	do_silence(pcm);
	snd_pcm_direct_semaphore_down(dshare, DIRECT_IPC_SEM_CLIENT);
	dshare->shmptr->u.dshare.chn_mask &= ~dshare->u.dshare.chn_mask;
	snd_pcm_close(dshare->spcm);
 	if (dshare->server)
 		snd_pcm_direct_server_discard(dshare);
 	if (dshare->client)
 		snd_pcm_direct_client_discard(dshare);
 	if (snd_pcm_direct_shm_discard(dshare) > 0) {
 		if (snd_pcm_direct_semaphore_discard(dshare) < 0)
 			snd_pcm_direct_semaphore_up(dshare, DIRECT_IPC_SEM_CLIENT);
 	} else {
		snd_pcm_direct_semaphore_up(dshare, DIRECT_IPC_SEM_CLIENT);
	}
	if (dshare->bindings)
		free(dshare->bindings);
	pcm->private_data = NULL;
	free(dshare);
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dshare_mmap_commit(snd_pcm_t *pcm,
						  snd_pcm_uframes_t offset ATTRIBUTE_UNUSED,
						  snd_pcm_uframes_t size)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	int err;

	snd_pcm_mmap_appl_forward(pcm, size);
	if (dshare->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dshare_sync_ptr(pcm);
		if (err < 0)
			return err;
		/* ok, we commit the changes after the validation of area */
		/* it's intended, although the result might be crappy */
		snd_pcm_dshare_sync_area(pcm, size);
	}
	return size;
}

static snd_pcm_sframes_t snd_pcm_dshare_avail_update(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	snd_pcm_direct_t *dshare = pcm->private_data;
	int err;
	
	if (dshare->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dshare_sync_ptr(pcm);
		if (err < 0)
			return err;
	}
	return snd_pcm_mmap_playback_avail(pcm);
}

static void snd_pcm_dshare_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_direct_t *dshare = pcm->private_data;

	snd_output_printf(out, "Direct Stream Mixing PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	if (dshare->spcm)
		snd_pcm_dump(dshare->spcm, out);
}

static snd_pcm_ops_t snd_pcm_dshare_ops = {
	.close = snd_pcm_dshare_close,
	.info = snd_pcm_direct_info,
	.hw_refine = snd_pcm_direct_hw_refine,
	.hw_params = snd_pcm_direct_hw_params,
	.hw_free = snd_pcm_direct_hw_free,
	.sw_params = snd_pcm_direct_sw_params,
	.channel_info = snd_pcm_direct_channel_info,
	.dump = snd_pcm_dshare_dump,
	.nonblock = snd_pcm_direct_nonblock,
	.async = snd_pcm_direct_async,
	.poll_revents = snd_pcm_direct_poll_revents,
	.mmap = snd_pcm_direct_mmap,
	.munmap = snd_pcm_direct_munmap,
};

static snd_pcm_fast_ops_t snd_pcm_dshare_fast_ops = {
	.status = snd_pcm_dshare_status,
	.state = snd_pcm_dshare_state,
	.hwsync = snd_pcm_dshare_hwsync,
	.delay = snd_pcm_dshare_delay,
	.prepare = snd_pcm_dshare_prepare,
	.reset = snd_pcm_dshare_reset,
	.start = snd_pcm_dshare_start,
	.drop = snd_pcm_dshare_drop,
	.drain = snd_pcm_dshare_drain,
	.pause = snd_pcm_dshare_pause,
	.rewind = snd_pcm_dshare_rewind,
	.forward = snd_pcm_dshare_forward,
	.resume = snd_pcm_dshare_resume,
	.writei = snd_pcm_mmap_writei,
	.writen = snd_pcm_mmap_writen,
	.readi = snd_pcm_dshare_readi,
	.readn = snd_pcm_dshare_readn,
	.avail_update = snd_pcm_dshare_avail_update,
	.mmap_commit = snd_pcm_dshare_mmap_commit,
};

/**
 * \brief Creates a new dshare PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param ipc_key IPC key for semaphore and shared memory
 * \param ipc_mode IPC permissions for semaphore and shared memory
 * \param params Parameters for slave
 * \param bindings Channel bindings
 * \param slowptr Slow but more precise pointer updates
 * \param root Configuration root
 * \param sconf Slave configuration
 * \param stream PCM Direction (stream)
 * \param mode PCM Mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_dshare_open(snd_pcm_t **pcmp, const char *name,
			key_t ipc_key, mode_t ipc_perm,
			struct slave_params *params,
			snd_config_t *bindings,
			int slowptr,
			snd_config_t *root, snd_config_t *sconf,
			snd_pcm_stream_t stream, int mode)
{
	snd_pcm_t *pcm = NULL, *spcm = NULL;
	snd_pcm_direct_t *dshare = NULL;
	int ret, first_instance;
	unsigned int chn;
	int fail_sem_loop = 10;

	assert(pcmp);

	if (stream != SND_PCM_STREAM_PLAYBACK) {
		SNDERR("The dshare plugin supports only playback stream");
		return -EINVAL;
	}

	dshare = calloc(1, sizeof(snd_pcm_direct_t));
	if (!dshare) {
		ret = -ENOMEM;
		goto _err;
	}
	
	ret = snd_pcm_direct_parse_bindings(dshare, bindings);
	if (ret < 0)
		goto _err;
		
	if (!dshare->bindings) {
		SNDERR("dshare: specify bindings!!!");
		ret = -EINVAL;
		goto _err;
	}
	
	dshare->ipc_key = ipc_key;
	dshare->ipc_perm = ipc_perm;
	dshare->semid = -1;
	dshare->shmid = -1;

	ret = snd_pcm_new(&pcm, dshare->type = SND_PCM_TYPE_DSHARE, name, stream, mode);
	if (ret < 0)
		goto _err;

	while (1) {
		ret = snd_pcm_direct_semaphore_create_or_connect(dshare);
		if (ret < 0) {
			SNDERR("unable to create IPC semaphore");
			goto _err;
		}
	
		ret = snd_pcm_direct_semaphore_down(dshare, DIRECT_IPC_SEM_CLIENT);
		if (ret < 0) {
			snd_pcm_direct_semaphore_discard(dshare);
			if (--fail_sem_loop <= 0)
				goto _err;
			continue;
		}
		break;
	}

	first_instance = ret = snd_pcm_direct_shm_create_or_connect(dshare);
	if (ret < 0) {
		SNDERR("unable to create IPC shm instance");
		goto _err;
	}
		
	pcm->ops = &snd_pcm_dshare_ops;
	pcm->fast_ops = &snd_pcm_dshare_fast_ops;
	pcm->private_data = dshare;
	dshare->state = SND_PCM_STATE_OPEN;
	dshare->slowptr = slowptr;
	dshare->sync_ptr = snd_pcm_dshare_sync_ptr;

	if (first_instance) {
		ret = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
		if (ret < 0) {
			SNDERR("unable to open slave");
			goto _err;
		}
	
		if (snd_pcm_type(spcm) != SND_PCM_TYPE_HW) {
			SNDERR("dshare plugin can be only connected to hw plugin");
			goto _err;
		}
		
		ret = snd_pcm_direct_initialize_slave(dshare, spcm, params);
		if (ret < 0) {
			SNDERR("unable to initialize slave");
			goto _err;
		}

		dshare->spcm = spcm;
		
		ret = snd_pcm_direct_server_create(dshare);
		if (ret < 0) {
			SNDERR("unable to create server");
			goto _err;
		}

		dshare->shmptr->type = spcm->type;
	} else {
		ret = snd_pcm_direct_client_connect(dshare);
		if (ret < 0) {
			SNDERR("unable to connect client");
			return ret;
		}
			
		ret = snd_pcm_hw_open_fd(&spcm, "dshare_client", dshare->hw_fd, 0);
		if (ret < 0) {
			SNDERR("unable to open hardware");
			goto _err;
		}
		
		spcm->donot_close = 1;
		spcm->setup = 1;
		spcm->buffer_size = dshare->shmptr->s.buffer_size;
		spcm->sample_bits = dshare->shmptr->s.sample_bits;
		spcm->channels = dshare->shmptr->s.channels;
		spcm->format = dshare->shmptr->s.format;
		spcm->boundary = dshare->shmptr->s.boundary;
		spcm->info = dshare->shmptr->s.info;
		ret = snd_pcm_mmap(spcm);
		if (ret < 0) {
			SNDERR("unable to mmap channels");
			goto _err;
		}
		dshare->spcm = spcm;
	}

	for (chn = 0; chn < dshare->channels; chn++)
		dshare->u.dshare.chn_mask |= (1ULL<<dshare->bindings[chn]);
	if (dshare->shmptr->u.dshare.chn_mask & dshare->u.dshare.chn_mask) {
		SNDERR("destination channel specified in bindings is already used");
		dshare->u.dshare.chn_mask = 0;
		ret = -EINVAL;
		goto _err;
	}
	dshare->shmptr->u.dshare.chn_mask |= dshare->u.dshare.chn_mask;
		
	ret = snd_pcm_direct_initialize_poll_fd(dshare);
	if (ret < 0) {
		SNDERR("unable to initialize poll_fd");
		goto _err;
	}

	pcm->poll_fd = dshare->poll_fd;
	pcm->poll_events = POLLIN;	/* it's different than other plugins */
		
	pcm->mmap_rw = 1;
	snd_pcm_set_hw_ptr(pcm, &dshare->hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &dshare->appl_ptr, -1, 0);
	
	snd_pcm_direct_semaphore_up(dshare, DIRECT_IPC_SEM_CLIENT);

	*pcmp = pcm;
	return 0;
	
 _err:
	if (dshare) {
		if (dshare->shmptr)
			dshare->shmptr->u.dshare.chn_mask &= ~dshare->u.dshare.chn_mask;
	 	if (dshare->timer)
 			snd_timer_close(dshare->timer);
 		if (dshare->server)
 			snd_pcm_direct_server_discard(dshare);
 		if (dshare->client)
 			snd_pcm_direct_client_discard(dshare);
 		if (spcm)
 			snd_pcm_close(spcm);
 		if (dshare->shmid >= 0) {
 			if (snd_pcm_direct_shm_discard(dshare) > 0) {
			 	if (dshare->semid >= 0) {
 					if (snd_pcm_direct_semaphore_discard(dshare) < 0)
 						snd_pcm_direct_semaphore_up(dshare, DIRECT_IPC_SEM_CLIENT);
 				}
 			}
 		}
 		if (dshare->bindings)
 			free(dshare->bindings);
		free(dshare);
	}
	if (pcm)
		snd_pcm_free(pcm);
	return ret;
}

/*! \page pcm_plugins

\section pcm_plugins_dshare Plugin: dshare

This plugin provides sharing channels.
Unlike \ref pcm_plugins_share "share plugin", this plugin doesn't need
the explicit server program but accesses the shared buffer concurrently
from each client as well as \ref pcm_plugins_dmix "dmix" and
\ref pcm_plugins_dsnoop "dsnoop" plugins do.
The parameters below are almost identical with these plugins.

\code
pcm.name {
	type dshare		# Direct sharing
	ipc_key INT		# unique IPC key
	ipc_key_add_uid BOOL	# add current uid to unique IPC key
	slave STR
	# or
	slave {			# Slave definition
		pcm STR		# slave PCM name
		# or
		pcm { }		# slave PCM definition
		format STR	# format definition
		rate INT	# rate definition
		channels INT
		period_time INT	# in usec
		# or
		period_size INT	# in bytes
		buffer_time INT	# in usec
		# or
		buffer_size INT # in bytes
		periods INT	# when buffer_size or buffer_time is not specified
	}
	bindings {		# note: this is client independent!!!
		N INT		# maps slave channel to client channel N
	}
}
\endcode

\subsection pcm_plugins_dshare_funcref Function reference

<UL>
  <LI>snd_pcm_dshare_open()
  <LI>_snd_pcm_dshare_open()
</UL>

*/

/**
 * \brief Creates a new dshare PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with dshare PCM description
 * \param stream PCM Stream
 * \param mode PCM Mode
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_dshare_open(snd_pcm_t **pcmp, const char *name,
		       snd_config_t *root, snd_config_t *conf,
		       snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	snd_config_t *slave = NULL, *bindings = NULL, *sconf;
	struct slave_params params;
	int bsize, psize, ipc_key_add_uid = 0, slowptr = 0;
	key_t ipc_key = 0;
	mode_t ipc_perm = 0600;
	
	int err;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "ipc_key") == 0) {
			long key;
			err = snd_config_get_integer(n, &key);
			if (err < 0) {
				SNDERR("The field ipc_key must be an integer type");
				return err;
			}
			ipc_key = key;
			continue;
		}
		if (strcmp(id, "ipc_perm") == 0) {
			char *perm;
			char *endp;
			err = snd_config_get_ascii(n, &perm);
			if (err < 0) {
				SNDERR("The field ipc_perm must be a valid file permission");
				return err;
			}
			if (isdigit(*perm) == 0) {
				SNDERR("The field ipc_perm must be a valid file permission");
				return -EINVAL;
			}
			ipc_perm = strtol(perm, &endp, 8);
			continue;
		}
		if (strcmp(id, "ipc_key_add_uid") == 0) {
			char *tmp;
			err = snd_config_get_ascii(n, &tmp);
			if (err < 0) {
				SNDERR("The field ipc_key_add_uid must be a boolean type");
				return err;
			}
			err = snd_config_get_bool_ascii(tmp);
			free(tmp);
			if (err < 0) {
				SNDERR("The field ipc_key_add_uid must be a boolean type");
				return err;
			}
			ipc_key_add_uid = err;
			continue;
		}
		if (strcmp(id, "slave") == 0) {
			slave = n;
			continue;
		}
		if (strcmp(id, "bindings") == 0) {
			bindings = n;
			continue;
		}
		if (strcmp(id, "slowptr") == 0) {
			char *tmp;
			err = snd_config_get_ascii(n, &tmp);
			if (err < 0) {
				SNDERR("The field slowptr must be a boolean type");
				return err;
			}
			err = snd_config_get_bool_ascii(tmp);
			free(tmp);
			if (err < 0) {
				SNDERR("The field slowptr must be a boolean type");
				return err;
			}
			slowptr = err;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}
	if (ipc_key_add_uid)
		ipc_key += getuid();
	if (!ipc_key) {
		SNDERR("Unique IPC key is not defined");
		return -EINVAL;
	}
	/* the default settings, it might be invalid for some hardware */
	params.format = SND_PCM_FORMAT_S16;
	params.rate = 48000;
	params.channels = 2;
	params.period_time = 125000;	/* 0.125 seconds */
	params.buffer_time = -1;
	bsize = psize = -1;
	params.periods = 3;
	err = snd_pcm_slave_conf(root, slave, &sconf, 8,
				 SND_PCM_HW_PARAM_FORMAT, 0, &params.format,
				 SND_PCM_HW_PARAM_RATE, 0, &params.rate,
				 SND_PCM_HW_PARAM_CHANNELS, 0, &params.channels,
				 SND_PCM_HW_PARAM_PERIOD_TIME, 0, &params.period_time,
				 SND_PCM_HW_PARAM_BUFFER_TIME, 0, &params.buffer_time,
				 SND_PCM_HW_PARAM_PERIOD_SIZE, 0, &psize,
				 SND_PCM_HW_PARAM_BUFFER_SIZE, 0, &bsize,
				 SND_PCM_HW_PARAM_PERIODS, 0, &params.periods);
	if (err < 0)
		return err;

	params.period_size = psize;
	params.buffer_size = bsize;
	err = snd_pcm_dshare_open(pcmp, name, ipc_key, ipc_perm, &params, bindings, slowptr, root, sconf, stream, mode);
	if (err < 0)
		snd_config_delete(sconf);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_dshare_open, SND_PCM_DLSYM_VERSION);
#endif
