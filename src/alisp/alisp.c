/*
 *  ALSA lisp implementation
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
 *
 *  Based on work of Sandro Sigala (slisp-1.2)
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <assert.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <err.h>
#include <wordexp.h>

#define alisp_seq_iterator alisp_object

#include "local.h"
#include "alisp.h"
#include "alisp_local.h"


#define ALISP_FREE_OBJ_POOL	500	/* free objects above this pool */
#define ALISP_AUTO_GC_THRESHOLD	200	/* run automagically garbage-collect when this threshold is reached */
#define ALISP_MAIN_ID		"---alisp---main---"

struct alisp_object alsa_lisp_nil;
struct alisp_object alsa_lisp_t;

/* parser prototypes */
static struct alisp_object * parse_object(struct alisp_instance *instance, int havetoken);
static void princ_cons(snd_output_t *out, struct alisp_object * p);
static void princ_object(snd_output_t *out, struct alisp_object * p);
static struct alisp_object * eval(struct alisp_instance *instance, struct alisp_object * p);

/* functions */
static struct alisp_object *F_eval(struct alisp_instance *instance, struct alisp_object *);
static struct alisp_object *F_progn(struct alisp_instance *instance, struct alisp_object *);

/* others */
static int alisp_include_file(struct alisp_instance *instance, const char *filename);

/*
 *  object handling
 */

static void nomem(void)
{
	SNDERR("alisp: no enough memory");
}

static void lisp_verbose(struct alisp_instance *instance, const char *fmt, ...)
{
	va_list ap;

	if (!instance->verbose)
		return;
	va_start(ap, fmt);
	snd_output_printf(instance->vout, "alisp: ");
	snd_output_vprintf(instance->vout, fmt, ap);
	snd_output_putc(instance->vout, '\n');
	va_end(ap);
}

static void lisp_error(struct alisp_instance *instance, const char *fmt, ...)
{
	va_list ap;

	if (!instance->warning)
		return;
	va_start(ap, fmt);
	snd_output_printf(instance->eout, "alisp error: ");
	snd_output_vprintf(instance->eout, fmt, ap);
	snd_output_putc(instance->eout, '\n');
	va_end(ap);
}

static void lisp_warn(struct alisp_instance *instance, const char *fmt, ...)
{
	va_list ap;

	if (!instance->warning)
		return;
	va_start(ap, fmt);
	snd_output_printf(instance->wout, "alisp warning: ");
	snd_output_vprintf(instance->wout, fmt, ap);
	snd_output_putc(instance->wout, '\n');
	va_end(ap);
}

static void lisp_debug(struct alisp_instance *instance, const char *fmt, ...)
{
	va_list ap;

	if (!instance->debug)
		return;
	va_start(ap, fmt);
	snd_output_printf(instance->dout, "alisp debug: ");
	snd_output_vprintf(instance->dout, fmt, ap);
	snd_output_putc(instance->dout, '\n');
	va_end(ap);
}

static struct alisp_object * new_object(struct alisp_instance *instance, int type)
{
	struct alisp_object * p;

	if (instance->free_objs_list == NULL) {
		p = (struct alisp_object *)malloc(sizeof(struct alisp_object));
		if (p == NULL) {
			nomem();
			return NULL;
		}
		++instance->gc_thr_objs;
		lisp_debug(instance, "allocating cons %p", p);
	} else {
		p = instance->free_objs_list;
		instance->free_objs_list = instance->free_objs_list->next;
		--instance->free_objs;
		lisp_debug(instance, "recycling cons %p", p);
	}

	p->next = instance->used_objs_list;
	instance->used_objs_list = p;

	p->type = type;
	if (type == ALISP_OBJ_CONS) {
		p->value.c.car = &alsa_lisp_nil;
		p->value.c.cdr = &alsa_lisp_nil;
	}
	p->gc = 1;

	++instance->used_objs;
	if (instance->used_objs + instance->free_objs > instance->max_objs)
		instance->max_objs = instance->used_objs + instance->free_objs;

	return p;
}

static void free_object(struct alisp_object * p)
{
	switch (p->type) {
	case ALISP_OBJ_STRING:
		if (p->value.s)
			free(p->value.s);
		break;
	case ALISP_OBJ_IDENTIFIER:
		if (p->value.id)
			free(p->value.id);
		break;
	}
}

static void free_objects(struct alisp_instance *instance)
{
	struct alisp_object * p, * next;

	for (p = instance->used_objs_list; p != NULL; p = next) {
		next = p->next;
		free_object(p);
		free(p);
	}
	for (p = instance->free_objs_list; p != NULL; p = next) {
		next = p->next;
		free(p);
	}
}

static struct alisp_object * search_object_identifier(struct alisp_instance *instance, const char *s)
{
	struct alisp_object * p;

	for (p = instance->used_objs_list; p != NULL; p = p->next)
		if (p->type == ALISP_OBJ_IDENTIFIER && !strcmp(p->value.id, s))
			return p;

	return NULL;
}

static struct alisp_object * search_object_string(struct alisp_instance *instance, const char *s)
{
	struct alisp_object * p;

	for (p = instance->used_objs_list; p != NULL; p = p->next)
		if (p->type == ALISP_OBJ_STRING && !strcmp(p->value.s, s))
			return p;

	return NULL;
}

static struct alisp_object * search_object_integer(struct alisp_instance *instance, long in)
{
	struct alisp_object * p;

	for (p = instance->used_objs_list; p != NULL; p = p->next)
		if (p->type == ALISP_OBJ_INTEGER && p->value.i == in)
			return p;

	return NULL;
}

static struct alisp_object * search_object_float(struct alisp_instance *instance, double in)
{
	struct alisp_object * p;

	for (p = instance->used_objs_list; p != NULL; p = p->next)
		if (p->type == ALISP_OBJ_FLOAT && p->value.f == in)
			return p;

	return NULL;
}

static struct alisp_object * search_object_pointer(struct alisp_instance *instance, const void *ptr)
{
	struct alisp_object * p;

	for (p = instance->used_objs_list; p != NULL; p = p->next)
		if (p->type == ALISP_OBJ_POINTER && p->value.ptr == ptr)
			return p;

	return NULL;
}

static struct alisp_object * new_integer(struct alisp_instance *instance, long value)
{
	struct alisp_object * obj;
	
	obj = search_object_integer(instance, value);
	if (obj != NULL)
		return obj;
	obj = new_object(instance, ALISP_OBJ_INTEGER);
	if (obj)
		obj->value.i = value;
	return obj;
}

static struct alisp_object * new_float(struct alisp_instance *instance, double value)
{
	struct alisp_object * obj;
	
	obj = search_object_float(instance, value);
	if (obj != NULL)
		return obj;
	obj = new_object(instance, ALISP_OBJ_FLOAT);
	if (obj)
		obj->value.f = value;
	return obj;
}

static struct alisp_object * new_string(struct alisp_instance *instance, const char *str)
{
	struct alisp_object * obj;
	
	obj = search_object_string(instance, str);
	if (obj != NULL)
		return obj;
	obj = new_object(instance, ALISP_OBJ_STRING);
	if (obj && (obj->value.s = strdup(str)) == NULL) {
		nomem();
		return NULL;
	}
	return obj;
}

static struct alisp_object * new_identifier(struct alisp_instance *instance, const char *id)
{
	struct alisp_object * obj;
	
	obj = search_object_identifier(instance, id);
	if (obj != NULL)
		return obj;
	obj = new_object(instance, ALISP_OBJ_IDENTIFIER);
	if (obj && (obj->value.id = strdup(id)) == NULL) {
		nomem();
		return NULL;
	}
	return obj;
}

static struct alisp_object * new_pointer(struct alisp_instance *instance, const void *ptr)
{
	struct alisp_object * obj;
	
	obj = search_object_pointer(instance, ptr);
	if (obj != NULL)
		return obj;
	obj = new_object(instance, ALISP_OBJ_POINTER);
	if (obj)
		obj->value.ptr = ptr;
	return obj;
}

static struct alisp_object * new_cons_pointer(struct alisp_instance * instance, const char *ptr_id, void *ptr)
{
	struct alisp_object * lexpr;

	if (ptr == NULL)
		return &alsa_lisp_nil;
	lexpr = new_object(instance, ALISP_OBJ_CONS);
	if (lexpr == NULL)
		return NULL;
	lexpr->value.c.car = new_string(instance, ptr_id);
	if (lexpr->value.c.car == NULL)
		return NULL;
	lexpr->value.c.cdr = new_pointer(instance, ptr);
	if (lexpr->value.c.cdr == NULL)
		return NULL;
	return lexpr;
}

void alsa_lisp_init_objects(void) __attribute__ ((constructor));

void alsa_lisp_init_objects(void)
{
	memset(&alsa_lisp_nil, 0, sizeof(alsa_lisp_nil));
	alsa_lisp_nil.type = ALISP_OBJ_NIL;
	memset(&alsa_lisp_t, 0, sizeof(alsa_lisp_t));
	alsa_lisp_t.type = ALISP_OBJ_T;
}

