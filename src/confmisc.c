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

/**
 * \brief Refer the configuration block to another
 * \param dst new configuration block (if *dst != root -> dst needs to be deleted)
 * \param name the identifier of new configuration block
 * \param root the root of all configurations
 * \param config redirect configuration
 */
int snd_config_refer_load(snd_config_t **dst,
			  char **name,
			  snd_config_t *root,
			  snd_config_t *config)
{
	int err;
	snd_config_t *result, *c;
	char *rname;

	assert(dst);
	assert(name);
	assert(root);
	assert(config);
	if (snd_config_get_type(config) == SND_CONFIG_TYPE_STRING) {
		const char *str;
		snd_config_get_string(config, &str);
		*name = strdup(str);
		if (*name == NULL)
			return -ENOMEM;
		*dst = root;
		return 0;
	}
	if (snd_config_get_type(config) != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;
	result = root;
	rname = NULL;
	if (snd_config_search(config, "file", &c) >= 0) {
		snd_config_t *rconfig;
		const char *filename;
		snd_input_t *input;
		err = snd_config_copy(&rconfig, root);
		if (err < 0)
			return err;
		if (snd_config_get_type(c) == SND_CONFIG_TYPE_STRING) {
			snd_config_get_string(c, &filename);
		} else {
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
		}
		snd_input_close(input);
		result = rconfig;
	}
	if (snd_config_search(config, "name", &c) >= 0) {
		const char *ptr;
		if ((err = snd_config_get_string(c, &ptr)) < 0)
			goto __error;
		rname = strdup(ptr);
		if (rname == NULL) {
			err = -ENOMEM;
			goto __error;
		}
	}
	if (rname == NULL) {
		err = -EINVAL;
		goto __error;
	}
	*dst = result;
	*name = rname;
	return 0;
      __error:
      	if (rname)
      		free(rname);
      	if (result != root)
      		snd_config_delete(result);
      	return err;
}

/*
 *  Helper functions for the configuration file
 */

int snd_func_getenv(snd_config_t **dst, snd_config_t *root, snd_config_t *src, void *private_data)
{
	snd_config_t *n, *d;
	snd_config_iterator_t i, next;
	char *res, *def = NULL;
	int idx = 0, err, hit;
	
	err = snd_config_search(src, "vars", &n);
	if (err < 0) {
		SNDERR("field vars not found");
		goto __error;
	}
	err = snd_config_evaluate(n, root, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating vars");
		goto __error;
	}
	err = snd_config_search(src, "default", &d);
	if (err < 0) {
		SNDERR("field default not found");
		goto __error;
	}
	err = snd_config_evaluate(d, root, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating default");
		goto __error;
	}
	err = snd_config_get_ascii(d, &def);
	if (err < 0) {
		SNDERR("error getting field default");
		goto __error;
	}
	do {
		hit = 0;
		snd_config_for_each(i, next, n) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			const char *ptr, *env;
			long i;
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_STRING) {
				SNDERR("field %s is not a string", id);
				err = -EINVAL;
				goto __error;
			}
			err = safe_strtol(id, &i);
			if (err < 0) {
				SNDERR("id of field %s is not an integer", id);
				err = -EINVAL;
				goto __error;
			}
			if (i == idx) {
				idx++;
				snd_config_get_string(n, &ptr);
				env = getenv(ptr);
				if (env != NULL && *env != '\0') {
					res = strdup(env);
					goto __ok;
				}
				hit = 1;
			}
		}
	} while (hit);
	res = def;
	def = NULL;
      __ok:
	err = res == NULL ? -ENOMEM : 0;
	if (err >= 0) {
		err = snd_config_make_string(dst, snd_config_get_id(src));
		if (err >= 0)
			snd_config_set_string(*dst, res);
		free(res);
	}
      __error:
      	if (def)
      		free(def);
	return err;
}

