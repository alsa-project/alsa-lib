/*
 *  Control Interface - highlevel API - hcontrol bag operations
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
#include "control_local.h"

int snd_ctl_hbag_create(void **bag)
{
	assert(bag != NULL);
	*bag = NULL;
	return 0;
}

static void snd_ctl_hbag_free_private(snd_hcontrol_t *hcontrol ATTRIBUTE_UNUSED)
{
	/* nothing */
}

int snd_ctl_hbag_destroy(void **bag, void (*hcontrol_free)(snd_hcontrol_t *hcontrol))
{
	assert(bag != NULL);
	if (hcontrol_free == NULL)
		hcontrol_free = snd_ctl_hbag_free_private;
	tdestroy(*bag, (__free_fn_t)hcontrol_free);
	*bag = NULL;
	return 0;
}

int snd_ctl_hbag_add(void **bag, snd_hcontrol_t *hcontrol)
{
	void *res;

	assert(bag != NULL && hcontrol != NULL);
	res = tsearch(hcontrol, bag, (__compar_fn_t)snd_ctl_hsort);
	if (res == NULL)
		return -ENOMEM;
	if ((snd_hcontrol_t *)res == hcontrol)
		return -EALREADY;
	return 0;
}

int snd_ctl_hbag_del(void **bag, snd_hcontrol_t *hcontrol)
{
	assert(bag != NULL && hcontrol != NULL);
	if (tdelete(hcontrol, bag, (__compar_fn_t)snd_ctl_hsort) == NULL)
		return -ENOENT;
	return 0;
}

snd_hcontrol_t *snd_ctl_hbag_find(void **bag, snd_control_id_t *id)
{
	void *res;

	assert(bag != NULL && id != NULL);
	if (*bag == NULL)
		return NULL;
	res = tfind(id, bag, (__compar_fn_t)snd_ctl_hsort);
	return res == NULL ? NULL : *(snd_hcontrol_t **)res;
}
