/**
 * \file control/setup.c
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2001
 *
 * Routines to setup control primitives from configuration
 */
/*
 *  Control Interface - routines for setup from configuration
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "control_local.h"

typedef struct snd_sctl_elem {
	struct list_head list;
	snd_ctl_elem_value_t *value;
	int lock: 1,
	    preserve: 1;
} snd_sctl_elem_t;

struct _snd_sctl {
	struct list_head elements;
};

static const char *id_str(snd_ctl_elem_id_t *id)
{
        static char str[128];
        assert(id);
        sprintf(str, "%i,%i,%i,%s,%i",
		snd_enum_to_int(snd_ctl_elem_id_get_interface(id)),
		snd_ctl_elem_id_get_device(id),
		snd_ctl_elem_id_get_subdevice(id),
		snd_ctl_elem_id_get_name(id),
		snd_ctl_elem_id_get_index(id));
	return str;
}

static int config_iface(const char *value)
{
	int idx;

	if (isdigit(*value))
		return atoi(value);
	for (idx = 0; idx <= SND_CTL_ELEM_IFACE_LAST; idx++)
		if (strcasecmp(snd_ctl_elem_iface_name(idx), value) == 0)
			return snd_enum_to_int(idx);
	SNDERR("iface '%s' error", value);
	return -1;
}

static void try_replace(snd_sctl_replace_t *replace, const char *id, const char **value)
{
	if (replace == NULL)
		return;
	while (replace->key) {
		if (!strcmp(id, replace->key)) {
			if (!strcmp(*value, replace->old_value)) {
				*value = replace->new_value;
				return;
			}
		}
		replace++;
	}
}

static void add_value(snd_ctl_elem_info_t *info, snd_ctl_elem_value_t *evalue, int idx, const char *value)
{
	switch (snd_ctl_elem_info_get_type(info)) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		{
			int val = 0;
			if (idx < 0 || idx >= 128) {
				SNDERR("index out of range (0-127): %i", idx);
				return;
			}
			if (!strcasecmp(value, "true") || !strcasecmp(value, "on"))
				val = 1;
			else if (isdigit(*value))
				val = atoi(value);
			snd_ctl_elem_value_set_boolean(evalue, idx, val);
		}
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		{
			if (idx < 0 || idx >= 128) {
				SNDERR("index out of range (0-127): %i", idx);
				return;
			}
			snd_ctl_elem_value_set_integer(evalue, idx, strtoul(value, NULL, 10));
		}
		break;
#if 0
	case SND_CTL_ELEM_TYPE_ENUMERATED:
	case SND_CTL_ELEM_TYPE_BYTES:
	case SND_CTL_ELEM_TYPE_IEC958:
		break;
#endif
	default:
		SNDERR("type %i is not supported", snd_ctl_elem_info_get_type(info));
	}
}

/**
 * \brief build and set control primitives from configuration
 * \param setup container for control primitives
 * \return 0 on success otherwise a negative error code
 *
 * Build and set control primitives from configuration.
 */
