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
	snd_pcm_t *handle;
	int fd;
	int card, device, subdevice;
} snd_pcm_hw_t;

#define SND_FILE_PCM_STREAM_PLAYBACK		"/dev/snd/pcmC%iD%ip"
#define SND_FILE_PCM_STREAM_CAPTURE		"/dev/snd/pcmC%iD%ic"
#define SND_PCM_VERSION_MAX	SND_PROTOCOL_VERSION(2, 0, 0)

static int snd_pcm_hw_close(void *private)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	free(private);
	if (fd >= 0)
		if (close(fd))
			return -errno;
	return 0;
}

static int snd_pcm_hw_nonblock(void *private, int nonblock)
{
	long flags;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;

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

static int snd_pcm_hw_info(void *private, snd_pcm_info_t * info)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_params_info(void *private, snd_pcm_params_info_t * info)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_PARAMS_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_params(void *private, snd_pcm_params_t * params)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_PARAMS, params) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_setup(void *private, snd_pcm_setup_t * setup)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_SETUP, setup) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_setup(void *private, snd_pcm_channel_setup_t * setup)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_SETUP, setup) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_status(void *private, snd_pcm_status_t * status)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_STATUS, status) < 0)
		return -errno;
	return 0;
}

static ssize_t snd_pcm_hw_state(void *private)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	snd_pcm_status_t status;
	if (ioctl(fd, SND_PCM_IOCTL_STATUS, status) < 0)
		return -errno;
	return status.state;
}

static ssize_t snd_pcm_hw_frame_io(void *private, int update UNUSED)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	ssize_t pos = ioctl(fd, SND_PCM_IOCTL_FRAME_IO);
	if (pos < 0)
		return -errno;
	return pos;
}

static int snd_pcm_hw_prepare(void *private)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_PREPARE) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_go(void *private)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_GO) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_drain(void *private)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_DRAIN) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_flush(void *private)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_FLUSH) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_pause(void *private, int enable)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_PAUSE, enable) < 0)
		return -errno;
	return 0;
}

static ssize_t snd_pcm_hw_frame_data(void *private, off_t offset)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	snd_pcm_t *handle = hw->handle;
	int fd = hw->fd;
	if (handle->mmap_status && handle->mmap_control)
		return snd_pcm_mmap_frame_data(handle, offset);
	result = ioctl(fd, SND_PCM_IOCTL_FRAME_DATA, offset);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_write(void *private, snd_timestamp_t *tstamp, const void *buffer, size_t size)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	snd_xfer_t xfer;
	if (tstamp)
		xfer.tstamp = *tstamp;
	else
		xfer.tstamp.tv_sec = xfer.tstamp.tv_usec = 0;
	xfer.buf = (char*) buffer;
	xfer.count = size;
	result = ioctl(fd, SND_PCM_IOCTL_WRITE_FRAMES, &xfer);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_writev(void *private, snd_timestamp_t *tstamp, const struct iovec *vector, unsigned long count)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	snd_xferv_t xferv;
	if (tstamp)
		xferv.tstamp = *tstamp;
	else
		xferv.tstamp.tv_sec = xferv.tstamp.tv_usec = 0;
	xferv.vector = vector;
	xferv.count = count;
	result = ioctl(fd, SND_PCM_IOCTL_WRITEV_FRAMES, &xferv);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_read(void *private, snd_timestamp_t *tstamp, void *buffer, size_t size)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	snd_xfer_t xfer;
	if (tstamp)
		xfer.tstamp = *tstamp;
	else
		xfer.tstamp.tv_sec = xfer.tstamp.tv_usec = 0;
	xfer.buf = buffer;
	xfer.count = size;
	result = ioctl(fd, SND_PCM_IOCTL_READ_FRAMES, &xfer);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_pcm_hw_readv(void *private, snd_timestamp_t *tstamp, const struct iovec *vector, unsigned long count)
{
	ssize_t result;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	int fd = hw->fd;
	snd_xferv_t xferv;
	if (tstamp)
		xferv.tstamp = *tstamp;
	else
		xferv.tstamp.tv_sec = xferv.tstamp.tv_usec = 0;
	xferv.vector = vector;
	xferv.count = count;
	result = ioctl(fd, SND_PCM_IOCTL_READV_FRAMES, &xferv);
	if (result < 0)
		return -errno;
	return result;
}

static int snd_pcm_hw_mmap_status(void *private, snd_pcm_mmap_status_t **status)
{
	void *ptr;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_control_t), PROT_READ, MAP_FILE|MAP_SHARED, 
		   hw->fd, SND_PCM_MMAP_OFFSET_STATUS);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	*status = ptr;
	return 0;
}

static int snd_pcm_hw_mmap_control(void *private, snd_pcm_mmap_control_t **control)
{
	void *ptr;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_control_t), PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, 
		   hw->fd, SND_PCM_MMAP_OFFSET_CONTROL);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	*control = ptr;
	return 0;
}

static int snd_pcm_hw_mmap_data(void *private, void **buffer, size_t bsize)
{
	int prot;
	void *daddr;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	prot = hw->handle->stream == SND_PCM_STREAM_PLAYBACK ? PROT_WRITE : PROT_READ;
	daddr = mmap(NULL, bsize, prot, MAP_FILE|MAP_SHARED, 
		     hw->fd, SND_PCM_MMAP_OFFSET_DATA);
	if (daddr == MAP_FAILED || daddr == NULL)
		return -errno;
	*buffer = daddr;
	return 0;
}