int snd_func_igetenv(snd_config_t **dst, snd_config_t *root, snd_config_t *src, void *private_data)
{
	snd_config_t *d;
	const char *str;
	int err;
	long v;
	err = snd_func_getenv(&d, root, src, private_data);
	if (err < 0)
		return err;
	err = snd_config_get_string(d, &str);
	if (err < 0)
		goto _end;
	err = safe_strtol(str, &v);
	if (err < 0)
		goto _end;
	err = snd_config_make_integer(dst, snd_config_get_id(src));
	if (err < 0)
		goto _end;
	snd_config_set_integer(*dst, v);
	err = 0;

 _end:
	return err;
}
	
	
int snd_func_concat(snd_config_t **dst, snd_config_t *root, snd_config_t *src, void *private_data)
{
	snd_config_t *n;
	snd_config_iterator_t i, next;
	char *res = NULL, *tmp;
	int idx = 0, len = 0, len1, err, hit;
	
	err = snd_config_search(src, "strings", &n);
	if (err < 0) {
		SNDERR("field strings not found");
		goto __error;
	}
	err = snd_config_evaluate(n, root, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating strings");
		goto __error;
	}
	do {
		hit = 0;
		snd_config_for_each(i, next, n) {
			snd_config_t *n = snd_config_iterator_entry(i);
			char *ptr;
			const char *id = snd_config_get_id(n);
			long i;
			err = safe_strtol(id, &i);
			if (err < 0) {
				SNDERR("id of field %s is not an integer", id);
				err = -EINVAL;
				goto __error;
			}
			if (i == idx) {
				idx++;
				snd_config_get_ascii(n, &ptr);
					len1 = strlen(ptr);
				tmp = realloc(res, len + len1 + 1);
				if (tmp == NULL) {
					free(ptr);
					if (res)
						free(res);
					err = -ENOMEM;
					goto __error;
				}
				memcpy(tmp + len, ptr, len1);
				free(ptr);
				len += len1;
				tmp[len] = '\0';
				res = tmp;
				hit = 1;
			}
		}
	} while (hit);
	if (res == NULL) {
		SNDERR("empty string is not accepted");
		err = -EINVAL;
		goto __error;
	}
	err = snd_config_make_string(dst, snd_config_get_id(src));
	if (err >= 0)
		snd_config_set_string(*dst, res);
	free(res);
      __error:
	return err;
}

int snd_func_datadir(snd_config_t **dst, snd_config_t *root ATTRIBUTE_UNUSED,
		     snd_config_t *src, void *private_data ATTRIBUTE_UNUSED)
{
	int err = snd_config_make_string(dst, snd_config_get_id(src));
	if (err >= 0)
		err = snd_config_set_string(*dst, DATADIR "/alsa");
	return 0;
}

static int open_ctl(long card, snd_ctl_t **ctl)
{
	char name[16];
	snprintf(name, sizeof(name), "hw:%li", card);
	name[sizeof(name)-1] = '\0';
	return snd_ctl_open(ctl, name, 0);
}

#if 0
static int string_from_integer(char **dst, long v)
{
	char str[32];
	char *res;
	sprintf(str, "%li", v);
	res = strdup(str);
	if (res == NULL)
		return -ENOMEM;
	*dst = res;
	return 0;
}
#endif

int snd_func_private_string(snd_config_t **dst, snd_config_t *root ATTRIBUTE_UNUSED, snd_config_t *src, void *private_data)
{
	int err;

	if (private_data == NULL)
		return snd_config_copy(dst, src);
	err = snd_config_make_string(dst, snd_config_get_id(src));
	if (err >= 0)
		err = snd_config_set_string(*dst, (char *)private_data);
	return err;
}

