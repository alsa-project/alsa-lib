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
#include <regex.h>

static int get_string(snd_config_t *compound, const char *key, const char **str)
{
	snd_config_t *node;
	int err;

	err = snd_config_search(compound, key, &node);
	if (err < 0)
		return err;
	return snd_config_get_string(node, str);
}

static int if_eval_string(snd_use_case_mgr_t *uc_mgr, snd_config_t *eval)
{
	const char *string1 = NULL, *string2 = NULL;
	char *s1, *s2;
	int err;

	if (uc_mgr->conf_format >= 3) {
		err = get_string(eval, "Empty", &string1);
		if (err < 0 && err != -ENOENT) {
			uc_error("String error (If.Condition.Empty)");
			return -EINVAL;
		}

		if (string1) {
			err = uc_mgr_get_substituted_value(uc_mgr, &s1, string1);
			if (err < 0)
				return err;
			err = s1 == NULL || s1[0] == '\0';
			free(s1);
			return err;
		}
	}

	err = get_string(eval, "String1", &string1);
	if (err < 0 && err != -ENOENT) {
		uc_error("String error (If.Condition.String1)");
		return -EINVAL;
	}

	err = get_string(eval, "String2", &string2);
	if (err < 0 && err != -ENOENT) {
		uc_error("String error (If.Condition.String2)");
		return -EINVAL;
	}

	if (string1 || string2) {
		if (string1 == NULL) {
			uc_error("If.Condition.String1 not defined");
			return -EINVAL;
		}
		if (string2 == NULL) {
			uc_error("If.Condition.String2 not defined");
			return -EINVAL;
		}
		err = uc_mgr_get_substituted_value(uc_mgr, &s1, string1);
		if (err < 0)
			return err;
		err = uc_mgr_get_substituted_value(uc_mgr, &s2, string2);
		if (err < 0) {
			free(s1);
			return err;
		}
		err = strcasecmp(s1, s2) == 0;
		free(s2);
		free(s1);
		return err;
	}

	err = get_string(eval, "Haystack", &string1);
	if (err < 0 && err != -ENOENT) {
		uc_error("String error (If.Condition.Haystack)");
		return -EINVAL;
	}

	err = get_string(eval, "Needle", &string2);
	if (err < 0 && err != -ENOENT) {
		uc_error("String error (If.Condition.Needle)");
		return -EINVAL;
	}

	if (string1 || string2) {
		if (string1 == NULL) {
			uc_error("If.Condition.Haystack not defined");
			return -EINVAL;
		}
		if (string2 == NULL) {
			uc_error("If.Condition.Needle not defined");
			return -EINVAL;
		}
		err = uc_mgr_get_substituted_value(uc_mgr, &s1, string1);
		if (err < 0)
			return err;
		err = uc_mgr_get_substituted_value(uc_mgr, &s2, string2);
		if (err < 0) {
			free(s1);
			return err;
		}
		err = strstr(s1, s2) != NULL;
		free(s2);
		free(s1);
		return err;
	}

	uc_error("Unknown String condition arguments");
	return -EINVAL;
}

static int if_eval_regex_match(snd_use_case_mgr_t *uc_mgr, snd_config_t *eval)
{
	const char *string, *regex_string;
	char *s;
	regex_t re;
	int options = REG_EXTENDED | REG_ICASE;
	regmatch_t match[1];
	int err;

	err = get_string(eval, "String", &string);
	if (err < 0) {
		uc_error("RegexMatch error (If.Condition.String)");
		return -EINVAL;
	}

	err = get_string(eval, "Regex", &regex_string);
	if (err < 0) {
		uc_error("RegexMatch error (If.Condition.Regex)");
		return -EINVAL;
	}

	err = uc_mgr_get_substituted_value(uc_mgr, &s, regex_string);
	if (err < 0)
		return err;
	err = regcomp(&re, s, options);
	if (err) {
		uc_error("Regex '%s' compilation failed (code %d)", s, err);
		free(s);
		return -EINVAL;
	}
	free(s);

	err = uc_mgr_get_substituted_value(uc_mgr, &s, string);
	if (err < 0) {
		regfree(&re);
		return err;
	}
	err = regexec(&re, s, ARRAY_SIZE(match), match, 0);
	free(s);
	regfree(&re);
	return err == 0;
}

