/*
 *  RawMIDI Interface - main file
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

#define SND_FILE_RAWMIDI	"/dev/snd/midiC%iD%i"
#define SND_RAWMIDI_VERSION_MAX	SND_PROTOCOL_VERSION(2, 0, 0)

struct snd_rawmidi {
	int card;
	int device;
	int fd;
	int mode;
};

int snd_rawmidi_open(snd_rawmidi_t **handle, int card, int device, int mode)
{
	int fd, ver;
	char filename[32];
	snd_rawmidi_t *rmidi;

	*handle = NULL;
	
	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	sprintf(filename, SND_FILE_RAWMIDI, card, device);
	if ((fd = open(filename, mode)) < 0) {
		snd_card_load(card);
		if ((fd = open(filename, mode)) < 0)
			return -errno;
	}
	if (ioctl(fd, SND_RAWMIDI_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_RAWMIDI_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	rmidi = (snd_rawmidi_t *) calloc(1, sizeof(snd_rawmidi_t));
	if (rmidi == NULL) {
		close(fd);
		return -ENOMEM;
	}
	rmidi->card = card;
	rmidi->device = device;
	rmidi->fd = fd;
	rmidi->mode = mode;
	*handle = rmidi;
	return 0;
}

int snd_rawmidi_close(snd_rawmidi_t *rmidi)
{
	int res;

	if (!rmidi)
		return -EINVAL;
	res = close(rmidi->fd) < 0 ? -errno : 0;
	free(rmidi);
	return res;
}

int snd_rawmidi_file_descriptor(snd_rawmidi_t *rmidi)
{
	if (!rmidi)
		return -EINVAL;
	return rmidi->fd;
}

int snd_rawmidi_block_mode(snd_rawmidi_t *rmidi, int enable)
{
	long flags;

	if (!rmidi)
		return -EINVAL;
	if (rmidi->mode == SND_RAWMIDI_OPEN_OUTPUT_APPEND ||
	    rmidi->mode == SND_RAWMIDI_OPEN_DUPLEX_APPEND)
		return -EINVAL;
	if ((flags = fcntl(rmidi->fd, F_GETFL)) < 0)
		return -errno;
	if (enable)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	if (fcntl(rmidi->fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

int snd_rawmidi_info(snd_rawmidi_t *rmidi, snd_rawmidi_info_t * info)
{
	if (!rmidi || !info)
		return -EINVAL;
	if (ioctl(rmidi->fd, SND_RAWMIDI_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_rawmidi_channel_params(snd_rawmidi_t *rmidi, snd_rawmidi_params_t * params)
{
	if (!rmidi || !params)
		return -EINVAL;
	if (params->channel < 0 || params->channel > 1)
		return -EINVAL;
	if (ioctl(rmidi->fd, SND_RAWMIDI_IOCTL_CHANNEL_PARAMS, params) < 0)
		return -errno;
	return 0;
}

int snd_rawmidi_channel_status(snd_rawmidi_t *rmidi, snd_rawmidi_status_t * status)
{
	if (!rmidi || !status)
		return -EINVAL;
	if (status->channel < 0 || status->channel > 1)
		return -EINVAL;
	if (ioctl(rmidi->fd, SND_RAWMIDI_IOCTL_CHANNEL_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_rawmidi_output_drain(snd_rawmidi_t *rmidi)
{
	int chn = SND_RAWMIDI_CHANNEL_OUTPUT;
	if (!rmidi)
		return -EINVAL;
	if (ioctl(rmidi->fd, SND_RAWMIDI_IOCTL_CHANNEL_DRAIN, &chn) < 0)
		return -errno;
	return 0;
}

int snd_rawmidi_channel_flush(snd_rawmidi_t *rmidi, int chn)
{
	if (!rmidi)
		return -EINVAL;
	if (chn < 0 || chn > 1)
		return -EINVAL;
	if (ioctl(rmidi->fd, SND_RAWMIDI_IOCTL_CHANNEL_FLUSH) < 0)
		return -errno;
	return 0;
}

int snd_rawmidi_output_flush(snd_rawmidi_t *rmidi)
{
	return snd_rawmidi_channel_flush(rmidi, SND_RAWMIDI_CHANNEL_OUTPUT);
}

int snd_rawmidi_input_flush(snd_rawmidi_t *rmidi)
{
	return snd_rawmidi_channel_flush(rmidi, SND_RAWMIDI_CHANNEL_INPUT);
}

ssize_t snd_rawmidi_write(snd_rawmidi_t *rmidi, const void *buffer, size_t size)
{
	ssize_t result;

	if (!rmidi || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = write(rmidi->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_rawmidi_read(snd_rawmidi_t *rmidi, void *buffer, size_t size)
{
	ssize_t result;

	if (!rmidi || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = read(rmidi->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}
