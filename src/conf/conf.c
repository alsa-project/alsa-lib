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

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "asoundlib.h"
#include "list.h"

#define SYS_ASOUNDRC "/etc/asound.conf"
#define USR_ASOUNDRC ".asoundrc"

struct filedesc {
	char *name;
	FILE *fp;
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
	c = getc(fd->fp);
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
			fclose(fd->fp);
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
			FILE *fp;
			struct filedesc *fd;
			int err = get_delimstring(&file, '>', input);
			if (err < 0)
				return err;
			fp = fopen(file, "r");
			if (!fp)
				return -errno;
			fd = malloc(sizeof(*fd));
			fd->name = file;
			fd->fp = fp;
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
		case '{':
		case '}':
		case ',':
		case ';':
		case '\'':
		case '"':
		case '\\':
		case '#':
		{
			char *s = malloc(idx + 1);
			unget_char(c, input);
			memcpy(s, buf, idx);
			s[idx] = '\0';
			*string = s;
			return 0;
		}
		default:
			break;
		}
		if (idx >= alloc) {
			size_t old_alloc = alloc;
			alloc += bufsize;
			if (old_alloc == bufsize) {
				buf = malloc(alloc);
				memcpy(buf, _buf, old_alloc);
			} else
				buf = realloc(buf, alloc);
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
			return -1;
		case '\\':
			c = get_quotedchar(input);
			if (c < 0) {
				input->error = UNTERMINATED_QUOTE;
				return -1;
			}
			break;
		default:
			if (c == delim) {
				char *s = malloc(idx + 1);
				memcpy(s, buf, idx);
				s[idx] = '\0';
				*string = s;
				return 0;
			}
		}
		if (idx >= alloc) {
			size_t old_alloc = alloc;
			alloc += bufsize;
			if (old_alloc == bufsize) {
				buf = malloc(alloc);
				memcpy(buf, _buf, old_alloc);
			} else
				buf = realloc(buf, alloc);
		}
		buf[idx++] = c;
	}
	return 0;
}

/* Return 0 for free string, 1 for delimited string */
static int get_string(char **string, int id, input_t *input)
{
	int c = get_nonwhite(input);
	int err;
	switch (c) {
	case EOF:
		input->error = UNEXPECTED_EOF;
		return -1;
	case '=':
#if 0
	  	/* I'm not sure to want unnamed fields */
		*string = 0;
		return 0;
#endif
	case '.':
	case '{':
	case '}':
	case ',':
	case ';':
		input->error = UNEXPECTED_CHAR;
		return -1;
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

static int _snd_config_search(snd_config_t *config, char *id, int len, snd_config_t **result)
{
	snd_config_iterator_t i;
	snd_config_foreach(i, config) {
		snd_config_t *n = snd_config_entry(i);
		if (len < 0) {
			if (strcmp(n->id, id) == 0) {
				*result = n;
				return 0;
			}
		} else {
			if (strlen(n->id) != (size_t) len)
				continue;
			if (memcmp(n->id, id, len) == 0) {
				*result = n;
				return 0;
			}
		}
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
				if (n->type != SND_CONFIG_TYPE_COMPOUND)
					return -EINVAL;
				n->u.compound.join = 1;
				father = n;
				free(id);
				continue;
			}
			snd_config_delete(n);
		}
		if (mode == NOCREATE) {
			free(id);
			return -ENOENT;
		}
		err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_COMPOUND, father);
		if (err < 0)
			return err;
		n->u.compound.join = 1;
		father = n;
	}
	if (c == '=' )
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
			free(id);
			return -ENOENT;
		}
	}
	switch (c) {
	case '{':
	{
		if (n) {
			if (n->type != SND_CONFIG_TYPE_COMPOUND)
				return -EINVAL;
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
			input->error = UNEXPECTED_CHAR;
			return -1;
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
						if (n->type != SND_CONFIG_TYPE_REAL)
							return -EINVAL;
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
					if (n->type != SND_CONFIG_TYPE_INTEGER)
						return -EINVAL;
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

int snd_config_top(snd_config_t **config)
{
	assert(config);
	return _snd_config_make(config, 0, SND_CONFIG_TYPE_COMPOUND);
}

int snd_config_load(snd_config_t *config, FILE *fp)
{
	int err;
	input_t input;
	struct filedesc *fd;
	assert(config && fp);
	fd = malloc(sizeof(*fd));
	fd->name = NULL;
	fd->fp = fp;
	fd->line = 1;
	fd->column = 0;
	fd->next = NULL;
	input.current = fd;
	input.unget = 0;
	err = parse_defs(config, &input);
	if (err < 0) {
		snd_config_delete(config);
		return err;
	}
	if (get_char(&input) != EOF) {
		snd_config_delete(config);
		return -1;
	}
	return 0;
}

int snd_config_add(snd_config_t *father, snd_config_t *leaf)
{
	snd_config_iterator_t i;
	assert(father && leaf);
	snd_config_foreach(i, father) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(leaf->id, n->id) == 0)
			return -EEXIST;
	}
	leaf->father = father;
	list_add_tail(&leaf->list, &father->u.compound.fields);
	return 0;
}

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
			snd_config_t *leaf = snd_config_entry(i);
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
	return 0;
}

