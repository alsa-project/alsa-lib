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
#include <limits.h>
#include <sys/uio.h>
#include <errno.h>
#include "asoundlib.h"

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define ERR(...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, 0, __VA_ARGS__)
#define SYSERR(...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, errno, __VA_ARGS__)
#else
#define ERR(args...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, 0, ##args)
#define SYSERR(args...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, errno, ##args)
#endif

typedef struct _snd_pcm_channel_info {
	unsigned int channel;
	void *addr;			/* base address of channel samples */
	unsigned int first;		/* offset to first sample in bits */
	unsigned int step;		/* samples distance in bits */
	enum { SND_PCM_AREA_SHM, SND_PCM_AREA_MMAP } type;
	union {
		struct {
			int shmid;
		} shm;
		struct {
			int fd;
			off_t offset;
		} mmap;
	} u;
	char reserved[64];
} snd_pcm_channel_info_t;

typedef struct {
	int (*close)(snd_pcm_t *pcm);
	int (*nonblock)(snd_pcm_t *pcm, int nonblock);
	int (*async)(snd_pcm_t *pcm, int sig, pid_t pid);
	int (*info)(snd_pcm_t *pcm, snd_pcm_info_t *info);
	int (*hw_info)(snd_pcm_t *pcm, snd_pcm_hw_info_t *info);
	int (*hw_params)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
	int (*sw_params)(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
	int (*dig_info)(snd_pcm_t *pcm, snd_pcm_dig_info_t *info);
	int (*dig_params)(snd_pcm_t *pcm, snd_pcm_dig_params_t *params);
	int (*channel_info)(snd_pcm_t *pcm, snd_pcm_channel_info_t *info);
	void (*dump)(snd_pcm_t *pcm, FILE *fp);
	int (*mmap)(snd_pcm_t *pcm);
	int (*munmap)(snd_pcm_t *pcm);
} snd_pcm_ops_t;

typedef struct {
	int (*status)(snd_pcm_t *pcm, snd_pcm_status_t *status);
	int (*prepare)(snd_pcm_t *pcm);
	int (*reset)(snd_pcm_t *pcm);
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
	ssize_t (*avail_update)(snd_pcm_t *pcm);
	ssize_t (*mmap_forward)(snd_pcm_t *pcm, size_t size);
	int (*set_avail_min)(snd_pcm_t *pcm, size_t frames);
} snd_pcm_fast_ops_t;

struct _snd_pcm {
	char *name;
	snd_pcm_type_t type;
	int stream;
	int mode;
	int poll_fd;
	int setup;
	unsigned int access;		/* access mode */
	unsigned int format;		/* SND_PCM_FORMAT_* */
	unsigned int subformat;		/* subformat */
	unsigned int channels;		/* channels */
	unsigned int rate;		/* rate in Hz */
	size_t fragment_size;		/* fragment size */
	unsigned int fragments;		/* fragments */
	unsigned int start_mode;	/* start mode */
	unsigned int ready_mode;	/* ready detection mode */
	unsigned int xrun_mode;		/* xrun detection mode */
	size_t avail_min;		/* min avail frames for wakeup */
	size_t xfer_min;		/* xfer min size */
	size_t xfer_align;		/* xfer size need to be a multiple */
	unsigned int time: 1;		/* timestamp switch */
	size_t boundary;		/* pointers wrap point */
	unsigned int info;		/* Info for returned setup */
	unsigned int msbits;		/* used most significant bits */
	unsigned int rate_num;		/* rate numerator */
	unsigned int rate_den;		/* rate denominator */
	size_t fifo_size;		/* chip FIFO size in frames */
	size_t buffer_size;
	size_t bits_per_sample;
	size_t bits_per_frame;
	size_t *appl_ptr;
	volatile size_t *hw_ptr;
	int mmap_rw;
	snd_pcm_channel_info_t *mmap_channels;
	snd_pcm_channel_area_t *running_areas;
	snd_pcm_channel_area_t *stopped_areas;
	void *stopped;
	snd_pcm_ops_t *ops;
	snd_pcm_fast_ops_t *fast_ops;
	snd_pcm_t *op_arg;
	snd_pcm_t *fast_op_arg;
	void *private;
};

int snd_pcm_hw_open(snd_pcm_t **pcm, char *name, int card, int device, int subdevice, int stream, int mode);
int snd_pcm_plug_open_hw(snd_pcm_t **pcm, char *name, int card, int device, int subdevice, int stream, int mode);
int snd_pcm_shm_open(snd_pcm_t **pcmp, char *name, char *socket, char *sname, int stream, int mode);
int snd_pcm_file_open(snd_pcm_t **pcmp, char *name, char *fname, int fd, char *fmt, snd_pcm_t *slave, int close_slave);
int snd_pcm_null_open(snd_pcm_t **pcmp, char *name, int stream, int mode);


void snd_pcm_areas_from_buf(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, void *buf);
void snd_pcm_areas_from_bufs(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, void **bufs);

int snd_pcm_mmap(snd_pcm_t *pcm);
int snd_pcm_munmap(snd_pcm_t *pcm);
int snd_pcm_mmap_ready(snd_pcm_t *pcm);
ssize_t snd_pcm_mmap_appl_ptr(snd_pcm_t *pcm, off_t offset);
void snd_pcm_mmap_appl_backward(snd_pcm_t *pcm, size_t frames);
void snd_pcm_mmap_appl_forward(snd_pcm_t *pcm, size_t frames);
void snd_pcm_mmap_hw_backward(snd_pcm_t *pcm, size_t frames);
void snd_pcm_mmap_hw_forward(snd_pcm_t *pcm, size_t frames);
size_t snd_pcm_mmap_hw_offset(snd_pcm_t *pcm);
size_t snd_pcm_mmap_playback_xfer(snd_pcm_t *pcm, size_t frames);
size_t snd_pcm_mmap_capture_xfer(snd_pcm_t *pcm, size_t frames);

typedef ssize_t (*snd_pcm_xfer_areas_func_t)(snd_pcm_t *pcm, 
					     const snd_pcm_channel_area_t *areas,
					     size_t offset, size_t size,
					     size_t *slave_sizep);

ssize_t snd_pcm_read_areas(snd_pcm_t *pcm, const snd_pcm_channel_area_t *areas,
			   size_t offset, size_t size,
			   snd_pcm_xfer_areas_func_t func);
ssize_t snd_pcm_write_areas(snd_pcm_t *pcm, const snd_pcm_channel_area_t *areas,
			    size_t offset, size_t size,
			    snd_pcm_xfer_areas_func_t func);
ssize_t snd_pcm_read_mmap(snd_pcm_t *pcm, size_t size);
ssize_t snd_pcm_write_mmap(snd_pcm_t *pcm, size_t size);
int snd_pcm_hw_info_complete(snd_pcm_hw_info_t *info);
void snd_pcm_hw_params_to_info(snd_pcm_hw_params_t *params, snd_pcm_hw_info_t *info);
int snd_pcm_hw_info_to_params(snd_pcm_t *pcm, snd_pcm_hw_info_t *info, snd_pcm_hw_params_t *params);
int snd_pcm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info);
int snd_pcm_channel_info_shm(snd_pcm_t *pcm, snd_pcm_channel_info_t *info, int shmid);
int snd_pcm_hw_info_par_nearest_next(const snd_pcm_hw_info_t *info,
				     unsigned int param,
				     unsigned int best, int value,
				     snd_pcm_t *pcm);