int snd_determine_driver(int card, char **driver)
{
	snd_ctl_t *ctl = NULL;
	snd_ctl_card_info_t *info;
	char *res = NULL;
	int err;

	assert(card >= 0 && card <= 32);
	err = open_ctl(card, &ctl);
	if (err < 0) {
		SNDERR("could not open control for card %li", card);
		goto __error;
	}
	snd_ctl_card_info_alloca(&info);
	err = snd_ctl_card_info(ctl, info);
	if (err < 0) {
		SNDERR("snd_ctl_card_info error: %s", snd_strerror(err));
		goto __error;
	}
	res = strdup(snd_ctl_card_info_get_driver(info));
	if (res == NULL)
		err = -ENOMEM;
	else {
		*driver = res;
		err = 0;
	}
      __error:
	if (ctl)
		snd_ctl_close(ctl);
	return err;
}

int snd_func_private_card_strtype(snd_config_t **dst, snd_config_t *root ATTRIBUTE_UNUSED, snd_config_t *src, void *private_data)
{
	char *driver;
	int err;

	if ((err = snd_determine_driver((long)private_data, &driver)) < 0)
		return err;
	err = snd_config_make_string(dst, snd_config_get_id(src));
	if (err >= 0)
		err = snd_config_set_string(*dst, driver);
	free(driver);
	return err;
}

int snd_func_card_strtype(snd_config_t **dst, snd_config_t *root, snd_config_t *src, void *private_data)
{
	snd_config_t *n;
	char *str;
	long v;
	int err;
	
	err = snd_config_search(src, "card", &n);
	if (err < 0) {
		SNDERR("field card not found");
		return err;
	}
	err = snd_config_evaluate(n, root, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating card");
		return err;
	}
	err = snd_config_get_ascii(n, &str);
	if (err < 0) {
		SNDERR("field card is not an integer or a string");
		return err;
	}
	v = snd_card_get_index(str);
	if (v < 0) {
		SNDERR("cannot find card '%s'", str);
		free(str);
		return v;
	}
	free(str);
	return snd_func_private_card_strtype(dst, root, src, (void *)v);
}

int snd_func_card_id(snd_config_t **dst, snd_config_t *root, snd_config_t *src, void *private_data)
{
	snd_config_t *n;
	char *res = NULL;
	snd_ctl_t *ctl = NULL;
	snd_ctl_card_info_t *info;
	long v;
	int err;
	
	err = snd_config_search(src, "card", &n);
	if (err < 0) {
		SNDERR("field card not found");
		goto __error;
	}
	err = snd_config_evaluate(n, root, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating card");
		goto __error;
	}
	err = snd_config_get_integer(n, &v);
	if (err < 0) {
		SNDERR("field card is not an integer");
		goto __error;
	}
	err = open_ctl(v, &ctl);
	if (err < 0) {
		SNDERR("could not open control for card %li", v);
		goto __error;
	}
	snd_ctl_card_info_alloca(&info);
	err = snd_ctl_card_info(ctl, info);
	if (err < 0) {
		SNDERR("snd_ctl_card_info error: %s", snd_strerror(err));
		goto __error;
	}
	res = strdup(snd_ctl_card_info_get_id(info));
	if (res == NULL) {
		err = -ENOMEM;
		goto __error;
	}
	err = snd_config_make_string(dst, snd_config_get_id(src));
	if (err >= 0)
		err = snd_config_set_string(*dst, res);
	free(res);
      __error:
      	if (ctl)
      		snd_ctl_close(ctl);
	return err;
}

