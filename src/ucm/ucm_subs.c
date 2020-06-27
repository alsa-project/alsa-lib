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

	return get_card_number(get_ctl_list_by_name(uc_mgr, id));
}

static char *rval_card_id_by_name(snd_use_case_mgr_t *uc_mgr, const char *id)
{
	struct ctl_list *ctl_list;

	if (uc_mgr->conf_format < 3) {
		uc_error("CardIdByName substitution is supported in v3+ syntax");
		return NULL;
	}

	ctl_list = get_ctl_list_by_name(uc_mgr, id);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_id(ctl_list->ctl_info));
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
	char *e;
	int fd;

	e = getenv("SYSFS_PATH");
	if (e == NULL)
		e = "/sys";
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

int uc_mgr_get_substituted_value(snd_use_case_mgr_t *uc_mgr,
				 char **_rvalue,
				 const char *value)
{
	size_t size, nsize, idsize, rvalsize, dpos = 0;
	const char *tmp;
	char *r, *nr, *rval, v2[48];
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
		allow_empty = false;
		fcn2 = NULL;
		MATCH_VARIABLE(value, "${OpenName}", rval_open_name, false);
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
		tmp = strchr(value + idsize, '}');
		if (tmp) {
			rvalsize = tmp - (value + idsize);
			if (rvalsize >= sizeof(v2)) {
				err = -ENOMEM;
				goto __error;
			}
			strncpy(v2, value + idsize, rvalsize);
			v2[rvalsize] = '\0';
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
			uc_error("variable '%s' is not defined in this context!", r);
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
