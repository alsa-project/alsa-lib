/*
 *  Interval header
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
  
#include <stdio.h>

#ifdef INTERVAL_INLINE
#include "interval_inline.h"
#else
void interval_any(interval_t *i);
void interval_none(interval_t *i);
int interval_setinteger(interval_t *i);
int interval_empty(const interval_t *i);
int interval_single(const interval_t *i);
int interval_value(const interval_t *i);
int interval_min(const interval_t *i);
int interval_max(const interval_t *i);
int interval_test(const interval_t *i, unsigned int val);
void interval_copy(interval_t *dst, const interval_t *src);
void interval_round(interval_t *i);
int interval_always_eq(const interval_t *i1, const interval_t *i2);
int interval_never_eq(const interval_t *i1, const interval_t *i2);
#endif

void interval_add(const interval_t *a, const interval_t *b, interval_t *c);
void interval_sub(const interval_t *a, const interval_t *b, interval_t *c);
void interval_mul(const interval_t *a, const interval_t *b, interval_t *c);
void interval_div(const interval_t *a, const interval_t *b, interval_t *c);
void interval_muldiv(const interval_t *a, const interval_t *b, 
		     const interval_t *c, interval_t *d);
void interval_muldivk(const interval_t *a, const interval_t *b, 
		      unsigned int k, interval_t *c);
void interval_mulkdiv(const interval_t *a, unsigned int k,
		      const interval_t *b, interval_t *c);
void interval_print(const interval_t *i, FILE *fp);
int interval_refine_min(interval_t *i, unsigned int min, int openmin);
int interval_refine_max(interval_t *i, unsigned int max, int openmax);
int interval_refine(interval_t *i, const interval_t *v);
int interval_refine_first(interval_t *i);
int interval_refine_last(interval_t *i);
int interval_refine_set(interval_t *i, unsigned int val);
