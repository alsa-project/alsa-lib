/*
 *  Mask header
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
  
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include "asoundlib.h"

#define MASK_MAX 31

#ifdef MASK_INLINE
#include "mask_inline.h"
#else
void mask_none(mask_t *mask);
void mask_any(mask_t *mask);
void mask_load(mask_t *mask, unsigned int msk);
int mask_empty(const mask_t *mask);
int mask_full(const mask_t *mask);
void mask_set(mask_t *mask, unsigned int val);
void mask_reset(mask_t *mask, unsigned int val);
void mask_copy(mask_t *mask, const mask_t *v);
int mask_test(const mask_t *mask, unsigned int val);
void mask_intersect(mask_t *mask, const mask_t *v);
void mask_union(mask_t *mask, const mask_t *v);
unsigned int mask_count(const mask_t *mask);
unsigned int mask_min(const mask_t *mask);
unsigned int mask_max(const mask_t *mask);
void mask_set_range(mask_t *mask, unsigned int from, unsigned int to);
void mask_reset_range(mask_t *mask, unsigned int from, unsigned int to);
void mask_leave(mask_t *mask, unsigned int val);
int mask_eq(const mask_t *mask, const mask_t *v);
int mask_single(const mask_t *mask);
int mask_refine(mask_t *mask, const mask_t *v);
int mask_refine_first(mask_t *mask);
int mask_refine_last(mask_t *mask);
int mask_refine_min(mask_t *mask, unsigned int val);
int mask_refine_max(mask_t *mask, unsigned int val);
int mask_refine_set(mask_t *mask, unsigned int val);
int mask_value(const mask_t *mask);
int mask_always_eq(const mask_t *m1, const mask_t *m2);
int mask_never_eq(const mask_t *m1, const mask_t *m2);
#endif
