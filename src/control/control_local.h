/*
 *  Control Interface - local header file
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

#include "local.h"
#include "list.h"

typedef struct _snd_ctl_ops {
	int (*close)(snd_ctl_t *handle);
	int (*nonblock)(snd_ctl_t *handle, int nonblock);
	int (*async)(snd_ctl_t *handle, int sig, pid_t pid);
	int (*poll_descriptor)(snd_ctl_t *handle);
	int (*hw_info)(snd_ctl_t *handle, snd_ctl_card_info_t *info);
	int (*element_list)(snd_ctl_t *handle, snd_ctl_elem_list_t *list);
	int (*element_info)(snd_ctl_t *handle, snd_ctl_elem_info_t *info);
	int (*element_read)(snd_ctl_t *handle, snd_ctl_elem_t *control);
	int (*element_write)(snd_ctl_t *handle, snd_ctl_elem_t *control);
	int (*hwdep_next_device)(snd_ctl_t *handle, int *device);
	int (*hwdep_info)(snd_ctl_t *handle, snd_hwdep_info_t * info);
	int (*pcm_next_device)(snd_ctl_t *handle, int *device);
	int (*pcm_info)(snd_ctl_t *handle, snd_pcm_info_t * info);
	int (*pcm_prefer_subdevice)(snd_ctl_t *handle, int subdev);
	int (*rawmidi_next_device)(snd_ctl_t *handle, int *device);
	int (*rawmidi_info)(snd_ctl_t *handle, snd_rawmidi_info_t * info);
	int (*rawmidi_prefer_subdevice)(snd_ctl_t *handle, int subdev);
	int (*read)(snd_ctl_t *handle, snd_ctl_event_t *event);
} snd_ctl_ops_t;


struct _snd_ctl {
	char *name;
	snd_ctl_type_t type;
	snd_ctl_ops_t *ops;
	void *private;
	int nonblock;
	struct list_head hlist;	/* list of all controls */
	unsigned int halloc;	
	unsigned int hcount;
	snd_hctl_elem_t **helems;
	snd_hctl_compare_t hcompare;
	snd_hctl_callback_t callback;
	void *callback_private;
};

struct _snd_hctl_elem {
	snd_ctl_elem_id_t id; 	/* must be always on top */
	struct list_head list;		/* links for list of all helems */
	/* event callback */
	snd_hctl_elem_callback_t callback;
	void *callback_private;
	/* links */
	snd_ctl_t *ctl;			/* associated handle */
};


int snd_ctl_hw_open(snd_ctl_t **handle, const char *name, int card);
int snd_ctl_shm_open(snd_ctl_t **handlep, const char *name, const char *socket, const char *sname);
int snd_hctl_free(snd_ctl_t *ctl);
int snd_hctl_compare_default(const snd_hctl_elem_t *c1,
			     const snd_hctl_elem_t *c2);

