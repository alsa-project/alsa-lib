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

#include "asoundlib.h"
  
struct snd_pcm {
	int card;
	int device;
	int fd;
	int mode;
	int setup_is_valid[2];
	snd_pcm_channel_setup_t setup[2];
	snd_pcm_mmap_control_t *mmap_caddr[2];
	char *mmap_daddr[2];
	long mmap_size[2];
	snd_pcm_plugin_t *plugin_first[2];
	snd_pcm_plugin_t *plugin_last[2];
	void *plugin_alloc_ptr[4];
	long plugin_alloc_size[4];
	int plugin_alloc_lock[4];
	void *plugin_alloc_xptr[2];
	long plugin_alloc_xsize[2];
};

unsigned int snd_pcm_plugin_formats(unsigned int formats);
int snd_pcm_plugin_hwparams(snd_pcm_channel_params_t *params,
			    snd_pcm_channel_info_t *hwinfo,
			    snd_pcm_channel_params_t *hwparams);
int snd_pcm_plugin_format(snd_pcm_t *pcm, 
			  snd_pcm_channel_params_t *params, 
			  snd_pcm_channel_params_t *hwparams, 
			  snd_pcm_channel_info_t *hwinfo);

#if 0
#define PLUGIN_DEBUG
#define pdprintf( args... ) printf( "plugin: " ##args)
#else
#define pdprintf( args... ) { ; }
#endif
