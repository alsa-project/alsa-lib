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

enum alisp_tokens {
	ALISP_IDENTIFIER,
	ALISP_INTEGER,
	ALISP_STRING
};

enum alisp_objects {
	ALISP_OBJ_NIL,
	ALISP_OBJ_T,
	ALISP_OBJ_INTEGER,
	ALISP_OBJ_IDENTIFIER,
	ALISP_OBJ_STRING,
	ALISP_OBJ_CONS
};

struct alisp_object {
	int	type;
	int	gc;
	union {
		char	*id;
		char	*s;
		int	i;
		struct {
			struct alisp_object *car;
			struct alisp_object *cdr;
		} c;
	} value;
	struct alisp_object *next;
};

struct alisp_object_pair {
	struct alisp_object *name;
 	struct alisp_object *value;
	struct alisp_object_pair *next;
};

#define ALISP_LEX_BUF_MAX 16

struct alisp_instance {
	int verbose: 1,
	    warning: 1,
	    debug: 1;
	/* i/o */
	snd_input_t *in;
	snd_output_t *out;
	snd_output_t *eout;	/* error output */
	snd_output_t *vout;	/* verbose output */
	snd_output_t *wout;	/* warning output */
	snd_output_t *dout;	/* debug output */
	/* lexer */
	int charno;
	int lineno;
	int lex_buf[ALISP_LEX_BUF_MAX];
	int *lex_bufp;
	char *token_buffer;
	int token_buffer_max;
	int thistoken;
	/* object allocator */
	int free_objs;
	int used_objs;
	struct alisp_object *free_objs_list;
	struct alisp_object *used_objs_list;
	/* set object */
	struct alisp_object_pair *setobjs_list;
	/* garbage collect */
	int gc_id;
	/* alsa configuration */
	snd_config_t *root;	/* configuration root */
	snd_config_t *node;	/* result */
};
