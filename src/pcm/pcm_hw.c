/**
 * \file pcm/pcm_hw.c
 * \ingroup PCM_Plugins
 * \brief PCM HW Plugin Interface
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2000-2001
 */
/*
 *  PCM - Hardware
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
#include "pcm_local.h"
#include "../control/control_local.h"

//#define DEBUG_RW		/* use to debug readi/writei/readn/writen */
//#define DEBUG_MMAP		/* debug mmap_commit */

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_hw = "";
#endif

#ifndef DOC_HIDDEN

#ifndef F_SETSIG
#define F_SETSIG 10
#endif

/*
 *  Compatibility
 */

struct sndrv_pcm_hw_params_old {
	unsigned int flags;
	unsigned int masks[SNDRV_PCM_HW_PARAM_SUBFORMAT -
			   SNDRV_PCM_HW_PARAM_ACCESS + 1];
	struct sndrv_interval intervals[SNDRV_PCM_HW_PARAM_TICK_TIME -
					SNDRV_PCM_HW_PARAM_SAMPLE_BITS + 1];
	unsigned int rmask;
	unsigned int cmask;
	unsigned int info;
	unsigned int msbits;
	unsigned int rate_num;
	unsigned int rate_den;
	sndrv_pcm_uframes_t fifo_size;
	unsigned char reserved[64];
};

#define SND_PCM_IOCTL_HW_REFINE_OLD _IOWR('A', 0x10, struct sndrv_pcm_hw_params_old)
#define SND_PCM_IOCTL_HW_PARAMS_OLD _IOWR('A', 0x11, struct sndrv_pcm_hw_params_old)

static int use_old_hw_params_ioctl(int fd, unsigned int cmd, snd_pcm_hw_params_t *params);
static snd_pcm_sframes_t snd_pcm_hw_avail_update(snd_pcm_t *pcm);

/*
 *
 */

typedef struct {
	int version;
	int fd;
	int card, device, subdevice;
	int mmap_emulation;
	volatile struct sndrv_pcm_mmap_status * mmap_status;
	struct sndrv_pcm_mmap_control *mmap_control;
	int shadow_appl_ptr: 1,
	    avail_update_flag: 1,
	    mmap_shm: 1;
	snd_pcm_uframes_t appl_ptr;
} snd_pcm_hw_t;

#define SNDRV_FILE_PCM_STREAM_PLAYBACK		"/dev/snd/pcmC%iD%ip"
#define SNDRV_FILE_PCM_STREAM_CAPTURE		"/dev/snd/pcmC%iD%ic"
#define SNDRV_PCM_VERSION_MAX			SNDRV_PROTOCOL_VERSION(2, 0, 4)

/* update appl_ptr with driver */
#define UPDATE_SHADOW_PTR(hw) \
	do { if (hw->shadow_appl_ptr && !hw->avail_update_flag) \
	       hw->appl_ptr = hw->mmap_control->appl_ptr; } while (0)
#define FAST_PCM_STATE(hw) \
	((enum sndrv_pcm_state) (hw)->mmap_status->state)
#define FAST_PCM_TSTAMP(hw) \
	((hw)->mmap_status->tstamp)

#endif /* DOC_HIDDEN */

struct timespec snd_pcm_hw_fast_tstamp(snd_pcm_t *pcm)
{
	struct timespec res;
	snd_pcm_hw_t *hw = pcm->private_data;
	res = FAST_PCM_TSTAMP(hw);
	if (SNDRV_PROTOCOL_VERSION(2, 0, 5) > hw->version)
		res.tv_nsec *= 1000L;
	return res;
}