static int snd_pcm_hw_munmap_status(void *private UNUSED, snd_pcm_mmap_status_t *status)
{
	if (munmap(status, sizeof(*status)) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_munmap_control(void *private UNUSED, snd_pcm_mmap_control_t *control)
{
	if (munmap(control, sizeof(*control)) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_munmap_data(void *private UNUSED, void *buffer, size_t bsize)
{
	if (munmap(buffer, bsize) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_file_descriptor(void *private)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	return hw->fd;
}

static int snd_pcm_hw_channels_mask(void *private UNUSED,
				    bitset_t *client_vmask UNUSED)
{
	return 0;
}

static void snd_pcm_hw_dump(void *private, FILE *fp)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) private;
	snd_pcm_t *handle = hw->handle;
	char *name = "Unknown";
	snd_card_get_name(hw->card, &name);
	fprintf(fp, "Hardware PCM card %d '%s' device %d subdevice %d\n",
		hw->card, name, hw->device, hw->subdevice);
	free(name);
	if (handle->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(handle, fp);
	}
}

struct snd_pcm_ops snd_pcm_hw_ops = {
	close: snd_pcm_hw_close,
	info: snd_pcm_hw_info,
	params_info: snd_pcm_hw_params_info,
	params: snd_pcm_hw_params,
	setup: snd_pcm_hw_setup,
	dump: snd_pcm_hw_dump,
};

struct snd_pcm_fast_ops snd_pcm_hw_fast_ops = {
	nonblock: snd_pcm_hw_nonblock,
	channel_setup: snd_pcm_hw_channel_setup,
	status: snd_pcm_hw_status,
	frame_io: snd_pcm_hw_frame_io,
	state: snd_pcm_hw_state,
	prepare: snd_pcm_hw_prepare,
	go: snd_pcm_hw_go,
	drain: snd_pcm_hw_drain,
	flush: snd_pcm_hw_flush,
	pause: snd_pcm_hw_pause,
	frame_data: snd_pcm_hw_frame_data,
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

int snd_pcm_hw_open_subdevice(snd_pcm_t **handlep, int card, int device, int subdevice, int stream, int mode)
{
	char filename[32];
	char *filefmt;
	int ver;
	int ret = 0, fd = -1;
	int attempt = 0;
	snd_pcm_info_t info;
	int fmode;
	snd_ctl_t *ctl;
	snd_pcm_t *handle;
	snd_pcm_hw_t *hw;

	*handlep = 0;

	if ((ret = snd_ctl_open(&ctl, card)) < 0)
		return ret;

	switch (stream) {
	case SND_PCM_STREAM_PLAYBACK:
		filefmt = SND_FILE_PCM_STREAM_PLAYBACK;
		break;
	case SND_PCM_STREAM_CAPTURE:
		filefmt = SND_FILE_PCM_STREAM_CAPTURE;
		break;
	default:
		assert(0);
	}
	if ((ret = snd_ctl_pcm_prefer_subdevice(ctl, subdevice)) < 0)
		goto __end;
	sprintf(filename, filefmt, card, device);

      __again:
      	if (attempt++ > 3) {
		ret = -EBUSY;
		goto __end;
      	}
	fmode = O_RDWR;
	if (mode & SND_PCM_NONBLOCK)
		fmode |= O_NONBLOCK;
	if ((fd = open(filename, fmode)) < 0) {
		ret = -errno;
		goto __end;
	}
	if (ioctl(fd, SND_PCM_IOCTL_PVERSION, &ver) < 0) {
		ret = -errno;
		goto __end;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_PCM_VERSION_MAX)) {
		ret = -SND_ERROR_INCOMPATIBLE_VERSION;
		goto __end;
	}
	if (subdevice >= 0) {
		memset(&info, 0, sizeof(info));
		if (ioctl(fd, SND_PCM_IOCTL_INFO, &info) < 0) {
			ret = -errno;
			goto __end;
		}
		if (info.subdevice != subdevice) {
			close(fd);
			goto __again;
		}
	}
	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		ret = -ENOMEM;
		goto __end;
	}
	hw = calloc(1, sizeof(snd_pcm_hw_t));
	if (!handle) {
		free(handle);
		ret = -ENOMEM;
		goto __end;
	}
	hw->handle = handle;
	hw->card = card;
	hw->device = device;
	hw->subdevice = subdevice;
	hw->fd = fd;
	handle->type = SND_PCM_TYPE_HW;
	handle->stream = stream;
	handle->ops = &snd_pcm_hw_ops;
	handle->op_arg = hw;
	handle->fast_ops = &snd_pcm_hw_fast_ops;
	handle->fast_op_arg = hw;
	handle->mode = mode;
	handle->private = hw;
	*handlep = handle;
	
 __end:
	if (ret < 0 && fd >= 0)
		close(fd);
	snd_ctl_close(ctl);
	return ret;
}

int snd_pcm_hw_open(snd_pcm_t **handlep, int card, int device, int stream, int mode)
{
	return snd_pcm_hw_open_subdevice(handlep, card, device, -1, stream, mode);
}

