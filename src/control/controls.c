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
#include "asoundlib.h"
#include "control_local.h"

static void snd_ctl_link_after(snd_ctl_t *handle, snd_hcontrol_t *point, snd_hcontrol_t *hcontrol);

int snd_ctl_cbuild(snd_ctl_t *handle, snd_ctl_csort_t *csort)
{
	snd_control_list_t list;
	snd_hcontrol_t *hcontrol, *prev;
	int err, idx;

	assert(handle != NULL);
	if ((err = snd_ctl_cfree(handle)) < 0)
		return err;
      __rebuild:
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
		if (hcontrol == NULL) {
			snd_ctl_cfree(handle);
			free(list.pids);
			return -ENOMEM;
		}
		hcontrol->id = list.pids[idx];
		if (prev == NULL) {
			handle->cfirst = handle->clast = hcontrol;
			handle->ccount = 1;
		} else {
			snd_ctl_link_after(handle, prev, hcontrol);
		}
		prev = hcontrol;
	}
	if (list.pids != NULL)
		free(list.pids);
	if (csort != NULL && (err = snd_ctl_cresort(handle, csort)) < 0)
		return err;
	return 0;
}

snd_hcontrol_t *snd_ctl_cfirst(snd_ctl_t *handle)
{
	assert(handle != NULL);
	return handle->cfirst;
}

snd_hcontrol_t *snd_ctl_clast(snd_ctl_t *handle)
{
	assert(handle != NULL);
	return handle->clast;
}

static void snd_ctl_unlink(snd_ctl_t *handle, snd_hcontrol_t *hcontrol)
{
	if (handle->cfirst == hcontrol)
		handle->cfirst = hcontrol->next;
	if (handle->clast == hcontrol)
		handle->clast = hcontrol->prev;
	if (hcontrol->prev != NULL)
		hcontrol->prev->next = hcontrol->next;
	if (hcontrol->next != NULL)
		hcontrol->next->prev = hcontrol->prev;
	hcontrol->prev = hcontrol->next = NULL;
	handle->ccount--;
}

static void snd_ctl_link_before(snd_ctl_t *handle, snd_hcontrol_t *point, snd_hcontrol_t *hcontrol)
{
	if (point == handle->cfirst)
		handle->cfirst = hcontrol;
	hcontrol->next = point;
	hcontrol->prev = point->prev;
	if (point->prev != NULL)
		point->prev->next = hcontrol;
	point->prev = hcontrol;
	handle->ccount++;
}

static void snd_ctl_link_after(snd_ctl_t *handle, snd_hcontrol_t *point, snd_hcontrol_t *hcontrol)
{
	if (point == handle->clast)
		handle->clast = hcontrol;
	hcontrol->prev = point;
	hcontrol->next = point->next;
	if (point->next != NULL)
		point->next->prev = hcontrol;
	point->next = hcontrol;
	handle->ccount++;
}

static void snd_ctl_cfree1(snd_ctl_t *handle, snd_hcontrol_t *hcontrol)
{
	snd_ctl_unlink(handle, hcontrol);
	if (hcontrol->event_remove)
		hcontrol->event_remove(handle, hcontrol);
	if (hcontrol->private_free)
		hcontrol->private_free(hcontrol->private_data);
	free(hcontrol);
}

int snd_ctl_cfree(snd_ctl_t *handle)
{
	handle->csort = NULL;
	while (handle->cfirst)
		snd_ctl_cfree1(handle, handle->cfirst);
	assert(handle->ccount == 0);
}

int snd_ctl_csort(const snd_hcontrol_t **_c1, const snd_hcontrol_t **_c2)
{
	const snd_hcontrol_t *c1 = *_c1;
	const snd_hcontrol_t *c2 = *_c2;
	int res;

	res = strcmp(c1->id.name, c2->id.name);
	if (res == 0) {
		if (c1->id.index < c2->id.index)
			return -1;
		if (c1->id.index > c2->id.index)
			return 1;
		return 0;
	}
}

int snd_ctl_cresort(snd_ctl_t *handle, snd_ctl_csort_t *csort)
{
	int idx, count;
	snd_hcontrol_t **pmap, *hcontrol;

	assert(handle != NULL && csort != NULL);
	if (handle->ccount == 0)
		return 0;
	pmap = (snd_hcontrol_t **)calloc(handle->ccount, sizeof(snd_hcontrol_t *));
	if (pmap == NULL)
		return -ENOMEM;
	for (hcontrol = handle->cfirst, idx = 0; hcontrol != NULL; hcontrol = hcontrol->next, idx++) {
		printf("idx = %i, hcontrol = 0x%x (0x%x), '%s'\n", idx, (int)hcontrol, (int)&pmap[idx], hcontrol->id.name);
		pmap[idx] = hcontrol;
	}
	assert(idx == handle->ccount);
	handle->csort = csort;
	qsort(pmap, count = handle->ccount, sizeof(snd_hcontrol_t *), (int (*)(const void *, const void *))csort);
	while (handle->cfirst)
		snd_ctl_unlink(handle, handle->cfirst);
	handle->cfirst = handle->clast = pmap[0]; handle->ccount = 1;
	for (idx = 1; idx < count; idx++)
		snd_ctl_link_after(handle, pmap[idx-1], pmap[idx]);
	free(pmap);
	return 0;
}

