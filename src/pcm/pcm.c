/*
 *  PCM Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@suse.cz>
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "pcm_local.h"

#define SND_FILE_PCM		"/dev/snd/pcmC%iD%i"
#define SND_PCM_VERSION_MAX	SND_PROTOCOL_VERSION(2, 0, 0)

int snd_pcm_open(snd_pcm_t **handle, int card, int device, int mode)
{
	return snd_pcm_open_subdevice(handle, card, device, -1, mode);
}

#ifndef O_WRITEFLG
#define O_WRITEFLG	0x10000000
#endif
#ifndef O_READFLG
#define O_READFLG	0x20000000
#endif

int snd_pcm_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int mode)
{
	int fd, fmode, ver, err, attempt = 0;
	char filename[32];
	snd_pcm_t *pcm;
	snd_ctl_t *ctl;
	snd_pcm_channel_info_t info;

	*handle = NULL;
	
	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	if ((err = snd_ctl_open(&ctl, card)) < 0)
		return err;
	fmode = O_RDWR;
	if (mode & SND_PCM_OPEN_PLAYBACK)
		fmode |= O_WRITEFLG;
	if (mode & SND_PCM_OPEN_CAPTURE)
		fmode |= O_READFLG;
	if (fmode == O_RDWR) {
		snd_ctl_close(ctl);
		return -EINVAL;
	}
	if (mode & SND_PCM_OPEN_STREAM)
		fmode |= O_NONBLOCK;
      __again:
      	if (attempt++ > 3) {
      		snd_ctl_close(ctl);
      		return -EBUSY;
      	}
	if ((err = snd_ctl_pcm_prefer_subdevice(ctl, device, subdevice)) < 0) {
		snd_ctl_close(ctl);
		return err;
	}
	sprintf(filename, SND_FILE_PCM, card, device);
	if ((fd = open(filename, fmode)) < 0) {
		err = -errno;
		snd_ctl_close(ctl);
		return err;
	}
	if (ioctl(fd, SND_PCM_IOCTL_PVERSION, &ver) < 0) {
		err = -errno;
		close(fd);
		snd_ctl_close(ctl);
		return err;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_PCM_VERSION_MAX)) {
		close(fd);
		snd_ctl_close(ctl);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	if (subdevice >= 0 && (mode & SND_PCM_OPEN_PLAYBACK) != 0) {
		memset(&info, 0, sizeof(info));
		info.channel = SND_PCM_CHANNEL_PLAYBACK;
		if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_INFO, &info) < 0) {
			err = -errno;
			close(fd);
			snd_ctl_close(ctl);
			return err;
		}
		if (info.subdevice != subdevice) {
			close(fd);
			goto __again;
		}
	}
	if (subdevice >= 0 && (mode & SND_PCM_OPEN_CAPTURE) != 0) {
		memset(&info, 0, sizeof(info));
		info.channel = SND_PCM_CHANNEL_CAPTURE;
		if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_INFO, &info) < 0) {
			err = -errno;
			close(fd);
			snd_ctl_close(ctl);
			return err;
		}
		if (info.subdevice != subdevice) {
			close(fd);
			goto __again;
		}
	}
	snd_ctl_close(ctl);
	pcm = (snd_pcm_t *) calloc(1, sizeof(snd_pcm_t));
	if (pcm == NULL) {
		close(fd);
		return -ENOMEM;
	}
	pcm->card = card;
	pcm->device = device;
	pcm->fd = fd;
	pcm->mode = mode;
	*handle = pcm;
	return 0;
}

int snd_pcm_close(snd_pcm_t *pcm)
{
	int res;

	if (!pcm)
		return -EINVAL;
	snd_pcm_munmap(pcm, SND_PCM_CHANNEL_PLAYBACK);
	snd_pcm_munmap(pcm, SND_PCM_CHANNEL_CAPTURE);
	snd_pcm_plugin_clear(pcm, SND_PCM_CHANNEL_PLAYBACK);
	snd_pcm_plugin_clear(pcm, SND_PCM_CHANNEL_CAPTURE);
	res = close(pcm->fd) < 0 ? -errno : 0;
	free(pcm);
	return res;
}

int snd_pcm_file_descriptor(snd_pcm_t *pcm)
{
	if (!pcm)
		return -EINVAL;
	return pcm->fd;
}

int snd_pcm_nonblock_mode(snd_pcm_t *pcm, int nonblock)
{
	long flags;

	if (!pcm)
		return -EINVAL;
	if ((flags = fcntl(pcm->fd, F_GETFL)) < 0)
		return -errno;
	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if (fcntl(pcm->fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	if (!pcm || !info)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_pcm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	if (!pcm || !info)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CHANNEL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_pcm_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t * params)
{
	int err;

	if (!pcm || !params)
		return -EINVAL;
	if (params->channel < 0 || params->channel > 1)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CHANNEL_PARAMS, params) < 0)
		return -errno;
	pcm->setup_is_valid[params->channel] = 0;
	memset(&pcm->setup[params->channel], 0, sizeof(snd_pcm_channel_setup_t));
	pcm->setup[params->channel].channel = params->channel;
	if ((err = snd_pcm_channel_setup(pcm, &pcm->setup[params->channel]))<0)
		return err;
	pcm->setup_is_valid[params->channel] = 1;
	return 0;
}

int snd_pcm_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t * setup)
{
	if (!pcm || !setup)
		return -EINVAL;
	if (setup->channel < 0 || setup->channel > 1)
		return -EINVAL;
	if (pcm->setup_is_valid[setup->channel]) {
		memcpy(setup, &pcm->setup[setup->channel], sizeof(*setup));
	} else {
		if (ioctl(pcm->fd, SND_PCM_IOCTL_CHANNEL_SETUP, setup) < 0)
			return -errno;
		memcpy(&pcm->setup[setup->channel], setup, sizeof(*setup));
		pcm->setup_is_valid[setup->channel] = 1;
	}
	return 0;
}

int snd_pcm_channel_status(snd_pcm_t *pcm, snd_pcm_channel_status_t * status)
{
	if (!pcm || !status)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CHANNEL_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_prepare(snd_pcm_t *pcm)
{
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_PREPARE) < 0)
		return -errno;
	return 0;
}

int snd_pcm_capture_prepare(snd_pcm_t *pcm)
{
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CAPTURE_PREPARE) < 0)
		return -errno;
	return 0;
}

int snd_pcm_channel_prepare(snd_pcm_t *pcm, int channel)
{
	switch (channel) {
	case SND_PCM_CHANNEL_PLAYBACK:
		return snd_pcm_playback_prepare(pcm);
	case SND_PCM_CHANNEL_CAPTURE:
		return snd_pcm_capture_prepare(pcm);
	default:
		return -EIO;
	}
}

int snd_pcm_playback_go(snd_pcm_t *pcm)
{
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_GO) < 0)
		return -errno;
	return 0;
}

int snd_pcm_capture_go(snd_pcm_t *pcm)
{
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CAPTURE_GO) < 0)
		return -errno;
	return 0;
}

int snd_pcm_channel_go(snd_pcm_t *pcm, int channel)
{
	switch (channel) {
	case SND_PCM_CHANNEL_PLAYBACK:
		return snd_pcm_playback_go(pcm);
	case SND_PCM_CHANNEL_CAPTURE:
		return snd_pcm_capture_go(pcm);
	default:
		return -EIO;
	}
}

int snd_pcm_sync_go(snd_pcm_t *pcm, snd_pcm_sync_t *sync)
{
	if (!pcm || !sync)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_SYNC_GO, sync) < 0)
		return -errno;
	return 0;
}

int snd_pcm_drain_playback(snd_pcm_t *pcm)
{
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_DRAIN_PLAYBACK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_flush_playback(snd_pcm_t *pcm)
{
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_FLUSH_PLAYBACK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_flush_capture(snd_pcm_t *pcm)
{
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_FLUSH_CAPTURE) < 0)
		return -errno;
	return 0;
}

int snd_pcm_flush_channel(snd_pcm_t *pcm, int channel)
{
	switch (channel) {
	case SND_PCM_CHANNEL_PLAYBACK:
		return snd_pcm_flush_playback(pcm);
	case SND_PCM_CHANNEL_CAPTURE:
		return snd_pcm_flush_capture(pcm);
	default:
		return -EIO;
	}
}

int snd_pcm_playback_pause(snd_pcm_t *pcm, int enable)
{
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_PAUSE, &enable) < 0)
		return -errno;
	return 0;
}

ssize_t snd_pcm_transfer_size(snd_pcm_t *pcm, int channel)
{
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->setup_is_valid[channel])
		return -EBADFD;
	if (pcm->setup[channel].mode != SND_PCM_MODE_BLOCK)
		return -EBADFD;
	return pcm->setup[channel].buf.block.frag_size;
}

ssize_t snd_pcm_write(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	ssize_t result;

	if (!pcm || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = write(pcm->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_pcm_read(snd_pcm_t *pcm, void *buffer, size_t size)
{
	ssize_t result;

	if (!pcm || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = read(pcm->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

int snd_pcm_mmap(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t **control, void **buffer)
{
	snd_pcm_channel_info_t info;
	int err;
	void *caddr, *daddr;
	off_t offset;

	if (control)
		*control = NULL;
	if (buffer)
		*buffer = NULL;
	if (!pcm || channel < 0 || channel > 1 || !control || !buffer)
		return -EINVAL;
	memset(&info, 0, sizeof(info));
	info.channel = channel;
	if ((err = snd_pcm_channel_info(pcm, &info))<0)
		return err;
	offset = channel == SND_PCM_CHANNEL_PLAYBACK ?
			SND_PCM_MMAP_OFFSET_PLAYBACK_CONTROL :
			SND_PCM_MMAP_OFFSET_CAPTURE_CONTROL;
	caddr = mmap(NULL, sizeof(snd_pcm_mmap_control_t), PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, pcm->fd, offset);
	if (caddr == (caddr_t)-1 || caddr == NULL)
		return -errno;
	offset = channel == SND_PCM_CHANNEL_PLAYBACK ?
			SND_PCM_MMAP_OFFSET_PLAYBACK :
			SND_PCM_MMAP_OFFSET_CAPTURE;
	daddr = mmap(NULL, info.mmap_size, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, pcm->fd, offset);
	if (daddr == (caddr_t)-1 || daddr == NULL) {
		err = -errno;
		munmap(caddr, sizeof(snd_pcm_mmap_control_t));
		return err;
	}
	*control = pcm->mmap_caddr[channel] = caddr;
	*buffer = pcm->mmap_daddr[channel] = daddr;
	pcm->mmap_size[channel] = info.mmap_size;
	return 0;
}

int snd_pcm_munmap(snd_pcm_t *pcm, int channel)
{
	if (!pcm || channel < 0 || channel > 1)
		return -EINVAL;
	if (pcm->mmap_caddr[channel]) {
		munmap(pcm->mmap_caddr[channel], sizeof(snd_pcm_mmap_control_t));
		pcm->mmap_caddr[channel] = NULL;
	}
	if (pcm->mmap_daddr[channel]) {
		munmap(pcm->mmap_daddr[channel], pcm->mmap_size[channel]);
		pcm->mmap_daddr[channel] = NULL;
		pcm->mmap_size[channel] = 0;
	}
	return 0;
}
