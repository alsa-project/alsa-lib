/*
 *  PCM - Common plugin code
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
  
#include "atomic.h"

typedef snd_pcm_uframes_t (*snd_pcm_slave_xfer_areas_func_t)
     (snd_pcm_t *pcm, 
      const snd_pcm_channel_area_t *areas,
      snd_pcm_uframes_t offset, 
      snd_pcm_uframes_t size,
      const snd_pcm_channel_area_t *slave_areas,
      snd_pcm_uframes_t slave_offset, 
      snd_pcm_uframes_t *slave_sizep);

typedef struct {
	snd_pcm_t *slave;
	int close_slave;
	snd_pcm_slave_xfer_areas_func_t read;
	snd_pcm_slave_xfer_areas_func_t write;
	snd_pcm_sframes_t (*client_frames)(snd_pcm_t *pcm, snd_pcm_sframes_t frames);
	snd_pcm_sframes_t (*slave_frames)(snd_pcm_t *pcm, snd_pcm_sframes_t frames);
	int (*init)(snd_pcm_t *pcm);
	int shmid;
	snd_pcm_uframes_t appl_ptr, hw_ptr;
	snd_atomic_write_t watom;
} snd_pcm_plugin_t;	

int snd_pcm_plugin_close(snd_pcm_t *pcm);
int snd_pcm_plugin_card(snd_pcm_t *pcm);
int snd_pcm_plugin_nonblock(snd_pcm_t *pcm, int nonblock);
int snd_pcm_plugin_async(snd_pcm_t *pcm, int sig, pid_t pid);
int snd_pcm_plugin_info(snd_pcm_t *pcm, snd_pcm_info_t * info);
int snd_pcm_plugin_hw_free(snd_pcm_t *pcm);
int snd_pcm_plugin_sw_refine(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int snd_pcm_plugin_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int snd_pcm_plugin_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info);
int snd_pcm_plugin_status(snd_pcm_t *pcm, snd_pcm_status_t * status);
snd_pcm_state_t snd_pcm_plugin_state(snd_pcm_t *pcm);
int snd_pcm_plugin_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp);
int snd_pcm_plugin_prepare(snd_pcm_t *pcm);
int snd_pcm_plugin_start(snd_pcm_t *pcm);
int snd_pcm_plugin_drop(snd_pcm_t *pcm);
int snd_pcm_plugin_drain(snd_pcm_t *pcm);
int snd_pcm_plugin_pause(snd_pcm_t *pcm, int enable);
snd_pcm_sframes_t snd_pcm_plugin_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
snd_pcm_sframes_t snd_pcm_plugin_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_plugin_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_plugin_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_plugin_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_plugin_mmap_commit(snd_pcm_t *pcm, snd_pcm_uframes_t offset, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_plugin_avail_update(snd_pcm_t *pcm);
int snd_pcm_plugin_mmap_status(snd_pcm_t *pcm);
int snd_pcm_plugin_mmap_control(snd_pcm_t *pcm);
int snd_pcm_plugin_mmap(snd_pcm_t *pcm);
int snd_pcm_plugin_munmap_status(snd_pcm_t *pcm);
int snd_pcm_plugin_munmap_control(snd_pcm_t *pcm);
int snd_pcm_plugin_munmap(snd_pcm_t *pcm);
int snd_pcm_plugin_poll_descriptors(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int space);
int snd_pcm_plugin_hw_params_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_plugin_hw_refine_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);

extern snd_pcm_fast_ops_t snd_pcm_plugin_fast_ops;

#define RATE_MIN 4000
#define RATE_MAX 192000

int snd_pcm_linear_get_index(snd_pcm_format_t src_format, snd_pcm_format_t dst_format);
int snd_pcm_linear_put_index(snd_pcm_format_t src_format, snd_pcm_format_t dst_format);
int snd_pcm_linear_convert_index(snd_pcm_format_t src_format, snd_pcm_format_t dst_format);
int snd_pcm_copy_open(snd_pcm_t **pcmp, const char *name, snd_pcm_t *slave, int close_slave);
int snd_pcm_linear_open(snd_pcm_t **pcmp, const char *name, snd_pcm_format_t sformat, snd_pcm_t *slave, int close_slave);
int snd_pcm_mulaw_open(snd_pcm_t **pcmp, const char *name, snd_pcm_format_t sformat, snd_pcm_t *slave, int close_slave);
int snd_pcm_alaw_open(snd_pcm_t **pcmp, const char *name, snd_pcm_format_t sformat, snd_pcm_t *slave, int close_slave);
int snd_pcm_adpcm_open(snd_pcm_t **pcmp, const char *name, snd_pcm_format_t sformat, snd_pcm_t *slave, int close_slave);
int snd_pcm_route_load_ttable(snd_config_t *tt, snd_pcm_route_ttable_entry_t *ttable,
			      unsigned int tt_csize, unsigned int tt_ssize,
			      unsigned int *tt_cused, unsigned int *tt_sused,
			      int schannels);
int snd_pcm_route_open(snd_pcm_t **pcmp, const char *name,
		       snd_pcm_format_t sformat, int schannels,
		       snd_pcm_route_ttable_entry_t *ttable,
		       unsigned int tt_ssize,
		       unsigned int tt_cused, unsigned int tt_sused,
		       snd_pcm_t *slave, int close_slave);
int snd_pcm_rate_open(snd_pcm_t **pcmp, const char *name, snd_pcm_format_t sformat, unsigned int srate, snd_pcm_t *slave, int close_slave);


void snd_pcm_linear_convert(const snd_pcm_channel_area_t *dst_areas, snd_pcm_uframes_t dst_offset,
			    const snd_pcm_channel_area_t *src_areas, snd_pcm_uframes_t src_offset,
			    unsigned int channels, snd_pcm_uframes_t frames,
			    unsigned int convidx);
void snd_pcm_alaw_decode(const snd_pcm_channel_area_t *dst_areas,
			 snd_pcm_uframes_t dst_offset,
			 const snd_pcm_channel_area_t *src_areas,
			 snd_pcm_uframes_t src_offset,
			 unsigned int channels, snd_pcm_uframes_t frames,
			 unsigned int putidx);
void snd_pcm_alaw_encode(const snd_pcm_channel_area_t *dst_areas,
			 snd_pcm_uframes_t dst_offset,
			 const snd_pcm_channel_area_t *src_areas,
			 snd_pcm_uframes_t src_offset,
			 unsigned int channels, snd_pcm_uframes_t frames,
			 unsigned int getidx);
void snd_pcm_mulaw_decode(const snd_pcm_channel_area_t *dst_areas,
			  snd_pcm_uframes_t dst_offset,
			  const snd_pcm_channel_area_t *src_areas,
			  snd_pcm_uframes_t src_offset,
			  unsigned int channels, snd_pcm_uframes_t frames,
			  unsigned int putidx);
void snd_pcm_mulaw_encode(const snd_pcm_channel_area_t *dst_areas,
			  snd_pcm_uframes_t dst_offset,
			  const snd_pcm_channel_area_t *src_areas,
			  snd_pcm_uframes_t src_offset,
			  unsigned int channels, snd_pcm_uframes_t frames,
			  unsigned int getidx);

typedef struct _snd_pcm_adpcm_state {
	int pred_val;		/* Calculated predicted value */
	int step_idx;		/* Previous StepSize lookup index */
} snd_pcm_adpcm_state_t;

void snd_pcm_adpcm_decode(const snd_pcm_channel_area_t *dst_areas,
			  snd_pcm_uframes_t dst_offset,
			  const snd_pcm_channel_area_t *src_areas,
			  snd_pcm_uframes_t src_offset,
			  unsigned int channels, snd_pcm_uframes_t frames,
			  unsigned int putidx,
			  snd_pcm_adpcm_state_t *states);
void snd_pcm_adpcm_encode(const snd_pcm_channel_area_t *dst_areas,
			  snd_pcm_uframes_t dst_offset,
			  const snd_pcm_channel_area_t *src_areas,
			  snd_pcm_uframes_t src_offset,
			  unsigned int channels, snd_pcm_uframes_t frames,
			  unsigned int getidx,
			  snd_pcm_adpcm_state_t *states);
