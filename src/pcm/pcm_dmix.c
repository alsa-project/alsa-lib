/**
 * \file pcm/pcm_dmix.c
 * \ingroup PCM_Plugins
 * \brief PCM Direct Stream Mixing (dmix) Plugin Interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 */
/*
 *  PCM - Direct Stream Mixing
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
const char *_snd_module_pcm_dmix = "";
#endif

/*
 *
 */

/*
 *  sum ring buffer shared memory area 
 */
static int shm_sum_create_or_connect(snd_pcm_direct_t *dmix)
{
	static int shm_sum_discard(snd_pcm_direct_t *dmix);
	struct shmid_ds buf;
	int tmpid, err;
	size_t size;

	size = dmix->shmptr->s.channels *
	       dmix->shmptr->s.buffer_size *
	       sizeof(signed int);	
retryshm:
	dmix->u.dmix.shmid_sum = shmget(dmix->ipc_key + 1, size, IPC_CREAT | 0666);
	err = -errno;
	if (dmix->u.dmix.shmid_sum < 0){
		if (errno == EINVAL)
		if ((tmpid = shmget(dmix->ipc_key + 1, 0, 0666)) != -1)
		if (!shmctl(tmpid, IPC_STAT, &buf))
	    	if (!buf.shm_nattch) 
		/* no users so destroy the segment */
		if (!shmctl(tmpid, IPC_RMID, NULL))
		    goto retryshm;
		return err;
	}
	dmix->u.dmix.sum_buffer = shmat(dmix->u.dmix.shmid_sum, 0, 0);
	if (dmix->u.dmix.sum_buffer == (void *) -1) {
		shm_sum_discard(dmix);
		return -errno;
	}
	mlock(dmix->u.dmix.sum_buffer, size);
	return 0;
}

static int shm_sum_discard(snd_pcm_direct_t *dmix)
{
	struct shmid_ds buf;
	int ret = 0;

	if (dmix->u.dmix.shmid_sum < 0)
		return -EINVAL;
	if (dmix->u.dmix.sum_buffer != (void *) -1 && shmdt(dmix->u.dmix.sum_buffer) < 0)
		return -errno;
	dmix->u.dmix.sum_buffer = (void *) -1;
	if (shmctl(dmix->u.dmix.shmid_sum, IPC_STAT, &buf) < 0)
		return -errno;
	if (buf.shm_nattch == 0) {	/* we're the last user, destroy the segment */
		if (shmctl(dmix->u.dmix.shmid_sum, IPC_RMID, NULL) < 0)
			return -errno;
		ret = 1;
	}
	dmix->u.dmix.shmid_sum = -1;
	return ret;
}

static void dmix_server_free(snd_pcm_direct_t *dmix)
{
	/* remove the memory region */
	shm_sum_create_or_connect(dmix);
	shm_sum_discard(dmix);
}

/*
 *  the main function of this plugin: mixing
 *  FIXME: optimize it for different architectures
 */

#ifdef __i386__

#define ADD_AND_SATURATE

#define MIX_AREAS1 mix_areas1
#define MIX_AREAS1_MMX mix_areas1_mmx
#define MIX_AREAS2 mix_areas2
#define LOCK_PREFIX ""
#include "pcm_dmix_i386.h"
#undef MIX_AREAS1
#undef MIX_AREAS1_MMX
#undef MIX_AREAS2
#undef LOCK_PREFIX

#define MIX_AREAS1 mix_areas1_smp
#define MIX_AREAS1_MMX mix_areas1_smp_mmx
#define MIX_AREAS2 mix_areas2_smp
#define LOCK_PREFIX "lock ; "
#include "pcm_dmix_i386.h"
#undef MIX_AREAS1
#undef MIX_AREAS1_MMX
#undef MIX_AREAS2
#undef LOCK_PREFIX
 
