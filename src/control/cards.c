/*
 *  Control Interface - main file
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
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "control_local.h"
#include "asoundlib.h"

#define SND_FILE_CONTROL	"/dev/snd/controlC%i"
#define SND_FILE_LOAD		"/dev/aloadC%i"

int snd_card_load(int card)
{
	int open_dev;
	char control[32];

	sprintf(control, SND_FILE_CONTROL, card);

	if ((open_dev=open(control, O_RDONLY)) < 0) {
		char aload[32];
		sprintf(aload, SND_FILE_LOAD, card);
		open_dev = open(aload, O_RDONLY);
	}
	if (open_dev >= 0) {
		close (open_dev);
		return 0;
	}
	return open_dev;
}

int snd_cards(void)
{
	int idx, count;
	unsigned int mask;

	mask = snd_cards_mask();
	for (idx = 0, count = 0; idx < SND_CARDS; idx++) {
		if (mask & (1 << idx))
			count++;
	}
	return count;
}

/*
 *  this routine uses very ugly method...
 *    need to do... (use only stat on /proc/asound?)
 *    now is information cached over static variable
 */

unsigned int snd_cards_mask(void)
{
	int idx;
	unsigned int mask;
	static unsigned int save_mask = 0;

	if (save_mask)
		return save_mask;
	for (idx = 0, mask = 0; idx < SND_CARDS; idx++) {
	        if (snd_card_load(idx) >= 0)
			mask |= 1 << idx;
	}
	save_mask = mask;
	return mask;
}

int snd_card_get_index(const char *string)
{
	int card;
	snd_ctl_t *handle;
	snd_ctl_hw_info_t info;

	if (!string || *string == '\0')
		return -EINVAL;
	if ((isdigit(*string) && *(string + 1) == 0) ||
	    (isdigit(*string) && isdigit(*(string + 1)) && *(string + 2) == 0)) {
		sscanf(string, "%i", &card);
		if (card < 0 || card > 31)
			return -EINVAL;
	        if (snd_card_load(card) >= 0)
			return card;
		return -EINVAL;
	}
	for (card = 0; card < 32; card++) {
		if (snd_card_load(card) < 0)
			continue;
		if (snd_ctl_hw_open(&handle, NULL, card) < 0)
			continue;
		if (snd_ctl_hw_info(handle, &info) < 0) {
			snd_ctl_close(handle);
			continue;
		}
		snd_ctl_close(handle);
		if (!strcmp(info.id, string))
			return card;
	}
	return -ENODEV;
}

int snd_card_get_name(int card, char **name)
{
	snd_ctl_t *handle;
	snd_ctl_hw_info_t info;
	int err;
	
	if (name == NULL)
		return -EINVAL;
	if ((err = snd_ctl_hw_open(&handle, NULL, card)) < 0)
		return err;
	if ((err = snd_ctl_hw_info(handle, &info)) < 0) {
		snd_ctl_close(handle);
		return err;
	}
	snd_ctl_close(handle);
	*name = strdup(info.name);
	if (*name == NULL)
		return -ENOMEM;
	return 0;
}

int snd_card_get_longname(int card, char **name)
{
	snd_ctl_t *handle;
	snd_ctl_hw_info_t info;
	int err;
	
	if (name == NULL)
		return -EINVAL;
	if ((err = snd_ctl_hw_open(&handle, NULL, card)) < 0)
		return err;
	if ((err = snd_ctl_hw_info(handle, &info)) < 0) {
		snd_ctl_close(handle);
		return err;
	}
	snd_ctl_close(handle);
	*name = strdup(info.longname);
	if (*name == NULL)
		return -ENOMEM;
	return 0;
}