static inline size_t snd_pcm_mmap_playback_avail(snd_pcm_t *pcm)
{
	ssize_t avail;
	avail = *pcm->hw_ptr + pcm->buffer_size - *pcm->appl_ptr;
	if (avail < 0)
		avail += pcm->boundary;
	return avail;
}

static inline size_t snd_pcm_mmap_capture_avail(snd_pcm_t *pcm)
{
	ssize_t avail;
	avail = *pcm->hw_ptr - *pcm->appl_ptr;
	if (avail < 0)
		avail += pcm->boundary;
	return avail;
}

static inline size_t snd_pcm_mmap_avail(snd_pcm_t *pcm)
{
	ssize_t avail;
	avail = *pcm->hw_ptr - *pcm->appl_ptr;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		avail += pcm->buffer_size;
	if (avail < 0)
		avail += pcm->boundary;
	return avail;
}

static inline ssize_t snd_pcm_mmap_playback_hw_avail(snd_pcm_t *pcm)
{
	ssize_t avail;
	avail = *pcm->hw_ptr + pcm->buffer_size - *pcm->appl_ptr;
	if (avail < 0)
		avail += pcm->boundary;
	return pcm->buffer_size - avail;
}

static inline ssize_t snd_pcm_mmap_capture_hw_avail(snd_pcm_t *pcm)
{
	ssize_t avail;
	avail = *pcm->hw_ptr - *pcm->appl_ptr;
	if (avail < 0)
		avail += pcm->boundary;
	return pcm->buffer_size - avail;
}

static inline ssize_t snd_pcm_mmap_hw_avail(snd_pcm_t *pcm)
{
	ssize_t avail;
	avail = *pcm->hw_ptr - *pcm->appl_ptr;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		avail += pcm->buffer_size;
	if (avail < 0)
		avail += pcm->boundary;
	return pcm->buffer_size - avail;
}

#define snd_pcm_mmap_playback_delay snd_pcm_mmap_playback_hw_avail
#define snd_pcm_mmap_capture_delay snd_pcm_mmap_capture_avail

static inline ssize_t snd_pcm_mmap_delay(snd_pcm_t *pcm)
{
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_mmap_playback_delay(pcm);
	else
		return snd_pcm_mmap_capture_delay(pcm);
}

static inline void *snd_pcm_channel_area_addr(const snd_pcm_channel_area_t *area, size_t offset)
{
	size_t bitofs = area->first + area->step * offset;
	assert(bitofs % 8 == 0);
	return area->addr + bitofs / 8;
}

static inline size_t snd_pcm_channel_area_step(const snd_pcm_channel_area_t *area)
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

static inline int muldiv(int a, int b, int c, int *r)
{
	int64_t n = (int64_t)a * b;
	int64_t v = n / c;
	if (v > INT_MAX) {
		*r = 0;
		return INT_MAX;
	}
	if (v < INT_MIN) {
		*r = 0;
		return INT_MIN;
	}
	*r = n % c;
	return v;
}

static inline int muldiv_down(int a, int b, int c)
{
	int64_t v = (int64_t)a * b / c;
	if (v > INT_MAX) {
		return INT_MAX;
	}
	if (v < INT_MIN) {
		return INT_MIN;
	}
	return v;
}

static inline int muldiv_near(int a, int b, int c)
{
	int r;
	int n = muldiv(a, b, c, &r);
	if (r >= (c + 1) / 2)
		n++;
	return n;
}
