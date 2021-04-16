/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Support for the verb/device/modifier core logic and API,
 *  command line tool and file parser was kindly sponsored by
 *  Texas Instruments Inc.
 *  Support for multiple active modifiers and devices,
 *  transition sequences, multiple client access and user defined use
 *  cases was kindly sponsored by Wolfson Microelectronics PLC.
 *
 *  Copyright (C) 2019 Red Hat Inc.
 *  Authors: Jaroslav Kysela <perex@perex.cz>
 */

#include "ucm_local.h"
#include <stdbool.h>
#include <sys/stat.h>
#include <limits.h>
#include <regex.h>

static char *rval_open_name(snd_use_case_mgr_t *uc_mgr)
{
	const char *name;
	if (uc_mgr->conf_format < 3)
		return NULL;
	name = uc_mgr->card_name;
	if (name) {
		if (strncmp(name, "strict:", 7) == 0)
			name += 7;
		return strdup(name);
	}
	return NULL;
}

static char *rval_conf_libdir(snd_use_case_mgr_t *uc_mgr)
{
	if (uc_mgr->conf_format < 4)
		return NULL;
	return strdup(snd_config_topdir());
}

static char *rval_conf_topdir(snd_use_case_mgr_t *uc_mgr)
{
	const char *dir;

	if (uc_mgr->conf_format < 3)
		return NULL;
	dir = uc_mgr_config_dir(uc_mgr->conf_format);
	if (dir && dir[0])
		return strdup(dir);
	return NULL;
}

static char *rval_conf_dir(snd_use_case_mgr_t *uc_mgr)
{
	if (uc_mgr->conf_format < 3)
		return NULL;
	if (uc_mgr->conf_dir_name && uc_mgr->conf_dir_name[0])
		return strdup(uc_mgr->conf_dir_name);
	return NULL;
}

static char *rval_conf_name(snd_use_case_mgr_t *uc_mgr)
{
	if (uc_mgr->conf_file_name && uc_mgr->conf_file_name[0])
		return strdup(uc_mgr->conf_file_name);
	return NULL;
}

static char *get_card_number(struct ctl_list *ctl_list)
{
	char num[16];

	if (ctl_list == NULL)
		return strdup("");
	snprintf(num, sizeof(num), "%i", snd_ctl_card_info_get_card(ctl_list->ctl_info));
	return strdup(num);
}

static char *rval_card_number(snd_use_case_mgr_t *uc_mgr)
{
	if (uc_mgr->conf_format < 3)
		return NULL;
	return get_card_number(uc_mgr_get_master_ctl(uc_mgr));
}

static char *rval_card_id(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_master_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_id(ctl_list->ctl_info));
}

static char *rval_card_driver(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_master_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_driver(ctl_list->ctl_info));
}

static char *rval_card_name(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_master_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_name(ctl_list->ctl_info));
}

static char *rval_card_longname(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_master_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_longname(ctl_list->ctl_info));
}

static char *rval_card_components(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_master_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_components(ctl_list->ctl_info));
}

static struct ctl_list *get_ctl_list_by_name(snd_use_case_mgr_t *uc_mgr, const char *id)
{
	char *name, *index;
	long idx = 0;

	name = alloca(strlen(id) + 1);
	strcpy(name, id);
	index = strchr(name, '#');
	if (index) {
		*index = '\0';
		if (safe_strtol(index + 1, &idx))
			return NULL;
	}
	return uc_mgr_get_ctl_by_name(uc_mgr, name, idx);
}

static char *rval_card_number_by_name(snd_use_case_mgr_t *uc_mgr, const char *id)
{
	if (uc_mgr->conf_format < 3) {
		uc_error("CardNumberByName substitution is supported in v3+ syntax");
		return NULL;
	}

	uc_error("${CardNumberByName} substitution is obsolete - use ${find-card}!");

	return get_card_number(get_ctl_list_by_name(uc_mgr, id));
}

