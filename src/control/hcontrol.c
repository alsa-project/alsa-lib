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

#define NOT_FOUND 1000000000

static int snd_hctl_compare_default(const snd_hctl_elem_t *c1,
				    const snd_hctl_elem_t *c2);

int snd_hctl_open(snd_hctl_t **hctlp, const char *name, int mode)
{
	snd_hctl_t *hctl;
	snd_ctl_t *ctl;
	int err;
	
	assert(hctlp);
	*hctlp = NULL;
	if ((err = snd_ctl_open(&ctl, name, mode)) < 0)
		return err;
	if ((hctl = (snd_hctl_t *)calloc(1, sizeof(snd_hctl_t))) == NULL) {
		snd_ctl_close(ctl);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&hctl->elems);
	hctl->ctl = ctl;
	*hctlp = hctl;
	return 0;
}

int snd_hctl_close(snd_hctl_t *hctl)
{
	int err;

	assert(hctl);
	err = snd_ctl_close(hctl->ctl);
	snd_hctl_free(hctl);
	free(hctl);
	return err;
}

const char *snd_hctl_name(snd_hctl_t *hctl)
{
	assert(hctl);
	return snd_ctl_name(hctl->ctl);
}

int snd_hctl_nonblock(snd_hctl_t *hctl, int nonblock)
{
	assert(hctl);
	return snd_ctl_nonblock(hctl->ctl, nonblock);
}

int snd_hctl_async(snd_hctl_t *hctl, int sig, pid_t pid)
{
	assert(hctl);
	return snd_ctl_async(hctl->ctl, sig, pid);
}

int snd_hctl_poll_descriptors_count(snd_hctl_t *hctl)
{
	assert(hctl);
	return snd_ctl_poll_descriptors_count(hctl->ctl);
}

int snd_hctl_poll_descriptors(snd_hctl_t *hctl, struct pollfd *pfds, unsigned int space)
{
	assert(hctl);
	return snd_ctl_poll_descriptors(hctl->ctl, pfds, space);
}

int snd_hctl_throw_event(snd_hctl_t *hctl, unsigned int mask,
			 snd_hctl_elem_t *elem)
{
	if (hctl->callback)
		return hctl->callback(hctl, mask, elem);
	return 0;
}

int snd_hctl_elem_throw_event(snd_hctl_elem_t *elem,
			      unsigned int mask)
{
	if (elem->callback)
		return elem->callback(elem, mask);
	return 0;
}

static int snd_hctl_compare_mixer_priority_lookup(char **name, char * const *names, int coef)
{
	int res;

	for (res = 0; *names; names++, res += coef) {
		if (!strncmp(*name, *names, strlen(*names))) {
			*name += strlen(*names);
			if (**name == ' ')
				(*name)++;
			return res+1;
		}
	}
	return NOT_FOUND;
}

