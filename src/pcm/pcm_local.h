/*
 *  PCM Interface - local header file
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
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

#include <pthread.h>
#include "asoundlib.h"
  
struct snd_pcm_plug {
	snd_pcm_plugin_t *first;
	snd_pcm_plugin_t *last;
	void *alloc_ptr[2];
	long alloc_size[2];
	int alloc_lock[2];
	snd_pcm_mmap_control_t *mmap_control;
	char *mmap_data;
	long mmap_size;
	pthread_t thread;
	int thread_stop;
	int setup_is_valid;
	snd_pcm_channel_setup_t setup;
	int hwstatus;
};

struct snd_pcm_chan {
	int fd;
	int setup_is_valid;
	snd_pcm_channel_setup_t setup;
	snd_pcm_mmap_control_t *mmap_control;
	char *mmap_data;
	long mmap_size;
	struct snd_pcm_plug plug;
};

struct snd_pcm {
	int card;
	int device;
	int mode;
	int ver;
	struct snd_pcm_chan chan[2];
};

unsigned int snd_pcm_plugin_formats(unsigned int formats);
int snd_pcm_plugin_hwparams(snd_pcm_channel_params_t *params,
			    snd_pcm_channel_info_t *hwinfo,
			    snd_pcm_channel_params_t *hwparams);
int snd_pcm_plugin_format(snd_pcm_t *pcm,
			  snd_pcm_channel_params_t *params,
			  snd_pcm_channel_params_t *hwparams,
			  snd_pcm_channel_info_t *hwinfo);

#define ROUTE_PLUGIN_RESOLUTION 16

int getput_index(int format);
int copy_index(int src_format, int dst_format);

void zero_voice(snd_pcm_plugin_t *plugin,
		const snd_pcm_plugin_voice_t *dst_voice,
		size_t samples);

#ifdef PLUGIN_DEBUG
#define pdprintf( args... ) printf( "plugin: " ##args)
#else
#define pdprintf( args... ) { ; }
#endif
