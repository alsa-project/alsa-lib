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
	int (*poll_descriptor)(snd_ctl_t *handle);
	int (*hw_info)(snd_ctl_t *handle, snd_ctl_info_t *info);
	int (*clist)(snd_ctl_t *handle, snd_control_list_t *list);
	int (*cinfo)(snd_ctl_t *handle, snd_control_info_t *info);
	int (*cread)(snd_ctl_t *handle, snd_control_t *control);
	int (*cwrite)(snd_ctl_t *handle, snd_control_t *control);
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
	int hcount;
	int herr;
	struct list_head hlist;	/* list of all controls */
	void *hroot;		/* root of controls */
	void *hroot_new;	/* new croot */
	snd_ctl_hsort_t hsort;
	snd_ctl_hcallback_rebuild_t callback_rebuild;
	void *callback_rebuild_private_data;
	snd_ctl_hcallback_add_t callback_add;
	void *callback_add_private_data;
};

struct _snd_ctl_callbacks {
	void *private_data;	/* may be used by an application */
	void (*rebuild) (snd_ctl_t *handle, void *private_data);
	void (*value) (snd_ctl_t *handle, void *private_data, snd_control_id_t * id);
	void (*change) (snd_ctl_t *handle, void *private_data, snd_control_id_t * id);
	void (*add) (snd_ctl_t *handle, void *private_data, snd_control_id_t * id);
	void (*remove) (snd_ctl_t *handle, void *private_data, snd_control_id_t * id);
	void *reserved[58];	/* reserved for the future use - must be NULL!!! */
};

struct _snd_hcontrol_list {
	unsigned int offset;	/* W: first control ID to get */
	unsigned int space;	/* W: count of control IDs to get */
	unsigned int used;	/* R: count of available (set) controls */
	unsigned int count;	/* R: count of all available controls */
	snd_control_id_t *pids;		/* W: IDs */
};

struct _snd_hcontrol {
	snd_control_id_t id; 	/* must be always on top */
	struct list_head list;	/* links for list of all hcontrols */
	int change: 1,		/* structure change */
	    value: 1;		/* value change */
	/* event callbacks */
	snd_hcontrol_callback_t callback_change;
	snd_hcontrol_callback_t callback_value;
	snd_hcontrol_callback_t callback_remove;
	/* private data */
	void *private_data;
	snd_hcontrol_private_free_t private_free;
	/* links */
	snd_ctl_t *handle;	/* associated handle */
};

int snd_ctl_hw_open(snd_ctl_t **handle, const char *name, int card);
int snd_ctl_shm_open(snd_ctl_t **handlep, const char *name, const char *socket, const char *sname);
