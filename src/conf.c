/**
 * \file conf.c
 * \brief Configuration helper functions
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2000-2001
 *
 * Generic stdio-like input interface
 */
/*
 *  Configuration helper functions
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

#include <stdarg.h>
#include <wordexp.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "local.h"

#ifndef DOC_HIDDEN

struct _snd_config {
	char *id;
	snd_config_type_t type;
	union {
		long integer;
		char *string;
		double real;
		struct {
			struct list_head fields;
			int join;
		} compound;
	} u;
	struct list_head list;
	snd_config_t *father;
};

struct filedesc {
	char *name;
	snd_input_t *in;
	unsigned int line, column;
	struct filedesc *next;
};

#define LOCAL_ERROR			(-0x68000000)

#define LOCAL_UNTERMINATED_STRING 	(LOCAL_ERROR - 0)
#define LOCAL_UNTERMINATED_QUOTE	(LOCAL_ERROR - 1)
#define LOCAL_UNEXPECTED_CHAR		(LOCAL_ERROR - 2)
#define LOCAL_UNEXPECTED_EOF		(LOCAL_ERROR - 3)

typedef struct {
	struct filedesc *current;
	int unget;
	int ch;
} input_t;

int safe_strtol(const char *str, long *val)
{
	char *end;
	long v;
	if (!*str)
		return -EINVAL;
	errno = 0;
	v = strtol(str, &end, 0);
	if (errno)
		return -errno;
	if (*end)
		return -EINVAL;
	*val = v;
	return 0;
}

static int safe_strtod(const char *str, double *val)
{
	char *end;
	double v;
	if (!*str)
		return -EINVAL;
	errno = 0;
	v = strtod(str, &end);
	if (errno)
		return -errno;
	if (*end)
		return -EINVAL;
	*val = v;
	return 0;
}

static int get_char(input_t *input)
{
	int c;
	struct filedesc *fd;
	if (input->unget) {
		input->unget = 0;
		return input->ch;
	}
 again:
	fd = input->current;
	c = snd_input_getc(fd->in);
	switch (c) {
	case '\n':
		fd->column = 0;
		fd->line++;
		break;
	case '\t':
		fd->column += 8 - fd->column % 8;
		break;
	case EOF:
		if (fd->next) {
			snd_input_close(fd->in);
			free(fd->name);
			input->current = fd->next;
			free(fd);
			goto again;
		}
		return LOCAL_UNEXPECTED_EOF;
	default:
		fd->column++;
		break;
	}
	return (unsigned char)c;
}

static void unget_char(int c, input_t *input)
{
	assert(!input->unget);
	input->ch = c;
	input->unget = 1;
}

static int get_delimstring(char **string, int delim, input_t *input);

static int get_char_skip_comments(input_t *input)
{
	int c;
	while (1) {
		c = get_char(input);
		if (c == '<') {
			char *str;
			snd_input_t *in;
			struct filedesc *fd;
			int err = get_delimstring(&str, '>', input);
			if (err < 0)
				return err;
			if (!strncmp(str, "confdir:", 8)) {
				char *tmp = malloc(strlen(DATADIR "/alsa") + 1 + strlen(str + 8) + 1);
				if (tmp == NULL) {
					free(str);
					return -ENOMEM;
				}
				sprintf(tmp, DATADIR "/alsa/%s", str + 8);
				free(str);
				str = tmp;
			}
			err = snd_input_stdio_open(&in, str, "r");
			if (err < 0) {
				free(str);
				return err;
			}
			fd = malloc(sizeof(*fd));
			if (!fd)
				return -ENOMEM;
			fd->name = str;
			fd->in = in;
			fd->next = input->current;
			fd->line = 1;
			fd->column = 0;
			input->current = fd;
			continue;
		}
		if (c != '#')
			break;
		while (1) {
			c = get_char(input);
			if (c == EOF)
				return LOCAL_UNEXPECTED_EOF;
			if (c == '\n')
				break;
		}
	}
		
	return c;
}
			

static int get_nonwhite(input_t *input)
{
	int c;
	while (1) {
		c = get_char_skip_comments(input);
		switch (c) {
		case ' ':
		case '\f':
		case '\t':
		case '\n':
		case '\r':
			break;
		default:
			return c;
		}
	}
}

static int get_quotedchar(input_t *input)
{
	int c;
	c = get_char(input);
	switch (c) {
	case 'n':
		return '\n';
	case 't':
		return '\t';
	case 'v':
		return '\v';
	case 'b':
		return '\b';
	case 'r':
		return '\r';
	case 'f':
		return '\f';
	case '0' ... '7':
	{
		int num = c - '0';
		int i = 1;
		do {
			c = get_char(input);
			if (c < '0' || c > '7') {
				unget_char(c, input);
				break;
			}
			num = num * 8 + c - '0';
			i++;
		} while (i < 3);
		return num;
	}
	default:
		return c;
	}
}

static int get_freestring(char **string, int id, input_t *input)
{
	const size_t bufsize = 64;
	char _buf[bufsize];
	char *buf = _buf;
	size_t alloc = bufsize;
	size_t idx = 0;
	int c;
	while (1) {
		c = get_char(input);
		if (c < 0) {
			if (buf != _buf)
				free(buf);
			return c;
		}
		switch (c) {
		case '.':
			if (!id)
				break;
		case ' ':
		case '\f':
		case '\t':
		case '\n':
		case '\r':
		case '=':
		case ',':
		case ';':
		case '{':
		case '}':
		case '[':
		case ']':
		case '\'':
		case '"':
		case '\\':
		case '#':
		{
			char *s = malloc(idx + 1);
			if (!s)
				return -ENOMEM;
			unget_char(c, input);
			memcpy(s, buf, idx);
			s[idx] = '\0';
			*string = s;
			if (alloc > bufsize)
				free(buf);
			return 0;
		}
		default:
			break;
		}
		if (idx >= alloc) {
			size_t old_alloc = alloc;
			alloc *= 2;
			if (old_alloc == bufsize) {
				buf = malloc(alloc);
				if (buf == NULL)
					return -ENOMEM;
				memcpy(buf, _buf, old_alloc);
			} else {
				char *ptr = realloc(buf, alloc);
				if (ptr == NULL) {
					free(buf);
					return -ENOMEM;
				}
				buf = ptr;
			}
		}
		buf[idx++] = c;
	}
	return 0;
}
			
static int get_delimstring(char **string, int delim, input_t *input)
{
	const size_t bufsize = 64;
	char _buf[bufsize];
	char *buf = _buf;
	size_t alloc = bufsize;
	size_t idx = 0;
	int c;
	while (1) {
		c = get_char(input);
		if (c < 0)
			return c;
		switch (c) {
		case '\\':
			c = get_quotedchar(input);
			if (c < 0)
				return c;
			break;
		default:
			if (c == delim) {
				char *s = malloc(idx + 1);
				if (!s)
					return -ENOMEM;
				memcpy(s, buf, idx);
				s[idx] = '\0';
				*string = s;
				if (alloc > bufsize)
					free(buf);
				return 0;
			}
		}
		if (idx >= alloc) {
			size_t old_alloc = alloc;
			alloc *= 2;
			if (old_alloc == bufsize) {
				buf = malloc(alloc);
				if (buf == NULL)
					return -ENOMEM;
				memcpy(buf, _buf, old_alloc);
			} else {
				char *ptr = realloc(buf, alloc);
				if (ptr == NULL) {
					free(buf);
					return -ENOMEM;
				}
				buf = ptr;
			}
		}
		buf[idx++] = c;
	}
}

/* Return 0 for free string, 1 for delimited string */
static int get_string(char **string, int id, input_t *input)
{
	int c = get_nonwhite(input), err;
	if (c < 0)
		return c;
	switch (c) {
	case '=':
	case ',':
	case ';':
	case '.':
	case '{':
	case '}':
	case '[':
	case ']':
		return LOCAL_UNEXPECTED_CHAR;
	case '\'':
	case '"':
		err = get_delimstring(string, c, input);
		if (err < 0)
			return err;
		return 1;
	default:
		unget_char(c, input);
		err = get_freestring(string, id, input);
		if (err < 0)
			return err;
		return 0;
	}
}

static int _snd_config_make(snd_config_t **config, char **id, snd_config_type_t type)
{
	snd_config_t *n;
	assert(config);
	n = calloc(1, sizeof(*n));
	if (n == NULL) {
		if (*id) {
			free(*id);
			*id = NULL;
		}
		return -ENOMEM;
	}
	if (id) {
		n->id = *id;
		*id = NULL;
	}
	n->type = type;
	if (type == SND_CONFIG_TYPE_COMPOUND)
		INIT_LIST_HEAD(&n->u.compound.fields);
	*config = n;
	return 0;
}
	

static int _snd_config_make_add(snd_config_t **config, char **id,
				snd_config_type_t type, snd_config_t *father)
{
	snd_config_t *n;
	int err;
	assert(father->type == SND_CONFIG_TYPE_COMPOUND);
	err = _snd_config_make(&n, id, type);
	if (err < 0)
		return err;
	n->father = father;
	list_add_tail(&n->list, &father->u.compound.fields);
	*config = n;
	return 0;
}

static int _snd_config_search(snd_config_t *config, 
			      const char *id, int len, snd_config_t **result)
{
	snd_config_iterator_t i, next;
	snd_config_for_each(i, next, config) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (len < 0) {
			if (strcmp(n->id, id) != 0)
				continue;
		} else if (strlen(n->id) != (size_t) len ||
			   memcmp(n->id, id, (size_t) len) != 0)
				continue;
		if (result)
			*result = n;
		return 0;
	}
	return -ENOENT;
}

