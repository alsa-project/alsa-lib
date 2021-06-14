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
 *  Copyright (C) 2008-2010 SlimLogic Ltd
 *  Copyright (C) 2010 Wolfson Microelectronics PLC
 *  Copyright (C) 2010 Texas Instruments Inc.
 *  Copyright (C) 2010 Red Hat Inc.
 *  Authors: Liam Girdwood <lrg@slimlogic.co.uk>
 *	         Stefan Schmidt <stefan@slimlogic.co.uk>
 *	         Justin Xu <justinx@slimlogic.co.uk>
 *               Jaroslav Kysela <perex@perex.cz>
 */

#include "ucm_local.h"
#include "../control/control_local.h"
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>

/*
 * misc
 */

static int get_value(snd_use_case_mgr_t *uc_mgr,
			const char *identifier,
			char **value,
			const char *mod_dev_name,
			const char *verb_name,
			int exact);
static int get_value1(snd_use_case_mgr_t *uc_mgr, char **value,
		      struct list_head *value_list, const char *identifier);
static int get_value3(snd_use_case_mgr_t *uc_mgr,
		      char **value,
		      const char *identifier,
		      struct list_head *value_list1,
		      struct list_head *value_list2,
		      struct list_head *value_list3);

static int execute_component_seq(snd_use_case_mgr_t *uc_mgr,
				 struct component_sequence *cmpt_seq,
				 struct list_head *value_list1,
				 struct list_head *value_list2,
				 struct list_head *value_list3,
				 char *cdev);

static int check_identifier(const char *identifier, const char *prefix)
{
	int len;

	len = strlen(prefix);
	if (strncmp(identifier, prefix, len) != 0)
		return 0;

	if (identifier[len] == 0 || identifier[len] == '/')
		return 1;

	return 0;
}

static int list_count(struct list_head *list)
{
	struct list_head *pos;
	int count = 0;

	list_for_each(pos, list) {
		count += 1;
	}
	return count;
}

static int alloc_str_list(struct list_head *list, int mult, char **result[])
{
	char **res;
	int cnt;

	cnt = list_count(list) * mult;
	if (cnt == 0) {
		*result = NULL;
		return cnt;
	}
	res = calloc(mult, cnt * sizeof(char *));
	if (res == NULL)
		return -ENOMEM;
	*result = res;
	return cnt;
}

/**
 * \brief Create an identifier
 * \param fmt Format (sprintf like)
 * \param ... Optional arguments for sprintf like format
 * \return Allocated string identifier or NULL on error
 */
char *snd_use_case_identifier(const char *fmt, ...)
{
	char *str, *res;
	int size = strlen(fmt) + 512;
	va_list args;

	str = malloc(size);
	if (str == NULL)
		return NULL;
	va_start(args, fmt);
	vsnprintf(str, size, fmt, args);
	va_end(args);
	str[size-1] = '\0';
	res = realloc(str, strlen(str) + 1);
	if (res)
		return res;
	return str;
}

/**
 * \brief Free a string list
 * \param list The string list to free
 * \param items Count of strings
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_free_list(const char *list[], int items)
{
	int i;
	if (list == NULL)
		return 0;
	for (i = 0; i < items; i++)
		free((void *)list[i]);
	free(list);
	return 0;
}

static int read_tlv_file(unsigned int **res,
			 const char *filepath)
{
	int err = 0;
	int fd;
	struct stat st;
	size_t sz;
	ssize_t sz_read;
	struct snd_ctl_tlv *tlv;

	fd = open(filepath, O_RDONLY);
	if (fd < 0) {
		err = -errno;
		return err;
	}
	if (fstat(fd, &st) == -1) {
		err = -errno;
		goto __fail;
	}
	sz = st.st_size;
	if (sz > 16 * 1024 * 1024 || sz < 8 || sz % 4) {
		uc_error("File size should be less than 16 MB "
			 "and multiple of 4");
		err = -EINVAL;
		goto __fail;
	}
	*res = malloc(sz);
	if (res == NULL) {
		err = -ENOMEM;
		goto __fail;
	}
	sz_read = read(fd, *res, sz);
	if (sz_read < 0 || (size_t)sz_read != sz) {
		err = -EIO;
		free(*res);
		*res = NULL;
	}
	/* Check if the tlv file specifies valid size. */
	tlv = (struct snd_ctl_tlv *)(*res);
	if (tlv->length + 2 * sizeof(unsigned int) != sz) {
		uc_error("Invalid tlv size: %d", tlv->length);
		err = -EINVAL;
		free(*res);
		*res = NULL;
	}

__fail:
	close(fd);
	return err;
}

static int binary_file_parse(snd_ctl_elem_value_t *dst,
			      snd_ctl_elem_info_t *info,
			      const char *filepath)
{
	int err = 0;
	int fd;
	struct stat st;
	size_t sz;
	ssize_t sz_read;
	char *res;
	snd_ctl_elem_type_t type;
	unsigned int idx, count;

	type = snd_ctl_elem_info_get_type(info);
	if (type != SND_CTL_ELEM_TYPE_BYTES) {
		uc_error("only support byte type!");
		err = -EINVAL;
		return err;
	}
	fd = open(filepath, O_RDONLY);
	if (fd < 0) {
		err = -errno;
		return err;
	}
	if (stat(filepath, &st) == -1) {
		err = -errno;
		goto __fail;
	}
	sz = st.st_size;
	count = snd_ctl_elem_info_get_count(info);
	if (sz != count || sz > sizeof(dst->value.bytes)) {
		uc_error("invalid parameter size %d!", sz);
		err = -EINVAL;
		goto __fail;
	}
	res = malloc(sz);
	if (res == NULL) {
		err = -ENOMEM;
		goto __fail;
	}
	sz_read = read(fd, res, sz);
	if (sz_read < 0 || (size_t)sz_read != sz) {
		err = -errno;
		goto __fail_read;
	}
	for (idx = 0; idx < sz; idx++)
		snd_ctl_elem_value_set_byte(dst, idx, *(res + idx));
      __fail_read:
	free(res);
      __fail:
	close(fd);
	return err;
}

static const char *parse_type(const char *p, const char *prefix, size_t len,
			      snd_ctl_elem_info_t *info)
{
	if (strncasecmp(p, prefix, len))
		return p;
	p += len;
	if (info->type != SND_CTL_ELEM_TYPE_NONE)
		return NULL;
	if (strncasecmp(p, "bool", sizeof("bool") - 1) == 0)
		info->type = SND_CTL_ELEM_TYPE_BOOLEAN;
	else if (strncasecmp(p, "integer64", sizeof("integer64") - 1) == 0)
		info->type = SND_CTL_ELEM_TYPE_INTEGER64;
	else if (strncasecmp(p, "int64", sizeof("int64") - 1) == 0)
		info->type = SND_CTL_ELEM_TYPE_INTEGER64;
	else if (strncasecmp(p, "int", sizeof("int") - 1) == 0)
		info->type = SND_CTL_ELEM_TYPE_INTEGER;
	else if (strncasecmp(p, "enum", sizeof("enum") - 1) == 0)
		info->type = SND_CTL_ELEM_TYPE_ENUMERATED;
	else if (strncasecmp(p, "bytes", sizeof("bytes") - 1) == 0)
		info->type = SND_CTL_ELEM_TYPE_BYTES;
	else
		return NULL;
	while (isalpha(*p))
		p++;
	return p;
}

static const char *parse_uint(const char *p, const char *prefix, size_t len,
			      unsigned int min, unsigned int max, unsigned int *rval)
{
	long v;
	char *end;

	if (strncasecmp(p, prefix, len))
		return p;
	p += len;
	v = strtol(p, &end, 0);
	if (*end != '\0' && *end != ' ' && *end != ',') {
		uc_error("unable to parse '%s'", prefix);
		return NULL;
	}
	if (v < min || v > max) {
		uc_error("value '%s' out of range %u-%u %(%ld)", min, max, v);
		return NULL;
	}
	*rval = v;
	return end;
}

static const char *parse_labels(const char *p, const char *prefix, size_t len,
				snd_ctl_elem_info_t *info)
{
	const char *s;
	char *buf, *bp;
	size_t l;
	int c;

	if (info->type != SND_CTL_ELEM_TYPE_ENUMERATED)
		return NULL;
	if (strncasecmp(p, prefix, len))
		return p;
	p += len;
	s = p;
	c = *s;
	l = 0;
	if (c == '\'' || c == '\"') {
		s++;
		while (*s && *s != c) {
			s++, l++;
		}
		if (*s == c)
			s++;
	} else {
		while (*s && *s != ',')
			l++;
	}
	if (l == 0)
		return NULL;
	buf = malloc(l + 1);
	if (buf == NULL)
		return NULL;
	memcpy(buf, p + ((c == '\'' || c == '\"') ? 1 : 0), l);
	buf[l] = '\0';
	info->value.enumerated.items = 1;
	for (bp = buf; *bp; bp++) {
		if (*bp == ';') {
			if (bp == buf || bp[1] == ';') {
				free(buf);
				return NULL;
			}
			info->value.enumerated.items++;
			*bp = '\0';
		}
	}
	info->value.enumerated.names_ptr = (uintptr_t)buf;
	info->value.enumerated.names_length = l + 1;
	return s;
}

