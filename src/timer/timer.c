/*
 *  Timer Interface - main file
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include "local.h"

#define SNDRV_FILE_TIMER		"/dev/snd/timer"
#define SNDRV_TIMER_VERSION_MAX	SNDRV_PROTOCOL_VERSION(2, 0, 0)

struct _snd_timer {
	int fd;
};

int snd_timer_open(snd_timer_t **handle, int mode ATTRIBUTE_UNUSED)
{
	int fd, ver;
	snd_timer_t *tmr;

	*handle = NULL;
	
	if ((fd = open(SNDRV_FILE_TIMER, O_RDONLY)) < 0)
		return -errno;
	if (ioctl(fd, SNDRV_TIMER_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SNDRV_PROTOCOL_INCOMPATIBLE(ver, SNDRV_TIMER_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	tmr = (snd_timer_t *) calloc(1, sizeof(snd_timer_t));
	if (tmr == NULL) {
		close(fd);
		return -ENOMEM;
	}
	tmr->fd = fd;
	*handle = tmr;
	return 0;
}

int snd_timer_close(snd_timer_t *handle)
{
	snd_timer_t *tmr;
	int res;

	tmr = handle;
	if (!tmr)
		return -EINVAL;
	res = close(tmr->fd) < 0 ? -errno : 0;
	free(tmr);
	return res;
}

int snd_timer_poll_descriptors_count(snd_timer_t *timer)
{
	assert(timer);
	return 1;
}

int snd_timer_poll_descriptors(snd_timer_t *timer, struct pollfd *pfds, unsigned int space)
{
	assert(timer);
	if (space >= 1) {
		pfds->fd = timer->fd;
		pfds->events = POLLIN;
		return 1;
	}
	return 0;
}

int snd_timer_next_device(snd_timer_t *handle, snd_timer_id_t * tid)
{
	snd_timer_t *tmr;

	tmr = handle;
	if (!tmr || !tid)
		return -EINVAL;
	if (ioctl(tmr->fd, SNDRV_TIMER_IOCTL_NEXT_DEVICE, tid) < 0)
		return -errno;
	return 0;
}

int snd_timer_select(snd_timer_t *handle, snd_timer_select_t * tselect)
{
	snd_timer_t *tmr;

	tmr = handle;
	if (!tmr || !tselect)
		return -EINVAL;
	if (ioctl(tmr->fd, SNDRV_TIMER_IOCTL_SELECT, tselect) < 0)
		return -errno;
	return 0;
}

int snd_timer_info(snd_timer_t *handle, snd_timer_info_t * info)
{
	snd_timer_t *tmr;

	tmr = handle;
	if (!tmr || !info)
		return -EINVAL;
	if (ioctl(tmr->fd, SNDRV_TIMER_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_timer_params(snd_timer_t *handle, snd_timer_params_t * params)
{
	snd_timer_t *tmr;

	tmr = handle;
	if (!tmr || !params)
		return -EINVAL;
	if (ioctl(tmr->fd, SNDRV_TIMER_IOCTL_PARAMS, params) < 0)
		return -errno;
	return 0;
}

int snd_timer_status(snd_timer_t *handle, snd_timer_status_t * status)
{
	snd_timer_t *tmr;

	tmr = handle;
	if (!tmr || !status)
		return -EINVAL;
	if (ioctl(tmr->fd, SNDRV_TIMER_IOCTL_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_timer_start(snd_timer_t *handle)
{
	snd_timer_t *tmr;

	tmr = handle;
	if (!tmr)
		return -EINVAL;
	if (ioctl(tmr->fd, SNDRV_TIMER_IOCTL_START) < 0)
		return -errno;
	return 0;
}

int snd_timer_stop(snd_timer_t *handle)
{
	snd_timer_t *tmr;

	tmr = handle;
	if (!tmr)
		return -EINVAL;
	if (ioctl(tmr->fd, SNDRV_TIMER_IOCTL_STOP) < 0)
		return -errno;
	return 0;
}

int snd_timer_continue(snd_timer_t *handle)
{
	snd_timer_t *tmr;

	tmr = handle;
	if (!tmr)
		return -EINVAL;
	if (ioctl(tmr->fd, SNDRV_TIMER_IOCTL_CONTINUE) < 0)
		return -errno;
	return 0;
}

ssize_t snd_timer_read(snd_timer_t *handle, void *buffer, size_t size)
{
	snd_timer_t *tmr;
	ssize_t result;

	tmr = handle;
	if (!tmr || (!buffer && size > 0))
		return -EINVAL;
	result = read(tmr->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}
