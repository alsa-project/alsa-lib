/*
 *  Control Interface - main file
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

#define SND_FILE_CONTROL	"/dev/snd/control%i"
#define SND_CTL_VERSION_MAX	SND_PROTOCOL_VERSION( 1, 0, 0 )

typedef struct {
	int card;
	int fd;
} snd_ctl_t;

int snd_ctl_open(void **handle, int card)
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
	if (SND_PROTOCOL_UNCOMPATIBLE(ver, SND_CTL_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_UNCOMPATIBLE_VERSION;
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

int snd_ctl_close(void *handle)
{
	snd_ctl_t *ctl;
	int res;

	ctl = (snd_ctl_t *) handle;
	if (!ctl)
		return -EINVAL;
	res = close(ctl->fd) < 0 ? -errno : 0;
	free(ctl);
	return res;
}

int snd_ctl_file_descriptor(void *handle)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl)
		return -EINVAL;
	return ctl->fd;
}

int snd_ctl_hw_info(void *handle, struct snd_ctl_hw_info *info)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !info)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_HW_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_switches(void *handle)
{
	snd_ctl_t *ctl;
	int result;

	ctl = (snd_ctl_t *) handle;
	if (!ctl)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_SWITCHES, &result) < 0)
		return -errno;
	return result;
}

int snd_ctl_switch(void *handle, const char *switch_id)
{
	snd_ctl_t *ctl;
	struct snd_ctl_switch uswitch;
	int idx, switches, err;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !switch_id)
		return -EINVAL;
	/* bellow implementation isn't optimized for speed */
	/* info about switches should be cached in the snd_ctl_t structure */
	if ((switches = snd_ctl_switches(handle)) < 0)
		return switches;
	for (idx = 0; idx < switches; idx++) {
		if ((err = snd_ctl_switch_read(handle, idx, &uswitch)) < 0)
			return err;
		if (!strncmp(switch_id, uswitch.name, sizeof(uswitch.name)))
			return idx;
	}
	return -EINVAL;
}

int snd_ctl_switch_read(void *handle, int switchn, snd_ctl_switch_t *data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || switchn < 0)
		return -EINVAL;
	bzero(data, sizeof(snd_ctl_switch_t));
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_SWITCH_READ, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_switch_write(void *handle, int switchn, snd_ctl_switch_t *data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || switchn < 0)
		return -EINVAL;
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_SWITCH_WRITE, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_info(void *handle, int dev, snd_pcm_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_playback_info(void *handle, int dev, snd_pcm_playback_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_PLAYBACK_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_record_info(void *handle, int dev, snd_pcm_record_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_RECORD_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_playback_switches(void *handle, int dev)
{
	snd_ctl_t *ctl;
	int result;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_PSWITCHES, &result) < 0)
		return -errno;
	return result;
}

int snd_ctl_pcm_playback_switch(void *handle, int dev, const char *switch_id)
{
	snd_ctl_t *ctl;
	snd_pcm_switch_t uswitch;
	int idx, switches, err;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !switch_id || dev < 0)
		return -EINVAL;
	/* bellow implementation isn't optimized for speed */
	/* info about switches should be cached in the snd_ctl_t structure */
	if ((switches = snd_ctl_pcm_playback_switches(handle, dev)) < 0)
		return switches;
	for (idx = 0; idx < switches; idx++) {
		if ((err = snd_ctl_pcm_playback_switch_read(handle, dev, idx, &uswitch)) < 0)
			return err;
		if (!strncmp(switch_id, uswitch.name, sizeof(uswitch.name)))
			return idx;
	}
	return -EINVAL;
}

int snd_ctl_pcm_playback_switch_read(void *handle, int dev, int switchn, snd_pcm_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	bzero(data, sizeof(snd_pcm_switch_t));
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_PSWITCH_READ, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_playback_switch_write(void *handle, int dev, int switchn, snd_pcm_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_PSWITCH_WRITE, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_record_switches(void *handle, int dev)
{
	snd_ctl_t *ctl;
	int result;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_RSWITCHES, &result) < 0)
		return -errno;
	return result;
}

int snd_ctl_pcm_record_switch(void *handle, int dev, const char *switch_id)
{
	snd_ctl_t *ctl;
	snd_pcm_switch_t uswitch;
	int idx, switches, err;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !switch_id || dev < 0)
		return -EINVAL;
	/* bellow implementation isn't optimized for speed */
	/* info about switches should be cached in the snd_ctl_t structure */
	if ((switches = snd_ctl_pcm_record_switches(handle, dev)) < 0)
		return switches;
	for (idx = 0; idx < switches; idx++) {
		if ((err = snd_ctl_pcm_record_switch_read(handle, dev, idx, &uswitch)) < 0)
			return err;
		if (!strncmp(switch_id, uswitch.name, sizeof(uswitch.name)))
			return idx;
	}
	return -EINVAL;
}

