/*
 *  Mask inlines
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
  
#include <sys/types.h>

#define MASK_INLINE static inline

#ifndef MASK_MASK
#define MASK_MAX 31
#endif

struct _snd_mask {
	unsigned int bits;
};

#define snd_mask_bits(mask) ((mask)->bits)

MASK_INLINE unsigned int ld2(u_int32_t v)
{
        unsigned r = 0;

        if (v >= 0x10000) {
                v >>= 16;
                r += 16;
        }
        if (v >= 0x100) {
                v >>= 8;
                r += 8;
        }
        if (v >= 0x10) {
                v >>= 4;
                r += 4;
        }
        if (v >= 4) {
                v >>= 2;
                r += 2;
        }
        if (v >= 2)
                r++;
        return r;
}

MASK_INLINE unsigned int hweight32(u_int32_t v)
{
        v = (v & 0x55555555) + ((v >> 1) & 0x55555555);
        v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
        v = (v & 0x0F0F0F0F) + ((v >> 4) & 0x0F0F0F0F);
        v = (v & 0x00FF00FF) + ((v >> 8) & 0x00FF00FF);
        return (v & 0x0000FFFF) + ((v >> 16) & 0x0000FFFF);
}

MASK_INLINE size_t snd_mask_sizeof(void)
{
	return sizeof(snd_mask_t);
}

MASK_INLINE void snd_mask_none(snd_mask_t *mask)
{
	snd_mask_bits(mask) = 0;
}

MASK_INLINE void snd_mask_any(snd_mask_t *mask)
{
	snd_mask_bits(mask) = ~0U;
}

MASK_INLINE void snd_mask_load(snd_mask_t *mask, unsigned int msk)
{
	snd_mask_bits(mask) = msk;
}

MASK_INLINE int snd_mask_empty(const snd_mask_t *mask)
{
	return snd_mask_bits(mask) == 0;
}

MASK_INLINE int snd_mask_full(const snd_mask_t *mask)
{
	return snd_mask_bits(mask) == ~0U;
}

MASK_INLINE unsigned int snd_mask_count(const snd_mask_t *mask)
{
	return hweight32(snd_mask_bits(mask));
}

MASK_INLINE unsigned int snd_mask_min(const snd_mask_t *mask)
{
	assert(!snd_mask_empty(mask));
	return ffs(snd_mask_bits(mask)) - 1;
}

MASK_INLINE unsigned int snd_mask_max(const snd_mask_t *mask)
{
	assert(!snd_mask_empty(mask));
	return ld2(snd_mask_bits(mask));
}

MASK_INLINE void snd_mask_set(snd_mask_t *mask, unsigned int val)
{
	assert(val <= SND_MASK_MAX);
	snd_mask_bits(mask) |= (1U << val);
}

MASK_INLINE void snd_mask_reset(snd_mask_t *mask, unsigned int val)
{
	assert(val <= SND_MASK_MAX);
	snd_mask_bits(mask) &= ~(1U << val);
}

MASK_INLINE void snd_mask_set_range(snd_mask_t *mask, unsigned int from, unsigned int to)
{
	assert(to <= SND_MASK_MAX && from <= to);
	snd_mask_bits(mask) |= ((1U << (from - to + 1)) - 1) << from;
}

MASK_INLINE void snd_mask_reset_range(snd_mask_t *mask, unsigned int from, unsigned int to)
{
	assert(to <= SND_MASK_MAX && from <= to);
	snd_mask_bits(mask) &= ~(((1U << (from - to + 1)) - 1) << from);
}

MASK_INLINE void snd_mask_leave(snd_mask_t *mask, unsigned int val)
{
	assert(val <= SND_MASK_MAX);
	snd_mask_bits(mask) &= 1U << val;
}

MASK_INLINE void snd_mask_intersect(snd_mask_t *mask, const snd_mask_t *v)
{
	snd_mask_bits(mask) &= snd_mask_bits(v);
}

MASK_INLINE void snd_mask_union(snd_mask_t *mask, const snd_mask_t *v)
{
	snd_mask_bits(mask) |= snd_mask_bits(v);
}

MASK_INLINE int snd_mask_eq(const snd_mask_t *mask, const snd_mask_t *v)
{
	return snd_mask_bits(mask) == snd_mask_bits(v);
}

MASK_INLINE void snd_mask_copy(snd_mask_t *mask, const snd_mask_t *v)
{
	snd_mask_bits(mask) = snd_mask_bits(v);
}

MASK_INLINE int snd_mask_test(const snd_mask_t *mask, unsigned int val)
{
	assert(val <= SND_MASK_MAX);
	return snd_mask_bits(mask) & (1U << val);
}

MASK_INLINE int snd_mask_single(const snd_mask_t *mask)
{
	assert(!snd_mask_empty(mask));
	return !(snd_mask_bits(mask) & (snd_mask_bits(mask) - 1));
}

MASK_INLINE int snd_mask_refine(snd_mask_t *mask, const snd_mask_t *v)
{
	snd_mask_t old;
	assert(!snd_mask_empty(mask));
	snd_mask_copy(&old, mask);
	snd_mask_intersect(mask, v);
	if (snd_mask_empty(mask))
		return -EINVAL;
	return !snd_mask_eq(mask, &old);
}

MASK_INLINE int snd_mask_refine_first(snd_mask_t *mask)
{
	assert(!snd_mask_empty(mask));
	if (snd_mask_single(mask))
		return 0;
	snd_mask_leave(mask, snd_mask_min(mask));
	return 1;
}

MASK_INLINE int snd_mask_refine_last(snd_mask_t *mask)
{
	assert(!snd_mask_empty(mask));
	if (snd_mask_single(mask))
		return 0;
	snd_mask_leave(mask, snd_mask_max(mask));
	return 1;
}

MASK_INLINE int snd_mask_refine_min(snd_mask_t *mask, unsigned int val)
{
	assert(!snd_mask_empty(mask));
	if (snd_mask_min(mask) >= val)
		return 0;
	snd_mask_reset_range(mask, 0, val - 1);
	if (snd_mask_empty(mask))
		return -EINVAL;
	return 1;
}

MASK_INLINE int snd_mask_refine_max(snd_mask_t *mask, unsigned int val)
{
	assert(!snd_mask_empty(mask));
	if (snd_mask_max(mask) <= val)
		return 0;
	snd_mask_reset_range(mask, val + 1, SND_MASK_MAX);
	if (snd_mask_empty(mask))
		return -EINVAL;
	return 1;
}

MASK_INLINE int snd_mask_refine_set(snd_mask_t *mask, unsigned int val)
{
	int changed;
	assert(!snd_mask_empty(mask));
	changed = !snd_mask_single(mask);
	snd_mask_leave(mask, val);
	if (snd_mask_empty(mask))
		return -EINVAL;
	return changed;
}

MASK_INLINE int snd_mask_value(const snd_mask_t *mask)
{
	assert(!snd_mask_empty(mask));
	return snd_mask_min(mask);
}

MASK_INLINE int snd_mask_always_eq(const snd_mask_t *m1, const snd_mask_t *m2)
{
	return snd_mask_single(m1) && snd_mask_single(m2) &&
		snd_mask_value(m1) == snd_mask_value(m2);
}

MASK_INLINE int snd_mask_never_eq(const snd_mask_t *m1, const snd_mask_t *m2)
{
	return (snd_mask_bits(m1) & snd_mask_bits(m2)) == 0;
}
