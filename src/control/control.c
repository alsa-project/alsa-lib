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
#include "asoundlib.h"

#define SND_FILE_CONTROL	"/dev/snd/controlC%i"
#define SND_CTL_VERSION_MAX	SND_PROTOCOL_VERSION(1, 0, 0)

struct snd_ctl {
	int card;
	int fd;
};

int snd_ctl_open(snd_ctl_t **handle, int card)
{
	int fd, ver;
	char filename[32];
	snd_ctl_t *ctl;

	*handle = NULL;	

	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
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
	*handle = ctl;
	return 0;
}

int snd_ctl_close(snd_ctl_t *handle)
{
	snd_ctl_t *ctl;
	int res;

	ctl = handle;
	if (!ctl)
		return -EINVAL;
	res = close(ctl->fd) < 0 ? -errno : 0;
	free(ctl);
	return res;
}

int snd_ctl_file_descriptor(snd_ctl_t *handle)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl)
		return -EINVAL;
	return ctl->fd;
}

int snd_ctl_hw_info(snd_ctl_t *handle, struct snd_ctl_hw_info *info)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !info)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_HW_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_switch_list(snd_ctl_t *handle, snd_switch_list_t *list)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !list)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_SWITCH_LIST, list) < 0)
		return -errno;
	return 0;
}

int snd_ctl_switch_read(snd_ctl_t *handle, snd_switch_t *sw)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !sw || sw->name[0] == '\0')
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_SWITCH_READ, sw) < 0)
		return -errno;
	return 0;
}

int snd_ctl_switch_write(snd_ctl_t *handle, snd_switch_t *sw)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !sw || sw->name[0] == '\0')
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_SWITCH_WRITE, sw) < 0)
		return -errno;
	return 0;
}

int snd_ctl_hwdep_info(snd_ctl_t *handle, int dev, snd_hwdep_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_HWDEP_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_HWDEP_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_info(snd_ctl_t *handle, int dev, snd_pcm_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_channel_info(snd_ctl_t *handle, int dev, int chn, int subdev, snd_pcm_channel_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !info || dev < 0 || chn < 0 || chn > 1 || subdev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_CHANNEL, &chn) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_SUBDEVICE, &subdev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_CHANNEL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_channel_prefer_subdevice(snd_ctl_t *handle, int dev, int chn, int subdev)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || dev < 0 || chn < 0 || chn > 1)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_CHANNEL, &chn) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_PREFER_SUBDEVICE, &subdev) < 0)
		return -errno;
	return 0;
}

int snd_ctl_mixer_info(snd_ctl_t *handle, int dev, snd_mixer_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_info(snd_ctl_t *handle, int dev, snd_rawmidi_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = handle;
	if (!ctl || !info)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_read(snd_ctl_t *handle, snd_ctl_callbacks_t * callbacks)
{
	snd_ctl_t *ctl;
	int result, count;
	snd_ctl_read_t r;

	ctl = handle;
	if (!ctl)
		return -EINVAL;
	count = 0;
	while ((result = read(ctl->fd, &r, sizeof(r))) > 0) {
		if (result != sizeof(r))
			return -EIO;
		if (!callbacks)
			continue;
		switch (r.cmd) {
		case SND_CTL_READ_REBUILD:
			if (callbacks->rebuild)
				callbacks->rebuild(callbacks->private_data);
			break;
		case SND_CTL_READ_SWITCH_VALUE:
		case SND_CTL_READ_SWITCH_CHANGE:
		case SND_CTL_READ_SWITCH_ADD:
		case SND_CTL_READ_SWITCH_REMOVE:
			if (callbacks->xswitch)
				callbacks->xswitch(callbacks->private_data,
						   r.cmd, r.data.sw.iface,
						   r.data.sw.device,
						   r.data.sw.channel,
						   &r.data.sw.switem);
			break;
		}
		count++;
	}
	return result >= 0 ? count : -errno;
}
