/*
 *  Hardware dependent Interface - main file
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include "local.h"

#define SNDRV_FILE_HWDEP	"/dev/snd/hwC%iD%i"
#define SNDRV_HWDEP_VERSION_MAX	SNDRV_PROTOCOL_VERSION(1, 0, 0)

struct _snd_hwdep {
	int card;
	int device;
	int fd;
	int mode;
};

int snd_hwdep_open(snd_hwdep_t **handle, int card, int device, int mode)
{
	int fd, ver;
	char filename[32];
	snd_hwdep_t *hwdep;
	assert(handle);

	*handle = NULL;
	
	if (card < 0 || card >= 32)
		return -EINVAL;
	sprintf(filename, SNDRV_FILE_HWDEP, card, device);
	if ((fd = open(filename, mode)) < 0) {
		snd_card_load(card);
		if ((fd = open(filename, mode)) < 0)
			return -errno;
	}
	if (ioctl(fd, SNDRV_HWDEP_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SNDRV_PROTOCOL_INCOMPATIBLE(ver, SNDRV_HWDEP_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	hwdep = (snd_hwdep_t *) calloc(1, sizeof(snd_hwdep_t));
	if (hwdep == NULL) {
		close(fd);
		return -ENOMEM;
	}
	hwdep->card = card;
	hwdep->device = device;
	hwdep->fd = fd;
	hwdep->mode = mode;
	*handle = hwdep;
	return 0;
}

int snd_hwdep_close(snd_hwdep_t *hwdep)
{
	int res;
	assert(hwdep);
	res = close(hwdep->fd) < 0 ? -errno : 0;
	free(hwdep);
	return res;
}

int snd_hwdep_poll_descriptors(snd_hwdep_t *hwdep, struct pollfd *pfds, unsigned int space)
{
	assert(hwdep);
	if (space >= 1) {
		pfds->fd = hwdep->fd;
		pfds->events = POLLOUT | POLLIN;
	}
	return 1;
}

int snd_hwdep_block_mode(snd_hwdep_t *hwdep, int enable)
{
	long flags;
	assert(hwdep);
	if ((flags = fcntl(hwdep->fd, F_GETFL)) < 0)
		return -errno;
	if (enable)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	if (fcntl(hwdep->fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

int snd_hwdep_info(snd_hwdep_t *hwdep, snd_hwdep_info_t *info)
{
	assert(hwdep && info);
	if (ioctl(hwdep->fd, SNDRV_HWDEP_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_hwdep_ioctl(snd_hwdep_t *hwdep, int request, void * arg)
{
	assert(hwdep);
	if (ioctl(hwdep->fd, request, arg) < 0)
		return -errno;
	return 0;
}

ssize_t snd_hwdep_write(snd_hwdep_t *hwdep, const void *buffer, size_t size)
{
	ssize_t result;
	assert(hwdep && (buffer || size == 0));
	result = write(hwdep->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_hwdep_read(snd_hwdep_t *hwdep, void *buffer, size_t size)
{
	ssize_t result;
	assert(hwdep && (buffer || size == 0));
	result = read(hwdep->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}
