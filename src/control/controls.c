/*
 *  Control Interface - highlevel API
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>
#define __USE_GNU
#include <search.h>
#include "asoundlib.h"
#include "control_local.h"

static void snd_ctl_hfree1(snd_hcontrol_t *hcontrol);

int snd_ctl_hbuild(snd_ctl_t *handle, snd_ctl_hsort_t *hsort)
{
	snd_control_list_t list;
	snd_hcontrol_t *hcontrol, *prev;
	int err;
	unsigned int idx;

	assert(handle != NULL);
	if ((err = snd_ctl_hfree(handle)) < 0)
		return err;
	if (hsort == NULL)
		hsort = snd_ctl_hsort;
	memset(&list, 0, sizeof(list));
	do {
		if (list.pids != NULL)
			free(list.pids);
		list.controls_offset = 0;
		list.controls_request = 0;
		list.controls_count = 0;
		if ((err = snd_ctl_clist(handle, &list)) < 0)
			return err;
		if (list.controls == 0)
			break;
		list.pids = (snd_control_id_t *)calloc(list.controls, sizeof(snd_control_id_t));
		if (list.pids == NULL)
			return -ENOMEM;
		list.controls_request = list.controls;
		if ((err = snd_ctl_clist(handle, &list)) < 0)
			return err;
	} while (list.controls != list.controls_count);
	for (idx = 0, prev = NULL; idx < list.controls_count; idx++) {
		hcontrol = (snd_hcontrol_t *)calloc(1, sizeof(snd_hcontrol_t));
		if (hcontrol == NULL)
			goto __nomem;
		hcontrol->id = list.pids[idx];
		hcontrol->handle = handle;
		if (tsearch(hcontrol, &handle->hroot, (__compar_fn_t)hsort) == NULL) {
		      __nomem:
			if (handle->hroot != NULL) {
				tdestroy(handle->hroot, (__free_fn_t)snd_ctl_hfree1);
				handle->hroot = NULL;
			}
			handle->hroot = NULL;
			if (hcontrol != NULL)
				free(hcontrol);
			free(list.pids);
			return -ENOMEM;
		}
		list_add_tail(&hcontrol->list, &handle->hlist);
		handle->hcount++;
	}
	if (list.pids != NULL)
		free(list.pids);
	if ((err = snd_ctl_hresort(handle, hsort)) < 0) {
		tdestroy(handle->hroot, (__free_fn_t)snd_ctl_hfree1);
		handle->hroot = NULL;
	}
	return 0;
}

static void snd_ctl_hfree1(snd_hcontrol_t *hcontrol)
{
	snd_ctl_t *handle;
	
	assert(hcontrol != NULL);
	handle = hcontrol->handle;
	assert(handle != NULL);
	assert(handle->hcount > 0);
	if (hcontrol->event_remove)
		hcontrol->event_remove(handle, hcontrol);
	if (hcontrol->private_free)
		hcontrol->private_free(hcontrol->private_data);
	list_del(&hcontrol->list);
	free(hcontrol);
	handle->hcount--;
}

int snd_ctl_hfree(snd_ctl_t *handle)
{
	handle->hsort = NULL;
	handle->herr = 0;
	if (handle->hroot != NULL) {
		tdestroy(handle->hroot, (__free_fn_t)snd_ctl_hfree1);
		handle->hroot = NULL;
	}
	assert(list_empty(&handle->hlist));
	assert(handle->hcount == 0);
	return 0;
}

#define NOT_FOUND 1000000000

static int snd_ctl_hsort_mixer_priority_lookup(char **name, char * const *names, int coef)
{
	int res;

	for (res = 0; *names; names++, res += coef) {
		if (!strncmp(*name, *names, strlen(*names))) {
			*name += strlen(*names);
			if (**name == ' ')
				(*name)++;
			return res;
		}
	}
	return NOT_FOUND;
}

static int snd_ctl_hsort_mixer_priority(const char *name)
{
	static char *names[] = {
		"Master",
		"Master Digital",
		"Master Mono",
		"Hardware Master",
		"Headphone",
		"Tone Control",
		"3D Control",
		"PCM",
		"PCM Front",
		"PCM Rear",
		"PCM Pan",
		"Synth",
		"FM",
		"Wave",
		"Music",
		"DSP",
		"Line",
		"CD",
		"Mic",
		"Phone",
		"Video",
		"PC Speaker",
		"Aux",
		"Mono",
		"Mono Output",
		"ADC",
		"Capture Source",
		"Capture",
		"Playback",
		"Loopback",
		"Analog Loopback",
		"Digital Loopback",
		"S/PDIF Input",
		"S/PDIF Output",
		NULL
	};
	static char *names1[] = {
		"Switch",
		"Volume",
		"Playback",
		"Capture",
		"Bypass",
		NULL
	};
	static char *names2[] = {
		"Switch",
		"Volume",
		"Bypass",
		NULL
	};
	int res, res1;
	
	if ((res = snd_ctl_hsort_mixer_priority_lookup((char **)&name, names, 1000000)) == NOT_FOUND)
		return NOT_FOUND;
	if ((res1 = snd_ctl_hsort_mixer_priority_lookup((char **)&name, names1, 1000)) == NOT_FOUND)
		return res;
	res += res1;
	if ((res1 = snd_ctl_hsort_mixer_priority_lookup((char **)&name, names2, 1)) == NOT_FOUND)
		return res;
	return res + res1;
}

int snd_ctl_hsort(const snd_hcontrol_t *c1, const snd_hcontrol_t *c2)
{
	int res, p1, p2;

	if (c1->id.iface < c2->id.iface)
		return -1;
	if (c1->id.iface > c2->id.iface)
		return 1;
	if ((res = strcmp(c1->id.name, c2->id.name)) != 0) {
		if (c1->id.iface != SND_CONTROL_IFACE_MIXER)
			return res;
		p1 = snd_ctl_hsort_mixer_priority(c1->id.name);
		p2 = snd_ctl_hsort_mixer_priority(c2->id.name);
		if (p1 < p2)
			return -1;
		if (p1 > p2)
			return 1;
		return res;
	}
	if (c1->id.index < c2->id.index)
		return -1;
	if (c1->id.index > c2->id.index)
		return 1;
	return 0;
}

static void snd_ctl_hresort_free(snd_hcontrol_t *hcontrol ATTRIBUTE_UNUSED)
{
	/* nothing */
}

