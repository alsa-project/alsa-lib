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
	size_t size;

	size = dmix->shmptr->s.channels *
	       dmix->shmptr->s.buffer_size *
	       sizeof(signed int);	
	dmix->u.dmix.shmid_sum = shmget(dmix->ipc_key + 1, size, IPC_CREAT | 0666);
	if (dmix->u.dmix.shmid_sum < 0)
		return -errno;
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
	
	/* safe settings for all i386 CPUs */
	dmix->u.dmix.mix_areas1 = mix_areas1_smp;
	/* try to determine, if we have a MMX capable CPU */
	in = fopen("/proc/cpuinfo", "r");
	if (in == NULL)
		return;
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
	// printf("MMX: %i, SMP: %i\n", mmx, smp);
	if (mmx) {
		dmix->u.dmix.mix_areas1 = smp > 1 ? mix_areas1_smp_mmx : mix_areas1_mmx;
	} else {
		dmix->u.dmix.mix_areas1 = smp > 1 ? mix_areas1_smp : mix_areas1;
	}
	dmix->u.dmix.mix_areas2 = smp > 1 ? mix_areas2_smp : mix_areas2;
}
#endif

#ifndef ADD_AND_SATURATE
#warning Please, recode mix_areas1() routine to your architecture...
static void mix_areas1(unsigned int size,
		       volatile signed short *dst, signed short *src,
		       volatile signed int *sum, unsigned int dst_step,
		       unsigned int src_step, unsigned int sum_step)
{
	register signed int sample, old_sample;

	while (size-- > 0) {
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
		((char *)src) += src_step;
		((char *)dst) += dst_step;
		((char *)sum) += sum_step;
	}
}

#warning Please, recode mix_areas2() routine to your architecture...
static void mix_areas2(unsigned int size,
		       volatile signed int *dst, signed int *src,
		       volatile signed int *sum, unsigned int dst_step,
		       unsigned int src_step, unsigned int sum_step)
{
	register signed int sample, old_sample;

	while (size-- > 0) {
		sample = *src >> 8;
		old_sample = *sum;
		if (*dst == 0)
			sample -= old_sample;
		*sum += sample;
		do {
			old_sample = *sum;
			if (old_sample > 0x7fffff)
				sample = 0x7fffff;
			else if (old_sample < -0x800000)
				sample = -0x800000;
			else
				sample = old_sample;
			*dst = sample << 8;
		} while (*sum != old_sample);
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
	
	channels = dmix->u.dmix.channels;
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
			dchn = dmix->u.dmix.bindings ? dmix->u.dmix.bindings[chn] : chn;
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
			dchn = dmix->u.dmix.bindings ? dmix->u.dmix.bindings[chn] : chn;
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
		transfer = slave_appl_ptr + transfer > dmix->shmptr->s.buffer_size ? dmix->shmptr->s.buffer_size - slave_appl_ptr : transfer;
		size -= transfer;
		mix_areas(dmix, src_areas, dst_areas, appl_ptr, slave_appl_ptr, transfer);
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
	// printf("sync ptr diff = %li\n", diff);
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

static int snd_pcm_dmix_nonblock(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int nonblock ATTRIBUTE_UNUSED)
{
	/* value is cached for us in pcm->mode (SND_PCM_NONBLOCK flag) */
	return 0;
}

static int snd_pcm_dmix_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	return snd_timer_async(dmix->timer, sig, pid);
}

static int snd_pcm_dmix_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	unsigned short events;
	static snd_timer_read_t rbuf[5];	/* can be overwriten by multiple plugins, we don't need the value */

	assert(pfds && nfds == 1 && revents);
	events = pfds[0].revents;
	if (events & POLLIN) {
		events |= POLLOUT;
		events &= ~POLLIN;
		/* empty the timer read queue */
		while (snd_timer_read(dmix->timer, &rbuf, sizeof(rbuf)) == sizeof(rbuf)) ;
	}
	*revents = events;
	return 0;
}

