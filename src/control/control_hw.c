/*
 *  Control Interface - Hardware
 *  Copyright (c) 1998,1999,2000 by Jaroslav Kysela <perex@suse.cz>
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
#include "control_local.h"

#ifndef F_SETSIG
#define F_SETSIG 10
#endif

#define SNDRV_FILE_CONTROL	"/dev/snd/controlC%i"
#define SNDRV_CTL_VERSION_MAX	SNDRV_PROTOCOL_VERSION(2, 0, 0)

typedef struct {
	int card;
	int fd;
} snd_ctl_hw_t;

static int snd_ctl_hw_close(snd_ctl_t *handle)
{
	snd_ctl_hw_t *hw = handle->private;
	int res;
	res = close(hw->fd) < 0 ? -errno : 0;
	free(hw);
	return res;
}

static int snd_ctl_hw_nonblock(snd_ctl_t *handle, int nonblock)
{
	snd_ctl_hw_t *hw = handle->private;
	long flags;
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

static int snd_ctl_hw_async(snd_ctl_t *ctl, int sig, pid_t pid)
{
	long flags;
	snd_ctl_hw_t *hw = ctl->private;
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
	if (sig == 0)
		sig = SIGIO;
	if (fcntl(fd, F_SETSIG, sig) < 0) {
		SYSERR("F_SETSIG failed");
		return -errno;
	}
	if (pid == 0)
		pid = getpid();
	if (fcntl(fd, F_SETOWN, pid) < 0) {
		SYSERR("F_SETOWN failed");
		return -errno;
	}
	return 0;
}

static int snd_ctl_hw_poll_descriptor(snd_ctl_t *handle)
{
	snd_ctl_hw_t *hw = handle->private;
	return hw->fd;
}

static int snd_ctl_hw_hw_info(snd_ctl_t *handle, snd_ctl_card_info_t *info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_elem_list(snd_ctl_t *handle, snd_ctl_elem_list_t *list)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_ELEM_LIST, list) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_elem_info(snd_ctl_t *handle, snd_ctl_elem_info_t *info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_ELEM_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_elem_read(snd_ctl_t *handle, snd_ctl_elem_t *control)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_ELEM_READ, control) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_elem_write(snd_ctl_t *handle, snd_ctl_elem_t *control)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_ELEM_WRITE, control) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_hwdep_next_device(snd_ctl_t *handle, int * device)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_HWDEP_NEXT_DEVICE, device) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_hwdep_info(snd_ctl_t *handle, snd_hwdep_info_t * info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_HWDEP_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_pcm_next_device(snd_ctl_t *handle, int * device)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_PCM_NEXT_DEVICE, device) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_pcm_info(snd_ctl_t *handle, snd_pcm_info_t * info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_PCM_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_pcm_prefer_subdevice(snd_ctl_t *handle, int subdev)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_PCM_PREFER_SUBDEVICE, &subdev) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_rawmidi_next_device(snd_ctl_t *handle, int * device)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_RAWMIDI_NEXT_DEVICE, device) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_rawmidi_info(snd_ctl_t *handle, snd_rawmidi_info_t * info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_RAWMIDI_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_rawmidi_prefer_subdevice(snd_ctl_t *handle, int subdev)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SNDRV_CTL_IOCTL_RAWMIDI_PREFER_SUBDEVICE, &subdev) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_read(snd_ctl_t *handle, snd_ctl_event_t *event)
{
	snd_ctl_hw_t *hw = handle->private;
	ssize_t res = read(hw->fd, event, sizeof(*event));
	if (res <= 0)
		return res;
	assert(res == sizeof(*event));
	return 1;
}

snd_ctl_ops_t snd_ctl_hw_ops = {
	close: snd_ctl_hw_close,
	nonblock: snd_ctl_hw_nonblock,
	async: snd_ctl_hw_async,
	poll_descriptor: snd_ctl_hw_poll_descriptor,
	hw_info: snd_ctl_hw_hw_info,
	element_list: snd_ctl_hw_elem_list,
	element_info: snd_ctl_hw_elem_info,
	element_read: snd_ctl_hw_elem_read,
	element_write: snd_ctl_hw_elem_write,
	hwdep_next_device: snd_ctl_hw_hwdep_next_device,
	hwdep_info: snd_ctl_hw_hwdep_info,
	pcm_next_device: snd_ctl_hw_pcm_next_device,
	pcm_info: snd_ctl_hw_pcm_info,
	pcm_prefer_subdevice: snd_ctl_hw_pcm_prefer_subdevice,
	rawmidi_next_device: snd_ctl_hw_rawmidi_next_device,
	rawmidi_info: snd_ctl_hw_rawmidi_info,
	rawmidi_prefer_subdevice: snd_ctl_hw_rawmidi_prefer_subdevice,
	read: snd_ctl_hw_read,
};

int snd_ctl_hw_open(snd_ctl_t **handle, const char *name, int card)
{
	int fd, ver;
	char filename[32];
	snd_ctl_t *ctl;
	snd_ctl_hw_t *hw;

	*handle = NULL;	

	assert(card >= 0 && card < 32);
	sprintf(filename, SNDRV_FILE_CONTROL, card);
	if ((fd = open(filename, O_RDWR)) < 0) {
		snd_card_load(card);
		if ((fd = open(filename, O_RDWR)) < 0)
			return -errno;
	}
		
	if (ioctl(fd, SNDRV_CTL_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SNDRV_PROTOCOL_INCOMPATIBLE(ver, SNDRV_CTL_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	ctl = calloc(1, sizeof(snd_ctl_t));
	if (ctl == NULL) {
		close(fd);
		return -ENOMEM;
	}
	hw = calloc(1, sizeof(snd_ctl_hw_t));
	if (hw == NULL) {
		close(fd);
		free(ctl);
		return -ENOMEM;
	}
	hw->card = card;
	hw->fd = fd;
	if (name)
		ctl->name = strdup(name);
	ctl->type = SND_CTL_TYPE_HW;
	ctl->ops = &snd_ctl_hw_ops;
	ctl->private = hw;
	INIT_LIST_HEAD(&ctl->hlist);
	*handle = ctl;
	return 0;
}

int _snd_ctl_hw_open(snd_ctl_t **handlep, char *name, snd_config_t *conf)
{
	snd_config_iterator_t i;
	long card = -1;
	const char *str;
	int err;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "type") == 0)
			continue;
		if (strcmp(id, "card") == 0) {
			err = snd_config_get_integer(n, &card);
			if (err < 0) {
				err = snd_config_get_string(n, &str);
				if (err < 0)
					return -EINVAL;
				card = snd_card_get_index(str);
				if (card < 0)
					return card;
			}
			continue;
		}
		return -EINVAL;
	}
	if (card < 0)
		return -EINVAL;
	return snd_ctl_hw_open(handlep, name, card);
}
				
