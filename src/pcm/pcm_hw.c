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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "pcm_local.h"

typedef struct {
	int fd;
} snd_pcm_hw_stream_t;

typedef struct snd_pcm_hw {
	int card;
	int device;
	int ver;
	snd_pcm_hw_stream_t stream[2];
} snd_pcm_hw_t;

#define SND_FILE_PCM_PLAYBACK		"/dev/snd/pcmC%iD%ip"
#define SND_FILE_PCM_CAPTURE		"/dev/snd/pcmC%iD%ic"
#define SND_PCM_VERSION_MAX	SND_PROTOCOL_VERSION(2, 0, 0)

static int snd_pcm_hw_stream_close(snd_pcm_t *pcm, int stream)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (fd >= 0)
		if (close(fd))
			return -errno;
	return 0;
}

static int snd_pcm_hw_stream_nonblock(snd_pcm_t *pcm, int stream, int nonblock)
{
	long flags;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;

	if ((flags = fcntl(fd, F_GETFL)) < 0)
		return -errno;
	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_info(snd_pcm_t *pcm, int stream, snd_pcm_info_t * info)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stream_info(snd_pcm_t *pcm, snd_pcm_stream_info_t * info)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[info->stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stream_params(snd_pcm_t *pcm, snd_pcm_stream_params_t * params)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[params->stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_PARAMS, params) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stream_setup(snd_pcm_t *pcm, snd_pcm_stream_setup_t * setup)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[setup->stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_SETUP, setup) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_setup(snd_pcm_t *pcm, int stream, snd_pcm_channel_setup_t * setup)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_SETUP, setup) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stream_status(snd_pcm_t *pcm, snd_pcm_stream_status_t * status)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[status->stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_STATUS, status) < 0)
		return -errno;
	return 0;
}

static ssize_t snd_pcm_hw_stream_state(snd_pcm_t *pcm, int stream)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	snd_pcm_stream_status_t status;
	status.stream = stream;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_STATUS, status) < 0)
		return -errno;
	return status.state;
}

static ssize_t snd_pcm_hw_stream_frame_io(snd_pcm_t *pcm, int stream, int update UNUSED)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	ssize_t pos = ioctl(fd, SND_PCM_IOCTL_STREAM_FRAME_IO);
	if (pos < 0)
		return -errno;
	return pos;
}

static int snd_pcm_hw_stream_prepare(snd_pcm_t *pcm, int stream)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_PREPARE) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stream_go(snd_pcm_t *pcm, int stream)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_GO) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_sync_go(snd_pcm_t *pcm, int stream, snd_pcm_sync_t *sync)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_SYNC_GO, sync) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stream_drain(snd_pcm_t *pcm, int stream)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_DRAIN) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stream_flush(snd_pcm_t *pcm, int stream)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_FLUSH) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stream_pause(snd_pcm_t *pcm, int stream, int enable)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	if (ioctl(fd, SND_PCM_IOCTL_STREAM_PAUSE, &enable) < 0)
		return -errno;
	return 0;
}

static ssize_t snd_pcm_hw_stream_frame_data(snd_pcm_t *pcm, int stream, off_t offset)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[stream].fd;
	result = ioctl(fd, SND_PCM_IOCTL_STREAM_FRAME_DATA, offset);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_write(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[SND_PCM_STREAM_PLAYBACK].fd;
	snd_xfer_t xfer;
	xfer.buf = (char*) buffer;
	xfer.count = size;
	result = ioctl(fd, SND_PCM_IOCTL_WRITE_FRAMES, &xfer);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[SND_PCM_STREAM_PLAYBACK].fd;
	snd_xferv_t xferv;
	xferv.vector = vector;
	xferv.count = count;
	result = ioctl(fd, SND_PCM_IOCTL_WRITEV_FRAMES, &xferv);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_read(snd_pcm_t *pcm, void *buffer, size_t size)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[SND_PCM_STREAM_CAPTURE].fd;
	snd_xfer_t xfer;
	xfer.buf = buffer;
	xfer.count = size;
	result = ioctl(fd, SND_PCM_IOCTL_READ_FRAMES, &xfer);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_pcm_hw_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	int fd = hw->stream[SND_PCM_STREAM_CAPTURE].fd;
	snd_xferv_t xferv;
	xferv.vector = vector;
	xferv.count = count;
	result = ioctl(fd, SND_PCM_IOCTL_READV_FRAMES, &xferv);
	if (result < 0)
		return -errno;
	return result;
}

