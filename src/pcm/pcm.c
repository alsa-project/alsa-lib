/*
 *  PCM Interface - main file
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

#define SND_FILE_PCM		"/dev/snd/pcmC%iD%i"
#define SND_PCM_VERSION_MAX	SND_PROTOCOL_VERSION( 1, 0, 1 )

struct snd_pcm {
	int card;
	int device;
	int fd;
};

int snd_pcm_open(snd_pcm_t **handle, int card, int device, int mode)
{
	int fd, ver;
	char filename[32];
	snd_pcm_t *pcm;

	*handle = NULL;
	
	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	sprintf(filename, SND_FILE_PCM, card, device);
	if ((fd = open(filename, mode)) < 0) {
		snd_card_load(card);
		if ((fd = open(filename, mode)) < 0) 
			return -errno;
	}
	if (ioctl(fd, SND_PCM_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_PCM_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	pcm = (snd_pcm_t *) calloc(1, sizeof(snd_pcm_t));
	if (pcm == NULL) {
		close(fd);
		return -ENOMEM;
	}
	pcm->card = card;
	pcm->device = device;
	pcm->fd = fd;
	*handle = pcm;
	return 0;
}

int snd_pcm_close(snd_pcm_t *handle)
{
	snd_pcm_t *pcm;
	int res;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	res = close(pcm->fd) < 0 ? -errno : 0;
	free(pcm);
	return res;
}

int snd_pcm_file_descriptor(snd_pcm_t *handle)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	return pcm->fd;
}

int snd_pcm_block_mode(snd_pcm_t *handle, int enable)
{
	snd_pcm_t *pcm;
	long flags;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	if ((flags = fcntl(pcm->fd, F_GETFL)) < 0)
		return -errno;
	if (enable)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	if (fcntl(pcm->fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

int snd_pcm_info(snd_pcm_t *handle, snd_pcm_info_t * info)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !info)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_info(snd_pcm_t *handle, snd_pcm_playback_info_t * info)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !info)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_pcm_capture_info(snd_pcm_t *handle, snd_pcm_capture_info_t * info)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !info)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CAPTURE_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_format(snd_pcm_t *handle, snd_pcm_format_t * format)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !format)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_FORMAT, format) < 0)
		return -errno;
	return 0;
}

int snd_pcm_capture_format(snd_pcm_t *handle, snd_pcm_format_t * format)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !format)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CAPTURE_FORMAT, format) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_params(snd_pcm_t *handle, snd_pcm_playback_params_t * params)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !params)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_PARAMS, params) < 0)
		return -errno;
	return 0;
}

int snd_pcm_capture_params(snd_pcm_t *handle, snd_pcm_capture_params_t * params)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !params)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CAPTURE_PARAMS, params) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_status(snd_pcm_t *handle, snd_pcm_playback_status_t * status)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !status)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_pcm_capture_status(snd_pcm_t *handle, snd_pcm_capture_status_t * status)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm || !status)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CAPTURE_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_pcm_drain_playback(snd_pcm_t *handle)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_DRAIN_PLAYBACK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_flush_playback(snd_pcm_t *handle)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_FLUSH_PLAYBACK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_flush_capture(snd_pcm_t *handle)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_FLUSH_CAPTURE) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_pause(snd_pcm_t *handle, int enable)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_PAUSE, &enable) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_time(snd_pcm_t *handle, int enable)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_TIME, &enable) < 0)
		return -errno;
	return 0;
}

int snd_pcm_capture_time(snd_pcm_t *handle, int enable)
{
	snd_pcm_t *pcm;

	pcm = handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_CAPTURE_TIME, &enable) < 0)
		return -errno;
	return 0;
}

ssize_t snd_pcm_write(snd_pcm_t *handle, const void *buffer, size_t size)
{
	snd_pcm_t *pcm;
	ssize_t result;

	pcm = handle;
	if (!pcm || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = write(pcm->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_pcm_read(snd_pcm_t *handle, void *buffer, size_t size)
{
	snd_pcm_t *pcm;
	ssize_t result;

	pcm = handle;
	if (!pcm || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = read(pcm->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}