static void mix_select_callbacks(snd_pcm_direct_t *dmix)
{
	FILE *in;
	char line[255];
	int smp = 0, mmx = 0;
	
	/* try to determine, if we have a MMX capable CPU */
	in = fopen("/proc/cpuinfo", "r");
	if (in) {
		while (!feof(in)) {
			fgets(line, sizeof(line), in);
			if (!strncmp(line, "processor", 9))
				smp++;
			else if (!strncmp(line, "flags", 5)) {
				if (strstr(line, " mmx"))
					mmx = 1;
			}
		}
		fclose(in);
	}
	// printf("MMX: %i, SMP: %i\n", mmx, smp);
	if (mmx) {
		dmix->u.dmix.mix_areas1 = smp > 1 ? mix_areas1_smp_mmx : mix_areas1_mmx;
	} else {
		dmix->u.dmix.mix_areas1 = smp > 1 ? mix_areas1_smp : mix_areas1;
	}
	dmix->u.dmix.mix_areas2 = smp > 1 ? mix_areas2_smp : mix_areas2;
}
#endif

#ifdef __x86_64__
#define ADD_AND_SATURATE

#define MIX_AREAS1 mix_areas1
#define MIX_AREAS2 mix_areas2
#define LOCK_PREFIX ""
#include "pcm_dmix_x86_64.h"
#undef MIX_AREAS1
#undef MIX_AREAS2
#undef LOCK_PREFIX

#define MIX_AREAS1 mix_areas1_smp
#define MIX_AREAS2 mix_areas2_smp
#define LOCK_PREFIX "lock ; "
#include "pcm_dmix_x86_64.h"
#undef MIX_AREAS1
#undef MIX_AREAS2
#undef LOCK_PREFIX
 
static void mix_select_callbacks(snd_pcm_direct_t *dmix)
{
	FILE *in;
	char line[255];
	int smp = 0;
	
	/* try to determine, if we have SMP */
	in = fopen("/proc/cpuinfo", "r");
	if (in) {
		while (!feof(in)) {
			fgets(line, sizeof(line), in);
			if (!strncmp(line, "processor", 9))
				smp++;
		}
		fclose(in);
	}
	// printf("SMP: %i\n", smp);
	dmix->u.dmix.mix_areas1 = smp > 1 ? mix_areas1_smp : mix_areas1;
	dmix->u.dmix.mix_areas2 = smp > 1 ? mix_areas2_smp : mix_areas2;
}
#endif


#ifndef ADD_AND_SATURATE
#warning Please, recode mix_areas1() routine to your architecture...
static void mix_areas1(unsigned int size,
		       volatile signed short *dst, signed short *src,
		       volatile signed int *sum, size_t dst_step,
		       size_t src_step, size_t sum_step)
{
	register signed int sample, old_sample;

	for (;;) {
		sample = *src;
		old_sample = *sum;
		if (*dst == 0)
			sample -= old_sample;
		*sum += sample;
		do {
			old_sample = *sum;
			if (old_sample > 0x7fff)
				sample = 0x7fff;
			else if (old_sample < -0x8000)
				sample = -0x8000;
			else
				sample = old_sample;
			*dst = sample;
		} while (*sum != old_sample);
		if (!--size)
			return;
		((char *)src) += src_step;
		((char *)dst) += dst_step;
		((char *)sum) += sum_step;
	}
}

#warning Please, recode mix_areas2() routine to your architecture...
static void mix_areas2(unsigned int size,
		       volatile signed int *dst, signed int *src,
		       volatile signed int *sum, size_t dst_step,
		       size_t src_step, size_t sum_step)
{
	register signed int sample, old_sample;

	for (;;) {
		sample = *src / 256;
		old_sample = *sum;
		if (*dst == 0)
			sample -= old_sample;
		*sum += sample;
		do {
			old_sample = *sum;
			if (old_sample > 0x7fffff)
				sample = 0x7fffffff;
			else if (old_sample < -0x800000)
				sample = -0x80000000;
			else
				sample = old_sample * 256;
			*dst = sample;
		} while (*sum != old_sample);
		if (!--size)
			return;
		((char *)src) += src_step;
		((char *)dst) += dst_step;
		((char *)sum) += sum_step;
	}
}

static void mix_select_callbacks(snd_pcm_direct_t *dmix)
{
	dmix->u.dmix.mix_areas1 = mix_areas1;
	dmix->u.dmix.mix_areas2 = mix_areas2;
}
#endif