/*
 * lexer
 */ 

static int xgetc(struct alisp_instance *instance)
{
	instance->charno++;
	if (instance->lex_bufp > instance->lex_buf)
		return *--(instance->lex_bufp);
	return snd_input_getc(instance->in);
}

static inline void xungetc(struct alisp_instance *instance, int c)
{
	*(instance->lex_bufp)++ = c;
	instance->charno--;
}

static int init_lex(struct alisp_instance *instance)
{
	instance->charno = instance->lineno = 1;
	instance->token_buffer_max = 10;
	if ((instance->token_buffer = (char *)malloc(instance->token_buffer_max)) == NULL) {
		nomem();
		return -ENOMEM;
	}
	instance->lex_bufp = instance->lex_buf;
	return 0;
}

static void done_lex(struct alisp_instance *instance)
{
	if (instance->token_buffer)
		free(instance->token_buffer);
}

static char * extend_buf(struct alisp_instance *instance, char *p)
{
	int off = p - instance->token_buffer;

	instance->token_buffer_max += 10;
	instance->token_buffer = (char *)realloc(instance->token_buffer, instance->token_buffer_max);
	if (instance->token_buffer == NULL) {
		nomem();
		return NULL;
	}

	return instance->token_buffer + off;
}

static int gettoken(struct alisp_instance *instance)
{
	char *p;
	int c;

	for (;;) {
		c = xgetc(instance);
		switch (c) {
		case '\n':
			++instance->lineno;
			break;

		case ' ': case '\f': case '\t': case '\v': case '\r':
			break;

		case ';':
			/* Comment: ";".*"\n" */
			while ((c = xgetc(instance)) != '\n' && c != EOF)
				;
			if (c != EOF)
				++instance->lineno;
			break;

		case '?':
			/* Character: "?". */
			c = xgetc(instance);
			sprintf(instance->token_buffer, "%d", c);
			return instance->thistoken = ALISP_INTEGER;

		case '-':
			/* Minus sign: "-". */
			c = xgetc(instance);
			if (!isdigit(c)) {
				xungetc(instance, c);
				c = '-';
				goto got_id;
			}
			xungetc(instance, c);
			c = '-';
			/* FALLTRHU */

		case '0':
		case '1': case '2': case '3':
		case '4': case '5': case '6':
		case '7': case '8': case '9':
			/* Integer: [0-9]+ */
			p = instance->token_buffer;
			instance->thistoken = ALISP_INTEGER;
			do {
			      __ok:
				if (p - instance->token_buffer >= instance->token_buffer_max) {
					p = extend_buf(instance, p);
					if (p == NULL)
						return instance->thistoken = EOF;
				}
				*p++ = c;
				c = xgetc(instance);
				if (c == '.' && instance->thistoken == ALISP_INTEGER) {
					c = xgetc(instance);
					xungetc(instance, c);
					if (isdigit(c)) {
						instance->thistoken = ALISP_FLOAT;
						c = '.';
						goto __ok;
					} else {
						c = '.';
					}
				} else if (c == 'e' && instance->thistoken == ALISP_FLOAT) {
					c = xgetc(instance);
					if (isdigit(c)) {
						instance->thistoken = ALISP_FLOATE;
						goto __ok;
					}
				}
			} while (isdigit(c));
			xungetc(instance, c);
			*p = '\0';
			return instance->thistoken;

		got_id:
		case '_': case '+': case '*': case '/': case '%':
		case '<': case '>': case '=': case '&':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
		case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
		case 's': case 't': case 'u': case 'v': case 'w': case 'x':
		case 'y': case 'z':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
		case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
		case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z':
			/* Identifier: [-/+*%<>=&a-zA-Z_][-/+*%<>=&a-zA-Z_0-9]* */
			p = instance->token_buffer;
			do {
				if (p - instance->token_buffer >= instance->token_buffer_max) {
					p = extend_buf(instance, p);
					if (p == NULL)
						return instance->thistoken = EOF;
				}
				*p++ = c;
				c = xgetc(instance);
			} while (isalnum(c) || strchr("_-+*/%<>=&", c) != NULL);
			xungetc(instance, c);
			*p = '\0';
			return instance->thistoken = ALISP_IDENTIFIER;

		case '"':
			/* String: "\""([^"]|"\\".)*"\"" */
			p = instance->token_buffer;
			while ((c = xgetc(instance)) != '"' && c != EOF) {
				if (p - instance->token_buffer >= instance->token_buffer_max) {
					p = extend_buf(instance, p);
					if (p == NULL)
						return instance->thistoken = EOF;
				}
				if (c == '\\') {
					c = xgetc(instance);
					switch (c) {
					case '\n': ++instance->lineno; break;
					case 'a': *p++ = '\a'; break;
					case 'b': *p++ = '\b'; break;
					case 'f': *p++ = '\f'; break;
					case 'n': *p++ = '\n'; break;
					case 'r': *p++ = '\r'; break;
					case 't': *p++ = '\t'; break;
					case 'v': *p++ = '\v'; break;
					default: *p++ = c;
					}
				} else {
					if (c == '\n')
						++instance->lineno;
					*p++ = c;
				}
			}
			*p = '\0';
			return instance->thistoken = ALISP_STRING;

		default:
			return instance->thistoken = c;
		}
	}
}

/*
 *  parser
 */

static struct alisp_object * parse_form(struct alisp_instance *instance)
{
	int thistoken;
	struct alisp_object * p, * first = NULL, * prev = NULL;

	while ((thistoken = gettoken(instance)) != ')' && thistoken != EOF) {
		/*
		 * Parse a dotted pair notation.
		 */
		if (thistoken == '.') {
			thistoken = gettoken(instance);
			if (prev == NULL) {
				lisp_error(instance, "unexpected `.'");
				return NULL;
			}
			prev->value.c.cdr = parse_object(instance, 1);
			if (prev->value.c.cdr == NULL)
				return NULL;
			if ((thistoken = gettoken(instance)) != ')') {
				lisp_error(instance, "expected `)'");
				return NULL;
			}
			break;
		}

		p = new_object(instance, ALISP_OBJ_CONS);
		if (p == NULL)
			return NULL;

		if (first == NULL)
			first = p;
		if (prev != NULL)
			prev->value.c.cdr = p;

		p->value.c.car = parse_object(instance, 1);
		if (p->value.c.car == NULL)
			return NULL;
		prev = p;
	}

	if (first == NULL)
		return &alsa_lisp_nil;
	else
		return first;
}

static struct alisp_object * quote_object(struct alisp_instance *instance, struct alisp_object * obj)
{
	struct alisp_object * p;

	if (obj == NULL)
		return NULL;

	p = new_object(instance, ALISP_OBJ_CONS);
	if (p == NULL)
		return NULL;

	p->value.c.car = new_identifier(instance, "quote");
	if (p->value.c.car == NULL)
		return NULL;
	p->value.c.cdr = new_object(instance, ALISP_OBJ_CONS);
	if (p->value.c.cdr == NULL)
		return NULL;

	p->value.c.cdr->value.c.car = obj;
	return p;
}

static inline struct alisp_object * parse_quote(struct alisp_instance *instance)
{
	return quote_object(instance, parse_object(instance, 0));
}

static struct alisp_object * parse_object(struct alisp_instance *instance, int havetoken)
{
	int thistoken;
	struct alisp_object * p = NULL;

	if (!havetoken)
		thistoken = gettoken(instance);
	else
		thistoken = instance->thistoken;

	switch (thistoken) {
	case EOF:
		break;
	case '(':
		p = parse_form(instance);
		break;
	case '\'':
		p = parse_quote(instance);
		break;
	case ALISP_IDENTIFIER:
		if (!strcmp(instance->token_buffer, "t"))
			p = &alsa_lisp_t;
		else if (!strcmp(instance->token_buffer, "nil"))
			p = &alsa_lisp_nil;
		else {
			p = new_identifier(instance, instance->token_buffer);
		}
		break;
	case ALISP_INTEGER: {
		p = new_integer(instance, atol(instance->token_buffer));
		break;
	}
	case ALISP_FLOAT:
	case ALISP_FLOATE: {
		p = new_float(instance, atof(instance->token_buffer));
		break;
	}
	case ALISP_STRING:
		p = new_string(instance, instance->token_buffer);
		break;
	default:
		lisp_warn(instance, "%d:%d: unexpected character `%c'", instance->lineno, instance->charno, thistoken);
		break;
	}

	return p;
}

/*
 *  object manipulation
 */

static struct alisp_object_pair * set_object(struct alisp_instance *instance, struct alisp_object * name, struct alisp_object * value)
{
	struct alisp_object_pair *p;

	if (name->value.id == NULL)
		return NULL;

	for (p = instance->setobjs_list; p != NULL; p = p->next)
		if (p->name->value.id != NULL &&
		    !strcmp(name->value.id, p->name->value.id)) {
			p->value = value;
			return p;
		}

