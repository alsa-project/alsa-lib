/*
 *  Mixer Interface - local header file
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

//#include "../control/control_local.h"
#include "list.h"
#include "local.h"

typedef struct _snd_hctl_bag {
	void *root;
	void *private;
} snd_hctl_bag_t;

int snd_hctl_bag_destroy(snd_hctl_bag_t *bag);
int snd_hctl_bag_add(snd_hctl_bag_t *bag, snd_hctl_elem_t *helem);
int snd_hctl_bag_del(snd_hctl_bag_t *bag, snd_hctl_elem_t *helem);
snd_hctl_elem_t *snd_hctl_bag_find(snd_hctl_bag_t *bag, snd_ctl_elem_id_t *id);
int snd_hctl_bag_empty(snd_hctl_bag_t *bag);

struct _snd_mixer_elem {
	snd_mixer_elem_type_t type;
	struct list_head list;		/* links for list of all elems */
	void *private;
	void (*private_free)(snd_mixer_elem_t *elem);
	snd_mixer_elem_callback_t callback;
	void *callback_private;
	snd_mixer_t *mixer;
};

struct _snd_mixer {
	snd_ctl_t *ctl;
	struct list_head elems;	/* list of all elemss */
	unsigned int count;
	snd_mixer_callback_t callback;
	void *callback_private;
};

#define SND_MIXER_SCTCAP_VOLUME         (1<<0)
#define SND_MIXER_SCTCAP_JOIN_VOLUME	(1<<1)
#define SND_MIXER_SCTCAP_MUTE           (1<<2)
#define SND_MIXER_SCTCAP_JOIN_MUTE   	(1<<3)
#define SND_MIXER_SCTCAP_CAPTURE        (1<<4)
#define SND_MIXER_SCTCAP_JOIN_CAPTURE	(1<<5)
#define SND_MIXER_SCTCAP_EXCL_CAPTURE   (1<<6)

struct _snd_mixer_selem_id {
	unsigned char name[60];
	unsigned int index;
};

struct _snd_mixer_selem {
	unsigned int caps;		/* RO: capabilities */
	unsigned int channels;		/* RO: bitmap of active channels */
	unsigned int mute;		/* RW: bitmap of muted channels */
	unsigned int capture;		/* RW: bitmap of capture channels */
	int capture_group;		/* RO: capture group (for exclusive capture) */
	long min;			/* RO: minimum value */
	long max;			/* RO: maximum value */
	long volume[32];
};

int snd_mixer_add_elem(snd_mixer_t *mixer, snd_mixer_elem_t *elem);
void snd_mixer_free(snd_mixer_t *mixer);