int snd_ctl_hresort(snd_ctl_t *handle, snd_ctl_hsort_t *hsort)
{
	struct list_head *list;
	snd_hcontrol_t *hcontrol;
	snd_control_id_t *ids, *pids;
	int idx;

	assert(handle != NULL && hsort != NULL);
	if (handle->hcount == 0)
		return 0;
	if (handle->herr < 0)
		return handle->herr;
	assert(handle->hroot_new == NULL);
	ids = pids = (snd_control_id_t *)malloc(sizeof(snd_control_id_t) * handle->hcount);
	if (ids == NULL)
		return -ENOMEM;
	/* first step - update search engine */
	list_for_each(list, &handle->hlist) {
		hcontrol = list_entry(list, snd_hcontrol_t, list);
		*pids++ = hcontrol->id;
		if (tsearch(hcontrol, &handle->hroot_new, (__compar_fn_t)hsort) == NULL) {
			if (handle->hroot_new != NULL)
				tdestroy(handle->hroot_new, (__free_fn_t)snd_ctl_hresort_free);
			handle->hroot_new = NULL;
			free(ids);
			return -ENOMEM;
		}
	}
	if (handle->hroot != NULL)
		tdestroy(handle->hroot, (__free_fn_t)snd_ctl_hresort_free);
	handle->hsort = hsort;
	handle->hroot = handle->hroot_new;
	handle->hroot_new = NULL;
	/* second step - perform qsort and save results */
	qsort(ids, handle->hcount, sizeof(snd_control_id_t), (int (*)(const void *, const void *))hsort);
	INIT_LIST_HEAD(&handle->hlist);
	for (idx = 0; idx < handle->hcount; idx++) {
		hcontrol = snd_ctl_hfind(handle, ids + idx);
		list_add_tail(&hcontrol->list, &handle->hlist);
	}
	free(ids);
	return 0;
}

snd_hcontrol_t *snd_ctl_hfirst(snd_ctl_t *handle)
{
	assert(handle != NULL);
	if (list_empty(&handle->hlist))
		return NULL;
	return (snd_hcontrol_t *)list_entry(handle->hlist.next, snd_hcontrol_t, list);
}

snd_hcontrol_t *snd_ctl_hlast(snd_ctl_t *handle)
{
	assert(handle != NULL);
	if (list_empty(&handle->hlist))
		return NULL;
	return (snd_hcontrol_t *)list_entry(handle->hlist.prev, snd_hcontrol_t, list);
}

snd_hcontrol_t *snd_ctl_hnext(snd_ctl_t *handle, snd_hcontrol_t *hcontrol)
{
	assert(handle != NULL && hcontrol != NULL);
	if (hcontrol->list.next == &handle->hlist)
		return NULL;
	return (snd_hcontrol_t *)list_entry(hcontrol->list.next, snd_hcontrol_t, list);
}

snd_hcontrol_t *snd_ctl_hprev(snd_ctl_t *handle, snd_hcontrol_t *hcontrol)
{
	assert(handle != NULL && hcontrol != NULL);
	if (hcontrol->list.prev == &handle->hlist)
		return NULL;
	return (snd_hcontrol_t *)list_entry(hcontrol->list.prev, snd_hcontrol_t, list);
}

int snd_ctl_hcount(snd_ctl_t *handle)
{
	assert(handle != NULL);
	return handle->hcount;
}

snd_hcontrol_t *snd_ctl_hfind(snd_ctl_t *handle, snd_control_id_t *id)
{
	void *res;

	assert(handle != NULL);
	if (handle->hroot == NULL)
		return NULL;
	res = tfind(id, &handle->hroot, (__compar_fn_t)handle->hsort);
	return res == NULL ? NULL : *(snd_hcontrol_t **)res;
}

