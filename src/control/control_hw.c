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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>
#include "asoundlib.h"
#include "control_local.h"

#define SND_FILE_CONTROL	"/dev/snd/controlC%i"
#define SND_CTL_VERSION_MAX	SND_PROTOCOL_VERSION(2, 0, 0)

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

static int snd_ctl_hw_poll_descriptor(snd_ctl_t *handle)
{
	snd_ctl_hw_t *hw = handle->private;
	return hw->fd;
}

static int snd_ctl_hw_hw_info(snd_ctl_t *handle, snd_ctl_hw_info_t *info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_HW_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_clist(snd_ctl_t *handle, snd_control_list_t *list)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_CONTROL_LIST, list) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_cinfo(snd_ctl_t *handle, snd_control_info_t *info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_CONTROL_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_cread(snd_ctl_t *handle, snd_control_t *control)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_CONTROL_READ, control) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_cwrite(snd_ctl_t *handle, snd_control_t *control)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_CONTROL_WRITE, control) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_hwdep_info(snd_ctl_t *handle, snd_hwdep_info_t * info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_HWDEP_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_pcm_info(snd_ctl_t *handle, snd_pcm_info_t * info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_PCM_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_pcm_prefer_subdevice(snd_ctl_t *handle, int subdev)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_PCM_PREFER_SUBDEVICE, &subdev) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_rawmidi_info(snd_ctl_t *handle, snd_rawmidi_info_t * info)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_RAWMIDI_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_rawmidi_prefer_subdevice(snd_ctl_t *handle, int subdev)
{
	snd_ctl_hw_t *hw = handle->private;
	if (ioctl(hw->fd, SND_CTL_IOCTL_RAWMIDI_PREFER_SUBDEVICE, &subdev) < 0)
		return -errno;
	return 0;
}

static int snd_ctl_hw_read(snd_ctl_t *handle, snd_ctl_event_t *event)
{
	snd_ctl_hw_t *hw = handle->private;
	return read(hw->fd, event, sizeof(*event));
}

snd_ctl_ops_t snd_ctl_hw_ops = {
	close: snd_ctl_hw_close,
	poll_descriptor: snd_ctl_hw_poll_descriptor,
	hw_info: snd_ctl_hw_hw_info,
	clist: snd_ctl_hw_clist,
	cinfo: snd_ctl_hw_cinfo,
	cread: snd_ctl_hw_cread,
	cwrite: snd_ctl_hw_cwrite,
	hwdep_info: snd_ctl_hw_hwdep_info,
	pcm_info: snd_ctl_hw_pcm_info,
	pcm_prefer_subdevice: snd_ctl_hw_pcm_prefer_subdevice,
	rawmidi_info: snd_ctl_hw_rawmidi_info,
	rawmidi_prefer_subdevice: snd_ctl_hw_rawmidi_prefer_subdevice,
	read: snd_ctl_hw_read,
};

int snd_ctl_hw_open(snd_ctl_t **handle, char *name, int card)
{
	int fd, ver;
	char filename[32];
	snd_ctl_t *ctl;
	snd_ctl_hw_t *hw;

	*handle = NULL;	

	assert(card >= 0 && card < SND_CARDS);
	sprintf(filename, SND_FILE_CONTROL, card);
	if ((fd = open(filename, O_RDWR)) < 0) {
		snd_card_load(card);
		if ((fd = open(filename, O_RDWR)) < 0)
			return -errno;
	}
		
	if (ioctl(fd, SND_CTL_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_CTL_VERSION_MAX)) {
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
	char *str;
	int err;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
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
		return -EINVAL;
	}
	if (card < 0)
		return -EINVAL;
	return snd_ctl_hw_open(handlep, name, card);
}
				
