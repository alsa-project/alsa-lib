/*
 *  Interval inlines
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
  
#ifdef INTERVAL_C
#define INLINE inline
#else
#define INLINE static inline
#endif

INLINE void interval_all(interval_t *i)
{
	i->min = 1;
	i->max = UINT_MAX;
}

INLINE int interval_empty(const interval_t *i)
{
	return i->empty;
}

INLINE int interval_single(const interval_t *i)
{
	assert(!interval_empty(i));
	return (i->min == i->max || 
		(i->min + 1 == i->max && i->openmax));
}

INLINE int interval_value(const interval_t *i)
{
	assert(interval_single(i));
	return i->min;
}

INLINE int interval_min(const interval_t *i)
{
	assert(!interval_empty(i));
	return i->min;
}

INLINE int interval_max(const interval_t *i)
{
	unsigned int v;
	assert(!interval_empty(i));
	v = i->max;
	if (i->openmax)
		v--;
	return v;
}

INLINE int interval_test(const interval_t *i, unsigned int val)
{
	return !((i->min > val || (i->min == val && i->openmin) ||
		  i->max < val || (i->max == val && i->openmax)));
}

INLINE void interval_copy(interval_t *d, const interval_t *s)
{
	*d = *s;
}

INLINE void interval_setreal(interval_t *i)
{
	i->real = 1;
}

INLINE int interval_eq(const interval_t *i1, const interval_t *i2)
{
	if (i1->empty)
		return i2->empty;
	if (i2->empty)
		return i1->empty;
	return i1->min == i2->min && i1->openmin == i2->openmin &&
		i1->max == i2->max && i1->openmax == i2->openmax;
}