static char *rval_card_id_by_name(snd_use_case_mgr_t *uc_mgr, const char *id)
{
	struct ctl_list *ctl_list;

	if (uc_mgr->conf_format < 3) {
		uc_error("CardIdByName substitution is supported in v3+ syntax");
		return NULL;
	}

	uc_error("${CardIdByName} substitution is obsolete - use ${find-card}!");

	ctl_list = get_ctl_list_by_name(uc_mgr, id);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_id(ctl_list->ctl_info));
}

typedef struct lookup_iterate *(*lookup_iter_fcn_t)
			(snd_use_case_mgr_t *uc_mgr, struct lookup_iterate *iter);
typedef const char *(*lookup_fcn_t)(void *);

struct lookup_fcn {
	char *name;
	const char *(*fcn)(void *opaque);
};

struct lookup_iterate {
	int (*init)(snd_use_case_mgr_t *uc_mgr, struct lookup_iterate *iter,
		    snd_config_t *config);
	void (*done)(struct lookup_iterate *iter);
	lookup_iter_fcn_t first;
	lookup_iter_fcn_t next;
	char *(*retfcn)(struct lookup_iterate *iter, snd_config_t *config);
	struct lookup_fcn *fcns;
	lookup_fcn_t fcn;
	struct ctl_list *ctl_list;
	void *info;
};

static snd_config_t *parse_lookup_query(const char *query)
{
	snd_input_t *input;
	snd_config_t *config;
	int err;

	err = snd_input_buffer_open(&input, query, strlen(query));
	if (err < 0) {
		uc_error("unable to create memory input buffer");
		return NULL;
	}
	snd_config_top(&config);
	err = snd_config_load(config, input);
	snd_input_close(input);
	if (err < 0) {
		snd_config_delete(config);
		uc_error("wrong arguments '%s'", query);
		return NULL;
	}
	return config;
}

static char *rval_lookup_main(snd_use_case_mgr_t *uc_mgr,
			      const char *query,
			      struct lookup_iterate *iter)
{
	snd_config_t *config, *d;
	struct lookup_fcn *fcn;
	struct lookup_iterate *curr;
	const char *s;
	char *result;
	regmatch_t match[1];
	regex_t re;
	int err;

	if (uc_mgr->conf_format < 4) {
		uc_error("Lookups are supported in v4+ syntax");
		return NULL;
	}

	config = parse_lookup_query(query);
	if (config == NULL)
		return NULL;
	if (iter->init && iter->init(uc_mgr, iter, config))
		goto null;
	if (snd_config_search(config, "field", &d)) {
		uc_error("Lookups require field!");
		goto null;
	}
	if (snd_config_get_string(d, &s))
		goto null;
	for (fcn = iter->fcns ; fcn; fcn++) {
		if (strcasecmp(fcn->name, s) == 0) {
			iter->fcn = fcn->fcn;
			break;
		}
	}
	if (iter->fcn == NULL) {
		uc_error("Unknown field value '%s'", s);
		goto null;
	}
	if (snd_config_search(config, "regex", &d)) {
		uc_error("Lookups require regex!");
		goto null;
	}
	if (snd_config_get_string(d, &s))
		goto null;
	err = regcomp(&re, s, REG_EXTENDED | REG_ICASE);
	if (err) {
		uc_error("Regex '%s' compilation failed (code %d)", s, err);
		goto null;
	}

	result = NULL;
	for (curr = iter->first(uc_mgr, iter); curr; curr = iter->next(uc_mgr, iter)) {
		s = curr->fcn(iter->info);
		if (s == NULL)
			continue;
		if (regexec(&re, s, ARRAY_SIZE(match), match, 0) == 0) {
			result = curr->retfcn(iter, config);
			break;
		}
	}
	regfree(&re);
fin:
	snd_config_delete(config);
	if (iter->done)
		iter->done(iter);
	return result;
null:
	result = NULL;
	goto fin;
}

static struct lookup_iterate *rval_card_lookup1(snd_use_case_mgr_t *uc_mgr,
						struct lookup_iterate *iter,
						int card)
{
	if (snd_card_next(&card) < 0 || card < 0)
		return NULL;
	iter->ctl_list = uc_mgr_get_ctl_by_card(uc_mgr, card);
	if (iter->ctl_list == NULL)
		return NULL;
	iter->info = iter->ctl_list->ctl_info;
	return iter;
}