static int parse_cset_new_info(snd_ctl_elem_info_t *info, const char *s, const char **pos)
{
	const char *p = s, *op;

	info->count = 1;
	while (*s) {
		op = p;
		p = parse_type(p, "type=", sizeof("type=") - 1, info);
		if (p != op)
			goto next;
		p = parse_uint(p, "elements=", sizeof("elements=") - 1, 1, 128, (unsigned int *)&info->owner);
		if (p != op)
			goto next;
		p = parse_uint(p, "count=", sizeof("count=") - 1, 1, 128, &info->count);
		if (p != op)
			goto next;
		p = parse_labels(p, "labels=", sizeof("labels=") - 1, info);
next:
		if (p == NULL)
			goto er;
		if (*p == ',')
			p++;
		if (isspace(*p))
			break;
		if (op == p)
			goto er;
	}
	*pos = p;
	return 0;
er:
	uc_error("unknown syntax '%s'", p);
	return -EINVAL;
}

static int execute_cset(snd_ctl_t *ctl, const char *cset, unsigned int type)
{
	const char *pos;
	int err;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *value;
	snd_ctl_elem_info_t *info, *info2 = NULL;
	unsigned int *res = NULL;

	snd_ctl_elem_id_malloc(&id);
	snd_ctl_elem_value_malloc(&value);
	snd_ctl_elem_info_malloc(&info);

	err = __snd_ctl_ascii_elem_id_parse(id, cset, &pos);
	if (err < 0)
		goto __fail;
	while (*pos && isspace(*pos))
		pos++;
	if (type == SEQUENCE_ELEMENT_TYPE_CSET_NEW) {
		snd_ctl_elem_info_malloc(&info2);
		snd_ctl_elem_info_set_id(info2, id);
		err = parse_cset_new_info(info2, pos, &pos);
		if (err < 0 || !*pos) {
			uc_error("undefined or wrong id config for cset-new", cset);
			err = -EINVAL;
			goto __fail;
		}
		while (*pos && isspace(*pos))
			pos++;
	}
	if (!*pos) {
		if (type != SEQUENCE_ELEMENT_TYPE_CTL_REMOVE) {
			uc_error("undefined value for cset >%s<", cset);
			err = -EINVAL;
			goto __fail;
		}
	} else if (type == SEQUENCE_ELEMENT_TYPE_CTL_REMOVE) {
		uc_error("extra value for ctl-remove >%s<", cset);
		err = -EINVAL;
		goto __fail;
	}

	snd_ctl_elem_info_set_id(info, id);
	err = snd_ctl_elem_info(ctl, info);
	if (type == SEQUENCE_ELEMENT_TYPE_CSET_NEW ||
	    type == SEQUENCE_ELEMENT_TYPE_CTL_REMOVE) {
		if (err >= 0) {
			err = snd_ctl_elem_remove(ctl, id);
			if (err < 0) {
				uc_error("unable to remove control");
				err = -EINVAL;
				goto __fail;
			}
		}
		if (type == SEQUENCE_ELEMENT_TYPE_CTL_REMOVE)
			goto __ok;
		err = __snd_ctl_add_elem_set(ctl, info2, info2->owner, info2->count);
		if (err < 0) {
			uc_error("unable to create new control");
			goto __fail;
		}
		/* new id copy */
		snd_ctl_elem_info_get_id(info2, id);
		snd_ctl_elem_info_set_id(info, id);
	} else if (err < 0)
		goto __fail;
	if (type == SEQUENCE_ELEMENT_TYPE_CSET_TLV) {
		if (!snd_ctl_elem_info_is_tlv_writable(info)) {
			err = -EINVAL;
			goto __fail;
		}
		err = read_tlv_file(&res, pos);
		if (err < 0)
			goto __fail;
		err = snd_ctl_elem_tlv_write(ctl, id, res);
		if (err < 0)
			goto __fail;
	} else {
		snd_ctl_elem_value_set_id(value, id);
		err = snd_ctl_elem_read(ctl, value);
		if (err < 0)
			goto __fail;
		if (type == SEQUENCE_ELEMENT_TYPE_CSET_BIN_FILE)
			err = binary_file_parse(value, info, pos);
		else
			err = snd_ctl_ascii_value_parse(ctl, value, info, pos);
		if (err < 0)
			goto __fail;
		err = snd_ctl_elem_write(ctl, value);
		if (err < 0)
			goto __fail;
		if (type == SEQUENCE_ELEMENT_TYPE_CSET_NEW) {
			unsigned int idx;
			for (idx = 1; idx < (unsigned int)info2->owner; idx++) {
				value->id.numid += 1;
				err = snd_ctl_elem_write(ctl, value);
				if (err < 0)
					goto __fail;
			}
		}
	}
      __ok:
	err = 0;
      __fail:
	free(id);
	free(value);
	if (info2) {
		if (info2->type == SND_CTL_ELEM_TYPE_ENUMERATED)
			free((void *)info2->value.enumerated.names_ptr);
		free(info2);
	}
	free(info);
	free(res);

	return err;
}

static int execute_sysw(const char *sysw)
{
	char path[PATH_MAX];
	const char *e;
	char *s, *value;
	ssize_t wlen;
	size_t len;
	int fd, myerrno;
	bool ignore_error = false;

	if (sysw == NULL || *sysw == '\0')
		return 0;

	if (sysw[0] == '-') {
		ignore_error = true;
		sysw++;
	}

	if (sysw[0] == ':')
		return -EINVAL;

	s = strdup(sysw[0] != '/' ? sysw : sysw + 1);
	if (s == NULL)
		return -ENOMEM;

	value = strchr(s, ':');
	if (!value) {
		free(s);
		return -EINVAL;
	}
	*value = '\0';
	value++;
	len = strlen(value);
	if (len < 1) {
		free(s);
		return -EINVAL;
	}

	e = uc_mgr_sysfs_root();
	if (e == NULL) {
		free(s);
		return -EINVAL;
	}
	snprintf(path, sizeof(path), "%s/%s", e, s);

	fd = open(path, O_WRONLY|O_CLOEXEC);
	if (fd < 0) {
		free(s);
		if (ignore_error)
			return 0;
		uc_error("unable to open '%s' for write", path);
		return -EINVAL;
	}
	wlen = write(fd, value, len);
	myerrno = errno;
	close(fd);
	free(s);

	if (ignore_error)
		return 0;

	if (wlen != (ssize_t)len) {
		uc_error("unable to write '%s' to '%s': %s", value, path, strerror(myerrno));
		return -EINVAL;
	}

	return 0;
}

int _snd_config_save_node_value(snd_config_t *n, snd_output_t *out, unsigned int level);

static int execute_cfgsave(snd_use_case_mgr_t *uc_mgr, const char *filename)
{
	snd_config_t *config = uc_mgr->local_config;
	char *file, *root;
	snd_output_t *out;
	bool with_root = false;
	int err = 0;

	file = strdup(filename);
	if (!file)
		return -ENOMEM;
	root = strchr(file, ':');
	if (config && root) {
		*root++ = '\0';
		if (*root == '+') {
			with_root = true;
			root++;
		}
		err = snd_config_search(config, root, &config);
		if (err < 0) {
			uc_error("Unable to find subtree '%s'", root);
			goto _err;
		}
	}

	err = snd_output_stdio_open(&out, file, "w+");
	if (err < 0) {
		uc_error("unable to open file '%s': %s", file, snd_strerror(err));
		goto _err;
	}
	if (!config || snd_config_is_empty(config)) {
		snd_output_close(out);
		goto _err;
	}
	if (with_root) {
		snd_output_printf(out, "%s ", root);
		err = _snd_config_save_node_value(config, out, 0);
	} else {
		err = snd_config_save(config, out);
	}
	snd_output_close(out);
	if (err < 0) {
		uc_error("unable to save configuration: %s", snd_strerror(err));
		goto _err;
	}
_err:
	free(file);
	return err;
}

static int rewrite_device_value(snd_use_case_mgr_t *uc_mgr, const char *name, char **value)
{
	char *sval;
	size_t l;
	static const char **s, *_prefix[] = {
		"PlaybackCTL",
		"CaptureCTL",
		"PlaybackMixer",
		"CaptureMixer",
		"PlaybackPCM",
		"CapturePCM",
		NULL
	};

	if (!uc_mgr_has_local_config(uc_mgr))
		return 0;
	for (s = _prefix; *s && *value; s++) {
		if (strcmp(*s, name) != 0)
			continue;
		l = strlen(*value) + 9 + 1;
		sval = malloc(l);
		if (sval == NULL) {
			free(*value);
			*value = NULL;
			return -ENOMEM;
		}
		snprintf(sval, l, "_ucm%04X.%s", uc_mgr->ucm_card_number, *value);
		free(*value);
		*value = sval;
		break;
	}
	return 0;
}

/**
 * \brief Execute the sequence
 * \param uc_mgr Use case manager
 * \param seq Sequence
 * \return zero on success, otherwise a negative error code
 */
static int execute_sequence(snd_use_case_mgr_t *uc_mgr,
			    struct list_head *seq,
			    struct list_head *value_list1,
			    struct list_head *value_list2,
			    struct list_head *value_list3)
{
	struct list_head *pos;
	struct sequence_element *s;
	char *cdev = NULL;
	snd_ctl_t *ctl = NULL;
	struct ctl_list *ctl_list;
	bool ignore_error;
	int err = 0;

