/*
 *  PCM - Hardware
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
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include "pcm_local.h"
#include "../control/control_local.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_hw = "";
#endif

#ifndef F_SETSIG
#define F_SETSIG 10
#endif

typedef struct {
	int fd;
	int card, device, subdevice;
	volatile struct sndrv_pcm_mmap_status *mmap_status;
	struct sndrv_pcm_mmap_control *mmap_control;
	int shmid;
} snd_pcm_hw_t;

#define SNDRV_FILE_PCM_STREAM_PLAYBACK		"/dev/snd/pcmC%iD%ip"
#define SNDRV_FILE_PCM_STREAM_CAPTURE		"/dev/snd/pcmC%iD%ic"
#define SNDRV_PCM_VERSION_MAX	SNDRV_PROTOCOL_VERSION(2, 0, 0)

static int snd_pcm_hw_nonblock(snd_pcm_t *pcm, int nonblock)
{
	long flags;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;

	if ((flags = fcntl(fd, F_GETFL)) < 0) {
		SYSERR("F_GETFL failed");
		return -errno;
	}
	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) {
		SYSERR("F_SETFL for O_NONBLOCK failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	long flags;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;

	if ((flags = fcntl(fd, F_GETFL)) < 0) {
		SYSERR("F_GETFL failed");
		return -errno;
	}
	if (sig >= 0)
		flags |= O_ASYNC;
	else
		flags &= ~O_ASYNC;
	if (fcntl(fd, F_SETFL, flags) < 0) {
		SYSERR("F_SETFL for O_ASYNC failed");
		return -errno;
	}
	if (sig < 0)
		return 0;
	if (fcntl(fd, F_SETSIG, (long)sig) < 0) {
		SYSERR("F_SETSIG failed");
		return -errno;
	}
	if (fcntl(fd, F_SETOWN, (long)pid) < 0) {
		SYSERR("F_SETOWN failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_INFO, info) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_INFO failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, params) < 0) {
		// SYSERR("SNDRV_PCM_IOCTL_HW_REFINE failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, params) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_HW_PARAMS failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_HW_FREE) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_HW_FREE failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
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
		SYSERR("SNDRV_PCM_IOCTL_SW_PARAMS failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	struct sndrv_pcm_channel_info i;
	int fd = hw->fd;
	i.channel = info->channel;
	if (ioctl(fd, SNDRV_PCM_IOCTL_CHANNEL_INFO, &i) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_CHANNEL_INFO failed");
		return -errno;
	}
	info->channel = i.channel;
	if (pcm->info & SND_PCM_INFO_MMAP) {
		info->addr = 0;
		info->first = i.first;
		info->step = i.step;
		info->type = SND_PCM_AREA_MMAP;
		info->u.mmap.fd = fd;
		info->u.mmap.offset = i.offset;
		return 0;
	}
	return snd_pcm_channel_info_shm(pcm, info, hw->shmid);
}

static int snd_pcm_hw_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_STATUS, status) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_STATUS failed");
		return -errno;
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
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_DELAY, delayp) < 0) {
		// SYSERR("SNDRV_PCM_IOCTL_DELAY failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_prepare(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_PREPARE failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_reset(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_RESET) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_RESET failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_start(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
#if 0
	assert(pcm->stream != SND_PCM_STREAM_PLAYBACK ||
	       snd_pcm_mmap_playback_hw_avail(pcm) > 0);
#endif
	if (ioctl(fd, SNDRV_PCM_IOCTL_START) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_START failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_drop(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_DROP) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_DROP failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_drain(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_DRAIN) < 0) {
		if (errno != EAGAIN)
			SYSERR("SNDRV_PCM_IOCTL_DRAIN failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_PAUSE, enable) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_PAUSE failed");
		return -errno;
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_hw_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_REWIND, &frames) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_REWIND failed");
		return -errno;
	}
	return frames;
}

static int snd_pcm_hw_resume(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	if (ioctl(fd, SNDRV_PCM_IOCTL_RESUME) < 0) {
		if (errno != ENXIO && errno != ENOSYS)
			SYSERR("SNDRV_PCM_IOCTL_RESUME failed");
		return -errno;
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_hw_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_sframes_t result;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	struct sndrv_xferi xferi;
	xferi.buf = (char*) buffer;
	xferi.frames = size;
	result = ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &xferi);
	if (result < 0)
		return -errno;
	return xferi.result;
}

static snd_pcm_sframes_t snd_pcm_hw_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_sframes_t result;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	struct sndrv_xfern xfern;
	xfern.bufs = bufs;
	xfern.frames = size;
	result = ioctl(fd, SNDRV_PCM_IOCTL_WRITEN_FRAMES, &xfern);
	if (result < 0)
		return -errno;
	return xfern.result;
}

static snd_pcm_sframes_t snd_pcm_hw_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_sframes_t result;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	struct sndrv_xferi xferi;
	xferi.buf = buffer;
	xferi.frames = size;
	result = ioctl(fd, SNDRV_PCM_IOCTL_READI_FRAMES, &xferi);
	if (result < 0)
		return -errno;
	return xferi.result;
}

static snd_pcm_sframes_t snd_pcm_hw_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_sframes_t result;
	snd_pcm_hw_t *hw = pcm->private_data;
	int fd = hw->fd;
	struct sndrv_xfern xfern;
	xfern.bufs = bufs;
	xfern.frames = size;
	result = ioctl(fd, SNDRV_PCM_IOCTL_READN_FRAMES, &xfern);
	if (result < 0)
		return -errno;
	return xfern.result;
}

static int snd_pcm_hw_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	void *ptr;
	ptr = mmap(NULL, page_align(sizeof(struct sndrv_pcm_mmap_status)),
		   PROT_READ, MAP_FILE|MAP_SHARED, 
		   hw->fd, SNDRV_PCM_MMAP_OFFSET_STATUS);
	if (ptr == MAP_FAILED || ptr == NULL) {
		SYSERR("status mmap failed");
		return -errno;
	}
	hw->mmap_status = ptr;
	pcm->hw_ptr = &hw->mmap_status->hw_ptr;
	return 0;
}

static int snd_pcm_hw_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	void *ptr;
	ptr = mmap(NULL, page_align(sizeof(struct sndrv_pcm_mmap_control)),
		   PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, 
		   hw->fd, SNDRV_PCM_MMAP_OFFSET_CONTROL);
	if (ptr == MAP_FAILED || ptr == NULL) {
		SYSERR("control mmap failed");
		return -errno;
	}
	hw->mmap_control = ptr;
	pcm->appl_ptr = &hw->mmap_control->appl_ptr;
	return 0;
}

static int snd_pcm_hw_munmap_status(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	if (munmap((void*)hw->mmap_status, page_align(sizeof(*hw->mmap_status))) < 0) {
		SYSERR("status munmap failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_munmap_control(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	if (munmap(hw->mmap_control, page_align(sizeof(*hw->mmap_control))) < 0) {
		SYSERR("control munmap failed");
		return -errno;
	}
	return 0;
}

static int snd_pcm_hw_mmap(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	if (!(pcm->info & SND_PCM_INFO_MMAP)) {
		snd_pcm_uframes_t size = snd_pcm_frames_to_bytes(pcm, (snd_pcm_sframes_t) pcm->buffer_size);
		int id = shmget(IPC_PRIVATE, size, 0666);
		if (id < 0) {
			SYSERR("shmget failed");
			return -errno;
		}
		hw->shmid = id;
	}
	return 0;
}

static int snd_pcm_hw_munmap(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	if (!(pcm->info & SND_PCM_INFO_MMAP)) {
		if (shmctl(hw->shmid, IPC_RMID, 0) < 0) {
			SYSERR("shmctl IPC_RMID failed");
			return -errno;
		}
	}
	return 0;
}

static int snd_pcm_hw_close(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private_data;
	if (close(hw->fd)) {
		SYSERR("close failed\n");
		return -errno;
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
	if (!(pcm->info & SND_PCM_INFO_MMAP) && 
	    pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_write_mmap(pcm, size);
	snd_pcm_mmap_appl_forward(pcm, size);
	return size;
}

static snd_pcm_sframes_t snd_pcm_hw_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_uframes_t avail;
	snd_pcm_sframes_t err;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		avail = snd_pcm_mmap_playback_avail(pcm);
	} else {
		avail = snd_pcm_mmap_capture_avail(pcm);
		if (avail > 0 && 
		    !(pcm->info & SND_PCM_INFO_MMAP)) {
			err = snd_pcm_read_mmap(pcm, avail);
			if (err < 0)
				return err;
			assert((snd_pcm_uframes_t)err == avail);
			return err;
		}
	}
	if (avail > pcm->buffer_size)
		return -EPIPE;
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

snd_pcm_ops_t snd_pcm_hw_ops = {
	close: snd_pcm_hw_close,
	info: snd_pcm_hw_info,
	hw_refine: snd_pcm_hw_hw_refine,
	hw_params: snd_pcm_hw_hw_params,
	hw_free: snd_pcm_hw_hw_free,
	sw_params: snd_pcm_hw_sw_params,
	channel_info: snd_pcm_hw_channel_info,
	dump: snd_pcm_hw_dump,
	nonblock: snd_pcm_hw_nonblock,
	async: snd_pcm_hw_async,
	mmap: snd_pcm_hw_mmap,
	munmap: snd_pcm_hw_munmap,
};

snd_pcm_fast_ops_t snd_pcm_hw_fast_ops = {
	status: snd_pcm_hw_status,
	state: snd_pcm_hw_state,
	delay: snd_pcm_hw_delay,
	prepare: snd_pcm_hw_prepare,
	reset: snd_pcm_hw_reset,
	start: snd_pcm_hw_start,
	drop: snd_pcm_hw_drop,
	drain: snd_pcm_hw_drain,
	pause: snd_pcm_hw_pause,
	rewind: snd_pcm_hw_rewind,
	resume: snd_pcm_hw_resume,
	writei: snd_pcm_hw_writei,
	writen: snd_pcm_hw_writen,
	readi: snd_pcm_hw_readi,
	readn: snd_pcm_hw_readn,
	avail_update: snd_pcm_hw_avail_update,
	mmap_commit: snd_pcm_hw_mmap_commit,
};

int snd_pcm_hw_open(snd_pcm_t **pcmp, const char *name, int card, int device, int subdevice, snd_pcm_stream_t stream, int mode)
{
	char filename[32];
	const char *filefmt;
	int ver;
	int err, ret = 0, fd = -1;
	int attempt = 0;
	snd_pcm_info_t info;
	int fmode;
	snd_ctl_t *ctl;
	snd_pcm_t *pcm = NULL;
	snd_pcm_hw_t *hw = NULL;

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
		SYSERR("open %s failed", filename);
		ret = -errno;
		goto _err;
	}
	if (ioctl(fd, SNDRV_PCM_IOCTL_PVERSION, &ver) < 0) {
		SYSERR("SNDRV_PCM_IOCTL_PVERSION failed");
		ret = -errno;
		goto _err;
	}
	if (SNDRV_PROTOCOL_INCOMPATIBLE(ver, SNDRV_PCM_VERSION_MAX)) {
		ret = -SND_ERROR_INCOMPATIBLE_VERSION;
		goto _err;
	}
	if (subdevice >= 0) {
		memset(&info, 0, sizeof(info));
		if (ioctl(fd, SNDRV_PCM_IOCTL_INFO, &info) < 0) {
			SYSERR("SNDRV_PCM_IOCTL_INFO failed");
			ret = -errno;
			goto _err;
		}
		if (info.subdevice != (unsigned int) subdevice) {
			close(fd);
			goto __again;
		}
	}
	hw = calloc(1, sizeof(snd_pcm_hw_t));
	if (!hw) {
		ret = -ENOMEM;
		goto _err;
	}
	hw->card = card;
	hw->device = device;
	hw->subdevice = subdevice;
	hw->fd = fd;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_HW, name, stream, mode);
	if (err < 0) {
		ret = err;
		goto _err;
	}
	snd_ctl_close(ctl);
	pcm->ops = &snd_pcm_hw_ops;
	pcm->fast_ops = &snd_pcm_hw_fast_ops;
	pcm->private_data = hw;
	pcm->poll_fd = fd;
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
	
 _err:
	if (hw)
		free(hw);
	if (pcm)
		free(pcm);
	if (fd >= 0)
		close(fd);
	snd_ctl_close(ctl);
	return ret;
}

int _snd_pcm_hw_open(snd_pcm_t **pcmp, const char *name,
		     snd_config_t *root ATTRIBUTE_UNUSED, snd_config_t *conf,
		     snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	long card = -1, device = 0, subdevice = -1;
	const char *str;
	int err;
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
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (card < 0) {
		SNDERR("card is not defined");
		return -EINVAL;
	}
	return snd_pcm_hw_open(pcmp, name, card, device, subdevice, stream, mode);
}
SND_DLSYM_BUILD_VERSION(_snd_pcm_hw_open, SND_PCM_DLSYM_VERSION);