static struct lookup_iterate *rval_card_lookup_first(snd_use_case_mgr_t *uc_mgr,
						     struct lookup_iterate *iter)
{
	return rval_card_lookup1(uc_mgr, iter, -1);
}

static struct lookup_iterate *rval_card_lookup_next(snd_use_case_mgr_t *uc_mgr,
						    struct lookup_iterate *iter)
{
	return rval_card_lookup1(uc_mgr, iter, snd_ctl_card_info_get_card(iter->info));
}

static char *rval_card_lookup_return(struct lookup_iterate *iter, snd_config_t *config)
{
	snd_config_t *d;
	const char *s;

	if (snd_config_search(config, "return", &d))
		return strdup(snd_ctl_card_info_get_id(iter->info));
	else if (snd_config_get_string(d, &s))
		return NULL;
	else if (strcasecmp(s, "id") == 0)
		return strdup(snd_ctl_card_info_get_id(iter->info));
	else if (strcasecmp(s, "number") == 0) {
		char num[16];
		snprintf(num, sizeof(num), "%d", snd_ctl_card_info_get_card(iter->info));
		return strdup(num);
	} else {
		uc_error("Unknown return type '%s'", s);
		return NULL;
	}
}

static char *rval_card_lookup(snd_use_case_mgr_t *uc_mgr, const char *query)
{
	static struct lookup_fcn fcns[] = {
		{ .name = "id", (lookup_fcn_t)snd_ctl_card_info_get_id },
		{ .name = "driver", (lookup_fcn_t)snd_ctl_card_info_get_driver },
		{ .name = "name", (lookup_fcn_t)snd_ctl_card_info_get_name },
		{ .name = "longname", (lookup_fcn_t)snd_ctl_card_info_get_longname },
		{ .name = "mixername", (lookup_fcn_t)snd_ctl_card_info_get_mixername },
		{ .name = "components", (lookup_fcn_t)snd_ctl_card_info_get_components },
		{ 0 },
	};
	struct lookup_iterate iter = {
		.first = rval_card_lookup_first,
		.next = rval_card_lookup_next,
		.retfcn = rval_card_lookup_return,
		.fcns = fcns,
	};
	return rval_lookup_main(uc_mgr, query, &iter);
}

static struct lookup_iterate *rval_pcm_lookup1(struct lookup_iterate *iter,
					       int device)
{
	snd_pcm_info_t *pcminfo;
	snd_ctl_t *ctl = iter->ctl_list->ctl;
	int err;

next:
	if (snd_ctl_pcm_next_device(ctl, &device) < 0 || device < 0)
		return NULL;
	pcminfo = iter->info;
	snd_pcm_info_set_device(pcminfo, device);
	err = snd_ctl_pcm_info(ctl, pcminfo);
	if (err < 0) {
		if (err == -ENOENT)
			goto next;
		uc_error("Unable to obtain PCM info (device %d)", device);
		return NULL;
	}
	return iter;
}

static struct lookup_iterate *rval_pcm_lookup_first(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
						    struct lookup_iterate *iter)
{
	return rval_pcm_lookup1(iter, -1);
}

static struct lookup_iterate *rval_pcm_lookup_next(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
						   struct lookup_iterate *iter)
{
	return rval_pcm_lookup1(iter, snd_pcm_info_get_device(iter->info));
}

static char *rval_pcm_lookup_return(struct lookup_iterate *iter,
				    snd_config_t *config ATTRIBUTE_UNUSED)
{
	char num[16];
	snprintf(num, sizeof(num), "%d", snd_pcm_info_get_device(iter->info));
	return strdup(num);
}