	list_for_each(pos, seq) {
		s = list_entry(pos, struct sequence_element, list);
		switch (s->type) {
		case SEQUENCE_ELEMENT_TYPE_CDEV:
			cdev = strdup(s->data.cdev);
			if (cdev == NULL)
				goto __fail_nomem;
			if (rewrite_device_value(uc_mgr, "PlaybackCTL", &cdev))
				goto __fail_nomem;
			break;
		case SEQUENCE_ELEMENT_TYPE_CSET:
		case SEQUENCE_ELEMENT_TYPE_CSET_BIN_FILE:
		case SEQUENCE_ELEMENT_TYPE_CSET_TLV:
		case SEQUENCE_ELEMENT_TYPE_CSET_NEW:
		case SEQUENCE_ELEMENT_TYPE_CTL_REMOVE:
			if (cdev == NULL && uc_mgr->in_component_domain) {
				/* For sequence of a component device, use
				 * its parent's cdev stored by ucm manager.
				 */
				if (uc_mgr->cdev == NULL) {
					uc_error("cdev is not defined!");
					return err;
				}

				cdev = strndup(uc_mgr->cdev, PATH_MAX);
				if (!cdev)
					return -ENOMEM;
			} else if (cdev == NULL) {
				char *playback_ctl = NULL;
				char *capture_ctl = NULL;

				err = get_value3(uc_mgr, &playback_ctl, "PlaybackCTL",
						 value_list1,
						 value_list2,
						 value_list3);
				if (err < 0 && err != -ENOENT) {
					uc_error("cdev is not defined!");
					return err;
				}
				err = get_value3(uc_mgr, &capture_ctl, "CaptureCTL",
						 value_list1,
						 value_list2,
						 value_list3);
				if (err < 0 && err != -ENOENT) {
					free(playback_ctl);
					uc_error("cdev is not defined!");
					return err;
				}
				if (playback_ctl == NULL &&
				    capture_ctl == NULL) {
					uc_error("cdev is not defined!");
					return -EINVAL;
				}
				if (playback_ctl != NULL &&
				    capture_ctl != NULL &&
				    strcmp(playback_ctl, capture_ctl) != 0) {
					free(playback_ctl);
					free(capture_ctl);
					uc_error("cdev is not equal for playback and capture!");
					return -EINVAL;
				}
				if (playback_ctl != NULL) {
					cdev = playback_ctl;
					free(capture_ctl);
				} else {
					cdev = capture_ctl;
				}
			}
			if (ctl == NULL) {
				err = uc_mgr_open_ctl(uc_mgr, &ctl_list, cdev, 1);
				if (err < 0) {
					uc_error("unable to open ctl device '%s'", cdev);
					goto __fail;
				}
				ctl = ctl_list->ctl;
			}
			err = execute_cset(ctl, s->data.cset, s->type);
			if (err < 0) {
				uc_error("unable to execute cset '%s'", s->data.cset);
				goto __fail;
			}
			break;
		case SEQUENCE_ELEMENT_TYPE_SYSSET:
			err = execute_sysw(s->data.sysw);
			if (err < 0)
				goto __fail;
			break;
		case SEQUENCE_ELEMENT_TYPE_SLEEP:
			usleep(s->data.sleep);
			break;
		case SEQUENCE_ELEMENT_TYPE_EXEC:
			if (s->data.exec == NULL)
				break;
			ignore_error = s->data.exec[0] == '-';
			err = uc_mgr_exec(s->data.exec + (ignore_error ? 1 : 0));
			if (ignore_error == false && err != 0) {
				uc_error("exec '%s' failed (exit code %d)", s->data.exec, err);
				goto __fail;
			}
			break;
		case SEQUENCE_ELEMENT_TYPE_SHELL:
			if (s->data.exec == NULL)
				break;
			ignore_error = s->data.exec[0] == '-';
shell_retry:
			err = system(s->data.exec + (ignore_error ? 1 : 0));
			if (WIFSIGNALED(err)) {
				err = -EINTR;
			} if (WIFEXITED(err)) {
				if (ignore_error == false && WEXITSTATUS(err) != 0) {
					uc_error("command '%s' failed (exit code %d)", s->data.exec, WEXITSTATUS(err));
					err = -EINVAL;
					goto __fail;
				}
			} else if (err < 0) {
				if (errno == EAGAIN)
					goto shell_retry;
				err = -errno;
				goto __fail;
			}
			break;
		case SEQUENCE_ELEMENT_TYPE_CMPT_SEQ:
			/* Execute enable or disable sequence of a component
			 * device. Pass the cdev defined by the machine device.
			 */
			err = execute_component_seq(uc_mgr,
						    &s->data.cmpt_seq,
						    value_list1,
						    value_list2,
						    value_list3,
						    cdev);
			if (err < 0)
				goto __fail;
			break;
		case SEQUENCE_ELEMENT_TYPE_CFGSAVE:
			err = execute_cfgsave(uc_mgr, s->data.cfgsave);
			if (err < 0)
				goto __fail;
			break;
		default:
			uc_error("unknown sequence command %i", s->type);
			break;
		}
	}
	free(cdev);
	return 0;
      __fail_nomem:
	err = -ENOMEM;
      __fail:
	free(cdev);
	return err;

}

/* Execute enable or disable sequence of a component device.
 *
 * For a component device (a codec or embedded DSP), its sequence doesn't
 * specify the sound card device 'cdev', because a component can be reused
 * by different sound cards (machines). So when executing its sequence, a
 * parameter 'cdev' is used to pass cdev defined by the sequence of its
 * parent, the machine device. UCM manger will store the cdev when entering
 * the component domain.
 */
static int execute_component_seq(snd_use_case_mgr_t *uc_mgr,
				 struct component_sequence *cmpt_seq,
				 struct list_head *value_list1 ATTRIBUTE_UNUSED,
				 struct list_head *value_list2 ATTRIBUTE_UNUSED,
				 struct list_head *value_list3 ATTRIBUTE_UNUSED,
				 char *cdev)
{
	struct use_case_device *device = cmpt_seq->device;
	struct list_head *seq;
	int err;

	/* enter component domain and store cdev for the component */
	uc_mgr->in_component_domain = 1;
	uc_mgr->cdev = cdev;

	/* choose enable or disable sequence of the component device */
	if (cmpt_seq->enable)
		seq = &device->enable_list;
	else
		seq = &device->disable_list;

	/* excecute the sequence of the component dev */
	err = execute_sequence(uc_mgr, seq,
			       &device->value_list,
			       &uc_mgr->active_verb->value_list,
			       &uc_mgr->value_list);

	/* exit component domain and clear cdev */
	uc_mgr->in_component_domain = 0;
	uc_mgr->cdev = NULL;

	return err;
}

static int add_auto_value(snd_use_case_mgr_t *uc_mgr, const char *key, char *value)
{
	char *s;
	int err;

	err = get_value1(uc_mgr, &value, &uc_mgr->value_list, key);
	if (err == -ENOENT) {
		s = strdup(value);
		if (s == NULL)
			return -ENOMEM;
		return uc_mgr_add_value(&uc_mgr->value_list, key, s);
	} else if (err < 0) {
		return err;
	}
	free(value);
	return 0;
}

static int add_auto_values(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;
	const char *id;
	char buf[40];
	int err;

	ctl_list = uc_mgr_get_master_ctl(uc_mgr);
	if (ctl_list) {
		id = snd_ctl_card_info_get_id(ctl_list->ctl_info);
		snprintf(buf, sizeof(buf), "hw:%s", id);
		err = add_auto_value(uc_mgr, "PlaybackCTL", buf);
		if (err < 0)
			return err;
		err = add_auto_value(uc_mgr, "CaptureCTL", buf);
		if (err < 0)
			return err;
	}
	return 0;
}

/**
 * \brief execute default commands
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
static int set_defaults(snd_use_case_mgr_t *uc_mgr)
{
	int err;

	if (uc_mgr->default_list_executed)
		return 0;
	err = execute_sequence(uc_mgr, &uc_mgr->default_list,
			       &uc_mgr->value_list, NULL, NULL);
	if (err < 0) {
		uc_error("Unable to execute default sequence");
		return err;
	}
	uc_mgr->default_list_executed = 1;
	return 0;
}

/**
 * \brief Import master config and execute the default sequence
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
static int import_master_config(snd_use_case_mgr_t *uc_mgr)
{
	int err;
	
	err = uc_mgr_import_master_config(uc_mgr);
	if (err < 0)
		return err;
	return add_auto_values(uc_mgr);
}

/**
 * \brief Check, if the UCM configuration is empty
 * \param uc_mgr Use case Manager
 * \return zero on success, otherwise a negative error code
 */
static int check_empty_configuration(snd_use_case_mgr_t *uc_mgr)
{
	int err;
	char *value;

	err = get_value(uc_mgr, "Linked", &value, NULL, NULL, 1);
	if (err >= 0) {
		err = strcasecmp(value, "true") == 0 ||
		      strcmp(value, "1") == 0;
		free(value);
		if (err)
			return 0;
	}
	if (!list_empty(&uc_mgr->verb_list))
		return 0;
	if (!list_empty(&uc_mgr->fixedboot_list))
		return 0;
	if (!list_empty(&uc_mgr->boot_list))
		return 0;
	return -ENXIO;
}

/**
 * \brief Universal find - string in a list
 * \param list List of structures
 * \param offset Offset of list structure
 * \param soffset Offset of string structure
 * \param match String to match
 * \return structure on success, otherwise a NULL (not found)
 */
