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

#include <assert.h>
#include "asoundlib.h"
#include "list.h"

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define ERR(...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, 0, __VA_ARGS__)
#define SYSERR(...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, errno, __VA_ARGS__)
#else
#define ERR(args...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, 0, ##args)
#define SYSERR(args...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, errno, ##args)
#endif

typedef struct {
	int (*close)(snd_ctl_t *handle);
	int (*poll_descriptor)(snd_ctl_t *handle);
	int (*hw_info)(snd_ctl_t *handle, snd_ctl_hw_info_t *info);
	int (*clist)(snd_ctl_t *handle, snd_control_list_t *list);
	int (*cinfo)(snd_ctl_t *handle, snd_control_info_t *info);
	int (*cread)(snd_ctl_t *handle, snd_control_t *control);
	int (*cwrite)(snd_ctl_t *handle, snd_control_t *control);
	int (*hwdep_info)(snd_ctl_t *handle, snd_hwdep_info_t * info);
	int (*pcm_info)(snd_ctl_t *handle, snd_pcm_info_t * info);
	int (*pcm_prefer_subdevice)(snd_ctl_t *handle, int subdev);
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
	snd_ctl_hsort_t *hsort;
	snd_ctl_hcallback_rebuild_t *callback_rebuild;
	void *callback_rebuild_private_data;
	snd_ctl_hcallback_add_t *callback_add;
	void *callback_add_private_data;
};

int snd_ctl_hw_open(snd_ctl_t **handle, char *name, int card);
int snd_ctl_shm_open(snd_ctl_t **handlep, char *name, char *socket, char *sname);
