/*
 *  Mixer Interface - main file
 *  Copyright (c) 1998/1999/2000 by Jaroslav Kysela <perex@suse.cz>
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "mixer_local.h"

int snd_mixer_open(snd_mixer_t **mixerp, char *name)
{
	snd_mixer_t *mixer;
	snd_ctl_t *ctl;
	int err;
	assert(mixerp);
	if ((err = snd_ctl_open(&ctl, name)) < 0)
		return err;
	mixer = calloc(1, sizeof(snd_mixer_t));
	if (mixer == NULL) {
		snd_ctl_close(ctl);
		return -ENOMEM;
	}
	mixer->ctl = ctl;
	INIT_LIST_HEAD(&mixer->elems);
	*mixerp = mixer;
	return 0;
}

snd_mixer_elem_t *snd_mixer_elem_add(snd_mixer_t *mixer)
{
	snd_mixer_elem_t *elem;
	elem = calloc(1, sizeof(*elem));
	if (!elem)
		return NULL;
	elem->mixer = mixer;
	list_add_tail(&elem->list, &mixer->elems);
	mixer->count++;
	return elem;
}

void snd_mixer_elem_remove(snd_mixer_elem_t *elem)
{
	snd_mixer_t *mixer = elem->mixer;
	if (elem->private_free)
		elem->private_free(elem);
	if (elem->callback)
		elem->callback(elem, SND_CTL_EVENT_REMOVE);
	list_del(&elem->list);
	free(elem);
	mixer->count--;
}

void snd_mixer_free(snd_mixer_t *mixer)
{
	while (!list_empty(&mixer->elems))
		snd_mixer_elem_remove(list_entry(mixer->elems.next, snd_mixer_elem_t, list));
}

int snd_mixer_close(snd_mixer_t *mixer)
{
	assert(mixer);
	snd_mixer_free(mixer);
	return snd_ctl_close(mixer->ctl);
}

int snd_mixer_poll_descriptor(snd_mixer_t *mixer)
{
	if (mixer == NULL || mixer->ctl == NULL)
		return -EIO;
	return snd_ctl_poll_descriptor(mixer->ctl);
}

snd_mixer_elem_t *snd_mixer_first_elem(snd_mixer_t *mixer)
{
	assert(mixer);
	if (list_empty(&mixer->elems))
		return NULL;
	return list_entry(mixer->elems.next, snd_mixer_elem_t, list);
}

snd_mixer_elem_t *snd_mixer_last_elem(snd_mixer_t *mixer)
{
	assert(mixer);
	if (list_empty(&mixer->elems))
		return NULL;
	return list_entry(mixer->elems.prev, snd_mixer_elem_t, list);
}

snd_mixer_elem_t *snd_mixer_elem_next(snd_mixer_elem_t *elem)
{
	assert(elem);
	if (elem->list.next == &elem->mixer->elems)
		return NULL;
	return list_entry(elem->list.next, snd_mixer_elem_t, list);
}

snd_mixer_elem_t *snd_mixer_elem_prev(snd_mixer_elem_t *elem)
{
	assert(elem);
	if (elem->list.prev == &elem->mixer->elems)
		return NULL;
	return list_entry(elem->list.prev, snd_mixer_elem_t, list);
}

int snd_mixer_events(snd_mixer_t *mixer)
{
	return snd_hctl_events(mixer->ctl);
}