int snd_ctl_pcm_record_switch_read(void *handle, int dev, int switchn, snd_pcm_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	bzero(data, sizeof(snd_pcm_switch_t));
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_RSWITCH_READ, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_pcm_record_switch_write(void *handle, int dev, int switchn, snd_pcm_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_PCM_RSWITCH_WRITE, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_mixer_info(void *handle, int dev, snd_mixer_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_mixer_switches(void *handle, int dev)
{
	snd_ctl_t *ctl;
	int result;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_SWITCHES, &result) < 0)
		return -errno;
	return result;
}

int snd_ctl_mixer_switch(void *handle, int dev, const char *switch_id)
{
	snd_ctl_t *ctl;
	snd_mixer_switch_t uswitch;
	int idx, switches, err;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !switch_id || dev < 0)
		return -EINVAL;
	/* bellow implementation isn't optimized for speed */
	/* info about switches should be cached in the snd_ctl_t structure */
	if ((switches = snd_ctl_mixer_switches(handle, dev)) < 0)
		return switches;
	for (idx = 0; idx < switches; idx++) {
		if ((err = snd_ctl_mixer_switch_read(handle, dev, idx, &uswitch)) < 0)
			return err;
		if (!strncmp(switch_id, uswitch.name, sizeof(uswitch.name)))
			return idx;
	}
	return -EINVAL;
}

int snd_ctl_mixer_switch_read(void *handle, int dev, int switchn, snd_mixer_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	bzero(data, sizeof(snd_mixer_switch_t));
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_SWITCH_READ, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_mixer_switch_write(void *handle, int dev, int switchn, snd_mixer_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_MIXER_SWITCH_WRITE, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_info(void *handle, int dev, snd_rawmidi_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !info)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_output_info(void *handle, int dev, snd_rawmidi_output_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_OUTPUT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_input_info(void *handle, int dev, snd_rawmidi_input_info_t * info)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !info || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_INPUT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_output_switches(void *handle, int dev)
{
	snd_ctl_t *ctl;
	int result;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_OSWITCHES, &result) < 0)
		return -errno;
	return result;
}

int snd_ctl_rawmidi_output_switch(void *handle, int dev, const char *switch_id)
{
	snd_ctl_t *ctl;
	snd_rawmidi_switch_t uswitch;
	int idx, switches, err;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !switch_id || dev < 0)
		return -EINVAL;
	/* bellow implementation isn't optimized for speed */
	/* info about switches should be cached in the snd_ctl_t structure */
	if ((switches = snd_ctl_rawmidi_output_switches(handle, dev)) < 0)
		return switches;
	for (idx = 0; idx < switches; idx++) {
		if ((err = snd_ctl_rawmidi_output_switch_read(handle, dev, idx, &uswitch)) < 0)
			return err;
		if (!strncmp(switch_id, uswitch.name, sizeof(uswitch.name)))
			return idx;
	}
	return -EINVAL;
}

int snd_ctl_rawmidi_output_switch_read(void *handle, int dev, int switchn, snd_rawmidi_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	bzero(data, sizeof(snd_rawmidi_switch_t));
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_OSWITCH_READ, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_output_switch_write(void *handle, int dev, int switchn, snd_rawmidi_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_OSWITCH_WRITE, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_input_switches(void *handle, int dev)
{
	snd_ctl_t *ctl;
	int result;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || dev < 0)
		return -EINVAL;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_ISWITCHES, &result) < 0)
		return -errno;
	return result;
}

int snd_ctl_rawmidi_input_switch(void *handle, int dev, const char *switch_id)
{
	snd_ctl_t *ctl;
	snd_rawmidi_switch_t uswitch;
	int idx, switches, err;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !switch_id || dev < 0)
		return -EINVAL;
	/* bellow implementation isn't optimized for speed */
	/* info about switches should be cached in the snd_ctl_t structure */
	if ((switches = snd_ctl_rawmidi_input_switches(handle, dev)) < 0)
		return switches;
	for (idx = 0; idx < switches; idx++) {
		if ((err = snd_ctl_rawmidi_input_switch_read(handle, dev, idx, &uswitch)) < 0)
			return err;
		if (!strncmp(switch_id, uswitch.name, sizeof(uswitch.name)))
			return idx;
	}
	return -EINVAL;
}

int snd_ctl_rawmidi_input_switch_read(void *handle, int dev, int switchn, snd_rawmidi_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	bzero(data, sizeof(snd_rawmidi_switch_t));
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_ISWITCH_READ, data) < 0)
		return -errno;
	return 0;
}

int snd_ctl_rawmidi_input_switch_write(void *handle, int dev, int switchn, snd_rawmidi_switch_t * data)
{
	snd_ctl_t *ctl;

	ctl = (snd_ctl_t *) handle;
	if (!ctl || !data || dev < 0 || switchn < 0)
		return -EINVAL;
	data->switchn = switchn;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_DEVICE, &dev) < 0)
		return -errno;
	if (ioctl(ctl->fd, SND_CTL_IOCTL_RAWMIDI_ISWITCH_WRITE, data) < 0)
		return -errno;
	return 0;
}
