/*
 *  PCM Interface - local header file
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
 *                        Abramo Bagnara <abramo@alsa-project.org>
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
#include "local.h"

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
	int (*card)(snd_pcm_t *pcm);
	int (*nonblock)(snd_pcm_t *pcm, int nonblock);
	int (*async)(snd_pcm_t *pcm, int sig, pid_t pid);
	int (*info)(snd_pcm_t *pcm, snd_pcm_info_t *info);
	int (*hw_refine)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
	int (*hw_params)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
	int (*sw_params)(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
	int (*channel_info)(snd_pcm_t *pcm, snd_pcm_channel_info_t *info);
	void (*dump)(snd_pcm_t *pcm, snd_output_t *out);
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
	int (*delay)(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp);
	snd_pcm_sframes_t (*rewind)(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
	snd_pcm_sframes_t (*writei)(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
	snd_pcm_sframes_t (*writen)(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);
	snd_pcm_sframes_t (*readi)(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size);
	snd_pcm_sframes_t (*readn)(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);
	snd_pcm_sframes_t (*avail_update)(snd_pcm_t *pcm);
	snd_pcm_sframes_t (*mmap_forward)(snd_pcm_t *pcm, snd_pcm_uframes_t size);
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
	snd_pcm_uframes_t period_size;
	unsigned int period_time;	/* period duration */
	unsigned int tick_time;
	snd_pcm_start_t start_mode;	/* start mode */
	snd_pcm_xrun_t xrun_mode;	/* xrun detection mode */
	snd_pcm_tstamp_t tstamp_mode;	/* timestamp mode */
	unsigned int period_step;
	unsigned int sleep_min;
	snd_pcm_uframes_t avail_min;	/* min avail frames for wakeup */
	snd_pcm_uframes_t silence_threshold;	/* Silence filling happens when
					   noise is nearest than this */
	snd_pcm_uframes_t silence_size;	/* Silence filling size */
	snd_pcm_uframes_t xfer_align;	/* xfer size need to be a multiple */
	snd_pcm_uframes_t boundary;	/* pointers wrap point */
	unsigned int info;		/* Info for returned setup */
	unsigned int msbits;		/* used most significant bits */
	unsigned int rate_num;		/* rate numerator */
	unsigned int rate_den;		/* rate denominator */
	snd_pcm_uframes_t fifo_size;	/* chip FIFO size in frames */
	snd_pcm_uframes_t buffer_size;
	unsigned int sample_bits;
	unsigned int frame_bits;
	snd_pcm_uframes_t *appl_ptr;
	snd_pcm_uframes_t min_align;
	volatile snd_pcm_uframes_t *hw_ptr;
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
snd_pcm_sframes_t snd_pcm_mmap_appl_ptr(snd_pcm_t *pcm, off_t offset);
void snd_pcm_mmap_appl_backward(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
void snd_pcm_mmap_appl_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
void snd_pcm_mmap_hw_backward(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
void snd_pcm_mmap_hw_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
snd_pcm_uframes_t snd_pcm_mmap_hw_offset(snd_pcm_t *pcm);
snd_pcm_uframes_t snd_pcm_mmap_playback_xfer(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
snd_pcm_uframes_t snd_pcm_mmap_capture_xfer(snd_pcm_t *pcm, snd_pcm_uframes_t frames);

typedef snd_pcm_sframes_t (*snd_pcm_xfer_areas_func_t)(snd_pcm_t *pcm, 
					     const snd_pcm_channel_area_t *areas,
					     snd_pcm_uframes_t offset, snd_pcm_uframes_t size,
					     snd_pcm_uframes_t *slave_sizep);

snd_pcm_sframes_t snd_pcm_read_areas(snd_pcm_t *pcm, const snd_pcm_channel_area_t *areas,
			   snd_pcm_uframes_t offset, snd_pcm_uframes_t size,
			   snd_pcm_xfer_areas_func_t func);
snd_pcm_sframes_t snd_pcm_write_areas(snd_pcm_t *pcm, const snd_pcm_channel_area_t *areas,
			    snd_pcm_uframes_t offset, snd_pcm_uframes_t size,
			    snd_pcm_xfer_areas_func_t func);
snd_pcm_sframes_t snd_pcm_read_mmap(snd_pcm_t *pcm, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_write_mmap(snd_pcm_t *pcm, snd_pcm_uframes_t size);
int snd_pcm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info);
int snd_pcm_channel_info_shm(snd_pcm_t *pcm, snd_pcm_channel_info_t *info, int shmid);

static inline snd_pcm_uframes_t snd_pcm_mmap_playback_avail(snd_pcm_t *pcm)
{
	snd_pcm_sframes_t avail;
	avail = *pcm->hw_ptr + pcm->buffer_size - *pcm->appl_ptr;
	if (avail < 0)
		avail += pcm->boundary;
	return avail;
}

static inline snd_pcm_uframes_t snd_pcm_mmap_capture_avail(snd_pcm_t *pcm)
{
	snd_pcm_sframes_t avail;
	avail = *pcm->hw_ptr - *pcm->appl_ptr;
	if (avail < 0)
		avail += pcm->boundary;
	return avail;
}

static inline snd_pcm_uframes_t snd_pcm_mmap_avail(snd_pcm_t *pcm)
{
	snd_pcm_sframes_t avail;
	avail = *pcm->hw_ptr - *pcm->appl_ptr;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		avail += pcm->buffer_size;
	if (avail < 0)
		avail += pcm->boundary;
	return avail;
}

static inline snd_pcm_sframes_t snd_pcm_mmap_playback_hw_avail(snd_pcm_t *pcm)
{
	return pcm->buffer_size - snd_pcm_mmap_playback_avail(pcm);
}

static inline snd_pcm_sframes_t snd_pcm_mmap_capture_hw_avail(snd_pcm_t *pcm)
{
	return pcm->buffer_size - snd_pcm_mmap_capture_avail(pcm);
}

static inline snd_pcm_sframes_t snd_pcm_mmap_hw_avail(snd_pcm_t *pcm)
{
	snd_pcm_sframes_t avail;
	avail = *pcm->hw_ptr - *pcm->appl_ptr;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		avail += pcm->buffer_size;
	if (avail < 0)
		avail += pcm->boundary;
	return pcm->buffer_size - avail;
}

#define snd_pcm_mmap_playback_delay snd_pcm_mmap_playback_hw_avail
#define snd_pcm_mmap_capture_delay snd_pcm_mmap_capture_avail

static inline snd_pcm_sframes_t snd_pcm_mmap_delay(snd_pcm_t *pcm)
{
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_mmap_playback_delay(pcm);
	else
		return snd_pcm_mmap_capture_delay(pcm);
}

static inline void *snd_pcm_channel_area_addr(const snd_pcm_channel_area_t *area, snd_pcm_uframes_t offset)
{
	unsigned int bitofs = area->first + area->step * offset;
	assert(bitofs % 8 == 0);
	return area->addr + bitofs / 8;
}

static inline unsigned int snd_pcm_channel_area_step(const snd_pcm_channel_area_t *area)
{
	assert(area->step % 8 == 0);
	return area->step / 8;
}

static inline snd_pcm_sframes_t _snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	return pcm->fast_ops->writei(pcm->fast_op_arg, buffer, size);
}

static inline snd_pcm_sframes_t _snd_pcm_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	return pcm->fast_ops->writen(pcm->fast_op_arg, bufs, size);
}

static inline snd_pcm_sframes_t _snd_pcm_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	return pcm->fast_ops->readi(pcm->fast_op_arg, buffer, size);
}