static int parse_value(snd_config_t **_n, snd_config_t *father, input_t *input, char **id, int skip)
{
	snd_config_t *n = *_n;
	char *s;
	int err;

	err = get_string(&s, 0, input);
	if (err < 0)
		return err;
	if (skip) {
		free(s);
		return 0;
	}
	if (err == 0 && ((s[0] >= '0' && s[0] <= '9') || s[0] == '-')) {
		long i;
		errno = 0;
		err = safe_strtol(s, &i);
		if (err < 0) {
			double r;
			err = safe_strtod(s, &r);
			if (err >= 0) {
				free(s);
				if (n) {
					if (n->type != SND_CONFIG_TYPE_REAL) {
						SNDERR("%s is not a real", *id);
						return -EINVAL;
					}
				} else {
					err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_REAL, father);
					if (err < 0)
						return err;
				}
				n->u.real = r;
				*_n = n;
				return 0;
			}
		} else {
			free(s);
			if (n) {
				if (n->type != SND_CONFIG_TYPE_INTEGER) {
					SNDERR("%s is not an integer", *id);
					return -EINVAL;
				}
			} else {
				err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_INTEGER, father);
				if (err < 0)
					return err;
			}
			n->u.integer = i;
			*_n = n;
			return 0;
		}
	}
	if (n) {
		if (n->type != SND_CONFIG_TYPE_STRING) {
			SNDERR("%s is not a string", *id);
			free(s);
			return -EINVAL;
		}
	} else {
		err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_STRING, father);
		if (err < 0)
			return err;
	}
	if (n->u.string)
		free(n->u.string);
	n->u.string = s;
	*_n = n;
	return 0;
}

static int parse_defs(snd_config_t *father, input_t *input, int skip);
static int parse_array_defs(snd_config_t *farther, input_t *input, int skip);

static int parse_array_def(snd_config_t *father, input_t *input, int idx, int skip)
{
	char static_id[12], *id = NULL;
	int c;
	int err;
	snd_config_t *n = NULL;

	if (!skip) {
		snprintf(static_id, sizeof(static_id), "%i", idx);
		static_id[sizeof(static_id)-1] = '\0';
		id = strdup(static_id);
		if (id == NULL)
			return -ENOMEM;
	}
	c = get_nonwhite(input);
	if (c < 0) {
		err = c;
		goto __end;
	}
	switch (c) {
	case '{':
	case '[':
	{
		char endchr;
		if (!skip) {
			if (n) {
				if (n->type != SND_CONFIG_TYPE_COMPOUND) {
					SNDERR("%s is not a compound", id);
					err = -EINVAL;
					goto __end;
				}
			} else {
				err = _snd_config_make_add(&n, &id, SND_CONFIG_TYPE_COMPOUND, father);
				if (err < 0)
					goto __end;
			}
		}
		if (c == '{') {
			err = parse_defs(n, input, skip);
			endchr = '}';
		} else {
			err = parse_array_defs(n, input, skip);
			endchr = ']';
		}
		c = get_nonwhite(input);
		if (c < 0) {
			err = c;
			goto __end;
		}
		if (c != endchr) {
			if (n)
				snd_config_delete(n);
			err = LOCAL_UNEXPECTED_CHAR;
			goto __end;
		}
		break;
	}
	default:
		unget_char(c, input);
		err = parse_value(&n, father, input, &id, skip);
		if (err < 0)
			goto __end;
		break;
	}
	err = 0;
      __end:
      	if (id)
      		free(id);
      	return err;
}

static int parse_array_defs(snd_config_t *father, input_t *input, int skip)
{
	int idx = 0;
	while (1) {
		int c = get_nonwhite(input), err;
		if (c < 0)
			return c;
		unget_char(c, input);
		if (c == ']')
			return 0;
		err = parse_array_def(father, input, idx++, skip);
		if (err < 0)
			return err;
	}
	return 0;
}

static int parse_def(snd_config_t *father, input_t *input, int skip)
{
	char *id = NULL;
	int c;
	int err;
	snd_config_t *n;
	enum {MERGE_CREATE, MERGE, OVERRIDE, DONT_OVERRIDE} mode;
	while (1) {
		c = get_nonwhite(input);
		if (c < 0)
			return c;
		switch (c) {
		case '?':
			mode = DONT_OVERRIDE;
			break;
		case '!':
			mode = OVERRIDE;
			break;
		default:
			mode = MERGE_CREATE;
			unget_char(c, input);
		}
		err = get_string(&id, 1, input);
		if (err < 0)
			return err;
		c = get_nonwhite(input);
		if (c != '.')
			break;
		if (skip) {
			free(id);
			continue;
		}
		if (_snd_config_search(father, id, -1, &n) == 0) {
			if (mode == DONT_OVERRIDE) {
				skip = 1;
				free(id);
				continue;
			}
			if (mode != OVERRIDE) {
				if (n->type != SND_CONFIG_TYPE_COMPOUND) {
					SNDERR("%s is not a compound", id);
					return -EINVAL;
				}
				n->u.compound.join = 1;
				father = n;
				free(id);
				continue;
			}
			snd_config_delete(n);
		}
		if (mode == MERGE) {
			SNDERR("%s does not exists", id);
			err = -ENOENT;
			goto __end;
		}
		err = _snd_config_make_add(&n, &id, SND_CONFIG_TYPE_COMPOUND, father);
		if (err < 0)
			goto __end;
		n->u.compound.join = 1;
		father = n;
	}
	if (c == '=') {
		c = get_nonwhite(input);
		if (c < 0)
			return c;
	}
	if (!skip) {
		if (_snd_config_search(father, id, -1, &n) == 0) {
			if (mode == DONT_OVERRIDE) {
				skip = 1;
				n = NULL;
			} else if (mode == OVERRIDE) {
				snd_config_delete(n);
				n = NULL;
			}
		} else {
			n = NULL;
			if (mode == MERGE) {
				SNDERR("%s does not exists", id);
				err = -ENOENT;
				goto __end;
			}
		}
	}
	switch (c) {
	case '{':
	case '[':
	{
		char endchr;
		if (!skip) {
			if (n) {
				if (n->type != SND_CONFIG_TYPE_COMPOUND) {
					SNDERR("%s is not a compound", id);
					err = -EINVAL;
					goto __end;
				}
			} else {
				err = _snd_config_make_add(&n, &id, SND_CONFIG_TYPE_COMPOUND, father);
				if (err < 0)
					goto __end;
			}
		}
		if (c == '{') {
			err = parse_defs(n, input, skip);
			endchr = '}';
		} else {
			err = parse_array_defs(n, input, skip);
			endchr = ']';
		}
		c = get_nonwhite(input);
		if (c != endchr) {
			if (n)
				snd_config_delete(n);
			err = LOCAL_UNEXPECTED_CHAR;
			goto __end;
		}
		break;
	}
	default:
		unget_char(c, input);
		err = parse_value(&n, father, input, &id, skip);
		if (err < 0)
			goto __end;
		break;
	}
	c = get_nonwhite(input);
	switch (c) {
	case ';':
	case ',':
		break;
	default:
		unget_char(c, input);
	}
      __end:
      	if (id)
	      	free(id);
	return err;
}
		
static int parse_defs(snd_config_t *father, input_t *input, int skip)
{
	int c, err;
	while (1) {
		c = get_nonwhite(input);
		if (c < 0)
			return c == LOCAL_UNEXPECTED_EOF ? 0 : c;
		unget_char(c, input);
		if (c == '}')
			return 0;
		err = parse_def(father, input, skip);
		if (err < 0)
			return err;
	}
	return 0;
}

static void string_print(char *str, int id, snd_output_t *out)
{
	unsigned char *p = str;
	if (!id) {
		switch (*p) {
		case 0:
			assert(0);
			break;
		case '0' ... '9':
		case '-':
			goto quoted;
		}
	}
	if (!*p) {
		snd_output_puts(out, "''");
		return;
	}
 loop:
	switch (*p) {
	case 0:
		goto nonquoted;
	case 1 ... 31:
	case 127 ... 255:
	case ' ':
	case '=':
	case ';':
	case ',':
	case '.':
	case '{':
	case '}':
	case '\'':
	case '"':
		goto quoted;
	default:
		p++;
		goto loop;
	}
 nonquoted:
	snd_output_puts(out, str);
	return;
 quoted:
	snd_output_putc(out, '\'');
	p = str;
	while (*p) {
		int c;
		c = *p;
		switch (c) {
		case '\n':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'n');
			break;
		case '\t':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 't');
			break;
		case '\v':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'v');
			break;
		case '\b':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'b');
			break;
		case '\r':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'r');
			break;
		case '\f':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'f');
			break;
		case '\'':
			snd_output_putc(out, '\\');
			snd_output_putc(out, c);
			break;
		case 32 ... '\'' - 1:
		case '\'' + 1 ... 126:
			snd_output_putc(out, c);
			break;
		default:
			snd_output_printf(out, "\\%04o", c);
			break;
		}
		p++;
	}
	snd_output_putc(out, '\'');
}

static int _snd_config_save_leaves(snd_config_t *config, snd_output_t *out, unsigned int level, unsigned int joins);

