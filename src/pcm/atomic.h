/*
 *  Atomic read/write
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
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

#include <asm/system.h>

/* Max number of times we must spin on a spinlock calling sched_yield().
   After MAX_SPIN_COUNT iterations, we put the calling thread to sleep. */

#ifndef MAX_SPIN_COUNT
#define MAX_SPIN_COUNT 50
#endif

/* Duration of sleep (in nanoseconds) when we can't acquire a spinlock
   after MAX_SPIN_COUNT iterations of sched_yield().
   This MUST BE > 2ms.
   (Otherwise the kernel does busy-waiting for realtime threads,
    giving other threads no chance to run.) */

#ifndef SPIN_SLEEP_DURATION
#define SPIN_SLEEP_DURATION 2000001
#endif

typedef struct {
	unsigned int begin, end;
} snd_atomic_write_t;

typedef struct {
	volatile const snd_atomic_write_t *write;
	unsigned int end;
} snd_atomic_read_t;

void snd_atomic_read_wait(snd_atomic_read_t *t);

static inline void snd_atomic_write_init(snd_atomic_write_t *w)
{
	w->begin = 0;
	w->end = 0;
}

static inline void snd_atomic_write_begin(snd_atomic_write_t *w)
{
	w->begin++;
	wmb();
}

static inline void snd_atomic_write_end(snd_atomic_write_t *w)
{
	wmb();
	w->end++;
}

static inline void snd_atomic_read_init(snd_atomic_read_t *r, snd_atomic_write_t *w)
{
	r->write = w;
}

static inline void snd_atomic_read_begin(snd_atomic_read_t *r)
{
	r->end = r->write->end;
	rmb();
}

static inline int snd_atomic_read_ok(snd_atomic_read_t *r)
{
	rmb();
	return r->end == r->write->begin;
}

