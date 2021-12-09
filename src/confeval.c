/**
 * \file confeval.c
 * \ingroup Configuration
 * \brief Configuration helper functions
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2021
 *
 * Configuration string evaluation.
 *
 * See the \ref confarg_math page for more details.
 */
/*
 *  Configuration string evaluation
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>,
 *			  Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "local.h"

typedef long long value_type_t;

static const char *_find_end_of_expression(const char *s, char begin, char end)
{
	int count = 1;
	while (*s) {
		if (*s == begin) {
			count++;
		} else if (*s == end) {
			count--;
			if (count == 0)
				return s + 1;
		}
		s++;
	}
	return NULL;
}

static int _parse_integer(value_type_t *val, const char **s)
{
	long long v;
	char *end;

	errno = 0;
	v = strtoll(*s, &end, 0);
	if (errno)
		return -errno;
	*val = v;
	if (((long long)*val) != v)
		return -ERANGE;
	*s = end;
	return 0;
}

static int _to_integer(value_type_t *val, snd_config_t *c)
{
	int err;

	switch(snd_config_get_type(c)) {
	case SND_CONFIG_TYPE_INTEGER:
		{
			long v;
			err = snd_config_get_integer(c, &v);
			if (err >= 0)
				*val = v;
		}
		break;
	case SND_CONFIG_TYPE_INTEGER64:
		{
			long long v;
			err = snd_config_get_integer64(c, &v);
			if (err >= 0) {
				*val = v;
				if (((long long)*val) != v)
					return -ERANGE;
				return 0;
			}
		}
		break;
	case SND_CONFIG_TYPE_STRING:
		{
			const char *s;
			long long v;
			err = snd_config_get_string(c, &s);
			if (err >= 0) {
				err = safe_strtoll(s, &v);
				if (err >= 0) {
					*val = v;
					if (((long long)*val) != v)
						return -ERANGE;
					return 0;
				}
			}
		}
		break;
	default:
		return -EINVAL;
	}
	return err;
}

int _snd_eval_string(snd_config_t **dst, const char *s,
		     snd_config_expand_fcn_t fcn, void *private_data)
{
	snd_config_t *tmp;
	const char *save, *e;
	char *m;
	value_type_t left, right;
	int err, c, op, off;
	enum {
		LEFT,
		OP,
		RIGHT,
		END
	} pos;

	while (*s && *s <= ' ') s++;
	save = s;
	pos = LEFT;
	op = 0;
	while (*s) {
		while (*s && *s <= ' ') s++;
		c = *s;
		if (c == '\0')
			break;
		if (pos == END) {
			SNDERR("unexpected expression tail '%s'", s);
			return -EINVAL;
		}
		if (pos == OP) {
			switch (c) {
				case '+':
				case '-':
				case '*':
				case '/':
				case '%':
				case '|':
				case '&': op = c; break;
				default:
					SNDERR("unknown operation '%c'", c);
					return -EINVAL;
			}
			pos = RIGHT;
			s++;
			continue;
		}
		if (c == '(') {
			e = _find_end_of_expression(s + 1, '(', ')');
			off = 1;
			goto _expr;
		} else if (c == '$') {
			if (s[1] == '[') {
				e = _find_end_of_expression(s + 2, '[', ']');
				off = 2;
  _expr:
				if (e == NULL)
					return -EINVAL;
				m = malloc(e - s - (off - 1));
				if (m == NULL)
					return -ENOMEM;
				memcpy(m, s + off, e - s - off);
				m[e - s - (off + 1)] = '\0';
				err = _snd_eval_string(&tmp, m, fcn, private_data);
				free(m);
				if (err < 0)
					return err;
				s = e;
				if (*s)
					s++;
			} else {
				e = s + 1;
				while (*e) {
					if (!isalnum(*e) && *e != '_')
						break;
					e++;
				}
				m = malloc(e - s);
				if (m == NULL)
					return -ENOMEM;
				memcpy(m, s + 1, e - s - 1);
				m[e - s - 1] = '\0';
				err = fcn(&tmp, m, private_data);
				free(m);
				if (err < 0)
					return err;
				if (tmp == NULL) {
					err = snd_config_imake_integer(&tmp, NULL, 0);
					if (err < 0)
						return err;
				}
				s = e;
			}
			err = _to_integer(op == LEFT ? &left : &right, tmp);
			snd_config_delete(tmp);
		} else if (c == '-' || (c >= '0' && c <= '9')) {
			err = _parse_integer(op == LEFT ? &left : &right, &s);
		} else {
			return -EINVAL;
		}
		if (err < 0)
			return err;
		pos = op == LEFT ? OP : END;
	}
	if (pos != OP && pos != END) {
		SNDERR("incomplete expression '%s'", save);
		return -EINVAL;
	}

	if (pos == END) {
		switch (op) {
		case '+': left = left + right; break;
		case '-': left = left - right; break;
		case '*': left = left * right; break;
		case '/': left = left / right; break;
		case '%': left = left % right; break;
		case '|': left = left | right; break;
		case '&': left = left & right; break;
		default: return -EINVAL;
		}
	}

	if (left > INT_MAX || left < INT_MIN)
		return snd_config_imake_integer64(dst, NULL, left);
	else
		return snd_config_imake_integer(dst, NULL, left);
}

/**
 * \brief Evaluate an math expression in the string
 * \param[out] dst The function puts the handle to the new configuration
 *                 node at the address specified by \a dst.
 * \param[in] s A string to evaluate
 * \param[in] fcn A function to get the variable contents
 * \param[in] private_value A private value for the variable contents function
 * \return 0 if successful, otherwise a negative error code.
 */
int snd_config_evaluate_string(snd_config_t **dst, const char *s,
			       snd_config_expand_fcn_t fcn, void *private_data)
{
	assert(dst && s);
	int err;

	if (*s != '$')
		return -EINVAL;
	if (s[1] == '[') {
		err = _snd_eval_string(dst, s, fcn, private_data);
		if (err < 0)
			SNDERR("wrong expression '%s'", s);
	} else {
		err = fcn(dst, s + 1, private_data);
	}
	return err;
}