static int _snd_config_save_leaf(snd_config_t *n, snd_output_t *out, 
				 unsigned int level)
{
	int err;
	unsigned int k;
	switch (n->type) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_output_printf(out, "%ld", n->u.integer);
		break;
	case SND_CONFIG_TYPE_REAL:
		snd_output_printf(out, "%-16g", n->u.real);
		break;
	case SND_CONFIG_TYPE_STRING:
		string_print(n->u.string, 0, out);
		break;
	case SND_CONFIG_TYPE_COMPOUND:
		snd_output_putc(out, '{');
		snd_output_putc(out, '\n');
		err = _snd_config_save_leaves(n, out, level + 1, 0);
		if (err < 0)
			return err;
		for (k = 0; k < level; ++k) {
			snd_output_putc(out, '\t');
		}
		snd_output_putc(out, '}');
		break;
	}
	return 0;
}

static void id_print(snd_config_t *n, snd_output_t *out, unsigned int joins)
{
	if (joins > 0) {
		assert(n->father);
		id_print(n->father, out, joins - 1);
		snd_output_putc(out, '.');
	}
	string_print(n->id, 1, out);
}

static int _snd_config_save_leaves(snd_config_t *config, snd_output_t *out, unsigned int level, unsigned int joins)
{
	unsigned int k;
	int err;
	snd_config_iterator_t i, next;
	assert(config && out);
	snd_config_for_each(i, next, config) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (n->type == SND_CONFIG_TYPE_COMPOUND &&
		    n->u.compound.join) {
			err = _snd_config_save_leaves(n, out, level, joins + 1);
			if (err < 0)
				return err;
			continue;
		}
		for (k = 0; k < level; ++k) {
			snd_output_putc(out, '\t');
		}
		id_print(n, out, joins);
#if 0
		snd_output_putc(out, ' ');
		snd_output_putc(out, '=');
#endif
		snd_output_putc(out, ' ');
		err = _snd_config_save_leaf(n, out, level);
		if (err < 0)
			return err;
#if 0
		snd_output_putc(out, ';');
#endif
		snd_output_putc(out, '\n');
	}
	return 0;
}
#endif


/**
 * \brief Substitute one node to another
 * \param dst Destination node
 * \param src Source node (invalid after call)
 * \return zero if success, otherwise a negative error code
 */
int snd_config_substitute(snd_config_t *dst, snd_config_t *src)
{
	assert(dst && src);
	if (src->type == SND_CONFIG_TYPE_COMPOUND) {	/* append */
		snd_config_iterator_t i, next;
		snd_config_for_each(i, next, src) {
			snd_config_t *n = snd_config_iterator_entry(i);
			n->father = dst;
		}
		src->u.compound.fields.next->prev = &dst->u.compound.fields;
		src->u.compound.fields.prev->next = &dst->u.compound.fields;
	}
	if (dst->id)
		free(dst->id);
	dst->id = src->id;
	dst->type = src->type;
	dst->u = src->u;
	free(src);
	return 0;
}

/**
 * \brief Return type of a config node from an ASCII string
 * \param config Config node handle
 * \return node type
 */
int snd_config_get_type_ascii(const char *ascii, snd_config_type_t *type)
{
	assert(ascii && type);
	if (!strcmp(ascii, "integer")) {
		*type = SND_CONFIG_TYPE_INTEGER;
		return 0;
	}
	if (!strcmp(ascii, "real")) {
		*type = SND_CONFIG_TYPE_REAL;
		return 0;
	}
	if (!strcmp(ascii, "string")) {
		*type = SND_CONFIG_TYPE_STRING;
		return 0;
	}
	if (!strcmp(ascii, "compound")) {
		*type = SND_CONFIG_TYPE_COMPOUND;
		return 0;
	}
	return -EINVAL;
}

/**
 * \brief Return type of a config node
 * \param config Config node handle
 * \return node type
 */
snd_config_type_t snd_config_get_type(snd_config_t *config)
{
	return config->type;
}

/**
 * \brief Return id of a config node
 * \param config Config node handle
 * \return node id
 */
const char *snd_config_get_id(snd_config_t *config)
{
	return config->id;
}

/**
 * \brief Set id of a config node
 * \param config Config node handle
 * \return id Node id
 * \return 0 on success otherwise a negative error code
 */
int snd_config_set_id(snd_config_t *config, const char *id)
{
	free(config->id);
	config->id = strdup(id);
	if (!config->id)
		return -ENOMEM;
	return 0;
}

/**
 * \brief Build a top level config node
 * \param config Returned config node handle pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_config_top(snd_config_t **config)
{
	assert(config);
	return _snd_config_make(config, 0, SND_CONFIG_TYPE_COMPOUND);
}

/**
 * \brief Load a config tree
 * \param config Config top node handle
 * \param in Input handle
 * \return 0 on success otherwise a negative error code
 */
int snd_config_load(snd_config_t *config, snd_input_t *in)
{
	int err;
	input_t input;
	struct filedesc *fd;
	assert(config && in);
	fd = malloc(sizeof(*fd));
	if (!fd)
		return -ENOMEM;
	fd->name = NULL;
	fd->in = in;
	fd->line = 1;
	fd->column = 0;
	fd->next = NULL;
	input.current = fd;
	input.unget = 0;
	err = parse_defs(config, &input, 0);
	fd = input.current;
	if (err < 0) {
		const char *str;
		switch (err) {
		case LOCAL_UNTERMINATED_STRING:
			str = "Unterminated string";
			break;
		case LOCAL_UNTERMINATED_QUOTE:
			str = "Unterminated quote";
			break;
		case LOCAL_UNEXPECTED_CHAR:
			str = "Unexpected char";
			break;
		case LOCAL_UNEXPECTED_EOF:
			str = "Unexpected end of file";
			break;
		default:
			str = strerror(-err);
			break;
		}
		SNDERR("%s:%d:%d:%s", fd->name ? fd->name : "_toplevel_", fd->line, fd->column, str);
		goto _end;
	}
	if (get_char(&input) != LOCAL_UNEXPECTED_EOF) {
		SNDERR("%s:%d:%d:Unexpected }", fd->name ? fd->name : "", fd->line, fd->column);
		err = -EINVAL;
		goto _end;
	}
 _end:
	while (fd->next) {
		snd_input_close(fd->in);
		free(fd->name);
		free(fd);
		fd = fd->next;
	}
	free(fd);
	return err;
}

/**
 * \brief Add a leaf to a config compound node
 * \param father Config compound node handle
 * \param leaf Leaf config node handle
 * \return 0 on success otherwise a negative error code
 */
int snd_config_add(snd_config_t *father, snd_config_t *leaf)
{
	snd_config_iterator_t i, next;
	assert(father && leaf);
	snd_config_for_each(i, next, father) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (strcmp(leaf->id, n->id) == 0)
			return -EEXIST;
	}
	leaf->father = father;
	list_add_tail(&leaf->list, &father->u.compound.fields);
	return 0;
}

/**
 * \brief Remove a leaf config node from tree
 * \param config Config node handle
 * \return 0 on success otherwise a negative error code
 */
int snd_config_remove(snd_config_t *config)
{
	assert(config);
	if (config->father)
		list_del(&config->list);
	config->father = NULL;
	return 0;
}

/**
 * \brief Remove a leaf config node (freeing all the related resources)
 * \param config Config node handle
 * \return 0 on success otherwise a negative error code
 */
int snd_config_delete(snd_config_t *config)
{
	assert(config);
	switch (config->type) {
	case SND_CONFIG_TYPE_COMPOUND:
	{
		int err;
		struct list_head *i;
		i = config->u.compound.fields.next;
		while (i != &config->u.compound.fields) {
			struct list_head *nexti = i->next;
			snd_config_t *leaf = snd_config_iterator_entry(i);
			err = snd_config_delete(leaf);
			if (err < 0)
				return err;
			i = nexti;
		}
		break;
	}
	case SND_CONFIG_TYPE_STRING:
		if (config->u.string)
			free(config->u.string);
		break;
	default:
		break;
	}
	if (config->father)
		list_del(&config->list);
	free(config->id);
	free(config);
	return 0;
}

/**
 * \brief Build a config node
 * \param config Returned config node handle pointer
 * \param id Node id
 * \param type Node type
 * \return 0 on success otherwise a negative error code
 */
int snd_config_make(snd_config_t **config, const char *id,
		    snd_config_type_t type)
{
	char *id1;
	assert(config);
	if (id) {
		id1 = strdup(id);
		if (!id1)
			return -ENOMEM;
	} else
		id1 = NULL;
	return _snd_config_make(config, &id1, type);
}

/**
 * \brief Build an integer config node
 * \param config Returned config node handle pointer
 * \param id Node id
 * \return 0 on success otherwise a negative error code
 */
int snd_config_make_integer(snd_config_t **config, const char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_INTEGER);
}

/**
 * \brief Build a real config node
 * \param config Returned config node handle pointer
 * \param id Node id
 * \return 0 on success otherwise a negative error code
 */
int snd_config_make_real(snd_config_t **config, const char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_REAL);
}

/**
 * \brief Build a string config node
 * \param config Returned config node handle pointer
 * \param id Node id
 * \return 0 on success otherwise a negative error code
 */
int snd_config_make_string(snd_config_t **config, const char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_STRING);
}

/**
 * \brief Build an empty compound config node
 * \param config Returned config node handle pointer
 * \param id Node id
 * \param join Join flag (checked in snd_config_save to change look)
 * \return 0 on success otherwise a negative error code
 */