static int snd_pcm_hw_mmap_status(snd_pcm_t *pcm, int stream, snd_pcm_mmap_status_t **status)
{
	void *ptr;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_control_t), PROT_READ, MAP_FILE|MAP_SHARED, 
		   hw->stream[stream].fd, SND_PCM_MMAP_OFFSET_STATUS);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	*status = ptr;
	return 0;
}

static int snd_pcm_hw_mmap_control(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t **control)
{
	void *ptr;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_control_t), PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, 
		   hw->stream[stream].fd, SND_PCM_MMAP_OFFSET_CONTROL);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	*control = ptr;
	return 0;
}

static int snd_pcm_hw_mmap_data(snd_pcm_t *pcm, int stream, void **buffer, size_t bsize)
{
	int prot;
	void *daddr;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	prot = stream == SND_PCM_STREAM_PLAYBACK ? PROT_WRITE : PROT_READ;
	daddr = mmap(NULL, bsize, prot, MAP_FILE|MAP_SHARED, 
		     hw->stream[stream].fd, SND_PCM_MMAP_OFFSET_DATA);
	if (daddr == MAP_FAILED || daddr == NULL)
		return -errno;
	*buffer = daddr;
	return 0;
}

static int snd_pcm_hw_munmap_status(snd_pcm_t *pcm UNUSED, int stream UNUSED, snd_pcm_mmap_status_t *status)
{
	if (munmap(status, sizeof(*status)) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_munmap_control(snd_pcm_t *pcm UNUSED, int stream UNUSED, snd_pcm_mmap_control_t *control)
{
	if (munmap(control, sizeof(*control)) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_munmap_data(snd_pcm_t *pcm UNUSED, int stream UNUSED, void *buffer, size_t bsize)
{
	if (munmap(buffer, bsize) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_file_descriptor(snd_pcm_t *pcm, int stream)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	return hw->stream[stream].fd;
}

static int snd_pcm_hw_channels_mask(snd_pcm_t *pcm UNUSED, int stream UNUSED,
				  bitset_t *client_vmask UNUSED)
{
	return 0;
}

struct snd_pcm_ops snd_pcm_hw_ops = {
	stream_close: snd_pcm_hw_stream_close,
	stream_nonblock: snd_pcm_hw_stream_nonblock,
	info: snd_pcm_hw_info,
	stream_info: snd_pcm_hw_stream_info,
	stream_params: snd_pcm_hw_stream_params,
	stream_setup: snd_pcm_hw_stream_setup,
	channel_setup: snd_pcm_hw_channel_setup,
	stream_status: snd_pcm_hw_stream_status,
	stream_frame_io: snd_pcm_hw_stream_frame_io,
	stream_state: snd_pcm_hw_stream_state,
	stream_prepare: snd_pcm_hw_stream_prepare,
	stream_go: snd_pcm_hw_stream_go,
	sync_go: snd_pcm_hw_sync_go,
	stream_drain: snd_pcm_hw_stream_drain,
	stream_flush: snd_pcm_hw_stream_flush,
	stream_pause: snd_pcm_hw_stream_pause,
	stream_frame_data: snd_pcm_hw_stream_frame_data,
	write: snd_pcm_hw_write,
	writev: snd_pcm_hw_writev,
	read: snd_pcm_hw_read,
	readv: snd_pcm_hw_readv,
	mmap_status: snd_pcm_hw_mmap_status,
	mmap_control: snd_pcm_hw_mmap_control,
	mmap_data: snd_pcm_hw_mmap_data,
	munmap_status: snd_pcm_hw_munmap_status,
	munmap_control: snd_pcm_hw_munmap_control,
	munmap_data: snd_pcm_hw_munmap_data,
	file_descriptor: snd_pcm_hw_file_descriptor,
	channels_mask: snd_pcm_hw_channels_mask,
};

static int snd_pcm_hw_open_stream(int card, int device, int stream, int subdevice, int fmode, snd_ctl_t *ctl, int *ver)
{
	char filename[32];
	char *filefmt;
	int err, fd;
	int attempt = 0;
	snd_pcm_stream_info_t info;
	switch (stream) {
	case SND_PCM_STREAM_PLAYBACK:
		filefmt = SND_FILE_PCM_PLAYBACK;
		break;
	case SND_PCM_STREAM_CAPTURE:
		filefmt = SND_FILE_PCM_CAPTURE;
		break;
	default:
		assert(0);
	}
	if ((err = snd_ctl_pcm_stream_prefer_subdevice(ctl, device, stream, subdevice)) < 0)
		return err;
	sprintf(filename, filefmt, card, device);

      __again:
      	if (attempt++ > 3) {
      		snd_ctl_close(ctl);
      		return -EBUSY;
      	}
	if ((fd = open(filename, fmode)) < 0) {
		err = -errno;
		return err;
	}
	if (ioctl(fd, SND_PCM_IOCTL_PVERSION, ver) < 0) {
		err = -errno;
		close(fd);
		return err;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(*ver, SND_PCM_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	if (subdevice >= 0) {
		memset(&info, 0, sizeof(info));
		if (ioctl(fd, SND_PCM_IOCTL_STREAM_INFO, &info) < 0) {
			err = -errno;
			close(fd);
			return err;
		}
		if (info.subdevice != subdevice) {
			close(fd);
			goto __again;
		}
	}
	return fd;
}

int snd_pcm_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int mode)
{
	int fmode, ver, err;
	snd_pcm_t *pcm;
	snd_pcm_hw_t *hw;
	snd_ctl_t *ctl;
	int pfd = -1, cfd = -1;

	assert(handle);
	*handle = NULL;
	
	assert(card >= 0 && card < SND_CARDS);
	if ((err = snd_ctl_open(&ctl, card)) < 0)
		return err;
	if (mode & SND_PCM_OPEN_PLAYBACK) {
		fmode = O_RDWR;
		if (mode & SND_PCM_NONBLOCK_PLAYBACK)
			fmode |= O_NONBLOCK;
		pfd = snd_pcm_hw_open_stream(card, device, SND_PCM_STREAM_PLAYBACK,
					  subdevice, fmode, ctl, &ver);
		if (pfd < 0) {
			snd_ctl_close(ctl);
			return pfd;
		}
	}
	if (mode & SND_PCM_OPEN_CAPTURE) {
		fmode = O_RDWR;
		if (mode & SND_PCM_NONBLOCK_CAPTURE)
			fmode |= O_NONBLOCK;
		cfd = snd_pcm_hw_open_stream(card, device, SND_PCM_STREAM_CAPTURE,
					  subdevice, fmode, ctl, &ver);
		if (cfd < 0) {
			if (pfd >= 0)
				close(pfd);
			snd_ctl_close(ctl);
			return cfd;
		}
	}
	snd_ctl_close(ctl);
	assert(pfd >= 0 || cfd >= 0);

	err = snd_pcm_abstract_open(handle, mode, SND_PCM_TYPE_HW, sizeof(snd_pcm_hw_t));
	if (err < 0) {
		if (pfd >= 0)
			close(pfd);
		if (cfd >= 0)
			close(cfd);
		return err;
	}
	pcm = *handle;
	pcm->ops = &snd_pcm_hw_ops;
	hw = (snd_pcm_hw_t*) &pcm->private;
	hw->card = card;
	hw->device = device;
	hw->ver = ver;
	hw->stream[SND_PCM_STREAM_PLAYBACK].fd = pfd;
	hw->stream[SND_PCM_STREAM_CAPTURE].fd = cfd;
	return 0;
}

int snd_pcm_open(snd_pcm_t **handle, int card, int device, int mode)
{
	return snd_pcm_open_subdevice(handle, card, device, -1, mode);
}