static int if_eval_control_exists(snd_use_case_mgr_t *uc_mgr, snd_config_t *eval)
{
	snd_ctl_t *ctl;
	struct ctl_list *ctl_list;
	const char *device = NULL, *ctldef, *enumval = NULL, *name;
	snd_ctl_elem_id_t *elem_id;
	snd_ctl_elem_info_t *elem_info;
	snd_ctl_elem_type_t type;
	char *s;
	int err, i, items;

	snd_ctl_elem_id_alloca(&elem_id);
	snd_ctl_elem_info_alloca(&elem_info);

	err = get_string(eval, "Device", &device);
	if (err < 0 && err != -ENOENT) {
		uc_error("ControlExists error (If.Condition.Device)");
		return -EINVAL;
	}

	err = get_string(eval, "Control", &ctldef);
	if (err < 0) {
		uc_error("ControlExists error (If.Condition.Control)");
		return -EINVAL;
	}

	err = get_string(eval, "ControlEnum", &enumval);
	if (err < 0 && err != -ENOENT) {
		uc_error("ControlExists error (If.Condition.ControlEnum)");
		return -EINVAL;
	}

	err = uc_mgr_get_substituted_value(uc_mgr, &s, ctldef);
	if (err < 0)
		return err;
	err = snd_ctl_ascii_elem_id_parse(elem_id, s);
	free(s);
	if (err < 0) {
		uc_error("unable to parse element identificator (%s)", ctldef);
		return -EINVAL;
	}

	if (device == NULL) {
		ctl = uc_mgr_get_ctl(uc_mgr);
		if (ctl == NULL) {
			uc_error("cannot determine control device");
			return -EINVAL;
		}
	} else {
		err = uc_mgr_get_substituted_value(uc_mgr, &s, device);
		if (err < 0)
			return err;
		err = uc_mgr_open_ctl(uc_mgr, &ctl_list, s, 1);
		free(s);
		if (err < 0)
			return err;
		ctl = ctl_list->ctl;
	}

	snd_ctl_elem_info_set_id(elem_info, elem_id);
	err = snd_ctl_elem_info(ctl, elem_info);
	if (err < 0)
		return 0;

	if (enumval) {
		type = snd_ctl_elem_info_get_type(elem_info);
		if (type != SND_CTL_ELEM_TYPE_ENUMERATED)
			return 0;
		err = uc_mgr_get_substituted_value(uc_mgr, &s, enumval);
		if (err < 0)
			return err;
		items = snd_ctl_elem_info_get_items(elem_info);
		for (i = 0; i < items; i++) {
			snd_ctl_elem_info_set_item(elem_info, i);
			err = snd_ctl_elem_info(ctl, elem_info);
			if (err < 0) {
				free(s);
				return err;
			}
			name = snd_ctl_elem_info_get_item_name(elem_info);
			if (strcasecmp(name, s) == 0) {
				free(s);
				return 1;
			}
		}
		free(s);
		return 0;
	}

	return 1;
}

static int if_eval_path(snd_use_case_mgr_t *uc_mgr, snd_config_t *eval)
{
	const char *path, *mode = "";
	int err, amode = F_OK;

	if (uc_mgr->conf_format < 4) {
		uc_error("Path condition is supported in v4+ syntax");
		return -EINVAL;
	}

	err = get_string(eval, "Path", &path);
	if (err < 0) {
		uc_error("Path error (If.Condition.Path)");
		return -EINVAL;
	}

	err = get_string(eval, "Mode", &mode);
	if (err < 0 && err != -ENOENT) {
		uc_error("Path error (If.Condition.Mode)");
		return -EINVAL;
	}

	if (strncasecmp(mode, "exist", 5) == 0) {
		amode = F_OK;
	} else if (strcasecmp(mode, "read") == 0) {
		amode = R_OK;
	} else if (strcasecmp(mode, "write") == 0) {
		amode = W_OK;
	} else if (strcasecmp(mode, "exec") == 0) {
		amode = X_OK;
	} else {
		uc_error("Path unknown mode (If.Condition.Mode)");
		return -EINVAL;
	}

#ifdef HAVE_EACCESS
	if (eaccess(path, amode))
#else
	if (access(path, amode))
#endif
		return 0;

	return 1;
}

