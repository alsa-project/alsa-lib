/*
 *  Mixer Interface - local header file
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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

#include <assert.h>
#include "asoundlib.h"

typedef struct mixer_simple mixer_simple_t;

typedef int (mixer_simple_get_t) (snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control);
typedef int (mixer_simple_put_t) (snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control);
typedef int (mixer_simple_event_t) (snd_mixer_t *handle, snd_ctl_event_type_t etype, snd_control_id_t *id);

struct mixer_simple {
	int refs;			/* number of references */
	int change;			/* simple control was changed */
	snd_mixer_sid_t id;
	mixer_simple_get_t *get;
	mixer_simple_put_t *put;
	mixer_simple_event_t *event;
	mixer_simple_t *prev;
	mixer_simple_t *next;
	unsigned long private_value;
};
  
struct snd_mixer {
	snd_ctl_t *ctl_handle;
	int simple_valid;
	int simple_count;
	int simple_changes;		/* total number of changes */
	mixer_simple_t *simple_first;
	mixer_simple_t *simple_last;
	snd_mixer_simple_callbacks_t *callbacks;
};

int snd_mixer_simple_build(snd_mixer_t *handle);
int snd_mixer_simple_destroy(snd_mixer_t *handle);
