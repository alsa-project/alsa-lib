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
	int card, device, subdevice;
	int mmap_emulation;
} snd_pcm_hw_t;

#define SND_FILE_PCM_STREAM_PLAYBACK		"/dev/snd/pcmC%iD%ip"
#define SND_FILE_PCM_STREAM_CAPTURE		"/dev/snd/pcmC%iD%ic"
#define SND_PCM_VERSION_MAX	SND_PROTOCOL_VERSION(2, 0, 0)

static int snd_pcm_hw_close(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	free(hw);
	if (fd >= 0)
		if (close(fd))
			return -errno;
	return 0;
}

static int snd_pcm_hw_nonblock(snd_pcm_t *pcm, int nonblock)
{
	long flags;
	snd_pcm_hw_t *hw = pcm->private;
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

static int snd_pcm_hw_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t * info)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_PARAMS_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_PARAMS, params) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_SETUP, setup) < 0)
		return -errno;
	if (setup->mmap_shape == SND_PCM_MMAP_UNSPECIFIED) {
		if (setup->xfer_mode == SND_PCM_XFER_INTERLEAVED)
			setup->mmap_shape = SND_PCM_MMAP_INTERLEAVED;
		else
			setup->mmap_shape = SND_PCM_MMAP_NONINTERLEAVED;
		hw->mmap_emulation = 1;
	} else
		hw->mmap_emulation = 0;
	return 0;
}

static int snd_pcm_hw_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t * params)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_PARAMS, params) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t * setup)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_SETUP, setup) < 0)
		return -errno;
	if (hw->mmap_emulation) {
		if (pcm->setup.mmap_shape == SND_PCM_MMAP_INTERLEAVED) {
			setup->area.addr = pcm->mmap_data;
			setup->area.first = setup->channel * pcm->bits_per_sample;
			setup->area.step = pcm->bits_per_frame;
		} else {
			setup->area.addr = pcm->mmap_data + setup->channel * pcm->setup.buffer_size * pcm->bits_per_sample / 8;
			setup->area.first = 0;
			setup->area.step = pcm->bits_per_sample;
		}
	} else
		setup->area.addr = (char *)pcm->mmap_data + (long)setup->area.addr;
	return 0;
}

static int snd_pcm_hw_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_STATUS, status) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_state(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	snd_pcm_status_t status;
	if (pcm->mmap_status)
		return pcm->mmap_status->state;
	if (ioctl(fd, SND_PCM_IOCTL_STATUS, &status) < 0)
		return -errno;
	return status.state;
}

static int snd_pcm_hw_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_DELAY, delayp) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_prepare(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_PREPARE) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_start(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_START) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_stop(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_STOP) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_flush(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_FLUSH) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (ioctl(fd, SND_PCM_IOCTL_PAUSE, enable) < 0)
		return -errno;
	return 0;
}

static ssize_t snd_pcm_hw_appl_ptr(snd_pcm_t *pcm, off_t offset)
{
	ssize_t result;
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	if (pcm->mmap_status && pcm->mmap_control)
		return snd_pcm_mmap_appl_ptr(pcm, offset);
	result = ioctl(fd, SND_PCM_IOCTL_APPL_PTR, offset);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	ssize_t result;
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	snd_xferi_t xferi;
	xferi.buf = (char*) buffer;
	xferi.frames = size;
	result = ioctl(fd, SND_PCM_IOCTL_WRITEI_FRAMES, &xferi);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	ssize_t result;
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	snd_xfern_t xfern;
	xfern.bufs = bufs;
	xfern.frames = size;
	result = ioctl(fd, SND_PCM_IOCTL_WRITEN_FRAMES, &xfern);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	ssize_t result;
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	snd_xferi_t xferi;
	xferi.buf = buffer;
	xferi.frames = size;
	result = ioctl(fd, SND_PCM_IOCTL_READI_FRAMES, &xferi);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_pcm_hw_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	ssize_t result;
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	snd_xfern_t xfern;
	xfern.bufs = bufs;
	xfern.frames = size;
	result = ioctl(fd, SND_PCM_IOCTL_READN_FRAMES, &xfern);
	if (result < 0)
		return -errno;
	return result;
}

