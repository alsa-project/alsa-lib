/*
 *  Miscellaneous configuration helper functions
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>,
 *			  Jaroslav Kysela <perex@suse.cz>
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "local.h"
#include "asoundlib.h"

/**
 * \brief Get the boolean value from given ASCII string
 * \param ascii The ASCII string to be parsed
 * \return a positive value when success otherwise a negative error number
 */
int snd_config_get_bool_ascii(const char *ascii)
{
	unsigned int k;
	static struct {
		const char *str;
		int val;
	} b[] = {
		{ "0", 0 },
		{ "1", 1 },
		{ "false", 0 },
		{ "true", 1 },
		{ "no", 0 },
		{ "yes", 1 },
		{ "off", 0 },
		{ "on", 1 },
	};
	for (k = 0; k < sizeof(b) / sizeof(*b); k++) {
		if (strcasecmp(b[k].str, ascii) == 0)
			return b[k].val;
	}
	return -EINVAL;
}

/**
 * \brief Get the boolean value
 * \param conf The configuration node to be parsed
 * \return a positive value when success otherwise a negative error number
 */
int snd_config_get_bool(snd_config_t *conf)
{
	long v;
	const char *str;
	int err;

	err = snd_config_get_integer(conf, &v);
	if (err >= 0) {
		if (v < 0 || v > 1) {
		_invalid_value:
			SNDERR("Invalid value for %s", snd_config_get_id(conf));
			return -EINVAL;
		}
		return v;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", snd_config_get_id(conf));
		return -EINVAL;
	}
	err = snd_config_get_bool_ascii(str);
	if (err < 0)
		goto _invalid_value;
	return err;
}

/**
 * \brief Get the control interface index from given ASCII string
 * \param ascii The ASCII string to be parsed
 * \return a positive value when success otherwise a negative error number
 */ 
int snd_config_get_ctl_iface_ascii(const char *ascii)
{
	long v;
	snd_ctl_elem_iface_t idx;
	if (isdigit(ascii[0])) {
		if (safe_strtol(ascii, &v) >= 0) {
			if (v < 0 || v > SND_CTL_ELEM_IFACE_LAST)
				return -EINVAL;
			return v;
		}
	}
	for (idx = 0; idx <= SND_CTL_ELEM_IFACE_LAST; idx++) {
		if (strcasecmp(snd_ctl_elem_iface_name(idx), ascii) == 0)
			return idx;
	}
	return -EINVAL;
}

/**
 * \brief Get the control interface index
 * \param conf The configuration node to be parsed
 * \return a positive value when success otherwise a negative error number
 */ 
int snd_config_get_ctl_iface(snd_config_t *conf)
{
	long v;
	const char *str;
	int err;
	err = snd_config_get_integer(conf, &v);
	if (err >= 0) {
		if (v < 0 || v > SND_CTL_ELEM_IFACE_LAST) {
		_invalid_value:
			SNDERR("Invalid value for %s", snd_config_get_id(conf));
			return -EINVAL;
		}
		return v;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", snd_config_get_id(conf));
		return -EINVAL;
	}
	err = snd_config_get_ctl_iface_ascii(str);
	if (err < 0)
		goto _invalid_value;
	return err;
}