int snd_func_pcm_id(snd_config_t **dst, snd_config_t *root, snd_config_t *src, void *private_data)
{
	snd_config_t *n;
	snd_ctl_t *ctl = NULL;
	snd_pcm_info_t *info;
	long card, device, subdevice = 0;
	int err;
	
	err = snd_config_search(src, "card", &n);
	if (err < 0) {
		SNDERR("field card not found");
		goto __error;
	}
	err = snd_config_evaluate(n, root, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating card");
		goto __error;
	}
	err = snd_config_get_integer(n, &card);
	if (err < 0) {
		SNDERR("field card is not an integer");
		goto __error;
	}
	err = snd_config_search(src, "device", &n);
	if (err < 0) {
		SNDERR("field device not found");
		goto __error;
	}
	err = snd_config_evaluate(n, root, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating device");
		goto __error;
	}
	err = snd_config_get_integer(n, &device);
	if (err < 0) {
		SNDERR("field device is not an integer");
		goto __error;
	}
	if (snd_config_search(src, "subdevice", &n) >= 0) {
		err = snd_config_evaluate(n, root, private_data, NULL);
		if (err < 0) {
			SNDERR("error evaluating subdevice");
			goto __error;
		}
		err = snd_config_get_integer(n, &subdevice);
		if (err < 0) {
			SNDERR("field subdevice is not an integer");
			goto __error;
		}
	}
	err = open_ctl(card, &ctl);
	if (err < 0) {
		SNDERR("could not open control for card %li", card);
		goto __error;
	}
	snd_pcm_info_alloca(&info);
	snd_pcm_info_set_device(info, device);
	snd_pcm_info_set_subdevice(info, subdevice);
	err = snd_ctl_pcm_info(ctl, info);
	if (err < 0) {
		SNDERR("snd_ctl_pcm_info error: %s", snd_strerror(err));
		goto __error;
	}
	err = snd_config_make_string(dst, snd_config_get_id(src));
	if (err >= 0)
		err = snd_config_set_string(*dst, snd_pcm_info_get_id(info));
      __error:
      	if (ctl)
      		snd_ctl_close(ctl);
	return err;
}

int snd_func_private_pcm_subdevice(snd_config_t **dst, snd_config_t *root ATTRIBUTE_UNUSED, snd_config_t *src, void *private_data)
{
	char *res = NULL;
	snd_pcm_info_t *info;
	int err;

	if (private_data == NULL)
		return snd_config_copy(dst, src);
	snd_pcm_info_alloca(&info);
	err = snd_pcm_info((snd_pcm_t *)private_data, info);
	if (err < 0) {
		SNDERR("snd_ctl_pcm_info error: %s", snd_strerror(err));
		return err;
	}
	res = strdup(snd_pcm_info_get_id(info));
	if (res == NULL)
		return -ENOMEM;
	err = snd_config_make_integer(dst, snd_config_get_id(src));
	if (err >= 0)
		err = snd_config_set_integer(*dst, snd_pcm_info_get_subdevice(info));
	free(res);
	return err;
}

int snd_func_refer(snd_config_t **dst, snd_config_t *root, snd_config_t *src, void *private_data)
{
	snd_config_t *n;
	const char *file = NULL, *name = NULL;
	int err;
	
	err = snd_config_search(src, "file", &n);
	if (err >= 0) {
		err = snd_config_evaluate(n, root, private_data, NULL);
		if (err < 0) {
			SNDERR("error evaluating file");
			goto _end;
		}
		err = snd_config_get_string(n, &file);
		if (err < 0) {
			SNDERR("file is not a string");
			goto _end;
		}
	}
	err = snd_config_search(src, "name", &n);
	if (err >= 0) {
		err = snd_config_evaluate(n, root, private_data, NULL);
		if (err < 0) {
			SNDERR("error evaluating name");
			goto _end;
		}
		err = snd_config_get_string(n, &name);
		if (err < 0) {
			SNDERR("name is not a string");
			goto _end;
		}
	}
	if (!name) {
		err = -EINVAL;
		SNDERR("name is not specified");
		goto _end;
	}
	if (file) {
		snd_input_t *input;
		err = snd_input_stdio_open(&input, file, "r");
		if (err < 0) {
			SNDERR("Unable to open file %s: %s", file, snd_strerror(err));
			goto _end;
		}
		err = snd_config_load(root, input);
		if (err < 0) {
			snd_input_close(input);
			goto _end;
		}
	}
	err = snd_config_search_definition(root, NULL, name, dst);
	if (err >= 0)
		err = snd_config_set_id(*dst, snd_config_get_id(src));
	else
		SNDERR("Unable to find definition '%s'", name);
 _end:
	return err;
}
