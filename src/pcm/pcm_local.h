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
	int (*stream_close)(snd_pcm_t *pcm, int stream);
	int (*stream_nonblock)(snd_pcm_t *pcm, int stream, int nonblock);
	int (*info)(snd_pcm_t *pcm, snd_pcm_info_t *info);
	int (*stream_info)(snd_pcm_t *pcm, snd_pcm_stream_info_t *info);
	int (*stream_params)(snd_pcm_t *pcm, snd_pcm_stream_params_t *params);
	int (*stream_setup)(snd_pcm_t *pcm, snd_pcm_stream_setup_t *setup);
	int (*channel_setup)(snd_pcm_t *pcm, int stream, snd_pcm_channel_setup_t *setup);
	int (*stream_status)(snd_pcm_t *pcm, snd_pcm_stream_status_t *status);
	int (*stream_prepare)(snd_pcm_t *pcm, int stream);
	int (*stream_update)(snd_pcm_t *pcm, int stream);
	int (*stream_go)(snd_pcm_t *pcm, int stream);
	int (*sync_go)(snd_pcm_t *pcm, snd_pcm_sync_t *sync);
	int (*stream_drain)(snd_pcm_t *pcm, int stream);
	int (*stream_flush)(snd_pcm_t *pcm, int stream);
	int (*stream_pause)(snd_pcm_t *pcm, int stream, int enable);
	ssize_t (*stream_seek)(snd_pcm_t *pcm, int stream, off_t offset);
	ssize_t (*write)(snd_pcm_t *pcm, const void *buffer, size_t size);
	ssize_t (*writev)(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count);
	ssize_t (*read)(snd_pcm_t *pcm, void *buffer, size_t size);
	ssize_t (*readv)(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count);
	int (*mmap_control)(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t **control, size_t csize);
	int (*mmap_data)(snd_pcm_t *pcm, int stream, void **buffer, size_t bsize);
	int (*munmap_control)(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t *control, size_t csize);
	int (*munmap_data)(snd_pcm_t *pcm, int stream, void *buffer, size_t bsize);
	int (*file_descriptor)(snd_pcm_t* pcm, int stream);
	int (*channels_mask)(snd_pcm_t *pcm, int stream, bitset_t *client_vmask);
};


struct snd_pcm_plug_stream {
	snd_pcm_plugin_t *first;
	snd_pcm_plugin_t *last;
	void *alloc_ptr[2];
	size_t alloc_size[2];
	int alloc_lock[2];
};

typedef struct snd_pcm_plug {
	int close_slave;
	snd_pcm_t *slave;
	struct snd_pcm_plug_stream stream[2];
} snd_pcm_plug_t;

struct snd_pcm_hw_stream {
	int fd;
};

typedef struct snd_pcm_hw {
	int card;
	int device;
	int ver;
	struct snd_pcm_hw_stream stream[2];
} snd_pcm_hw_t;

struct snd_pcm_stream {
	int open;
	int mode;
	int valid_setup;
	snd_pcm_stream_setup_t setup;
	snd_pcm_channel_area_t *channels;
	size_t sample_width;
	size_t bits_per_frame;
	size_t frames_per_frag;
	snd_pcm_mmap_control_t *mmap_control;
	size_t mmap_control_size;
	char *mmap_data;
	size_t mmap_data_size;
};

struct snd_pcm {
	snd_pcm_type_t type;
	int mode;
	struct snd_pcm_ops *ops;
	struct snd_pcm_stream stream[2];
	int private[0];
};

int snd_pcm_abstract_open(snd_pcm_t **handle, int mode, snd_pcm_type_t type, size_t extra);


unsigned int snd_pcm_plug_formats(unsigned int formats);
int snd_pcm_plug_slave_params(snd_pcm_stream_params_t *params,
			      snd_pcm_stream_info_t *slave_info,
			      snd_pcm_stream_params_t *slave_params);
int snd_pcm_plug_format(snd_pcm_plugin_handle_t *pcm,
			snd_pcm_stream_params_t *params,
			snd_pcm_stream_params_t *slave_params);

ssize_t snd_pcm_plug_write_transfer(snd_pcm_plugin_handle_t *handle, snd_pcm_plugin_channel_t *src_channels, size_t size);
ssize_t snd_pcm_plug_read_transfer(snd_pcm_plugin_handle_t *handle, snd_pcm_plugin_channel_t *dst_channels_final, size_t size);
ssize_t snd_pcm_plug_client_channels_iovec(snd_pcm_plugin_handle_t *handle, int stream,
					 const struct iovec *vector, unsigned long count,
					 snd_pcm_plugin_channel_t **channels);
ssize_t snd_pcm_plug_client_channels_buf(snd_pcm_plugin_handle_t *handle, int stream,
				       char *buf, size_t count,
				       snd_pcm_plugin_channel_t **channels);

int snd_pcm_plug_playback_channels_mask(snd_pcm_plugin_handle_t *handle,
				      bitset_t *client_vmask);
int snd_pcm_plug_capture_channels_mask(snd_pcm_plugin_handle_t *handle,
				     bitset_t *client_vmask);
int snd_pcm_plugin_client_channels(snd_pcm_plugin_t *plugin,
                                 size_t frames,
                                 snd_pcm_plugin_channel_t **channels);

void *snd_pcm_plug_buf_alloc(snd_pcm_t *pcm, int stream, size_t size);
void snd_pcm_plug_buf_unlock(snd_pcm_t *pcm, int stream, void *ptr);

#define ROUTE_PLUGIN_RESOLUTION 16

int getput_index(int format);
int conv_index(int src_format, int dst_format);

#ifdef PLUGIN_DEBUG
#define pdprintf( args... ) fprintf(stderr, "plugin: " ##args)
#else
#define pdprintf( args... ) { ; }
#endif
