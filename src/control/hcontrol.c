/*
 *  Control Interface - highlevel API
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
#include "control_local.h"

static int _snd_hctl_find_elem(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id, int *dir)
{
	unsigned int l, u;
	int c = 0;
	int idx = -1;
	assert(ctl && id);
	assert(ctl->hcompare);
	l = 0;
	u = ctl->hcount;
	while (l < u) {
		idx = (l + u) / 2;
		c = ctl->hcompare((snd_hctl_elem_t *) id, ctl->helems[idx]);
		if (c < 0)
			u = idx;
		else if (c > 0)
			l = idx + 1;
		else
			break;
	}
	*dir = c;
	return idx;
}

static int snd_hctl_elem_add(snd_ctl_t *ctl, snd_hctl_elem_t *elem)
{
	int dir;
	int idx; 
	if (ctl->hcount == ctl->halloc) {
		snd_hctl_elem_t **h;
		ctl->halloc += 32;
		h = realloc(ctl->helems, sizeof(*h) * ctl->halloc);
		if (!h)
			return -ENOMEM;
		ctl->helems = h;
	}
	if (ctl->hcount == 0) {
		list_add_tail(&elem->list, &ctl->hlist);
		ctl->helems[0] = elem;
	} else {
		idx = _snd_hctl_find_elem(ctl, &elem->id, &dir);
		assert(dir != 0);
		if (dir > 0) {
			list_add(&elem->list, &ctl->helems[idx]->list);
		} else {
			list_add_tail(&elem->list, &ctl->helems[idx]->list);
			idx++;
		}
		memmove(ctl->helems + idx + 1,
			ctl->helems + idx,
			ctl->hcount - idx);
	}
	ctl->hcount++;
	if (ctl->callback) {
		int res = ctl->callback(ctl, SND_CTL_EVENT_ADD, elem);
		if (res < 0)
			return res;
	}
	return 0;
}


static void snd_hctl_elem_remove(snd_ctl_t *ctl, unsigned int idx)
{
	snd_hctl_elem_t *elem = ctl->helems[idx];
	unsigned int m;
	if (elem->callback)
		elem->callback(elem, SND_CTL_EVENT_REMOVE);
	list_del(&elem->list);
	free(elem);
	ctl->hcount--;
	m = ctl->hcount - idx;
	if (m > 0)
		memmove(ctl->helems + idx, ctl->helems + idx + 1, m);
}

int snd_hctl_free(snd_ctl_t *ctl)
{
	while (ctl->hcount > 0)
		snd_hctl_elem_remove(ctl, ctl->hcount - 1);
	free(ctl->helems);
	ctl->helems = 0;
	ctl->halloc = 0;
	INIT_LIST_HEAD(&ctl->hlist);
	return 0;
}

static void snd_hctl_sort(snd_ctl_t *ctl)
{
	unsigned int k;
	int compar(const void *a, const void *b) {
		return ctl->hcompare(*(const snd_hctl_elem_t **) a,
				     *(const snd_hctl_elem_t **) b);
	}
	assert(ctl);
	assert(ctl->hcompare);
	INIT_LIST_HEAD(&ctl->hlist);
	qsort(ctl->helems, ctl->hcount, sizeof(*ctl->helems), compar);
	for (k = 0; k < ctl->hcount; k++)
		list_add_tail(&ctl->helems[k]->list, &ctl->hlist);
}

void snd_hctl_set_compare(snd_ctl_t *ctl, snd_hctl_compare_t hsort)
{
	assert(ctl);
	ctl->hcompare = hsort;
	snd_hctl_sort(ctl);
}

#define NOT_FOUND 1000000000

static int snd_hctl_compare_mixer_priority_lookup(char **name, char * const *names, int coef)
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

static int snd_hctl_compare_mixer_priority(const char *name)
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
	
	if ((res = snd_hctl_compare_mixer_priority_lookup((char **)&name, names, 1000000)) == NOT_FOUND)
		return NOT_FOUND;
	if ((res1 = snd_hctl_compare_mixer_priority_lookup((char **)&name, names1, 1000)) == NOT_FOUND)
		return res;
	res += res1;
	if ((res1 = snd_hctl_compare_mixer_priority_lookup((char **)&name, names2, 1)) == NOT_FOUND)
		return res;
	return res + res1;
}

int snd_hctl_compare_fast(const snd_hctl_elem_t *c1,
			  const snd_hctl_elem_t *c2)
{
	return c1->id.numid - c2->id.numid;
}

int snd_hctl_compare_default(const snd_hctl_elem_t *c1,
			     const snd_hctl_elem_t *c2)
{
	int res, p1, p2;
	int d = c1->id.iface - c2->id.iface;
	if (d != 0)
		return d;
	if ((res = strcmp(c1->id.name, c2->id.name)) != 0) {
		if (c1->id.iface != SNDRV_CTL_ELEM_IFACE_MIXER)
			return res;
		p1 = snd_hctl_compare_mixer_priority(c1->id.name);
		p2 = snd_hctl_compare_mixer_priority(c2->id.name);
		d = p1 - p2;
		if (d != 0)
			return d;
		return res;
	}
	d = c1->id.index - c2->id.index;
	return d;
}

snd_hctl_elem_t *snd_hctl_first_elem(snd_ctl_t *ctl)
{
	assert(ctl);
	if (list_empty(&ctl->hlist))
		return NULL;
	return list_entry(ctl->hlist.next, snd_hctl_elem_t, list);
}

snd_hctl_elem_t *snd_hctl_last_elem(snd_ctl_t *ctl)
{
	assert(ctl);
	if (list_empty(&ctl->hlist))
		return NULL;
	return list_entry(ctl->hlist.prev, snd_hctl_elem_t, list);
}

snd_hctl_elem_t *snd_hctl_elem_next(snd_hctl_elem_t *elem)
{
	assert(elem);
	if (elem->list.next == &elem->ctl->hlist)
		return NULL;
	return list_entry(elem->list.next, snd_hctl_elem_t, list);
}

snd_hctl_elem_t *snd_hctl_elem_prev(snd_hctl_elem_t *elem)
{
	assert(elem);
	if (elem->list.prev == &elem->ctl->hlist)
		return NULL;
	return list_entry(elem->list.prev, snd_hctl_elem_t, list);
}

snd_hctl_elem_t *snd_hctl_find_elem(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id)
{
	int dir;
	int res = _snd_hctl_find_elem(ctl, id, &dir);
	if (res < 0 || dir != 0)
		return NULL;
	return ctl->helems[res];
}

int snd_hctl_build(snd_ctl_t *ctl)
{
	snd_ctl_elem_list_t list;
	int err = 0;
	unsigned int idx;

	assert(ctl);
	assert(ctl->hcount == 0);
	assert(list_empty(&ctl->hlist));
	memset(&list, 0, sizeof(list));
	if ((err = snd_ctl_elem_list(ctl, &list)) < 0)
		goto _end;
	while (list.count != list.used) {
		err = snd_ctl_elem_list_alloc_space(&list, list.count);
		if (err < 0)
			goto _end;
		if ((err = snd_ctl_elem_list(ctl, &list)) < 0)
			goto _end;
	}
	if (ctl->halloc < list.count) {
		ctl->halloc = list.count;
		free(ctl->helems);
		ctl->helems = malloc(ctl->halloc * sizeof(*ctl->helems));
		if (!ctl->helems) {
			err = -ENOMEM;
			goto _end;
		}
	}
	for (idx = 0; idx < list.count; idx++) {
		snd_hctl_elem_t *elem;
		elem = calloc(1, sizeof(snd_hctl_elem_t));
		if (elem == NULL) {
			snd_hctl_free(ctl);
			err = -ENOMEM;
			goto _end;
		}
		elem->id = list.pids[idx];
		elem->ctl = ctl;
		ctl->helems[idx] = elem;
		list_add_tail(&elem->list, &ctl->hlist);
		ctl->hcount++;
	}
	if (!ctl->hcompare)
		ctl->hcompare = snd_hctl_compare_default;
	snd_hctl_sort(ctl);
	if (ctl->callback) {
		for (idx = 0; idx < ctl->hcount; idx++) {
			int res = ctl->callback(ctl, SND_CTL_EVENT_ADD,
						ctl->helems[idx]);
			if (res < 0)
				return res;
		}
	}
 _end:
	if (list.pids)
		free(list.pids);
	return err;
}

void snd_hctl_set_callback(snd_ctl_t *ctl, snd_hctl_callback_t callback)
{
	assert(ctl);
	ctl->callback = callback;
}

void snd_hctl_set_callback_private(snd_ctl_t *ctl, void *callback_private)
{
	assert(ctl);
	ctl->callback_private = callback_private;
}

void *snd_hctl_get_callback_private(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->callback_private;
}

unsigned int snd_hctl_get_count(snd_ctl_t *ctl)
{
	return ctl->hcount;
}

int snd_hctl_event(snd_ctl_t *ctl, snd_ctl_event_t *event)
{
	snd_hctl_elem_t *elem;
	int res;

	assert(ctl);
	switch (event->type) {
	case SND_CTL_EVENT_REMOVE:
	{
		int dir;
		res = _snd_hctl_find_elem(ctl, &event->data.id, &dir);
		assert(res >= 0 && dir == 0);
		if (res < 0 || dir != 0)
			return -ENOENT;
		snd_hctl_elem_remove(ctl, res);
		break;
	}
	case SND_CTL_EVENT_VALUE:
	case SND_CTL_EVENT_CHANGE:
		elem = snd_hctl_find_elem(ctl, &event->data.id);
		assert(elem);
		if (!elem)
			return -ENOENT;
		if (elem->callback) {
			res = elem->callback(elem, event->type);
			if (res < 0)
				return res;
		}
		break;
	case SND_CTL_EVENT_ADD:
		elem = calloc(1, sizeof(snd_hctl_elem_t));
		if (elem == NULL)
			return -ENOMEM;
		elem->id = event->data.id;
		elem->ctl = ctl;
		res = snd_hctl_elem_add(ctl, elem);
		if (res < 0)
			return res;
		break;
	case SND_CTL_EVENT_REBUILD:
		snd_hctl_free(ctl);
		res = snd_hctl_build(ctl);
		if (ctl->callback) {
			res = ctl->callback(ctl, event->type, NULL);
			if (res < 0)
				return res;
		}
		break;
	default:
		assert(0);
		break;
	}
	return 0;
}

int snd_hctl_events(snd_ctl_t *ctl)
{
	snd_ctl_event_t event;
	int res;
	while ((res = snd_ctl_read(ctl, &event)) != 0) {
		if (res < 0)
			return res;
		res = snd_hctl_event(ctl, &event);
		if (res < 0)
			return res;
	}
	return 0;
}

int snd_hctl_elem_info(snd_hctl_elem_t *elem, snd_ctl_elem_info_t *info)
{
	info->id = elem->id;
	return snd_ctl_elem_info(elem->ctl, info);
}

int snd_hctl_elem_read(snd_hctl_elem_t *elem, snd_ctl_elem_t * value)
{
	value->id = elem->id;
	return snd_ctl_elem_read(elem->ctl, value);
}

int snd_hctl_elem_write(snd_hctl_elem_t *elem, snd_ctl_elem_t * value)
{
	value->id = elem->id;
	return snd_ctl_elem_write(elem->ctl, value);
}

