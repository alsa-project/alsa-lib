/*
 *  Control Interface - card ID conversions
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
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "control_local.h"

#define ALSA_CARDS_FILE DATADIR "/alsa/cards.conf"

static int build_config(snd_config_t **r_conf)
{
	int err;
	snd_input_t *in;
	snd_config_t *conf, *file;
	const char *filename = ALSA_CARDS_FILE;

	assert(r_conf);
	*r_conf = NULL;
	if ((err = snd_config_update()) < 0)
		return err;
	if ((err = snd_config_search(snd_config, "cards_file", &file)) >= 0) {
		if ((err = snd_config_get_string(file, &filename)) < 0) {
			SNDERR("cards_file definition must be string");
			filename = ALSA_CARDS_FILE;
		}
	}
	if ((err = snd_input_stdio_open(&in, filename, "r")) < 0) {
		SNDERR("unable to open configuration file '%s'", filename);
		return err;
	}
	if ((err = snd_config_top(&conf)) < 0) {
		SNDERR("config_top");
		snd_input_close(in);
		return err;
	}
	if ((err = snd_config_load(conf, in)) < 0) {
		SNDERR("config load error");
		snd_config_delete(conf);
		snd_input_close(in);
		return err;
	}
	snd_input_close(in);
	*r_conf = conf;
	return 0;
}

int snd_card_type_string_to_enum(const char *strid, snd_card_type_t *enumid)
{
	int err;
	snd_config_t *conf = NULL, *card;
	snd_config_iterator_t i, next;

	assert(enumid);
	*enumid = SND_CARD_TYPE_GUS_CLASSIC;
	if ((err = build_config(&conf)) < 0)
		return err;
	if ((err = snd_config_search(conf, "card", &card)) < 0) {
		SNDERR("unable to find card definitions");
		snd_config_delete(conf);
		return err;
	}
	if (snd_config_get_type(card) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("compound type expected");
		snd_config_delete(conf);
		return err;
	}
	snd_config_for_each(i, next, card) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		unsigned long i;
		if (snd_config_get_type(n) != SND_CONFIG_TYPE_INTEGER) {
			SNDERR("entry '%s' is invalid", id);
			continue;
		}
		if ((err = snd_config_get_integer(n, &i)) < 0) {
			SNDERR("entry '%s' is invalid", id);
			continue;
		}
		if (!strcmp(id, strid)) {
			*enumid = i;
			return 0;
		}
	}
	snd_config_delete(conf);
	return -ENOENT;
}

int snd_card_type_enum_to_string(snd_card_type_t enumid, char **strid)
{
	int err;
	snd_config_t *conf = NULL, *card;
	snd_config_iterator_t i, next;

	assert(strid);
	*strid = NULL;
	if ((err = build_config(&conf)) < 0)
		return err;
	if ((err = snd_config_search(conf, "card", &card)) < 0) {
		SNDERR("unable to find card definitions");
		snd_config_delete(conf);
		return err;
	}
	if (snd_config_get_type(card) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("compound type expected");
		snd_config_delete(conf);
		return err;
	}
	snd_config_for_each(i, next, card) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		unsigned long i;
		if (snd_config_get_type(n) != SND_CONFIG_TYPE_INTEGER) {
			SNDERR("entry '%s' is invalid", id);
			continue;
		}
		if ((err = snd_config_get_integer(n, &i)) < 0) {
			SNDERR("entry '%s' is invalid", id);
			continue;
		}
		if ((unsigned long)enumid == i) {
			*strid = id ? strdup(id) : NULL;
			snd_config_delete(conf);
			return 0;
		}
	}
	snd_config_delete(conf);
	return -ENOENT;
}
