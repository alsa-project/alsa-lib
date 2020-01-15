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

static char *rval_conf_name(snd_use_case_mgr_t *uc_mgr)
{
	if (uc_mgr->conf_file_name && uc_mgr->conf_file_name[0])
		return strdup(uc_mgr->conf_file_name);
	return NULL;
}

static char *rval_card_id(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_one_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_id(ctl_list->ctl_info));
}

static char *rval_card_driver(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_one_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_driver(ctl_list->ctl_info));
}

static char *rval_card_name(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_one_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_name(ctl_list->ctl_info));
}

static char *rval_card_longname(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_one_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_longname(ctl_list->ctl_info));
}

static char *rval_card_components(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_one_ctl(uc_mgr);
	if (ctl_list == NULL)
		return NULL;
	return strdup(snd_ctl_card_info_get_components(ctl_list->ctl_info));
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

#define MATCH_VARIABLE(name, id, fcn, empty_ok)				\
	if (strncmp((name), (id), sizeof(id) - 1) == 0) { 		\
		rval = fcn(uc_mgr);					\
		idsize = sizeof(id) - 1;				\
		allow_empty = (empty_ok);				\
		goto __rval;						\
	}

#define MATCH_VARIABLE2(name, id, fcn)					\
	if (strncmp((name), (id), sizeof(id) - 1) == 0) {		\
		idsize = sizeof(id) - 1;				\
		tmp = strchr(value + idsize, '}');			\
		if (tmp) {						\
			rvalsize = tmp - (value + idsize);		\
			if (rvalsize > sizeof(v2)) {			\
				err = -ENOMEM;				\
				goto __error;				\
			}						\
			strncpy(v2, value + idsize, rvalsize);		\
			v2[rvalsize] = '\0';				\
			idsize += rvalsize + 1;				\
			rval = fcn(uc_mgr, v2);				\
			goto __rval;					\
		}							\
	}

int uc_mgr_get_substituted_value(snd_use_case_mgr_t *uc_mgr,
				 char **_rvalue,
				 const char *value)
{
	size_t size, nsize, idsize, rvalsize, dpos = 0;
	const char *tmp;
	char *r, *nr, *rval, v2[32];
	int err;

	if (value == NULL)
		return -ENOENT;

	size = strlen(value) + 1;
	r = malloc(size);
	if (r == NULL)
		return -ENOMEM;

	while (*value) {
		if (*value == '$' && *(value+1) == '{') {
			bool allow_empty = false;

			MATCH_VARIABLE(value, "${ConfName}", rval_conf_name, false);
			MATCH_VARIABLE(value, "${CardId}", rval_card_id, false);
			MATCH_VARIABLE(value, "${CardDriver}", rval_card_driver, false);
			MATCH_VARIABLE(value, "${CardName}", rval_card_name, false);
			MATCH_VARIABLE(value, "${CardLongName}", rval_card_longname, false);
			MATCH_VARIABLE(value, "${CardComponents}", rval_card_components, true);
			MATCH_VARIABLE2(value, "${env:", rval_env);
			MATCH_VARIABLE2(value, "${sys:", rval_sysfs);
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
__rval:
			if (rval == NULL || (!allow_empty && rval[0] == '\0')) {
				free(rval);
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
		} else {
			r[dpos++] = *value;
			value++;
		}
	}
	r[dpos] = '\0';

	*_rvalue = r;
	return 0;

__error:
	free(r);
	return err;
}
