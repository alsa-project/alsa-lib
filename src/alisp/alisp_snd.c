/*
 *  ALSA lisp implementation - sound related commands
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
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

struct acall_table {
	const char *name;
	struct alisp_object * (*func) (struct alisp_instance *instance, struct acall_table * item, struct alisp_object * args);
	void * xfunc;
	const char *prefix;
};

/*
 *  helper functions
 */

static inline const void *get_pointer(struct alisp_object * obj)
{
	if (obj->type == ALISP_OBJ_POINTER)
		return obj->value.ptr;
	return NULL;
}

static const char *get_string(struct alisp_object * obj, const char * deflt)
{
	if (obj == &alsa_lisp_t)
		return "true";
	if (obj->type == ALISP_OBJ_STRING)
		return obj->value.s;
	if (obj->type == ALISP_OBJ_IDENTIFIER)
		return obj->value.id;
	return deflt;
}

struct flags {
	const char *key;
	unsigned int mask;
}; 

static unsigned int get_flags(struct alisp_object * obj, const struct flags * flags, unsigned int deflt)
{
	const char *key;
	int invert;
	unsigned int result;
	const struct flags *ptr;

	if (obj == &alsa_lisp_nil)
		return deflt;
	result = deflt;
	do {
		key = get_string(obj, NULL);
		if (key) {
			invert = key[0] == '!';
			key += invert;
			ptr = flags;
			while (ptr->key) {
				if (!strcmp(ptr->key, key)) {
					if (invert)
						result &= ~ptr->mask;
					else
						result |= ptr->mask;
					break;
				}
				ptr++;
			}
		}
		obj = cdr(obj);
	} while (obj != &alsa_lisp_nil);
	return result;
}

static const void *get_ptr(struct alisp_object * obj, const char *_ptr_id)
{
	const char *ptr_id;
	
	ptr_id = get_string(car(obj), NULL);
	if (ptr_id == NULL)
		return NULL;
	if (strcmp(ptr_id, _ptr_id))
		return NULL;
	return get_pointer(cdr(obj));
}

static inline struct alisp_object * new_result(struct alisp_instance * instance, int err)
{
	return new_integer(instance, err);
}

static struct alisp_object * new_result1(struct alisp_instance * instance, int err, const char *ptr_id, void *ptr)
{
	struct alisp_object * lexpr, * p1;

	if (err < 0)
		ptr = NULL;
	lexpr = new_object(instance, ALISP_OBJ_CONS);
	if (lexpr == NULL)
		return NULL;
	lexpr->value.c.car = new_integer(instance, err);
	if (lexpr->value.c.car == NULL)
		return NULL;
	p1 = lexpr->value.c.cdr = new_object(instance, ALISP_OBJ_CONS);
	if (p1 == NULL)
		return NULL;
	p1->value.c.car = new_object(instance, ALISP_OBJ_CONS);
	if ((p1 = p1->value.c.car) == NULL)
		return NULL;
	p1->value.c.car = new_string(instance, ptr_id);
	if (p1->value.c.car == NULL)
		return NULL;
	p1->value.c.cdr = new_pointer(instance, ptr);
	if (p1->value.c.cdr == NULL)
		return NULL;
	return lexpr;
}

/*
 *  macros
 */

/*
 *  HCTL functions
 */

typedef int (*snd_xxx_open_t)(void **rctl, const char *name, int mode);
typedef int (*snd_xxx_open1_t)(void **rctl, void *handle);
typedef int (*snd_xxx_close_t)(void **rctl);

static struct alisp_object * FA_xxx_open(struct alisp_instance * instance, struct acall_table * item, struct alisp_object * args)
{
	const char *name;
	int err, mode;
	void *handle;
	static struct flags flags[] = {
		{ "nonblock", SND_CTL_NONBLOCK },
		{ "async", SND_CTL_ASYNC },
		{ "readonly", SND_CTL_READONLY },
		{ NULL, 0 }
	};

	name = get_string(eval(instance, car(args)), NULL);
	if (name == NULL)
		return &alsa_lisp_nil;
	mode = get_flags(eval(instance, car(cdr(args))), flags, 0);
	
	err = ((snd_xxx_open_t)item->xfunc)(&handle, name, mode);
	return new_result1(instance, err, item->prefix, handle);
}

static struct alisp_object * FA_xxx_open1(struct alisp_instance * instance, struct acall_table * item, struct alisp_object * args)
{
	int err;
	void *handle;
	const char *prefix1 = "ctl";

	args = eval(instance, args);
	handle = (void *)get_ptr(args, prefix1);
	if (handle == NULL)
		return &alsa_lisp_nil;
	err = ((snd_xxx_open1_t)item->xfunc)(&handle, handle);
	return new_result1(instance, err, item->prefix, handle);
}

static struct alisp_object * FA_xxx_close(struct alisp_instance * instance, struct acall_table * item, struct alisp_object * args)
{
	void *handle;

	args = eval(instance, args);
	handle = (void *)get_ptr(args, item->prefix);
	if (handle == NULL)
		return &alsa_lisp_nil;
	return new_result(instance, ((snd_xxx_close_t)item->xfunc)(handle));
}

/*
 *  main code
 */

static struct acall_table acall_table[] = {
	{ "ctl_close", &FA_xxx_close, (void *)&snd_ctl_close, "ctl" },
	{ "ctl_open", &FA_xxx_open, (void *)&snd_ctl_open, "ctl" },
	{ "hctl_close", &FA_xxx_close, (void *)&snd_hctl_close, "hctl" },
	{ "hctl_open", &FA_xxx_open, (void *)&snd_hctl_open, "hctl" },
	{ "hctl_open_ctl", &FA_xxx_open1, (void *)&snd_hctl_open_ctl, "hctl" },
};

static int acall_compar(const void *p1, const void *p2)
{
	return strcmp(((struct acall_table *)p1)->name,
        	      ((struct acall_table *)p2)->name);
}

static struct alisp_object * F_acall(struct alisp_instance *instance, struct alisp_object * args)
{
	struct alisp_object * p1, *p2;
	struct acall_table key, *item;

	p1 = eval(instance, car(args));
	if (p1->type != ALISP_OBJ_IDENTIFIER && p1->type != ALISP_OBJ_STRING)
		return &alsa_lisp_nil;
	p2 = car(cdr(args));
	key.name = p1->value.s;
	if ((item = bsearch(&key, acall_table,
			    sizeof acall_table / sizeof acall_table[0],
			    sizeof acall_table[0], acall_compar)) != NULL)
		return item->func(instance, item, p2);
	lisp_warn(instance, "acall function %s' is undefined", p1->value.s);
	return &alsa_lisp_nil;
}

static struct intrinsic snd_intrinsics[] = {
	{ "acall", F_acall },
};