static void *find0(struct list_head *list,
		   unsigned long offset,
		   unsigned long soffset,
		   const char *match)
{
	struct list_head *pos;
	char *ptr, *str;

	list_for_each(pos, list) {
		ptr = list_entry_offset(pos, char, offset);
		str = *((char **)(ptr + soffset));
		if (strcmp(str, match) == 0)
			return ptr;
	}
	return NULL;
}

#define find(list, type, member, value, match) \
	find0(list, (unsigned long)(&((type *)0)->member), \
		    (unsigned long)(&((type *)0)->value), match)

/**
 * \brief Universal string list
 * \param list List of structures
 * \param result Result list
 * \param offset Offset of list structure
 * \param s1offset Offset of string structure
 * \return count of items on success, otherwise a negative error code
 */
static int get_list0(struct list_head *list,
		     const char **result[],
		     unsigned long offset,
		     unsigned long s1offset)
{
	char **res;
	int cnt;
	struct list_head *pos;
	char *ptr, *str1;

	cnt = alloc_str_list(list, 1, &res);
	if (cnt <= 0) {
		*result = NULL;
		return cnt;
	}
	*result = (const char **)res;
	list_for_each(pos, list) {
		ptr = list_entry_offset(pos, char, offset);
		str1 = *((char **)(ptr + s1offset));
		if (str1 != NULL) {
			*res = strdup(str1);
			if (*res == NULL)
				goto __fail;
		} else {
			*res = NULL;
		}
		res++;
	}
	return cnt;
      __fail:
	snd_use_case_free_list(*result, cnt);
	return -ENOMEM;
}

#define get_list(list, result, type, member, s1) \
	get_list0(list, result, \
		    (unsigned long)(&((type *)0)->member), \
		    (unsigned long)(&((type *)0)->s1))

/**
 * \brief Universal string list - pair of strings
 * \param list List of structures
 * \param result Result list
 * \param offset Offset of list structure
 * \param s1offset Offset of string structure
 * \param s1offset Offset of string structure
 * \return count of items on success, otherwise a negative error code
 */
static int get_list20(struct list_head *list,
		      const char **result[],
		      unsigned long offset,
		      unsigned long s1offset,
		      unsigned long s2offset)
{
	char **res;
	int cnt;
	struct list_head *pos;
	char *ptr, *str1, *str2;

	cnt = alloc_str_list(list, 2, &res);
	if (cnt <= 0) {
		*result = NULL;
		return cnt;
	}
	*result = (const char **)res;
	list_for_each(pos, list) {
		ptr = list_entry_offset(pos, char, offset);
		str1 = *((char **)(ptr + s1offset));
		if (str1 != NULL) {
			*res = strdup(str1);
			if (*res == NULL)
				goto __fail;
		} else {
			*res = NULL;
		}
		res++;
		str2 = *((char **)(ptr + s2offset));
		if (str2 != NULL) {
			*res = strdup(str2);
			if (*res == NULL)
				goto __fail;
		} else {
			*res = NULL;
		}
		res++;
	}
	return cnt;
      __fail:
	snd_use_case_free_list(*result, cnt);
	return -ENOMEM;
}

#define get_list2(list, result, type, member, s1, s2) \
	get_list20(list, result, \
		    (unsigned long)(&((type *)0)->member), \
		    (unsigned long)(&((type *)0)->s1), \
		    (unsigned long)(&((type *)0)->s2))

/**
 * \brief Find verb
 * \param uc_mgr Use case manager
 * \param verb_name verb to find
 * \return structure on success, otherwise a NULL (not found)
 */
static inline struct use_case_verb *find_verb(snd_use_case_mgr_t *uc_mgr,
					      const char *verb_name)
{
	return find(&uc_mgr->verb_list,
		    struct use_case_verb, list, name,
		    verb_name);
}

static int is_devlist_supported(snd_use_case_mgr_t *uc_mgr, 
	struct dev_list *dev_list)
{
	struct dev_list_node *device;
	struct use_case_device *adev;
	struct list_head *pos, *pos1;
	int found_ret;

	switch (dev_list->type) {
	case DEVLIST_NONE:
	default:
		return 1;
	case DEVLIST_SUPPORTED:
		found_ret = 1;
		break;
	case DEVLIST_CONFLICTING:
		found_ret = 0;
		break;
	}

	list_for_each(pos, &dev_list->list) {
		device = list_entry(pos, struct dev_list_node, list);

		list_for_each(pos1, &uc_mgr->active_devices) {
			adev = list_entry(pos1, struct use_case_device,
					    active_list);
			if (!strcmp(device->name, adev->name))
				return found_ret;
		}
	}
	return 1 - found_ret;
}

static inline int is_modifier_supported(snd_use_case_mgr_t *uc_mgr, 
	struct use_case_modifier *modifier)
{
	return is_devlist_supported(uc_mgr, &modifier->dev_list);
}

static inline int is_device_supported(snd_use_case_mgr_t *uc_mgr, 
	struct use_case_device *device)
{
	return is_devlist_supported(uc_mgr, &device->dev_list);
}

/**
 * \brief Find device
 * \param verb Use case verb
 * \param device_name device to find
 * \return structure on success, otherwise a NULL (not found)
 */
static inline struct use_case_device *
	find_device(snd_use_case_mgr_t *uc_mgr, struct use_case_verb *verb,
		    const char *device_name, int check_supported)
{
	struct use_case_device *device;
	struct list_head *pos;

	list_for_each(pos, &verb->device_list) {
		device = list_entry(pos, struct use_case_device, list);

		if (strcmp(device_name, device->name))
			continue;

		if (check_supported &&
		    !is_device_supported(uc_mgr, device))
			continue;

		return device;
	}
	return NULL;
}

/**
 * \brief Find modifier
 * \param verb Use case verb
 * \param modifier_name modifier to find
 * \return structure on success, otherwise a NULL (not found)
 */
static struct use_case_modifier *
	find_modifier(snd_use_case_mgr_t *uc_mgr, struct use_case_verb *verb,
		      const char *modifier_name, int check_supported)
{
	struct use_case_modifier *modifier;
	struct list_head *pos;

	list_for_each(pos, &verb->modifier_list) {
		modifier = list_entry(pos, struct use_case_modifier, list);

		if (strcmp(modifier->name, modifier_name))
			continue;

		if (check_supported &&
		    !is_modifier_supported(uc_mgr, modifier))
			continue;

		return modifier;
	}
	return NULL;
}

long device_status(snd_use_case_mgr_t *uc_mgr,
		   const char *device_name)
{
	struct use_case_device *dev;
	struct list_head *pos;

	list_for_each(pos, &uc_mgr->active_devices) {
		dev = list_entry(pos, struct use_case_device, active_list);
		if (strcmp(dev->name, device_name) == 0)
			return 1;
	}
	return 0;
}

long modifier_status(snd_use_case_mgr_t *uc_mgr,
		     const char *modifier_name)
{
	struct use_case_modifier *mod;
	struct list_head *pos;

	list_for_each(pos, &uc_mgr->active_modifiers) {
		mod = list_entry(pos, struct use_case_modifier, active_list);
		if (strcmp(mod->name, modifier_name) == 0)
			return 1;
	}
	return 0;
}

/**
 * \brief Set verb
 * \param uc_mgr Use case manager
 * \param verb verb to set
 * \param enable nonzero = enable, zero = disable
 * \return zero on success, otherwise a negative error code
 */
static int set_verb(snd_use_case_mgr_t *uc_mgr,
		    struct use_case_verb *verb,
		    int enable)
{
	struct list_head *seq;
	int err;

	if (enable) {
		err = set_defaults(uc_mgr);
		if (err < 0)
			return err;
		seq = &verb->enable_list;
	} else {
		seq = &verb->disable_list;
	}
	err = execute_sequence(uc_mgr, seq,
			       &verb->value_list,
			       &uc_mgr->value_list,
			       NULL);
	if (enable && err >= 0)
		uc_mgr->active_verb = verb;
	return err;
}

/**
 * \brief Set modifier
 * \param uc_mgr Use case manager
 * \param modifier modifier to set
 * \param enable nonzero = enable, zero = disable
 * \return zero on success, otherwise a negative error code
 */
static int set_modifier(snd_use_case_mgr_t *uc_mgr,
			struct use_case_modifier *modifier,
			int enable)
{
	struct list_head *seq;
	int err;

	if (modifier_status(uc_mgr, modifier->name) == enable)
		return 0;

	if (enable) {
		seq = &modifier->enable_list;
	} else {
		seq = &modifier->disable_list;
	}
	err = execute_sequence(uc_mgr, seq,
			       &modifier->value_list,
			       &uc_mgr->active_verb->value_list,
			       &uc_mgr->value_list);
	if (enable && err >= 0) {
		list_add_tail(&modifier->active_list, &uc_mgr->active_modifiers);
	} else if (!enable) {
		list_del(&modifier->active_list);
	}
	return err;
}

/**
 * \brief Set device
 * \param uc_mgr Use case manager
 * \param device device to set
 * \param enable nonzero = enable, zero = disable
 * \return zero on success, otherwise a negative error code
 */
