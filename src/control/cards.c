/**
 * \file control/cards.c
 * \brief Basic Soundcard Operations
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 1998-2001
 */
/*
 *  Soundcard Operations - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "control_local.h"

#ifndef DOC_HIDDEN
#define SND_FILE_CONTROL	ALSA_DEVICE_DIRECTORY "controlC%i"
#define SND_FILE_LOAD		ALOAD_DEVICE_DIRECTORY "aloadC%i"
#endif

static int snd_card_load2(const char *control)
{
	int open_dev;
	snd_ctl_card_info_t info;

	open_dev = snd_open_device(control, O_RDONLY);
	if (open_dev >= 0) {
		if (ioctl(open_dev, SNDRV_CTL_IOCTL_CARD_INFO, &info) < 0) {
			int err = -errno;
			close(open_dev);
			return err;
		}
		close(open_dev);
		return info.card;
	} else {
		return -errno;
	}
}

static int snd_card_load1(int card)
{
	int res;
	char control[sizeof(SND_FILE_CONTROL) + 10];

	sprintf(control, SND_FILE_CONTROL, card);
	res = snd_card_load2(control);
#ifdef SUPPORT_ALOAD
	if (res < 0) {
		char aload[sizeof(SND_FILE_LOAD) + 10];
		sprintf(aload, SND_FILE_LOAD, card);
		res = snd_card_load2(aload);
	}
#endif
	return res;
}

/**
 * \brief Try to load the driver for a card.
 * \param card Card index.
 * \return 1 if driver is present, zero if driver is not present.
 */
int snd_card_load(int card)
{
	return !!(snd_card_load1(card) >= 0);
}

/**
 * \brief Iterate over physical sound cards.
 *
 * This function takes the index of a physical sound card and sets it to the
 * index of the next card. If index is -1, it is set to the index of the first
 * card. After the last card, the index is set to -1.
 *
 * For example, if you have 2 sound cards (with index 0 and 1), the index will
 * be modified as follows:
 *
 * - -1 --> 0
 * - 0 --> 1
 * - 1 --> -1
 *
 * This does not work for virtual sound cards.
 *
 * \param rcard Index of current card. The index of the next card is stored
 *        here.
 * \result zero if success, otherwise a negative error code.
 */
int snd_card_next(int *rcard)
{
	int card;
	
	if (rcard == NULL)
		return -EINVAL;
	card = *rcard;
	card = card < 0 ? 0 : card + 1;
	for (; card < SND_MAX_CARDS; card++) {
		if (snd_card_load(card)) {
			*rcard = card;
			return 0;
		}
	}
	*rcard = -1;
	return 0;
}

/**
 * \brief Convert a card string to the card index.
 *
 * This works only for physical sound cards, not for virtual cards.
 *
 * \param string A string identifying the card.
 * \return The index of the card. On error, a a negative error code
 *         is returned.
 *
 * The accepted formats for "string" are:
 * - The index of the card (as listed in /proc/asound/cards), given as string
 * - The ID of the card (as listed in /proc/asound/cards)
 * - The control device name (like /dev/snd/controlC0)
 */
int snd_card_get_index(const char *string)
{
	int card, err;
	snd_ctl_t *handle;
	snd_ctl_card_info_t info;

	if (!string || *string == '\0')
		return -EINVAL;
	if ((isdigit(*string) && *(string + 1) == 0) ||
	    (isdigit(*string) && isdigit(*(string + 1)) && *(string + 2) == 0)) {
		/* We got an index */
		if (sscanf(string, "%i", &card) != 1)
			return -EINVAL;
		if (card < 0 || card >= SND_MAX_CARDS)
			return -EINVAL;
		err = snd_card_load1(card);
		if (err >= 0)
			return card;
		return err;
	}
	if (string[0] == '/')
		/* We got a device name */
		return snd_card_load2(string);
	/* We got in ID */
	for (card = 0; card < SND_MAX_CARDS; card++) {
#ifdef SUPPORT_ALOAD
		if (! snd_card_load(card))
			continue;
#endif
		if (snd_ctl_hw_open(&handle, NULL, card, 0) < 0)
			continue;
		if (snd_ctl_card_info(handle, &info) < 0) {
			snd_ctl_close(handle);
			continue;
		}
		snd_ctl_close(handle);
		if (!strcmp((const char *)info.id, string))
			return card;
	}
	return -ENODEV;
}

/**
 * \brief Obtain the card name.
 *
 * \param card The index of the card.
 * \param name Result - card name corresponding to card index.
 * \result zero if success, otherwise a negative error code
 *
 * The value returned in name is allocated with strdup and should be
 * freed when no longer used.
 */
int snd_card_get_name(int card, char **name)
{
	snd_ctl_t *handle;
	snd_ctl_card_info_t info;
	int err;
	
	if (name == NULL)
		return -EINVAL;
	if ((err = snd_ctl_hw_open(&handle, NULL, card, 0)) < 0)
		return err;
	if ((err = snd_ctl_card_info(handle, &info)) < 0) {
		snd_ctl_close(handle);
		return err;
	}
	snd_ctl_close(handle);
	*name = strdup((const char *)info.name);
	if (*name == NULL)
		return -ENOMEM;
	return 0;
}

/**
 * \brief Obtain the card long name.
 * \param card Index of the card.
 * \param name Result - card long name corresponding to card index.
 * \result Zero if success, otherwise a negative error code.
 *
 * The value returned in name is allocated with strdup and should be
 * freed when no longer used.
 */
int snd_card_get_longname(int card, char **name)
{
	snd_ctl_t *handle;
	snd_ctl_card_info_t info;
	int err;
	
	if (name == NULL)
		return -EINVAL;
	if ((err = snd_ctl_hw_open(&handle, NULL, card, 0)) < 0)
		return err;
	if ((err = snd_ctl_card_info(handle, &info)) < 0) {
		snd_ctl_close(handle);
		return err;
	}
	snd_ctl_close(handle);
	*name = strdup((const char *)info.longname);
	if (*name == NULL)
		return -ENOMEM;
	return 0;
}