static int rval_pcm_lookup_init(struct lookup_iterate *iter,
				snd_config_t *config)
{
	static struct lookup_fcn pcm_fcns[] = {
		{ .name = "id", (lookup_fcn_t)snd_pcm_info_get_id },
		{ .name = "name", (lookup_fcn_t)snd_pcm_info_get_name },
		{ .name = "subname", (lookup_fcn_t)snd_pcm_info_get_subdevice_name },
		{ 0 },
	};
	snd_config_t *d;
	const char *s;
	snd_pcm_info_t *pcminfo;
	snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;

	if (snd_config_search(config, "stream", &d) == 0 &&
	    snd_config_get_string(d, &s) == 0) {
		if (strcasecmp(s, "playback") == 0)
			stream = SND_PCM_STREAM_PLAYBACK;
		else if (strcasecmp(s, "capture") == 0)
			stream = SND_PCM_STREAM_CAPTURE;
		else {
			uc_error("Unknown stream type '%s'", s);
			return -EINVAL;
		}
	}
	if (snd_pcm_info_malloc(&pcminfo))
		return -ENOMEM;
	snd_pcm_info_set_device(pcminfo, 0);
	snd_pcm_info_set_subdevice(pcminfo, 0);
	snd_pcm_info_set_stream(pcminfo, stream);
	iter->first = rval_pcm_lookup_first;
	iter->next = rval_pcm_lookup_next;
	iter->retfcn = rval_pcm_lookup_return;
	iter->fcns = pcm_fcns;
	iter->info = pcminfo;
	return 0;
}

static int rval_device_lookup_init(snd_use_case_mgr_t *uc_mgr,
				   struct lookup_iterate *iter,
				   snd_config_t *config)
{
	static struct {
		const char *name;
		int (*init)(struct lookup_iterate *iter, snd_config_t *config);
	} *t, types[] = {
		{ .name = "pcm", .init = rval_pcm_lookup_init },
		{ 0 }
	};
	snd_config_t *d;
	const char *s;
	int err;

	if (snd_config_search(config, "ctl", &d) || snd_config_get_string(d, &s)) {
		iter->ctl_list = uc_mgr_get_master_ctl(uc_mgr);
		if (iter->ctl_list == NULL) {
			uc_error("Control device is not defined!");
			return -EINVAL;
		}
	} else {
		err = uc_mgr_open_ctl(uc_mgr, &iter->ctl_list, s, 1);
		if (err < 0) {
			uc_error("Control device '%s' not found", s);
			return -EINVAL;
		}
	}
	if (snd_config_search(config, "type", &d) || snd_config_get_string(d, &s)) {
		uc_error("Missing device type!");
		return -EINVAL;
	}
	for (t = types; t; t++)
		if (strcasecmp(t->name, s) == 0)
			return t->init(iter, config);
	uc_error("Device type '%s' is invalid", s);
	return -EINVAL;
}

static void rval_device_lookup_done(struct lookup_iterate *iter)
{
	free(iter->info);
}

static char *rval_device_lookup(snd_use_case_mgr_t *uc_mgr, const char *query)
{
	struct lookup_iterate iter = {
		.init = rval_device_lookup_init,
		.done = rval_device_lookup_done,
	};
	return rval_lookup_main(uc_mgr, query, &iter);
}

static char *rval_env(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED, const char *id)
{
	char *e;

	e = getenv(id);
	if (e)
		return strdup(e);
	return NULL;
}

static char *rval_sysfs(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED, const char *id)
{
	char path[PATH_MAX], link[PATH_MAX + 1];
	struct stat sb;
	ssize_t len;
	const char *e;
	int fd;

	e = uc_mgr_sysfs_root();
	if (e == NULL)
		return NULL;
	if (id[0] == '/')
		id++;
	snprintf(path, sizeof(path), "%s/%s", e, id);
	if (lstat(path, &sb) != 0)
		return NULL;
	if (S_ISLNK(sb.st_mode)) {
		len = readlink(path, link, sizeof(link) - 1);
		if (len <= 0) {
			uc_error("sysfs: cannot read link '%s' (%d)", path, errno);
			return NULL;
		}
		link[len] = '\0';
		e = strrchr(link, '/');
		if (e)
			return strdup(e + 1);
		return NULL;
	}
	if (S_ISDIR(sb.st_mode))
		return NULL;
	if ((sb.st_mode & S_IRUSR) == 0)
		return NULL;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		uc_error("sysfs open failed for '%s' (%d)", path, errno);
		return NULL;
	}
	len = read(fd, path, sizeof(path)-1);
	close(fd);
	if (len < 0) {
		uc_error("sysfs unable to read value '%s' (%d)", path, errno);
		return NULL;
	}
	while (len > 0 && path[len-1] == '\n')
		len--;
	path[len] = '\0';
	return strdup(path);
}

