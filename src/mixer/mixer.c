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
#include "mixer_local.h"

static void snd_mixer_simple_read_rebuild(snd_ctl_t *ctl_handle, void *private_data);
static void snd_mixer_simple_read_add(snd_ctl_t *ctl_handle, void *private_data, snd_hcontrol_t *hcontrol);

int snd_mixer_open(snd_mixer_t **r_handle, char *name)
{
	snd_mixer_t *handle;
	snd_ctl_t *ctl_handle;
	int err;

	if (r_handle == NULL)
		return -EINVAL;
	*r_handle = NULL;
	if ((err = snd_ctl_open(&ctl_handle, name)) < 0)
		return err;
	handle = (snd_mixer_t *) calloc(1, sizeof(snd_mixer_t));
	if (handle == NULL) {
		snd_ctl_close(ctl_handle);
		return -ENOMEM;
	}
	if ((err = snd_ctl_hcallback_rebuild(ctl_handle, snd_mixer_simple_read_rebuild, handle)) < 0) {
		snd_ctl_close(ctl_handle);
		free(handle);
		return err;
	}
	if ((err = snd_ctl_hcallback_add(ctl_handle, snd_mixer_simple_read_add, handle)) < 0) {
		snd_ctl_close(ctl_handle);
		free(handle);
		return err;
	}
	handle->ctl_handle = ctl_handle;
	INIT_LIST_HEAD(&handle->simples);
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

int snd_mixer_poll_descriptor(snd_mixer_t *handle)
{
	if (handle == NULL || handle->ctl_handle == NULL)
		return -EIO;
	return snd_ctl_poll_descriptor(handle->ctl_handle);
}

const char *snd_mixer_simple_channel_name(snd_mixer_channel_id_t channel)
{
	static char *array[snd_enum_to_int(SND_MIXER_CHN_LAST) + 1] = {
		[SND_MIXER_CHN_FRONT_LEFT] = "Front Left",
		[SND_MIXER_CHN_FRONT_RIGHT] = "Front Right",
		[SND_MIXER_CHN_FRONT_CENTER] = "Front Center",
		[SND_MIXER_CHN_REAR_LEFT] = "Rear Left",
		[SND_MIXER_CHN_REAR_RIGHT] = "Rear Right",
		[SND_MIXER_CHN_WOOFER] = "Woofer"
	};
	char *p;
	assert(channel <= SND_MIXER_CHN_LAST);
	p = array[snd_enum_to_int(channel)];
	if (!p)
		return "?";
	return p;
}

int snd_mixer_simple_control_list(snd_mixer_t *handle, snd_mixer_simple_control_list_t *list)
{
	struct list_head *lh;
	mixer_simple_t *s;
	snd_mixer_sid_t *p;
	int err;
	unsigned int idx;

	if (handle == NULL || list == NULL)
		return -EINVAL;
	if (!handle->simple_valid)
		if ((err = snd_mixer_simple_build(handle)) < 0)
			return err;
	list->controls_count = 0;
	p = list->pids;
	if (list->controls_request > 0 && p == NULL)
		return -EINVAL;
	idx = 0;
	list_for_each(lh, &handle->simples) {
		if (idx >= list->controls_offset + list->controls_request)
			break;
		if (idx >= list->controls_offset) {
			s = list_entry(lh, mixer_simple_t, list);
			memcpy(p, &s->sid, sizeof(*p)); p++;
			list->controls_count++;
		}
		idx++;
	}
	list->controls = handle->simple_count;
	return 0;
}

static mixer_simple_t *look_for_simple(snd_mixer_t *handle, snd_mixer_sid_t *sid)
{
	struct list_head *list;
	mixer_simple_t *s;
	
	list_for_each(list, &handle->simples) {
		s = list_entry(list, mixer_simple_t, list);
		if (!strcmp(s->sid.name, sid->name) && s->sid.index == sid->index)
			return s;
	}
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

static void snd_mixer_simple_read_add(snd_ctl_t *ctl_handle ATTRIBUTE_UNUSED, void *private_data, snd_hcontrol_t *hcontrol)
{
	snd_mixer_t *handle = (snd_mixer_t *)private_data;
	mixer_simple_t *s;
	struct list_head *list;
	
	list_for_each(list, &handle->simples) {
		s = list_entry(list, mixer_simple_t, list);
		if (s->event_add)
			s->event_add(handle, hcontrol);
	}
}

int snd_mixer_simple_read(snd_mixer_t *handle, snd_mixer_simple_callbacks_t *callbacks)
{
	mixer_simple_t *s;
	struct list_head *list;
	int err;

	if (handle == NULL || callbacks == NULL)
		return -EINVAL;
	if (!handle->simple_valid)
		snd_mixer_simple_build(handle);
	handle->callbacks = callbacks;
	handle->simple_changes = 0;
	if ((err = snd_ctl_hevent(handle->ctl_handle)) <= 0) {
		handle->callbacks = NULL;
		return err;
	}
	handle->callbacks = NULL;
	list_for_each(list, &handle->simples) {
		s = list_entry(list, mixer_simple_t, list);
		if (s->change > 0) {
			s->change = 0;
			if (callbacks->value)
				callbacks->value(handle, callbacks->private_data, &s->sid);
		}
	}
	return handle->simple_changes;
}
