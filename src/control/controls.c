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

static void snd_ctl_cfree1(snd_hcontrol_t *hcontrol);

int snd_ctl_cbuild(snd_ctl_t *handle, snd_ctl_csort_t *csort)
{
	snd_control_list_t list;
	snd_hcontrol_t *hcontrol, *prev;
	int err;
	unsigned int idx;

	printf("cbuild - start\n");
	assert(handle != NULL);
	if ((err = snd_ctl_cfree(handle)) < 0)
		return err;
	if (csort == NULL)
		csort = snd_ctl_csort;
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
		hcontrol->handle = handle;
		if (tsearch(hcontrol, &handle->croot, (__compar_fn_t)csort) == NULL) {
			tdestroy(&handle->croot, (__free_fn_t)snd_ctl_cfree1);
			handle->croot = NULL;
		}
		handle->ccount++;
	}
	if (list.pids != NULL)
		free(list.pids);
	handle->csort = csort;
	return 0;
}

static void snd_ctl_cfree1(snd_hcontrol_t *hcontrol)
{
	snd_ctl_t *handle;
	
	assert(hcontrol != NULL);
	handle = hcontrol->handle;
	assert(handle != NULL);
	assert(handle->ccount > 0);
	if (hcontrol->event_remove)
		hcontrol->event_remove(handle, hcontrol);
	if (hcontrol->private_free)
		hcontrol->private_free(hcontrol->private_data);
	free(hcontrol);
	handle->ccount--;
}

int snd_ctl_cfree(snd_ctl_t *handle)
{
	handle->csort = NULL;
	handle->cerr = 0;
	if (handle->croot != NULL) {
		tdestroy(handle->croot, (__free_fn_t)snd_ctl_cfree1);
		handle->croot = NULL;
	}
	assert(handle->ccount == 0);
	return 0;
}

int snd_ctl_csort(const snd_hcontrol_t *c1, const snd_hcontrol_t *c2)
{
	int res;

	res = strcmp(c1->id.name, c2->id.name);
	if (res == 0) {
		if (c1->id.index < c2->id.index)
			return -1;
		if (c1->id.index > c2->id.index)
			return 1;
		return 0;
	}
	return res;
}

static void snd_ctl_cresort_action(snd_hcontrol_t *hcontrol, VISIT which, int level)
{
	snd_ctl_t *handle;

	level = 0;			/* to keep GCC happy */
	assert(hcontrol != NULL);
	handle = hcontrol->handle;
	assert(handle != NULL);
	if (handle->cerr < 0)
		return;
	switch (which) {
	case preorder: break;
	case postorder: break;
	case endorder:
	case leaf:
		if (tsearch(hcontrol, &handle->croot, (__compar_fn_t)handle->csort) == NULL)
			handle->cerr = -ENOMEM;
		break;
	}
}

static void snd_ctl_cresort_free(snd_hcontrol_t *hcontrol)
{
	hcontrol = NULL;		/* to keep GCC happy */
	/* nothing */
}

int snd_ctl_cresort(snd_ctl_t *handle, snd_ctl_csort_t *csort)
{
	int result;
	snd_ctl_csort_t *csort_old;

	assert(handle != NULL && csort != NULL);
	if (handle->ccount == 0)
		return 0;
	if (handle->cerr < 0)
		return handle->cerr;
	assert(handle->croot_new == NULL);
	csort_old = handle->csort;
	handle->csort = csort;
	twalk(handle->croot, (__action_fn_t)snd_ctl_cresort_action);
	if (handle->cerr < 0) {
		result = handle->cerr;
		handle->cerr = 0;
		handle->csort = csort_old;
		tdestroy(handle->croot_new, (__free_fn_t)snd_ctl_cresort_free);
		handle->croot_new = NULL;
		return result;
	}
	tdestroy(handle->croot, (__free_fn_t)snd_ctl_cresort_free);
	handle->croot = handle->croot_new;
	handle->croot_new = NULL;
	return 0;
}