static char *rval_var(snd_use_case_mgr_t *uc_mgr, const char *id)
{
	const char *v;

	if (uc_mgr->conf_format < 3) {
		uc_error("variable substitution is supported in v3+ syntax");
		return NULL;
	}

	v = uc_mgr_get_variable(uc_mgr, id);
	if (v)
		return strdup(v);
	return NULL;
}

#define MATCH_VARIABLE(name, id, fcn, empty_ok)				\
	if (strncmp((name), (id), sizeof(id) - 1) == 0) { 		\
		rval = fcn(uc_mgr);					\
		idsize = sizeof(id) - 1;				\
		allow_empty = (empty_ok);				\
		goto __rval;						\
	}

#define MATCH_VARIABLE2(name, id, fcn, empty_ok)			\
	if (strncmp((name), (id), sizeof(id) - 1) == 0) {		\
		idsize = sizeof(id) - 1;				\
		allow_empty = (empty_ok);				\
		fcn2 = (fcn);						\
		goto __match2;						\
	}

/*
 * skip escaped } character (simple version)
 */
static inline const char *strchr_with_escape(const char *str, char c)
{
	char *s;

	while (1) {
		s = strchr(str, c);
		if (s && s != str) {
			if (*(s - 1) == '\\') {
				str = s + 1;
				continue;
			}
		}
		return s;
	}
}

/*
 * remove escaped } character (simple version)
 */
static inline void strncpy_with_escape(char *dst, const char *src, size_t len)
{
	char c;

	c = *src++;
	while (c != '\0' && len > 0) {
		if (c == '\\' && *src == '}') {
			c = *src++;
			len--;
		}
		*dst++ = c;
		len--;
		c = *src++;
	}
	*dst = '\0';
}