static int snd_pcm_dmix_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	// snd_pcm_direct_t *dmix = pcm->private_data;

	memset(info, 0, sizeof(*info));
	info->stream = pcm->stream;
	info->card = -1;
	/* FIXME: fill this with something more useful: we know the hardware name */
	strncpy(info->id, pcm->name, sizeof(info->id));
	strncpy(info->name, pcm->name, sizeof(info->name));
	strncpy(info->subname, pcm->name, sizeof(info->subname));
	info->subdevices_count = 1;
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

static int hw_param_interval_refine_one(snd_pcm_hw_params_t *params,
					snd_pcm_hw_param_t var,
					snd_pcm_hw_params_t *src)
{
	snd_interval_t *i;

	if (!(params->rmask & (1<<var)))	/* nothing to do? */
		return 0;
	i = hw_param_interval(params, var);
	if (snd_interval_empty(i)) {
		SNDERR("dmix interval %i empty?", (int)var);
		return -EINVAL;
	}
	if (snd_interval_refine(i, hw_param_interval(src, var)))
		params->cmask |= 1<<var;
	return 0;
}

#undef REFINE_DEBUG

static int snd_pcm_dmix_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_direct_t *dmix = pcm->private_data;
	snd_pcm_hw_params_t *hw_params = &dmix->shmptr->hw_params;
	static snd_mask_t access = { .bits = { 
					(1<<SNDRV_PCM_ACCESS_MMAP_INTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_RW_INTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_RW_NONINTERLEAVED),
					0, 0, 0 } };
	int err;

#ifdef REFINE_DEBUG
	snd_output_t *log;
	snd_output_stdio_attach(&log, stderr, 0);
	snd_output_puts(log, "DMIX REFINE (begin):\n");
	snd_pcm_hw_params_dump(params, log);
#endif
	if (params->rmask & (1<<SND_PCM_HW_PARAM_ACCESS)) {
		if (snd_mask_empty(hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS))) {
			SNDERR("dmix access mask empty?");
			return -EINVAL;
		}
		if (snd_mask_refine(hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS), &access))
			params->cmask |= 1<<SND_PCM_HW_PARAM_ACCESS;
	}
	if (params->rmask & (1<<SND_PCM_HW_PARAM_FORMAT)) {
		if (snd_mask_empty(hw_param_mask(params, SND_PCM_HW_PARAM_FORMAT))) {
			SNDERR("dmix format mask empty?");
			return -EINVAL;
		}
		if (snd_mask_refine_set(hw_param_mask(params, SND_PCM_HW_PARAM_FORMAT),
				        snd_mask_value(hw_param_mask(hw_params, SND_PCM_HW_PARAM_FORMAT))))
			params->cmask |= 1<<SND_PCM_HW_PARAM_FORMAT;
	}
	//snd_mask_none(hw_param_mask(params, SND_PCM_HW_PARAM_SUBFORMAT));
	if (params->rmask & (1<<SND_PCM_HW_PARAM_CHANNELS)) {
		if (snd_interval_empty(hw_param_interval(params, SND_PCM_HW_PARAM_CHANNELS))) {
			SNDERR("dmix channels mask empty?");
			return -EINVAL;
		}
		err = snd_interval_refine_set(hw_param_interval(params, SND_PCM_HW_PARAM_CHANNELS), dmix->u.dmix.channels);
		if (err < 0)
			return err;
	}
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_RATE, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_BUFFER_SIZE, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_BUFFER_TIME, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_PERIOD_SIZE, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_PERIOD_TIME, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_PERIODS, hw_params);
	if (err < 0)
		return err;
#ifdef REFINE_DEBUG
	snd_output_puts(log, "DMIX REFINE (end):\n");
	snd_pcm_hw_params_dump(params, log);
	snd_output_close(log);
#endif
	return 0;
}

