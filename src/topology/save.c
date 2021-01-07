/*
  Copyright(c) 2019 Red Hat Inc.
  All rights reserved.

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  Authors: Jaroslav Kysela <perex@perex.cz>
*/

#include "list.h"
#include "tplg_local.h"

#define SAVE_ALLOC_SHIFT	(13)	/* 8192 bytes */
#define PRINT_ALLOC_SHIFT	(10)	/* 1024 bytes */
#define PRINT_BUF_SIZE_MAX	(1024 * 1024)
#define NEXT_CHUNK(val, shift)	((((val) >> (shift)) + 1) << (shift))

void tplg_buf_init(struct tplg_buf *buf)
{
	buf->dst = NULL;
	buf->dst_len = 0;
	buf->printf_buf = NULL;
	buf->printf_buf_size = 0;
}

void tplg_buf_free(struct tplg_buf *buf)
{
	free(buf->dst);
	free(buf->printf_buf);
}

char *tplg_buf_detach(struct tplg_buf *buf)
{
	char *ret = buf->dst;
	free(buf->printf_buf);
	return ret;
}

int tplg_save_printf(struct tplg_buf *dst, const char *pfx, const char *fmt, ...)
{
	va_list va;
	char *s;
	size_t n, l, t, pl;
	int ret = 0;

	if (pfx == NULL)
		pfx = "";

	va_start(va, fmt);
	n = vsnprintf(dst->printf_buf, dst->printf_buf_size, fmt, va);
	va_end(va);

	if (n >= PRINT_BUF_SIZE_MAX) {
		ret = -EOVERFLOW;
		goto end;
	}

	if (n >= dst->printf_buf_size) {
		t = NEXT_CHUNK(n + 1, PRINT_ALLOC_SHIFT);
		s = realloc(dst->printf_buf, t);
		if (!s) {
			ret = -ENOMEM;
			goto end;
		}
		dst->printf_buf = s;
		dst->printf_buf_size = t;
		va_start(va, fmt);
		n = vsnprintf(dst->printf_buf, n + 1, fmt, va);
		va_end(va);
	}

	pl = strlen(pfx);
	l = dst->dst_len;
	t = l + pl + n + 1;
	/* allocate chunks */
	if (dst->dst == NULL ||
	    (l >> SAVE_ALLOC_SHIFT) != (t >> SAVE_ALLOC_SHIFT)) {
		s = realloc(dst->dst, NEXT_CHUNK(t, SAVE_ALLOC_SHIFT));
		if (s == NULL) {
			ret = -ENOMEM;
			goto end;
		}
	} else {
		s = dst->dst;
	}

	if (pl > 0)
		strcpy(s + l, pfx);
	strcpy(s + l + pl, dst->printf_buf);
	dst->dst = s;
	dst->dst_len = t - 1;
end:
	return ret;
}

int tplg_nice_value_format(char *dst, size_t dst_size, unsigned int value)
{
	if ((value % 1000) != 0) {
		if (value > 0xfffffff0)
			return snprintf(dst, dst_size, "%d", (int)value);
		if (value >= 0xffff0000)
			return snprintf(dst, dst_size, "0x%x", value);
	}
	return snprintf(dst, dst_size, "%u", value);
}

static int tplg_pprint_integer(snd_config_t *n, char **ret)
{
	long lval;
	int err, type;
	char buf[16];

	type = snd_config_get_type(n);
	if (type == SND_CONFIG_TYPE_INTEGER) {
		err = snd_config_get_integer(n, &lval);
		if (err < 0)
			return err;
		if (lval < INT_MIN || lval > UINT_MAX)
			return snd_config_get_ascii(n, ret);
	} else if (type == SND_CONFIG_TYPE_INTEGER64) {
		long long llval;
		err = snd_config_get_integer64(n, &llval);
		if (err < 0)
			return err;
		if (llval < INT_MIN || llval > UINT_MAX)
			return snd_config_get_ascii(n, ret);
		lval = llval;
	} else {
		lval = 0;
	}
	err = tplg_nice_value_format(buf, sizeof(buf), (unsigned int)lval);
	if (err < 0)
		return err;
	*ret = strdup(buf);
	if (*ret == NULL)
		return -ENOMEM;
	return 0;
}

static int _compar(const void *a, const void *b)
{
	const snd_config_t *c1 = *(snd_config_t **)a;
	const snd_config_t *c2 = *(snd_config_t **)b;
	const char *id1, *id2;
	if (snd_config_get_id(c1, &id1)) return 0;
	if (snd_config_get_id(c2, &id2)) return 0;
	return strcmp(id1, id2);
}

