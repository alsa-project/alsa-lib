/*
 *  Mixer Interface - main file
 *  Copyright (c) 1998/1999/2000 by Jaroslav Kysela <perex@suse.cz>
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
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "asoundlib.h"
#include "mixer_local.h"

int snd_mixer_open(snd_mixer_t **r_handle, int card)
{
	snd_mixer_t *handle;
	snd_ctl_t *ctl_handle;
	int err;

	if (r_handle == NULL)
		return -EINVAL;
	*r_handle = NULL;
	if ((err = snd_ctl_open(&ctl_handle, card)) < 0)
		return err;
	handle = (snd_mixer_t *) calloc(1, sizeof(snd_mixer_t));
	if (handle == NULL) {
		snd_ctl_close(ctl_handle);
		return -ENOMEM;
	}
	handle->ctl_handle = ctl_handle;
	*r_handle = handle;
	return 0;
}

int snd_mixer_close(snd_mixer_t *handle)
{
	int err = 0;

	if (handle == NULL)
		return -EINVAL;
	if (handle->simple_valid)
		snd_mixer_simple_destroy(handle);
	if (handle->ctl_handle)
		err = snd_ctl_close(handle->ctl_handle);
	return err;
}

int snd_mixer_file_descriptor(snd_mixer_t *handle)
{
	if (handle == NULL || handle->ctl_handle == NULL)
		return -EIO;
	return snd_ctl_file_descriptor(handle->ctl_handle);
}

const char *snd_mixer_simple_channel_name(int channel)
{
	static char *array[6] = {
		"Front-Left",
		"Front-Right",
		"Front-Center",
		"Rear-Left",
		"Rear-Right",
		"Woofer"
	};

	if (channel < 0 || channel > 5)
		return "?";
	return array[channel];
}

int snd_mixer_simple_control_list(snd_mixer_t *handle, snd_mixer_simple_control_list_t *list)
{
	mixer_simple_t *s;
	snd_mixer_sid_t *p;
	int err;
	unsigned int tmp;

	if (handle == NULL || list == NULL)
		return -EINVAL;
	if (!handle->simple_valid)
		if ((err = snd_mixer_simple_build(handle)) < 0)
			return err;
	list->controls_count = 0;
	tmp = list->controls_offset;
	for (s = handle->simple_first; s != NULL && tmp > 0; s = s->next);
	tmp = list->controls_request;
	p = list->pids;
	if (tmp > 0 && p == NULL)
		return -EINVAL;
	for (; s != NULL && tmp > 0; s = s->next, tmp--, p++, list->controls_count++)
		memcpy(p, &s->sid, sizeof(*p));
	list->controls = handle->simple_count;
	return 0;
}

static mixer_simple_t *look_for_simple(snd_mixer_t *handle, snd_mixer_sid_t *sid)
{
	mixer_simple_t *s;
	
	for (s = handle->simple_first; s != NULL; s = s->next)
		if (!strcmp(s->sid.name, sid->name) && s->sid.index == sid->index)
			return s;
	return NULL;
}

int snd_mixer_simple_control_read(snd_mixer_t *handle, snd_mixer_simple_control_t *control)
{
	mixer_simple_t *s;

	if (handle == NULL || control == NULL)
		return -EINVAL;
	if (!handle->simple_valid)
		snd_mixer_simple_build(handle);
	s = look_for_simple(handle, &control->sid);
	if (s == NULL)
		return -ENOENT;
	if (s->get == NULL)
		return -EIO;
	return s->get(handle, s, control);
}

int snd_mixer_simple_control_write(snd_mixer_t *handle, snd_mixer_simple_control_t *control)
{
	mixer_simple_t *s;

	if (handle == NULL || control == NULL)
		return -EINVAL;
	if (!handle->simple_valid)
		snd_mixer_simple_build(handle);
	s = look_for_simple(handle, &control->sid);
	if (s == NULL)
		return -ENOENT;
	if (s->put == NULL)
		return -EIO;
	return s->put(handle, s, control);
}

static void snd_mixer_simple_read_rebuild(snd_ctl_t *ctl_handle, void *private_data)
{
	snd_mixer_t *handle = (snd_mixer_t *)private_data;
	if (handle->ctl_handle != ctl_handle)
		return;
	handle->callbacks->rebuild(handle, handle->callbacks->private_data);
	handle->simple_changes++;
}

static void event_for_all_simple_controls(snd_mixer_t *handle, snd_ctl_event_type_t etype, snd_control_id_t *id)
{
	mixer_simple_t *s;
	
	for (s = handle->simple_first; s != NULL; s = s->next) {
		if (s->event)
			s->event(handle, etype, id);
	}
}

static void snd_mixer_simple_read_value(snd_ctl_t *ctl_handle, void *private_data, snd_control_id_t *id)
{
	snd_mixer_t *handle = (snd_mixer_t *)private_data;
	if (handle->ctl_handle != ctl_handle)
		return;
	event_for_all_simple_controls(handle, SND_CTL_EVENT_VALUE, id);
}

static void snd_mixer_simple_read_change(snd_ctl_t *ctl_handle, void *private_data, snd_control_id_t *id)
{
	snd_mixer_t *handle = (snd_mixer_t *)private_data;
	if (handle->ctl_handle != ctl_handle)
		return;
	event_for_all_simple_controls(handle, SND_CTL_EVENT_CHANGE, id);
}

static void snd_mixer_simple_read_add(snd_ctl_t *ctl_handle, void *private_data, snd_control_id_t *id)
{
	snd_mixer_t *handle = (snd_mixer_t *)private_data;
	if (handle->ctl_handle != ctl_handle)
		return;
	event_for_all_simple_controls(handle, SND_CTL_EVENT_ADD, id);
}

static void snd_mixer_simple_read_remove(snd_ctl_t *ctl_handle, void *private_data, snd_control_id_t *id)
{
	snd_mixer_t *handle = (snd_mixer_t *)private_data;
	if (handle->ctl_handle != ctl_handle)
		return;
	event_for_all_simple_controls(handle, SND_CTL_EVENT_REMOVE, id);
}

int snd_mixer_simple_read(snd_mixer_t *handle, snd_mixer_simple_callbacks_t *callbacks)
{
	snd_ctl_callbacks_t xcallbacks;
	int err;

	if (handle == NULL)
		return -EINVAL;
	if (!handle->simple_valid)
		snd_mixer_simple_build(handle);
	handle->callbacks = callbacks;
	handle->simple_changes = 0;
	if ((err = snd_ctl_cevent(handle->ctl_handle)) <= 0) {
		handle->callbacks = NULL;
		return err;
	}
	handle->callbacks = NULL;
	return handle->simple_changes;
}
