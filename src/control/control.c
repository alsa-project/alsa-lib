/*
 *  Control Interface - main file
 *  Copyright (c) 1998,1999,2000 by Jaroslav Kysela <perex@suse.cz>
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

int snd_ctl_open(snd_ctl_t **handle, int card)
{
	int fd, ver;
	char filename[32];
	snd_ctl_t *ctl;

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
	ctl = (snd_ctl_t *) calloc(1, sizeof(snd_ctl_t));
	if (ctl == NULL) {
		close(fd);
		return -ENOMEM;
	}
	ctl->card = card;
	ctl->fd = fd;
	INIT_LIST_HEAD(&ctl->hlist);
	*handle = ctl;
	return 0;
}

int snd_ctl_close(snd_ctl_t *handle)
{
	int res;
	assert(handle);
	res = close(handle->fd) < 0 ? -errno : 0;
	free(handle);
	return res;
}

int snd_ctl_file_descriptor(snd_ctl_t *handle)
{
	assert(handle);
	return handle->fd;
}

int snd_ctl_hw_info(snd_ctl_t *handle, snd_ctl_hw_info_t *info)
{
	assert(handle && info);
	if (ioctl(handle->fd, SND_CTL_IOCTL_HW_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_clist(snd_ctl_t *handle, snd_control_list_t *list)
{
	assert(handle && list);
	if (ioctl(handle->fd, SND_CTL_IOCTL_CONTROL_LIST, list) < 0)
		return -errno;
	return 0;
}

int snd_ctl_cinfo(snd_ctl_t *handle, snd_control_info_t *info)
{
	assert(handle && info && (info->id.name[0] || info->id.numid));
	if (ioctl(handle->fd, SND_CTL_IOCTL_CONTROL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_cread(snd_ctl_t *handle, snd_control_t *control)
{
	assert(handle && control && (control->id.name[0] || control->id.numid));
	if (ioctl(handle->fd, SND_CTL_IOCTL_CONTROL_READ, control) < 0)
		return -errno;
	return 0;
}

int snd_ctl_cwrite(snd_ctl_t *handle, snd_control_t *control)
{
	assert(handle && control && (control->id.name[0] || control->id.numid));
	if (ioctl(handle->fd, SND_CTL_IOCTL_CONTROL_WRITE, control) < 0)
		return -errno;
	return 0;
}

int snd_ctl_hwdep_info(snd_ctl_t *handle, snd_hwdep_info_t * info)
{
	assert(handle && info);
	if (ioctl(handle->fd, SND_CTL_IOCTL_HWDEP_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_info(snd_ctl_t *handle, snd_pcm_info_t * info)
{
	assert(handle && info);
	if (ioctl(handle->fd, SND_CTL_IOCTL_PCM_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_prefer_subdevice(snd_ctl_t *handle, int subdev)
{
	assert(handle);
	if (ioctl(handle->fd, SND_CTL_IOCTL_PCM_PREFER_SUBDEVICE, &subdev) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_info(snd_ctl_t *handle, snd_rawmidi_info_t * info)
{
	assert(handle && info);
	if (ioctl(handle->fd, SND_CTL_IOCTL_RAWMIDI_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_read(snd_ctl_t *handle, snd_ctl_callbacks_t * callbacks)
{
	int result, count;
	snd_ctl_event_t r;

	assert(handle);
	count = 0;
	while ((result = read(handle->fd, &r, sizeof(r))) > 0) {
		if (result != sizeof(r))
			return -EIO;
		if (!callbacks)
			continue;
		switch (r.type) {
		case SND_CTL_EVENT_REBUILD:
			if (callbacks->rebuild)
				callbacks->rebuild(handle, callbacks->private_data);
			break;
		case SND_CTL_EVENT_VALUE:
			if (callbacks->value)
				callbacks->value(handle, callbacks->private_data, &r.data.id);
			break;
		case SND_CTL_EVENT_CHANGE:
			if (callbacks->change)
				callbacks->change(handle, callbacks->private_data, &r.data.id);
			break;
		case SND_CTL_EVENT_ADD:
			if (callbacks->add)
				callbacks->add(handle, callbacks->private_data, &r.data.id);
			break;
		case SND_CTL_EVENT_REMOVE:
			if (callbacks->remove)
				callbacks->remove(handle, callbacks->private_data, &r.data.id);
			break;
		}
		count++;
	}
	return result >= 0 ? count : -errno;
}