static int snd_pcm_dmix_hw_params(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t * params ATTRIBUTE_UNUSED)
{
	/* values are cached in the pcm structure */
	
	return 0;
}

static int snd_pcm_dmix_hw_free(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	/* values are cached in the pcm structure */
	return 0;
}

static int snd_pcm_dmix_sw_params(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t * params ATTRIBUTE_UNUSED)
{
	/* values are cached in the pcm structure */
	return 0;
}

static int snd_pcm_dmix_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
        return snd_pcm_channel_info_shm(pcm, info, -1);
}

static int snd_pcm_dmix_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_direct_t *dmix = pcm->private_data;

	memset(status, 0, sizeof(*status));
	status->state = dmix->state;
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

	avail = snd_pcm_mmap_avail(pcm);
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

static int snd_pcm_dmix_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_dmix_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
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
	if (dmix->u.dmix.bindings)
		free(dmix->u.dmix.bindings);
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
	close: snd_pcm_dmix_close,
	info: snd_pcm_dmix_info,
	hw_refine: snd_pcm_dmix_hw_refine,
	hw_params: snd_pcm_dmix_hw_params,
	hw_free: snd_pcm_dmix_hw_free,
	sw_params: snd_pcm_dmix_sw_params,
	channel_info: snd_pcm_dmix_channel_info,
	dump: snd_pcm_dmix_dump,
	nonblock: snd_pcm_dmix_nonblock,
	async: snd_pcm_dmix_async,
	poll_revents: snd_pcm_dmix_poll_revents,
	mmap: snd_pcm_dmix_mmap,
	munmap: snd_pcm_dmix_munmap,
};

static snd_pcm_fast_ops_t snd_pcm_dmix_fast_ops = {
	status: snd_pcm_dmix_status,
	state: snd_pcm_dmix_state,
	hwsync: snd_pcm_dmix_hwsync,
	delay: snd_pcm_dmix_delay,
	prepare: snd_pcm_dmix_prepare,
	reset: snd_pcm_dmix_reset,
	start: snd_pcm_dmix_start,
	drop: snd_pcm_dmix_drop,
	drain: snd_pcm_dmix_drain,
	pause: snd_pcm_dmix_pause,
	rewind: snd_pcm_dmix_rewind,
	forward: snd_pcm_dmix_forward,
	resume: snd_pcm_dmix_resume,
	writei: snd_pcm_mmap_writei,
	writen: snd_pcm_mmap_writen,
	readi: snd_pcm_dmix_readi,
	readn: snd_pcm_dmix_readn,
	avail_update: snd_pcm_dmix_avail_update,
	mmap_commit: snd_pcm_dmix_mmap_commit,
};

/*
 * parse the channel map
 * id == client channel
 * value == slave's channel
 */
static int parse_bindings(snd_pcm_direct_t *dmix, snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	unsigned int chn, chn1, count = 0;
	int err;

	dmix->u.dmix.channels = UINT_MAX;
	if (cfg == NULL)
		return 0;
	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("invalid type for bindings");
		return -EINVAL;
	}
	snd_config_for_each(i, next, cfg) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		long cchannel;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		err = safe_strtol(id, &cchannel);
		if (err < 0 || cchannel < 0) {
			SNDERR("invalid client channel in binding: %s\n", id);
			return -EINVAL;
		}
		if ((unsigned)cchannel > count)
			count = cchannel + 1;
	}
	if (count == 0)
		return 0;
	if (count > 1024) {
		SNDERR("client channel out of range");
		return -EINVAL;
	}
	dmix->u.dmix.bindings = malloc(count * sizeof(unsigned int));
	for (chn = 0; chn < count; chn++)
		dmix->u.dmix.bindings[chn] = UINT_MAX;		/* don't route */
	snd_config_for_each(i, next, cfg) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		long cchannel, schannel;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		safe_strtol(id, &cchannel);
		if (snd_config_get_integer(n, &schannel) < 0) {
			SNDERR("unable to get slave channel (should be integer type) in binding: %s\n", id);
			return -EINVAL;
		}
		dmix->u.dmix.bindings[cchannel] = schannel;
	}
	for (chn = 0; chn < count; chn++) {
		for (chn1 = 0; chn1 < count; chn1++) {
			if (chn == chn1)
				continue;
			if (dmix->u.dmix.bindings[chn] == dmix->u.dmix.bindings[chn1]) {
				SNDERR("unable to route channels %d,%d to same destination %d", chn, chn1, dmix->u.dmix.bindings[chn]);
				return -EINVAL;
			}
		}
	}
	dmix->u.dmix.channels = count;
	return 0;
}