static snd_config_t *sort_config(const char *id, snd_config_t *src)
{
	snd_config_t *dst, **a;
	snd_config_iterator_t i, next;
	int index, array, count;

	if (snd_config_get_type(src) != SND_CONFIG_TYPE_COMPOUND) {
		if (snd_config_copy(&dst, src) >= 0)
			return dst;
		return NULL;
	}
	count = 0;
	snd_config_for_each(i, next, src)
		count++;
	a = malloc(sizeof(dst) * count);
	if (a == NULL)
		return NULL;
	array = snd_config_is_array(src);
	index = 0;
	snd_config_for_each(i, next, src) {
		snd_config_t *s = snd_config_iterator_entry(i);
		a[index++] = s;
	}
	if (array <= 0)
		qsort(a, count, sizeof(a[0]), _compar);
	if (snd_config_make_compound(&dst, id, count == 1))
		goto lerr;
	for (index = 0; index < count; index++) {
		snd_config_t *s = a[index];
		const char *id2;
		if (snd_config_get_id(s, &id2)) {
			snd_config_delete(dst);
			goto lerr;
		}
		s = sort_config(id2, s);
		if (s == NULL || snd_config_add(dst, s)) {
			if (s)
				snd_config_delete(s);
			snd_config_delete(dst);
			goto lerr;
		}
	}
	free(a);
	return dst;
lerr:
	free(a);
	return NULL;
}

static int tplg_check_quoted(const unsigned char *p)
{
	for ( ; *p != '\0'; p++) {
		switch (*p) {
		case ' ':
		case '=':
		case ';':
		case ',':
		case '.':
		case '{':
		case '}':
		case '\'':
		case '"':
			return 1;
		default:
			if (*p <= 31 || *p >= 127)
				return 1;

		}
	}
	return 0;
}

static int tplg_save_quoted(struct tplg_buf *dst, const char *str)
{
	static const char nibble[16] = "0123456789abcdef";
	unsigned char *p, *d, *t;
	int c;

	d = t = alloca(strlen(str) * 5 + 1 + 1);
	for (p = (unsigned char *)str; *p != '\0'; p++) {
		c = *p;
		switch (c) {
		case '\n':
			*t++ = '\\';
			*t++ = 'n';
			break;
		case '\t':
			*t++ = '\\';
			*t++ = 't';
			break;
		case '\v':
			*t++ = '\\';
			*t++ = 'v';
			break;
		case '\b':
			*t++ = '\\';
			*t++ = 'b';
			break;
		case '\r':
			*t++ = '\\';
			*t++ = 'r';
			break;
		case '\f':
			*t++ = '\\';
			*t++ = 'f';
			break;
		case '\'':
			*t++ = '\\';
			*t++ = c;
			break;
		default:
			if (c >= 32 && c <= 126) {
				*t++ = c;
			} else {
				*t++ = '\\';
				*t++ = 'x';
				*t++ = nibble[(c >> 4) & 0x0f];
				*t++ = nibble[(c >> 0) & 0x0f];
			}
			break;
		}
	}
	*t = '\0';
	return tplg_save_printf(dst, NULL, "'%s'", d);
}

static int tplg_save_string(struct tplg_buf *dst, const char *str, int id)
{
	const unsigned char *p = (const unsigned char *)str;

	if (!p || !*p)
		return tplg_save_printf(dst, NULL, "''");

	if (!id && ((*p >= '0' && *p <= '9') || *p == '-'))
		return tplg_save_quoted(dst, str);

	if (tplg_check_quoted(p))
		return tplg_save_quoted(dst, str);

	return tplg_save_printf(dst, NULL, "%s", str);
}