static int snd_pcm_hw_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	void *ptr;
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_status_t), PROT_READ, MAP_FILE|MAP_SHARED, 
		   hw->fd, SND_PCM_MMAP_OFFSET_STATUS);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	pcm->mmap_status = ptr;
	return 0;
}

static int snd_pcm_hw_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	void *ptr;
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_control_t), PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, 
		   hw->fd, SND_PCM_MMAP_OFFSET_CONTROL);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	pcm->mmap_control = ptr;
	return 0;
}

static int snd_pcm_hw_mmap_data(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	void *ptr;
	if (hw->mmap_emulation) {
		ptr = malloc(snd_pcm_frames_to_bytes(pcm, pcm->setup.buffer_size));
		if (!ptr)
			return -ENOMEM;
	} else {
		int prot;
		prot = PROT_WRITE | PROT_READ;
		ptr = mmap(NULL, pcm->setup.mmap_bytes,
			   prot, MAP_FILE|MAP_SHARED, 
			   hw->fd, SND_PCM_MMAP_OFFSET_DATA);
		if (ptr == MAP_FAILED || ptr == NULL)
			return -errno;
	}
	pcm->mmap_data = ptr;
	return 0;
}

static int snd_pcm_hw_munmap_status(snd_pcm_t *pcm)
{
	if (munmap(pcm->mmap_status, sizeof(*pcm->mmap_status)) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_munmap_control(snd_pcm_t *pcm)
{
	if (munmap(pcm->mmap_control, sizeof(*pcm->mmap_control)) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_munmap_data(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	if (hw->mmap_emulation)
		free(pcm->mmap_data);
	else
		if (munmap(pcm->mmap_data, pcm->setup.mmap_bytes) < 0)
			return -errno;
	return 0;
}

static ssize_t snd_pcm_hw_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_hw_t *hw = pcm->private;
	if (hw->mmap_emulation && pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_write_mmap(pcm, size);
	snd_pcm_mmap_appl_forward(pcm, size);
	return size;
}

static ssize_t snd_pcm_hw_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	int fd = hw->fd;
	size_t avail;
	ssize_t err;
	if (pcm->setup.ready_mode == SND_PCM_READY_ASAP) {
		ssize_t d;
		int err = ioctl(fd, SND_PCM_IOCTL_DELAY, &d);
		if (err < 0)
			return -errno;
	}
	avail = snd_pcm_mmap_avail(pcm);
	if (avail > 0 && hw->mmap_emulation && 
	    pcm->stream == SND_PCM_STREAM_CAPTURE) {
		err = snd_pcm_read_mmap(pcm, avail);
		if (err < 0)
			return err;
		assert((size_t)err == avail);
		return err;
	}
	return avail;
}

static int snd_pcm_hw_poll_descriptor(snd_pcm_t *pcm)
{
	snd_pcm_hw_t *hw = pcm->private;
	return hw->fd;
}

static int snd_pcm_hw_channels_mask(snd_pcm_t *pcm ATTRIBUTE_UNUSED,
				    bitset_t *cmask ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_hw_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_hw_t *hw = pcm->private;
	char *name = "Unknown";
	snd_card_get_name(hw->card, &name);
	fprintf(fp, "Hardware PCM card %d '%s' device %d subdevice %d\n",
		hw->card, name, hw->device, hw->subdevice);
	free(name);
	if (pcm->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
}

struct snd_pcm_ops snd_pcm_hw_ops = {
	close: snd_pcm_hw_close,
	info: snd_pcm_hw_info,
	params_info: snd_pcm_hw_params_info,
	params: snd_pcm_hw_params,
	setup: snd_pcm_hw_setup,
	channel_info: snd_pcm_hw_channel_info,
	channel_params: snd_pcm_hw_channel_params,
	channel_setup: snd_pcm_hw_channel_setup,
	dump: snd_pcm_hw_dump,
	nonblock: snd_pcm_hw_nonblock,
	mmap_status: snd_pcm_hw_mmap_status,
	mmap_control: snd_pcm_hw_mmap_control,
	mmap_data: snd_pcm_hw_mmap_data,
	munmap_status: snd_pcm_hw_munmap_status,
	munmap_control: snd_pcm_hw_munmap_control,
	munmap_data: snd_pcm_hw_munmap_data,
};

struct snd_pcm_fast_ops snd_pcm_hw_fast_ops = {
	status: snd_pcm_hw_status,
	state: snd_pcm_hw_state,
	delay: snd_pcm_hw_delay,
	prepare: snd_pcm_hw_prepare,
	start: snd_pcm_hw_start,
	stop: snd_pcm_hw_stop,
	flush: snd_pcm_hw_flush,
	pause: snd_pcm_hw_pause,
	appl_ptr: snd_pcm_hw_appl_ptr,
	writei: snd_pcm_hw_writei,
	writen: snd_pcm_hw_writen,
	readi: snd_pcm_hw_readi,
	readn: snd_pcm_hw_readn,
	poll_descriptor: snd_pcm_hw_poll_descriptor,
	channels_mask: snd_pcm_hw_channels_mask,
	avail_update: snd_pcm_hw_avail_update,
	mmap_forward: snd_pcm_hw_mmap_forward,
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

	assert(handlep);

	if ((ret = snd_ctl_hw_open(&ctl, card)) < 0)
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
	hw = calloc(1, sizeof(snd_pcm_hw_t));
	if (!hw) {
		ret = -ENOMEM;
		goto __end;
	}
	hw->card = card;
	hw->device = device;
	hw->subdevice = subdevice;
	hw->fd = fd;

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		free(hw);
		ret = -ENOMEM;
		goto __end;
	}
	handle->type = SND_PCM_TYPE_HW;
	handle->stream = stream;
	handle->ops = &snd_pcm_hw_ops;
	handle->op_arg = handle;
	handle->fast_ops = &snd_pcm_hw_fast_ops;
	handle->fast_op_arg = handle;
	handle->mode = mode;
	handle->private = hw;
	ret = snd_pcm_init(handle);
	if (ret < 0) {
		snd_pcm_close(handle);
		snd_ctl_close(ctl);
		return ret;
	}
	*handlep = handle;
	
 __end:
	if (ret < 0 && fd >= 0)
		close(fd);
	snd_ctl_close(ctl);
	return ret;
}

int snd_pcm_hw_open_device(snd_pcm_t **handlep, int card, int device, int stream, int mode)
{
	return snd_pcm_hw_open_subdevice(handlep, card, device, -1, stream, mode);
}

int _snd_pcm_hw_open(snd_pcm_t **pcmp, char *name, snd_config_t *conf,
		     int stream, int mode)
{
	snd_config_iterator_t i;
	long card = -1, device = 0, subdevice = -1;
	char *str;
	int err;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "card") == 0) {
			err = snd_config_integer_get(n, &card);
			if (err < 0) {
				err = snd_config_string_get(n, &str);
				if (err < 0)
					return -EINVAL;
				card = snd_card_get_index(str);
				if (card < 0)
					return card;
			}
			continue;
		}
		if (strcmp(n->id, "device") == 0) {
			err = snd_config_integer_get(n, &device);
			if (err < 0)
				return err;
			continue;
		}
		if (strcmp(n->id, "subdevice") == 0) {
			err = snd_config_integer_get(n, &subdevice);
			if (err < 0)
				return err;
			continue;
		}
		return -EINVAL;
	}
	if (card < 0)
		return -EINVAL;
	return snd_pcm_hw_open_subdevice(pcmp, card, device, subdevice, stream, mode);
}
				
