/*
 *  IPC SHM area manager
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
  
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include "list.h"

struct snd_shm_area {
	struct list_head list;
	int shmid;
	void *ptr;
	int share;
};

static LIST_HEAD(shm_areas);

struct snd_shm_area *snd_shm_area_create(int shmid, void *ptr)
{
	struct snd_shm_area *area = malloc(sizeof(*area));
	if (area) {
		area->shmid = shmid;
		area->ptr = ptr;
		area->share = 1;
		list_add_tail(&area->list, &shm_areas);
	}
	return area;
}

struct snd_shm_area *snd_shm_area_share(struct snd_shm_area *area)
{
	if (area == NULL)
		return NULL;
	area->share++;
	return area;
}

int snd_shm_area_destroy(struct snd_shm_area *area)
{
	if (area == NULL)
		return -ENOENT;
	if (--area->share)
		return 0;
	list_del(&area->list);
	shmdt(area->ptr);
	free(area);
	return 0;
}

void snd_shm_area_destructor(void) __attribute__ ((destructor));

void snd_shm_area_destructor(void)
{
	struct list_head *pos;
	struct snd_shm_area *area;

	list_for_each(pos, &shm_areas) {
		area = list_entry(pos, struct snd_shm_area, list);
		shmdt(area->ptr);
	}
}
