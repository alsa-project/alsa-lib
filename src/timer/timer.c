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
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "asoundlib.h"

#define SND_FILE_TIMER		"/dev/snd/timer"
#define SND_TIMER_VERSION_MAX	SND_PROTOCOL_VERSION( 1, 0, 0 )

typedef struct {
	int fd;
} snd_timer_t;

int snd_timer_open(void **handle)
{
	int fd, ver;
	snd_timer_t *tmr;

	*handle = NULL;
	
	if ((fd = open(SND_FILE_TIMER, O_RDONLY)) < 0) {
		snd_cards_mask();
		if ((fd = open(SND_FILE_TIMER, O_RDONLY)) < 0)
			return -errno;
	}
	if (ioctl(fd, SND_TIMER_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_UNCOMPATIBLE(ver, SND_TIMER_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_UNCOMPATIBLE_VERSION;
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

int snd_timer_close(void *handle)
{
	snd_timer_t *tmr;
	int res;

	tmr = (snd_timer_t *) handle;
	if (!tmr)
		return -EINVAL;
	res = close(tmr->fd) < 0 ? -errno : 0;
	free(tmr);
	return res;
}

int snd_timer_file_descriptor(void *handle)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr)
		return -EINVAL;
	return tmr->fd;
}

int snd_timer_general_info(void *handle, snd_timer_general_info_t * info)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr || !info)
		return -EINVAL;
	if (ioctl(tmr->fd, SND_TIMER_IOCTL_GINFO, info) < 0)
		return -errno;
	return 0;
}

int snd_timer_select(void *handle, snd_timer_select_t * tselect)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr || !tselect)
		return -EINVAL;
	if (ioctl(tmr->fd, SND_TIMER_IOCTL_SELECT, tselect) < 0)
		return -errno;
	return 0;
}

int snd_timer_info(void *handle, snd_timer_info_t * info)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr || !info)
		return -EINVAL;
	if (ioctl(tmr->fd, SND_TIMER_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_timer_params(void *handle, snd_timer_params_t * params)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr || !params)
		return -EINVAL;
	if (ioctl(tmr->fd, SND_TIMER_IOCTL_PARAMS, params) < 0)
		return -errno;
	return 0;
}

int snd_timer_status(void *handle, snd_timer_status_t * status)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr || !status)
		return -EINVAL;
	if (ioctl(tmr->fd, SND_TIMER_IOCTL_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_timer_start(void *handle)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr)
		return -EINVAL;
	if (ioctl(tmr->fd, SND_TIMER_IOCTL_START) < 0)
		return -errno;
	return 0;
}

int snd_timer_stop(void *handle)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr)
		return -EINVAL;
	if (ioctl(tmr->fd, SND_TIMER_IOCTL_STOP) < 0)
		return -errno;
	return 0;
}

int snd_timer_continue(void *handle)
{
	snd_timer_t *tmr;

	tmr = (snd_timer_t *) handle;
	if (!tmr)
		return -EINVAL;
	if (ioctl(tmr->fd, SND_TIMER_IOCTL_CONTINUE) < 0)
		return -errno;
	return 0;
}

ssize_t snd_timer_read(void *handle, void *buffer, size_t size)
{
	snd_timer_t *tmr;
	ssize_t result;

	tmr = (snd_timer_t *) handle;
	if (!tmr || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = read(tmr->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}