int snd_sctl_build(snd_ctl_t *handle, snd_sctl_t **r_setup, snd_config_t *config, snd_sctl_replace_t *replace)
{
	snd_sctl_t *setup;
	snd_config_iterator_t i, next;
	int err;

	assert(handle && r_setup && config);
	*r_setup = NULL;
	if ((setup = calloc(1, sizeof(*setup))) == NULL)
		return -ENOMEM;
	INIT_LIST_HEAD(&setup->elements);
	snd_config_for_each(i, next, config) {
		snd_config_t *n = snd_config_iterator_entry(i);
		// const char *id = snd_config_get_id(n);
		if (snd_config_get_type(n) == SND_CONFIG_TYPE_COMPOUND) {
			snd_config_iterator_t i, next;
			snd_sctl_elem_t *elem;
			snd_ctl_elem_id_t *eid;
			snd_ctl_elem_info_t *einfo;
			snd_ctl_elem_value_t *evalue;
			snd_ctl_elem_id_alloca(&eid);
			snd_ctl_elem_info_alloca(&einfo);
			snd_ctl_elem_value_alloca(&evalue);
			elem = calloc(1, sizeof(*elem));
			if (elem == NULL) {
				SNDERR("malloc problem");
				continue;
			}
			snd_config_for_each(i, next, n) {
				snd_config_t *n = snd_config_iterator_entry(i);
				const char *id = snd_config_get_id(n);
				char svalue[16];
				const char *value;
				unsigned long i;
				if (snd_config_get_type(n) == SND_CONFIG_TYPE_INTEGER) {
					snd_config_get_integer(n, &i);
					sprintf(svalue, "%li", i);
					value = svalue;
				} else if (snd_config_get_type(n) == SND_CONFIG_TYPE_STRING) {
					snd_config_get_string(n, &value);
				} else if (snd_config_get_type(n) == SND_CONFIG_TYPE_COMPOUND) {
					value = NULL;
				} else {
					SNDERR("unknown configuration entry type");
					continue;
				}
				try_replace(replace, id, &value);
				if (!strcmp(id, "iface"))
					snd_ctl_elem_id_set_interface(eid, config_iface(value));
				else if (!strcmp(id, "name"))
					snd_ctl_elem_id_set_name(eid, value);
				else if (!strcmp(id, "index"))
					snd_ctl_elem_id_set_index(eid, atoi(value));
				else if (!strcmp(id, "lock"))
					elem->lock = 1;
				else if (!strcmp(id, "preserve"))
					elem->preserve = 1;
				else if (!strcmp(id, "value")) {
					if (elem->value == NULL) {
						if (snd_ctl_elem_value_malloc(&elem->value) < 0) {
							SNDERR("malloc problem");
							continue;
						}
						snd_ctl_elem_info_set_id(einfo, eid);
						if ((err = snd_ctl_elem_info(handle, einfo)) < 0) {
							SNDERR("snd_ctl_elem_info %s : %s", id_str(eid), snd_strerror(err));
							continue;
						}
						snd_ctl_elem_value_set_id(elem->value, eid);
						if ((err = snd_ctl_elem_read(handle, elem->value)) < 0) {
							SNDERR("snd_ctl_elem_read %s : %s", id_str(eid), snd_strerror(err));
							continue;
						}
						snd_ctl_elem_value_copy(evalue, elem->value);
					}
					if (value == NULL) {
						snd_config_iterator_t i, next;
						snd_config_for_each(i, next, n) {
							snd_config_t *n = snd_config_iterator_entry(i);
							char svalue[16];
							const char *value;
							int idx = atoi(snd_config_get_id(n));
							unsigned long i;
							if (snd_config_get_type(n) == SND_CONFIG_TYPE_INTEGER) {
								snd_config_get_integer(n, &i);
								sprintf(svalue, "%li", i);
								value = svalue;
							} else if (snd_config_get_type(n) == SND_CONFIG_TYPE_STRING) {
								snd_config_get_string(n, &value);
							} else {
								SNDERR("unknown configuration entry type");
								continue;
							}
							add_value(einfo, evalue, idx, value);
						}
					} else {
						add_value(einfo, evalue, 0, value);
					}
				}
			}
			list_add_tail(&elem->list, &setup->elements);
			if ((err = snd_ctl_elem_write(handle, evalue)) < 0) {
				SNDERR("snd_ctl_elem_write %s : %s", id_str(eid), snd_strerror(err));
				snd_sctl_free(handle, setup);
				return err;
			}
			if (elem->lock) {
				if ((err = snd_ctl_elem_lock(handle, eid)) < 0) {
					SNDERR("snd_ctl_elem_lock %s : %s", id_str(eid), snd_strerror(err));
					snd_sctl_free(handle, setup);
					return err;
				}
			}
			if (!elem->preserve) {
				snd_ctl_elem_value_free(elem->value);
				elem->value = NULL;
			}
		} else {
			SNDERR("compound type expected here");
		}
	}
	*r_setup = setup;
	return 0;
}

/**
 * \brief restore control primitives from configuration
 * \param setup container for control primitives
 * \return 0 on success otherwise a negative error code
 *
 * Restore control primitives from configuration
 */
int snd_sctl_free(snd_ctl_t *handle, snd_sctl_t *setup)
{
	struct list_head *pos;
	snd_sctl_elem_t *elem;
	int err;

	assert(setup);
      __again:
	list_for_each(pos, &setup->elements) {
		elem = list_entry(pos, snd_sctl_elem_t, list);
		if (elem->preserve && elem->value) {
			if ((err = snd_ctl_elem_write(handle, elem->value)) < 0)
				SNDERR("snd_ctl_elem_write %s : %s", id_str(&elem->value->id), snd_strerror(err));
		}
		if (elem->value) {
			snd_ctl_elem_value_free(elem->value);
			elem->value = NULL;
		}
		list_del(&elem->list);
		free(elem);
		goto __again;
	}
	free(setup);
	return 0;
}