static int save_config(struct tplg_buf *dst, int level, const char *delim, snd_config_t *src)
{
	snd_config_iterator_t i, next;
	snd_config_t *s;
	const char *id;
	char *pfx;
	unsigned int count;
	int type, err, quoted, array;

	if (delim == NULL)
		delim = "";

	type = snd_config_get_type(src);
	if (type != SND_CONFIG_TYPE_COMPOUND) {
		char *val;
		if (type == SND_CONFIG_TYPE_INTEGER ||
		    type == SND_CONFIG_TYPE_INTEGER64) {
			err = tplg_pprint_integer(src, &val);
		} else {
			err = snd_config_get_ascii(src, &val);
		}
		if (err < 0)
			return err;
		if (type == SND_CONFIG_TYPE_STRING) {
			/* hexa array pretty print */
			id = strchr(val, '\n');
			if (id) {
				err = tplg_save_printf(dst, NULL, "\n");
				if (err < 0)
					goto retval;
				for (id++; *id == '\t'; id++) {
					err = tplg_save_printf(dst, NULL, "\t");
					if (err < 0)
						goto retval;
				}
				delim = "";
			}
			err = tplg_save_printf(dst, NULL, "%s'%s'\n", delim, val);
		} else {
			err = tplg_save_printf(dst, NULL, "%s%s\n", delim, val);
		}
retval:
		free(val);
		return err;
	}

	count = 0;
	quoted = 0;
	array = snd_config_is_array(src);
	s = NULL;
	snd_config_for_each(i, next, src) {
		s = snd_config_iterator_entry(i);
		err = snd_config_get_id(s, &id);
		if (err < 0)
			return err;
		if (!quoted && tplg_check_quoted((unsigned char *)id))
			quoted = 1;
		count++;
	}
	if (count == 0)
		return 0;

	if (count == 1) {
		err = snd_config_get_id(s, &id);
		if (err >= 0 && level > 0)
			err = tplg_save_printf(dst, NULL, ".");
		if (err >= 0)
			err = tplg_save_string(dst, id, 1);
		if (err >= 0)
			err = save_config(dst, level, " ", s);
		return err;
	}

	pfx = alloca(level + 1);
	memset(pfx, '\t', level);
	pfx[level] = '\0';

	if (level > 0) {
		err = tplg_save_printf(dst, NULL, "%s%s\n", delim,
				       array > 0 ? "[" : "{");
		if (err < 0)
			return err;
	}

	snd_config_for_each(i, next, src) {
		s = snd_config_iterator_entry(i);
		const char *id;
		err = snd_config_get_id(s, &id);
		if (err < 0)
			return err;
		err = tplg_save_printf(dst, pfx, "");
		if (err < 0)
			return err;
		if (array <= 0) {
			delim = " ";
			if (quoted) {
				err = tplg_save_quoted(dst, id);
			} else {
				err = tplg_save_string(dst, id, 1);
			}
			if (err < 0)
				return err;
		} else {
			delim = "";
		}
		err = save_config(dst, level + 1, delim, s);
		if (err < 0)
			return err;
	}

	if (level > 0) {
		pfx[level - 1] = '\0';
		err = tplg_save_printf(dst, pfx, "%s\n",
				       array > 0 ? "]" : "}");
		if (err < 0)
			return err;
	}

	return 0;
}

static int tplg_save(snd_tplg_t *tplg, struct tplg_buf *dst,
		     int gindex, const char *prefix)
{
	struct tplg_table *tptr;
	struct tplg_elem *elem;
	struct list_head *list, *pos;
	char pfx2[16];
	unsigned int index;
	int err, count;

	snprintf(pfx2, sizeof(pfx2), "%s\t", prefix ?: "");

	/* write all blocks */
	for (index = 0; index < tplg_table_items; index++) {
		tptr = &tplg_table[index];
		list = (struct list_head *)((void *)tplg + tptr->loff);

		/* count elements */
		count = 0;
		list_for_each(pos, list) {
			elem = list_entry(pos, struct tplg_elem, list);
			if (gindex >= 0 && elem->index != gindex)
				continue;
			if (tptr->save == NULL && tptr->gsave == NULL) {
				SNDERR("unable to create %s block (no callback)",
				       tptr->id);
				err = -ENXIO;
				goto _err;
			}
			if (tptr->save)
				count++;
		}

		if (count == 0)
			continue;

		if (count > 1) {
			err = tplg_save_printf(dst, prefix, "%s {\n",
					       elem->table ?
						elem->table->id : "_NOID_");
		} else {
			err = tplg_save_printf(dst, prefix, "%s.",
					       elem->table ?
						elem->table->id : "_NOID_");
		}

		if (err < 0)
			goto _err;

		list_for_each(pos, list) {
			elem = list_entry(pos, struct tplg_elem, list);
			if (gindex >= 0 && elem->index != gindex)
				continue;
			if (count > 1) {
				err = tplg_save_printf(dst, pfx2, "");
				if (err < 0)
					goto _err;
			}
			err = tptr->save(tplg, elem, dst, count > 1 ? pfx2 : prefix);
			if (err < 0) {
				SNDERR("failed to save %s elements: %s",
				       tptr->id, snd_strerror(-err));
				goto _err;
			}
		}
		if (count > 1) {
			err = tplg_save_printf(dst, prefix, "}\n");
			if (err < 0)
				goto _err;
		}
	}

	/* save globals */
	for (index = 0; index < tplg_table_items; index++) {
		tptr = &tplg_table[index];
		if (tptr->gsave) {
			err = tptr->gsave(tplg, gindex, dst, prefix);
			if (err < 0)
				goto _err;
		}
	}

	return 0;

_err:
	return err;
}