static inline snd_pcm_sframes_t _snd_pcm_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
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

int snd_pcm_hw_refine_soft(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_refine_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			    int (*cprepare)(snd_pcm_t *pcm,
					    snd_pcm_hw_params_t *params),
			    int (*cchange)(snd_pcm_t *pcm,
					   snd_pcm_hw_params_t *params,
					   snd_pcm_hw_params_t *sparams),
			    int (*sprepare)(snd_pcm_t *pcm,
					    snd_pcm_hw_params_t *params),
			    int (*schange)(snd_pcm_t *pcm,
					   snd_pcm_hw_params_t *params,
					   snd_pcm_hw_params_t *sparams),
			    int (*srefine)(snd_pcm_t *pcm,
					   snd_pcm_hw_params_t *sparams));
int snd_pcm_hw_params_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			    int (*cchange)(snd_pcm_t *pcm,
					   snd_pcm_hw_params_t *params,
					   snd_pcm_hw_params_t *sparams),
			    int (*sprepare)(snd_pcm_t *pcm,
					    snd_pcm_hw_params_t *params),
			    int (*schange)(snd_pcm_t *pcm,
					   snd_pcm_hw_params_t *params,
					   snd_pcm_hw_params_t *sparams),
			    int (*sparams)(snd_pcm_t *pcm,
					   snd_pcm_hw_params_t *sparams));


