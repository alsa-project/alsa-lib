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
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "asoundlib.h"

#define SND_FILE_CONTROL	"/dev/snd/control%i"

int snd_card_load(int card)
{
	int open_dev;
	char control[32];
	char aload[32];

	sprintf (control, "/dev/snd/control%d",card);
	sprintf (aload, "/dev/aload%d", card);	 

	if ((open_dev=open(control, O_RDONLY)) < 0) {
		close(open(aload, O_RDONLY));
	} else {
		close (open_dev);
	}
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
	int fd, idx;
	unsigned int mask;
	char filename[32];
	static unsigned int save_mask = 0;

	if (save_mask)
		return save_mask;
	for (idx = 0, mask = 0; idx < SND_CARDS; idx++) {
	        snd_card_load(idx);
		sprintf(filename, SND_FILE_CONTROL, idx);
		if ((fd = open(filename, O_RDWR)) < 0) {
			snd_card_load(idx);
			if ((fd = open(filename, O_RDWR)) < 0)
				continue;
		}
		close(fd);
		mask |= 1 << idx;
	}
	save_mask = mask;
	return mask;
}

int snd_card_name(const char *string)
{
	int card, bitmask;
	void *handle;
	struct snd_ctl_hw_info info;

	if (!string)
		return -EINVAL;
	bitmask = snd_cards_mask();
	if (!bitmask)
		return -ENODEV;
	if ((isdigit(*string) && *(string + 1) == 0) ||
	    (isdigit(*string) && isdigit(*(string + 1)) && *(string + 2) == 0)) {
		sscanf(string, "%i", &card);
		card--;
		if (card < 0 || card > 31)
			return -EINVAL;
		if (card < 0 || !((1 << card) & bitmask))
			return -EINVAL;
		return card;
	}
	for (card = 0; card < 32; card++) {
		if (!((1 << card) & bitmask))
			continue;
		if (snd_ctl_open(&handle, card) < 0)
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