static int set_device(snd_use_case_mgr_t *uc_mgr,
		      struct use_case_device *device,
		      int enable)
{
	struct list_head *seq;
	int err;

	if (device_status(uc_mgr, device->name) == enable)
		return 0;

	if (enable) {
		seq = &device->enable_list;
	} else {
		seq = &device->disable_list;
	}
	err = execute_sequence(uc_mgr, seq,
			       &device->value_list,
			       &uc_mgr->active_verb->value_list,
			       &uc_mgr->value_list);
	if (enable && err >= 0) {
		list_add_tail(&device->active_list, &uc_mgr->active_devices);
	} else if (!enable) {
		list_del(&device->active_list);
	}
	return err;
}

/**
 * \brief Init sound card use case manager.
 * \param uc_mgr Returned use case manager pointer
 * \param card_name name of card to open
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_open(snd_use_case_mgr_t **uc_mgr,
			  const char *card_name)
{
	snd_use_case_mgr_t *mgr;
	int err;

	/* create a new UCM */
	mgr = calloc(1, sizeof(snd_use_case_mgr_t));
	if (mgr == NULL)
		return -ENOMEM;
	INIT_LIST_HEAD(&mgr->verb_list);
	INIT_LIST_HEAD(&mgr->fixedboot_list);
	INIT_LIST_HEAD(&mgr->boot_list);
	INIT_LIST_HEAD(&mgr->default_list);
	INIT_LIST_HEAD(&mgr->value_list);
	INIT_LIST_HEAD(&mgr->active_modifiers);
	INIT_LIST_HEAD(&mgr->active_devices);
	INIT_LIST_HEAD(&mgr->ctl_list);
	INIT_LIST_HEAD(&mgr->variable_list);
	pthread_mutex_init(&mgr->mutex, NULL);

	err = uc_mgr_card_open(mgr);
	if (err < 0) {
		uc_mgr_free(mgr);
		return err;
	}

	err = snd_config_top(&mgr->local_config);
	if (err < 0)
		goto _err;

	mgr->card_name = strdup(card_name);
	if (mgr->card_name == NULL) {
		err = -ENOMEM;
		goto _err;
	}

	/* get info on use_cases and verify against card */
	err = import_master_config(mgr);
	if (err < 0) {
		uc_error("error: failed to import %s use case configuration %d",
			 card_name, err);
		goto _err;
	}

	err = check_empty_configuration(mgr);
	if (err < 0) {
		uc_error("error: failed to import %s (empty configuration)", card_name);
		goto _err;
	}

	*uc_mgr = mgr;
	return 0;

_err:
	uc_mgr_card_close(mgr);
	uc_mgr_free(mgr);
	return err;
}

/**
 * \brief Reload and reparse all use case files.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_reload(snd_use_case_mgr_t *uc_mgr)
{
	int err;

	pthread_mutex_lock(&uc_mgr->mutex);

	uc_mgr_free_verb(uc_mgr);

	uc_mgr->default_list_executed = 0;

	/* reload all use cases */
	err = import_master_config(uc_mgr);
	if (err < 0) {
		uc_error("error: failed to reload use cases");
		pthread_mutex_unlock(&uc_mgr->mutex);
		return -EINVAL;
	}

	pthread_mutex_unlock(&uc_mgr->mutex);
	return err;
}

/**
 * \brief Close use case manager.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_close(snd_use_case_mgr_t *uc_mgr)
{
	uc_mgr_card_close(uc_mgr);
	uc_mgr_free(uc_mgr);

	return 0;
}

/*
 * Tear down current use case verb, device and modifier.
 */
static int dismantle_use_case(snd_use_case_mgr_t *uc_mgr)
{
	struct list_head *pos, *npos;
	struct use_case_modifier *modifier;
	struct use_case_device *device;
	int err;

	list_for_each_safe(pos, npos, &uc_mgr->active_modifiers) {
		modifier = list_entry(pos, struct use_case_modifier,
				      active_list);
		err = set_modifier(uc_mgr, modifier, 0);
		if (err < 0)
			uc_error("Unable to disable modifier %s", modifier->name);
	}
	INIT_LIST_HEAD(&uc_mgr->active_modifiers);

	list_for_each_safe(pos, npos, &uc_mgr->active_devices) {
		device = list_entry(pos, struct use_case_device,
				    active_list);
		err = set_device(uc_mgr, device, 0);
		if (err < 0)
			uc_error("Unable to disable device %s", device->name);
	}
	INIT_LIST_HEAD(&uc_mgr->active_devices);

	err = set_verb(uc_mgr, uc_mgr->active_verb, 0);
	if (err < 0) {
		uc_error("Unable to disable verb %s", uc_mgr->active_verb->name);
		return err;
	}
	uc_mgr->active_verb = NULL;

	err = execute_sequence(uc_mgr, &uc_mgr->default_list,
			       &uc_mgr->value_list, NULL, NULL);
	
	return err;
}

/**
 * \brief Reset sound card controls to default values.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_reset(snd_use_case_mgr_t *uc_mgr)
{
	int err;

	pthread_mutex_lock(&uc_mgr->mutex);
	err = execute_sequence(uc_mgr, &uc_mgr->default_list,
			       &uc_mgr->value_list, NULL, NULL);
	INIT_LIST_HEAD(&uc_mgr->active_modifiers);
	INIT_LIST_HEAD(&uc_mgr->active_devices);
	uc_mgr->active_verb = NULL;
	pthread_mutex_unlock(&uc_mgr->mutex);
	return err;
}

/**
 * \brief Get list of verbs in pair verbname+comment
 * \param list Returned list
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_verb_list(snd_use_case_mgr_t *uc_mgr, const char **list[])
{
	return get_list2(&uc_mgr->verb_list, list,
			 struct use_case_verb, list,
			 name, comment);
}

/**
 * \brief Get list of devices in pair devicename+comment
 * \param list Returned list
 * \param verbname For verb (NULL = current)
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_device_list(snd_use_case_mgr_t *uc_mgr, const char **list[],
			   char *verbname)
{
	struct use_case_verb *verb;

	if (verbname) {
		verb = find_verb(uc_mgr, verbname);
	} else {
		verb = uc_mgr->active_verb;
	}
	if (verb == NULL)
		return -ENOENT;
	return get_list2(&verb->device_list, list,
			 struct use_case_device, list,
			 name, comment);
}

/**
 * \brief Get list of modifiers in pair devicename+comment
 * \param list Returned list
 * \param verbname For verb (NULL = current)
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_modifier_list(snd_use_case_mgr_t *uc_mgr, const char **list[],
			     char *verbname)
{
	struct use_case_verb *verb;
	if (verbname) {
		verb = find_verb(uc_mgr, verbname);
	} else {
		verb = uc_mgr->active_verb;
	}
	if (verb == NULL)
		return -ENOENT;
	return get_list2(&verb->modifier_list, list,
			 struct use_case_modifier, list,
			 name, comment);
}

/**
 * \brief Get list of supported/conflicting devices
 * \param list Returned list
 * \param name Name of modifier or verb to query
 * \param type Type of device list entries to return
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_supcon_device_list(snd_use_case_mgr_t *uc_mgr,
				  const char **list[], char *name,
				  enum dev_list_type type)
{
	char *str;
	struct use_case_verb *verb;
	struct use_case_modifier *modifier;
	struct use_case_device *device;

	if (!name)
		return -ENOENT;

	str = strchr(name, '/');
	if (str) {
		*str = '\0';
		verb = find_verb(uc_mgr, str + 1);
	}
	else {
		verb = uc_mgr->active_verb;
	}
	if (!verb)
		return -ENOENT;

	modifier = find_modifier(uc_mgr, verb, name, 0);
	if (modifier) {
		if (modifier->dev_list.type != type) {
			*list = NULL;
			return 0;
		}
		return get_list(&modifier->dev_list.list, list,
				struct dev_list_node, list,
				name);
	}

	device = find_device(uc_mgr, verb, name, 0);
	if (device) {
		if (device->dev_list.type != type) {
			*list = NULL;
			return 0;
		}
		return get_list(&device->dev_list.list, list,
				struct dev_list_node, list,
				name);
	}

	return -ENOENT;
}

/**
 * \brief Get list of supported devices
 * \param list Returned list
 * \param name Name of verb or modifier to query
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_supported_device_list(snd_use_case_mgr_t *uc_mgr,
				     const char **list[], char *name)
{
	return get_supcon_device_list(uc_mgr, list, name, DEVLIST_SUPPORTED);
}

/**
 * \brief Get list of conflicting devices
 * \param list Returned list
 * \param name Name of verb or modifier to query
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_conflicting_device_list(snd_use_case_mgr_t *uc_mgr,
				       const char **list[], char *name)
{
	return get_supcon_device_list(uc_mgr, list, name, DEVLIST_CONFLICTING);
}

#ifndef DOC_HIDDEN
struct myvalue {
	struct list_head list;
	const char *text;
};
#endif

/**
 * \brief Convert myvalue list string list
 * \param list myvalue list
 * \param res string list
 * \retval Number of list entries if success, otherwise a negativer error code
 */
static int myvalue_to_str_list(struct list_head *list, char ***res)
{
	struct list_head *pos;
	struct myvalue *value;
	char **p;
	int cnt;

	cnt = alloc_str_list(list, 1, res);
	if (cnt < 0)
		return cnt;
	p = *res;
	list_for_each(pos, list) {
		value = list_entry(pos, struct myvalue, list);
		*p = strdup(value->text);
		if (*p == NULL) {
			snd_use_case_free_list((const char **)p, cnt);
			return -ENOMEM;
		}
		p++;
	}
	return cnt;
}

/**
 * \brief Free myvalue list
 * \param list myvalue list
 */
