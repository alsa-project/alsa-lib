/*
 *  PCM Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@jcu.cz>
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

#define SND_FILE_PCM		"/dev/snd/pcm%i%i"
#define SND_PCM_VERSION_MAX	SND_PROTOCOL_VERSION( 1, 0, 1 )

typedef struct {
	int card;
	int device;
	int fd;
} snd_pcm_t;

int snd_pcm_open(void **handle, int card, int device, int mode)
{
	int fd, ver;
	char filename[32];
	snd_pcm_t *pcm;

	*handle = NULL;
	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	sprintf(filename, SND_FILE_PCM, card, device);
	if ((fd = open(filename, mode)) < 0)
		return -errno;
	if (ioctl(fd, SND_PCM_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_UNCOMPATIBLE(ver, SND_PCM_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_UNCOMPATIBLE_VERSION;
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

int snd_pcm_close(void *handle)
{
	snd_pcm_t *pcm;
	int res;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	res = close(pcm->fd) < 0 ? -errno : 0;
	free(pcm);
	return res;
}

int snd_pcm_file_descriptor(void *handle)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	return pcm->fd;
}

int snd_pcm_block_mode(void *handle, int enable)
{
	snd_pcm_t *pcm;
	long flags;

	pcm = (snd_pcm_t *) handle;
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

int snd_pcm_info(void *handle, snd_pcm_info_t * info)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !info)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_info(void *handle, snd_pcm_playback_info_t * info)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !info)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_pcm_record_info(void *handle, snd_pcm_record_info_t * info)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !info)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_RECORD_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_switches(void *handle)
{
	snd_pcm_t *pcm;
	int result;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PSWITCHES, &result) < 0)
		return -errno;
	return result;
}

int snd_pcm_playback_switch(void *handle, const char *switch_id)
{
	snd_pcm_t *pcm;
	snd_pcm_switch_t uswitch;
	int idx, switches, err;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !switch_id)
		return -EINVAL;
	/* bellow implementation isn't optimized for speed */
	/* info about switches should be cached in the snd_mixer_t structure */
	if ((switches = snd_pcm_playback_switches(handle)) < 0)
		return switches;
	for (idx = 0; idx < switches; idx++) {
		if ((err = snd_pcm_playback_switch_read(handle, idx, &uswitch)) < 0)
			return err;
		if (!strncmp(switch_id, uswitch.name, sizeof(uswitch.name)))
			return idx;
	}
	return -EINVAL;
}

int snd_pcm_playback_switch_read(void *handle, int switchn, snd_pcm_switch_t * data)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !data || switchn < 0)
		return -EINVAL;
	bzero(data, sizeof(snd_pcm_switch_t));
	data->switchn = switchn;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PSWITCH_READ, data) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_switch_write(void *handle, int switchn, snd_pcm_switch_t * data)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !data || switchn < 0)
		return -EINVAL;
	data->switchn = switchn;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PSWITCH_WRITE, data) < 0)
		return -errno;
	return 0;
}

int snd_pcm_record_switches(void *handle)
{
	snd_pcm_t *pcm;
	int result;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_RSWITCHES, &result) < 0)
		return -errno;
	return result;
}

int snd_pcm_record_switch(void *handle, const char *switch_id)
{
	snd_pcm_t *pcm;
	snd_pcm_switch_t uswitch;
	int idx, switches, err;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !switch_id)
		return -EINVAL;
	/* bellow implementation isn't optimized for speed */
	/* info about switches should be cached in the snd_mixer_t structure */
	if ((switches = snd_pcm_record_switches(handle)) < 0)
		return switches;
	for (idx = 0; idx < switches; idx++) {
		if ((err = snd_pcm_record_switch_read(handle, idx, &uswitch)) < 0)
			return err;
		if (!strncmp(switch_id, uswitch.name, sizeof(uswitch.name)))
			return idx;
	}
	return -EINVAL;
}

int snd_pcm_record_switch_read(void *handle, int switchn, snd_pcm_switch_t * data)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !data || switchn < 0)
		return -EINVAL;
	bzero(data, sizeof(snd_pcm_switch_t));
	data->switchn = switchn;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_RSWITCH_READ, data) < 0)
		return -errno;
	return 0;
}

int snd_pcm_record_switch_write(void *handle, int switchn, snd_pcm_switch_t * data)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !data || switchn < 0)
		return -EINVAL;
	data->switchn = switchn;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_RSWITCH_WRITE, data) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_format(void *handle, snd_pcm_format_t * format)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !format)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_FORMAT, format) < 0)
		return -errno;
	return 0;
}

int snd_pcm_record_format(void *handle, snd_pcm_format_t * format)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !format)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_RECORD_FORMAT, format) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_params(void *handle, snd_pcm_playback_params_t * params)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !params)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_PARAMS, params) < 0)
		return -errno;
	return 0;
}

int snd_pcm_record_params(void *handle, snd_pcm_record_params_t * params)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !params)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_RECORD_PARAMS, params) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_status(void *handle, snd_pcm_playback_status_t * status)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !status)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_pcm_record_status(void *handle, snd_pcm_record_status_t * status)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || !status)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_RECORD_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_pcm_drain_playback(void *handle)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_DRAIN_PLAYBACK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_flush_playback(void *handle)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_FLUSH_PLAYBACK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_flush_record(void *handle)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_FLUSH_RECORD) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_pause(void *handle, int enable)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_PAUSE, &enable) < 0)
		return -errno;
	return 0;
}

int snd_pcm_playback_time(void *handle, int enable)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_PLAYBACK_TIME, &enable) < 0)
		return -errno;
	return 0;
}

int snd_pcm_record_time(void *handle, int enable)
{
	snd_pcm_t *pcm;

	pcm = (snd_pcm_t *) handle;
	if (!pcm)
		return -EINVAL;
	if (ioctl(pcm->fd, SND_PCM_IOCTL_RECORD_TIME, &enable) < 0)
		return -errno;
	return 0;
}

ssize_t snd_pcm_write(void *handle, const void *buffer, size_t size)
{
	snd_pcm_t *pcm;
	ssize_t result;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = write(pcm->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_pcm_read(void *handle, void *buffer, size_t size)
{
	snd_pcm_t *pcm;
	ssize_t result;

	pcm = (snd_pcm_t *) handle;
	if (!pcm || (!buffer && size > 0) || size < 0)
		return -EINVAL;
	result = read(pcm->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}