int snd_config_make_compound(snd_config_t **config, const char *id,
			     int join)
{
	int err;
	err = snd_config_make(config, id, SND_CONFIG_TYPE_COMPOUND);
	if (err < 0)
		return err;
	(*config)->u.compound.join = join;
	return 0;
}

/**
 * \brief Change the value of an integer config node
 * \param config Config node handle
 * \param value Value
 * \return 0 on success otherwise a negative error code
 */
int snd_config_set_integer(snd_config_t *config, long value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_INTEGER)
		return -EINVAL;
	config->u.integer = value;
	return 0;
}

/**
 * \brief Change the value of a real config node
 * \param config Config node handle
 * \param value Value
 * \return 0 on success otherwise a negative error code
 */
int snd_config_set_real(snd_config_t *config, double value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_REAL)
		return -EINVAL;
	config->u.real = value;
	return 0;
}

/**
 * \brief Change the value of a string config node
 * \param config Config node handle
 * \param value Value
 * \return 0 on success otherwise a negative error code
 */
int snd_config_set_string(snd_config_t *config, const char *value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_STRING)
		return -EINVAL;
	if (config->u.string)
		free(config->u.string);
	config->u.string = strdup(value);
	if (!config->u.string)
		return -ENOMEM;
	return 0;
}

/**
 * \brief Change the value of a config node
 * \param config Config node handle
 * \param ascii Value in ASCII form
 * \return 0 on success otherwise a negative error code
 */
