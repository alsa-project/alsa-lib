/**
 * \file names.c
 * \ingroup Configuration
 * \brief Configuration helper functions - device names
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2005
 *
 * Provide a list of device names for applications.
 *
 * See the \ref conf page for more details.
 */
/*
 *  Configuration helper functions - device names
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

#include <stdarg.h>
#include <limits.h>
#include <sys/stat.h>
#include "local.h"

#ifndef DOC_HIDDEN
#define ALSA_NAMES_ENV	 "ALSA_NAMES_FILE"
#define ALSA_NAMES_PATH1 "/etc/asound.names"
#define ALSA_NAMES_PATH2 "~/.asoundnm"
#endif

static int names_parse(snd_config_t *top, const char *iface, snd_devname_t **list)
{
	snd_config_iterator_t i, next;
	snd_config_iterator_t j, jnext;
	char *name, *comment;
	const char *id;
	snd_devname_t *dn, *last = NULL;
	int err;

	err = snd_config_search(top, iface, &top);
	if (err < 0)
		return err;
	snd_config_for_each(i, next, top) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;
		name = comment = NULL;
		snd_config_for_each(j, jnext, n) {
			snd_config_t *m = snd_config_iterator_entry(j);
			if (snd_config_get_id(m, &id) < 0)
				continue;
			if (strcmp(id, "name") == 0) {
				err = snd_config_get_string(m, (const char **)&name);
				if (err < 0)
					continue;
				name = strdup(name);
				if (name == NULL) {
					err = -ENOMEM;
					goto _err;
				}
				continue;
			}
			if (strcmp(id, "comment") == 0) {
				err = snd_config_get_string(m, (const char **)&comment);
				if (err < 0)
					continue;
				comment = strdup(comment);
				if (name == NULL) {
					err = -ENOMEM;
					goto _err;
				}
				continue;
			}
		}
		if (name != NULL) {
			dn = malloc(sizeof(*dn));
			if (dn == NULL) {
				err = -ENOMEM;
				goto _err;
			}
			dn->name = name;
			dn->comment = comment;
			dn->next = NULL;
			if (last == NULL) {
				*list = dn;
			} else {
				last->next = dn;
			}
			last = dn;
		} else {
			free(comment);
		}
	}
	return 0;

       _err:
	free(name);
	free(comment);
      	return err;
}

/** 
 * \brief Give a list of device names and associated comments for selected interface
 * \param iface a string identifying interface ("pcm", "ctl", "seq", "rawmidi")
 * \param list result - a pointer to list
 * \return A non-negative value if successful, otherwise a negative error code.
 *
 * The global configuration files are specified in the environment variable
 * \c ALSA_NAMES_FILE.
 */
int snd_names_list(const char *iface, snd_devname_t **list)
{
	char *file;
	snd_config_t *top;
	snd_input_t *in;
	int err;
	
	assert(iface);
	assert(list);
	*list = NULL;
	file = getenv(ALSA_NAMES_ENV);
	if (file) {
		file = strdup(file);
		if (file == NULL)
			return -ENOMEM;
	} else {
		err = snd_user_file(ALSA_NAMES_PATH2, &file);
		if (err < 0)
			return err;
		if (access(file, R_OK)) {
			file = strdup(ALSA_NAMES_PATH1);
			if (file == NULL)
				return -ENOMEM;
		}
	}
	top = NULL;
	err = snd_config_top(&top);
	if (err >= 0)
		err = snd_input_stdio_open(&in, file, "r");
	if (err >= 0) {
		err = snd_config_load(top, in);
		snd_input_close(in);
		if (err < 0) {
			SNDERR("%s may be old or corrupted: consider to remove or fix it", file);
		} else {
			err = names_parse(top, iface, list);
			if (err < 0) {
				snd_names_list_free(*list);
				*list = NULL;
			}
		}
	} else {
		SNDERR("cannot access file %s", file);
	}
	if (top)
		snd_config_delete(top);
	return err >= 0 ? 0 : err;
}

/**
 * \brief Release the list of device names
 * \param list the name list to release
 *
 * Releases the list of device names allocated via #snd_names_list().
 */
void snd_names_list_free(snd_devname_t *list)
{
	snd_devname_t *next;
	
	while (list != NULL) {
		next = list->next;
		free(list->name);
		free(list->comment);
		free(list);
		list = next;
	}
}