static int tplg_index_compar(const void *a, const void *b)
{
	const int *a1 = a, *b1 = b;
	return *a1 - *b1;
}

static int tplg_index_groups(snd_tplg_t *tplg, int **indexes)
{
	struct tplg_table *tptr;
	struct tplg_elem *elem;
	struct list_head *list, *pos;
	unsigned int index, j, count, size;
	int *a, *b;

	count = 0;
	size = 16;
	a = malloc(size * sizeof(a[0]));

	for (index = 0; index < tplg_table_items; index++) {
		tptr = &tplg_table[index];
		list = (struct list_head *)((void *)tplg + tptr->loff);
		list_for_each(pos, list) {
			elem = list_entry(pos, struct tplg_elem, list);
			for (j = 0; j < count; j++) {
				if (a[j] == elem->index)
					break;
			}
			if (j < count)
				continue;
			if (count + 1 >= size) {
				size += 8;
				b = realloc(a, size * sizeof(a[0]));
				if (b == NULL) {
					free(a);
					return -ENOMEM;
				}
				a = b;
			}
			a[count++] = elem->index;
		}
	}
	a[count] = -1;

	qsort(a, count, sizeof(a[0]), tplg_index_compar);

	*indexes = a;
	return 0;
}

int snd_tplg_save(snd_tplg_t *tplg, char **dst, int flags)
{
	struct tplg_buf buf, buf2;
	snd_input_t *in;
	snd_config_t *top, *top2;
	int *indexes, *a;
	int err;

	assert(tplg);
	assert(dst);
	*dst = NULL;

	tplg_buf_init(&buf);

	if (flags & SND_TPLG_SAVE_GROUPS) {
		err = tplg_index_groups(tplg, &indexes);
		if (err < 0)
			return err;
		for (a = indexes; err >= 0 && *a >= 0; a++) {
			err = tplg_save_printf(&buf, NULL,
					       "IndexGroup.%d {\n",
					       *a);
			if (err >= 0)
				err = tplg_save(tplg, &buf, *a, "\t");
			if (err >= 0)
				err = tplg_save_printf(&buf, NULL, "}\n");
		}
		free(indexes);
	} else {
		err = tplg_save(tplg, &buf, -1, NULL);
	}

	if (err < 0)
		goto _err;

	if (buf.dst == NULL) {
		err = -EINVAL;
		goto _err;
	}

	if (flags & SND_TPLG_SAVE_NOCHECK) {
		*dst = tplg_buf_detach(&buf);
		return 0;
	}

	/* always load configuration - check */
	err = snd_input_buffer_open(&in, buf.dst, strlen(buf.dst));
	if (err < 0) {
		SNDERR("could not create input buffer");
		goto _err;
	}

	err = snd_config_top(&top);
	if (err < 0) {
		snd_input_close(in);
		goto _err;
	}

	err = snd_config_load(top, in);
	snd_input_close(in);
	if (err < 0) {
		SNDERR("could not load configuration");
		snd_config_delete(top);
		goto _err;
	}

	if (flags & SND_TPLG_SAVE_SORT) {
		top2 = sort_config(NULL, top);
		if (top2 == NULL) {
			SNDERR("could not sort configuration");
			snd_config_delete(top);
			err = -EINVAL;
			goto _err;
		}
		snd_config_delete(top);
		top = top2;
	}

	tplg_buf_init(&buf2);
	err = save_config(&buf2, 0, NULL, top);
	snd_config_delete(top);
	if (err < 0) {
		SNDERR("could not save configuration");
		goto _err;
	}

	tplg_buf_free(&buf);
	*dst = tplg_buf_detach(&buf2);
	return 0;

_err:
	tplg_buf_free(&buf);
	*dst = NULL;
	return err;
}