int snd_config_set_ascii(snd_config_t *config, const char *ascii)
{
	assert(config && ascii);
	switch (config->type) {
	case SND_CONFIG_TYPE_INTEGER:
		{
			long i;
			int err = safe_strtol(ascii, &i);
			if (err < 0)
				return err;
			config->u.integer = i;
		}
		break;
	case SND_CONFIG_TYPE_REAL:
		{
			double d;
			int err = safe_strtod(ascii, &d);
			if (err < 0)
				return err;
			config->u.real = d;
			break;
		}
	case SND_CONFIG_TYPE_STRING:
		{
			char *ptr = realloc(config->u.string, strlen(ascii) + 1);
			if (ptr == NULL)
				return -ENOMEM;
			strcpy(config->u.string, ascii);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/**
 * \brief Get the value of an integer config node
 * \param config Config node handle
 * \param ptr Returned value pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_config_get_integer(snd_config_t *config, long *ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_INTEGER)
		return -EINVAL;
	*ptr = config->u.integer;
	return 0;
}

/**
 * \brief Get the value of a real config node
 * \param config Config node handle
 * \param ptr Returned value pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_config_get_real(snd_config_t *config, double *ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_REAL)
		return -EINVAL;
	*ptr = config->u.real;
	return 0;
}

/**
 * \brief Get the value of a string config node
 * \param config Config node handle
 * \param ptr Returned value pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_config_get_string(snd_config_t *config, const char **ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_STRING)
		return -EINVAL;
	*ptr = config->u.string;
	return 0;
}

/**
 * \brief Get the value in ASCII form
 * \param config Config node handle
 * \param ascii Returned dynamically allocated ASCII string
 * \return 0 on success otherwise a negative error code
 */
int snd_config_get_ascii(snd_config_t *config, char **ascii)
{
	assert(config && ascii);
	switch (config->type) {
	case SND_CONFIG_TYPE_INTEGER:
		{
			char res[12];
			int err;
			err = snprintf(res, sizeof(res), "%li", config->u.integer);
			if (err < 0 || err == sizeof(res)) {
				assert(0);
				return -ENOMEM;
			}
			*ascii = strdup(res);
		}
		break;
	case SND_CONFIG_TYPE_REAL:
		{
			char res[32];
			int err;
			err = snprintf(res, sizeof(res), "%-16g", config->u.real);
			if (err < 0 || err == sizeof(res)) {
				assert(0);
				return -ENOMEM;
			}
			if (res[0]) {		/* trim the string */
				char *ptr;
				ptr = res + strlen(res) - 1;
				while (ptr != res && *ptr == ' ')
					ptr--;
				if (*ptr != ' ')
					ptr++;
				*ptr = '\0';
			}
			*ascii = strdup(res);
		}
		break;
	case SND_CONFIG_TYPE_STRING:
		*ascii = strdup(config->u.string);
		break;
	default:
		return -EINVAL;
	}
	if (*ascii == NULL)
		return -ENOMEM;
	return 0;
}

/**
 * \brief Dump a config tree contents
 * \param config Config node handle
 * \param out Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_config_save(snd_config_t *config, snd_output_t *out)
{
	assert(config && out);
	return _snd_config_save_leaves(config, out, 0, 0);
}

/*
 *  *** search macros ***
 */

#ifndef DOC_HIDDEN

#define SND_CONFIG_SEARCH(config, key, result, extra_code) \
{ \
	snd_config_t *n; \
	int err; \
	const char *p; \
	assert(config && key); \
	while (1) { \
		if (config->type != SND_CONFIG_TYPE_COMPOUND) \
			return -ENOENT; \
		{ extra_code ; } \
		p = strchr(key, '.'); \
		if (p) { \
			err = _snd_config_search(config, key, p - key, &n); \
			if (err < 0) \
				return err; \
			config = n; \
			key = p + 1; \
		} else \
			return _snd_config_search(config, key, -1, result); \
	} \
}

#define SND_CONFIG_SEARCHA(root, config, key, result, fcn, extra_code) \
{ \
	snd_config_t *n; \
	int err; \
	const char *p; \
	assert(config && key); \
	while (1) { \
		if (config->type != SND_CONFIG_TYPE_COMPOUND) { \
			if (snd_config_get_string(config, &p) < 0) \
				return -ENOENT; \
			err = fcn(root, root, p, &config); \
			if (err < 0) \
				return err; \
		} \
		{ extra_code ; } \
		p = strchr(key, '.'); \
		if (p) { \
			err = _snd_config_search(config, key, p - key, &n); \
			if (err < 0) \
				return err; \
			config = n; \
			key = p + 1; \
		} else \
			return _snd_config_search(config, key, -1, result); \
	} \
}

#define SND_CONFIG_SEARCHV(config, result, fcn) \
{ \
	snd_config_t *n; \
	va_list arg; \
	assert(config); \
	va_start(arg, result); \
	while (1) { \
		const char *k = va_arg(arg, const char *); \
		int err; \
		if (!k) \
			break; \
		err = fcn(config, k, &n); \
		if (err < 0) \
			return err; \
		config = n; \
	} \
	va_end(arg); \
	if (result) \
		*result = n; \
	return 0; \
}

#define SND_CONFIG_SEARCHVA(root, config, result, fcn) \
{ \
	snd_config_t *n; \
	va_list arg; \
	assert(config); \
	va_start(arg, result); \
	while (1) { \
		const char *k = va_arg(arg, const char *); \
		int err; \
		if (!k) \
			break; \
		err = fcn(root, config, k, &n); \
		if (err < 0) \
			return err; \
		config = n; \
	} \
	va_end(arg); \
	if (result) \
		*result = n; \
	return 0; \
}

#define SND_CONFIG_SEARCH_ALIAS(config, base, key, result, fcn1, fcn2) \
{ \
	snd_config_t *res = NULL; \
	int err, first = 1; \
	assert(config && key); \
	do { \
		err = first && base ? -EIO : fcn1(config, config, key, &res); \
		if (err < 0) { \
			if (!base) \
				break; \
			err = fcn2(config, config, &res, base, key, NULL); \
			if (err < 0) \
				break; \
		} \
		first = 0; \
	} while (snd_config_get_string(res, &key) >= 0); \
	if (!res) \
		return err; \
	if (result) \
		*result = res; \
	return 0; \
}

#endif /* DOC_HIDDEN */

/**
 * \brief Search a node inside a config tree
 * \param config Config node handle
 * \param key Dot separated search key
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 */
int snd_config_search(snd_config_t *config, const char *key, snd_config_t **result)
{
	SND_CONFIG_SEARCH(config, key, result, );
}

/**
 * \brief Search a node inside a config tree and expand aliases, excluding the top one
 * \param config Config node handle
 * \param key Dot separated search key
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 */
int snd_config_searcha(snd_config_t *root, snd_config_t *config, const char *key, snd_config_t **result)
{
	SND_CONFIG_SEARCHA(root, config, key, result, snd_config_searcha, );
}

/**
 * \brief Search a node inside a config tree
 * \param config Config node handle
 * \param result Pointer to found node
 * \param ... one or more concatenated dot separated search key
 * \return 0 on success otherwise a negative error code
 */
int snd_config_searchv(snd_config_t *config, snd_config_t **result, ...)
{
	SND_CONFIG_SEARCHV(config, result, snd_config_search);
}

/**
 * \brief Search a node inside a config tree and expand aliases, excluding the top one
 * \param config Config node handle
 * \param result Pointer to found node
 * \param ... one or more concatenated dot separated search key
 * \return 0 on success otherwise a negative error code
 */
int snd_config_searchva(snd_config_t *root, snd_config_t *config, snd_config_t **result, ...)
{
	SND_CONFIG_SEARCHVA(root, config, result, snd_config_searcha);
}

/**
 * \brief Search a node inside a config tree using alias
 * \param config Config node handle
 * \param base Key base (or NULL)
 * \param key Key suffix
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 *
 * First key is tried and if nothing is found is tried base.key.
 * If the value found is a string this is recursively tried in the
 * same way.
 */
int snd_config_search_alias(snd_config_t *config,
			    const char *base, const char *key,
			    snd_config_t **result)
{
	SND_CONFIG_SEARCH_ALIAS(config, base, key, result,
				snd_config_searcha, snd_config_searchva);
}

/**
 * \brief Search a node inside a config tree and expand hooks
 * \param config Config node handle
 * \param key Dot separated search key
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 */
int snd_config_search_hooks(snd_config_t *config, const char *key, snd_config_t **result)
{
	static int snd_config_hooks(snd_config_t *config, void *private_data);
	SND_CONFIG_SEARCH(config, key, result, \
					err = snd_config_hooks(config, NULL); \
					if (err < 0) \
						return err; \
			 );
}

/**
 * \brief Search a node inside a config tree and expand aliases and hooks
 * \param config Config node handle
 * \param key Dot separated search key
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 */
int snd_config_searcha_hooks(snd_config_t *root, snd_config_t *config, const char *key, snd_config_t **result)
{
	static int snd_config_hooks(snd_config_t *config, void *private_data);
	SND_CONFIG_SEARCHA(root, config, key, result,
					snd_config_searcha_hooks,
					err = snd_config_hooks(config, NULL); \
					if (err < 0) \
						return err; \
			 );
}

/**
 * \brief Search a node inside a config tree and expand hooks and aliases
 * \param config Config node handle
 * \param result Pointer to found node
 * \param ... one or more concatenated dot separated search key
 * \return 0 on success otherwise a negative error code
 */
int snd_config_searchva_hooks(snd_config_t *root, snd_config_t *config,
			      snd_config_t **result, ...)
{
	SND_CONFIG_SEARCHVA(root, config, result, snd_config_searcha_hooks);
}

/**
 * \brief Search a node inside a config tree using alias and expand hooks
 * \param config Config node handle
 * \param base Key base (or NULL)
 * \param key Key suffix
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 *
 * First key is tried and if nothing is found is tried base.key.
 * If the value found is a string this is recursively tried in the
 * same way.
 */
int snd_config_search_alias_hooks(snd_config_t *config,
				  const char *base, const char *key,
				  snd_config_t **result)
{
	SND_CONFIG_SEARCH_ALIAS(config, base, key, result,
				snd_config_searcha_hooks,
				snd_config_searchva_hooks);
}

/** Environment variable containing files list for #snd_config_update */
#define ALSA_CONFIG_PATH_VAR "ALSA_CONFIG_PATH"

/** Default files used by #snd_config_update */
#define ALSA_CONFIG_PATH_DEFAULT DATADIR "/alsa/alsa.conf"

/** \ingroup Config
  * Config top node */
snd_config_t *snd_config = NULL;

static struct finfo {
	char *name;
	dev_t dev;
	ino_t ino;
	time_t mtime;
} *files_info = NULL;

static unsigned int files_info_count = 0;

static int snd_config_hooks_call(snd_config_t *root, snd_config_t *config, void *private_data)
{
	void *h = NULL;
	snd_config_t *c, *func_conf = NULL;
	char *buf = NULL;
	const char *lib = NULL, *func_name = NULL;
	const char *str;
	int (*func)(snd_config_t *root, snd_config_t *config, snd_config_t **dst, void *private_data) = NULL;
	int err;

	err = snd_config_search(config, "func", &c);
	if (err < 0) {
		SNDERR("Field func is missing");
		return err;
	}
	err = snd_config_get_string(c, &str);
	if (err < 0) {
		SNDERR("Invalid type for field func");
		return err;
	}
	err = snd_config_search_definition(root, "hook_func", str, &func_conf);
	if (err >= 0) {
		snd_config_iterator_t i, next;
		if (snd_config_get_type(func_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for func %s definition", str);
			goto _err;
		}
		snd_config_for_each(i, next, func_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			if (strcmp(id, "func") == 0) {
				err = snd_config_get_string(n, &func_name);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
		}
	}
	if (!func_name) {
		int len = 16 + strlen(str) + 1;
		buf = malloc(len);
		snprintf(buf, len, "snd_config_hook_%s", str);
		buf[len-1] = '\0';
		func_name = buf;
	}
	h = dlopen(lib, RTLD_NOW);
	if ((err = snd_dlsym_verify(h, func_name, SND_DLSYM_VERSION(SND_CONFIG_DLSYM_VERSION_HOOK))) < 0)
		goto _err;
	func = h ? dlsym(h, func_name) : NULL;
	err = 0;
	if (!h) {
		SNDERR("Cannot open shared library %s", lib);
		err = -ENOENT;
	} else if (!func) {
		SNDERR("symbol %s is not defined inside %s", func_name, lib);
		dlclose(h);
		err = -ENXIO;
	}
	_err:
	if (func_conf)
		snd_config_delete(func_conf);
	if (err >= 0) {
		snd_config_t *nroot;
		err = func(root, config, &nroot, private_data);
		if (err < 0)
			SNDERR("function %s returned error: %s", func_name, snd_strerror(err));
		dlclose(h);
		if (err >= 0 && nroot)
			snd_config_substitute(root, nroot);
	}
	if (buf)
		free(buf);
	if (err < 0)
		return err;
	return 0;
}

static int snd_config_hooks(snd_config_t *config, void *private_data)
{
	snd_config_t *n;
	snd_config_iterator_t i, next;
	int err, hit, idx = 0;

	if ((err = snd_config_search(config, "@hooks", &n)) < 0)
		return 0;
	snd_config_remove(n);
	do {
		hit = 0;
		snd_config_for_each(i, next, n) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			long i;
			err = safe_strtol(id, &i);
			if (err < 0) {
				SNDERR("id of field %s is not and integer", id);
				err = -EINVAL;
				goto _err;
			}
			if (i == idx) {
				err = snd_config_hooks_call(config, n, private_data);
				if (err < 0)
					return err;
				idx++;
				hit = 1;
			}
		}
	} while (hit);
	err = 0;
       _err:
	snd_config_delete(n);
	return err;
}

/**
 * \brief Load configuration from specified files
 * \param root Configuration root node
 * \param config Configuration node
 * \param dst Destination node
 * \param private_data Private data
 * \return zero if success, otherwise a negative error code
 */
SND_DLSYM_BUILD_VERSION(snd_config_hook_load, SND_CONFIG_DLSYM_VERSION_HOOK);
int snd_config_hook_load(snd_config_t *root, snd_config_t *config, snd_config_t **dst, void *private_data)
{
	snd_config_t *n, *res = NULL;
	snd_config_iterator_t i, next;
	struct finfo *fi = NULL;
	int err, idx = 0, fi_count = 0, errors = 1, hit;

	assert(root && dst);
	if ((err = snd_config_search(config, "errors", &n)) >= 0) {
		char *tmp;
		err = snd_config_get_ascii(n, &tmp);
		if (err < 0)
			return err;
		errors = snd_config_get_bool_ascii(tmp);
		free(tmp);
		if (errors < 0) {
			SNDERR("Invalid bool value in field errors");
			return errors;
		}
	}
	if ((err = snd_config_search(config, "files", &n)) < 0) {
		SNDERR("Unable to find field files in the preload section");
		return -EINVAL;
	}
	if ((err = snd_config_expand(n, root, NULL, private_data, &n)) < 0) {
		SNDERR("Unable to expand filenames in the preload section");
		return err;
	}
	if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("Invalid type for field filenames");
		goto _err;
	}
	snd_config_for_each(i, next, n) {
		snd_config_t *c = snd_config_iterator_entry(i);
		const char *str;
		if ((err = snd_config_get_string(c, &str)) < 0) {
			SNDERR("Field %s is not a string", snd_config_get_id(c));
			goto _err;
		}
		fi_count++;
	}
	fi = calloc(fi_count, sizeof(*fi));
	if (fi == NULL) {
		err = -ENOMEM;
		goto _err;
	}
	do {
		hit = 0;
		snd_config_for_each(i, next, n) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			long i;
			err = safe_strtol(id, &i);
			if (err < 0) {
				SNDERR("id of field %s is not and integer", id);
				err = -EINVAL;
				goto _err;
			}
			if (i == idx) {
				wordexp_t we;
				char *name;
				if ((err = snd_config_get_ascii(n, &name)) < 0)
					goto _err;
				err = wordexp(name, &we, WRDE_NOCMD);
				switch (err) {
				case WRDE_NOSPACE:
					err = -ENOMEM;
					goto _err;
				case 0:
					if (we.we_wordc == 1)
						break;
					/* Fall through */
				default:
					err = -EINVAL;
					goto _err;
				}
				fi[idx].name = strdup(we.we_wordv[0]);
				wordfree(&we);
				free(name);
				if (fi[idx].name == NULL) {
					err = -ENOMEM;
					goto _err;
				}
				idx++;
				hit = 1;
			}
		}
	} while (hit);
	err = snd_config_top(&res);
	if (err < 0)
		goto _err;
	for (idx = 0; idx < fi_count; idx++) {
		snd_input_t *in;
		if (!errors && access(fi[idx].name, R_OK) < 0)
			continue;
		err = snd_input_stdio_open(&in, fi[idx].name, "r");
		if (err >= 0) {
			err = snd_config_load(root, in);
			snd_input_close(in);
			if (err < 0) {
				SNDERR("%s may be old or corrupted: consider to remove or fix it", fi[idx].name);
				goto _err;
			}
		} else {
			SNDERR("cannot access file %s", fi[idx].name);
		}
	}
	*dst = NULL;
	res = NULL;
	err = 0;
       _err:
	for (idx = 0; idx < fi_count; idx++)
       		if (fi[idx].name)
       			free(fi[idx].name);
       	if (fi)
       		free(fi);
       	if (res)
       		snd_config_delete(res);
	snd_config_delete(n);
	return err;
}

#ifndef DOC_HIDDEN
int snd_determine_driver(int card, char **driver);
#endif

/**
 * \brief Load configuration for all present cards
 * \param root Configuration root node
 * \param config Configuration node
 * \param dst Destination node
 * \param private_data Private data
 * \return zero if success, otherwise a negative error code
 */
SND_DLSYM_BUILD_VERSION(snd_config_hook_load_for_all_cards, SND_CONFIG_DLSYM_VERSION_HOOK);
int snd_config_hook_load_for_all_cards(snd_config_t *root, snd_config_t *config, snd_config_t **dst, void *private_data ATTRIBUTE_UNUSED)
{
	int card = -1, err;
	
	do {
		err = snd_card_next(&card);
		if (err < 0)
			return err;
		if (card >= 0) {
			snd_config_t *n;
			const char *driver;
			char *fdriver = NULL;
			err = snd_determine_driver(card, &fdriver);
			if (err < 0)
				return err;
			if (snd_config_search(root, fdriver, &n) >= 0) {
				if (snd_config_get_string(n, &driver) < 0)
					continue;
				while (1) {
					char *s = strchr(driver, '.');
					if (s == NULL)
						break;
					driver = s + 1;
				}
				if (snd_config_search(root, driver, &n) >= 0)
					continue;
			} else {
				driver = fdriver;
			}
			err = snd_config_hook_load(root, config, &n, (void *)driver);
			if (fdriver)
				free(fdriver);
			if (err < 0)
				return err;
		}
	} while (card >= 0);
	*dst = NULL;
	return 0;
}

/** 
 * \brief Update #snd_config rereading (if needed) files specified in
 * environment variable ALSA_CONFIG_PATH. If it's not set the default value is
 * "/usr/share/alsa/alsa.conf".
 * \return non-negative value on success, otherwise a negative error code
 * \retval 0 no action is needed
 * \retval 1 tree has been rebuild
 *
 * Warning: If config tree is reread all the string pointer and config 
 * node handle previously obtained from this tree become invalid
 */
int snd_config_update()
{
	int err;
	char *configs, *c;
	unsigned int k;
	wordexp_t we;
	size_t l;
	struct finfo *fi = NULL;
	unsigned int fi_count;
	configs = getenv(ALSA_CONFIG_PATH_VAR);
	if (!configs)
		configs = ALSA_CONFIG_PATH_DEFAULT;
	for (k = 0, c = configs; (l = strcspn(c, ": ")) > 0; ) {
		c += l;
		k++;
		if (!*c)
			break;
		c++;
	}
	fi_count = k;
	if (fi_count == 0)
		goto _reread;
	fi = calloc(fi_count, sizeof(*fi));
	if (!fi)
		return -ENOMEM;
	for (k = 0, c = configs; (l = strcspn(c, ": ")) > 0; ) {
		char name[l + 1];
		memcpy(name, c, l);
		name[l] = 0;
		err = wordexp(name, &we, WRDE_NOCMD);
		switch (err) {
		case WRDE_NOSPACE:
			err = -ENOMEM;
			goto _end;
		case 0:
			if (we.we_wordc == 1)
				break;
			/* Fall through */
		default:
			err = -EINVAL;
			goto _end;
		}
		fi[k].name = strdup(we.we_wordv[0]);
		wordfree(&we);
		if (!fi[k].name) {
			err = -ENOMEM;
			goto _end;
		}
		c += l;
		k++;
		if (!*c)
			break;
		c++;
	}
	for (k = 0; k < fi_count; ++k) {
		struct stat st;
		if (stat(fi[k].name, &st) >= 0) {
			fi[k].dev = st.st_dev;
			fi[k].ino = st.st_ino;
			fi[k].mtime = st.st_mtime;
		} else {
			memmove(&fi[k], &fi[k+1], sizeof(*fi) * (fi_count - k - 1));
			k--;
			fi_count--;
		}
	}
	if (!files_info)
		goto _reread;
	if (fi_count != files_info_count)
		goto _reread;
	for (k = 0; k < fi_count; ++k) {
		if (strcmp(fi[k].name, files_info[k].name) != 0 ||
		    fi[k].dev != files_info[k].dev ||
		    fi[k].ino != files_info[k].ino ||
		    fi[k].mtime != files_info[k].mtime)
			goto _reread;
	}
	err = 0;

 _end:
	if (err < 0 && snd_config) {
		snd_config_delete(snd_config);
		snd_config = NULL;
	}
	for (k = 0; k < fi_count; ++k)
		free(fi[k].name);
	if (fi)
		free(fi);
	return err;

 _reread:
	if (files_info) {
		for (k = 0; k < files_info_count; ++k)
			free(files_info[k].name);
		free(files_info);
		files_info = NULL;
		files_info_count = 0;
	}
	if (snd_config) {
		snd_config_delete(snd_config);
		snd_config = NULL;
	}
	err = snd_config_top(&snd_config);
	if (err < 0)
		goto _end;
	for (k = 0; k < fi_count; ++k) {
		snd_input_t *in;
		err = snd_input_stdio_open(&in, fi[k].name, "r");
		if (err >= 0) {
			err = snd_config_load(snd_config, in);
			snd_input_close(in);
			if (err < 0) {
				SNDERR("%s may be old or corrupted: consider to remove or fix it", fi[k].name);
				goto _end;
			}
		} else {
			SNDERR("cannot access file %s", fi[k].name);
		}
	}
	err = snd_config_hooks(snd_config, NULL);
	if (err < 0) {
		SNDERR("hooks failed, removing configuration");
		goto _end;
	}
	files_info = fi;
	files_info_count = fi_count;
	return 1;
}


/**
 * \brief Return an iterator pointing to first leaf of a compound config node
 * \param node Config node handle
 * \return iterator value for first leaf
 */
snd_config_iterator_t snd_config_iterator_first(snd_config_t *node)
{
	assert(node->type == SND_CONFIG_TYPE_COMPOUND);
	return node->u.compound.fields.next;
}

/**
 * \brief Return an iterator pointing to next leaf
 * \param iterator Config node iterator
 * \return iterator value for next leaf
 */
snd_config_iterator_t snd_config_iterator_next(snd_config_iterator_t iterator)
{
	return iterator->next;
}

/**
 * \brief Return an iterator pointing past the last leaf of a compound config node
 * \param node Config node handle
 * \return iterator value for end
 */
snd_config_iterator_t snd_config_iterator_end(snd_config_t *node)
{
	assert(node->type == SND_CONFIG_TYPE_COMPOUND);
	return &node->u.compound.fields;
}

/**
 * \brief Return the node handle pointed by iterator
 * \param iterator Config node iterator
 * \return config node handle
 */
snd_config_t *snd_config_iterator_entry(snd_config_iterator_t iterator)
{
	return list_entry(iterator, snd_config_t, list);
}

#ifndef DOC_HIDDEN
typedef enum _snd_config_walk_pass {
	SND_CONFIG_WALK_PASS_PRE,
	SND_CONFIG_WALK_PASS_POST,
	SND_CONFIG_WALK_PASS_LEAF,
} snd_config_walk_pass_t;
#endif

/* Return 1 if node needs to be attached to father */
/* Return 2 if compound is replaced with standard node */
#ifndef DOC_HIDDEN
typedef int (*snd_config_walk_callback_t)(snd_config_t *src,
					  snd_config_t *root,
					  snd_config_t **dst,
					  snd_config_walk_pass_t pass,
					  void *private_data);
#endif

static int snd_config_walk(snd_config_t *src,
			   snd_config_t *root,
			   snd_config_t **dst, 
			   snd_config_walk_callback_t callback,
			   void *private_data)
{
	int err;
	snd_config_iterator_t i, next;
	switch (snd_config_get_type(src)) {
	case SND_CONFIG_TYPE_COMPOUND:
		err = callback(src, root, dst, SND_CONFIG_WALK_PASS_PRE, private_data);
		if (err <= 0)
			return err;
		snd_config_for_each(i, next, src) {
			snd_config_t *s = snd_config_iterator_entry(i);
			snd_config_t *d = NULL;

			err = snd_config_walk(s, root, (dst && *dst) ? &d : NULL,
					      callback, private_data);
			if (err < 0)
				goto _error;
			if (err && d) {
				err = snd_config_add(*dst, d);
				if (err < 0)
					goto _error;
			}
		}
		err = callback(src, root, dst, SND_CONFIG_WALK_PASS_POST, private_data);
		if (err <= 0) {
		_error:
			if (dst && *dst)
				snd_config_delete(*dst);
		}
		break;
	default:
		err = callback(src, root, dst, SND_CONFIG_WALK_PASS_LEAF, private_data);
		break;
	}
	return err;
}

static int _snd_config_copy(snd_config_t *src,
			    snd_config_t *root ATTRIBUTE_UNUSED,
			    snd_config_t **dst,
			    snd_config_walk_pass_t pass,
			    void *private_data ATTRIBUTE_UNUSED)
{
	int err;
	const char *id = snd_config_get_id(src);
	snd_config_type_t type = snd_config_get_type(src);
	switch (pass) {
	case SND_CONFIG_WALK_PASS_PRE:
		err = snd_config_make_compound(dst, id, src->u.compound.join);
		if (err < 0)
			return err;
		break;
	case SND_CONFIG_WALK_PASS_LEAF:
		err = snd_config_make(dst, id, type);
		if (err < 0)
			return err;
		switch (type) {
		case SND_CONFIG_TYPE_INTEGER:
		{
			long v;
			err = snd_config_get_integer(src, &v);
			assert(err >= 0);
			snd_config_set_integer(*dst, v);
			break;
		}
		case SND_CONFIG_TYPE_REAL:
		{
			double v;
			err = snd_config_get_real(src, &v);
			assert(err >= 0);
			snd_config_set_real(*dst, v);
			break;
		}
		case SND_CONFIG_TYPE_STRING:
		{
			const char *s;
			err = snd_config_get_string(src, &s);
			assert(err >= 0);
			err = snd_config_set_string(*dst, s);
			if (err < 0)
				return err;
			break;
		}
		default:
			assert(0);
		}
		break;
	default:
		break;
	}
	return 1;
}

/**
 * \brief Return the copy of configuration
 * \param dst Destination configuration handle
 * \return src Source configuration handle
 */
int snd_config_copy(snd_config_t **dst,
		    snd_config_t *src)
{
	return snd_config_walk(src, NULL, dst, _snd_config_copy, NULL);
}

static int _snd_config_expand(snd_config_t *src,
			      snd_config_t *root ATTRIBUTE_UNUSED,
			      snd_config_t **dst,
			      snd_config_walk_pass_t pass,
			      void *private_data)
{
	int err;
	const char *id = snd_config_get_id(src);
	snd_config_type_t type = snd_config_get_type(src);
	switch (pass) {
	case SND_CONFIG_WALK_PASS_PRE:
	{
		if (strcmp(id, "@args") == 0)
			return 0;
		err = snd_config_make_compound(dst, id, src->u.compound.join);
		if (err < 0)
			return err;
		break;
	}
	case SND_CONFIG_WALK_PASS_LEAF:
		switch (type) {
		case SND_CONFIG_TYPE_INTEGER:
		{
			long v;
			err = snd_config_make(dst, id, type);
			if (err < 0)
				return err;
			err = snd_config_get_integer(src, &v);
			assert(err >= 0);
			snd_config_set_integer(*dst, v);
			break;
		}
		case SND_CONFIG_TYPE_REAL:
		{
			double v;
			err = snd_config_make(dst, id, type);
			if (err < 0)
				return err;
			err = snd_config_get_real(src, &v);
			assert(err >= 0);
			snd_config_set_real(*dst, v);
			break;
		}
		case SND_CONFIG_TYPE_STRING:
		{
			const char *s;
			snd_config_t *val;
			snd_config_t *vars = private_data;
			snd_config_get_string(src, &s);
			if (*s == '$') {
				s++;
				if (snd_config_search(vars, s, &val) < 0)
					return 0;
				err = snd_config_copy(dst, val);
				if (err < 0)
					return err;
				err = snd_config_set_id(*dst, id);
				if (err < 0) {
					snd_config_delete(*dst);
					return err;
				}
			} else {
				err = snd_config_make(dst, id, type);
				if (err < 0)
					return err;
				err = snd_config_set_string(*dst, s);
				if (err < 0) {
					snd_config_delete(*dst);
					return err;
				}
			}
			break;
		}
		default:
			assert(0);
		}
		break;
	default:
		break;
	}
	return 1;
}

static int _snd_config_evaluate(snd_config_t *src,
				snd_config_t *root,
				snd_config_t **dst ATTRIBUTE_UNUSED,
				snd_config_walk_pass_t pass,
				void *private_data)
{
	int err;
	if (pass == SND_CONFIG_WALK_PASS_PRE) {
		char *buf = NULL;
		const char *lib = NULL, *func_name = NULL;
		const char *str;
		int (*func)(snd_config_t **dst, snd_config_t *root,
			    snd_config_t *src, void *private_data) = NULL;
		void *h = NULL;
		snd_config_t *c, *func_conf = NULL;
		err = snd_config_search(src, "@func", &c);
		if (err < 0)
			return 1;
		err = snd_config_get_string(c, &str);
		if (err < 0) {
			SNDERR("Invalid type for @func");
			return err;
		}
		err = snd_config_search_definition(root, "func", str, &func_conf);
		if (err >= 0) {
			snd_config_iterator_t i, next;
			if (snd_config_get_type(func_conf) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for func %s definition", str);
				goto _err;
			}
			snd_config_for_each(i, next, func_conf) {
				snd_config_t *n = snd_config_iterator_entry(i);
				const char *id = snd_config_get_id(n);
				if (strcmp(id, "comment") == 0)
					continue;
				if (strcmp(id, "lib") == 0) {
					err = snd_config_get_string(n, &lib);
					if (err < 0) {
						SNDERR("Invalid type for %s", id);
						goto _err;
					}
					continue;
				}
				if (strcmp(id, "func") == 0) {
					err = snd_config_get_string(n, &func_name);
					if (err < 0) {
						SNDERR("Invalid type for %s", id);
						goto _err;
					}
					continue;
				}
				SNDERR("Unknown field %s", id);
			}
		}
		if (!func_name) {
			int len = 9 + strlen(str) + 1;
			buf = malloc(len);
			snprintf(buf, len, "snd_func_%s", str);
			buf[len-1] = '\0';
			func_name = buf;
		}
		h = dlopen(lib, RTLD_NOW);
		if (h) {
			if ((err = snd_dlsym_verify(h, func_name, SND_DLSYM_VERSION(SND_CONFIG_DLSYM_VERSION_EVALUATE))) < 0)
				goto _err;
			func = dlsym(h, func_name);
		}
		err = 0;
		if (!h) {
			SNDERR("Cannot open shared library %s", lib);
			return -ENOENT;
		} else if (!func) {
			SNDERR("symbol %s is not defined inside %s", func_name, lib);
			dlclose(h);
			return -ENXIO;
		}
	       _err:
		if (func_conf)
			snd_config_delete(func_conf);
		if (err >= 0) {
			snd_config_t *eval;
			err = func(&eval, root, src, private_data);
			if (err < 0)
				SNDERR("function %s returned error: %s", func_name, snd_strerror(err));
			dlclose(h);
			if (err >= 0 && eval)
				snd_config_substitute(src, eval);
		}
		if (buf)
			free(buf);
		if (err < 0)
			return err;
		return 0;
	}
	return 1;
}

/**
 * \brief Evaluate a config node in runtime
 * \param config source configuration node
 * \param root root of the source configuration
 * \param private_data private data for runtime evaluation
 * \param result result configuration node
 * \return zero if success, otherwise a negative error code
 */
int snd_config_evaluate(snd_config_t *config, snd_config_t *root,
		        void *private_data, snd_config_t **result)
{
	/* FIXME: Only in place evaluation is currently implemented */
	assert(result == NULL);
	return snd_config_walk(config, root, result, _snd_config_evaluate, private_data);
}

static int load_defaults(snd_config_t *subs, snd_config_t *defs)
{
	snd_config_iterator_t d, dnext;
	snd_config_for_each(d, dnext, defs) {
		snd_config_t *def = snd_config_iterator_entry(d);
		snd_config_iterator_t f, fnext;
		if (snd_config_get_type(def) != SND_CONFIG_TYPE_COMPOUND)
			continue;
		snd_config_for_each(f, fnext, def) {
			snd_config_t *fld = snd_config_iterator_entry(f);
			const char *id = snd_config_get_id(fld);
			if (strcmp(id, "type") == 0)
				continue;
			if (strcmp(id, "default") == 0) {
				snd_config_t *deflt;
				int err;
				err = snd_config_copy(&deflt, fld);
				if (err < 0)
					return err;
				err = snd_config_set_id(deflt, snd_config_get_id(def));
				if (err < 0) {
					snd_config_delete(deflt);
					return err;
				}
				err = snd_config_add(subs, deflt);
				if (err < 0)
					return err;
				continue;
			}
			SNDERR("Unknown field %s", id);
			return -EINVAL;
		}
	}
	return 0;
}

static void skip_blank(const char **ptr)
{
	while (1) {
		switch (**ptr) {
		case ' ':
		case '\f':
		case '\t':
		case '\n':
		case '\r':
			break;
		default:
			return;
		}
		(*ptr)++;
	}
}

static int parse_char(const char **ptr)
{
	int c;
	assert(**ptr == '\\');
	(*ptr)++;
	c = **ptr;
	switch (c) {
	case 'n':
		c = '\n';
		break;
	case 't':
		c = '\t';
		break;
	case 'v':
		c = '\v';
		break;
	case 'b':
		c = '\b';
		break;
	case 'r':
		c = '\r';
		break;
	case 'f':
		c = '\f';
		break;
	case '0' ... '7':
	{
		int num = c - '0';
		int i = 1;
		(*ptr)++;
		do {
			c = **ptr;
			if (c < '0' || c > '7')
				break;
			num = num * 8 + c - '0';
			i++;
			(*ptr)++;
		} while (i < 3);
		return num;
	}
	default:
		break;
	}
	(*ptr)++;
	return c;
}

static int parse_id(const char **ptr)
{
	if (!**ptr)
		return -EINVAL;
	while (1) {
		switch (**ptr) {
		case '\f':
		case '\t':
		case '\n':
		case '\r':
		case ',':
		case '=':
		case '\0':
			return 0;
		default:
			break;
		}
		(*ptr)++;
	}
}

static int parse_string(const char **ptr, char **val)
{
	const size_t bufsize = 256;
	char _buf[bufsize];
	char *buf = _buf;
	size_t alloc = bufsize;
	char delim = **ptr;
	size_t idx = 0;
	(*ptr)++;
	while (1) {
		int c = **ptr;
		switch (c) {
		case '\0':
			SNDERR("Unterminated string");
			return -EINVAL;
		case '\\':
			c = parse_char(ptr);
			if (c < 0)
				return c;
			break;
		default:
			(*ptr)++;
			if (c == delim) {
				*val = malloc(idx + 1);
				if (!*val)
					return -ENOMEM;
				memcpy(*val, buf, idx);
				(*val)[idx] = 0;
				if (alloc > bufsize)
					free(buf);
				return 0;
			}
		}
		if (idx >= alloc) {
			size_t old_alloc = alloc;
			alloc *= 2;
			if (old_alloc == bufsize) {
				buf = malloc(alloc);
				memcpy(buf, _buf, old_alloc);
			} else
				buf = realloc(buf, alloc);
			if (!buf)
				return -ENOMEM;
		}
		buf[idx++] = c;
	}
}
				

/* Parse var=val or val */
static int parse_arg(const char **ptr, unsigned int *varlen, char **val)
{
	const char *str;
	int err, vallen;
	skip_blank(ptr);
	str = *ptr;
	if (*str == '"' || *str == '\'') {
		err = parse_string(ptr, val);
		if (err < 0)
			return err;
		*varlen = 0;
		return 0;
	}
	err = parse_id(ptr);
	if (err < 0)
		return err;
	vallen = *ptr - str;
	skip_blank(ptr);
	if (**ptr != '=') {
		*varlen = 0;
		goto _value;
	}
	*varlen = vallen;
	(*ptr)++;
	skip_blank(ptr);
	str = *ptr;
	if (*str == '"' || *str == '\'') {
		err = parse_string(ptr, val);
		if (err < 0)
			return err;
		return 0;
	}
	err = parse_id(ptr);
	if (err < 0)
		return err;
	vallen = *ptr - str;
 _value:
	*val = malloc(vallen + 1);
	if (!*val)
		return -ENOMEM;
	memcpy(*val, str, vallen);
	(*val)[vallen] = 0;
	return 0;
}


/* val1, val2, ...
 * var1=val1,var2=val2,...
 * { conf syntax }
 */
static int parse_args(snd_config_t *subs, const char *str, snd_config_t *defs)
{
	int err;
	int arg = 0;
	if (str == NULL)
		return 0;
	skip_blank(&str);
	if (!*str)
		return 0;
	if (*str == '{') {
		int len = strlen(str);
		snd_input_t *input;
		snd_config_iterator_t i, next;
		while (1) {
			switch (str[--len]) {
			case ' ':
			case '\f':
			case '\t':
			case '\n':
			case '\r':
				continue;
			default:
				break;
			}
			break;
		}
		if (str[len] != '}')
			return -EINVAL;
		err = snd_input_buffer_open(&input, str + 1, len - 1);
		if (err < 0)
			return err;
		err = snd_config_load(subs, input);
		snd_input_close(input);
		if (err < 0) {
			snd_config_delete(subs);
			return err;
		}
		snd_config_for_each(i, next, subs) {
			snd_config_t *n = snd_config_iterator_entry(i);
			snd_config_t *d;
			const char *id = snd_config_get_id(n);
			err = snd_config_search(defs, id, &d);
			if (err < 0) {
				SNDERR("Unknown parameter %s", id);
				return err;
			}
		}
		return 0;
	}
	
	while (1) {
		char buf[256];
		const char *var = buf;
		unsigned int varlen;
		snd_config_t *def, *sub, *typ;
		const char *new = str;
		const char *tmp;
		char *val;
		err = parse_arg(&new, &varlen, &val);
		if (err < 0)
			goto _err;
		if (varlen > 0) {
			assert(varlen < sizeof(buf));
			memcpy(buf, str, varlen);
			buf[varlen] = 0;
		} else {
			sprintf(buf, "%d", arg);
		}
		err = snd_config_search_alias(defs, NULL, var, &def);
		if (err < 0) {
			SNDERR("Unknown parameter %s", var);
			goto _err;
		}
		if (snd_config_get_type(def) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Parameter %s definition is not correct", var);
			err = -EINVAL;
			goto _err;
		}
		var = snd_config_get_id(def);
		err = snd_config_search(subs, var, &sub);
		if (err >= 0)
			snd_config_delete(sub);
		err = snd_config_search(def, "type", &typ);
		if (err < 0) {
		_invalid_type:
			SNDERR("Parameter %s definition is missing a valid type info", var);
			goto _err;
		}
		err = snd_config_get_string(typ, &tmp);
		if (err < 0)
			goto _invalid_type;
		if (strcmp(tmp, "integer") == 0) {
			long v;
			err = snd_config_make(&sub, var, SND_CONFIG_TYPE_INTEGER);
			if (err < 0)
				goto _err;
			err = safe_strtol(val, &v);
			if (err < 0) {
				SNDERR("Parameter %s must be an integer", var);
				goto _err;
			}
			err = snd_config_set_integer(sub, v);
			if (err < 0)
				goto _err;
		} else if (strcmp(tmp, "real") == 0) {
			double v;
			err = snd_config_make(&sub, var, SND_CONFIG_TYPE_REAL);
			if (err < 0)
				goto _err;
			err = safe_strtod(val, &v);
			if (err < 0) {
				SNDERR("Parameter %s must be a real", var);
				goto _err;
			}
			err = snd_config_set_real(sub, v);
			if (err < 0)
				goto _err;
		} else if (strcmp(tmp, "string") == 0) {
			err = snd_config_make(&sub, var, SND_CONFIG_TYPE_STRING);
			if (err < 0)
				goto _err;
			err = snd_config_set_string(sub, val);
			if (err < 0)
				goto _err;
		} else {
			err = -EINVAL;
			goto _invalid_type;
		}
		err = snd_config_set_id(sub, var);
		if (err < 0)
			goto _err;
		err = snd_config_add(subs, sub);
		if (err < 0) {
		_err:
			free(val);
			return err;
		}
		free(val);
		if (!*new)
			break;
		if (*new != ',')
			return -EINVAL;
		str = new + 1;
		arg++;
	}
	return 0;
}

/**
 * \brief Expand a node applying arguments and functions
 * \param config Config node handle
 * \param root Root config node handle
 * \param args Arguments string (optional)
 * \param private_data Private data for functions
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 */
int snd_config_expand(snd_config_t *config, snd_config_t *root, const char *args,
		      void *private_data, snd_config_t **result)
{
	int err;
	snd_config_t *defs, *subs = NULL, *res;
	err = snd_config_search(config, "@args", &defs);
	if (err < 0) {
		err = snd_config_copy(&res, config);
		if (err < 0)
			return err;
	} else {
		err = snd_config_top(&subs);
		if (err < 0)
			return err;
		err = load_defaults(subs, defs);
		if (err < 0) {
			SNDERR("Load defaults error: %s", snd_strerror(err));
			goto _end;
		}
		err = parse_args(subs, args, defs);
		if (err < 0) {
			SNDERR("Parse arguments error: %s", snd_strerror(err));
			goto _end;
		}
		err = snd_config_evaluate(subs, root, private_data, NULL);
		if (err < 0) {
			SNDERR("Args evaluate error: %s", snd_strerror(err));
			goto _end;
		}
		err = snd_config_walk(config, root, &res, _snd_config_expand, subs);
		if (err < 0) {
			SNDERR("Expand error (walk): %s", snd_strerror(err));
			goto _end;
		}
	}
	err = snd_config_evaluate(res, root, private_data, NULL);
	if (err < 0) {
		SNDERR("Evaluate error: %s", snd_strerror(err));
		snd_config_delete(res);
		goto _end;
	}
	*result = res;
	err = 1;
 _end:
 	if (subs)
		snd_config_delete(subs);
	return err;
}
	
/**
 * \brief Search a definition inside a config tree using alias and expand hooks and arguments
 * \param config Config node handle
 * \param base Key base (or NULL)
 * \param key Key suffix
 * \param result Pointer to expanded found node
 * \return 0 on success otherwise a negative error code
 *
 * First key is tried and if nothing is found is tried base.key.
 * If the value found is a string this is recursively tried in the
 * same way.
 */
int snd_config_search_definition(snd_config_t *config,
				 const char *base, const char *name,
				 snd_config_t **result)
{
	snd_config_t *conf;
	char *key;
	const char *args = strchr(name, ':');
	int err;
	if (args) {
		args++;
		key = alloca(args - name);
		memcpy(key, name, args - name - 1);
		key[args - name - 1] = '\0';
	} else {
		key = (char *) name;
	} 
	err = snd_config_search_alias_hooks(config, NULL, key, &conf);
	if (err < 0)
		err = snd_config_search_alias_hooks(config, base, key, &conf);
	if (err < 0)
		return err;
	return snd_config_expand(conf, config, args, NULL, result);
}


#if 0
/* Not strictly needed, but useful to check for memory leaks */
void _snd_config_end(void) __attribute__ ((destructor));

static void _snd_config_end(void)
{
	int k;
	if (snd_config)
		snd_config_delete(snd_config);
	snd_config = 0;
	for (k = 0; k < files_info_count; ++k)
		free(files_info[k].name);
	free(files_info);
	files_info = NULL;
	files_info_count = 0;
}
#endif
