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
 * \brief Expand the dynamic contents
 * \param src Source string
 * \param idchr Identification character
 * \param callback Callback function
 * \param private_data Private data for the given callback function
 * \param dst Destination string
 */
int snd_config_string_replace(const char *src, char idchr,
			      snd_config_string_replace_callback_t *callback,
			      void *private_data,
			      char **dst)
{
	int len = 0, len1, err;
	const char *ptr, *end;
	char *tmp, *what, *fptr, *rdst = NULL;

	assert(src && idchr && dst);
	while (*src != '\0') {
		ptr = strchr(src, idchr);
		end = NULL;
		if (ptr == src && *(ptr + 1) == '(' && (end = strchr(ptr + 2, ')')) != NULL) {
			src = end + 1;
			if (callback == NULL)
				continue;
			len1 = end - (ptr + 2);
			if (len1 == 0)		/* empty */
				continue;
			what = malloc(len1 + 1);
			memcpy(what, ptr + 2, len1);
			what[len1] = '\0';
			fptr = NULL;
			err = callback(what, &fptr, private_data);
			free(what);
			if (err < 0) {
				if (*dst != NULL)
					free(*dst);
				return err;
			}
			if (fptr == NULL)	/* empty */
				continue;
			len1 = strlen(ptr = fptr);
		} else {
			if (ptr == NULL) {
				len1 = strlen(ptr = src);
			} else {
				len1 = ptr - src;
				ptr = src;
			}
			src += len1;
			fptr = NULL;
		}
		tmp = realloc(rdst, len + len1 + 1);
		if (tmp == NULL) {
			if (*dst != NULL)
				free(*dst);
			return -ENOMEM;
		}
		memcpy(tmp + len, ptr, len1);
		tmp[len+=len1] = '\0';
		if (fptr)
			free(fptr);
		rdst = tmp;
	}
	*dst = rdst;
	return 0;
}

/*
 *  Helper functions for the configuration file
 */

int snd_func_getenv(snd_config_t **dst, snd_config_t *src, void *private_data)
{
	snd_config_t *n, *d;
	snd_config_iterator_t i, next;
	char *res, *def = NULL;
	int idx = 0, err;
	
	err = snd_config_search(src, "envname", &n);
	if (err < 0) {
		SNDERR("field envname not found");
		goto __error;
	}
	err = snd_config_evaluate(n, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating envname");
		goto __error;
	}
	err = snd_config_search(src, "default", &d);
	if (err < 0) {
		SNDERR("field default not found");
		goto __error;
	}
	err = snd_config_evaluate(d, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating default");
		goto __error;
	}
	err = snd_config_get_ascii(d, &def);
	if (err < 0) {
		SNDERR("error getting field default");
		goto __error;
	}
      __retry:
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
			goto __retry;
		}
	}
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

int snd_func_igetenv(snd_config_t **dst, snd_config_t *src, void *private_data)
{
	snd_config_t *d;
	const char *str;
	int err;
	long v;
	err = snd_func_getenv(&d, src, private_data);
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
	
	

int snd_func_concat(snd_config_t **dst, snd_config_t *src, void *private_data)
{
	snd_config_t *n;
	snd_config_iterator_t i, next;
	char *res = NULL, *tmp;
	int idx = 0, len = 0, len1, err;
	
	err = snd_config_search(src, "strings", &n);
	if (err < 0) {
		SNDERR("field strings not found");
		goto __error;
	}
	err = snd_config_evaluate(n, private_data, NULL);
	if (err < 0) {
		SNDERR("error evaluating strings");
		goto __error;
	}
      __retry:
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
			goto __retry;
		}
	}
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

int snd_func_datadir(snd_config_t **dst, snd_config_t *src, void *private_data ATTRIBUTE_UNUSED)
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

int snd_func_card_strtype(snd_config_t **dst, snd_config_t *src, void *private_data)
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
	err = snd_config_evaluate(n, private_data, NULL);
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
	err = snd_card_type_enum_to_string(snd_ctl_card_info_get_type(info), &res);
	if (err < 0) {
		SNDERR("snd_card_type_enum_to_string failed for %i", (int)snd_ctl_card_info_get_type(info));
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

int snd_func_card_id(snd_config_t **dst, snd_config_t *src, void *private_data)
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
	err = snd_config_evaluate(n, private_data, NULL);
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

int snd_func_pcm_id(snd_config_t **dst, snd_config_t *src, void *private_data)
{
	snd_config_t *n;
	char *res = NULL;
	snd_ctl_t *ctl = NULL;
	snd_pcm_info_t *info;
	long card, device, subdevice = 0;
	int err;
	
	err = snd_config_search(src, "card", &n);
	if (err < 0) {
		SNDERR("field card not found");
		goto __error;
	}
	err = snd_config_evaluate(n, private_data, NULL);
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
	err = snd_config_evaluate(n, private_data, NULL);
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
		err = snd_config_evaluate(n, private_data, NULL);
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
	res = strdup(snd_pcm_info_get_id(info));
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

int snd_func_refer(snd_config_t **dst, snd_config_t *src, void *private_data)
{
	snd_config_t *n;
	snd_config_t *root = NULL;
	const char *file = NULL, *name = NULL;
	int err;
	
	err = snd_config_search(src, "file", &n);
	if (err >= 0) {
		err = snd_config_evaluate(n, private_data, NULL);
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
		err = snd_config_evaluate(n, private_data, NULL);
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
	if (!file && !name) {
		err = -EINVAL;
		SNDERR("neither file or name are specified");
		goto _end;
	}
	if (!file)
		root = snd_config;
	else {
		snd_input_t *input;
		err = snd_config_top(&root);
		if (err < 0)
			goto _end;
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
	if (!name) {
		if (root == snd_config)
			snd_config_copy(dst, root);
		else
			*dst = root;
		err = 0;
	} else
		err = snd_config_search_definition(root, 0, name, dst);
	if (err >= 0)
		err = snd_config_set_id(*dst, snd_config_get_id(src));
 _end:
	if (root && root != snd_config)
		snd_config_delete(root);
	return err;
}
