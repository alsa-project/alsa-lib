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
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <errno.h>
#include "asoundlib.h"

struct snd_pcm_ops {
	int (*close)(snd_pcm_t *pcm);
	int (*nonblock)(snd_pcm_t *pcm, int nonblock);
	int (*info)(snd_pcm_t *pcm, snd_pcm_info_t *info);
	int (*params_info)(snd_pcm_t *pcm, snd_pcm_params_info_t *info);
	int (*params)(snd_pcm_t *pcm, snd_pcm_params_t *params);
	int (*setup)(snd_pcm_t *pcm, snd_pcm_setup_t *setup);
	int (*channel_info)(snd_pcm_t *pcm, snd_pcm_channel_info_t *info);
	int (*channel_params)(snd_pcm_t *pcm, snd_pcm_channel_params_t *params);
	int (*channel_setup)(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup);
	void (*dump)(snd_pcm_t *pcm, FILE *fp);
	int (*mmap_status)(snd_pcm_t *pcm);
	int (*mmap_control)(snd_pcm_t *pcm);
	int (*mmap_data)(snd_pcm_t *pcm);
	int (*munmap_status)(snd_pcm_t *pcm);
	int (*munmap_control)(snd_pcm_t *pcm);
	int (*munmap_data)(snd_pcm_t *pcm);
};

struct snd_pcm_fast_ops {
	int (*status)(snd_pcm_t *pcm, snd_pcm_status_t *status);
	int (*prepare)(snd_pcm_t *pcm);
	int (*start)(snd_pcm_t *pcm);
	int (*drop)(snd_pcm_t *pcm);
	int (*drain)(snd_pcm_t *pcm);
	int (*pause)(snd_pcm_t *pcm, int enable);
	int (*state)(snd_pcm_t *pcm);
	int (*delay)(snd_pcm_t *pcm, ssize_t *delayp);
	ssize_t (*rewind)(snd_pcm_t *pcm, size_t frames);
	ssize_t (*writei)(snd_pcm_t *pcm, const void *buffer, size_t size);
	ssize_t (*writen)(snd_pcm_t *pcm, void **bufs, size_t size);
	ssize_t (*readi)(snd_pcm_t *pcm, void *buffer, size_t size);
	ssize_t (*readn)(snd_pcm_t *pcm, void **bufs, size_t size);
	int (*poll_descriptor)(snd_pcm_t *pcm);
	int (*channels_mask)(snd_pcm_t *pcm, bitset_t *cmask);
	ssize_t (*avail_update)(snd_pcm_t *pcm);
	ssize_t (*mmap_forward)(snd_pcm_t *pcm, size_t size);
};

struct snd_pcm {
	snd_pcm_type_t type;
	int stream;
	int mode;
	int valid_setup;
	snd_pcm_setup_t setup;
	size_t bits_per_sample;
	size_t bits_per_frame;
	snd_pcm_mmap_status_t *mmap_status;
	snd_pcm_mmap_control_t *mmap_control;
	void *mmap_data;
	snd_pcm_channel_area_t *mmap_areas;
	struct snd_pcm_ops *ops;
	struct snd_pcm_fast_ops *fast_ops;
	snd_pcm_t *op_arg;
	snd_pcm_t *fast_op_arg;
	void *private;
};

int snd_pcm_init(snd_pcm_t *pcm);
void snd_pcm_areas_from_buf(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, void *buf);
void snd_pcm_areas_from_bufs(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, void **bufs);

int snd_pcm_mmap_status(snd_pcm_t *pcm, snd_pcm_mmap_status_t **status);
int snd_pcm_mmap_control(snd_pcm_t *pcm, snd_pcm_mmap_control_t **control);
int snd_pcm_mmap_data(snd_pcm_t *pcm, void **buffer);
int snd_pcm_munmap_status(snd_pcm_t *pcm);
int snd_pcm_munmap_control(snd_pcm_t *pcm);
int snd_pcm_munmap_data(snd_pcm_t *pcm);
int snd_pcm_mmap_ready(snd_pcm_t *pcm);
ssize_t snd_pcm_mmap_appl_ptr(snd_pcm_t *pcm, off_t offset);
void snd_pcm_mmap_appl_backward(snd_pcm_t *pcm, size_t frames);
void snd_pcm_mmap_appl_forward(snd_pcm_t *pcm, size_t frames);
void snd_pcm_mmap_hw_forward(snd_pcm_t *pcm, size_t frames);
size_t snd_pcm_mmap_hw_offset(snd_pcm_t *pcm);
size_t snd_pcm_mmap_avail(snd_pcm_t *pcm);
size_t snd_pcm_mmap_playback_xfer(snd_pcm_t *pcm, size_t frames);
size_t snd_pcm_mmap_capture_xfer(snd_pcm_t *pcm, size_t frames);

typedef ssize_t (*snd_pcm_xfer_areas_func_t)(snd_pcm_t *pcm, 
					     snd_pcm_channel_area_t *areas,
					     size_t offset, size_t size,
					     size_t *slave_sizep);

ssize_t snd_pcm_read_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas,
			   size_t offset, size_t size,
			   snd_pcm_xfer_areas_func_t func);
ssize_t snd_pcm_write_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas,
			    size_t offset, size_t size,
			    snd_pcm_xfer_areas_func_t func);
ssize_t snd_pcm_read_mmap(snd_pcm_t *pcm, size_t size);
ssize_t snd_pcm_write_mmap(snd_pcm_t *pcm, size_t size);

static inline size_t snd_pcm_mmap_playback_avail(snd_pcm_t *pcm)
{
	ssize_t avail;
	avail = pcm->mmap_status->hw_ptr + pcm->setup.buffer_size - pcm->mmap_control->appl_ptr;
	if (avail < 0)
		avail += pcm->setup.boundary;
	return avail;
}

static inline size_t snd_pcm_mmap_capture_avail(snd_pcm_t *pcm)
{
	ssize_t avail;
	avail = pcm->mmap_status->hw_ptr - pcm->mmap_control->appl_ptr;
	if (avail < 0)
		avail += pcm->setup.boundary;
	return avail;
}

static inline void *snd_pcm_channel_area_addr(snd_pcm_channel_area_t *area, size_t offset)
{
	size_t bitofs = area->first + area->step * offset;
	assert(bitofs % 8 == 0);
	return area->addr + bitofs / 8;
}

static inline size_t snd_pcm_channel_area_step(snd_pcm_channel_area_t *area)
{
	assert(area->step % 8 == 0);
	return area->step / 8;
}

static inline ssize_t _snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	return pcm->fast_ops->writei(pcm->fast_op_arg, buffer, size);
}

static inline ssize_t _snd_pcm_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	return pcm->fast_ops->writen(pcm->fast_op_arg, bufs, size);
}

static inline ssize_t _snd_pcm_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	return pcm->fast_ops->readi(pcm->fast_op_arg, buffer, size);
}

static inline ssize_t _snd_pcm_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	return pcm->fast_ops->readn(pcm->fast_op_arg, bufs, size);
}