static int if_eval(snd_use_case_mgr_t *uc_mgr, snd_config_t *eval)
{
	const char *type;
	int err;

	if (snd_config_get_type(eval) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for If.Condition");
		return -EINVAL;
	}

	err = get_string(eval, "Type", &type);
	if (err < 0) {
		uc_error("type block error (If.Condition)");
		return -EINVAL;
	}

	if (strcmp(type, "AlwaysTrue") == 0)
		return 1;

	if (strcmp(type, "String") == 0)
		return if_eval_string(uc_mgr, eval);

	if (strcmp(type, "ControlExists") == 0)
		return if_eval_control_exists(uc_mgr, eval);

	if (strcmp(type, "RegexMatch") == 0)
		return if_eval_regex_match(uc_mgr, eval);

	if (strcmp(type, "Path") == 0)
		return if_eval_path(uc_mgr, eval);

	uc_error("unknown If.Condition.Type");
	return -EINVAL;
}

static int if_eval_one(snd_use_case_mgr_t *uc_mgr,
		       snd_config_t *cond,
		       snd_config_t **result,
		       snd_config_t **before,
		       snd_config_t **after)
{
	snd_config_t *expr, *_true = NULL, *_false = NULL;
	int err;

	*result = NULL;

	if (snd_config_get_type(cond) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for If.1");
		return -EINVAL;
	}

	if (snd_config_search(cond, "Condition", &expr) < 0) {
		uc_error("condition block expected (If)");
		return -EINVAL;
	}

	err = snd_config_search(cond, "True", &_true);
	if (err < 0 && err != -ENOENT) {
		uc_error("true block error (If)");
		return -EINVAL;
	}

	err = snd_config_search(cond, "False", &_false);
	if (err < 0 && err != -ENOENT) {
		uc_error("false block error (If)");
		return -EINVAL;
	}

	err = snd_config_search(cond, "Before", before);
	if (err < 0 && err != -ENOENT) {
		uc_error("before block identifier error");
		return -EINVAL;
	}

	err = snd_config_search(cond, "After", after);
	if (err < 0 && err != -ENOENT) {
		uc_error("before block identifier error");
		return -EINVAL;
	}

	err = if_eval(uc_mgr, expr);
	if (err > 0) {
		*result = _true;
		return 0;
	} else if (err == 0) {
		*result = _false;
		return 0;
	} else {
		return err;
	}
}

#if 0
static void config_dump(snd_config_t *cfg)
{
	snd_output_t *out;
	snd_output_stdio_attach(&out, stderr, 0);
	snd_output_printf(out, "-----\n");
	snd_config_save(cfg, out);
	snd_output_close(out);
}
#endif

/*
 * put back the result from all conditions to the parent
 */
int uc_mgr_evaluate_condition(snd_use_case_mgr_t *uc_mgr,
			      snd_config_t *parent,
			      snd_config_t *cond)
{
	snd_config_iterator_t i, next;
	snd_config_t *a, *n, *before, *after;
	int err;

	if (uc_mgr->conf_format < 2) {
		uc_error("conditions are not supported for v1 syntax");
		return -EINVAL;
	}

	if (snd_config_get_type(cond) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for If");
		return -EINVAL;
	}

	snd_config_for_each(i, next, cond) {
		n = snd_config_iterator_entry(i);
		before = after = NULL;
		err = if_eval_one(uc_mgr, n, &a, &before, &after);
		if (err < 0)
			return err;
		if (a == NULL)
			continue;
		err = uc_mgr_evaluate_inplace(uc_mgr, a);
		if (err < 0)
			return err;
		err = uc_mgr_config_tree_merge(uc_mgr, parent, a, before, after);
		if (err < 0)
			return err;
		snd_config_delete(a);
	}
	return 0;
}