static void mix_areas(snd_pcm_direct_t *dmix,
		      const snd_pcm_channel_area_t *src_areas,
		      const snd_pcm_channel_area_t *dst_areas,
		      snd_pcm_uframes_t src_ofs,
		      snd_pcm_uframes_t dst_ofs,
		      snd_pcm_uframes_t size)
{
	volatile signed int *sum;
	unsigned int src_step, dst_step;
	unsigned int chn, dchn, channels;
	
	channels = dmix->channels;
	if (dmix->shmptr->s.format == SND_PCM_FORMAT_S16) {
		signed short *src;
		volatile signed short *dst;
		if (dmix->interleaved) {
			/*
			 * process all areas in one loop
			 * it optimizes the memory accesses for this case
			 */
			dmix->u.dmix.mix_areas1(size * channels,
					 ((signed short *)dst_areas[0].addr) + (dst_ofs * channels),
					 ((signed short *)src_areas[0].addr) + (src_ofs * channels),
					 dmix->u.dmix.sum_buffer + (dst_ofs * channels),
					 sizeof(signed short),
					 sizeof(signed short),
					 sizeof(signed int));
			return;
		}
		for (chn = 0; chn < channels; chn++) {
			dchn = dmix->bindings ? dmix->bindings[chn] : chn;
			if (dchn >= dmix->shmptr->s.channels)
				continue;
			src_step = src_areas[chn].step / 8;
			dst_step = dst_areas[dchn].step / 8;
			src = (signed short *)(((char *)src_areas[chn].addr + src_areas[chn].first / 8) + (src_ofs * src_step));
			dst = (signed short *)(((char *)dst_areas[dchn].addr + dst_areas[dchn].first / 8) + (dst_ofs * dst_step));
			sum = dmix->u.dmix.sum_buffer + channels * dst_ofs + chn;
			dmix->u.dmix.mix_areas1(size, dst, src, sum, dst_step, src_step, channels * sizeof(signed int));
		}
	} else {
		signed int *src;
		volatile signed int *dst;
		if (dmix->interleaved) {
			/*
			 * process all areas in one loop
			 * it optimizes the memory accesses for this case
			 */
			dmix->u.dmix.mix_areas2(size * channels,
					 ((signed int *)dst_areas[0].addr) + (dst_ofs * channels),
					 ((signed int *)src_areas[0].addr) + (src_ofs * channels),
					 dmix->u.dmix.sum_buffer + (dst_ofs * channels),
					 sizeof(signed int),
					 sizeof(signed int),
					 sizeof(signed int));
			return;
		}
		for (chn = 0; chn < channels; chn++) {
			dchn = dmix->bindings ? dmix->bindings[chn] : chn;
			if (dchn >= dmix->shmptr->s.channels)
				continue;
			src_step = src_areas[chn].step / 8;
			dst_step = dst_areas[dchn].step / 8;
			src = (signed int *)(((char *)src_areas[chn].addr + src_areas[chn].first / 8) + (src_ofs * src_step));
			dst = (signed int *)(((char *)dst_areas[dchn].addr + dst_areas[dchn].first / 8) + (dst_ofs * dst_step));
			sum = dmix->u.dmix.sum_buffer + channels * dst_ofs + chn;
			dmix->u.dmix.mix_areas2(size, dst, src, sum, dst_step, src_step, channels * sizeof(signed int));
		}
	}
}

/*
 *  synchronize shm ring buffer with hardware
 */
static void snd_pcm_dmix_sync_area(snd_pcm_t *pcm, snd_pcm_uframes_t size)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	snd_pcm_uframes_t appl_ptr, slave_appl_ptr, transfer;
	const snd_pcm_channel_area_t *src_areas, *dst_areas;
	
	/* get the start of update area */
	appl_ptr = dmix->appl_ptr - size;
	if (appl_ptr > pcm->boundary)
		appl_ptr += pcm->boundary;
	appl_ptr %= pcm->buffer_size;
	/* add sample areas here */
	src_areas = snd_pcm_mmap_areas(pcm);
	dst_areas = snd_pcm_mmap_areas(dmix->spcm);
	slave_appl_ptr = dmix->slave_appl_ptr % dmix->shmptr->s.buffer_size;
	dmix->slave_appl_ptr += size;
	dmix->slave_appl_ptr %= dmix->shmptr->s.boundary;
	while (size > 0) {
		transfer = appl_ptr + size > pcm->buffer_size ? pcm->buffer_size - appl_ptr : size;
		if ((transfer = slave_appl_ptr + transfer > dmix->shmptr->s.buffer_size ? dmix->shmptr->s.buffer_size - slave_appl_ptr : transfer))
		mix_areas(dmix, src_areas, dst_areas, appl_ptr, slave_appl_ptr, transfer);
		if (transfer >= size)
			return;
		size -= transfer;
		slave_appl_ptr += transfer;
		slave_appl_ptr %= dmix->shmptr->s.buffer_size;
		appl_ptr += transfer;
		appl_ptr %= pcm->buffer_size;
	}
}