	p = (struct alisp_object_pair *)malloc(sizeof(struct alisp_object_pair));
	if (p == NULL) {
		nomem();
		return NULL;
	}
	p->next = instance->setobjs_list;
	instance->setobjs_list = p;
	p->name = name;
	p->value = value;
	return p;
}

static struct alisp_object * unset_object1(struct alisp_instance *instance, const char *id)
{
	struct alisp_object * res;
	struct alisp_object_pair *p, *p1;

	for (p = instance->setobjs_list, p1 = NULL; p != NULL; p1 = p, p = p->next) {
		if (p->name->value.id != NULL &&
		    !strcmp(id, p->name->value.id)) {
		    	if (p1)
		    		p1->next = p->next;
		    	else
		    		instance->setobjs_list = p->next;
		    	res = p->value;
		    	free(p);
		    	return res;
		}
	}
	
	return &alsa_lisp_nil;
}

static inline struct alisp_object * unset_object(struct alisp_instance *instance, struct alisp_object * name)
{
	return unset_object1(instance, name->value.id);
}

static struct alisp_object * get_object1(struct alisp_instance *instance, const char *id)
{
	struct alisp_object_pair *p;

	for (p = instance->setobjs_list; p != NULL; p = p->next) {
		if (p->name->value.id != NULL &&
		    !strcmp(id, p->name->value.id))
			return p->value;
	}

	return &alsa_lisp_nil;
}

static inline struct alisp_object * get_object(struct alisp_instance *instance, struct alisp_object * name)
{
	return get_object1(instance, name->value.id);
}

static void dump_objects(struct alisp_instance *instance, const char *fname)
{
	struct alisp_object_pair *p;
	snd_output_t *out;
	int err;

	if (!strcmp(fname, "-"))
		err = snd_output_stdio_attach(&out, stdout, 0);
	else
		err = snd_output_stdio_open(&out, fname, "w+");
	if (err < 0) {
		SNDERR("alisp: cannot open file '%s' for writting (%s)", fname, snd_strerror(errno));
		return;
	}

	for (p = instance->setobjs_list; p != NULL; p = p->next) {
		if (p->value->type == ALISP_OBJ_CONS &&
		    p->value->value.c.car->type == ALISP_OBJ_IDENTIFIER &&
		    !strcmp(p->value->value.c.car->value.id, "lambda")) {
		    	snd_output_printf(out, "(defun %s ", p->name->value.id);
		    	princ_cons(out, p->value->value.c.cdr);
		    	snd_output_printf(out, ")\n");
		    	continue;
		}
		if (!strcmp(p->name->value.id, ALISP_MAIN_ID))	/* internal thing */
			continue;
		snd_output_printf(out, "(setq %s '", p->name->value.id);
 		princ_object(out, p->value);
		snd_output_printf(out, ")\n");
	}
	
	snd_output_close(out);
}

static const char *obj_type_str(struct alisp_object * p)
{
	switch (p->type) {
	case ALISP_OBJ_NIL: return "nil";
	case ALISP_OBJ_T: return "t";
	case ALISP_OBJ_INTEGER: return "integer";
	case ALISP_OBJ_FLOAT: return "float";
	case ALISP_OBJ_IDENTIFIER: return "identifier";
	case ALISP_OBJ_STRING: return "string";
	case ALISP_OBJ_POINTER: return "pointer";
	case ALISP_OBJ_CONS: return "cons";
	default: assert(0);
	}
}

static void print_obj_lists(struct alisp_instance *instance, snd_output_t *out)
{
	struct alisp_object * p;

	snd_output_printf(out, "** used objects\n");
	for (p = instance->used_objs_list; p != NULL; p = p->next)
		snd_output_printf(out, "**   %p (%s)\n", p, obj_type_str(p));
	snd_output_printf(out, "** free objects\n");
	for (p = instance->free_objs_list; p != NULL; p = p->next)
		snd_output_printf(out, "**   %p (%s)\n", p, obj_type_str(p));
}

static void dump_obj_lists(struct alisp_instance *instance, const char *fname)
{
	snd_output_t *out;
	int err;

	if (!strcmp(fname, "-"))
		err = snd_output_stdio_attach(&out, stdout, 0);
	else
		err = snd_output_stdio_open(&out, fname, "w+");
	if (err < 0) {
		SNDERR("alisp: cannot open file '%s' for writting (%s)", fname, snd_strerror(errno));
		return;
	}

	print_obj_lists(instance, out);

	snd_output_close(out);
}

/*
 *  garbage collection
 */

static void tag_tree(struct alisp_instance *instance, struct alisp_object * p)
{
	if (p->gc == instance->gc_id)
		return;

	p->gc = instance->gc_id;

	if (p->type == ALISP_OBJ_CONS) {
		tag_tree(instance, p->value.c.car);
		tag_tree(instance, p->value.c.cdr);
	}
}

static void tag_whole_tree(struct alisp_instance *instance)
{
	struct alisp_object_pair *p;

	for (p = instance->setobjs_list; p != NULL; p = p->next) {
		tag_tree(instance, p->name);
		tag_tree(instance, p->value);
	}
}
       
static void do_garbage_collect(struct alisp_instance *instance)
{
	struct alisp_object * p, * new_used_objs_list = NULL, * next;
	struct alisp_object_pair * op, * new_set_objs_list = NULL, * onext;

	/*
	 * remove nil variables
	 */
	for (op = instance->setobjs_list; op != NULL; op = onext) {
		onext = op->next;
		if (op->value->type == ALISP_OBJ_NIL) {
			free(op);
		} else {
			op->next = new_set_objs_list;
			new_set_objs_list = op;
		}
	}
	instance->setobjs_list = new_set_objs_list;
	
	tag_whole_tree(instance);

	/*
	 * Search in the object vector.
	 */
	for (p = instance->used_objs_list; p != NULL; p = next) {
		next = p->next;
		if (p->gc != instance->gc_id && p->gc > 0) {
			/* Remove unreferenced object. */
			lisp_debug(instance, "** collecting cons %p", p);
			free_object(p);

			if (instance->free_objs < ALISP_FREE_OBJ_POOL) {
				p->next = instance->free_objs_list;
				instance->free_objs_list = p;
				++instance->free_objs;
				if (instance->gc_thr_objs > 0)
					instance->gc_thr_objs--;
			} else {
				free(p);
			}

			--instance->used_objs;
		} else {
			/* The object is referenced somewhere. */
			p->next = new_used_objs_list;
			new_used_objs_list = p;
		}
	}

	instance->used_objs_list = new_used_objs_list;
}

static inline void auto_garbage_collect(struct alisp_instance *instance)
{
	if (instance->gc_thr_objs >= ALISP_AUTO_GC_THRESHOLD) {
		do_garbage_collect(instance);
		instance->gc_thr_objs = 0;
	}
}

static void garbage_collect(struct alisp_instance *instance)
{
	if (++instance->gc_id == 255)
		instance->gc_id = 1;
	do_garbage_collect(instance);
}

/*
 *  functions
 */

static int count_list(struct alisp_object * p)
{
	int i = 0;

	while (p != &alsa_lisp_nil && p->type == ALISP_OBJ_CONS)
		p = p->value.c.cdr, ++i;

	return i;
}

static inline struct alisp_object * car(struct alisp_object * p)
{
	if (p->type == ALISP_OBJ_CONS)
		return p->value.c.car;

	return &alsa_lisp_nil;
}

/*
 * Syntax: (car expr)
 */
static struct alisp_object * F_car(struct alisp_instance *instance, struct alisp_object * args)
{
	return car(eval(instance, car(args)));
}

static inline struct alisp_object * cdr(struct alisp_object * p)
{
	if (p->type == ALISP_OBJ_CONS)
		return p->value.c.cdr;

	return &alsa_lisp_nil;
}

/*
 * Syntax: (cdr expr)
 */
static struct alisp_object * F_cdr(struct alisp_instance *instance, struct alisp_object * args)
{
	return cdr(eval(instance, car(args)));
}

/*
 * Syntax: (+ expr...)
 */
static struct alisp_object * F_add(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;

	p1 = eval(instance, car(p));
	if (p1->type == ALISP_OBJ_INTEGER || p1->type == ALISP_OBJ_FLOAT) {
		long v = 0;
		double f = 0;
		int type = ALISP_OBJ_INTEGER;
		for (;;) {
			if (p1->type == ALISP_OBJ_INTEGER) {
				if (type == ALISP_OBJ_FLOAT)
					f += p1->value.i;
				else
					v += p1->value.i;
			} else if (p1->type == ALISP_OBJ_FLOAT) {
				f += p1->value.f + v;
				v = 0;
				type = ALISP_OBJ_FLOAT;
			} else {
				lisp_warn(instance, "sum with a non integer or float operand");
			}
			p = cdr(p);
			if (p == &alsa_lisp_nil)
				break;
			p1 = eval(instance, car(p));
		}
		if (type == ALISP_OBJ_INTEGER) {
			return new_integer(instance, v);
		} else {
			return new_float(instance, f);
		}
	} else if (p1->type == ALISP_OBJ_STRING) {
		char *str = NULL, *str1;
		for (;;) {
			if (p1->type == ALISP_OBJ_STRING) {
				str1 = realloc(str, (str ? strlen(str) : 0) + strlen(p1->value.s) + 1);
				if (str1 == NULL) {
					nomem();
					if (str)
						free(str);
					return NULL;
				}
				if (str == NULL)
					strcpy(str1, p1->value.s);
				else
					strcat(str1, p1->value.s);
				str = str1;
			} else {
				lisp_warn(instance, "concat with a non string or identifier operand");
			}
			p = cdr(p);
			if (p == &alsa_lisp_nil)
				break;
			p1 = eval(instance, car(p));
		}
		p = new_string(instance, str);
		free(str);
		return p;
	}
	return &alsa_lisp_nil;
}