snd_hcontrol_t *snd_ctl_cfind(snd_ctl_t *handle, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;
	
	assert(handle != NULL);
	for (hcontrol = handle->cfirst; hcontrol != NULL; hcontrol = hcontrol->next) {
		if (hcontrol->id.iface != id->iface)
			continue;
		if (hcontrol->id.device != id->device)
			continue;
		if (hcontrol->id.subdevice != id->subdevice)
			continue;
		if (strncmp(hcontrol->id.name, id->name, sizeof(hcontrol->id.name)))
			continue;
		if (hcontrol->id.index != id->index)
			continue;
		return hcontrol;
	}
	return NULL;
}

int snd_ctl_ccallback_rebuild(snd_ctl_t *handle, snd_ctl_ccallback_rebuild_t *callback, void *private_data)
{
	assert(handle != NULL);
	handle->callback_rebuild = callback;
	handle->callback_rebuild_private_data = private_data;
}

int snd_ctl_ccallback_add(snd_ctl_t *handle, snd_ctl_ccallback_add_t *callback, void *private_data)
{
	assert(handle != NULL);
	handle->callback_add = callback;
	handle->callback_add_private_data = private_data;
}

static void callback_rebuild(snd_ctl_t *handle, void *private_data)
{
	handle->cerr = snd_ctl_cbuild(handle, handle->csort);
	if (handle->cerr >= 0 && handle->callback_rebuild)
		handle->callback_rebuild(handle, handle->callback_rebuild_private_data);
}

static void callback_change(snd_ctl_t *handle, void *private_data, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;

	if (handle->cerr < 0)
		return;
	hcontrol = snd_ctl_cfind(handle, id);
	if (hcontrol == NULL) {
		handle->cerr = -ENOENT;
		return;
	}
	hcontrol->change = 1;
}

static void callback_value(snd_ctl_t *handle, void *private_data, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;

	if (handle->cerr < 0)
		return;
	hcontrol = snd_ctl_cfind(handle, id);
	if (hcontrol == NULL) {
		handle->cerr = -ENOENT;
		return;
	}
	hcontrol->value = 1;
}

static void callback_add(snd_ctl_t *handle, void *private_data, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol, *icontrol;

	if (handle->cerr < 0)
		return;
	hcontrol = (snd_hcontrol_t *)calloc(1, sizeof(snd_hcontrol_t));
	if (hcontrol == NULL) {
		handle->cerr = -ENOMEM;
		return;
	}
	hcontrol->id = *id;
	if (handle->csort != NULL) {
		for (icontrol = handle->cfirst; icontrol != NULL; icontrol = icontrol->next) {
			if (handle->csort((const snd_hcontrol_t **)&icontrol, (const snd_hcontrol_t **)&hcontrol) > 0) {
				snd_ctl_link_before(handle, icontrol, hcontrol);
				break;
			}
		}
		if (icontrol == NULL)
			snd_ctl_link_after(handle, handle->clast, hcontrol);
	} else {
		snd_ctl_link_after(handle, handle->clast, hcontrol);
	}
	if (handle->callback_add)
		handle->callback_add(handle, handle->callback_add_private_data, hcontrol);
}

static void callback_remove(snd_ctl_t *handle, void *private_data, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;

	if (handle->cerr < 0)
		return;
	hcontrol = snd_ctl_cfind(handle, id);
	if (hcontrol == NULL) {
		handle->cerr = -ENOENT;
		return;
	}
	snd_ctl_cfree1(handle, hcontrol);
}

int snd_ctl_cevent(snd_ctl_t *handle)
{
	snd_ctl_callbacks_t callbacks = {
		rebuild: callback_rebuild,
		value: callback_value,
		change: callback_change,
		add: callback_add,
		remove: callback_remove
	};
	snd_hcontrol_t *hcontrol;
	int res;

	assert(handle != NULL);
	handle->cerr = 0;
	res = snd_ctl_read(handle, &callbacks);
	if (res < 0)
		return res;
	if (handle->cerr < 0)
		return handle->cerr;
	for (hcontrol = handle->cfirst; hcontrol != NULL; hcontrol = hcontrol->next) {
		if (hcontrol->change && hcontrol->event_change) {
			hcontrol->event_change(handle, hcontrol);
			hcontrol->change = 0;
		}
		if (hcontrol->value && hcontrol->event_value) {
			hcontrol->event_value(handle, hcontrol);
			hcontrol->value = 0;
		}			
	}
	return res;
}
