/*
 *  Interval functions
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
  
#define INTERVAL_C

#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <linux/asound.h>
#include "interval.h"

static inline void div64_32(u_int64_t *n, u_int32_t div, u_int32_t *rem)
{
	*rem = *n % div;
	*n /= div;
}

static inline unsigned int div32(unsigned int a, unsigned int b, 
				 unsigned int *r)
{
	*r = a % b;
	return a / b;
}

static inline unsigned int div_down(unsigned int a, unsigned int b)
{
	return a / b;
}

static inline unsigned int div_up(unsigned int a, unsigned int b)
{
	unsigned int r;
	unsigned int q = div32(a, b, &r);
	if (r)
		++q;
	return q;
}

static inline unsigned int mul(unsigned int a, unsigned int b)
{
	if (div_down(UINT_MAX, a) < b)
		return UINT_MAX;
	return a * b;
}

static inline unsigned int add(unsigned int a, unsigned int b)
{
	if (a >= UINT_MAX - b)
		return UINT_MAX;
	return a + b;
}

static inline unsigned int sub(unsigned int a, unsigned int b)
{
	if (a > b)
		return a - b;
	return 0;
}

static inline unsigned int muldiv32(unsigned int a, unsigned int b,
				    unsigned int c, unsigned int *r)
{
	u_int64_t n = (u_int64_t) a * b;
	div64_32(&n, c, r);
	if (n >= UINT_MAX) {
		*r = 0;
		return UINT_MAX;
	}
	return n;
}

int interval_refine_min(interval_t *i, unsigned int min)
{
	int changed = 0;
	int openmin = 0;
	assert(!interval_empty(i));
	if (i->min < min) {
		i->min = min;
		i->openmin = openmin;
		changed = 1;
	} else if (i->min == min && !i->openmin && openmin) {
		i->openmin = 1;
		changed = 1;
	}
	if (!i->real) {
		if (i->openmin) {
			i->min++;
			i->openmin = 0;
		}
	}
	if (i->min > i->max ||
	    (i->min == i->max && (i->openmin || i->openmax))) {
		i->empty = 1;
		return -EINVAL;
	}
	return changed;
}

int interval_refine_max(interval_t *i, unsigned int max)
{
	int changed = 0;
	int openmax = 1;
	max = add(max, 1);
	assert(!interval_empty(i));
	if (i->max > max) {
		i->max = max;
		i->openmax = openmax;
		changed = 1;
	} else if (i->max == max && !i->openmax && openmax) {
		i->openmax = 1;
		changed = 1;
	}
	if (!i->real) {
		if (i->openmax) {
			i->max--;
			i->openmax = 0;
		}
	}
	if (i->min > i->max ||
	    (i->min == i->max && (i->openmin || i->openmax))) {
		i->empty = 1;
		return -EINVAL;
	}
	return changed;
}

/* r <- v */
int interval_refine(interval_t *i, const interval_t *v)
{
	int changed = 0;
	assert(!interval_empty(i));
	if (i->min < v->min) {
		i->min = v->min;
		i->openmin = v->openmin;
		changed = 1;
	} else if (i->min == v->min && !i->openmin && v->openmin) {
		i->openmin = 1;
		changed = 1;
	}
	if (i->max > v->max) {
		i->max = v->max;
		i->openmax = v->openmax;
		changed = 1;
	} else if (i->max == v->max && !i->openmax && v->openmax) {
		i->openmax = 1;
		changed = 1;
	}
	if (!i->real) {
		if (i->openmin) {
			i->min++;
			i->openmin = 0;
		}
		if (i->openmax) {
			i->max--;
			i->openmax = 0;
		}
	}
	if (i->min > i->max ||
	    (i->min == i->max && (i->openmin || i->openmax))) {
		i->empty = 1;
		return -EINVAL;
	}
	return changed;
}

int interval_refine_first(interval_t *i)
{
	assert(!interval_empty(i));
	if (i->min == i->max ||
		(i->min + 1 == i->max && i->openmin && i->openmax))
		return 0;
	i->max = i->min;
	i->openmax = i->openmin;
	if (i->openmax)
		i->max++;
	return 1;
}

int interval_refine_last(interval_t *i)
{
	assert(!interval_empty(i));
	if (i->min == i->max ||
		(i->min + 1 == i->max && i->openmin && i->openmax))
		return 0;
	i->min = i->max;
	i->openmin = i->openmax;
	if (i->openmin)
		i->min--;
	return 1;
}

int interval_refine_set(interval_t *i, unsigned int val)
{
	interval_t t;
	t.min = val;
	t.openmin = 0;
	t.max = add(val, 1);
	t.openmax = 1;
	return interval_refine(i, &t);
}

/* a <- b * c */
int interval_mul(interval_t *a, const interval_t *b, const interval_t *c)
{
	interval_t t;
	assert(!a->empty && !b->empty && !c->empty);
	t.min = mul(b->min, c->min);
	t.openmin = (b->openmin || c->openmin);
	t.max = mul(b->max,  c->max);
	t.openmax = (b->openmax || c->openmax);
	return interval_refine(a, &t);
}

/* a <- b / c */
int interval_div(interval_t *a, const interval_t *b, const interval_t *c)
{
	interval_t t;
	unsigned int r;
	assert(!a->empty && !b->empty && !c->empty);
	t.min = div32(b->min, c->max, &r);
	t.openmin = (r || b->openmin || c->openmax);
	t.max = div32(b->max, c->min, &r);
	if (r) {
		t.max++;
		t.openmax = 1;
	} else
		t.openmax = (b->openmax || c->openmin);
	return interval_refine(a, &t);
}


/* a <- b * c / k */
int interval_muldivk(interval_t *a, unsigned int k,
		    const interval_t *b, const interval_t *c)
{
	interval_t t;
	unsigned int r;
	assert(!a->empty && !b->empty && !c->empty);
	t.min = muldiv32(b->min, c->min, k, &r);
	t.openmin = (r || b->openmin || c->openmin);
	t.max = muldiv32(b->max, c->max, k, &r);
	if (r) {
		t.max++;
		t.openmax = 1;
	} else
		t.openmax = (b->openmax || c->openmax);
	return interval_refine(a, &t);
}

/* a <- b * k / c */
int interval_mulkdiv(interval_t *a, unsigned int k,
		     const interval_t *b, const interval_t *c)
{
	interval_t t;
	unsigned int r;
	assert(!a->empty && !b->empty && !c->empty);
	t.min = muldiv32(b->min, k, c->max, &r);
	t.openmin = (r || b->openmin || c->openmax);
	t.max = muldiv32(b->max, k, c->min, &r);
	if (r) {
		t.max++;
		t.openmax = 1;
	} else
		t.openmax = (b->openmax || c->openmin);
	return interval_refine(a, &t);
}

void interval_print(const interval_t *i, FILE *fp)
{
	if (interval_single(i)) {
		fprintf(fp, "%u", interval_value(i));
	} else {
		fprintf(fp, "%c%u %u%c",
			i->openmin ? '(' : '[',
			i->min, i->max,
			i->openmax ? ')' : ']');
	}
}
