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
  
#ifdef SND_INTERVAL_C
#define INLINE inline
#else
#define INLINE extern inline
#endif

INLINE void snd_interval_any(snd_interval_t *i)
{
	i->min = 0;
	i->openmin = 0;
	i->max = UINT_MAX;
	i->openmax = 0;
	i->integer = 0;
	i->empty = 0;
}

INLINE void snd_interval_none(snd_interval_t *i)
{
	i->empty = 1;
}

INLINE int snd_interval_checkempty(const snd_interval_t *i)
{
	return (i->min > i->max ||
		(i->min == i->max && (i->openmin || i->openmax)));
}

INLINE int snd_interval_empty(const snd_interval_t *i)
{
	return i->empty;
}

INLINE int snd_interval_single(const snd_interval_t *i)
{
	assert(!snd_interval_empty(i));
	return (i->min == i->max || 
		(i->min + 1 == i->max && i->openmax));
}

INLINE int snd_interval_value(const snd_interval_t *i)
{
	assert(snd_interval_single(i));
	return i->min;
}

INLINE int snd_interval_min(const snd_interval_t *i)
{
	assert(!snd_interval_empty(i));
	return i->min;
}

INLINE int snd_interval_max(const snd_interval_t *i)
{
	assert(!snd_interval_empty(i));
	return i->max;
}

INLINE int snd_interval_test(const snd_interval_t *i, unsigned int val)
{
	return !((i->min > val || (i->min == val && i->openmin) ||
		  i->max < val || (i->max == val && i->openmax)));
}

INLINE void snd_interval_copy(snd_interval_t *d, const snd_interval_t *s)
{
	*d = *s;
}

INLINE int snd_interval_setinteger(snd_interval_t *i)
{
	if (i->integer)
		return 0;
	if (i->openmin && i->openmax && i->min == i->max)
		return -EINVAL;
	i->integer = 1;
	return 1;
}

INLINE void snd_interval_floor(snd_interval_t *i)
{
	if (i->integer || snd_interval_empty(i))
		return;
	i->openmin = 0;
	if (i->openmax) {
		i->max--;
		i->openmax = 0;
	}
	i->integer = 1;
}

INLINE void snd_interval_unfloor(snd_interval_t *i)
{
	if (snd_interval_empty(i))
		return;
	if (i->max == UINT_MAX)
		return;
	if (i->openmax)
		return;
	i->max++;
	i->openmax = 1;
	i->integer = 0;
}


INLINE int snd_interval_always_eq(const snd_interval_t *i1, const snd_interval_t *i2)
{
	return snd_interval_single(i1) && snd_interval_single(i2) &&
		snd_interval_value(i1) == snd_interval_value(i2);
}

INLINE int snd_interval_never_eq(const snd_interval_t *i1, const snd_interval_t *i2)
{
	
	return (i1->max < i2->min || 
		(i1->max == i2->min &&
		 (i1->openmax || i1->openmin)) ||
		i1->min > i2->max ||
		(i1->min == i2->max &&
		 (i1->openmin || i2->openmax)));
}



