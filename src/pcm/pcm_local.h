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

#include <assert.h>
#include "asoundlib.h"

struct snd_pcm_ops {
	int (*close)(void *private);
	int (*info)(void *private, snd_pcm_info_t *info);
	int (*params_info)(void *private, snd_pcm_params_info_t *info);
	int (*params)(void *private, snd_pcm_params_t *params);
	int (*setup)(void *private, snd_pcm_setup_t *setup);
	void (*dump)(void *private, FILE *fp);
};

struct snd_pcm_fast_ops {
	int (*nonblock)(void *private, int nonblock);
	int (*status)(void *private, snd_pcm_status_t *status);
	int (*channel_setup)(void *private, snd_pcm_channel_setup_t *setup);
	int (*prepare)(void *private);
	int (*go)(void *private);
	int (*drain)(void *private);
	int (*flush)(void *private);
	int (*pause)(void *private, int enable);
	int (*state)(void *private);
	ssize_t (*frame_io)(void *private, int update);
	ssize_t (*frame_data)(void *private, off_t offset);
	ssize_t (*write)(void *private, snd_timestamp_t *tstamp, const void *buffer, size_t size);
	ssize_t (*writev)(void *private, snd_timestamp_t *tstamp, const struct iovec *vector, unsigned long count);
	ssize_t (*read)(void *private, snd_timestamp_t *tstamp, void *buffer, size_t size);
	ssize_t (*readv)(void *private, snd_timestamp_t *tstamp, const struct iovec *vector, unsigned long count);
	int (*file_descriptor)(void *private);
	int (*channels_mask)(void *private, bitset_t *client_vmask);
	int (*mmap_status)(void *private, snd_pcm_mmap_status_t **status);
	int (*mmap_control)(void *private, snd_pcm_mmap_control_t **control);
	int (*mmap_data)(void *private, void **buffer, size_t bsize);
	int (*munmap_status)(void *private, snd_pcm_mmap_status_t *status);
	int (*munmap_control)(void *private, snd_pcm_mmap_control_t *control);
	int (*munmap_data)(void *private, void *buffer, size_t bsize);
};

struct snd_pcm {
	snd_pcm_type_t type;
	int stream;
	int mode;
	int valid_setup;
	snd_pcm_setup_t setup;
	snd_pcm_channel_area_t *channels;
	size_t bits_per_sample;
	size_t bits_per_frame;
	snd_pcm_mmap_status_t *mmap_status;
	snd_pcm_mmap_control_t *mmap_control;
	char *mmap_data;
	enum { _INTERLEAVED, _NONINTERLEAVED, _COMPLEX } mmap_type;
	struct snd_pcm_ops *ops;
	void *op_arg;
	struct snd_pcm_fast_ops *fast_ops;
	void *fast_op_arg;
	void *private;
};

#undef snd_pcm_plug_t
typedef struct snd_pcm_plug {
	int close_slave;
	snd_pcm_t *handle;
	snd_pcm_t *slave;
	snd_pcm_plugin_t *first;
	snd_pcm_plugin_t *last;
	size_t frames_alloc;
} snd_pcm_plug_t;

unsigned int snd_pcm_plug_formats(unsigned int slave_formats);
int snd_pcm_plug_slave_fmt(int format, snd_pcm_params_info_t *slave_info);
int snd_pcm_plug_slave_rate(unsigned int rate, snd_pcm_params_info_t *slave_info);
int snd_pcm_plug_slave_format(snd_pcm_format_t *format,
			      snd_pcm_info_t *slave_info,
			      snd_pcm_params_info_t *slave_params_info,
			      snd_pcm_format_t *slave_format);
int snd_pcm_plug_format_plugins(snd_pcm_plug_t *plug,
				snd_pcm_format_t *format,
				snd_pcm_format_t *slave_format);

ssize_t snd_pcm_plug_write_transfer(snd_pcm_plug_t *plug, snd_pcm_plugin_channel_t *src_channels, size_t size);
ssize_t snd_pcm_plug_read_transfer(snd_pcm_plug_t *plug, snd_pcm_plugin_channel_t *dst_channels_final, size_t size);
ssize_t snd_pcm_plug_client_channels_iovec(snd_pcm_plug_t *plug,
					   const struct iovec *vector, unsigned long count,
					   snd_pcm_plugin_channel_t **channels);
ssize_t snd_pcm_plug_client_channels_buf(snd_pcm_plug_t *plug,
					 char *buf, size_t count,
					 snd_pcm_plugin_channel_t **channels);

int snd_pcm_plug_playback_channels_mask(snd_pcm_plug_t *plug,
					bitset_t *client_vmask);
int snd_pcm_plug_capture_channels_mask(snd_pcm_plug_t *plug,
				       bitset_t *client_vmask);
ssize_t snd_pcm_plugin_client_channels(snd_pcm_plugin_t *plugin,
				       size_t frames,
				       snd_pcm_plugin_channel_t **channels);

void *snd_pcm_plug_buf_alloc(snd_pcm_plug_t *plug, size_t size);
void snd_pcm_plug_buf_unlock(snd_pcm_plug_t *pcm, void *ptr);

#define ROUTE_PLUGIN_RESOLUTION 16

int getput_index(int format);
int conv_index(int src_format, int dst_format);

#ifdef PLUGIN_DEBUG
#define pdprintf( args... ) fprintf(stderr, "plugin: " ##args)
#else
#define pdprintf( args... ) { ; }
#endif

static inline size_t snd_pcm_mmap_playback_frames_avail(snd_pcm_t *str)
{
	ssize_t frames_avail;
	frames_avail = str->mmap_status->frame_io + str->setup.buffer_size - str->mmap_control->frame_data;
	if (frames_avail < 0)
		frames_avail += str->setup.frame_boundary;
	return frames_avail;
}

static inline size_t snd_pcm_mmap_capture_frames_avail(snd_pcm_t *str)
{
	ssize_t frames_avail;
	frames_avail = str->mmap_status->frame_io - str->mmap_control->frame_data;
	if (frames_avail < 0)
		frames_avail += str->setup.frame_boundary;
	return frames_avail;
}

#define snd_pcm_plug_stream(plug) ((plug)->handle->stream)
