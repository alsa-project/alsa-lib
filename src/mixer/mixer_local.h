/*
 *  Mixer Interface - local header file
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
 *
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

#include "local.h"

typedef struct _bag1 {
	void *ptr;
	struct list_head list;
} bag1_t;

typedef struct list_head bag_t;

int bag_new(bag_t **bag);
void bag_free(bag_t *bag);
int bag_add(bag_t *bag, void *ptr);
int bag_del(bag_t *bag, void *ptr);
int bag_empty(bag_t *bag);
void bag_del_all(bag_t *bag);

typedef struct list_head *bag_iterator_t;

#define bag_iterator_entry(i) (list_entry((i), bag1_t, list)->ptr)
#define bag_for_each(pos, bag) list_for_each(pos, bag)
#define bag_for_each_safe(pos, next, bag) list_for_each_safe(pos, next, bag)

#define MIXER_COMPARE_WEIGHT_SIMPLE_BASE	0
#define MIXER_COMPARE_WEIGHT_NEXT_BASE		10000000
#define MIXER_COMPARE_WEIGHT_NOT_FOUND		1000000000

struct _snd_mixer_class {
	struct list_head list;
	snd_mixer_t *mixer;
	int (*event)(snd_mixer_class_t *class, unsigned int mask,
		     snd_hctl_elem_t *helem, snd_mixer_elem_t *melem);
	void *private_data;		
	void (*private_free)(snd_mixer_class_t *class);
	snd_mixer_compare_t compare;
};

struct _snd_mixer_elem {
	snd_mixer_elem_type_t type;
	struct list_head list;		/* links for list of all elems */
	snd_mixer_class_t *class;
	void *private_data;
	void (*private_free)(snd_mixer_elem_t *elem);
	snd_mixer_elem_callback_t callback;
	void *callback_private;
	bag_t helems;
	int compare_weight;		/* compare weight (reversed) */
};

struct _snd_mixer {
	struct list_head slaves;	/* list of all slaves */
	struct list_head classes;	/* list of all elem classes */
	struct list_head elems;		/* list of all elems */
	snd_mixer_elem_t **pelems;	/* array of all elems */
	unsigned int count;
	unsigned int alloc;
	unsigned int events;
	snd_mixer_callback_t callback;
	void *callback_private;
	snd_mixer_compare_t compare;
};

struct _snd_mixer_selem_id {
	unsigned char name[60];
	unsigned int index;
};

int snd_mixer_class_register(snd_mixer_class_t *class, snd_mixer_t *mixer);
int snd_mixer_add_elem(snd_mixer_t *mixer, snd_mixer_elem_t *elem);
int snd_mixer_remove_elem(snd_mixer_t *mixer, snd_mixer_elem_t *elem);
int snd_mixer_elem_add(snd_mixer_elem_t *elem, snd_mixer_class_t *class);
int snd_mixer_elem_remove(snd_mixer_elem_t *elem);
int snd_mixer_elem_info(snd_mixer_elem_t *elem);
int snd_mixer_elem_value(snd_mixer_elem_t *elem);
int snd_mixer_elem_attach(snd_mixer_elem_t *melem,
			  snd_hctl_elem_t *helem);
int snd_mixer_elem_detach(snd_mixer_elem_t *melem,
			  snd_hctl_elem_t *helem);
int snd_mixer_elem_empty(snd_mixer_elem_t *melem);
