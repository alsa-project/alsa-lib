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
  
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include "atomic.h"

void snd_atomic_read_wait(snd_atomic_read_t *t)
{
	volatile const snd_atomic_write_t *w = t->write;
	unsigned int loops = 0;
	struct timespec ts;
	while (w->begin != w->end) {
		if (loops < MAX_SPIN_COUNT) {
			sched_yield();
			loops++;
			continue;
		}
		loops = 0;
		ts.tv_sec = 0;
		ts.tv_nsec = SPIN_SLEEP_DURATION;
		nanosleep(&ts, NULL);
	}
}