int snd_config_make(snd_config_t **config, char *id,
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

int snd_config_integer_make(snd_config_t **config, char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_INTEGER);
}

int snd_config_real_make(snd_config_t **config, char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_REAL);
}

int snd_config_string_make(snd_config_t **config, char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_STRING);
}

int snd_config_compound_make(snd_config_t **config, char *id,
			     int join)
{
	int err;
	err = snd_config_make(config, id, SND_CONFIG_TYPE_COMPOUND);
	if (err < 0)
		return err;
	(*config)->u.compound.join = join;
	return 0;
}

int snd_config_integer_set(snd_config_t *config, long value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_INTEGER)
		return -EINVAL;
	config->u.integer = value;
	return 0;
}

int snd_config_real_set(snd_config_t *config, double value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_REAL)
		return -EINVAL;
	config->u.real = value;
	return 0;
}

int snd_config_string_set(snd_config_t *config, char *value)
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

int snd_config_set(snd_config_t *config, ...)
{
	va_list arg;
	va_start(arg, config);
	assert(config);
	switch (config->type) {
	case SND_CONFIG_TYPE_INTEGER:
		config->u.integer = va_arg(arg, long);
		break;
	case SND_CONFIG_TYPE_REAL:
		config->u.real = va_arg(arg, double);
		break;
	case SND_CONFIG_TYPE_STRING:
		config->u.string = va_arg(arg, char *);
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	va_end(arg);
	return 0;
}

int snd_config_integer_get(snd_config_t *config, long *ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_INTEGER)
		return -EINVAL;
	*ptr = config->u.integer;
	return 0;
}

int snd_config_real_get(snd_config_t *config, double *ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_REAL)
		return -EINVAL;
	*ptr = config->u.real;
	return 0;
}

int snd_config_string_get(snd_config_t *config, char **ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_STRING)
		return -EINVAL;
	*ptr = config->u.string;
	return 0;
}

