/*
 *  Configuration helper functions
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
#include "local.h"
#include "list.h"

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

typedef struct {
	struct filedesc *current;
	int unget;
	int ch;
	enum {
		UNTERMINATED_STRING = -1,
		UNTERMINATED_QUOTE = -2,
		UNEXPECTED_CHAR = -3,
		UNEXPECTED_EOF = -4,
	} error;
} input_t;

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
		break;
	default:
		fd->column++;
		break;
	}
	return c;
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
			char *file;
			snd_input_t *in;
			struct filedesc *fd;
			int err = get_delimstring(&file, '>', input);
			if (err < 0)
				return err;
			err = snd_input_stdio_open(&in, file, "r");
			if (err < 0)
				return err;
			fd = malloc(sizeof(*fd));
			if (!fd)
				return -ENOMEM;
			fd->name = file;
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
				return c;
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
	const size_t bufsize = 256;
	char _buf[bufsize];
	char *buf = _buf;
	size_t alloc = bufsize;
	size_t idx = 0;
	int c;
	while (1) {
		c = get_char(input);
		switch (c) {
		case '.':
			if (!id)
				break;
		case ' ':
		case '\f':
		case '\t':
		case '\n':
		case '\r':
		case EOF:
		case '=':
		case ',':
		case ';':
		case '{':
		case '}':
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
				memcpy(buf, _buf, old_alloc);
			} else
				buf = realloc(buf, alloc);
			if (!buf)
				return -ENOMEM;
		}
		buf[idx++] = c;
	}
	return 0;
}
			
static int get_delimstring(char **string, int delim, input_t *input)
{
	const size_t bufsize = 256;
	char _buf[bufsize];
	char *buf = _buf;
	size_t alloc = bufsize;
	size_t idx = 0;
	int c;
	while (1) {
		c = get_char(input);
		switch (c) {
		case EOF:
			input->error = UNTERMINATED_STRING;
			return -EINVAL;
		case '\\':
			c = get_quotedchar(input);
			if (c < 0) {
				input->error = UNTERMINATED_QUOTE;
				return -EINVAL;
			}
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
				memcpy(buf, _buf, old_alloc);
			} else
				buf = realloc(buf, alloc);
			if (!buf)
				return -ENOMEM;
		}
		buf[idx++] = c;
	}
}

/* Return 0 for free string, 1 for delimited string */
static int get_string(char **string, int id, input_t *input)
{
	int c = get_nonwhite(input);
	int err;
	switch (c) {
	case EOF:
		input->error = UNEXPECTED_EOF;
		return -EINVAL;
	case '=':
	case ',':
	case ';':
	case '.':
	case '{':
	case '}':
		input->error = UNEXPECTED_CHAR;
		return -EINVAL;
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

static int _snd_config_make(snd_config_t **config, char *id,
			    snd_config_type_t type)
{
	snd_config_t *n;
	n = calloc(1, sizeof(*n));
	if (n == NULL) {
		if (id)
			free(id);
		return -ENOMEM;
	}
	n->id = id;
	n->type = type;
	if (type == SND_CONFIG_TYPE_COMPOUND)
		INIT_LIST_HEAD(&n->u.compound.fields);
	*config = n;
	return 0;
}
	

static int _snd_config_make_add(snd_config_t **config, char *id,
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
		*result = n;
		return 0;
	}
	return -ENOENT;
}

static int parse_defs(snd_config_t *father, input_t *input);

static int parse_def(snd_config_t *father, input_t *input)
{
	char *id;
	int c;
	int err;
	snd_config_t *n;
	enum {MERGE, NOCREATE, REMOVE} mode;
	while (1) {
#if 0
		c = get_nonwhite(input);
		switch (c) {
		case '?':
			mode = NOCREATE;
			break;
		case '!':
			mode = REMOVE;
			break;
		default:
			mode = MERGE;
			unget_char(c, input);
		}
#else
		mode = MERGE;
#endif
		err = get_string(&id, 1, input);
		if (err < 0)
			return err;
		c = get_nonwhite(input);
		if (c != '.')
			break;
		if (_snd_config_search(father, id, -1, &n) == 0) {
			if (mode != REMOVE) {
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
		if (mode == NOCREATE) {
			SNDERR("%s does not exists", id);
			free(id);
			return -ENOENT;
		}
		err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_COMPOUND, father);
		if (err < 0)
			return err;
		n->u.compound.join = 1;
		father = n;
	}
	if (c == '=')
		c = get_nonwhite(input);
	if (_snd_config_search(father, id, -1, &n) == 0) {
		if (mode == REMOVE) {
			snd_config_delete(n);
			n = NULL;
		}
		else
			free(id);
	} else {
		n = NULL;
		if (mode == NOCREATE) {
			SNDERR("%s does not exists", id);
			free(id);
			return -ENOENT;
		}
	}
	switch (c) {
	case '{':
	{
		if (n) {
			if (n->type != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("%s is not a compound", id);
				return -EINVAL;
			}
		} else {
			err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_COMPOUND, father);
			if (err < 0)
				return err;
		}
		err = parse_defs(n, input);
		if (err < 0) {
			snd_config_delete(n);
			return err;
		}
		c = get_nonwhite(input);
		if (c != '}') {
			snd_config_delete(n);
			input->error = (c == EOF ? UNEXPECTED_EOF : UNEXPECTED_CHAR);
			return -EINVAL;
		}
		break;
	}
	default:
	{
		char *s;
		unget_char(c, input);
		err = get_string(&s, 0, input);
		if (err < 0)
			return err;
		if (!err && ((s[0] >= '0' && s[0] <= '9') || s[0] == '-')) {
			char *ptr;
			long i;
			errno = 0;
			i = strtol(s, &ptr, 0);
			if (*ptr == '.' || errno != 0) {
				double r;
				errno = 0;
				r = strtod(s, &ptr);
				if (errno == 0) {
					free(s);
					if (n) {
						if (n->type != SND_CONFIG_TYPE_REAL) {
							SNDERR("%s is not a real", id);
							return -EINVAL;
						}
					} else {
						err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_REAL, father);
						if (err < 0)
							return err;
					}
					n->u.real = r;
					break;
				}
			} else if (*ptr == '\0') {
				free(s);
				if (n) {
					if (n->type != SND_CONFIG_TYPE_INTEGER) {
						SNDERR("%s is not an integer", id);
						return -EINVAL;
					}
				} else {
					err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_INTEGER, father);
					if (err < 0)
						return err;
				}
				n->u.integer = i;
				break;
			}
		}
		if (n) {
			if (n->type != SND_CONFIG_TYPE_STRING) {
				SNDERR("%s is not a string", id);
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
	}
	}
	c = get_nonwhite(input);
	switch (c) {
	case ';':
	case ',':
		break;
	default:
		unget_char(c, input);
	}
	return err;
}
		
static int parse_defs(snd_config_t *father, input_t *input)
{
	while (1) {
		int c = get_nonwhite(input);
		int err;
		if (c == EOF)
			return 0;
		unget_char(c, input);
		if (c == '}')
			return 0;
		err = parse_def(father, input);
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
	switch (snd_enum_to_int(n->type)) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_output_printf(out, "%ld", n->u.integer);
		break;
	case SND_CONFIG_TYPE_REAL:
		snd_output_printf(out, "%16g", n->u.real);
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
	input.error = 0;
	err = parse_defs(config, &input);
	fd = input.current;
	if (err < 0) {
		if (input.error < 0) {
			const char *str;
			switch (input.error) {
			case UNTERMINATED_STRING:
				str = "Unterminated string";
				break;
			case UNTERMINATED_QUOTE:
				str = "Unterminated quote";
				break;
			case UNEXPECTED_CHAR:
				str = "Unexpected char";
				break;
			case UNEXPECTED_EOF:
				str = "Unexpected end of file";
				break;
			default:
				assert(0);
				break;
			}
			SNDERR("%s:%d:%d:%s", fd->name ? fd->name : "",
			    fd->line, fd->column, str);
		}
		snd_config_delete(config);
		goto _end;
	}
	if (get_char(&input) != EOF) {
		SNDERR("%s:%d:%d:Unexpected }", fd->name ? fd->name : "",
		    fd->line, fd->column);
		snd_config_delete(config);
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
 * \brief Remove a leaf config node (freeing all the related resources)
 * \param config Config node handle
 * \return 0 on success otherwise a negative error code
 */
int snd_config_delete(snd_config_t *config)
{
	assert(config);
	switch (snd_enum_to_int(config->type)) {
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
	return _snd_config_make(config, id1, type);
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

/**
 * \brief Search a node inside a config tree
 * \param config Config node handle
 * \param key Dot separated search key
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 */
int snd_config_search(snd_config_t *config, const char *key, snd_config_t **result)
{
	assert(config && key && result);
	while (1) {
		snd_config_t *n;
		int err;
		const char *p;
		if (config->type != SND_CONFIG_TYPE_COMPOUND)
			return -ENOENT;
		p = strchr(key, '.');
		if (p) {
			err = _snd_config_search(config, key, p - key, &n);
			if (err < 0)
				return err;
			config = n;
			key = p + 1;
		} else
			return _snd_config_search(config, key, -1, result);
	}
}

/**
 * \brief Search a node inside a config tree
 * \param config Config node handle
 * \param result Pointer to found node
 * \param ... one or more concatenated dot separated search key
 * \return 0 on success otherwise a negative error code
 */
int snd_config_searchv(snd_config_t *config,
		       snd_config_t **result, ...)
{
	snd_config_t *n;
	va_list arg;
	assert(config && result);
	va_start(arg, result);
	while (1) {
		const char *k = va_arg(arg, const char *);
		int err;
		if (!k)
			break;
		err = snd_config_search(config, k, &n);
		if (err < 0)
			return err;
		config = n;
	}
	va_end(arg);
	*result = n;
	return 0;
}

/**
 * \brief Search a node inside a config tree using alias
 * \param config Config node handle
 * \param base Key base (or NULL)
 * \param key Key suffix
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 *
 * If base.key is found and it's a string the value found is recursively
 * tried instead of suffix.
 */
int snd_config_search_alias(snd_config_t *config,
			    const char *base, const char *key,
			    snd_config_t **result)
{
	int err;
	assert(config && key && result);
	if (base) {
		err = snd_config_searchv(config, result, base, key, 0);
		if (err < 0)
			return err;
		while (snd_config_get_string(*result, &key) >= 0 &&
		       snd_config_searchv(config, result, base, key, 0) >= 0)
			;
	} else {
		err = snd_config_search(config, key, result);
		if (err < 0)
			return err;
		while (snd_config_get_string(*result, &key) >= 0 &&
		       snd_config_search(config, key, result) >= 0)
			;
	}
	return 0;
}

/** Environment variable containing files list for #snd_config_update */
#define ASOUND_CONFIGS_VAR "ASOUND_CONFIGS"

/** Default files used by #snd_config_update */
#define ASOUND_CONFIGS_DEFAULT DATADIR "/alsa/alsa.conf:/etc/asound.conf:~/.asoundrc"

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

/** 
 * \brief Update #snd_config rereading (if needed) files specified in
 * environment variable ASOUND_CONFIGS. If it's not set the default value is
 * "/usr/share/alsa/alsa.conf:/etc/asound.conf:~/.asoundrc"
 * \return 0 if no action is needed, 1 if tree has been rebuilt otherwise a negative error code
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
	struct finfo *fi;
	unsigned int fi_count;
	configs = getenv(ASOUND_CONFIGS_VAR);
	if (!configs)
		configs = ASOUND_CONFIGS_DEFAULT;
	for (k = 0, c = configs; (l = strcspn(c, ": ")) > 0; ) {
		c += l;
		k++;
		if (!*c)
			break;
		c++;
	}
	fi_count = k;
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
		}
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

typedef enum _snd_config_walk_pass {
	SND_CONFIG_WALK_PASS_PRE,
	SND_CONFIG_WALK_PASS_POST,
	SND_CONFIG_WALK_PASS_LEAF,
} snd_config_walk_pass_t;


/* Return 1 if node need to be attached to father */
typedef int (*snd_config_walk_callback_t)(snd_config_t *src,
					  snd_config_t **dst,
					  snd_config_walk_pass_t pass,
					  void *private_data);

static int snd_config_walk(snd_config_t *src,
			   snd_config_t **dst, 
			   snd_config_walk_callback_t callback,
			   void *private_data)
{
	int err;
	snd_config_iterator_t i, next;
	switch (snd_config_get_type(src)) {
	case SND_CONFIG_TYPE_COMPOUND:
		err = callback(src, dst, SND_CONFIG_WALK_PASS_PRE, private_data);
		if (err <= 0)
			return err;
		snd_config_for_each(i, next, src) {
			snd_config_t *s = snd_config_iterator_entry(i);
			snd_config_t *d = NULL;

			err = snd_config_walk(s, (dst && *dst) ? &d : NULL,
					      callback, private_data);
			if (err < 0)
				goto _error;
			if (err && d) {
				err = snd_config_add(*dst, d);
				if (err < 0)
					goto _error;
			}
		}
		err = callback(src, dst, SND_CONFIG_WALK_PASS_POST, private_data);
		if (err <= 0) {
		_error:
			if (dst && *dst)
				snd_config_delete(*dst);
		}
		break;
	default:
		err = callback(src, dst, SND_CONFIG_WALK_PASS_LEAF, private_data);
		break;
	}
	return err;
}

static int _snd_config_copy(snd_config_t *src,
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

int snd_config_copy(snd_config_t **dst,
		    snd_config_t *src)
{
	return snd_config_walk(src, dst, _snd_config_copy, NULL);
}

static int _snd_config_expand(snd_config_t *src,
			      snd_config_t **dst,
			      snd_config_walk_pass_t pass,
			      void *private_data)
{
	int err;
	const char *id = snd_config_get_id(src);
	snd_config_type_t type = snd_config_get_type(src);
	switch (pass) {
	case SND_CONFIG_WALK_PASS_PRE:
		if (strcmp(id, "$") == 0)
			return 0;
		err = snd_config_make_compound(dst, id, src->u.compound.join);
		if (err < 0)
			return err;
		break;
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
			err = snd_config_get_string(src, &s);
			if (s[0] == '$') {
				if (snd_config_search(vars, s + 1, &val) < 0)
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

static int safe_strtol(const char *str, long *val)
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
		if (err < 0)
			return err;
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
 * \brief Expand a node applying arguments
 * \param config Config node handle
 * \param args Arguments string
 * \param result Pointer to found node
 * \return 0 on success otherwise a negative error code
 */
int snd_config_expand(snd_config_t *config, const char *args,
		      snd_config_t **result)
{
	int err;
	snd_config_t *defs, *subs;
	err = snd_config_search(config, "$", &defs);
	if (err < 0)
		return -EINVAL;
	err = snd_config_top(&subs);
	if (err < 0)
		return err;
	err = load_defaults(subs, defs);
	if (err < 0)
		goto _end;
	err = parse_args(subs, args, defs);
	if (err < 0)
		goto _end;
	err = snd_config_walk(config, result, _snd_config_expand, subs);
	if (err < 0)
		goto _end;
	err = 1;
 _end:
	snd_config_delete(subs);
	return err;
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