/*
 *  synchronize hardware pointer (hw_ptr) with ours
 */
static int snd_pcm_dmix_sync_ptr(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	snd_pcm_uframes_t slave_hw_ptr, old_slave_hw_ptr, avail;
	snd_pcm_sframes_t diff;
	
	switch (snd_pcm_state(dmix->spcm)) {
	case SND_PCM_STATE_DISCONNECTED:
		dmix->state = -ENOTTY;
		return -ENOTTY;
	default:
		break;
	}
	if (dmix->slowptr)
		snd_pcm_hwsync(dmix->spcm);
	old_slave_hw_ptr = dmix->slave_hw_ptr;
	slave_hw_ptr = dmix->slave_hw_ptr = *dmix->spcm->hw.ptr;
	diff = slave_hw_ptr - old_slave_hw_ptr;
	if (diff == 0)		/* fast path */
		return 0;
	if (diff < 0) {
		slave_hw_ptr += dmix->shmptr->s.boundary;
		diff = slave_hw_ptr - old_slave_hw_ptr;
	}
	dmix->hw_ptr += diff;
	dmix->hw_ptr %= pcm->boundary;
	if (pcm->stop_threshold >= pcm->boundary)	/* don't care */
		return 0;
	if ((avail = snd_pcm_mmap_playback_avail(pcm)) >= pcm->stop_threshold) {
		struct timeval tv;
		gettimeofday(&tv, 0);
		dmix->trigger_tstamp.tv_sec = tv.tv_sec;
		dmix->trigger_tstamp.tv_nsec = tv.tv_usec * 1000L;
		dmix->state = SND_PCM_STATE_XRUN;
		dmix->avail_max = avail;
		return -EPIPE;
	}
	if (avail > dmix->avail_max)
		dmix->avail_max = avail;
	return 0;
}

/*
 *  plugin implementation
 */

static int snd_pcm_dmix_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	snd_pcm_state_t state;

	switch (dmix->state) {
	case SNDRV_PCM_STATE_DRAINING:
	case SNDRV_PCM_STATE_RUNNING:
		snd_pcm_dmix_sync_ptr(pcm);
		break;
	default:
		break;
	}
	memset(status, 0, sizeof(*status));
	state = snd_pcm_state(dmix->spcm);
	status->state = state == SNDRV_PCM_STATE_RUNNING ? dmix->state : state;
	status->trigger_tstamp = dmix->trigger_tstamp;
	status->tstamp = snd_pcm_hw_fast_tstamp(dmix->spcm);
	status->avail = snd_pcm_mmap_playback_avail(pcm);
	status->avail_max = status->avail > dmix->avail_max ? status->avail : dmix->avail_max;
	dmix->avail_max = 0;
	return 0;
}

static snd_pcm_state_t snd_pcm_dmix_state(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	switch (snd_pcm_state(dmix->spcm)) {
	case SND_PCM_STATE_SUSPENDED:
		return -ESTRPIPE;
	case SND_PCM_STATE_DISCONNECTED:
		dmix->state = -ENOTTY;
		return -ENOTTY;
	default:
		break;
	}
	return dmix->state;
}

