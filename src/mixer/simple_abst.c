/**
 * \file mixer/simple_abst.c
 * \brief Mixer Simple Element Class Interface - Module Abstraction
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2005
 *
 * Mixer simple element class interface.
 */
/*
 *  Mixer Interface - simple controls - abstraction module
 *  Copyright (c) 2005 by Jaroslav Kysela <perex@suse.cz>
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>
#include "mixer_local.h"
#include "mixer_simple.h"

/**
 * \brief Register mixer simple element class - basic abstraction
 * \param mixer Mixer handle
 * \param options Options container
 * \param classp Pointer to returned mixer simple element class handle (or NULL
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_simple_basic_register(snd_mixer_t *mixer,
				    struct snd_mixer_selem_regopt *options,
				    snd_mixer_class_t **classp)
{
	snd_mixer_class_t *class = calloc(1, sizeof(*class));
	const char *file;
	snd_input_t *input;
	int err;

	if (snd_mixer_class_malloc(&class))
		return -ENOMEM;
	//snd_mixer_class_set_event(class, simple_event);
	snd_mixer_class_set_compare(class, snd_mixer_selem_compare);
	file = getenv("ALSA_MIXER_SIMPLE");
	if (!file)
		file = DATADIR "/alsa/smixer.conf";
	if ((err = snd_input_stdio_open(&input, file, "r")) < 0) {
		SNDERR("unable to open simple mixer configuration file '%s'", file);
		goto __error;
	}
	err = snd_mixer_class_register(class, mixer);
	if (err < 0) {
	      __error:
	      	if (class)
			snd_mixer_class_free(class);
		return err;
	}
	if (classp)
		*classp = class;
	return 0;
}