int uc_mgr_get_substituted_value(snd_use_case_mgr_t *uc_mgr,
				 char **_rvalue,
				 const char *value)
{
	size_t size, nsize, idsize, rvalsize, dpos = 0;
	const char *tmp;
	char *r, *nr, *rval, v2[128];
	bool ignore_error, allow_empty;
	char *(*fcn2)(snd_use_case_mgr_t *, const char *id);
	int err;

	if (value == NULL)
		return -ENOENT;

	size = strlen(value) + 1;
	r = malloc(size);
	if (r == NULL)
		return -ENOMEM;

	while (*value) {
		if (*value != '$') {
__std:
			r[dpos++] = *value;
			value++;
			continue;
		}
		ignore_error = false;
		if (value[1] == '$' && value[2] == '{' && uc_mgr->conf_format >= 3) {
			value++;
			ignore_error = true;
		} else if (value[1] != '{') {
			goto __std;
		}
		fcn2 = NULL;
		MATCH_VARIABLE(value, "${OpenName}", rval_open_name, false);
		MATCH_VARIABLE(value, "${ConfLibDir}", rval_conf_libdir, false);
		MATCH_VARIABLE(value, "${ConfTopDir}", rval_conf_topdir, false);
		MATCH_VARIABLE(value, "${ConfDir}", rval_conf_dir, false);
		MATCH_VARIABLE(value, "${ConfName}", rval_conf_name, false);
		MATCH_VARIABLE(value, "${CardNumber}", rval_card_number, true);
		MATCH_VARIABLE(value, "${CardId}", rval_card_id, false);
		MATCH_VARIABLE(value, "${CardDriver}", rval_card_driver, false);
		MATCH_VARIABLE(value, "${CardName}", rval_card_name, false);
		MATCH_VARIABLE(value, "${CardLongName}", rval_card_longname, false);
		MATCH_VARIABLE(value, "${CardComponents}", rval_card_components, true);
		MATCH_VARIABLE2(value, "${env:", rval_env, false);
		MATCH_VARIABLE2(value, "${sys:", rval_sysfs, false);
		MATCH_VARIABLE2(value, "${var:", rval_var, true);
		MATCH_VARIABLE2(value, "${find-card:", rval_card_lookup, false);
		MATCH_VARIABLE2(value, "${find-device:", rval_device_lookup, false);
		MATCH_VARIABLE2(value, "${CardNumberByName:", rval_card_number_by_name, false);
		MATCH_VARIABLE2(value, "${CardIdByName:", rval_card_id_by_name, false);
__merr:
		err = -EINVAL;
		tmp = strchr(value, '}');
		if (tmp) {
			strncpy(r, value, tmp + 1 - value);
			r[tmp + 1 - value] = '\0';
			uc_error("variable '%s' is not known!", r);
		} else {
			uc_error("variable reference '%s' is not complete", value);
		}
		goto __error;
__match2:
		tmp = strchr_with_escape(value + idsize, '}');
		if (tmp) {
			rvalsize = tmp - (value + idsize);
			if (rvalsize >= sizeof(v2)) {
				err = -ENOMEM;
				goto __error;
			}
			strncpy_with_escape(v2, value + idsize, rvalsize);
			idsize += rvalsize + 1;
			if (*v2 == '$' && uc_mgr->conf_format >= 3) {
				tmp = uc_mgr_get_variable(uc_mgr, v2 + 1);
				if (tmp == NULL) {
					uc_error("define '%s' is not reachable in this context!", v2 + 1);
					rval = NULL;
				} else {
					rval = fcn2(uc_mgr, tmp);
				}
			} else {
				rval = fcn2(uc_mgr, v2);
			}
			goto __rval;
		}
		goto __merr;
__rval:
		if (rval == NULL || (!allow_empty && rval[0] == '\0')) {
			free(rval);
			if (ignore_error) {
				value += idsize;
				continue;
			}
			strncpy(r, value, idsize);
			r[idsize] = '\0';
			uc_error("variable '%s' is %s in this context!", r,
				 rval ? "empty" : "not defined");
			err = -EINVAL;
			goto __error;
		}
		value += idsize;
		rvalsize = strlen(rval);
		nsize = size + rvalsize - idsize;
		if (nsize > size) {
			nr = realloc(r, nsize);
			if (nr == NULL) {
				free(rval);
				err = -ENOMEM;
				goto __error;
			}
			size = nsize;
			r = nr;
		}
		strcpy(r + dpos, rval);
		dpos += rvalsize;
		free(rval);
	}
	r[dpos] = '\0';

	*_rvalue = r;
	return 0;

__error:
	free(r);
	return err;
}

static inline int uc_mgr_substitute_check(const char *s)
{
	return s && strstr(s, "${") != NULL;
}

int uc_mgr_substitute_tree(snd_use_case_mgr_t *uc_mgr, snd_config_t *node)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *s2;
	char *s;
	int err;

	err = snd_config_get_id(node, &id);
	if (err < 0)
		return err;
	if (uc_mgr_substitute_check(id)) {
		err = uc_mgr_get_substituted_value(uc_mgr, &s, id);
		if (err < 0)
			return err;
		err = snd_config_set_id(node, s);
		if (err < 0) {
			uc_error("unable to set substituted id '%s' (old id '%s')", s, id);
			free(s);
			return err;
		}
		free(s);
	}
	if (snd_config_get_type(node) != SND_CONFIG_TYPE_COMPOUND) {
		if (snd_config_get_type(node) == SND_CONFIG_TYPE_STRING) {
			err = snd_config_get_string(node, &s2);
			if (err < 0)
				return err;
			if (!uc_mgr_substitute_check(s2))
				return 0;
			err = uc_mgr_get_substituted_value(uc_mgr, &s, s2);
			if (err < 0)
				return err;
			err = snd_config_set_string(node, s);
			free(s);
			if (err < 0)
				return err;
		}
		return 0;
	}
	snd_config_for_each(i, next, node) {
		n = snd_config_iterator_entry(i);
		err = uc_mgr_substitute_tree(uc_mgr, n);
		if (err < 0)
			return err;
	}
	return 0;
}
