/*
 *  Mixer Interface - main file
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

#define SND_FILE_MIXER		"/dev/snd/mixerC%iD%i"
#define SND_MIXER_VERSION_MAX	SND_PROTOCOL_VERSION(2, 1, 0)

struct snd_mixer {
	int card;
	int device;
	int fd;
} ;

int snd_mixer_open(snd_mixer_t **handle, int card, int device)
{
	int fd, ver;
	char filename[32];
	snd_mixer_t *mixer;

	*handle = NULL;

	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	sprintf(filename, SND_FILE_MIXER, card, device);
	if ((fd = open(filename, O_RDWR)) < 0) {
		snd_card_load(card);	
		if ((fd = open(filename, O_RDWR)) < 0) 
			return -errno;
	}
	if (ioctl(fd, SND_MIXER_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_UNCOMPATIBLE(ver, SND_MIXER_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_UNCOMPATIBLE_VERSION;
	}
	mixer = (snd_mixer_t *) calloc(1, sizeof(snd_mixer_t));
	if (mixer == NULL) {
		close(fd);
		return -ENOMEM;
	}
	mixer->card = card;
	mixer->device = device;
	mixer->fd = fd;
	*handle = mixer;
	return 0;
}

int snd_mixer_close(snd_mixer_t *handle)
{
	snd_mixer_t *mixer;
	int res;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	res = close(mixer->fd) < 0 ? -errno : 0;
	free(mixer);
	return res;
}

int snd_mixer_file_descriptor(snd_mixer_t *handle)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	return mixer->fd;
}

int snd_mixer_info(snd_mixer_t *handle, snd_mixer_info_t * info)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !info)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_mixer_elements(snd_mixer_t *handle, snd_mixer_elements_t * elements)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ELEMENTS, elements) < 0)
		return -errno;
	return 0;
}

int snd_mixer_routes(snd_mixer_t *handle, snd_mixer_routes_t * routes)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ROUTES, routes) < 0)
		return -errno;
	return 0;
}

int snd_mixer_groups(snd_mixer_t *handle, snd_mixer_groups_t * groups)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_GROUPS, groups) < 0)
		return -errno;
	return 0;
}

int snd_mixer_group_read(snd_mixer_t *handle, snd_mixer_group_t * group)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_GROUP_READ, group) < 0)
		return -errno;
	return 0;
}

int snd_mixer_group_write(snd_mixer_t *handle, snd_mixer_group_t * group)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_GROUP_WRITE, group) < 0)
		return -errno;
	return 0;
}

int snd_mixer_element_info(snd_mixer_t *handle, snd_mixer_element_info_t * info)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ELEMENT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_mixer_element_read(snd_mixer_t *handle, snd_mixer_element_t * element)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ELEMENT_READ, element) < 0)
		return -errno;
	return 0;
}

int snd_mixer_element_write(snd_mixer_t *handle, snd_mixer_element_t * element)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ELEMENT_WRITE, element) < 0)
		return -errno;
	return 0;
}

int snd_mixer_read(snd_mixer_t *handle, snd_mixer_callbacks_t * callbacks)
{
	snd_mixer_t *mixer;
	int result, count;
	snd_mixer_read_t r;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	count = 0;
	while ((result = read(mixer->fd, &r, sizeof(r))) > 0) {
		if (result != sizeof(r))
			return -EIO;
		if (!callbacks)
			continue;
		switch (r.cmd) {
		case SND_MIXER_READ_REBUILD:
			if (callbacks->rebuild)
				callbacks->rebuild(callbacks->private_data);
			break;
		case SND_MIXER_READ_ELEMENT_VALUE:
		case SND_MIXER_READ_ELEMENT_CHANGE:
		case SND_MIXER_READ_ELEMENT_ROUTE:
		case SND_MIXER_READ_ELEMENT_ADD:
		case SND_MIXER_READ_ELEMENT_REMOVE:
			if (callbacks->element)
				callbacks->element(callbacks->private_data, r.cmd, &r.data.eid);
			break;
		case SND_MIXER_READ_GROUP_CHANGE:
		case SND_MIXER_READ_GROUP_ADD:
		case SND_MIXER_READ_GROUP_REMOVE:
			if (callbacks->group)
				callbacks->group(callbacks->private_data, r.cmd, &r.data.gid);
			break;
		}
		count++;
	}
	return result >= 0 ? count : -errno;
}

void snd_mixer_set_bit(unsigned int *bitmap, int bit, int val)
{
	if (val) {
		bitmap[bit >> 5] |= 1 << (bit & 31);
	} else {
		bitmap[bit >> 5] &= ~(1 << (bit & 31));
	}
}

int snd_mixer_get_bit(unsigned int *bitmap, int bit)
{
	return (bitmap[bit >> 5] & (1 << (bit & 31))) ? 1 : 0;
}