static void myvalue_list_free(struct list_head *list)
{
	struct list_head *pos, *npos;
	struct myvalue *value;

	list_for_each_safe(pos, npos, list) {
		value = list_entry(pos, struct myvalue, list);
		list_del(&value->list);
		free(value);
	}
}

/**
 * \brief Merge one value to the myvalue list
 * \param list The list with values
 * \param value The value to be merged (without duplicates)
 * \return 1 if dup, 0 if success, otherwise a negative error code
 */
static int merge_value(struct list_head *list, const char *text)
{
	struct list_head *pos;
	struct myvalue *value;

	list_for_each(pos, list) {
		value = list_entry(pos, struct myvalue, list);
		if (strcmp(value->text, text) == 0)
			return 1;
	}
	value = malloc(sizeof(*value));
	if (value == NULL)
		return -ENOMEM;
	value->text = text;
	list_add_tail(&value->list, list);
	return 0;
}

/**
 * \brief Find all values for given identifier
 * \param list Returned list
 * \param source Source list with ucm_value structures
 * \return Zero if success, otherwise a negative error code
 */
static int add_identifiers(struct list_head *list,
			   struct list_head *source)
{
	struct ucm_value *v;
	struct list_head *pos;
	int err;

	list_for_each(pos, source) {
		v = list_entry(pos, struct ucm_value, list);
		err = merge_value(list, v->name);
		if (err < 0)
			return err;
	}
	return 0;
}

/**
 * \brief Find all values for given identifier
 * \param list Returned list
 * \param identifier Identifier
 * \param source Source list with ucm_value structures
 */
static int add_values(struct list_head *list,
		      const char *identifier,
		      struct list_head *source)
{
	struct ucm_value *v;
	struct list_head *pos;
	int err;

	list_for_each(pos, source) {
		v = list_entry(pos, struct ucm_value, list);
		if (check_identifier(identifier, v->name)) {
			err = merge_value(list, v->data);
			if (err < 0)
				return err;
		}
	}
	return 0;
}

/**
 * \brief compare two identifiers
 */
static int identifier_cmp(const void *_a, const void *_b)
{
	const char * const *a = _a;
	const char * const *b = _b;
	return strcmp(*a, *b);
}

/**
 * \brief Get list of available identifiers
 * \param list Returned list
 * \param name Name of verb or modifier to query
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_identifiers_list(snd_use_case_mgr_t *uc_mgr,
				const char **list[], char *name)
{
	struct use_case_verb *verb;
	struct use_case_modifier *modifier;
	struct use_case_device *device;
	struct list_head mylist;
	struct list_head *value_list;
	char *str, **res;
	int err;

	if (!name)
		return -ENOENT;

	str = strchr(name, '/');
	if (str) {
		*str = '\0';
		verb = find_verb(uc_mgr, str + 1);
	}
	else {
		verb = uc_mgr->active_verb;
	}
	if (!verb)
		return -ENOENT;

	value_list = NULL;
	modifier = find_modifier(uc_mgr, verb, name, 0);
	if (modifier) {
		value_list = &modifier->value_list;
	} else {
		device = find_device(uc_mgr, verb, name, 0);
		if (device)
			value_list = &device->value_list;
	}
	if (value_list == NULL)
		return -ENOENT;

	INIT_LIST_HEAD(&mylist);
	err = add_identifiers(&mylist, &uc_mgr->value_list);
	if (err < 0)
		goto __fail;
	err = add_identifiers(&mylist, &verb->value_list);
	if (err < 0)
		goto __fail;
	err = add_identifiers(&mylist, value_list);
	if (err < 0)
		goto __fail;
	err = myvalue_to_str_list(&mylist, &res);
	if (err > 0)
		*list = (const char **)res;
	else if (err == 0)
		*list = NULL;
__fail:
	myvalue_list_free(&mylist);
	if (err <= 0)
		return err;
	qsort(*list, err, sizeof(char *), identifier_cmp);
	return err;
}

/**
 * \brief Get list of values
 * \param list Returned list
 * \param verbname For verb (NULL = current)
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_value_list(snd_use_case_mgr_t *uc_mgr,
			  const char *identifier,
			  const char **list[],
			  char *verbname)
{
	struct list_head mylist, *pos;
	struct use_case_verb *verb;
	struct use_case_device *dev;
	struct use_case_modifier *mod;
	char **res;
	int err;

	if (verbname) {
		verb = find_verb(uc_mgr, verbname);
	} else {
		verb = uc_mgr->active_verb;
	}
	if (verb == NULL)
		return -ENOENT;
	INIT_LIST_HEAD(&mylist);
	err = add_values(&mylist, identifier, &uc_mgr->value_list);
	if (err < 0)
		goto __fail;
	err = add_values(&mylist, identifier, &verb->value_list);
	if (err < 0)
		goto __fail;
	list_for_each(pos, &verb->device_list) {
		dev = list_entry(pos, struct use_case_device, list);
		err = add_values(&mylist, identifier, &dev->value_list);
		if (err < 0)
			goto __fail;
	}
	list_for_each(pos, &verb->modifier_list) {
		mod = list_entry(pos, struct use_case_modifier, list);
		err = add_values(&mylist, identifier, &mod->value_list);
		if (err < 0)
			goto __fail;
	}
	err = myvalue_to_str_list(&mylist, &res);
	if (err > 0)
		*list = (const char **)res;
	else if (err == 0)
		*list = NULL;
      __fail:
	myvalue_list_free(&mylist);
	return err;
}

/**
 * \brief Get list of enabled devices
 * \param list Returned list
 * \param verbname For verb (NULL = current)
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_enabled_device_list(snd_use_case_mgr_t *uc_mgr,
				   const char **list[])
{
	if (uc_mgr->active_verb == NULL)
		return -EINVAL;
	return get_list(&uc_mgr->active_devices, list,
			struct use_case_device, active_list,
			name);
}

/**
 * \brief Get list of enabled modifiers
 * \param list Returned list
 * \param verbname For verb (NULL = current)
 * \return Number of list entries if success, otherwise a negative error code
 */
static int get_enabled_modifier_list(snd_use_case_mgr_t *uc_mgr,
				     const char **list[])
{
	if (uc_mgr->active_verb == NULL)
		return -EINVAL;
	return get_list(&uc_mgr->active_modifiers, list,
			struct use_case_modifier, active_list,
			name);
}

/**
 * \brief Obtain a list of entries
 * \param uc_mgr Use case manager (may be NULL - card list)
 * \param identifier (may be NULL - card list)
 * \param list Returned allocated list
 * \return Number of list entries if success, otherwise a negative error code
 */
int snd_use_case_get_list(snd_use_case_mgr_t *uc_mgr,
			  const char *identifier,
			  const char **list[])
{
	char *str, *str1;
	int err;

	if (uc_mgr == NULL || identifier == NULL)
		return uc_mgr_scan_master_configs(list);
	pthread_mutex_lock(&uc_mgr->mutex);
	if (strcmp(identifier, "_verbs") == 0)
		err = get_verb_list(uc_mgr, list);
	else if (strcmp(identifier, "_enadevs") == 0)
		err = get_enabled_device_list(uc_mgr, list);
	else if (strcmp(identifier, "_enamods") == 0)
		err = get_enabled_modifier_list(uc_mgr, list);
	else {
		str1 = strchr(identifier, '/');
		if (str1) {
			str = strdup(str1 + 1);
			if (str == NULL) {
				err = -ENOMEM;
				goto __end;
			}
		} else {
			str = NULL;
		}
		if (check_identifier(identifier, "_devices"))
			err = get_device_list(uc_mgr, list, str);
		else if (check_identifier(identifier, "_modifiers"))
			err = get_modifier_list(uc_mgr, list, str);
		else if (check_identifier(identifier, "_identifiers"))
			err = get_identifiers_list(uc_mgr, list, str);
		else if (check_identifier(identifier, "_supporteddevs"))
			err = get_supported_device_list(uc_mgr, list, str);
		else if (check_identifier(identifier, "_conflictingdevs"))
			err = get_conflicting_device_list(uc_mgr, list, str);
		else if (identifier[0] == '_')
			err = -ENOENT;
		else
			err = get_value_list(uc_mgr, identifier, list, str);
		if (str)
			free(str);
	}
      __end:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return err;
}

static int get_value1(snd_use_case_mgr_t *uc_mgr, char **value,
		      struct list_head *value_list, const char *identifier)
{
	struct ucm_value *val;
	struct list_head *pos;
	int err;

	if (!value_list)
		return -ENOENT;

	list_for_each(pos, value_list) {
		val = list_entry(pos, struct ucm_value, list);
		if (check_identifier(identifier, val->name)) {
			if (uc_mgr->conf_format < 2) {
				*value = strdup(val->data);
				if (*value == NULL)
					return -ENOMEM;
				return 0;
			}
			err = uc_mgr_get_substituted_value(uc_mgr, value, val->data);
			if (err < 0)
				return err;
			return rewrite_device_value(uc_mgr, val->name, value);
		}
	}
	return -ENOENT;
}

static int get_value3(snd_use_case_mgr_t *uc_mgr,
		      char **value,
		      const char *identifier,
		      struct list_head *value_list1,
		      struct list_head *value_list2,
		      struct list_head *value_list3)
{
	int err;

