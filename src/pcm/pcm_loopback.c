/*
 *  PCM LoopBack Interface - main file
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

#define SND_FILE_PCM_LB		"/proc/asound/%i/pcmloopD%iS%i%s"
#define SND_PCM_LB_VERSION_MAX	SND_PROTOCOL_VERSION( 1, 0, 0 )

struct snd_pcm_loopback {
	int card;
	int device;
	int fd;
} ;

int snd_pcm_loopback_open(snd_pcm_loopback_t **handle, int card, int device, int subchn, int mode)
{
	int fd, ver;
	char filename[32];
	snd_pcm_loopback_t *lb;

	*handle = NULL;

	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	sprintf(filename, SND_FILE_PCM_LB, card, device, subchn,
		mode == SND_PCM_LB_OPEN_CAPTURE ? "c" : "p");
	if ((fd = open(filename, mode)) < 0) {
		snd_card_load(card);
		if ((fd = open(filename, mode)) < 0) 
			return -errno;
	}
	if (ioctl(fd, SND_PCM_LB_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_PCM_LB_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	lb = (snd_pcm_loopback_t *) calloc(1, sizeof(snd_pcm_loopback_t));
	if (lb == NULL) {
		close(fd);
		return -ENOMEM;
	}
	lb->card = card;
	lb->device = device;
	lb->fd = fd;
	*handle = lb;
	return 0;
}

int snd_pcm_loopback_close(snd_pcm_loopback_t *handle)
{
	snd_pcm_loopback_t *lb;
	int res;

	lb = handle;
	if (!lb)
		return -EINVAL;
	res = close(lb->fd) < 0 ? -errno : 0;
	free(lb);
	return res;
}

int snd_pcm_loopback_file_descriptor(snd_pcm_loopback_t *handle)
{
	snd_pcm_loopback_t *lb;

	lb = handle;
	if (!lb)
		return -EINVAL;
	return lb->fd;
}

int snd_pcm_loopback_block_mode(snd_pcm_loopback_t *handle, int enable)
{
	snd_pcm_loopback_t *lb;
	long flags;

	lb = handle;
	if (!lb)
		return -EINVAL;
	if ((flags = fcntl(lb->fd, F_GETFL)) < 0)
		return -errno;
	if (enable)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	if (fcntl(lb->fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

int snd_pcm_loopback_stream_mode(snd_pcm_loopback_t *handle, int mode)
{
	snd_pcm_loopback_t *lb;
	long lmode = mode;

	lb = handle;
	if (!lb)
		return -EINVAL;
	if (ioctl(lb->fd, SND_PCM_LB_IOCTL_STREAM_MODE, &lmode) < 0)
		return -errno;
	return 0;
}

int snd_pcm_loopback_format(snd_pcm_loopback_t *handle, snd_pcm_format_t * format)
{
	snd_pcm_loopback_t *lb;

	lb = handle;
	if (!lb)
		return -EINVAL;
	if (ioctl(lb->fd, SND_PCM_LB_IOCTL_FORMAT, format) < 0)
		return -errno;
	return 0;
}

ssize_t snd_pcm_loopback_read(snd_pcm_loopback_t *handle, void *buffer, size_t size)
{
	snd_pcm_loopback_t *lb;
	ssize_t result;

	lb = handle;
	if (!lb)
		return -EINVAL;
	result = read(lb->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}