static int snd_pcm_hw_nonblock(snd_pcm_t *pcm, int nonblock)
{
	long flags;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;

	if ((flags = fcntl(fd, F_GETFL)) < 0) {
		err = -errno;
		SYSERR("F_GETFL failed");
		return err;
	}
	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) {
		err = -errno;
		SYSERR("F_SETFL for O_NONBLOCK failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	long flags;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;

	if ((flags = fcntl(fd, F_GETFL)) < 0) {
		err = -errno;
		SYSERR("F_GETFL failed");
		return err;
	}
	if (sig >= 0)
		flags |= O_ASYNC;
	else
		flags &= ~O_ASYNC;
	if (fcntl(fd, F_SETFL, flags) < 0) {
		err = -errno;
		SYSERR("F_SETFL for O_ASYNC failed");
		return err;
	}
	if (sig < 0)
		return 0;
	if (fcntl(fd, F_SETSIG, (long)sig) < 0) {
		err = -errno;
		SYSERR("F_SETSIG failed");
		return err;
	}
	if (fcntl(fd, F_SETOWN, (long)pid) < 0) {
		err = -errno;
		SYSERR("F_SETOWN failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if (ioctl(fd, SNDRV_PCM_IOCTL_INFO, info) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_INFO failed");
		return err;
	}
	return 0;
}

static inline int hw_refine_call(snd_pcm_hw_t *pcm_hw, snd_pcm_hw_params_t *params)
{
	/* check for new hw_params structure; it's available from 2.0.2 version of PCM API */
	if (SNDRV_PROTOCOL_VERSION(2, 0, 2) <= pcm_hw->version)
		return ioctl(pcm_hw->fd, SNDRV_PCM_IOCTL_HW_REFINE, params);
	return use_old_hw_params_ioctl(pcm_hw->fd, SND_PCM_IOCTL_HW_REFINE_OLD, params);
}

static int snd_pcm_hw_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (hw->mmap_emulation) {
		int err = 0;
		snd_pcm_access_mask_t oldmask = *snd_pcm_hw_param_get_mask(params, SND_PCM_HW_PARAM_ACCESS);
		snd_pcm_access_mask_t mask;
		const snd_mask_t *pmask;

		snd_mask_empty(&mask);
		if (hw_refine_call(hw, params) < 0)
			err = -errno;
		if (err < 0) {
			snd_pcm_hw_params_t new = *params;

			if (!(params->rmask & (1<<SND_PCM_HW_PARAM_ACCESS)))
				return err;
			if (snd_pcm_access_mask_test(&oldmask, SND_PCM_ACCESS_MMAP_INTERLEAVED) &&
			    !snd_pcm_access_mask_test(&oldmask, SND_PCM_ACCESS_RW_INTERLEAVED))
				snd_pcm_access_mask_set(&mask, SND_PCM_ACCESS_RW_INTERLEAVED);
			if (snd_pcm_access_mask_test(&oldmask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED) &&
			    !snd_pcm_access_mask_test(&oldmask, SND_PCM_ACCESS_RW_NONINTERLEAVED))
				snd_pcm_access_mask_set(&mask, SND_PCM_ACCESS_RW_NONINTERLEAVED);
			if (snd_pcm_access_mask_empty(&mask))
				return err;
			pmask = snd_pcm_hw_param_get_mask(&new, SND_PCM_HW_PARAM_ACCESS);
			*(snd_mask_t *)pmask = mask;
			if (hw_refine_call(hw, &new) < 0)
				return -errno;
			*params = new;
		}
		pmask = snd_pcm_hw_param_get_mask(params, SND_PCM_HW_PARAM_ACCESS);
		if (snd_pcm_access_mask_test(pmask, SND_PCM_ACCESS_MMAP_INTERLEAVED) ||
		    snd_pcm_access_mask_test(pmask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED) ||
		    snd_pcm_access_mask_test(pmask, SND_PCM_ACCESS_MMAP_COMPLEX))
			return 0;
		if (snd_pcm_access_mask_test(&mask, SND_PCM_ACCESS_RW_INTERLEAVED)) {
			if (snd_pcm_access_mask_test(pmask, SND_PCM_ACCESS_RW_INTERLEAVED))
				snd_pcm_access_mask_set((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
			snd_pcm_access_mask_reset((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_RW_INTERLEAVED);
			params->cmask |= 1<<SND_PCM_HW_PARAM_ACCESS;
		}
		if (snd_pcm_access_mask_test(&mask, SND_PCM_ACCESS_RW_NONINTERLEAVED)) {
			if (snd_pcm_access_mask_test(pmask, SND_PCM_ACCESS_RW_NONINTERLEAVED))
				snd_pcm_access_mask_set((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
			snd_pcm_access_mask_reset((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_RW_NONINTERLEAVED);
			params->cmask |= 1<<SND_PCM_HW_PARAM_ACCESS;
		}
		if (snd_pcm_access_mask_test(&oldmask, SND_PCM_ACCESS_MMAP_INTERLEAVED)) {
			if (snd_pcm_access_mask_test(&oldmask, SND_PCM_ACCESS_RW_INTERLEAVED)) {
				if (snd_pcm_access_mask_test(pmask, SND_PCM_ACCESS_RW_INTERLEAVED)) {
					snd_pcm_access_mask_set((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
					params->cmask |= 1<<SND_PCM_HW_PARAM_ACCESS;
				}
			}
		}
		if (snd_pcm_access_mask_test(&oldmask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED)) {
			if (snd_pcm_access_mask_test(&oldmask, SND_PCM_ACCESS_RW_NONINTERLEAVED)) {
				if (snd_pcm_access_mask_test(pmask, SND_PCM_ACCESS_RW_NONINTERLEAVED)) {
					snd_pcm_access_mask_set((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
					params->cmask |= 1<<SND_PCM_HW_PARAM_ACCESS;
				}
			}
		}
	} else {
		if (hw_refine_call(hw, params) < 0) {
			err = -errno;
			// SYSERR("SNDRV_PCM_IOCTL_HW_REFINE failed");
			return err;
		}
	}
	
	return 0;
}

static inline int hw_params_call(snd_pcm_hw_t *pcm_hw, snd_pcm_hw_params_t *params)
{
	/* check for new hw_params structure; it's available from 2.0.2 version of PCM API */
	if (SNDRV_PROTOCOL_VERSION(2, 0, 2) <= pcm_hw->version)
		return ioctl(pcm_hw->fd, SNDRV_PCM_IOCTL_HW_PARAMS, params);
	return use_old_hw_params_ioctl(pcm_hw->fd, SND_PCM_IOCTL_HW_PARAMS_OLD, params);
}

static int snd_pcm_hw_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (hw->mmap_emulation) {
		snd_pcm_hw_params_t old = *params;
		if (hw_params_call(hw, params) < 0) {
			snd_pcm_access_t access;
			snd_pcm_access_mask_t oldmask;
			const snd_mask_t *pmask;

			*params = old;
			pmask = snd_pcm_hw_param_get_mask(params, SND_PCM_HW_PARAM_ACCESS);
			oldmask = *(snd_pcm_access_mask_t *)pmask;
			if (INTERNAL(snd_pcm_hw_params_get_access)(params, &access) < 0)
				goto _err;
			switch (access) {
			case SND_PCM_ACCESS_MMAP_INTERLEAVED:
				snd_pcm_access_mask_reset((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
				snd_pcm_access_mask_set((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_RW_INTERLEAVED);
				break;
			case SND_PCM_ACCESS_MMAP_NONINTERLEAVED:
				snd_pcm_access_mask_reset((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
				snd_pcm_access_mask_set((snd_pcm_access_mask_t *)pmask, SND_PCM_ACCESS_RW_NONINTERLEAVED);
				break;
			default:
				goto _err;
			}
			if (hw_params_call(hw, params) < 0)
				goto _err;
			hw->mmap_shm = 1;
			*(snd_pcm_access_mask_t *)pmask = oldmask;
		}
	} else {
		if (hw_params_call(hw, params) < 0) {
		      _err:
			err = -errno;
			SYSERR("SNDRV_PCM_IOCTL_HW_PARAMS failed");
			return err;
		}
	}
	if (pcm->stream == SND_PCM_STREAM_CAPTURE) {
		if (hw->mmap_shm) {
			hw->shadow_appl_ptr = 1;
			hw->appl_ptr = 0;
			snd_pcm_set_appl_ptr(pcm, &hw->appl_ptr, -1, 0);
		} else {
			hw->shadow_appl_ptr = 0;
			snd_pcm_set_appl_ptr(pcm, &hw->mmap_control->appl_ptr, hw->fd, SNDRV_PCM_MMAP_OFFSET_CONTROL);
		}
	}
	return 0;
}

static int snd_pcm_hw_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if (ioctl(fd, SNDRV_PCM_IOCTL_HW_FREE) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_HW_FREE failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if ((snd_pcm_tstamp_t) params->tstamp_mode == pcm->tstamp_mode &&
	    params->period_step == pcm->period_step &&
	    params->sleep_min == pcm->sleep_min &&
	    params->xfer_align == pcm->xfer_align &&
	    params->start_threshold == pcm->start_threshold &&
	    params->stop_threshold == pcm->stop_threshold &&
	    params->silence_threshold == pcm->silence_threshold &&
	    params->silence_size == pcm->silence_size) {
		hw->mmap_control->avail_min = params->avail_min;
		return 0;
	}
	if (ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, params) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_SW_PARAMS failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	struct sndrv_pcm_channel_info i;
	int fd = hw->fd, err;
	i.channel = info->channel;
	if (ioctl(fd, SNDRV_PCM_IOCTL_CHANNEL_INFO, &i) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_CHANNEL_INFO failed");
		return err;
	}
	info->channel = i.channel;
	if (!hw->mmap_shm) {
		info->addr = 0;
		info->first = i.first;
		info->step = i.step;
		info->type = SND_PCM_AREA_MMAP;
		info->u.mmap.fd = fd;
		info->u.mmap.offset = i.offset;
		return 0;
	}
	return snd_pcm_channel_info_shm(pcm, info, -1);
}

static int snd_pcm_hw_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if (ioctl(fd, SNDRV_PCM_IOCTL_STATUS, status) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_STATUS failed");
		return err;
	}
	if (SNDRV_PROTOCOL_VERSION(2, 0, 5) > hw->version) {
		status->tstamp.tv_nsec *= 1000L;
		status->trigger_tstamp.tv_nsec *= 1000L;
	}
	return 0;
}

static snd_pcm_state_t snd_pcm_hw_state(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	return (snd_pcm_state_t) hw->mmap_status->state;
}

static int snd_pcm_hw_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if (ioctl(fd, SNDRV_PCM_IOCTL_DELAY, delayp) < 0) {
		err = -errno;
		// SYSERR("SNDRV_PCM_IOCTL_DELAY failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if (SNDRV_PROTOCOL_VERSION(2, 0, 3) <= hw->version) {
		if (ioctl(fd, SNDRV_PCM_IOCTL_HWSYNC) < 0) {
			err = -errno;
			// SYSERR("SNDRV_PCM_IOCTL_HWSYNC failed");
			return err;
		}
	} else {
		snd_pcm_sframes_t delay;
		int err = snd_pcm_hw_delay(pcm, &delay);
		if (err < 0) {
			switch (snd_pcm_state(pcm)) {
			case SND_PCM_STATE_PREPARED:
			case SND_PCM_STATE_SUSPENDED:
				return 0;
			default:
				return err;
			}
		}
	}
	return 0;
}

static int snd_pcm_hw_prepare(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_PREPARE failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_reset(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if (ioctl(fd, SNDRV_PCM_IOCTL_RESET) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_RESET failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_start(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
#if 0
	assert(pcm->stream != SND_PCM_STREAM_PLAYBACK ||
	       snd_pcm_mmap_playback_hw_avail(pcm) > 0);
#endif
	if (ioctl(hw->fd, SNDRV_PCM_IOCTL_START) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_START failed");
#if 0
		if (err == -EBADFD)
			SNDERR("PCM state = %s", snd_pcm_state_name(snd_pcm_hw_state(pcm)));
#endif
		return err;
	}
	return 0;
}

static int snd_pcm_hw_drop(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (ioctl(hw->fd, SNDRV_PCM_IOCTL_DROP) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_DROP failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_drain(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (ioctl(hw->fd, SNDRV_PCM_IOCTL_DRAIN) < 0) {
		err = -errno;
		if (err != -EAGAIN)
			SYSERR("SNDRV_PCM_IOCTL_DRAIN failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (ioctl(hw->fd, SNDRV_PCM_IOCTL_PAUSE, enable) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_PAUSE failed");
		return err;
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_hw_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (ioctl(hw->fd, SNDRV_PCM_IOCTL_REWIND, &frames) < 0) {
		err = -errno;
		SYSERR("SNDRV_PCM_IOCTL_REWIND failed");
		return err;
	}
	return frames;
}

static snd_pcm_sframes_t snd_pcm_hw_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (SNDRV_PROTOCOL_VERSION(2, 0, 4) <= hw->version) {
		if (ioctl(hw->fd, SNDRV_PCM_IOCTL_FORWARD, &frames) < 0) {
			err = -errno;
			SYSERR("SNDRV_PCM_IOCTL_FORWARD failed");
			return err;
		}
		return frames;
	} else {
		snd_pcm_sframes_t avail;

		switch (FAST_PCM_STATE(hw)) {
		case SNDRV_PCM_STATE_RUNNING:
		case SNDRV_PCM_STATE_DRAINING:
		case SNDRV_PCM_STATE_PAUSED:
		case SNDRV_PCM_STATE_PREPARED:
			break;
		case SNDRV_PCM_STATE_XRUN:
			return -EPIPE;
		default:
			return -EBADFD;
		}
		avail = snd_pcm_mmap_avail(pcm);
		if (avail < 0)
			return 0;
		if (frames > (snd_pcm_uframes_t)avail)
			frames = avail;
		snd_pcm_mmap_appl_forward(pcm, frames);
		return frames;
	}
}

static int snd_pcm_hw_resume(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd, err;
	if (ioctl(fd, SNDRV_PCM_IOCTL_RESUME) < 0) {
		err = -errno;
		if (err != -ENXIO && err != -ENOSYS)
			SYSERR("SNDRV_PCM_IOCTL_RESUME failed");
		return err;
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_hw_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	int result, err;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	struct sndrv_xferi xferi;
	xferi.buf = (char*) buffer;
	xferi.frames = size;
	result = ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &xferi);
	err = -errno;
#ifdef DEBUG_RW
	fprintf(stderr, "hw_writei: frames = %li, result = %i, result = %li\n", size, result, xferi.result);
#endif
	if (result < 0)
		return err;
	return xferi.result;
}

static snd_pcm_sframes_t snd_pcm_hw_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	int result, err;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	struct sndrv_xfern xfern;
	xfern.bufs = bufs;
	xfern.frames = size;
	result = ioctl(fd, SNDRV_PCM_IOCTL_WRITEN_FRAMES, &xfern);
	err = -errno;
#ifdef DEBUG_RW
	fprintf(stderr, "hw_writen: frames = %li, result = %i, result = %li\n", size, result, xfern.result);
#endif
	if (result < 0)
		return err;
	return xfern.result;
}

static snd_pcm_sframes_t snd_pcm_hw_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	int result, err;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	struct sndrv_xferi xferi;
	xferi.buf = buffer;
	xferi.frames = size;
	result = ioctl(fd, SNDRV_PCM_IOCTL_READI_FRAMES, &xferi);
	err = -errno;
#ifdef DEBUG_RW
	fprintf(stderr, "hw_readi: frames = %li, result = %i, result = %li\n", size, result, xferi.result);
#endif
	if (result < 0)
		return err;
	UPDATE_SHADOW_PTR(hw);
	return xferi.result;
}

static snd_pcm_sframes_t snd_pcm_hw_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	int result, err;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	struct sndrv_xfern xfern;
	xfern.bufs = bufs;
	xfern.frames = size;
	result = ioctl(fd, SNDRV_PCM_IOCTL_READN_FRAMES, &xfern);
	err = -errno;
#ifdef DEBUG_RW
	fprintf(stderr, "hw_readn: frames = %li, result = %i, result = %li\n", size, result, xfern.result);
#endif
	if (result < 0)
		return err;
	UPDATE_SHADOW_PTR(hw);
	return xfern.result;
}

static int snd_pcm_hw_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	void *ptr;
	int err;
	ptr = mmap(NULL, page_align(sizeof(struct sndrv_pcm_mmap_status)),
		   PROT_READ, MAP_FILE|MAP_SHARED, 
		   hw->fd, SNDRV_PCM_MMAP_OFFSET_STATUS);
	if (ptr == MAP_FAILED || ptr == NULL) {
		err = -errno;
		SYSERR("status mmap failed");
		return err;
	}
	hw->mmap_status = ptr;
	snd_pcm_set_hw_ptr(pcm, &hw->mmap_status->hw_ptr, hw->fd, SNDRV_PCM_MMAP_OFFSET_STATUS + offsetof(struct sndrv_pcm_mmap_status, hw_ptr));
	return 0;
}

static int snd_pcm_hw_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	void *ptr;
	int err;
	ptr = mmap(NULL, page_align(sizeof(struct sndrv_pcm_mmap_control)),
		   PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, 
		   hw->fd, SNDRV_PCM_MMAP_OFFSET_CONTROL);
	if (ptr == MAP_FAILED || ptr == NULL) {
		err = -errno;
		SYSERR("control mmap failed");
		return err;
	}
	hw->mmap_control = ptr;
	snd_pcm_set_appl_ptr(pcm, &hw->mmap_control->appl_ptr, hw->fd, SNDRV_PCM_MMAP_OFFSET_CONTROL);
	return 0;
}

static int snd_pcm_hw_munmap_status(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (munmap((void*)hw->mmap_status, page_align(sizeof(*hw->mmap_status))) < 0) {
		err = -errno;
		SYSERR("status munmap failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_munmap_control(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (munmap(hw->mmap_control, page_align(sizeof(*hw->mmap_control))) < 0) {
		err = -errno;
		SYSERR("control munmap failed");
		return err;
	}
	return 0;
}

static int snd_pcm_hw_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_hw_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_hw_close(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int err;
	if (close(hw->fd)) {
		err = -errno;
		SYSERR("close failed\n");
		return err;
	}
	snd_pcm_hw_munmap_status(pcm);
	snd_pcm_hw_munmap_control(pcm);
	free(hw);
	return 0;
}

static snd_pcm_sframes_t snd_pcm_hw_mmap_commit(snd_pcm_t *pcm,
						snd_pcm_uframes_t offset ATTRIBUTE_UNUSED,
						snd_pcm_uframes_t size)
{
	snd_pcm_hw_t *hw = pcm->private_data;

	if (hw->mmap_shm) {
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		    	snd_pcm_sframes_t result = 0, res;

			do {
				res = snd_pcm_write_mmap(pcm, size);
				if (res < 0)
					return result > 0 ? result : res;
				size -= res;
				result += res;
			} while (size > 0);
			return result;
		} else {
			snd_pcm_hw_t *hw = pcm->private_data;
			assert(hw->shadow_appl_ptr);
		}
	}
	snd_pcm_mmap_appl_forward(pcm, size);
#ifdef DEBUG_MMAP
	fprintf(stderr, "appl_forward: hw_ptr = %li, appl_ptr = %li, size = %li\n", *pcm->hw.ptr, *pcm->appl.ptr, size);
#endif
	return size;
}

static snd_pcm_sframes_t snd_pcm_hw_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	snd_pcm_uframes_t avail;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		avail = snd_pcm_mmap_playback_avail(pcm);
	} else {
		avail = snd_pcm_mmap_capture_avail(pcm);
		if (avail > 0 && hw->mmap_shm) {
			snd_pcm_sframes_t err;
			snd_pcm_hw_t *hw = pcm->private_data;
			hw->avail_update_flag = 1;
			err = snd_pcm_read_mmap(pcm, avail);
			hw->avail_update_flag = 0;
			if (err < 0)
				return err;
			assert((snd_pcm_uframes_t)err == avail);
			return err;
		}
	}
	switch (FAST_PCM_STATE(hw)) {
	case SNDRV_PCM_STATE_RUNNING:
		if (avail >= pcm->stop_threshold) {
			/* SNDRV_PCM_IOCTL_XRUN ioctl has been implemented since PCM kernel API 2.0.1 */
			if (SNDRV_PROTOCOL_VERSION(2, 0, 1) <= hw->version) {
				if (ioctl(hw->fd, SNDRV_PCM_IOCTL_XRUN) < 0)
					return -errno;
			}
			/* everything is ok, state == SND_PCM_STATE_XRUN at the moment */
			return -EPIPE;
		}
		break;
	case SNDRV_PCM_STATE_XRUN:
		return -EPIPE;
	default:
		break;
	}
	return avail;
}

static void snd_pcm_hw_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	char *name;
	int err = snd_card_get_name(hw->card, &name);
	assert(err >= 0);
	snd_output_printf(out, "Hardware PCM card %d '%s' device %d subdevice %d\n",
			  hw->card, name, hw->device, hw->subdevice);
	free(name);
	if (pcm->setup) {
		snd_output_printf(out, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
}

static snd_pcm_ops_t snd_pcm_hw_ops = {
	.close = snd_pcm_hw_close,
	.info = snd_pcm_hw_info,
	.hw_refine = snd_pcm_hw_hw_refine,
	.hw_params = snd_pcm_hw_hw_params,
	.hw_free = snd_pcm_hw_hw_free,
	.sw_params = snd_pcm_hw_sw_params,
	.channel_info = snd_pcm_hw_channel_info,
	.dump = snd_pcm_hw_dump,
	.nonblock = snd_pcm_hw_nonblock,
	.async = snd_pcm_hw_async,
	.mmap = snd_pcm_hw_mmap,
	.munmap = snd_pcm_hw_munmap,
};

static snd_pcm_fast_ops_t snd_pcm_hw_fast_ops = {
	.status = snd_pcm_hw_status,
	.state = snd_pcm_hw_state,
	.hwsync = snd_pcm_hw_hwsync,
	.delay = snd_pcm_hw_delay,
	.prepare = snd_pcm_hw_prepare,
	.reset = snd_pcm_hw_reset,
	.start = snd_pcm_hw_start,
	.drop = snd_pcm_hw_drop,
	.drain = snd_pcm_hw_drain,
	.pause = snd_pcm_hw_pause,
	.rewind = snd_pcm_hw_rewind,
	.forward = snd_pcm_hw_forward,
	.resume = snd_pcm_hw_resume,
	.writei = snd_pcm_hw_writei,
	.writen = snd_pcm_hw_writen,
	.readi = snd_pcm_hw_readi,
	.readn = snd_pcm_hw_readn,
	.avail_update = snd_pcm_hw_avail_update,
	.mmap_commit = snd_pcm_hw_mmap_commit,
};

/**
 * \brief Creates a new hw PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param fd File descriptor
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_hw_open_fd(snd_pcm_t **pcmp, const char *name,
		       int fd, int mmap_emulation)
{
	int ver;
	long fmode;
	int mode;
	snd_pcm_t *pcm = NULL;
	snd_pcm_hw_t *hw = NULL;
	snd_pcm_info_t info;
	int ret;

	assert(pcmp);

	memset(&info, 0, sizeof(info));
	if (ioctl(fd, SNDRV_PCM_IOCTL_INFO, &info) < 0) {
		ret = -errno;
		SYSERR("SNDRV_PCM_IOCTL_INFO failed");
		close(fd);
		return ret;

	}

	if ((fmode = fcntl(fd, F_GETFL)) < 0) {
		ret = -errno;
		close(fd);
		return ret;
	}
	mode = 0;
	if (fmode & O_NONBLOCK)
		mode |= SND_PCM_NONBLOCK;
	if (fmode & O_ASYNC)
		mode |= SND_PCM_ASYNC;

#if 0
	/*
	 * this is bogus, an application have to care about open filedescriptors
	 */
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
		ret = -errno;
		SYSERR("fcntl FD_CLOEXEC failed");
		close(fd);
		return ret;
	}
#endif

	if (ioctl(fd, SNDRV_PCM_IOCTL_PVERSION, &ver) < 0) {
		ret = -errno;
		SYSERR("SNDRV_PCM_IOCTL_PVERSION failed");
		close(fd);
		return ret;
	}
	if (SNDRV_PROTOCOL_INCOMPATIBLE(ver, SNDRV_PCM_VERSION_MAX))
		return -SND_ERROR_INCOMPATIBLE_VERSION;

	if (SNDRV_PROTOCOL_VERSION(2, 0, 5) <= ver) {
		int on = 1;
		if (ioctl(fd, SNDRV_PCM_IOCTL_TSTAMP, &on) < 0) {
			ret = -errno;
			SNDERR("TSTAMP failed\n");
			return ret;
		}			
	}
	
	hw = calloc(1, sizeof(snd_pcm_hw_t));
	if (!hw) {
		close(fd);
		return -ENOMEM;
	}

	hw->version = ver;
	hw->card = info.card;
	hw->device = info.device;
	hw->subdevice = info.subdevice;
	hw->fd = fd;
	hw->mmap_emulation = mmap_emulation;

	ret = snd_pcm_new(&pcm, SND_PCM_TYPE_HW, name, info.stream, mode);
	if (ret < 0) {
		free(hw);
		close(fd);
		return ret;
	}

	pcm->ops = &snd_pcm_hw_ops;
	pcm->fast_ops = &snd_pcm_hw_fast_ops;
	pcm->private_data = hw;
	pcm->poll_fd = fd;
	pcm->poll_events = info.stream == SND_PCM_STREAM_PLAYBACK ? POLLOUT : POLLIN;

	*pcmp = pcm;

	ret = snd_pcm_hw_mmap_status(pcm);
	if (ret < 0) {
		snd_pcm_close(pcm);
		return ret;
	}
	ret = snd_pcm_hw_mmap_control(pcm);
	if (ret < 0) {
		snd_pcm_close(pcm);
		return ret;
	}
	return 0;
}

/**
 * \brief Creates a new hw PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param card Number of card
 * \param device Number of device
 * \param subdevice Number of subdevice
 * \param stream PCM Stream
 * \param mode PCM Mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_hw_open(snd_pcm_t **pcmp, const char *name,
		    int card, int device, int subdevice,
		    snd_pcm_stream_t stream, int mode,
		    int mmap_emulation)
{
	char filename[32];
	const char *filefmt;
	int ret = 0, fd = -1;
	int attempt = 0;
	snd_pcm_info_t info;
	int fmode;
	snd_ctl_t *ctl;

	assert(pcmp);

	if ((ret = snd_ctl_hw_open(&ctl, NULL, card, 0)) < 0)
		return ret;

	switch (stream) {
	case SND_PCM_STREAM_PLAYBACK:
		filefmt = SNDRV_FILE_PCM_STREAM_PLAYBACK;
		break;
	case SND_PCM_STREAM_CAPTURE:
		filefmt = SNDRV_FILE_PCM_STREAM_CAPTURE;
		break;
	default:
		assert(0);
	}
	sprintf(filename, filefmt, card, device);

      __again:
      	if (attempt++ > 3) {
		ret = -EBUSY;
		goto _err;
	}
	ret = snd_ctl_pcm_prefer_subdevice(ctl, subdevice);
	if (ret < 0)
		goto _err;
	fmode = O_RDWR;
	if (mode & SND_PCM_NONBLOCK)
		fmode |= O_NONBLOCK;
	if (mode & SND_PCM_ASYNC)
		fmode |= O_ASYNC;
	if ((fd = open(filename, fmode)) < 0) {
		ret = -errno;
		SYSERR("open %s failed", filename);
		goto _err;
	}
	if (subdevice >= 0) {
		memset(&info, 0, sizeof(info));
		if (ioctl(fd, SNDRV_PCM_IOCTL_INFO, &info) < 0) {
			ret = -errno;
			SYSERR("SNDRV_PCM_IOCTL_INFO failed");
			goto _err;
		}
		if (info.subdevice != (unsigned int) subdevice) {
			close(fd);
			goto __again;
		}
	}
	snd_ctl_close(ctl);
	return snd_pcm_hw_open_fd(pcmp, name, fd, mmap_emulation);
	
       _err:
	snd_ctl_close(ctl);
	return ret;
}

/*! \page pcm_plugins

\section pcm_plugins_hw Plugin: hw

This plugin communicates directly with the ALSA kernel driver. It is a raw
communication without any conversions. The emulation of mmap access can be
optionally enabled, but expect worse latency in the case.

\code
pcm.name {
	type hw			# Kernel PCM
	card INT/STR		# Card name (string) or number (integer)
	[device INT]		# Device number (default 0)
	[subdevice INT]		# Subdevice number (default -1: first available)
	[mmap_emulation BOOL]	# Enable mmap emulation for ro/wo devices
}
\endcode

\subsection pcm_plugins_hw_funcref Function reference

<UL>
  <LI>snd_pcm_hw_open()
  <LI>_snd_pcm_hw_open()
</UL>

*/

/**
 * \brief Creates a new hw PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with hw PCM description
 * \param stream PCM Stream
 * \param mode PCM Mode
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_hw_open(snd_pcm_t **pcmp, const char *name,
		     snd_config_t *root ATTRIBUTE_UNUSED, snd_config_t *conf,
		     snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	long card = -1, device = 0, subdevice = -1;
	const char *str;
	int err, mmap_emulation = 0;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "card") == 0) {
			err = snd_config_get_integer(n, &card);
			if (err < 0) {
				err = snd_config_get_string(n, &str);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					return -EINVAL;
				}
				card = snd_card_get_index(str);
				if (card < 0) {
					SNDERR("Invalid value for %s", id);
					return card;
				}
			}
			continue;
		}
		if (strcmp(id, "device") == 0) {
			err = snd_config_get_integer(n, &device);
			if (err < 0) {
				SNDERR("Invalid type for %s", id);
				return err;
			}
			continue;
		}
		if (strcmp(id, "subdevice") == 0) {
			err = snd_config_get_integer(n, &subdevice);
			if (err < 0) {
				SNDERR("Invalid type for %s", id);
				return err;
			}
			continue;
		}
		if (strcmp(id, "mmap_emulation") == 0) {
			err = snd_config_get_bool(n);
			if (err < 0)
				continue;
			mmap_emulation = err;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (card < 0) {
		SNDERR("card is not defined");
		return -EINVAL;
	}
	return snd_pcm_hw_open(pcmp, name, card, device, subdevice, stream, mode, mmap_emulation);
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_hw_open, SND_PCM_DLSYM_VERSION);
#endif

/*
 *  To be removed helpers, but keep binary compatibility at the time
 */

#ifndef DOC_HIDDEN
#define __OLD_TO_NEW_MASK(x) ((x&7)|((x&0x07fffff8)<<5))
#define __NEW_TO_OLD_MASK(x) ((x&7)|((x&0xffffff00)>>5))
#endif

static void snd_pcm_hw_convert_from_old_params(snd_pcm_hw_params_t *params,
					       struct sndrv_pcm_hw_params_old *oparams)
{
	unsigned int i;

	memset(params, 0, sizeof(*params));
	params->flags = oparams->flags;
	for (i = 0; i < sizeof(oparams->masks) / sizeof(unsigned int); i++)
		params->masks[i].bits[0] = oparams->masks[i];
	memcpy(params->intervals, oparams->intervals, sizeof(oparams->intervals));
	params->rmask = __OLD_TO_NEW_MASK(oparams->rmask);
	params->cmask = __OLD_TO_NEW_MASK(oparams->cmask);
	params->info = oparams->info;
	params->msbits = oparams->msbits;
	params->rate_num = oparams->rate_num;
	params->rate_den = oparams->rate_den;
	params->fifo_size = oparams->fifo_size;
}

static void snd_pcm_hw_convert_to_old_params(struct sndrv_pcm_hw_params_old *oparams,
					     snd_pcm_hw_params_t *params,
					     unsigned int *cmask)
{
	unsigned int i, j;

	memset(oparams, 0, sizeof(*oparams));
	oparams->flags = params->flags;
	for (i = 0; i < sizeof(oparams->masks) / sizeof(unsigned int); i++) {
		oparams->masks[i] = params->masks[i].bits[0];
		for (j = 1; j < sizeof(params->masks[i].bits) / sizeof(unsigned int); j++)
			if (params->masks[i].bits[j]) {
				*cmask |= 1 << i;
				break;
			}
	}
	memcpy(oparams->intervals, params->intervals, sizeof(oparams->intervals));
	oparams->rmask = __NEW_TO_OLD_MASK(params->rmask);
	oparams->cmask = __NEW_TO_OLD_MASK(params->cmask);
	oparams->info = params->info;
	oparams->msbits = params->msbits;
	oparams->rate_num = params->rate_num;
	oparams->rate_den = params->rate_den;
	oparams->fifo_size = params->fifo_size;
}

static int use_old_hw_params_ioctl(int fd, unsigned int cmd, snd_pcm_hw_params_t *params)
{
	struct sndrv_pcm_hw_params_old oparams;
	unsigned int cmask = 0;
	int res;
	
	snd_pcm_hw_convert_to_old_params(&oparams, params, &cmask);
	res = ioctl(fd, cmd, &oparams);
	snd_pcm_hw_convert_from_old_params(params, &oparams);
	params->cmask |= cmask;
	return res;
}