int snd_ctl_hlist(snd_ctl_t *handle, snd_hcontrol_list_t *hlist)
{
	struct list_head *list;
	snd_hcontrol_t *hcontrol;
	unsigned int idx;

	assert(hlist != NULL);
	if (hlist->controls_offset >= (unsigned int)handle->hcount)
		return -EINVAL;
	hlist->controls_count = 0;
	hlist->controls = handle->hcount;
	if (hlist->controls_request > 0) {
		if (hlist->pids == NULL)
			return -EINVAL;
		idx = 0;
		list_for_each(list, &handle->hlist) {
			hcontrol = list_entry(list, snd_hcontrol_t, list);
			if (idx >= hlist->controls_offset + hlist->controls_request)
				break;
			if (idx >= hlist->controls_offset) {
				hlist->pids[idx] = hcontrol->id;
				hlist->controls_count++;
			}
			idx++;
		}
	}
	return 0;
}

int snd_ctl_hcallback_rebuild(snd_ctl_t *handle, snd_ctl_hcallback_rebuild_t *callback, void *private_data)
{
	assert(handle != NULL);
	handle->callback_rebuild = callback;
	handle->callback_rebuild_private_data = private_data;
	return 0;
}

int snd_ctl_hcallback_add(snd_ctl_t *handle, snd_ctl_hcallback_add_t *callback, void *private_data)
{
	assert(handle != NULL);
	handle->callback_add = callback;
	handle->callback_add_private_data = private_data;
	return 0;
}

static void callback_rebuild(snd_ctl_t *handle, void *private_data ATTRIBUTE_UNUSED)
{
	handle->herr = snd_ctl_hbuild(handle, handle->hsort);
	if (handle->herr >= 0 && handle->callback_rebuild)
		handle->callback_rebuild(handle, handle->callback_rebuild_private_data);
}

static void callback_change(snd_ctl_t *handle, void *private_data ATTRIBUTE_UNUSED, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;

	if (handle->herr < 0)
		return;
	hcontrol = snd_ctl_hfind(handle, id);
	if (hcontrol == NULL) {
		handle->herr = -ENOENT;
		return;
	}
	hcontrol->change = 1;
}

static void callback_value(snd_ctl_t *handle, void *private_data ATTRIBUTE_UNUSED, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;

	if (handle->herr < 0)
		return;
	hcontrol = snd_ctl_hfind(handle, id);
	if (hcontrol == NULL) {
		handle->herr = -ENOENT;
		return;
	}
	hcontrol->value = 1;
}

static void callback_add(snd_ctl_t *handle, void *private_data ATTRIBUTE_UNUSED, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol, *icontrol;

	if (handle->herr < 0)
		return;
	hcontrol = (snd_hcontrol_t *)calloc(1, sizeof(snd_hcontrol_t));
	if (hcontrol == NULL) {
		handle->herr = -ENOMEM;
		return;
	}
	hcontrol->id = *id;
	hcontrol->handle = handle;
	icontrol = tsearch(hcontrol, &handle->hroot, (__compar_fn_t)handle->hsort);
	if (icontrol == NULL) {
		free(hcontrol);
		handle->herr = -ENOMEM;
		return;
	}
	if (icontrol != hcontrol) {	/* double hit */
		free(hcontrol);
		return;
	}
	list_add_tail(&hcontrol->list, &handle->hlist);
	if (handle->callback_add)
		handle->callback_add(handle, handle->callback_add_private_data, hcontrol);
}

static void callback_remove(snd_ctl_t *handle, void *private_data ATTRIBUTE_UNUSED, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;

	if (handle->herr < 0)
		return;
	hcontrol = snd_ctl_hfind(handle, id);
	if (hcontrol == NULL) {
		handle->herr = -ENOENT;
		return;
	}
	if (tdelete(hcontrol, &handle->hroot, (__compar_fn_t)handle->hsort) != NULL)
		snd_ctl_hfree1(hcontrol);
}

int snd_ctl_hevent(snd_ctl_t *handle)
{
	static snd_ctl_callbacks_t callbacks = {
		rebuild: callback_rebuild,
		value: callback_value,
		change: callback_change,
		add: callback_add,
		remove: callback_remove,
		private_data: NULL,
		reserved: { NULL, }
	};
	struct list_head *list;
	snd_hcontrol_t *hcontrol;
	int res;

	assert(handle != NULL);
	handle->herr = 0;
	res = snd_ctl_read(handle, &callbacks);
	if (res < 0)
		return res;
	if (handle->herr < 0)
		return handle->herr;
	list_for_each(list, &handle->hlist) {
		hcontrol = list_entry(list, snd_hcontrol_t, list);
		if (hcontrol->change && hcontrol->event_change) {
			hcontrol->event_change(hcontrol->handle, hcontrol);
			hcontrol->change = 0;
		}
		if (hcontrol->value && hcontrol->event_value) {
			hcontrol->event_value(hcontrol->handle, hcontrol);
			hcontrol->value = 0;
		}			
	}
	return res;
}