void _snd_pcm_hw_params_any(snd_pcm_hw_params_t *params);
int _snd_pcm_hw_param_refine_interval(snd_pcm_hw_params_t *params,
				      snd_pcm_hw_param_t var,
				      const interval_t *val);
int _snd_pcm_hw_param_mask(snd_pcm_hw_params_t *params,
			    unsigned int var, const mask_t *mask);
int _snd_pcm_hw_param_first(snd_pcm_hw_params_t *params,
			    unsigned int var);
int _snd_pcm_hw_param_last(snd_pcm_hw_params_t *params,
			   unsigned int var);
int _snd_pcm_hw_param_set(snd_pcm_hw_params_t *params,
			   unsigned int var, unsigned int val, int dir);
int _snd_pcm_hw_param_min(snd_pcm_hw_params_t *params,
			   unsigned int var, unsigned int val, int dir);
int _snd_pcm_hw_param_max(snd_pcm_hw_params_t *params,
			   unsigned int var, unsigned int val, int dir);
int _snd_pcm_hw_param_minmax(snd_pcm_hw_params_t *params,
			     snd_pcm_hw_param_t var,
			     unsigned int min, int mindir,
			     unsigned int max, int maxdir);
int _snd_pcm_hw_param_refine(snd_pcm_hw_params_t *params,
			     snd_pcm_hw_param_t var,
			     const snd_pcm_hw_params_t *src);
int _snd_pcm_hw_params_refine(snd_pcm_hw_params_t *params,
			      unsigned int vars,
			      const snd_pcm_hw_params_t *src);
void snd_pcm_hw_param_refine_near(snd_pcm_t *pcm,
				  snd_pcm_hw_params_t *params,
				  snd_pcm_hw_param_t var,
				  const snd_pcm_hw_params_t *src);
int snd_pcm_hw_param_always_eq(const snd_pcm_hw_params_t *params,
			       snd_pcm_hw_param_t var,
			       const snd_pcm_hw_params_t *params1);
int snd_pcm_hw_param_never_eq(const snd_pcm_hw_params_t *params,
			      snd_pcm_hw_param_t var,
			      const snd_pcm_hw_params_t *params1);

#define SND_PCM_HW_PARBIT_ACCESS	(1 << SND_PCM_HW_PARAM_ACCESS)
#define SND_PCM_HW_PARBIT_FORMAT	(1 << SND_PCM_HW_PARAM_FORMAT)
#define SND_PCM_HW_PARBIT_SUBFORMAT	(1 << SND_PCM_HW_PARAM_SUBFORMAT)
#define SND_PCM_HW_PARBIT_CHANNELS	(1 << SND_PCM_HW_PARAM_CHANNELS)
#define SND_PCM_HW_PARBIT_RATE		(1 << SND_PCM_HW_PARAM_RATE)
#define SND_PCM_HW_PARBIT_PERIOD_TIME	(1 << SND_PCM_HW_PARAM_PERIOD_TIME)
#define SND_PCM_HW_PARBIT_PERIOD_SIZE	(1 << SND_PCM_HW_PARAM_PERIOD_SIZE)
#define SND_PCM_HW_PARBIT_PERIODS	(1 << SND_PCM_HW_PARAM_PERIODS)
#define SND_PCM_HW_PARBIT_BUFFER_TIME	(1 << SND_PCM_HW_PARAM_BUFFER_TIME)
#define SND_PCM_HW_PARBIT_BUFFER_SIZE	(1 << SND_PCM_HW_PARAM_BUFFER_SIZE)
#define SND_PCM_HW_PARBIT_SAMPLE_BITS	(1 << SND_PCM_HW_PARAM_SAMPLE_BITS)
#define SND_PCM_HW_PARBIT_FRAME_BITS	(1 << SND_PCM_HW_PARAM_FRAME_BITS)
#define SND_PCM_HW_PARBIT_PERIOD_BYTES	(1 << SND_PCM_HW_PARAM_PERIOD_BYTES)
#define SND_PCM_HW_PARBIT_BUFFER_BYTES	(1 << SND_PCM_HW_PARAM_BUFFER_BYTES)
#define SND_PCM_HW_PARBIT_TICK_TIME	(1 << SND_PCM_HW_PARAM_TICK_TIME)


#define SND_PCM_ACCBIT_MMAP ((1 << SND_PCM_ACCESS_MMAP_INTERLEAVED) | \
			     (1 << SND_PCM_ACCESS_MMAP_NONINTERLEAVED) | \
			     (1 << SND_PCM_ACCESS_MMAP_COMPLEX))
