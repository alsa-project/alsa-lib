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
  
typedef struct {
	snd_pcm_t *slave;
	int close_slave;
	snd_pcm_xfer_areas_func_t read;
	snd_pcm_xfer_areas_func_t write;
	ssize_t (*client_frames)(snd_pcm_t *pcm, ssize_t frames);
	int (*init)(snd_pcm_t *pcm);
	int shmid;
	size_t appl_ptr, hw_ptr;
} snd_pcm_plugin_t;	

int snd_pcm_plugin_close(snd_pcm_t *pcm);
int snd_pcm_plugin_nonblock(snd_pcm_t *pcm, int nonblock);
int snd_pcm_plugin_async(snd_pcm_t *pcm, int sig, pid_t pid);
int snd_pcm_plugin_info(snd_pcm_t *pcm, snd_pcm_info_t * info);
int snd_pcm_plugin_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int snd_pcm_plugin_dig_info(snd_pcm_t *pcm, snd_pcm_dig_info_t *info);
int snd_pcm_plugin_dig_params(snd_pcm_t *pcm, snd_pcm_dig_params_t *params);
int snd_pcm_plugin_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info);
int snd_pcm_plugin_status(snd_pcm_t *pcm, snd_pcm_status_t * status);
int snd_pcm_plugin_state(snd_pcm_t *pcm);
int snd_pcm_plugin_delay(snd_pcm_t *pcm, ssize_t *delayp);
int snd_pcm_plugin_prepare(snd_pcm_t *pcm);
int snd_pcm_plugin_start(snd_pcm_t *pcm);
int snd_pcm_plugin_drop(snd_pcm_t *pcm);
int snd_pcm_plugin_drain(snd_pcm_t *pcm);
int snd_pcm_plugin_pause(snd_pcm_t *pcm, int enable);
ssize_t snd_pcm_plugin_rewind(snd_pcm_t *pcm, size_t frames);
ssize_t snd_pcm_plugin_writei(snd_pcm_t *pcm, const void *buffer, size_t size);
ssize_t snd_pcm_plugin_writen(snd_pcm_t *pcm, void **bufs, size_t size);
ssize_t snd_pcm_plugin_readi(snd_pcm_t *pcm, void *buffer, size_t size);
ssize_t snd_pcm_plugin_readn(snd_pcm_t *pcm, void **bufs, size_t size);
ssize_t snd_pcm_plugin_mmap_forward(snd_pcm_t *pcm, size_t size);
ssize_t snd_pcm_plugin_avail_update(snd_pcm_t *pcm);
int snd_pcm_plugin_mmap_status(snd_pcm_t *pcm);
int snd_pcm_plugin_mmap_control(snd_pcm_t *pcm);
int snd_pcm_plugin_mmap(snd_pcm_t *pcm);
int snd_pcm_plugin_munmap_status(snd_pcm_t *pcm);
int snd_pcm_plugin_munmap_control(snd_pcm_t *pcm);
int snd_pcm_plugin_munmap(snd_pcm_t *pcm);
int snd_pcm_plugin_poll_descriptor(snd_pcm_t *pcm);
int get_index(int src_format, int dst_format);
int put_index(int src_format, int dst_format);
int conv_index(int src_format, int dst_format);

#define SND_PCM_FMTBIT_LINEAR (SND_PCM_FMTBIT_S8     |SND_PCM_FMTBIT_U8 | \
				SND_PCM_FMTBIT_S16_LE|SND_PCM_FMTBIT_S16_BE | \
				SND_PCM_FMTBIT_U16_LE|SND_PCM_FMTBIT_U16_BE | \
				SND_PCM_FMTBIT_S24_LE|SND_PCM_FMTBIT_S24_BE | \
				SND_PCM_FMTBIT_U24_LE|SND_PCM_FMTBIT_U24_BE | \
				SND_PCM_FMTBIT_S32_LE|SND_PCM_FMTBIT_S32_BE | \
				SND_PCM_FMTBIT_U32_LE|SND_PCM_FMTBIT_U32_BE)

extern snd_pcm_fast_ops_t snd_pcm_plugin_fast_ops;

#define RATE_MIN 4000
#define RATE_MAX 192000

#define ROUTE_PLUGIN_FLOAT 1
#define ROUTE_PLUGIN_RESOLUTION 16

#if ROUTE_PLUGIN_FLOAT
typedef float ttable_entry_t;
#define HALF 0.5
#define FULL 1.0
#else
typedef int ttable_entry_t;
#define HALF (ROUTE_PLUGIN_RESOLUTION / 2)
#define FULL ROUTE_PLUGIN_RESOLUTION
#endif

int snd_pcm_copy_open(snd_pcm_t **pcmp, char *name, snd_pcm_t *slave, int close_slave);
int snd_pcm_linear_open(snd_pcm_t **pcmp, char *name, int sformat, snd_pcm_t *slave, int close_slave);
int snd_pcm_mulaw_open(snd_pcm_t **pcmp, char *name, int sformat, snd_pcm_t *slave, int close_slave);
int snd_pcm_alaw_open(snd_pcm_t **pcmp, char *name, int sformat, snd_pcm_t *slave, int close_slave);
int snd_pcm_adpcm_open(snd_pcm_t **pcmp, char *name, int sformat, snd_pcm_t *slave, int close_slave);
int snd_pcm_route_load_ttable(snd_config_t *tt, ttable_entry_t *ttable,
			      unsigned int tt_csize, unsigned int tt_ssize,
			      unsigned int *tt_cused, unsigned int *tt_sused,
			      int schannels);
int snd_pcm_route_open(snd_pcm_t **pcmp, char *name,
		       int sformat, unsigned int schannels,
		       ttable_entry_t *ttable,
		       unsigned int tt_ssize,
		       unsigned int tt_cused, unsigned int tt_sused,
		       snd_pcm_t *slave, int close_slave);
int snd_pcm_rate_open(snd_pcm_t **pcmp, char *name, int sformat, int srate, snd_pcm_t *slave, int close_slave);