static int _snd_config_redirect_load_replace(const char *what, char **dst, void *private_data ATTRIBUTE_UNUSED)
{
	enum {
		CARD_ID,
		PCM_ID,
		RAWMIDI_ID
	} id;
	int len;

	if (!strcmp(what, "datadir")) {
		*dst = strdup(DATADIR "/alsa");
		return *dst == NULL ? -ENOMEM : 0;
	}
	if (!strncmp(what, "card_id:", len = 8))
		id = CARD_ID;
	else if (!strncmp(what, "pcm_id:", len = 7))
		id = PCM_ID;
	else if (!strncmp(what, "rawmidi_id:", len = 11))
		id = RAWMIDI_ID;
	else
		return 0;
	{
		snd_ctl_t *ctl;
		int err;
		char name[12];
		const char *str = NULL;
		char *fstr = NULL;
		sprintf(name, "hw:%d", atoi(what + len));
		err = snd_ctl_open(&ctl, name, 0);
		if (err < 0)
			return err;
		switch (id) {
		case CARD_ID:
			{
				snd_ctl_card_info_t *info;
				snd_ctl_card_info_alloca(&info);
				err = snd_ctl_card_info(ctl, info);
				if (err < 0)
					return err;
				err = snd_card_type_enum_to_string(snd_ctl_card_info_get_type(info), &fstr);
			}
			break;
		case PCM_ID:
			{
				char *ptr = strchr(what + len, ',');
				int dev = atoi(what + len);
				int subdev = ptr ? atoi(ptr + 1) : -1;
				snd_pcm_info_t *info;
				snd_pcm_info_alloca(&info);
				snd_pcm_info_set_device(info, dev);
				snd_pcm_info_set_subdevice(info, subdev);
				err = snd_ctl_pcm_info(ctl, info);
				if (err < 0)
					return err;
				str = snd_pcm_info_get_id(info);
			}
			break;
		case RAWMIDI_ID:
			{
				char *ptr = strchr(what + len, ',');
				int dev = atoi(what + len);
				int subdev = ptr ? atoi(ptr + 1) : -1;
				snd_rawmidi_info_t *info;
				snd_rawmidi_info_alloca(&info);
				snd_rawmidi_info_set_device(info, dev);
				snd_rawmidi_info_set_subdevice(info, subdev);
				err = snd_ctl_rawmidi_info(ctl, info);
				if (err < 0)
					return err;
				str = snd_rawmidi_info_get_id(info);
			}
			break;
		}
		if (err < 0)
			return err;
		snd_ctl_close(ctl);
		*dst = fstr ? fstr : (str ? strdup(str) : NULL);
		if (*dst == NULL)
			return 0;
		return 0;
	}
	return 0;	/* empty */
}

/**
 * \brief Redirect the configuration block to an another
 * \param root the root of all configurations
 * \param config redirect configuration
 * \param name the identifier of new configuration block
 * \param dst_config new configuration block
 * \param dst_dynamic new configuration block is dynamically allocated
 */
int snd_config_redirect_load(snd_config_t *root,
			     snd_config_t *config,
			     char **name,
			     snd_config_t **dst_config,
			     int *dst_dynamic)
{
	int err, dynamic;
	snd_config_t *result, *c;
	char *rname;

	assert(config);
	assert(name);
	assert(dst_config);
	assert(dst_dynamic);
	if (snd_config_get_type(config) == SND_CONFIG_TYPE_STRING) {
		const char *str;
		snd_config_get_string(config, &str);
		*name = strdup(str);
		if (*name == NULL)
			return -ENOMEM;
		*dst_config = root;
		*dst_dynamic = 0;
		return 0;
	}
	if (snd_config_get_type(config) != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;
	result = root;
	dynamic = 0;
	rname = NULL;
	if (snd_config_search(config, "filename", &c) >= 0) {
		snd_config_t *rconfig;
		char *filename;
		snd_input_t *input;
		err = snd_config_copy(&rconfig, root);
		if (err < 0)
			return err;
		if (snd_config_get_type(c) == SND_CONFIG_TYPE_STRING) {
			snd_config_get_string(c, (const char **)&filename);
			if ((err = snd_config_string_replace(filename, '&', _snd_config_redirect_load_replace, NULL, &filename)) < 0)
				goto __filename_error;
			if (filename == NULL)
				goto __filename_error_einval;
		} else {
		      __filename_error_einval:
			err = -EINVAL;
		      __filename_error:
			snd_config_delete(rconfig);
			return err;
		}
		err = snd_input_stdio_open(&input, filename, "r");
		if (err < 0) {
			SNDERR("Unable to open filename %s: %s", filename, snd_strerror(err));
			goto __filename_error;
		}
		err = snd_config_load(rconfig, input);
		if (err < 0) {
			snd_input_close(input);
			goto __filename_error;
			return err;
		}
		snd_input_close(input);
		free(filename);
		result = rconfig;
		dynamic = 1;
	}
	if (snd_config_search(config, "name", &c) >= 0) {
		const char *ptr;
		if ((err = snd_config_get_string(c, &ptr)) < 0)
			goto __error;
		if ((err = snd_config_string_replace(ptr, '&', _snd_config_redirect_load_replace, NULL, &rname)) < 0)
			goto __error;
	}
	if (rname == NULL) {
		err = -EINVAL;
		goto __error;
	}
	*dst_config = result;
	*dst_dynamic = dynamic;
	*name = rname;
	return 0;
      __error:
      	if (rname)
      		free(rname);
      	if (dynamic)
      		snd_config_delete(result);
      	return err;
}
