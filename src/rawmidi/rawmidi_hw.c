/*
 *  RawMIDI - Hardware
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
 *                        Abramo Bagnara <abramo@alsa-project.org>
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
#include "../control/control_local.h"
#include "rawmidi_local.h"
#include "asoundlib.h"

#define SNDRV_FILE_RAWMIDI	"/dev/snd/midiC%iD%i"
#define SNDRV_RAWMIDI_VERSION_MAX	SNDRV_PROTOCOL_VERSION(2, 0, 0)

typedef struct {
	int fd;
	int card, device, subdevice;
} snd_rawmidi_hw_t;

static int snd_rawmidi_hw_close(snd_rawmidi_t *rmidi)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	if (close(hw->fd)) {
		SYSERR("close failed\n");
		return -errno;
	}
	free(hw);
	return 0;
}

static int snd_rawmidi_hw_card(snd_rawmidi_t *rmidi)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	return hw->card;
}

static int snd_rawmidi_hw_nonblock(snd_rawmidi_t *rmidi, int nonblock)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	long flags;

	if ((flags = fcntl(hw->fd, F_GETFL)) < 0) {
		SYSERR("F_GETFL failed");
		return -errno;
	}
	if (nonblock)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	if (fcntl(hw->fd, F_SETFL, flags) < 0) {
		SYSERR("F_SETFL for O_NONBLOCK failed");
		return -errno;
	}
	return 0;
}

static int snd_rawmidi_hw_info(snd_rawmidi_t *rmidi, snd_rawmidi_info_t * info)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_INFO, info) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_INFO failed");
		return -errno;
	}
	return 0;
}

static int snd_rawmidi_hw_params(snd_rawmidi_t *rmidi, snd_rawmidi_params_t * params)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_PARAMS, params) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_PARAMS failed");
		return -errno;
	}
	return 0;
}

static int snd_rawmidi_hw_status(snd_rawmidi_t *rmidi, snd_rawmidi_status_t * status)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_STATUS, status) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_STATUS failed");
		return -errno;
	}
	return 0;
}

static int snd_rawmidi_hw_drop(snd_rawmidi_t *rmidi, int stream)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_DROP, &stream) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_DROP failed");
		return -errno;
	}
	return 0;
}

static int snd_rawmidi_hw_drain(snd_rawmidi_t *rmidi, int stream)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_DRAIN, &stream) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_DRAIN failed");
		return -errno;
	}
	return 0;
}

static ssize_t snd_rawmidi_hw_write(snd_rawmidi_t *rmidi, const void *buffer, size_t size)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	ssize_t result;
	result = write(hw->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_rawmidi_hw_read(snd_rawmidi_t *rmidi, void *buffer, size_t size)
{
	snd_rawmidi_hw_t *hw = rmidi->private;
	ssize_t result;
	result = read(hw->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

snd_rawmidi_ops_t snd_rawmidi_hw_ops = {
	close: snd_rawmidi_hw_close,
	card: snd_rawmidi_hw_card,
	nonblock: snd_rawmidi_hw_nonblock,
	info: snd_rawmidi_hw_info,
	params: snd_rawmidi_hw_params,
	status: snd_rawmidi_hw_status,
	drop: snd_rawmidi_hw_drop,
	drain: snd_rawmidi_hw_drain,
	write: snd_rawmidi_hw_write,
	read: snd_rawmidi_hw_read,
};


int snd_rawmidi_hw_open(snd_rawmidi_t **handlep, char *name, int card, int device, int subdevice, int streams, int mode)
{
	int fd, ver, ret;
	int attempt = 0;
	char filename[32];
	snd_ctl_t *ctl;
	snd_rawmidi_t *rmidi;
	snd_rawmidi_hw_t *hw;
	snd_rawmidi_info_t info;
	int fmode;

	*handlep = NULL;
	
	assert(card >= 0 && card < 32);

	if ((ret = snd_ctl_hw_open(&ctl, NULL, card)) < 0)
		return ret;
	sprintf(filename, SNDRV_FILE_RAWMIDI, card, device);

      __again:
      	if (attempt++ > 3) {
      		snd_ctl_close(ctl);
      		return -EBUSY;
      	}
      	ret = snd_ctl_rawmidi_prefer_subdevice(ctl, subdevice);
	if (ret < 0) {
		snd_ctl_close(ctl);
		return ret;
	}

	switch (streams) {
	case SND_RAWMIDI_OPEN_OUTPUT:
		fmode = O_WRONLY;
		break;
	case SND_RAWMIDI_OPEN_INPUT:
		fmode = O_RDONLY;
		break;
	case SND_RAWMIDI_OPEN_DUPLEX:
		fmode = O_RDWR;
		break;
	default:
		assert(0);
		return -EINVAL;
	}

	if (mode & SND_RAWMIDI_APPEND) {
		assert(streams & SND_RAWMIDI_OPEN_OUTPUT);
		fmode |= O_APPEND;
	}

	if (mode & SND_RAWMIDI_NONBLOCK) {
		fmode |= O_NONBLOCK;
	}

	assert(!(mode & ~(SND_RAWMIDI_APPEND|SND_RAWMIDI_NONBLOCK)));

	if ((fd = open(filename, fmode)) < 0) {
		snd_card_load(card);
		if ((fd = open(filename, fmode)) < 0) {
			snd_ctl_close(ctl);
			SYSERR("open %s failed", filename);
			return -errno;
		}
	}
	if (ioctl(fd, SNDRV_RAWMIDI_IOCTL_PVERSION, &ver) < 0) {
		ret = -errno;
		SYSERR("SNDRV_RAWMIDI_IOCTL_PVERSION failed");
		close(fd);
		snd_ctl_close(ctl);
		return ret;
	}
	if (SNDRV_PROTOCOL_INCOMPATIBLE(ver, SNDRV_RAWMIDI_VERSION_MAX)) {
		close(fd);
		snd_ctl_close(ctl);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	if (subdevice >= 0) {
		memset(&info, 0, sizeof(info));
		if (ioctl(fd, SNDRV_RAWMIDI_IOCTL_INFO, &info) < 0) {
			SYSERR("SNDRV_RAWMIDI_IOCTL_INFO failed");
			ret = -errno;
			close(fd);
			snd_ctl_close(ctl);
			return ret;
		}
		if (info.subdevice != (unsigned int) subdevice) {
			close(fd);
			goto __again;
		}
	}
	hw = calloc(1, sizeof(snd_rawmidi_hw_t));
	if (hw == NULL) {
		close(fd);
		snd_ctl_close(ctl);
		return -ENOMEM;
	}
	rmidi = calloc(1, sizeof(snd_rawmidi_t));
	if (rmidi == NULL) {
		free(hw);
		close(fd);
		snd_ctl_close(ctl);
		return -ENOMEM;
	}
	hw->card = card;
	hw->device = device;
	hw->subdevice = subdevice;
	hw->fd = fd;
	if (name)
		rmidi->name = strdup(name);
	rmidi->type = SND_RAWMIDI_TYPE_HW;
	rmidi->streams = streams;
	rmidi->mode = mode;
	rmidi->poll_fd = fd;
	rmidi->ops = &snd_rawmidi_hw_ops;
	rmidi->private = hw;
	*handlep = rmidi;
	return 0;
}

int _snd_rawmidi_hw_open(snd_rawmidi_t **handlep, char *name, snd_config_t *conf,
			 int streams, int mode)
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
		if (strcmp(n->id, "streams") == 0)
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
	return snd_rawmidi_hw_open(handlep, name, card, device, subdevice, streams, mode);
}
				
