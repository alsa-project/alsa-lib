/*
 *  Control Interface - defaults
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

#include <stdlib.h>
#include <errno.h>
#include "asoundlib.h"

static int defaults_card(const char *env)
{
	char *e;
	
	e = getenv(env);
	if (!e)
		return -ENOENT;
	return snd_card_name(e);
}

static int defaults_device(const char *env)
{
	char *e;
	int dev;

	e = getenv(env);
	if (e) {
		dev = atoi(env);
		if (dev >= 0 && dev < 1024 * 1024)
			return dev;
	}
	return 0;
}

int snd_defaults_card(void)
{
	int result;
	unsigned int mask;

	result = defaults_card("ALSA_CARD");
	if (result >= 0)
		return result;
	mask = snd_cards_mask();
	for (result = 0; result < 31; result++)
		if (mask & (1 << result))
			return result;
	return -ENOENT;
}

int snd_defaults_mixer_card(void)
{
	int result;
	
	result = defaults_card("ALSA_MIXER_CARD");
	if (result >= 0)
		return result;
	return snd_defaults_card();
}

int snd_defaults_mixer_device(void)
{
	return defaults_device("ALSA_MIXER_DEVICE");
}

int snd_defaults_pcm_card(void)
{
	int result;
	
	result = defaults_card("ALSA_PCM_CARD");
	if (result >= 0)
		return result;
	return snd_defaults_card();
}

int snd_defaults_pcm_device(void)
{
	return defaults_device("ALSA_PCM_DEVICE");
}

int snd_defaults_rawmidi_card(void)
{
	int result;
	
	result = defaults_card("ALSA_PCM_CARD");
	if (result >= 0)
		return result;
	return snd_defaults_card();
}

int snd_defaults_rawmidi_device(void)
{
	return defaults_device("ALSA_RAWMIDI_DEVICE");
}
