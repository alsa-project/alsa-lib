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
  

struct snd_pcm_ops {
	int (*channel_close)(snd_pcm_t *pcm, int channel);
	int (*channel_nonblock)(snd_pcm_t *pcm, int channel, int nonblock);
	int (*info)(snd_pcm_t *pcm, snd_pcm_info_t *info);
	int (*channel_info)(snd_pcm_t *pcm, snd_pcm_channel_info_t *info);
	int (*channel_params)(snd_pcm_t *pcm, snd_pcm_channel_params_t *params);
	int (*channel_setup)(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup);
	int (*voice_setup)(snd_pcm_t *pcm, int channel, snd_pcm_voice_setup_t *setup);
	int (*channel_status)(snd_pcm_t *pcm, snd_pcm_channel_status_t *status);
	int (*channel_prepare)(snd_pcm_t *pcm, int channel);
	int (*channel_update)(snd_pcm_t *pcm, int channel);
	int (*channel_go)(snd_pcm_t *pcm, int channel);
	int (*sync_go)(snd_pcm_t *pcm, snd_pcm_sync_t *sync);
	int (*channel_drain)(snd_pcm_t *pcm, int channel);
	int (*channel_flush)(snd_pcm_t *pcm, int channel);
	int (*channel_pause)(snd_pcm_t *pcm, int channel, int enable);
	ssize_t (*write)(snd_pcm_t *pcm, const void *buffer, size_t size);
	ssize_t (*writev)(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count);
	ssize_t (*read)(snd_pcm_t *pcm, void *buffer, size_t size);
	ssize_t (*readv)(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count);
	int (*mmap_control)(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t **control, size_t csize);
	int (*mmap_data)(snd_pcm_t *pcm, int channel, void **buffer, size_t bsize);
	int (*munmap_control)(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t *control, size_t csize);
	int (*munmap_data)(snd_pcm_t *pcm, int channel, void *buffer, size_t bsize);
	int (*file_descriptor)(snd_pcm_t* pcm, int channel);
	int (*voices_mask)(snd_pcm_t *pcm, int channel, bitset_t *client_vmask);
};


struct snd_pcm_plug_chan {
	snd_pcm_plugin_t *first;
	snd_pcm_plugin_t *last;
	void *alloc_ptr[2];
	size_t alloc_size[2];
	int alloc_lock[2];
};

typedef struct snd_pcm_plug {
	int close_slave;
	snd_pcm_t *slave;
	struct snd_pcm_plug_chan chan[2];
} snd_pcm_plug_t;

struct snd_pcm_hw_chan {
	int fd;
};

typedef struct snd_pcm_hw {
	int card;
	int device;
	int ver;
	struct snd_pcm_hw_chan chan[2];
} snd_pcm_hw_t;

struct snd_pcm_chan {
	int open;
	int mode;
	int valid_setup;
	snd_pcm_channel_setup_t setup;
	int valid_voices_setup;
	snd_pcm_voice_setup_t *voices_setup;
	snd_pcm_mmap_control_t *mmap_control;
	size_t mmap_control_size;
	int mmap_control_emulation;
	char *mmap_data;
	size_t mmap_data_size;
	int mmap_data_emulation;
	pthread_t mmap_thread;
	int mmap_thread_stop;
	pthread_mutex_t mutex;
	pthread_cond_t status_cond;
	pthread_cond_t ready_cond;
};

struct snd_pcm {
	snd_pcm_type_t type;
	int mode;
	struct snd_pcm_ops *ops;
	struct snd_pcm_chan chan[2];
	int private[0];
};

void snd_pcm_mmap_status_change(snd_pcm_t *pcm, int channel, int newstatus);

int snd_pcm_abstract_open(snd_pcm_t **handle, int mode, snd_pcm_type_t type, size_t extra);


unsigned int snd_pcm_plug_formats(unsigned int formats);
int snd_pcm_plug_slave_params(snd_pcm_channel_params_t *params,
			      snd_pcm_channel_info_t *slave_info,
			      snd_pcm_channel_params_t *slave_params);
int snd_pcm_plug_format(snd_pcm_plugin_handle_t *pcm,
			snd_pcm_channel_params_t *params,
			snd_pcm_channel_params_t *slave_params);

ssize_t snd_pcm_plug_write_transfer(snd_pcm_plugin_handle_t *handle, snd_pcm_plugin_voice_t *src_voices, size_t size);
ssize_t snd_pcm_plug_read_transfer(snd_pcm_plugin_handle_t *handle, snd_pcm_plugin_voice_t *dst_voices_final, size_t size);
ssize_t snd_pcm_plug_client_voices_iovec(snd_pcm_plugin_handle_t *handle, int channel,
					 const struct iovec *vector, unsigned long count,
					 snd_pcm_plugin_voice_t **voices);
ssize_t snd_pcm_plug_client_voices_buf(snd_pcm_plugin_handle_t *handle, int channel,
				       char *buf, size_t count,
				       snd_pcm_plugin_voice_t **voices);

int snd_pcm_plug_playback_voices_mask(snd_pcm_plugin_handle_t *handle,
				      bitset_t *client_vmask);
int snd_pcm_plug_capture_voices_mask(snd_pcm_plugin_handle_t *handle,
				     bitset_t *client_vmask);
int snd_pcm_plugin_client_voices(snd_pcm_plugin_t *plugin,
                                 size_t samples,
                                 snd_pcm_plugin_voice_t **voices);

void *snd_pcm_plug_buf_alloc(snd_pcm_t *pcm, int channel, size_t size);
void snd_pcm_plug_buf_unlock(snd_pcm_t *pcm, int channel, void *ptr);

#define ROUTE_PLUGIN_RESOLUTION 16

int getput_index(int format);
int copy_index(int format);
int conv_index(int src_format, int dst_format);

void snd_pcm_plugin_silence_voice(snd_pcm_plugin_t *plugin,
				  const snd_pcm_plugin_voice_t *dst_voice,
				  size_t samples);

#ifdef PLUGIN_DEBUG
#define pdprintf( args... ) fprintf(stderr, "plugin: " ##args)
#else
#define pdprintf( args... ) { ; }
#endif