/**
 * \brief Creates a new dmix PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param ipc_key IPC key for semaphore and shared memory
 * \param params Parameters for slave
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
		      key_t ipc_key, struct slave_params *params,
		      snd_config_t *bindings,
		      snd_config_t *root, snd_config_t *sconf,
		      snd_pcm_stream_t stream, int mode)
{
	snd_pcm_t *pcm = NULL, *spcm = NULL;
	snd_pcm_direct_t *dmix = NULL;
	int ret, first_instance;

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
	
	ret = parse_bindings(dmix, bindings);
	if (ret < 0)
		goto _err;
	
	dmix->ipc_key = ipc_key;
	dmix->semid = -1;
	dmix->shmid = -1;

	ret = snd_pcm_new(&pcm, SND_PCM_TYPE_DMIX, name, stream, mode);
	if (ret < 0)
		goto _err;

	ret = snd_pcm_direct_semaphore_create_or_connect(dmix);
	if (ret < 0) {
		SNDERR("unable to create IPC semaphore");
		goto _err;
	}
	
	ret = snd_pcm_direct_semaphore_down(dmix, DIRECT_IPC_SEM_CLIENT);
	if (ret < 0) {
		snd_pcm_direct_semaphore_discard(dmix);
		goto _err;
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
		ret = snd_pcm_mmap(spcm);
		if (ret < 0) {
			SNDERR("unable to mmap channels");
			goto _err;
		}
		dmix->spcm = spcm;
	}

	ret = shm_sum_create_or_connect(dmix);
	if (ret < 0) {
		SNDERR("unabel to initialize sum ring buffer");
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
	
	if (dmix->u.dmix.channels == UINT_MAX)
		dmix->u.dmix.channels = dmix->shmptr->s.channels;
	
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
 		if (dmix->u.dmix.bindings)
 			free(dmix->u.dmix.bindings);
		free(dmix);
	}
	if (pcm)
		snd_pcm_free(pcm);
	return ret;
}

/*! \page pcm_plugins

\section pcm_plugins_dmix Plugin: dmix

This plugin provides direct mixing of multiple streams.

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

\subsection pcm_plugins_hw_funcref Function reference

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
	int bsize, psize, ipc_key_add_uid = 0;
	key_t ipc_key = 0;
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
				 SND_PCM_HW_PARAM_PERIOD_SIZE, 0, &bsize,
				 SND_PCM_HW_PARAM_BUFFER_SIZE, 0, &psize,
				 SND_PCM_HW_PARAM_PERIODS, 0, &params.periods);
	if (err < 0)
		return err;

	/* sorry, limited features */
	if (params.format != SND_PCM_FORMAT_S16 &&
	    params.format != SND_PCM_FORMAT_S32) {
		SNDERR("invalid format, specify s16 or s32");
		snd_config_delete(sconf);
		return -EINVAL;
	}

	params.period_size = psize;
	params.buffer_size = bsize;
	err = snd_pcm_dmix_open(pcmp, name, ipc_key, &params, bindings, root, sconf, stream, mode);
	if (err < 0)
		snd_config_delete(sconf);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_dmix_open, SND_PCM_DLSYM_VERSION);
#endif