snd_hcontrol_t *snd_ctl_cfind(snd_ctl_t *handle, snd_control_id_t *id)
{
	assert(handle != NULL);
	if (handle->croot == NULL)
		return NULL;
	return (snd_hcontrol_t *)tfind(id, &handle->croot, (__compar_fn_t)handle->csort);
}

int snd_ctl_ccallback_rebuild(snd_ctl_t *handle, snd_ctl_ccallback_rebuild_t *callback, void *private_data)
{
	assert(handle != NULL);
	handle->callback_rebuild = callback;
	handle->callback_rebuild_private_data = private_data;
	return 0;
}

int snd_ctl_ccallback_add(snd_ctl_t *handle, snd_ctl_ccallback_add_t *callback, void *private_data)
{
	assert(handle != NULL);
	handle->callback_add = callback;
	handle->callback_add_private_data = private_data;
	return 0;
}

static void callback_rebuild(snd_ctl_t *handle, void *private_data)
{
	private_data = NULL;	/* to keep GCC happy */
	handle->cerr = snd_ctl_cbuild(handle, handle->csort);
	if (handle->cerr >= 0 && handle->callback_rebuild)
		handle->callback_rebuild(handle, handle->callback_rebuild_private_data);
}

static void callback_change(snd_ctl_t *handle, void *private_data, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;

	private_data = NULL;	/* to keep GCC happy */
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

	private_data = NULL;	/* to keep GCC happy */
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

	private_data = NULL;	/* to keep GCC happy */
	if (handle->cerr < 0)
		return;
	hcontrol = (snd_hcontrol_t *)calloc(1, sizeof(snd_hcontrol_t));
	if (hcontrol == NULL) {
		handle->cerr = -ENOMEM;
		return;
	}
	hcontrol->id = *id;
	hcontrol->handle = handle;
	icontrol = tsearch(hcontrol, &handle->croot, (__compar_fn_t)handle->csort);
	if (icontrol == NULL) {
		free(hcontrol);
		handle->cerr = -ENOMEM;
		return;
	}
	if (icontrol != hcontrol) {	/* double hit */
		free(hcontrol);
		return;
	}
	if (handle->callback_add)
		handle->callback_add(handle, handle->callback_add_private_data, hcontrol);
}

static void callback_remove(snd_ctl_t *handle, void *private_data, snd_control_id_t *id)
{
	snd_hcontrol_t *hcontrol;

	private_data = NULL;	/* to keep GCC happy */
	if (handle->cerr < 0)
		return;
	hcontrol = snd_ctl_cfind(handle, id);
	if (hcontrol == NULL) {
		handle->cerr = -ENOENT;
		return;
	}
	if (tdelete(hcontrol, &handle->croot, (__compar_fn_t)handle->csort) != NULL)
		snd_ctl_cfree1(hcontrol);
}

static void snd_ctl_cevent_walk1(snd_hcontrol_t *hcontrol, VISIT which, int level)
{
	level = 0;	/* to keep GCC happy */
	assert(hcontrol != NULL);
	switch (which) {
	case preorder: break;
	case postorder: break;
	case endorder:
	case leaf:
		if (hcontrol->change && hcontrol->event_change) {
			hcontrol->event_change(hcontrol->handle, hcontrol);
			hcontrol->change = 0;
		}
		if (hcontrol->value && hcontrol->event_value) {
			hcontrol->event_value(hcontrol->handle, hcontrol);
			hcontrol->value = 0;
		}			
		break;
	}
}

int snd_ctl_cevent(snd_ctl_t *handle)
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
	int res;

	assert(handle != NULL);
	handle->cerr = 0;
	res = snd_ctl_read(handle, &callbacks);
	if (res < 0)
		return res;
	if (handle->cerr < 0)
		return handle->cerr;
	twalk(handle->croot, (__action_fn_t)snd_ctl_cevent_walk1);
	return res;
}