	err = get_value1(uc_mgr, value, value_list1, identifier);
	if (err >= 0 || err != -ENOENT)
		return err;
	err = get_value1(uc_mgr, value, value_list2, identifier);
	if (err >= 0 || err != -ENOENT)
		return err;
	err = get_value1(uc_mgr, value, value_list3, identifier);
	if (err >= 0 || err != -ENOENT)
		return err;
	return -ENOENT;
}

/**
 * \brief Get value
 * \param uc_mgr Use case manager
 * \param identifier Value identifier (string)
 * \param value Returned value string
 * \param item Modifier or Device name (string)
 * \return Zero on success (value is filled), otherwise a negative error code
 */
static int get_value(snd_use_case_mgr_t *uc_mgr,
			const char *identifier,
			char **value,
			const char *mod_dev_name,
			const char *verb_name,
			int exact)
{
	struct use_case_verb *verb;
	struct use_case_modifier *mod;
	struct use_case_device *dev;
	int err;

	if (mod_dev_name || verb_name || !exact) {
		if (verb_name && strlen(verb_name)) {
			verb = find_verb(uc_mgr, verb_name);
		} else {
			verb = uc_mgr->active_verb;
		}
		if (verb) {
			if (mod_dev_name) {
				mod = find_modifier(uc_mgr, verb,
						    mod_dev_name, 0);
				if (mod) {
					err = get_value1(uc_mgr, value,
							 &mod->value_list,
							 identifier);
					if (err >= 0 || err != -ENOENT)
						return err;
				}

				dev = find_device(uc_mgr, verb,
						  mod_dev_name, 0);
				if (dev) {
					err = get_value1(uc_mgr, value,
							 &dev->value_list,
							 identifier);
					if (err >= 0 || err != -ENOENT)
						return err;
				}

				if (exact)
					return -ENOENT;
			}

			err = get_value1(uc_mgr, value, &verb->value_list, identifier);
			if (err >= 0 || err != -ENOENT)
				return err;
		}

		if (exact)
			return -ENOENT;
	}

	err = get_value1(uc_mgr, value, &uc_mgr->value_list, identifier);
	if (err >= 0 || err != -ENOENT)
		return err;

	return -ENOENT;
}

/**
 * \brief Get private alsa-lib configuration (ASCII)
 * \param uc_mgr Use case manager
 * \param str Returned value string
 * \return Zero on success (value is filled), otherwise a negative error code
 */
static int get_alibcfg(snd_use_case_mgr_t *uc_mgr, char **str)
{
	snd_output_t *out;
	size_t size;
	int err;

	err = snd_output_buffer_open(&out);
	if (err < 0)
		return err;
	err = snd_config_save(uc_mgr->local_config, out);
	if (err >= 0) {
		size = snd_output_buffer_steal(out, str);
		if (*str)
			(*str)[size] = '\0';
	}
	snd_output_close(out);
	return 0;
}

/**
 * \brief Get device prefix for private alsa-lib configuration
 * \param uc_mgr Use case manager
 * \param str Returned value string
 * \return Zero on success (value is filled), otherwise a negative error code
 */
static int get_alibpref(snd_use_case_mgr_t *uc_mgr, char **str)
{
	const size_t l = 10;
	char *s;

	s = malloc(l);
	if (s == NULL)
		return -ENOMEM;
	snprintf(s, l, "_ucm%04X.", uc_mgr->ucm_card_number);
	*str = s;
	return 0;
}

/**
 * \brief Get current - string
 * \param uc_mgr Use case manager
 * \param identifier 
 * \param value Value pointer
 * \return Zero if success, otherwise a negative error code
 *
 * Note: String is dynamically allocated, use free() to
 * deallocate this string.
 */      
int snd_use_case_get(snd_use_case_mgr_t *uc_mgr,
		     const char *identifier,
		     const char **value)
{
	const char *slash1, *slash2, *mod_dev_after;
	const char *ident, *mod_dev, *verb;
	int exact = 0;
	int err;

	pthread_mutex_lock(&uc_mgr->mutex);
	if (identifier == NULL) {
		*value = strdup(uc_mgr->card_name);
		if (*value == NULL) {
			err = -ENOMEM;
			goto __end;
		}
		err = 0;
	} else if (strcmp(identifier, "_verb") == 0) {
		if (uc_mgr->active_verb == NULL) {
			err = -ENOENT;
			goto __end;
		}
		*value = strdup(uc_mgr->active_verb->name);
		if (*value == NULL) {
			err = -ENOMEM;
			goto __end;
		}
		err = 0;
	} else if (strcmp(identifier, "_file") == 0) {
		/* get the conf file name of the opened card */
		if ((uc_mgr->card_name == NULL) ||
		    (uc_mgr->conf_file_name == NULL) ||
		    (uc_mgr->conf_file_name[0] == '\0')) {
			err = -ENOENT;
			goto __end;
		}
		*value = strdup(uc_mgr->conf_file_name);
		if (*value == NULL) {
			err = -ENOMEM;
			goto __end;
		}
		err = 0;

	} else if (strcmp(identifier, "_alibcfg") == 0) {
		err = get_alibcfg(uc_mgr, (char **)value);
	} else if (strcmp(identifier, "_alibpref") == 0) {
		err = get_alibpref(uc_mgr, (char **)value);
	} else if (identifier[0] == '_') {
		err = -ENOENT;
	} else {
		if (identifier[0] == '=') {
			exact = 1;
			identifier++;
		}

		slash1 = strchr(identifier, '/');
		if (slash1) {
			ident = strndup(identifier, slash1 - identifier);

			slash2 = strchr(slash1 + 1, '/');
			if (slash2) {
				mod_dev_after = slash2;
				verb = slash2 + 1;
			}
			else {
				mod_dev_after = slash1 + strlen(slash1);
				verb = NULL;
			}

			if (mod_dev_after == slash1 + 1)
				mod_dev = NULL;
			else
				mod_dev = strndup(slash1 + 1,
						  mod_dev_after - (slash1 + 1));
		}
		else {
			ident = identifier;
			mod_dev = NULL;
			verb = NULL;
		}

		err = get_value(uc_mgr, ident, (char **)value, mod_dev, verb,
				exact);
		if (ident != identifier)
			free((void *)ident);
		if (mod_dev)
			free((void *)mod_dev);
	}
      __end:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return err;
}


/**
 * \brief Get current - integer
 * \param uc_mgr Use case manager
 * \param identifier 
 * \return Value if success, otherwise a negative error code 
 */
int snd_use_case_geti(snd_use_case_mgr_t *uc_mgr,
		      const char *identifier,
		      long *value)
{
	char *str, *str1;
	long err;

	pthread_mutex_lock(&uc_mgr->mutex);
	if (0) {
		/* nothing here - prepared for fixed identifiers */
	} else {
		str1 = strchr(identifier, '/');
		if (str1) {
			str = strdup(str1 + 1);
			if (str == NULL) {
				err = -ENOMEM;
				goto __end;
			}
		} else {
			str = NULL;
		}
		if (check_identifier(identifier, "_devstatus")) {
			if (!str) {
				err = -EINVAL;
				goto __end;
			}
			err = device_status(uc_mgr, str);
			if (err >= 0) {
				*value = err;
				err = 0;
			}
		} else if (check_identifier(identifier, "_modstatus")) {
			if (!str) {
				err = -EINVAL;
				goto __end;
			}
			err = modifier_status(uc_mgr, str);
			if (err >= 0) {
				*value = err;
				err = 0;
			}
#if 0
		/*
		 * enable this block if the else clause below is expanded to query
		 * user-supplied values
		 */
		} else if (identifier[0] == '_')
			err = -ENOENT;
#endif
		} else
			err = -ENOENT;
		if (str)
			free(str);
	}
      __end:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return err;
}

static int set_fixedboot_user(snd_use_case_mgr_t *uc_mgr,
			      const char *value)
{
	int err;

	if (value != NULL && *value) {
		uc_error("error: wrong value for _fboot (%s)", value);
		return -EINVAL;
	}
	if (list_empty(&uc_mgr->fixedboot_list))
		return -ENOENT;
	err = execute_sequence(uc_mgr, &uc_mgr->fixedboot_list,
			       &uc_mgr->value_list, NULL, NULL);
	if (err < 0) {
		uc_error("Unable to execute force boot sequence");
		return err;
	}
	return err;
}

static int set_boot_user(snd_use_case_mgr_t *uc_mgr,
			 const char *value)
{
	int err;

	if (value != NULL && *value) {
		uc_error("error: wrong value for _boot (%s)", value);
		return -EINVAL;
	}
	if (list_empty(&uc_mgr->boot_list))
		return -ENOENT;
	err = execute_sequence(uc_mgr, &uc_mgr->boot_list,
			       &uc_mgr->value_list, NULL, NULL);
	if (err < 0) {
		uc_error("Unable to execute boot sequence");
		return err;
	}
	return err;
}

static int set_defaults_user(snd_use_case_mgr_t *uc_mgr,
			     const char *value)
{
	if (value != NULL && *value) {
		uc_error("error: wrong value for _defaults (%s)", value);
		return -EINVAL;
	}
	return set_defaults(uc_mgr);
}

static int handle_transition_verb(snd_use_case_mgr_t *uc_mgr,
				  struct use_case_verb *new_verb)
{
	struct list_head *pos;
	struct transition_sequence *trans;
	int err;

