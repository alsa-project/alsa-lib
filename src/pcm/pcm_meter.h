/*
 *  PCM - Meter plugin
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
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

#include <pthread.h>
#include <asm/atomic.h>
#include "list.h"
#include "pcm_local.h"
#include "pcm_plugin.h"

typedef struct _snd_pcm_meter_scope snd_pcm_meter_scope_t;

struct _snd_pcm_meter_scope {
	snd_pcm_t *pcm;
	int active;
	char *name;
	int (*open)(snd_pcm_meter_scope_t *scope);
	void (*start)(snd_pcm_meter_scope_t *scope);
	void (*stop)(snd_pcm_meter_scope_t *scope);
	void (*update)(snd_pcm_meter_scope_t *scope);
	void (*reset)(snd_pcm_meter_scope_t *scope);
	void (*close)(snd_pcm_meter_scope_t *scope);
	void *private_data;
	struct list_head list;
};

typedef struct _snd_pcm_meter {
	snd_pcm_t *slave;
	int close_slave;
	snd_pcm_uframes_t rptr;
	snd_pcm_uframes_t buf_size;
	snd_pcm_channel_area_t *buf_areas;
	snd_pcm_uframes_t now;
	unsigned char *buf;
	struct list_head scopes;
	int closed;
	int running;
	atomic_t reset;
	pthread_t thread;
	pthread_mutex_t update_mutex;
	pthread_mutex_t running_mutex;
	pthread_cond_t running_cond;
	int16_t *buf16;
	snd_pcm_channel_area_t *buf16_areas;
	struct timespec delay;
} snd_pcm_meter_t;