static int snd_pcm_dmix_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	int err;
	
	switch(dmix->state) {
	case SNDRV_PCM_STATE_DRAINING:
	case SNDRV_PCM_STATE_RUNNING:
		err = snd_pcm_dmix_sync_ptr(pcm);
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

static int snd_pcm_dmix_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;

	switch(dmix->state) {
	case SNDRV_PCM_STATE_DRAINING:
	case SNDRV_PCM_STATE_RUNNING:
		return snd_pcm_dmix_sync_ptr(pcm);
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

static int snd_pcm_dmix_prepare(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;

	snd_pcm_direct_check_interleave(dmix, pcm);
	// assert(pcm->boundary == dmix->shmptr->s.boundary);	/* for sure */
	dmix->state = SND_PCM_STATE_PREPARED;
	dmix->appl_ptr = 0;
	dmix->hw_ptr = 0;
	return 0;
}

static int snd_pcm_dmix_reset(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	dmix->hw_ptr %= pcm->period_size;
	dmix->appl_ptr = dmix->hw_ptr;
	dmix->slave_appl_ptr = dmix->slave_hw_ptr = *dmix->spcm->hw.ptr;
	return 0;
}

static int snd_pcm_dmix_start(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	snd_pcm_sframes_t avail;
	struct timeval tv;
	int err;
	
	if (dmix->state != SND_PCM_STATE_PREPARED)
		return -EBADFD;
	err = snd_timer_start(dmix->timer);
	if (err < 0)
		return err;
	dmix->state = SND_PCM_STATE_RUNNING;
	snd_pcm_hwsync(dmix->spcm);
	dmix->slave_appl_ptr = dmix->slave_hw_ptr = *dmix->spcm->hw.ptr;
	avail = snd_pcm_mmap_playback_hw_avail(pcm);
	if (avail < 0)
		return 0;
	if (avail > (snd_pcm_sframes_t)pcm->buffer_size)
		avail = pcm->buffer_size;
	snd_pcm_dmix_sync_area(pcm, avail);
	gettimeofday(&tv, 0);
	dmix->trigger_tstamp.tv_sec = tv.tv_sec;
	dmix->trigger_tstamp.tv_nsec = tv.tv_usec * 1000L;
	return 0;
}

static int snd_pcm_dmix_drop(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	if (dmix->state == SND_PCM_STATE_OPEN)
		return -EBADFD;
	snd_timer_stop(dmix->timer);
	dmix->state = SND_PCM_STATE_SETUP;
	return 0;
}

static int snd_pcm_dmix_drain(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	snd_pcm_uframes_t stop_threshold;
	int err;

	if (dmix->state == SND_PCM_STATE_OPEN)
		return -EBADFD;
	stop_threshold = pcm->stop_threshold;
	if (pcm->stop_threshold > pcm->buffer_size)
		pcm->stop_threshold = pcm->buffer_size;
	while (dmix->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dmix_sync_ptr(pcm);
		if (err < 0)
			break;
		if (pcm->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		snd_pcm_wait(pcm, -1);
	}
	pcm->stop_threshold = stop_threshold;
	return snd_pcm_dmix_drop(pcm);
}

static int snd_pcm_dmix_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
        if (enable) {
		if (dmix->state != SND_PCM_STATE_RUNNING)
			return -EBADFD;
		dmix->state = SND_PCM_STATE_PAUSED;
		snd_timer_stop(dmix->timer);
	} else {
		if (dmix->state != SND_PCM_STATE_PAUSED)
			return -EBADFD;
                dmix->state = SND_PCM_STATE_RUNNING;
                snd_timer_start(dmix->timer);
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	/* FIXME: substract samples from the mix ring buffer, too? */
	snd_pcm_mmap_appl_backward(pcm, frames);
	return frames;
}

static snd_pcm_sframes_t snd_pcm_dmix_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_sframes_t avail;

	avail = snd_pcm_mmap_playback_avail(pcm);
	if (avail < 0)
		return 0;
	if (frames > (snd_pcm_uframes_t)avail)
		frames = avail;
	snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

static int snd_pcm_dmix_resume(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	// snd_pcm_direct_t *dmix = pcm->private_data;
	// FIXME
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_readi(snd_pcm_t *pcm ATTRIBUTE_UNUSED, void *buffer ATTRIBUTE_UNUSED, snd_pcm_uframes_t size ATTRIBUTE_UNUSED)
{
	return -ENODEV;
}

static snd_pcm_sframes_t snd_pcm_dmix_readn(snd_pcm_t *pcm ATTRIBUTE_UNUSED, void **bufs ATTRIBUTE_UNUSED, snd_pcm_uframes_t size ATTRIBUTE_UNUSED)
{
	return -ENODEV;
}

static int snd_pcm_dmix_close(snd_pcm_t *pcm)
{
	snd_pcm_direct_t *dmix = pcm->private_data;

	if (dmix->timer)
		snd_timer_close(dmix->timer);
	snd_pcm_direct_semaphore_down(dmix, DIRECT_IPC_SEM_CLIENT);
	snd_pcm_close(dmix->spcm);
 	if (dmix->server)
 		snd_pcm_direct_server_discard(dmix);
 	if (dmix->client)
 		snd_pcm_direct_client_discard(dmix);
 	shm_sum_discard(dmix);
 	if (snd_pcm_direct_shm_discard(dmix) > 0) {
 		if (snd_pcm_direct_semaphore_discard(dmix) < 0)
 			snd_pcm_direct_semaphore_up(dmix, DIRECT_IPC_SEM_CLIENT);
 	} else {
		snd_pcm_direct_semaphore_up(dmix, DIRECT_IPC_SEM_CLIENT);
	}
	if (dmix->bindings)
		free(dmix->bindings);
	pcm->private_data = NULL;
	free(dmix);
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_mmap_commit(snd_pcm_t *pcm,
						  snd_pcm_uframes_t offset ATTRIBUTE_UNUSED,
						  snd_pcm_uframes_t size)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	int err;

	snd_pcm_mmap_appl_forward(pcm, size);
	if (dmix->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dmix_sync_ptr(pcm);
		if (err < 0)
			return err;
		/* ok, we commit the changes after the validation of area */
		/* it's intended, although the result might be crappy */
		snd_pcm_dmix_sync_area(pcm, size);
	}
	return size;
}

static snd_pcm_sframes_t snd_pcm_dmix_avail_update(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	int err;
	
	if (dmix->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dmix_sync_ptr(pcm);
		if (err < 0)
			return err;
	}
	return snd_pcm_mmap_playback_avail(pcm);
}

static void snd_pcm_dmix_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_direct_t *dmix = pcm->private_data;

	snd_output_printf(out, "Direct Stream Mixing PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	if (dmix->spcm)
		snd_pcm_dump(dmix->spcm, out);
}

static snd_pcm_ops_t snd_pcm_dmix_ops = {
	.close = snd_pcm_dmix_close,
	.info = snd_pcm_direct_info,
	.hw_refine = snd_pcm_direct_hw_refine,
	.hw_params = snd_pcm_direct_hw_params,
	.hw_free = snd_pcm_direct_hw_free,
	.sw_params = snd_pcm_direct_sw_params,
	.channel_info = snd_pcm_direct_channel_info,
	.dump = snd_pcm_dmix_dump,
	.nonblock = snd_pcm_direct_nonblock,
	.async = snd_pcm_direct_async,
	.poll_revents = snd_pcm_direct_poll_revents,
	.mmap = snd_pcm_direct_mmap,
	.munmap = snd_pcm_direct_munmap,
};

static snd_pcm_fast_ops_t snd_pcm_dmix_fast_ops = {
	.status = snd_pcm_dmix_status,
	.state = snd_pcm_dmix_state,
	.hwsync = snd_pcm_dmix_hwsync,
	.delay = snd_pcm_dmix_delay,
	.prepare = snd_pcm_dmix_prepare,
	.reset = snd_pcm_dmix_reset,
	.start = snd_pcm_dmix_start,
	.drop = snd_pcm_dmix_drop,
	.drain = snd_pcm_dmix_drain,
	.pause = snd_pcm_dmix_pause,
	.rewind = snd_pcm_dmix_rewind,
	.forward = snd_pcm_dmix_forward,
	.resume = snd_pcm_dmix_resume,
	.writei = snd_pcm_mmap_writei,
	.writen = snd_pcm_mmap_writen,
	.readi = snd_pcm_dmix_readi,
	.readn = snd_pcm_dmix_readn,
	.avail_update = snd_pcm_dmix_avail_update,
	.mmap_commit = snd_pcm_dmix_mmap_commit,
};

/**
 * \brief Creates a new dmix PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param ipc_key IPC key for semaphore and shared memory
 * \param ipc_perm IPC permissions for semaphore and shared memory
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
int snd_pcm_dmix_open(snd_pcm_t **pcmp, const char *name,
		      key_t ipc_key, mode_t ipc_perm,
		      struct slave_params *params,
		      snd_config_t *bindings,
		      int slowptr,
		      snd_config_t *root, snd_config_t *sconf,
		      snd_pcm_stream_t stream, int mode)
{
	snd_pcm_t *pcm = NULL, *spcm = NULL;
	snd_pcm_direct_t *dmix = NULL;
	int ret, first_instance;
	int fail_sem_loop = 10;

	assert(pcmp);

	if (stream != SND_PCM_STREAM_PLAYBACK) {
		SNDERR("The dmix plugin supports only playback stream");
		return -EINVAL;
	}

	dmix = calloc(1, sizeof(snd_pcm_direct_t));
	if (!dmix) {
		ret = -ENOMEM;
		goto _err;
	}
	
	ret = snd_pcm_direct_parse_bindings(dmix, bindings);
	if (ret < 0)
		goto _err;
	
	dmix->ipc_key = ipc_key;
	dmix->ipc_perm = ipc_perm;
	dmix->semid = -1;
	dmix->shmid = -1;

	ret = snd_pcm_new(&pcm, dmix->type = SND_PCM_TYPE_DMIX, name, stream, mode);
	if (ret < 0)
		goto _err;

	
	while (1) {
		ret = snd_pcm_direct_semaphore_create_or_connect(dmix);
		if (ret < 0) {
			SNDERR("unable to create IPC semaphore");
			goto _err;
		}
		ret = snd_pcm_direct_semaphore_down(dmix, DIRECT_IPC_SEM_CLIENT);
		if (ret < 0) {
			snd_pcm_direct_semaphore_discard(dmix);
			if (--fail_sem_loop <= 0)
				goto _err;
			continue;
		}
		break;
	}
		
	first_instance = ret = snd_pcm_direct_shm_create_or_connect(dmix);
	if (ret < 0) {
		SNDERR("unable to create IPC shm instance");
		goto _err;
	}
		
	pcm->ops = &snd_pcm_dmix_ops;
	pcm->fast_ops = &snd_pcm_dmix_fast_ops;
	pcm->private_data = dmix;
	dmix->state = SND_PCM_STATE_OPEN;
	dmix->slowptr = slowptr;
	dmix->sync_ptr = snd_pcm_dmix_sync_ptr;

	if (first_instance) {
		ret = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
		if (ret < 0) {
			SNDERR("unable to open slave");
			goto _err;
		}
	
		if (snd_pcm_type(spcm) != SND_PCM_TYPE_HW) {
			SNDERR("dmix plugin can be only connected to hw plugin");
			goto _err;
		}
		
		ret = snd_pcm_direct_initialize_slave(dmix, spcm, params);
		if (ret < 0) {
			SNDERR("unable to initialize slave");
			goto _err;
		}

		dmix->spcm = spcm;

		dmix->server_free = dmix_server_free;
		
		ret = snd_pcm_direct_server_create(dmix);
		if (ret < 0) {
			SNDERR("unable to create server");
			goto _err;
		}

		dmix->shmptr->type = spcm->type;
	} else {
		ret = snd_pcm_direct_client_connect(dmix);
		if (ret < 0) {
			SNDERR("unable to connect client");
			return ret;
		}
			
		ret = snd_pcm_hw_open_fd(&spcm, "dmix_client", dmix->hw_fd, 0);
		if (ret < 0) {
			SNDERR("unable to open hardware");
			goto _err;
		}
		
		spcm->donot_close = 1;
		spcm->setup = 1;
		spcm->buffer_size = dmix->shmptr->s.buffer_size;
		spcm->sample_bits = dmix->shmptr->s.sample_bits;
		spcm->channels = dmix->shmptr->s.channels;
		spcm->format = dmix->shmptr->s.format;
		spcm->boundary = dmix->shmptr->s.boundary;
		spcm->info = dmix->shmptr->s.info;
		ret = snd_pcm_mmap(spcm);
		if (ret < 0) {
			SNDERR("unable to mmap channels");
			goto _err;
		}
		dmix->spcm = spcm;
	}

	ret = shm_sum_create_or_connect(dmix);
	if (ret < 0) {
		SNDERR("unable to initialize sum ring buffer");
		goto _err;
	}

	ret = snd_pcm_direct_initialize_poll_fd(dmix);
	if (ret < 0) {
		SNDERR("unable to initialize poll_fd");
		goto _err;
	}

	mix_select_callbacks(dmix);
		
	pcm->poll_fd = dmix->poll_fd;
	pcm->poll_events = POLLIN;	/* it's different than other plugins */
		
	pcm->mmap_rw = 1;
	snd_pcm_set_hw_ptr(pcm, &dmix->hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &dmix->appl_ptr, -1, 0);
	
	if (dmix->channels == UINT_MAX)
		dmix->channels = dmix->shmptr->s.channels;

	snd_pcm_direct_semaphore_up(dmix, DIRECT_IPC_SEM_CLIENT);

	*pcmp = pcm;
	return 0;
	
 _err:
	if (dmix) {
	 	if (dmix->timer)
 			snd_timer_close(dmix->timer);
 		if (dmix->server)
 			snd_pcm_direct_server_discard(dmix);
 		if (dmix->client)
 			snd_pcm_direct_client_discard(dmix);
 		if (spcm)
 			snd_pcm_close(spcm);
	 	if (dmix->u.dmix.shmid_sum >= 0)
 			shm_sum_discard(dmix);
 		if (dmix->shmid >= 0) {
 			if (snd_pcm_direct_shm_discard(dmix) > 0) {
			 	if (dmix->semid >= 0) {
 					if (snd_pcm_direct_semaphore_discard(dmix) < 0)
 						snd_pcm_direct_semaphore_up(dmix, DIRECT_IPC_SEM_CLIENT);
 				}
 			}
 		}
 		if (dmix->bindings)
 			free(dmix->bindings);
		free(dmix);
	}
	if (pcm)
		snd_pcm_free(pcm);
	return ret;
}

/*! \page pcm_plugins

\section pcm_plugins_dmix Plugin: dmix

This plugin provides direct mixing of multiple streams. The resolution
for 32-bit mixing is only 24-bit. The low significant byte is filled with
zeros. The extra 8 bits are used for the saturation.

\code
pcm.name {
	type dmix		# Direct mix
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

<code>ipc_key</code> specfies the unique IPC key in integer.
This number must be unique for each different dmix definition,
since the shared memory is created with this key number.
When <code>ipc_key_add_uid</code> is set true, the uid value is
added to the value set in <code>ipc_key</code>.  This will
avoid the confliction of the same IPC key with different users
concurrently.

Note that the dmix plugin itself supports only a single configuration.
That is, it supports only the fixed rate (default 48000), format
(\c S16), channels (2), and period_time (125000).
For using other configuration, you have to set the value explicitly
in the slave PCM definition.  The rate, format and channels can be
covered by an additional \ref pcm_plugins_dmix "plug plugin",
but there is only one base configuration, anyway.

An example configuration for setting 44100 Hz, \c S32_LE format
as the slave PCM of "hw:0" is like below:
\code
pcm.dmix_44 {
	type dmix
	ipc_key 321456	# any unique value
	ipc_key_add_uid true
	slave {
		pcm "hw:0"
		format S32_LE
		rate 44100
	}
}
\endcode
You can hear 48000 Hz samples still using this dmix pcm via plug plugin
like:
\code
% aplay -Dplug:dmix_44 foo_48k.wav
\endcode

For using the dmix plugin for OSS emulation device, you have to set
the period and the buffer sizes in power of two.  For example,
\code
pcm.dmixoss {
	type dmix
	ipc_key 321456	# any unique value
	ipc_key_add_uid true
	slave {
		pcm "hw:0"
		period_time 0
		period_size 1024  # must be power of 2
		buffer_size 8192  # ditto
	}
}
\endcode
<code>period_time 0</code> must be set, too, for resetting the
default value.  In the case of soundcards with multi-channel IO,
adding the bindings would help
\code
pcm.dmixoss {
	...
	bindings {
		0 0   # map from 0 to 0
		1 1   # map from 1 to 1
	}
}
\endcode
so that only the first two channels are used by dmix.
Also, note that ICE1712 have the limited buffer size, 5513 frames
(corresponding to 640 kB).  In this case, reduce the buffer_size
to 4096.

\subsection pcm_plugins_dmix_funcref Function reference

<UL>
  <LI>snd_pcm_dmix_open()
  <LI>_snd_pcm_dmix_open()
</UL>

*/

/**
 * \brief Creates a new dmix PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with dmix PCM description
 * \param stream PCM Stream
 * \param mode PCM Mode
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_dmix_open(snd_pcm_t **pcmp, const char *name,
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

	err = snd_pcm_dmix_open(pcmp, name, ipc_key, ipc_perm, &params, bindings, slowptr, root, sconf, stream, mode);
	if (err < 0)
		snd_config_delete(sconf);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_dmix_open, SND_PCM_DLSYM_VERSION);
#endif