int snd_config_get(snd_config_t *config, void *ptr)
{
	assert(config && ptr);
	switch (config->type) {
	case SND_CONFIG_TYPE_INTEGER:
		* (long*) ptr = config->u.integer;
		break;
	case SND_CONFIG_TYPE_REAL:
		* (double*) ptr = config->u.real;
		break;
	case SND_CONFIG_TYPE_STRING:
		* (char **) ptr = config->u.string;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	return 0;
}

void string_print(char *str, int id, FILE *fp)
{
	unsigned char *p = str;
	if (!id) {
		switch (*p) {
		case '0' ... '9':
		case '-':
			goto quoted;
		}
	}
 loop:
	switch (*p) {
	case 0:
		goto nonquoted;
	case 1 ... 31:
	case 127 ... 255:
	case ' ':
	case '=':
	case '.':
	case '{':
	case '}':
	case ';':
	case ',':
	case '\'':
	case '"':
		goto quoted;
	default:
		p++;
		goto loop;
	}
 nonquoted:
	fputs(str, fp);
	return;
 quoted:
	putc('\'', fp);
	p = str;
	while (*p) {
		int c;
		c = *p;
		switch (c) {
		case '\n':
			putc('\\', fp);
			putc('n', fp);
			break;
		case '\t':
			putc('\\', fp);
			putc('t', fp);
			break;
		case '\v':
			putc('\\', fp);
			putc('v', fp);
			break;
		case '\b':
			putc('\\', fp);
			putc('b', fp);
			break;
		case '\r':
			putc('\\', fp);
			putc('r', fp);
			break;
		case '\f':
			putc('\\', fp);
			putc('f', fp);
			break;
		case '\'':
			putc('\\', fp);
			putc(c, fp);
			break;
		case 32 ... '\'' - 1:
		case '\'' + 1 ... 126:
			putc(c, fp);
			break;
		default:
			fprintf(fp, "\\%04o", c);
			break;
		}
		p++;
	}
	putc('\'', fp);
}

static int _snd_config_save_leaves(snd_config_t *config, FILE *fp, unsigned int level, unsigned int joins);

static int _snd_config_save_leaf(snd_config_t *n, FILE *fp, 
				 unsigned int level)
{
	int err;
	unsigned int k;
	switch (n->type) {
	case SND_CONFIG_TYPE_INTEGER:
		fprintf(fp, "%ld", n->u.integer);
		break;
	case SND_CONFIG_TYPE_REAL:
		fprintf(fp, "%16g", n->u.real);
		break;
	case SND_CONFIG_TYPE_STRING:
		string_print(n->u.string, 0, fp);
		break;
	case SND_CONFIG_TYPE_COMPOUND:
		putc('{', fp);
		putc('\n', fp);
		err = _snd_config_save_leaves(n, fp, level + 1, 0);
		if (err < 0)
			return err;
		for (k = 0; k < level; ++k) {
			putc('\t', fp);
		}
		putc('}', fp);
		break;
	}
	return 0;
}

static void id_print(snd_config_t *n, FILE *fp, unsigned int joins)
{
	if (joins > 0) {
		assert(n->father);
		id_print(n->father, fp, joins - 1);
		putc('.', fp);
	}
	string_print(n->id, 1, fp);
}

static int _snd_config_save_leaves(snd_config_t *config, FILE *fp, unsigned int level, unsigned int joins)
{
	unsigned int k;
	int err;
	snd_config_iterator_t i;
	assert(config && fp);
	snd_config_foreach(i, config) {
		snd_config_t *n = snd_config_entry(i);
		if (n->type == SND_CONFIG_TYPE_COMPOUND &&
		    n->u.compound.join) {
			err = _snd_config_save_leaves(n, fp, level, joins + 1);
			if (err < 0)
				return err;
			continue;
		}
		for (k = 0; k < level; ++k) {
			putc('\t', fp);
		}
		id_print(n, fp, joins);
		putc(' ', fp);
		putc('=', fp);
		putc(' ', fp);
		err = _snd_config_save_leaf(n, fp, level);
		if (err < 0)
			return err;
		putc(';', fp);
		putc('\n', fp);
	}
	return 0;
}

int snd_config_save(snd_config_t *config, FILE *fp)
{
	assert(config && fp);
	return _snd_config_save_leaves(config, fp, 0, 0);
}

int snd_config_search(snd_config_t *config, char *key, snd_config_t **result)
{
	assert(config && key && result);
	while (1) {
		snd_config_t *n;
		int err;
		char *p = strchr(key, '.');
		if (config->type != SND_CONFIG_TYPE_COMPOUND)
			return -ENOENT;
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

int snd_config_searchv(snd_config_t *config,
		       snd_config_t **result, ...)
{
	snd_config_t *n;
	va_list arg;
	assert(config && result);
	va_start(arg, result);
	while (1) {
		char *k = va_arg(arg, char *);
		int err;
		if (!k)
			break;
		if (config->type != SND_CONFIG_TYPE_COMPOUND)
			return -ENOENT;
		err = _snd_config_search(config, k, -1, &n);
		if (err < 0)
			return err;
		config = n;
	}
	va_end(arg);
	*result = n;
	return 0;
}

snd_config_t *snd_config = 0;
static dev_t sys_asoundrc_device;
static ino_t sys_asoundrc_inode;
static time_t sys_asoundrc_mtime;
static dev_t usr_asoundrc_device;
static ino_t usr_asoundrc_inode;
static time_t usr_asoundrc_mtime;
	
int snd_config_update()
{
	int err;
	char *usr_asoundrc = NULL;
	char *home = getenv("HOME");
	struct stat usr_st, sys_st;
	int reload;
	FILE *fp;
	if (home) {
		size_t len = strlen(home);
		size_t len1 = strlen(USR_ASOUNDRC);
		usr_asoundrc = alloca(len + len1 + 2);
		memcpy(usr_asoundrc, home, len);
		usr_asoundrc[len] = '/';
		memcpy(usr_asoundrc + len + 1, USR_ASOUNDRC, len1);
		usr_asoundrc[len + 1 + len1] = '\0';
	}
	reload = (snd_config == NULL);
	if (stat(SYS_ASOUNDRC, &sys_st) == 0 &&
	    (sys_st.st_dev != sys_asoundrc_device ||
	     sys_st.st_ino != sys_asoundrc_inode ||
	     sys_st.st_mtime != sys_asoundrc_mtime))
		reload = 1;
	if (stat(usr_asoundrc, &usr_st) == 0 &&
	    (usr_st.st_dev != usr_asoundrc_device ||
	     usr_st.st_ino != usr_asoundrc_inode ||
	     usr_st.st_mtime != usr_asoundrc_mtime))
		reload = 1;
	if (!reload)
		return 0;
	if (snd_config) {
		err = snd_config_delete(snd_config);
		if (err < 0)
			return err;
		snd_config = 0;
	}
	err = snd_config_top(&snd_config);
	if (err < 0)
		return err;
	fp = fopen(SYS_ASOUNDRC, "r");
	if (fp) {
		err = snd_config_load(snd_config, fp);
		fclose(fp);
		if (err < 0) {
			snd_config = NULL;
			return err;
		}
		sys_asoundrc_device = sys_st.st_dev;
		sys_asoundrc_inode = sys_st.st_ino;
		sys_asoundrc_mtime = sys_st.st_mtime;
	}
	fp = fopen(usr_asoundrc, "r");
	if (fp) {
		err = snd_config_load(snd_config, fp);
		fclose(fp);
		if (err < 0) {
			snd_config = NULL;
			return err;
		}
		usr_asoundrc_device = usr_st.st_dev;
		usr_asoundrc_inode = usr_st.st_ino;
		usr_asoundrc_mtime = usr_st.st_mtime;
	}
	return 0;
}