/*
 * Syntax: (- expr...)
 */
static struct alisp_object * F_sub(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;
	long v = 0;
	double f = 0;
	int type = ALISP_OBJ_INTEGER;

	do {
		p1 = eval(instance, car(p));
		if (p1->type == ALISP_OBJ_INTEGER) {
			if (p == args && cdr(p) != &alsa_lisp_nil) {
				v = p1->value.i;
			} else {
				if (type == ALISP_OBJ_FLOAT)
					f -= p1->value.i;
				else
					v -= p1->value.i;
			}
		} else if (p1->type == ALISP_OBJ_FLOAT) {
			if (type == ALISP_OBJ_INTEGER) {
				f = v;
				type = ALISP_OBJ_FLOAT;
			}
			if (p == args && cdr(p) != &alsa_lisp_nil)
				f = p1->value.f;
			else {
				f -= p1->value.f;
			}
		} else
			lisp_warn(instance, "difference with a non integer or float operand");
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	if (type == ALISP_OBJ_INTEGER) {
		return new_integer(instance, v);
	} else {
		return new_object(instance, f);
	}
}

/*
 * Syntax: (* expr...)
 */
static struct alisp_object * F_mul(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;
	long v = 1;
	double f = 1;
	int type = ALISP_OBJ_INTEGER;

	do {
		p1 = eval(instance, car(p));
		if (p1->type == ALISP_OBJ_INTEGER) {
			if (type == ALISP_OBJ_FLOAT)
				f *= p1->value.i;
			else
				v *= p1->value.i;
		} else if (p1->type == ALISP_OBJ_FLOAT) {
			f *= p1->value.f * v; v = 1;
			type = ALISP_OBJ_FLOAT;
		} else {
			lisp_warn(instance, "product with a non integer or float operand");
		}
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	if (type == ALISP_OBJ_INTEGER) {
		return new_integer(instance, v);
	} else {
		return new_float(instance, f);
	}
}

/*
 * Syntax: (/ expr...)
 */
static struct alisp_object * F_div(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;
	long v = 0;
	double f = 0;
	int type = ALISP_OBJ_INTEGER;

	do {
		p1 = eval(instance, car(p));
		if (p1->type == ALISP_OBJ_INTEGER) {
			if (p == args && cdr(p) != &alsa_lisp_nil) {
				v = p1->value.i;
			} else {
				if (p1->value.i == 0) {
					lisp_warn(instance, "division by zero");
					v = 0;
					f = 0;
					break;
				} else {
					if (type == ALISP_OBJ_FLOAT)
						f /= p1->value.i;
					else
						v /= p1->value.i;
				}
			}
		} else if (p1->type == ALISP_OBJ_FLOAT) {
			if (type == ALISP_OBJ_INTEGER) {
				f = v;
				type = ALISP_OBJ_FLOAT;
			}
			if (p == args && cdr(p) != &alsa_lisp_nil) {
				f = p1->value.f;
			} else {
				if (p1->value.f == 0) {
					lisp_warn(instance, "division by zero");
					f = 0;
					break;
				} else {
					f /= p1->value.i;
				}
			}
		} else
			lisp_warn(instance, "quotient with a non integer or float operand");
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	if (type == ALISP_OBJ_INTEGER) {
		return new_integer(instance, v);
	} else {
		return new_float(instance, f);
	}
}

/*
 * Syntax: (% expr1 expr2)
 */
static struct alisp_object * F_mod(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2, * p3;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (p1->type == ALISP_OBJ_INTEGER && p2->type == ALISP_OBJ_INTEGER) {
		p3 = new_object(instance, ALISP_OBJ_INTEGER);
		if (p3 == NULL)
			return NULL;
		if (p2->value.i == 0) {
			lisp_warn(instance, "module by zero");
			p3->value.i = 0;
		} else
			p3->value.i = p1->value.i % p2->value.i;
	} else if ((p1->type == ALISP_OBJ_INTEGER || p1->type == ALISP_OBJ_FLOAT) &&
		   (p2->type == ALISP_OBJ_INTEGER || p2->type == ALISP_OBJ_FLOAT)) {
		double f1, f2;
		p3 = new_object(instance, ALISP_OBJ_FLOAT);
		if (p3 == NULL)
			return NULL;
		f1 = p1->type == ALISP_OBJ_INTEGER ? p1->value.i : p1->value.f;
		f2 = p2->type == ALISP_OBJ_INTEGER ? p2->value.i : p2->value.f;
		f1 = fmod(f1, f2);
		if (f1 == EDOM) {
			lisp_warn(instance, "module by zero");
			p3->value.f = 0;
		} else
			p3->value.f = f1;
	} else {
		lisp_warn(instance, "module with a non integer or float operand");
		return &alsa_lisp_nil;
	}

	return p3;
}

/*
 * Syntax: (< expr1 expr2)
 */
static struct alisp_object * F_lt(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (p1->type == ALISP_OBJ_INTEGER && p2->type == ALISP_OBJ_INTEGER) {
		if (p1->value.i < p2->value.i)
			return &alsa_lisp_t;
	} else if ((p1->type == ALISP_OBJ_INTEGER || p1->type == ALISP_OBJ_FLOAT) &&
		 (p2->type == ALISP_OBJ_INTEGER || p2->type == ALISP_OBJ_FLOAT)) {
		double f1, f2;
		f1 = p1->type == ALISP_OBJ_INTEGER ? p1->value.i : p1->value.f;
		f2 = p2->type == ALISP_OBJ_INTEGER ? p2->value.i : p2->value.f;
		if (f1 < f2)
			return &alsa_lisp_t;
	} else {
		lisp_warn(instance, "comparison with a non integer or float operand");
	}

	return &alsa_lisp_nil;
}

/*
 * Syntax: (> expr1 expr2)
 */
static struct alisp_object * F_gt(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (p1->type == ALISP_OBJ_INTEGER && p2->type == ALISP_OBJ_INTEGER) {
		if (p1->value.i > p2->value.i)
			return &alsa_lisp_t;
	} else if ((p1->type == ALISP_OBJ_INTEGER || p1->type == ALISP_OBJ_FLOAT) &&
		 (p2->type == ALISP_OBJ_INTEGER || p2->type == ALISP_OBJ_FLOAT)) {
		double f1, f2;
		f1 = p1->type == ALISP_OBJ_INTEGER ? p1->value.i : p1->value.f;
		f2 = p2->type == ALISP_OBJ_INTEGER ? p2->value.i : p2->value.f;
		if (f1 > f2)
			return &alsa_lisp_t;
	} else {
		lisp_warn(instance, "comparison with a non integer or float operand");
	}

	return &alsa_lisp_nil;
}

/*
 * Syntax: (<= expr1 expr2)
 */
static struct alisp_object * F_le(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (p1->type == ALISP_OBJ_INTEGER && p2->type == ALISP_OBJ_INTEGER) {
		if (p1->value.i <= p2->value.i)
			return &alsa_lisp_t;
	} else if ((p1->type == ALISP_OBJ_INTEGER || p1->type == ALISP_OBJ_FLOAT) &&
		 (p2->type == ALISP_OBJ_INTEGER || p2->type == ALISP_OBJ_FLOAT)) {
		double f1, f2;
		f1 = p1->type == ALISP_OBJ_INTEGER ? p1->value.i : p1->value.f;
		f2 = p2->type == ALISP_OBJ_INTEGER ? p2->value.i : p2->value.f;
		if (f1 <= f2)
			return &alsa_lisp_t;
	} else {
		lisp_warn(instance, "comparison with a non integer or float operand");
	}


	return &alsa_lisp_nil;
}

/*
 * Syntax: (>= expr1 expr2)
 */
static struct alisp_object * F_ge(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (p1->type == ALISP_OBJ_INTEGER && p2->type == ALISP_OBJ_INTEGER) {
		if (p1->value.i >= p2->value.i)
			return &alsa_lisp_t;
	} else if ((p1->type == ALISP_OBJ_INTEGER || p1->type == ALISP_OBJ_FLOAT) &&
		 (p2->type == ALISP_OBJ_INTEGER || p2->type == ALISP_OBJ_FLOAT)) {
		double f1, f2;
		f1 = p1->type == ALISP_OBJ_INTEGER ? p1->value.i : p1->value.f;
		f2 = p2->type == ALISP_OBJ_INTEGER ? p2->value.i : p2->value.f;
		if (f1 >= f2)
			return &alsa_lisp_t;
	} else {
		lisp_warn(instance, "comparison with a non integer or float operand");
	}

	return &alsa_lisp_nil;
}

/*
 * Syntax: (= expr1 expr2)
 */
static struct alisp_object * F_numeq(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (p1->type == ALISP_OBJ_INTEGER && p2->type == ALISP_OBJ_INTEGER) {
		if (p1->value.i == p2->value.i)
			return &alsa_lisp_t;
	} else if ((p1->type == ALISP_OBJ_INTEGER || p1->type == ALISP_OBJ_FLOAT) &&
		 (p2->type == ALISP_OBJ_INTEGER || p2->type == ALISP_OBJ_FLOAT)) {
		double f1, f2;
		f1 = p1->type == ALISP_OBJ_INTEGER ? p1->value.i : p1->value.f;
		f2 = p2->type == ALISP_OBJ_INTEGER ? p2->value.i : p2->value.f;
		if (f1 == f2)
			return &alsa_lisp_t;
	} else {
		lisp_warn(instance, "comparison with a non integer or float operand");
	}

	return &alsa_lisp_nil;
}

/*
 * Syntax: (exfun name)
 * Test, if a function exists
 */
static struct alisp_object * F_exfun(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = car(args);
	if (p1->type != ALISP_OBJ_STRING && p1->type != ALISP_OBJ_IDENTIFIER)
		return &alsa_lisp_nil;
	p2 = get_object(instance, p1);
	if (p2 == &alsa_lisp_nil)
		return &alsa_lisp_nil;
	p2 = car(p2);
	if (p2->type == ALISP_OBJ_IDENTIFIER && !strcmp(p2->value.id, "lambda"))
		return &alsa_lisp_t;

	return &alsa_lisp_nil;
}

static void princ_string(snd_output_t *out, char *s)
{
	char *p;

	snd_output_putc(out, '"');
	for (p = s; *p != '\0'; ++p)
		switch (*p) {
		case '\a': snd_output_putc(out, '\\'); snd_output_putc(out, 'a'); break;
		case '\b': snd_output_putc(out, '\\'); snd_output_putc(out, 'b'); break;
		case '\f': snd_output_putc(out, '\\'); snd_output_putc(out, 'f'); break;
		case '\n': snd_output_putc(out, '\\'); snd_output_putc(out, 'n'); break;
		case '\r': snd_output_putc(out, '\\'); snd_output_putc(out, 'r'); break;
		case '\t': snd_output_putc(out, '\\'); snd_output_putc(out, 't'); break;
		case '\v': snd_output_putc(out, '\\'); snd_output_putc(out, 'v'); break;
		default: snd_output_putc(out, *p);
		}
	snd_output_putc(out, '"');
}

static void princ_cons(snd_output_t *out, struct alisp_object * p)
{
	do {
		princ_object(out, p->value.c.car);
		p = p->value.c.cdr;
		if (p != &alsa_lisp_nil) {
			snd_output_putc(out, ' ');
			if (p->type != ALISP_OBJ_CONS) {
				snd_output_printf(out, ". ");
				princ_object(out, p);
			}
		}
	} while (p != &alsa_lisp_nil && p->type == ALISP_OBJ_CONS);
}

static void princ_object(snd_output_t *out, struct alisp_object * p)
{
	switch (p->type) {
	case ALISP_OBJ_NIL:
		snd_output_printf(out, "nil");
		break;
	case ALISP_OBJ_T:
		snd_output_putc(out, 't');
		break;
	case ALISP_OBJ_IDENTIFIER:
		snd_output_printf(out, "%s", p->value.id);
		break;
	case ALISP_OBJ_STRING:
		princ_string(out, p->value.s);
		break;
	case ALISP_OBJ_INTEGER:
		snd_output_printf(out, "%ld", p->value.i);
		break;
	case ALISP_OBJ_FLOAT:
		snd_output_printf(out, "%f", p->value.f);
		break;
	case ALISP_OBJ_POINTER:
		snd_output_printf(out, "<%p>", p->value.ptr);
		break;
	case ALISP_OBJ_CONS:
		snd_output_putc(out, '(');
		princ_cons(out, p);
		snd_output_putc(out, ')');
	}
}

/*
 * Syntax: (princ expr...)
 */
static struct alisp_object * F_princ(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;

	do {
		p1 = eval(instance, car(p));
		if (p1->type == ALISP_OBJ_STRING)
			snd_output_printf(instance->out, "%s", p1->value.s);
		else
			princ_object(instance->out, p1);
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	return p1;
}

/*
 * Syntax: (atom expr)
 */
static struct alisp_object * F_atom(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p;

	p = eval(instance, car(args));

	switch (p->type) {
	case ALISP_OBJ_T:
	case ALISP_OBJ_NIL:
	case ALISP_OBJ_INTEGER:
	case ALISP_OBJ_FLOAT:
	case ALISP_OBJ_STRING:
	case ALISP_OBJ_IDENTIFIER:
	case ALISP_OBJ_POINTER:
		return &alsa_lisp_t;
	}

	return &alsa_lisp_nil;
}

/*
 * Syntax: (cons expr1 expr2)
 */
static struct alisp_object * F_cons(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p;

	p = new_object(instance, ALISP_OBJ_CONS);
	if (p) {
		p->value.c.car = eval(instance, car(args));
		p->value.c.cdr = eval(instance, car(cdr(args)));
	}

	return p;
}

/*
 * Syntax: (list expr1...)
 */
static struct alisp_object * F_list(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * first = NULL, * prev = NULL, * p1;

	if (p == &alsa_lisp_nil)
		return &alsa_lisp_nil;

	do {
		p1 = new_object(instance, ALISP_OBJ_CONS);
		if (p1 == NULL)
			return NULL;
		p1->value.c.car = eval(instance, car(p));
		if (first == NULL)
			first = p1;
		if (prev != NULL)
			prev->value.c.cdr = p1;
		prev = p1;
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	return first;
}

static inline int eq(struct alisp_object * p1, struct alisp_object * p2)
{
	return p1 == p2;
}

static int equal(struct alisp_object * p1, struct alisp_object * p2)
{
	int type1, type2;

	if (eq(p1, p2))
		return 1;

	type1 = p1->type;
	type2 = p2->type;

	if (type1 == ALISP_OBJ_CONS || type2 == ALISP_OBJ_CONS)
		return 0;

	if (type1 == type2) {
		switch (type1) {
		case ALISP_OBJ_STRING:
			return !strcmp(p1->value.s, p2->value.s);
		case ALISP_OBJ_INTEGER:
			return p1->value.i == p2->value.i;
		case ALISP_OBJ_FLOAT:
			return p1->value.i == p2->value.i;
		}
	}

	return 0;
}

/*
 * Syntax: (eq expr1 expr2)
 */
static struct alisp_object * F_eq(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (eq(p1, p2))
		return &alsa_lisp_t;
	return &alsa_lisp_nil;
}

/*
 * Syntax: (equal expr1 expr2)
 */
static struct alisp_object * F_equal(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (equal(p1, p2))
		return &alsa_lisp_t;
	return &alsa_lisp_nil;
}

/*
 * Syntax: (quote expr)
 */
static struct alisp_object * F_quote(struct alisp_instance *instance ATTRIBUTE_UNUSED, struct alisp_object * args)
{
	return car(args);
}

/*
 * Syntax: (and expr...)
 */
static struct alisp_object * F_and(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;

	do {
		p1 = eval(instance, car(p));
		if (p1 == &alsa_lisp_nil)
			return &alsa_lisp_nil;
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	return p1;
}

/*
 * Syntax: (or expr...)
 */
static struct alisp_object * F_or(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;

	do {
		p1 = eval(instance, car(p));
		if (p1 != &alsa_lisp_nil)
			return p1;
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	return &alsa_lisp_nil;
}

/*
 * Syntax: (not expr)
 * Syntax: (null expr)
 */
static struct alisp_object * F_not(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = eval(instance, car(args));

	if (p != &alsa_lisp_nil)
		return &alsa_lisp_nil;

	return &alsa_lisp_t;
}

/*
 * Syntax: (cond (expr1 [expr2])...)
 */
static struct alisp_object * F_cond(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1, * p2, * p3;

	do {
		p1 = car(p);
		if ((p2 = eval(instance, car(p1))) != &alsa_lisp_nil) {
			if ((p3 = cdr(p1)) != &alsa_lisp_nil)
				return F_progn(instance, p3);
			return p2;
		}
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	return &alsa_lisp_nil;
}

/*
 * Syntax: (if expr then-expr else-expr...)
 */
static struct alisp_object * F_if(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2, * p3;

	p1 = car(args);
	p2 = car(cdr(args));
	p3 = cdr(cdr(args));

	if (eval(instance, p1) != &alsa_lisp_nil)
		return eval(instance, p2);

	return F_progn(instance, p3);
}

/*
 * Syntax: (when expr then-expr...)
 */
static struct alisp_object * F_when(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = car(args);
	p2 = cdr(args);
	if (eval(instance, p1) != &alsa_lisp_nil)
		return F_progn(instance, p2);

	return &alsa_lisp_nil;
}

/*
 * Syntax: (unless expr else-expr...)
 */
static struct alisp_object * F_unless(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = car(args);
	p2 = cdr(args);
	if (eval(instance, p1) == &alsa_lisp_nil)
		return F_progn(instance, p2);

	return &alsa_lisp_nil;
}

/*
 * Syntax: (while expr exprs...)
 */
static struct alisp_object * F_while(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;

	p1 = car(args);
	p2 = cdr(args);

	while (eval(instance, p1) != &alsa_lisp_nil)
		F_progn(instance, p2);

	return &alsa_lisp_nil;
}

/*
 * Syntax: (progn expr...)
 */
static struct alisp_object * F_progn(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;

	do {
		p1 = eval(instance, car(p));
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	return p1;
}

/*
 * Syntax: (prog1 expr...)
 */
static struct alisp_object * F_prog1(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * first = NULL, * p1;

	do {
		p1 = eval(instance, car(p));
		if (first == NULL)
			first = p1;
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	if (first == NULL)
		first = &alsa_lisp_nil;

	return first;
}

/*
 * Syntax: (prog2 expr...)
 */
static struct alisp_object * F_prog2(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * second = NULL, * p1;
	int i = 0;

	do {
		++i;
		p1 = eval(instance, car(p));
		if (i == 2)
			second = p1;
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	if (second == NULL)
		second = &alsa_lisp_nil;

	return second;
}

/*
 * Syntax: (set name value)
 */
static struct alisp_object * F_set(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1 = eval(instance, car(args)), * p2 = eval(instance, car(cdr(args)));

	if (p1 == &alsa_lisp_nil) {
		lisp_warn(instance, "setting the value of a nil object");
	} else
		if (set_object(instance, p1, p2) == NULL)
			return NULL;

	return p2;
}

/*
 * Syntax: (unset name)
 */
static struct alisp_object * F_unset(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1 = eval(instance, car(args));

	unset_object(instance, p1);
	return p1;
}

/*
 * Syntax: (setq name value...)
 * Syntax: (setf name value...)
 * `name' is not evalled
 */
static struct alisp_object * F_setq(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1, * p2;

	do {
		p1 = car(p);
		p2 = eval(instance, car(cdr(p)));
		if (set_object(instance, p1, p2) == NULL)
			return NULL;
		p = cdr(cdr(p));
	} while (p != &alsa_lisp_nil);

	return p2;
}

/*
 * Syntax: (unsetq name...)
 * Syntax: (unsetf name...)
 * `name' is not evalled
 */
static struct alisp_object * F_unsetq(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1, * res;

	do {
		p1 = car(p);
		res = unset_object(instance, p1);
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	return res;
}

/*
 * Syntax: (defun name arglist expr...)
 * `name' is not evalled
 * `arglist' is not evalled
 */
static struct alisp_object * F_defun(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1 = car(args), * p2 = car(cdr(args)), * p3 = cdr(cdr(args));
	struct alisp_object * lexpr;

	lexpr = new_object(instance, ALISP_OBJ_CONS);
	if (lexpr) {
		lexpr->value.c.car = new_identifier(instance, "lambda");
		if (lexpr->value.c.car == NULL)
			return NULL;
		if ((lexpr->value.c.cdr = new_object(instance, ALISP_OBJ_CONS)) == NULL)
			return NULL;
		lexpr->value.c.cdr->value.c.car = p2;
		lexpr->value.c.cdr->value.c.cdr = p3;

		if (set_object(instance, p1, lexpr) == NULL)
			return NULL;
	}

	return lexpr;
}

static struct alisp_object * eval_func(struct alisp_instance *instance, struct alisp_object * p, struct alisp_object * args)
{
	struct alisp_object * p1, * p2, * p3, * p4, * p5;
	struct alisp_object ** eval_objs, ** save_objs;
	int i;

	p1 = car(p);
	if (p1->type == ALISP_OBJ_IDENTIFIER && !strcmp(p1->value.id, "lambda")) {
		p2 = car(cdr(p));
		p3 = args;

		if ((i = count_list(p2)) != count_list(p3)) {
			lisp_warn(instance, "wrong number of parameters");
			return &alsa_lisp_nil;
		}

		eval_objs = malloc(2 * i * sizeof(struct alisp_object *));
		if (eval_objs == NULL) {
			nomem();
			goto _err;
		}
		save_objs = eval_objs + i;
		
		/*
		 * Save the new variable values.
		 */
		i = 0;
		while (p3 != &alsa_lisp_nil) {
			p5 = eval(instance, car(p3));
			eval_objs[i++] = p5;
			p3 = cdr(p3);
		}

		/*
		 * Save the old variable values and set the new ones.
		 */
		i = 0;
		while (p2 != &alsa_lisp_nil) {
			p4 = car(p2);
			save_objs[i] = get_object(instance, p4);
			if (set_object(instance, p4, eval_objs[i]) == NULL)
				goto _err;
			p2 = cdr(p2);
			++i;
		}

		p5 = F_progn(instance, cdr(cdr(p)));

		/*
		 * Restore the old variable values.
		 */
		p2 = car(cdr(p));
		i = 0;
		while (p2 != &alsa_lisp_nil) {
			p4 = car(p2);
			if (set_object(instance, p4, save_objs[i++]) == NULL)
				return NULL;
			p2 = cdr(p2);
		}

		if (eval_objs)
			free(eval_objs);

		return p5;
	}

	return &alsa_lisp_nil;

       _err:
	if (eval_objs)
		free(eval_objs);
       	return NULL;
}

struct alisp_object * F_gc(struct alisp_instance *instance, struct alisp_object * args ATTRIBUTE_UNUSED)
{
	garbage_collect(instance);

	return &alsa_lisp_t;
}

/*
 * Syntax: (path what)
 * what is string ('data')
 */
struct alisp_object * F_path(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;

	p1 = eval(instance, car(p));
	if (p1->type != ALISP_OBJ_STRING)
		return &alsa_lisp_nil;
	if (!strcmp(p1->value.s, "data"))
		return new_string(instance, DATADIR);
	return &alsa_lisp_nil;
}

/*
 * Syntax: (include filename...)
 */
struct alisp_object * F_include(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = args, * p1;
	int res = -ENOENT;

	do {
		p1 = eval(instance, car(p));
		if (p1->type == ALISP_OBJ_STRING)
			res = alisp_include_file(instance, p1->value.s);
		p = cdr(p);
	} while (p != &alsa_lisp_nil);

	return new_integer(instance, res);
}

/*
 * Syntax: (int value)
 * 'value' can be integer or float type
 */
struct alisp_object * F_int(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = eval(instance, car(args));

	if (p->type == ALISP_INTEGER)
		return p;
	if (p->type == ALISP_FLOAT)
		return new_integer(instance, floor(p->value.f));

	lisp_warn(instance, "expected an integer or float for integer conversion");
	return &alsa_lisp_nil;
}

/*
 * Syntax: (float value)
 * 'value' can be integer or float type
 */
struct alisp_object * F_float(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = eval(instance, car(args));

	if (p->type == ALISP_FLOAT)
		return p;
	if (p->type == ALISP_INTEGER)
		return new_float(instance, p->value.i);

	lisp_warn(instance, "expected an integer or float for integer conversion");
	return &alsa_lisp_nil;
}

/*
 * Syntax: (str value)
 * 'value' can be integer, float or string type
 */
struct alisp_object * F_str(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = eval(instance, car(args));

	if (p->type == ALISP_STRING)
		return p;
	if (p->type == ALISP_INTEGER || p->type == ALISP_FLOAT) {
		char buf[64];
		if (p->type == ALISP_INTEGER) {
			snprintf(buf, sizeof(buf), "%ld", p->value.i);
		} else {
			snprintf(buf, sizeof(buf), "%.f", p->value.f);
		}
		return new_string(instance, buf);
	}

	lisp_warn(instance, "expected an integer or float for integer conversion");
	return &alsa_lisp_nil;
}

/*
 *  Syntax: (assoc key alist)
 */
struct alisp_object * F_assoc(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, *p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	do {
		if (eq(p1, car(car(p2))))
			return car(p2);
		p2 = cdr(p2);
	} while (p2 != &alsa_lisp_nil);

	return &alsa_lisp_nil;	
}

/*
 *  Syntax: (rassoc value alist)
 */
struct alisp_object * F_rassoc(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, *p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	do {
		if (eq(p1, cdr(car(p2))))
			return car(p2);
		p2 = cdr(p2);
	} while (p2 != &alsa_lisp_nil);

	return &alsa_lisp_nil;	
}

/*
 *  Syntax: (assq key alist)
 */
struct alisp_object * F_assq(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, *p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	do {
		if (equal(p1, car(car(p2))))
			return car(p2);
		p2 = cdr(p2);
	} while (p2 != &alsa_lisp_nil);

	return &alsa_lisp_nil;	
}

/*
 *  Syntax: (nth index alist)
 */
struct alisp_object * F_nth(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, * p2;
	long idx;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	if (p1->type != ALISP_OBJ_INTEGER)
		return &alsa_lisp_nil;
	if (p2->type != ALISP_OBJ_CONS)
		return &alsa_lisp_nil;
	idx = p1->value.i;
	while (idx-- > 0)
		p2 = cdr(p2);
	return car(p2);
}

/*
 *  Syntax: (rassq value alist)
 */
struct alisp_object * F_rassq(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, *p2;

	p1 = eval(instance, car(args));
	p2 = eval(instance, car(cdr(args)));

	do {
		if (equal(p1, cdr(car(p2))))
			return car(p2);
		p2 = cdr(p2);
	} while (p2 != &alsa_lisp_nil);

	return &alsa_lisp_nil;	
}

static struct alisp_object * F_dump_memory(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = car(args);

	if (p != &alsa_lisp_nil && cdr(args) == &alsa_lisp_nil && p->type == ALISP_OBJ_STRING) {
		if (strlen(p->value.s) > 0) {
			dump_objects(instance, p->value.s);
			return &alsa_lisp_t;
		} else
			lisp_warn(instance, "expected filename");
	} else
		lisp_warn(instance, "wrong number of parameters (expected string)");

	return &alsa_lisp_nil;
}

static struct alisp_object * F_stat_memory(struct alisp_instance *instance, struct alisp_object * args ATTRIBUTE_UNUSED)
{
	snd_output_printf(instance->out, "*** Memory stats\n");
	snd_output_printf(instance->out, "  used_objs = %li, free_objs = %li, max_objs = %li, obj_size = %i (total bytes = %li, max bytes = %li)\n",
		instance->used_objs,
		instance->free_objs,
		instance->max_objs,
		sizeof(struct alisp_object),
		(instance->used_objs + instance->free_objs) * sizeof(struct alisp_object),
		instance->max_objs * sizeof(struct alisp_object));
	return &alsa_lisp_nil;
}

static struct alisp_object * F_dump_objects(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p = car(args);

	if (p != &alsa_lisp_nil && cdr(args) == &alsa_lisp_nil && p->type == ALISP_OBJ_STRING) {
		if (strlen(p->value.s) > 0) {
			dump_obj_lists(instance, p->value.s);
			return &alsa_lisp_t;
		} else
			lisp_warn(instance, "expected filename");
	} else
		lisp_warn(instance, "wrong number of parameters (expected string)");

	return &alsa_lisp_nil;
}

struct intrinsic {
	const char *name;
	struct alisp_object * (*func)(struct alisp_instance *instance, struct alisp_object * args);
};

static struct intrinsic intrinsics[] = {
	{ "%", F_mod },
	{ "&dump-memory", F_dump_memory },
	{ "&dump-objects", F_dump_objects },
	{ "&stat-memory", F_stat_memory },
	{ "*", F_mul },
	{ "+", F_add },
	{ "-", F_sub },
	{ "/", F_div },
	{ "<", F_lt },
	{ "<=", F_le },
	{ "=", F_numeq },
	{ ">", F_gt },
	{ ">=", F_ge },
	{ "and", F_and },
	{ "assoc", F_assoc },
	{ "assq", F_assq },
	{ "atom", F_atom },
	{ "car", F_car },
	{ "cdr", F_cdr },
	{ "cond", F_cond },
	{ "cons", F_cons },
	{ "defun", F_defun },
	{ "eq", F_eq },
	{ "equal", F_equal },
	{ "eval", F_eval },
	{ "exfun", F_exfun },
	{ "float", F_float },
	{ "garbage-collect", F_gc },
	{ "gc", F_gc },
	{ "if", F_if },
	{ "include", F_include },
	{ "int", F_int },
	{ "list", F_list },
	{ "not", F_not },
	{ "nth", F_nth },
	{ "null", F_not },
	{ "or", F_or },
	{ "path", F_path },
	{ "princ", F_princ },
	{ "prog1", F_prog1 },
	{ "prog2", F_prog2 },
	{ "progn", F_progn },
	{ "quote", F_quote },
	{ "rassoc", F_rassoc },
	{ "rassq", F_rassq },
	{ "set", F_set },
	{ "setf", F_setq },
	{ "setq", F_setq },
	{ "str", F_str },
	{ "string=", F_equal },
	{ "string-equal", F_equal },
	{ "unless", F_unless },
	{ "unset", F_unset },
	{ "unsetf", F_unsetq },
	{ "unsetq", F_unsetq },
	{ "when", F_when },
	{ "while", F_while },
};

#include "alisp_snd.c"

static int compar(const void *p1, const void *p2)
{
	return strcmp(((struct intrinsic *)p1)->name,
		      ((struct intrinsic *)p2)->name);
}

static struct alisp_object * eval_cons(struct alisp_instance *instance, struct alisp_object * p)
{
	struct alisp_object * p1 = car(p), * p2 = cdr(p), * p3;

	if (p1 != &alsa_lisp_nil && p1->type == ALISP_OBJ_IDENTIFIER) {
		struct intrinsic key, *item;

		if (!strcmp(p1->value.id, "lambda"))
			return p;

		auto_garbage_collect(instance);

		key.name = p1->value.id;
		if ((item = bsearch(&key, intrinsics,
				    sizeof intrinsics / sizeof intrinsics[0],
				    sizeof intrinsics[0], compar)) != NULL)
			return item->func(instance, p2);

		if ((item = bsearch(&key, snd_intrinsics,
				    sizeof snd_intrinsics / sizeof snd_intrinsics[0],
				    sizeof snd_intrinsics[0], compar)) != NULL)
			return item->func(instance, p2);

		if ((p3 = get_object(instance, p1)) != &alsa_lisp_nil)
			return eval_func(instance, p3, p2);
		else
			lisp_warn(instance, "function `%s' is undefined", p1->value.id);
	}

	return &alsa_lisp_nil;
}

static struct alisp_object * eval(struct alisp_instance *instance, struct alisp_object * p)
{
	switch (p->type) {
	case ALISP_OBJ_IDENTIFIER:
		return get_object(instance, p);
	case ALISP_OBJ_INTEGER:
	case ALISP_OBJ_FLOAT:
	case ALISP_OBJ_STRING:
	case ALISP_OBJ_POINTER:
		return p;
	case ALISP_OBJ_CONS:
		return eval_cons(instance, p);
	}

	return p;
}

static struct alisp_object * F_eval(struct alisp_instance *instance, struct alisp_object * args)
{
	return eval(instance, eval(instance, car(args)));
}

/*
 *  main routine
 */

static int alisp_include_file(struct alisp_instance *instance, const char *filename)
{
	snd_input_t *old_in;
	struct alisp_object *p, *p1, *omain;
	struct alisp_object_pair *pmain;
	char *name, *uname;
	int retval = 0, err;

	err = snd_user_file(filename, &name);
	if (err < 0)
		return err;
	old_in = instance->in;
	err = snd_input_stdio_open(&instance->in, name, "r");
	if (err < 0) {
		retval = err;
		goto _err;
	}
	if (instance->verbose)
		lisp_verbose(instance, "** include filename '%s'", name);
	uname = malloc(sizeof(ALISP_MAIN_ID) + strlen(name) + 2);
	if (uname == NULL) {
		retval = -ENOMEM;
		goto _err;
	}
	strcpy(uname, ALISP_MAIN_ID);
	strcat(uname, "-");
	strcat(uname, name);
	omain = new_identifier(instance, uname);
	free(uname);
	if (omain == NULL) {
		retval = -ENOMEM;
		goto _err;
	}
	pmain = set_object(instance, omain, &alsa_lisp_t);
	if (pmain == NULL) {
		retval = -ENOMEM;
		goto _err;
	}

	for (;;) {
		if ((p = parse_object(instance, 0)) == NULL)
			break;
		if (instance->verbose) {
			lisp_verbose(instance, "** code");
			princ_object(instance->vout, p);
			snd_output_putc(instance->vout, '\n');
		}
		pmain->value = p;		/* protect the code tree from garbage-collect */
		p1 = eval(instance, p);
		if (p1 == NULL) {
			retval = -ENOMEM;
			break;
		}
		if (instance->verbose) {
			lisp_verbose(instance, "** result");
			princ_object(instance->vout, p1);
			snd_output_putc(instance->vout, '\n');
		}
		if (instance->debug) {
			lisp_debug(instance, "** objects before collection");
			print_obj_lists(instance, instance->dout);
		}
		pmain->value = &alsa_lisp_t;	/* let garbage-collect working */
		garbage_collect(instance);
		if (instance->debug) {
			lisp_debug(instance, "** objects after collection");
			print_obj_lists(instance, instance->dout);
		}
	}	

	unset_object(instance, omain);

       _err:
	free(name);
	instance->in = old_in;
	return retval;
}
 
int alsa_lisp(struct alisp_cfg *cfg, struct alisp_instance **_instance)
{
	struct alisp_instance *instance;
	struct alisp_object *p, *p1, *omain;
	struct alisp_object_pair *pmain;
	int retval = 0;
	
	instance = (struct alisp_instance *)malloc(sizeof(struct alisp_instance));
	if (instance == NULL) {
		nomem();
		return -ENOMEM;
	}
	memset(instance, 0, sizeof(struct alisp_instance));
	instance->verbose = cfg->verbose && cfg->vout;
	instance->warning = cfg->warning && cfg->wout;
	instance->debug = cfg->debug && cfg->dout;
	instance->in = cfg->in;
	instance->out = cfg->out;
	instance->vout = cfg->vout;
	instance->eout = cfg->eout;
	instance->wout = cfg->wout;
	instance->dout = cfg->dout;
	instance->gc_id = 1;
	
	init_lex(instance);

	omain = new_identifier(instance, ALISP_MAIN_ID);
	if (omain == NULL) {
		alsa_lisp_free(instance);
		return -ENOMEM;
	}
	pmain = set_object(instance, omain, &alsa_lisp_t);
	if (pmain == NULL) {
		alsa_lisp_free(instance);
		return -ENOMEM;
	}

	for (;;) {
		if ((p = parse_object(instance, 0)) == NULL)
			break;
		if (instance->verbose) {
			lisp_verbose(instance, "** code");
			princ_object(instance->vout, p);
			snd_output_putc(instance->vout, '\n');
		}
		pmain->value = p;		/* protect the code tree from garbage-collect */
		p1 = eval(instance, p);
		if (p1 == NULL) {
			retval = -ENOMEM;
			break;
		}
		if (instance->verbose) {
			lisp_verbose(instance, "** result");
			princ_object(instance->vout, p1);
			snd_output_putc(instance->vout, '\n');
		}
		if (instance->debug) {
			lisp_debug(instance, "** objects before collection");
			print_obj_lists(instance, instance->dout);
		}
		pmain->value = &alsa_lisp_t;	/* let garbage-collect working */
		garbage_collect(instance);
		if (instance->debug) {
			lisp_debug(instance, "** objects after collection");
			print_obj_lists(instance, instance->dout);
		}
	}

	unset_object(instance, omain);

	if (_instance)
		*_instance = instance;
	else
		alsa_lisp_free(instance); 
	
	return 0;
}

void alsa_lisp_free(struct alisp_instance *instance)
{
	if (instance == NULL)
		return;
	done_lex(instance);
	free_objects(instance);
	free(instance);
}

struct alisp_cfg *alsa_lisp_default_cfg(snd_input_t *input)
{
	snd_output_t *output, *eoutput;
	struct alisp_cfg *cfg;
	int err;
	
	err = snd_output_stdio_attach(&output, stdout, 0);
	if (err < 0)
		return NULL;
	err = snd_output_stdio_attach(&eoutput, stderr, 0);
	if (err < 0) {
		snd_output_close(output);
		return NULL;
	}
	cfg = calloc(1, sizeof(struct alisp_cfg));
	if (cfg == NULL) {
		snd_output_close(eoutput);
		snd_output_close(output);
		return NULL;
	}
	cfg->out = output;
	cfg->wout = eoutput;
	cfg->eout = eoutput;
	cfg->dout = eoutput;
	cfg->in = input;
	return cfg;
}

void alsa_lisp_default_cfg_free(struct alisp_cfg *cfg)
{
	snd_input_close(cfg->in);
	snd_output_close(cfg->out);
	snd_output_close(cfg->dout);
	free(cfg);
}

int alsa_lisp_function(struct alisp_instance *instance, struct alisp_seq_iterator **result,
		       const char *id, const char *args, ...)
{
	int err = 0;
	struct alisp_object *aargs = NULL, *obj, *res;

	if (args && *args != 'n') {
		va_list ap;
		struct alisp_object *p;
		p = NULL;
		va_start(ap, args);
		while (*args) {
			if (*args++ != '%') {
				err = -EINVAL;
				break;
			}
			if (*args == '\0') {
				err = -EINVAL;
				break;
			}
			obj = NULL;
			err = 0;
			switch (*args++) {
			case 's':
				obj = new_string(instance, va_arg(ap, char *));
				break;
			case 'i':
				obj = new_integer(instance, va_arg(ap, int));
				break;
			case 'l':
				obj = new_integer(instance, va_arg(ap, long));
				break;
			case 'f':
			case 'd':
				obj = new_integer(instance, va_arg(ap, double));
				break;
			case 'p': {
				char _ptrid[24];
				char *ptrid = _ptrid;
				while (*args && *args != '%')
					*ptrid++ = *args++;
				*ptrid = 0;
				if (ptrid == _ptrid) {
					err = -EINVAL;
					break;
				}
				obj = new_cons_pointer(instance, _ptrid, va_arg(ap, void *));
				obj = quote_object(instance, obj);
				break;
			}
			default:
				err = -EINVAL;
				break;
			}
			if (err < 0)
				goto __args_end;
			if (obj == NULL) {
				err = -ENOMEM;
				goto __args_end;
			}
			if (p == NULL) {
				p = aargs = new_object(instance, ALISP_OBJ_CONS);
			} else {
				p->value.c.cdr = new_object(instance, ALISP_OBJ_CONS);
				p = p->value.c.cdr;
			}
			if (p == NULL) {
				err = -ENOMEM;
				goto __args_end;
			}
			p->value.c.car = obj;
		}
	      __args_end:
		va_end(ap);
		if (err < 0)
			return err;
#if 0
		snd_output_printf(instance->wout, ">>>");
		princ_object(instance->wout, aargs);
		snd_output_printf(instance->wout, "<<<\n");
#endif
	}

	err = -ENOENT;
	if (aargs == NULL)
		aargs = &alsa_lisp_nil;
	if ((obj = get_object1(instance, id)) != &alsa_lisp_nil) {
		res = eval_func(instance, obj, aargs);
		err = 0;
	} else {
		struct intrinsic key, *item;
		key.name = id;
		if ((item = bsearch(&key, intrinsics,
				    sizeof intrinsics / sizeof intrinsics[0],
				    sizeof intrinsics[0], compar)) != NULL) {
			res = item->func(instance, aargs);
			err = 0;
		} else if ((item = bsearch(&key, snd_intrinsics,
				         sizeof snd_intrinsics / sizeof snd_intrinsics[0],
					 sizeof snd_intrinsics[0], compar)) != NULL) {
			res = item->func(instance, aargs);
			err = 0;
		} else {
			res = &alsa_lisp_nil;
		}
	}
	if (res == NULL)
		err = -ENOMEM;
	if (err == 0 && result)
		*result = res;

	return 0;
}

int alsa_lisp_seq_first(struct alisp_instance *instance, const char *id,
			struct alisp_seq_iterator **seq)
{
	struct alisp_object * p1;

	p1 = get_object1(instance, id);
	if (p1 == NULL)
		return -ENOMEM;
	*seq = p1;
	return 0;
}

int alsa_lisp_seq_next(struct alisp_seq_iterator **seq)
{
	struct alisp_object * p1 = *seq;

	p1 = cdr(p1);
	if (p1 == &alsa_lisp_nil)
		return -ENOENT;
	*seq = p1;
	return 0;
}

int alsa_lisp_seq_count(struct alisp_seq_iterator *seq)
{
	int count = 0;
	
	while (seq != &alsa_lisp_nil) {
		count++;
		seq = cdr(seq);
	}
	return count;
}

int alsa_lisp_seq_integer(struct alisp_seq_iterator *seq, long *val)
{
	if (seq->type == ALISP_OBJ_CONS)
		seq = seq->value.c.cdr;
	if (seq->type == ALISP_OBJ_INTEGER)
		*val = seq->value.i;
	else
		return -EINVAL;
	return 0;
}

int alsa_lisp_seq_pointer(struct alisp_seq_iterator *seq, const char *ptr_id, void **ptr)
{
	struct alisp_object * p2;
	
	if (seq->type == ALISP_OBJ_CONS && seq->value.c.cdr->type == ALISP_OBJ_CONS)
		seq = seq->value.c.cdr;
	if (seq->type == ALISP_OBJ_CONS) {
		p2 = seq->value.c.car;
		if (p2->type != ALISP_OBJ_STRING)
			return -EINVAL;
		if (strcmp(p2->value.s, ptr_id))
			return -EINVAL;
		p2 = seq->value.c.cdr;
		if (p2->type != ALISP_OBJ_POINTER)
			return -EINVAL;
		*ptr = (void *)seq->value.ptr;
	} else
		return -EINVAL;
	return 0;
}
