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
#include <ctype.h>
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

static char *extract_substring(const char *data, regmatch_t *match)
{
	char *s;
	size_t len;

	len = match->rm_eo - match->rm_so;
	s = malloc(len + 1);
	if (s == NULL)
		return NULL;
	memcpy(s, data + match->rm_so, len);
	s[len] = '\0';
	return s;
}

static int set_variables(snd_use_case_mgr_t *uc_mgr, const char *data,
			 unsigned int match_size, regmatch_t *match,
			 const char *name)
{
	size_t name2_len = strlen(name) + 16;
	char *name2 = alloca(name2_len);
	char *s;
	unsigned int i;
	int err;

	if (match[0].rm_so < 0 || match[0].rm_eo < 0)
		return 0;
	s = extract_substring(data, &match[0]);
	if (s == NULL)
		return -ENOMEM;
	err = uc_mgr_set_variable(uc_mgr, name, s);
	free(s);
	if (err < 0)
		return err;
	for (i = 1; i < match_size; i++) {
		if (match[0].rm_so < 0 || match[0].rm_eo < 0)
			return 0;
		s = extract_substring(data, &match[i]);
		if (s == NULL)
			return -ENOMEM;
		snprintf(name2, name2_len, "%s%u", name, i);
		err = uc_mgr_set_variable(uc_mgr, name2, s);
		free(s);
		if (err < 0)
			return err;
	}
	return 0;
}

int uc_mgr_define_regex(snd_use_case_mgr_t *uc_mgr, const char *name,
			snd_config_t *eval)
{
	const char *string, *regex_string, *flags_string;
	char *s;
	regex_t re;
	int options = 0;
	regmatch_t match[20];
	int err;

	if (uc_mgr->conf_format < 3) {
		uc_error("define regex is supported in v3+ syntax");
		return -EINVAL;
	}

	if (snd_config_get_type(eval) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for DefineRegex");
		return -EINVAL;
	}

	err = get_string(eval, "String", &string);
	if (err < 0) {
		uc_error("DefineRegex error (String)");
		return -EINVAL;
	}

	err = get_string(eval, "Regex", &regex_string);
	if (err < 0) {
		uc_error("DefineRegex error (Regex string)");
		return -EINVAL;
	}

	err = get_string(eval, "Flags", &flags_string);
	if (err == -ENOENT) {
		options = REG_EXTENDED;
	} else if (err < 0) {
		uc_error("DefineRegex error (Flags string)");
		return -EINVAL;
	} else {
		while (*flags_string) {
			switch (tolower(*flags_string)) {
			case 'e':
				options |= REG_EXTENDED;
				break;
			case 'i':
				options |= REG_ICASE;
				break;
			case 's':
				options |= REG_NOSUB;
				break;
			case 'n':
				options |= REG_NEWLINE;
				break;
			default:
				uc_error("DefineRegex error (unknown flag '%c')", *flags_string);
				return -EINVAL;
			}
			flags_string++;
		}
	}

	err = uc_mgr_get_substituted_value(uc_mgr, &s, regex_string);
	if (err < 0)
		return err;
	err = regcomp(&re, s, options);
	free(s);
	if (err) {
		uc_error("Regex '%s' compilation failed (code %d)", err);
		return -EINVAL;
	}

	err = uc_mgr_get_substituted_value(uc_mgr, &s, string);
	if (err < 0) {
		regfree(&re);
		return err;
	}
	err = regexec(&re, s, ARRAY_SIZE(match), match, 0);
	if (err < 0)
		err = -errno;
	else if (err == REG_NOMATCH)
		err = 0;
	else
		err = set_variables(uc_mgr, s, ARRAY_SIZE(match), match, name);
	free(s);
	regfree(&re);
	return err;
}