	list_for_each(pos, &uc_mgr->active_verb->transition_list) {
		trans = list_entry(pos, struct transition_sequence, list);
		if (strcmp(trans->name, new_verb->name) == 0) {
			err = execute_sequence(uc_mgr, &trans->transition_list,
					       &uc_mgr->active_verb->value_list,
					       &uc_mgr->value_list,
					       NULL);
			if (err >= 0)
				return 1;
			return err;
		}
	}
	return 0;
}

static int set_verb_user(snd_use_case_mgr_t *uc_mgr,
			 const char *verb_name)
{
	struct use_case_verb *verb;
	int err = 0;

	if (uc_mgr->active_verb &&
	    strcmp(uc_mgr->active_verb->name, verb_name) == 0)
		return 0;
	if (strcmp(verb_name, SND_USE_CASE_VERB_INACTIVE) != 0) {
		verb = find_verb(uc_mgr, verb_name);
		if (verb == NULL)
			return -ENOENT;
	} else {
		verb = NULL;
	}
	if (uc_mgr->active_verb) {
		err = handle_transition_verb(uc_mgr, verb);
		if (err == 0) {
			err = dismantle_use_case(uc_mgr);
			if (err < 0)
				return err;
		} else if (err == 1) {
			uc_mgr->active_verb = verb;
			verb = NULL;
		} else {
			verb = NULL; /* show error */
		}
	}
	if (verb) {
		err = set_verb(uc_mgr, verb, 1);
		if (err < 0)
			uc_error("error: failed to initialize new use case: %s",
				 verb_name);
	}
	return err;
}


static int set_device_user(snd_use_case_mgr_t *uc_mgr,
			   const char *device_name,
			   int enable)
{
	struct use_case_device *device;

	if (uc_mgr->active_verb == NULL)
		return -ENOENT;
	device = find_device(uc_mgr, uc_mgr->active_verb, device_name, 1);
	if (device == NULL)
		return -ENOENT;
	return set_device(uc_mgr, device, enable);
}

static int set_modifier_user(snd_use_case_mgr_t *uc_mgr,
			     const char *modifier_name,
			     int enable)
{
	struct use_case_modifier *modifier;

	if (uc_mgr->active_verb == NULL)
		return -ENOENT;

	modifier = find_modifier(uc_mgr, uc_mgr->active_verb, modifier_name, 1);
	if (modifier == NULL)
		return -ENOENT;
	return set_modifier(uc_mgr, modifier, enable);
}

static int switch_device(snd_use_case_mgr_t *uc_mgr,
			 const char *old_device,
			 const char *new_device)
{
	struct use_case_device *xold, *xnew;
	struct transition_sequence *trans;
	struct list_head *pos;
	int err, seq_found = 0;

	if (uc_mgr->active_verb == NULL)
		return -ENOENT;
	if (device_status(uc_mgr, old_device) == 0) {
		uc_error("error: device %s not enabled", old_device);
		return -EINVAL;
	}
	if (device_status(uc_mgr, new_device) != 0) {
		uc_error("error: device %s already enabled", new_device);
		return -EINVAL;
	}
	xold = find_device(uc_mgr, uc_mgr->active_verb, old_device, 1);
	if (xold == NULL)
		return -ENOENT;
	list_del(&xold->active_list);
	xnew = find_device(uc_mgr, uc_mgr->active_verb, new_device, 1);
	list_add_tail(&xold->active_list, &uc_mgr->active_devices);
	if (xnew == NULL)
		return -ENOENT;
	err = 0;
	list_for_each(pos, &xold->transition_list) {
		trans = list_entry(pos, struct transition_sequence, list);
		if (strcmp(trans->name, new_device) == 0) {
			err = execute_sequence(uc_mgr, &trans->transition_list,
					       &xold->value_list,
					       &uc_mgr->active_verb->value_list,
					       &uc_mgr->value_list);
			if (err >= 0) {
				list_del(&xold->active_list);
				list_add_tail(&xnew->active_list, &uc_mgr->active_devices);
			}
			seq_found = 1;
			break;
		}
	}
	if (!seq_found) {
		err = set_device(uc_mgr, xold, 0);
		if (err < 0)
			return err;
		err = set_device(uc_mgr, xnew, 1);
		if (err < 0)
			return err;
	}
	return err;
}

static int switch_modifier(snd_use_case_mgr_t *uc_mgr,
			   const char *old_modifier,
			   const char *new_modifier)
{
	struct use_case_modifier *xold, *xnew;
	struct transition_sequence *trans;
	struct list_head *pos;
	int err, seq_found = 0;

	if (uc_mgr->active_verb == NULL)
		return -ENOENT;
	if (modifier_status(uc_mgr, old_modifier) == 0) {
		uc_error("error: modifier %s not enabled", old_modifier);
		return -EINVAL;
	}
	if (modifier_status(uc_mgr, new_modifier) != 0) {
		uc_error("error: modifier %s already enabled", new_modifier);
		return -EINVAL;
	}
	xold = find_modifier(uc_mgr, uc_mgr->active_verb, old_modifier, 1);
	if (xold == NULL)
		return -ENOENT;
	xnew = find_modifier(uc_mgr, uc_mgr->active_verb, new_modifier, 1);
	if (xnew == NULL)
		return -ENOENT;
	err = 0;
	list_for_each(pos, &xold->transition_list) {
		trans = list_entry(pos, struct transition_sequence, list);
		if (strcmp(trans->name, new_modifier) == 0) {
			err = execute_sequence(uc_mgr, &trans->transition_list,
					       &xold->value_list,
					       &uc_mgr->active_verb->value_list,
					       &uc_mgr->value_list);
			if (err >= 0) {
				list_del(&xold->active_list);
				list_add_tail(&xnew->active_list, &uc_mgr->active_modifiers);
			}
			seq_found = 1;
			break;
		}
	}
	if (!seq_found) {
		err = set_modifier(uc_mgr, xold, 0);
		if (err < 0)
			return err;
		err = set_modifier(uc_mgr, xnew, 1);
		if (err < 0)
			return err;
	}
	return err;
}

/**
 * \brief Set new
 * \param uc_mgr Use case manager
 * \param identifier
 * \param value Value
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_set(snd_use_case_mgr_t *uc_mgr,
		     const char *identifier,
		     const char *value)
{
	char *str, *str1;
	int err = 0;

	pthread_mutex_lock(&uc_mgr->mutex);
	if (strcmp(identifier, "_fboot") == 0)
		err = set_fixedboot_user(uc_mgr, value);
	else if (strcmp(identifier, "_boot") == 0)
		err = set_boot_user(uc_mgr, value);
	else if (strcmp(identifier, "_defaults") == 0)
		err = set_defaults_user(uc_mgr, value);
	else if (strcmp(identifier, "_verb") == 0)
		err = set_verb_user(uc_mgr, value);
	else if (strcmp(identifier, "_enadev") == 0)
		err = set_device_user(uc_mgr, value, 1);
	else if (strcmp(identifier, "_disdev") == 0)
		err = set_device_user(uc_mgr, value, 0);
	else if (strcmp(identifier, "_enamod") == 0)
		err = set_modifier_user(uc_mgr, value, 1);
	else if (strcmp(identifier, "_dismod") == 0)
		err = set_modifier_user(uc_mgr, value, 0);
	else {
		str1 = strchr(identifier, '/');
		if (str1) {
			str = strdup(str1 + 1);
			if (str == NULL) {
				err = -ENOMEM;
				goto __end;
			}
		} else {
			err = -EINVAL;
			goto __end;
		}
		if (check_identifier(identifier, "_swdev"))
			err = switch_device(uc_mgr, str, value);
		else if (check_identifier(identifier, "_swmod"))
			err = switch_modifier(uc_mgr, str, value);
		else
			err = -EINVAL;
		if (str)
			free(str);
	}
      __end:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return err;
}

/**
 * \brief Parse control element identifier
 * \param elem_id Element identifier
 * \param ucm_id Use case identifier
 * \param value String value to be parsed
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_parse_ctl_elem_id(snd_ctl_elem_id_t *dst,
				   const char *ucm_id,
				   const char *value)
{
	snd_ctl_elem_iface_t iface;
	int jack_control;

	jack_control = strcmp(ucm_id, "JackControl") == 0;
	if (!jack_control &&
	    strcmp(ucm_id, "PlaybackVolume") &&
	    strcmp(ucm_id, "PlaybackSwitch") &&
	    strcmp(ucm_id, "CaptureVolume") &&
	    strcmp(ucm_id, "CaptureSwitch"))
		return -EINVAL;
	snd_ctl_elem_id_clear(dst);
	if (strcasestr(ucm_id, "name="))
		return __snd_ctl_ascii_elem_id_parse(dst, value, NULL);
	iface = SND_CTL_ELEM_IFACE_MIXER;
	if (jack_control)
		iface = SND_CTL_ELEM_IFACE_CARD;
	snd_ctl_elem_id_set_interface(dst, iface);
	snd_ctl_elem_id_set_name(dst, value);
	return 0;
}

/**
 * \brief Parse mixer element identifier
 * \param dst Simple mixer element identifier
 * \param ucm_id Use case identifier
 * \param value String value to be parsed
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_parse_selem_id(snd_mixer_selem_id_t *dst,
				const char *ucm_id,
				const char *value)
{
#ifdef BUILD_MIXER
	if (strcmp(ucm_id, "PlaybackMixerId") == 0 ||
	    strcmp(ucm_id, "CaptureMixerId") == 0)
		return snd_mixer_selem_id_parse(dst, value);
#endif
	return -EINVAL;
}
