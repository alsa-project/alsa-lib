/*
 *  Control Interface - highlevel API - helem bag operations
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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
#define __USE_GNU
#include <search.h>
#include "mixer_local.h"

int snd_hctl_compare_fast(const snd_hctl_elem_t *c1,
			  const snd_hctl_elem_t *c2);

static void _free(void *ptr ATTRIBUTE_UNUSED) { };

int snd_hctl_bag_destroy(snd_hctl_bag_t *bag)
{
	assert(bag != NULL);
	tdestroy(bag->root, _free);
	bag->root = NULL;
	return 0;
}

int snd_hctl_bag_add(snd_hctl_bag_t *bag, snd_hctl_elem_t *helem)
{
	void *res;
	assert(bag != NULL && helem != NULL);
	res = tsearch(helem, &bag->root, (__compar_fn_t)snd_hctl_compare_fast);
	if (res == NULL)
		return -ENOMEM;
	if ((snd_hctl_elem_t *)res == helem)
		return -EALREADY;
	return 0;
}

int snd_hctl_bag_del(snd_hctl_bag_t *bag, snd_hctl_elem_t *helem)
{
	assert(bag != NULL && helem != NULL);
	if (tdelete(helem, &bag->root, (__compar_fn_t)snd_hctl_compare_fast) == NULL)
		return -ENOENT;
	return 0;
}

snd_hctl_elem_t *snd_hctl_bag_find(snd_hctl_bag_t *bag, snd_ctl_elem_id_t *id)
{
	void *res;
	assert(bag != NULL && id != NULL);
	if (bag->root == NULL)
		return NULL;
	res = tfind(id, &bag->root, (__compar_fn_t)snd_hctl_compare_fast);
	return res == NULL ? NULL : *(snd_hctl_elem_t **)res;
}

int snd_hctl_bag_empty(snd_hctl_bag_t *bag)
{
	assert(bag != NULL);
	return bag->root == NULL;
}