static int get_compare_weight(const char *name)
{
	static char *names[] = {
		"Master",
		"Hardware Master",
		"Headphone",
		"Tone Control",
		"3D Control",
		"PCM",
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
		"Mono",
		"Front",
		"Rear",
		"Pan",
		"Output",
		"-",
		NULL
	};
	static char *names2[] = {
		"Switch",
		"Volume",
		"Bypass",
		"Depth",
		"Wide",
		"Space",
		"Level",
		"Center",
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

static int _snd_hctl_find_elem(snd_hctl_t *hctl, const snd_ctl_elem_id_t *id, int *dir)
{
	unsigned int l, u;
	snd_hctl_elem_t el;
	int c = 0;
	int idx = -1;
	assert(hctl && id);
	assert(hctl->compare);
	el.id = *id;
	el.compare_weight = get_compare_weight(id->name);
	l = 0;
	u = hctl->count;
	while (l < u) {
		idx = (l + u) / 2;
		c = hctl->compare(&el, hctl->pelems[idx]);
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

static int snd_hctl_elem_add(snd_hctl_t *hctl, snd_hctl_elem_t *elem)
{
	int dir;
	int idx; 
	if (elem->id.iface == SNDRV_CTL_ELEM_IFACE_MIXER)
		elem->compare_weight = get_compare_weight(elem->id.name);
	if (hctl->count == hctl->alloc) {
		snd_hctl_elem_t **h;
		hctl->alloc += 32;
		h = realloc(hctl->pelems, sizeof(*h) * hctl->alloc);
		if (!h) {
			hctl->alloc -= 32;
			return -ENOMEM;
		}
		hctl->pelems = h;
	}
	if (hctl->count == 0) {
		list_add_tail(&elem->list, &hctl->elems);
		hctl->pelems[0] = elem;
	} else {
		idx = _snd_hctl_find_elem(hctl, &elem->id, &dir);
		assert(dir != 0);
		if (dir > 0) {
			list_add(&elem->list, &hctl->pelems[idx]->list);
			idx++;
		} else {
			list_add_tail(&elem->list, &hctl->pelems[idx]->list);
		}
		memmove(hctl->pelems + idx + 1,
			hctl->pelems + idx,
			(hctl->count - idx) * sizeof(snd_hctl_elem_t *));
		hctl->pelems[idx] = elem;
	}
	hctl->count++;
	return snd_hctl_throw_event(hctl, SNDRV_CTL_EVENT_MASK_ADD, elem);
}

static void snd_hctl_elem_remove(snd_hctl_t *hctl, unsigned int idx)
{
	snd_hctl_elem_t *elem = hctl->pelems[idx];
	unsigned int m;
	snd_hctl_elem_throw_event(elem, SNDRV_CTL_EVENT_MASK_REMOVE);
	list_del(&elem->list);
	free(elem);
	hctl->count--;
	m = hctl->count - idx;
	if (m > 0)
		memmove(hctl->pelems + idx,
			hctl->pelems + idx + 1,
			m * sizeof(snd_hctl_elem_t *));
}

int snd_hctl_free(snd_hctl_t *hctl)
{
	while (hctl->count > 0)
		snd_hctl_elem_remove(hctl, hctl->count - 1);
	free(hctl->pelems);
	hctl->pelems = 0;
	hctl->alloc = 0;
	INIT_LIST_HEAD(&hctl->elems);
	return 0;
}

static void snd_hctl_sort(snd_hctl_t *hctl)
{
	unsigned int k;
	int compar(const void *a, const void *b) {
		return hctl->compare(*(const snd_hctl_elem_t **) a,
				     *(const snd_hctl_elem_t **) b);
	}
	assert(hctl);
	assert(hctl->compare);
	INIT_LIST_HEAD(&hctl->elems);
	qsort(hctl->pelems, hctl->count, sizeof(*hctl->pelems), compar);
	for (k = 0; k < hctl->count; k++)
		list_add_tail(&hctl->pelems[k]->list, &hctl->elems);
}

int snd_hctl_set_compare(snd_hctl_t *hctl, snd_hctl_compare_t hsort)
{
	assert(hctl);
	hctl->compare = hsort == NULL ? snd_hctl_compare_default : hsort;
	snd_hctl_sort(hctl);
	return 0;
}

int snd_hctl_compare_fast(const snd_hctl_elem_t *c1,
			  const snd_hctl_elem_t *c2)
{
	return c1->id.numid - c2->id.numid;
}

static int snd_hctl_compare_default(const snd_hctl_elem_t *c1,
				    const snd_hctl_elem_t *c2)
{
	int res;
	int d = c1->id.iface - c2->id.iface;
	if (d != 0)
		return d;
	if (c1->id.iface == SNDRV_CTL_ELEM_IFACE_MIXER) {
		d = c1->compare_weight - c2->compare_weight;
		if (d != 0)
			return d;
	}
	res = strcmp(c1->id.name, c2->id.name);
	if (res != 0)
		return res;
	d = c1->id.index - c2->id.index;
	return d;
}

snd_hctl_elem_t *snd_hctl_first_elem(snd_hctl_t *hctl)
{
	assert(hctl);
	if (list_empty(&hctl->elems))
		return NULL;
	return list_entry(hctl->elems.next, snd_hctl_elem_t, list);
}

snd_hctl_elem_t *snd_hctl_last_elem(snd_hctl_t *hctl)
{
	assert(hctl);
	if (list_empty(&hctl->elems))
		return NULL;
	return list_entry(hctl->elems.prev, snd_hctl_elem_t, list);
}

snd_hctl_elem_t *snd_hctl_elem_next(snd_hctl_elem_t *elem)
{
	assert(elem);
	if (elem->list.next == &elem->hctl->elems)
		return NULL;
	return list_entry(elem->list.next, snd_hctl_elem_t, list);
}

snd_hctl_elem_t *snd_hctl_elem_prev(snd_hctl_elem_t *elem)
{
	assert(elem);
	if (elem->list.prev == &elem->hctl->elems)
		return NULL;
	return list_entry(elem->list.prev, snd_hctl_elem_t, list);
}

snd_hctl_elem_t *snd_hctl_find_elem(snd_hctl_t *hctl, const snd_ctl_elem_id_t *id)
{
	int dir;
	int res = _snd_hctl_find_elem(hctl, id, &dir);
	if (res < 0 || dir != 0)
		return NULL;
	return hctl->pelems[res];
}

int snd_hctl_load(snd_hctl_t *hctl)
{
	snd_ctl_elem_list_t list;
	int err = 0;
	unsigned int idx;

	assert(hctl);
	assert(hctl->ctl);
	assert(hctl->count == 0);
	assert(list_empty(&hctl->elems));
	memset(&list, 0, sizeof(list));
	if ((err = snd_ctl_elem_list(hctl->ctl, &list)) < 0)
		goto _end;
	while (list.count != list.used) {
		err = snd_ctl_elem_list_alloc_space(&list, list.count);
		if (err < 0)
			goto _end;
		if ((err = snd_ctl_elem_list(hctl->ctl, &list)) < 0)
			goto _end;
	}
	if (hctl->alloc < list.count) {
		hctl->alloc = list.count;
		free(hctl->pelems);
		hctl->pelems = malloc(hctl->alloc * sizeof(*hctl->pelems));
		if (!hctl->pelems) {
			err = -ENOMEM;
			goto _end;
		}
	}
	for (idx = 0; idx < list.count; idx++) {
		snd_hctl_elem_t *elem;
		elem = calloc(1, sizeof(snd_hctl_elem_t));
		if (elem == NULL) {
			snd_hctl_free(hctl);
			err = -ENOMEM;
			goto _end;
		}
		elem->id = list.pids[idx];
		elem->hctl = hctl;
		if (elem->id.iface == SNDRV_CTL_ELEM_IFACE_MIXER)
			elem->compare_weight = get_compare_weight(elem->id.name);
		hctl->pelems[idx] = elem;
		list_add_tail(&elem->list, &hctl->elems);
		hctl->count++;
	}
	if (!hctl->compare)
		hctl->compare = snd_hctl_compare_default;
	snd_hctl_sort(hctl);
	for (idx = 0; idx < hctl->count; idx++) {
		int res = snd_hctl_throw_event(hctl, SNDRV_CTL_EVENT_MASK_ADD,
					       hctl->pelems[idx]);
		if (res < 0)
			return res;
	}
	err = snd_ctl_subscribe_events(hctl->ctl, 1);
 _end:
	if (list.pids)
		free(list.pids);
	return err;
}

void snd_hctl_set_callback(snd_hctl_t *hctl, snd_hctl_callback_t callback)
{
	assert(hctl);
	hctl->callback = callback;
}

void snd_hctl_set_callback_private(snd_hctl_t *hctl, void *callback_private)
{
	assert(hctl);
	hctl->callback_private = callback_private;
}

void *snd_hctl_get_callback_private(snd_hctl_t *hctl)
{
	assert(hctl);
	return hctl->callback_private;
}

unsigned int snd_hctl_get_count(snd_hctl_t *hctl)
{
	return hctl->count;
}

int snd_hctl_wait(snd_hctl_t *hctl, int timeout)
{
	struct pollfd pfd;
	int err;
	err = snd_hctl_poll_descriptors(hctl, &pfd, 1);
	assert(err == 1);
	err = poll(&pfd, 1, timeout);
	if (err < 0)
		return -errno;
	return 0;
}

int snd_hctl_handle_event(snd_hctl_t *hctl, snd_ctl_event_t *event)
{
	snd_hctl_elem_t *elem;
	int res;

	assert(hctl);
	assert(hctl->ctl);
	switch (event->type) {
	case SND_CTL_EVENT_ELEM:
		break;
	default:
		return 0;
	}
	if (event->data.elem.mask == SNDRV_CTL_EVENT_MASK_REMOVE) {
		int dir;
		res = _snd_hctl_find_elem(hctl, &event->data.elem.id, &dir);
		assert(res >= 0 && dir == 0);
		if (res < 0 || dir != 0)
			return -ENOENT;
		snd_hctl_elem_remove(hctl, res);
		return 0;
	}
	if (event->data.elem.mask & SNDRV_CTL_EVENT_MASK_ADD) {
		elem = calloc(1, sizeof(snd_hctl_elem_t));
		if (elem == NULL)
			return -ENOMEM;
		elem->id = event->data.elem.id;
		elem->hctl = hctl;
		res = snd_hctl_elem_add(hctl, elem);
		if (res < 0)
			return res;
	}
	if (event->data.elem.mask & (SNDRV_CTL_EVENT_MASK_VALUE |
				     SNDRV_CTL_EVENT_MASK_INFO)) {
		elem = snd_hctl_find_elem(hctl, &event->data.elem.id);
		assert(elem);
		if (!elem)
			return -ENOENT;
		res = snd_hctl_elem_throw_event(elem, event->data.elem.mask &
						(SNDRV_CTL_EVENT_MASK_VALUE |
						 SNDRV_CTL_EVENT_MASK_INFO));
		if (res < 0)
			return res;
	}
	return 0;
}

int snd_hctl_handle_events(snd_hctl_t *hctl)
{
	snd_ctl_event_t event;
	int res;
	unsigned int count = 0;
	
	assert(hctl);
	assert(hctl->ctl);
	while ((res = snd_ctl_read(hctl->ctl, &event)) != 0 &&
	       res != -EAGAIN) {
		if (res < 0)
			return res;
		res = snd_hctl_handle_event(hctl, &event);
		if (res < 0)
			return res;
		count++;
	}
	return count;
}

int snd_hctl_elem_info(snd_hctl_elem_t *elem, snd_ctl_elem_info_t *info)
{
	assert(elem);
	assert(elem->hctl);
	assert(info);
	info->id = elem->id;
	return snd_ctl_elem_info(elem->hctl->ctl, info);
}

int snd_hctl_elem_read(snd_hctl_elem_t *elem, snd_ctl_elem_value_t * value)
{
	assert(elem);
	assert(elem->hctl);
	assert(value);
	value->id = elem->id;
	return snd_ctl_elem_read(elem->hctl->ctl, value);
}

int snd_hctl_elem_write(snd_hctl_elem_t *elem, snd_ctl_elem_value_t * value)
{
	assert(elem);
	assert(elem->hctl);
	assert(value);
	value->id = elem->id;
	return snd_ctl_elem_write(elem->hctl->ctl, value);
}

snd_hctl_t *snd_hctl_elem_get_hctl(snd_hctl_elem_t *elem)
{
	assert(elem);
	return elem->hctl;
}
