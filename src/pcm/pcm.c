/*
 *  PCM Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@suse.cz>
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
  
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <limits.h>
#include <dlfcn.h>
#include "pcm_local.h"
#include "list.h"

snd_pcm_type_t snd_pcm_type(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->type;
}

snd_pcm_type_t snd_pcm_stream(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->stream;
}

int snd_pcm_close(snd_pcm_t *pcm)
{
	int ret = 0;
	int err;
	assert(pcm);
	if (pcm->setup) {
		if (pcm->mode & SND_PCM_NONBLOCK)
			snd_pcm_drop(pcm);
		else
			snd_pcm_drain(pcm);
	}
	if (pcm->mmap_channels) {
		if ((err = snd_pcm_munmap(pcm)) < 0)
			ret = err;
	}
	if ((err = pcm->ops->close(pcm->op_arg)) < 0)
		ret = err;
	pcm->setup = 0;
	if (pcm->name)
		free(pcm->name);
	free(pcm);
	return 0;
}	

int snd_pcm_nonblock(snd_pcm_t *pcm, int nonblock)
{
	int err;
	assert(pcm);
	if ((err = pcm->ops->nonblock(pcm->op_arg, nonblock)) < 0)
		return err;
	if (nonblock)
		pcm->mode |= SND_PCM_NONBLOCK;
	else
		pcm->mode &= ~SND_PCM_NONBLOCK;
	return 0;
}

int snd_pcm_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	assert(pcm);
	return pcm->ops->async(pcm->op_arg, sig, pid);
}

int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	assert(pcm && info);
	return pcm->ops->info(pcm->op_arg, info);
}

int snd_pcm_hw_info(snd_pcm_t *pcm, snd_pcm_hw_info_t *info)
{
	int err;
	assert(pcm && info);
#if 0
	fprintf(stderr, "hw_info entered:\n");
	snd_pcm_dump_hw_info(info, stderr);
	fprintf(stderr, "\n");
#endif
	err = pcm->ops->hw_info(pcm->op_arg, info);
#if 0
	fprintf(stderr, "hw_info return %d:\n", err);
	snd_pcm_dump_hw_info(info, stderr);
	fprintf(stderr, "\n");
#endif
	return err;
}

void snd_pcm_hw_info_all(snd_pcm_hw_info_t *info)
{
	assert(info);
	info->access_mask = ~0;
	info->format_mask = ~0;
	info->subformat_mask = ~0;
	info->channels_min = 1;
	info->channels_max = UINT_MAX;
	info->rate_min = 1;
	info->rate_max = UINT_MAX;
	info->fragment_size_min = 1;
	info->fragment_size_max = ULONG_MAX;
	info->fragments_min = 1;
	info->fragments_max = UINT_MAX;
	info->buffer_size_min = 1;
	info->buffer_size_max = ULONG_MAX;
}

void snd_pcm_hw_params_to_info(snd_pcm_hw_params_t *params, snd_pcm_hw_info_t *info)
{
	assert(info && params);
	info->access_mask = 1U << params->access;
	info->format_mask = 1U << params->format;
	info->subformat_mask = 1U << params->subformat;
	info->channels_min = info->channels_max = params->channels;
	info->rate_min = info->rate_max = params->rate;
	info->fragment_size_min = info->fragment_size_max = params->fragment_size;
	info->fragments_min = info->fragments_max = params->fragments;
	info->buffer_size_min = info->buffer_size_max = params->fragment_size * params->fragments;
}

void snd_pcm_hw_info_to_params(snd_pcm_hw_info_t *info, snd_pcm_hw_params_t *params)
{
	assert(info->access_mask && 
	       !(info->access_mask & (info->access_mask - 1)));
	params->access = ffs(info->access_mask) - 1;
	assert(info->format_mask && 
	       !(info->format_mask & (info->format_mask - 1)));
	params->format = ffs(info->format_mask) - 1;
	assert(info->subformat_mask && 
	       !(info->subformat_mask & (info->subformat_mask - 1)));
	params->subformat = ffs(info->subformat_mask) - 1;
	assert(info->channels_min == info->channels_max);
	params->channels = info->channels_min;
	assert(info->rate_min == info->rate_max);
	params->rate = info->rate_min;
	assert(info->fragment_size_min == info->fragment_size_max);
	params->fragment_size = info->fragment_size_min;
	assert(info->fragments_min == info->fragments_max);
	params->fragments = info->fragments_min;
}

void snd_pcm_hw_info_to_params_fail(snd_pcm_hw_info_t *info, snd_pcm_hw_params_t *params)
{
	unsigned int f = 0;
	if (info->access_mask == 0)
		f |= SND_PCM_HW_PARBIT_ACCESS;
	if (info->format_mask == 0)
		f |= SND_PCM_HW_PARBIT_FORMAT;
	if (info->subformat_mask == 0)
		f |= SND_PCM_HW_PARBIT_SUBFORMAT;
	if (info->channels_min > info->channels_max)
		f |= SND_PCM_HW_PARBIT_CHANNELS;
	if (info->rate_min > info->rate_max)
		f |= SND_PCM_HW_PARBIT_RATE;
	if (info->fragment_size_min > info->fragment_size_max)
		f |= SND_PCM_HW_PARBIT_FRAGMENT_SIZE;
	if (info->fragments_min > info->fragments_max)
		f |= SND_PCM_HW_PARBIT_FRAGMENTS;
	assert(f);
	params->fail_mask = f;
}

int _snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	int err;
	snd_pcm_hw_info_t info;
	
	snd_pcm_hw_params_to_info(params, &info);
	err = snd_pcm_hw_info(pcm, &info);
	if (err < 0) {
		snd_pcm_hw_info_to_params_fail(&info, params);
		return err;
	}
	snd_pcm_hw_info_to_params(&info, params);
	if ((err = pcm->ops->hw_params(pcm->op_arg, params)) < 0)
		return err;
	pcm->setup = 1;
	pcm->access = params->access;
	pcm->format = params->format;
	pcm->subformat = params->subformat;
	pcm->rate = params->rate;
	pcm->channels = params->channels;
	pcm->fragment_size = params->fragment_size;
	pcm->fragments = params->fragments;
	pcm->bits_per_sample = snd_pcm_format_physical_width(params->format);
        pcm->bits_per_frame = pcm->bits_per_sample * params->channels;
	pcm->buffer_size = params->fragment_size * params->fragments;

	pcm->info = info.info;
	pcm->msbits = info.msbits;
	pcm->rate_master = info.rate_master;
	pcm->rate_divisor = info.rate_divisor;
	pcm->fifo_size = info.fifo_size;

	/* Default sw params */
	pcm->start_mode = SND_PCM_START_DATA;
	pcm->ready_mode = SND_PCM_READY_FRAGMENT;
	pcm->xrun_mode = SND_PCM_XRUN_FRAGMENT;
	pcm->avail_min = pcm->fragment_size;
	pcm->xfer_min = pcm->fragment_size;
	pcm->xfer_align = pcm->fragment_size;
	pcm->time = 0;
	pcm->boundary = LONG_MAX - pcm->buffer_size * 2 - LONG_MAX % pcm->buffer_size;
	return 0;
}

int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	int err;
	assert(pcm && params);
	if (pcm->setup && pcm->mmap_channels && 
	    (pcm->mmap_rw || 
	     (pcm->access == SND_PCM_ACCESS_MMAP_INTERLEAVED ||
	      pcm->access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED ||
	      pcm->access == SND_PCM_ACCESS_MMAP_COMPLEX))) {
		err = snd_pcm_munmap(pcm);
		if (err < 0)
			return err;
	}
	err = _snd_pcm_hw_params(pcm, params);
	if (pcm->setup &&
	    (pcm->mmap_rw || 
	     (pcm->access == SND_PCM_ACCESS_MMAP_INTERLEAVED ||
	      pcm->access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED ||
	      pcm->access == SND_PCM_ACCESS_MMAP_COMPLEX))) {
		int err;
		err = snd_pcm_mmap(pcm);
		if (err < 0)
			return err;
	}
	return err;
}

int snd_pcm_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	int err;
	assert(pcm && params);
	assert(pcm->setup);
	if ((err = pcm->ops->sw_params(pcm->op_arg, params)) < 0)
		return err;
	pcm->start_mode = params->start_mode;
	pcm->ready_mode = params->ready_mode;
	pcm->xrun_mode = params->xrun_mode;
	pcm->avail_min = params->avail_min;
	pcm->xfer_min = params->xfer_min;
	pcm->xfer_align = params->xfer_align;
	pcm->time = params->time;
	pcm->boundary = params->boundary;
	return 0;
}

int snd_pcm_dig_params(snd_pcm_t *pcm, snd_pcm_dig_params_t *params)
{
	assert(pcm && params);
	return pcm->ops->dig_params(pcm->op_arg, params);
}

int snd_pcm_dig_info(snd_pcm_t *pcm, snd_pcm_dig_info_t *info)
{
	assert(pcm && info);
	return pcm->ops->dig_info(pcm->op_arg, info);
}

int snd_pcm_status(snd_pcm_t *pcm, snd_pcm_status_t *status)
{
	assert(pcm && status);
	return pcm->fast_ops->status(pcm->fast_op_arg, status);
}

int snd_pcm_state(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->fast_ops->state(pcm->fast_op_arg);
}

int snd_pcm_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	assert(pcm);
	assert(pcm->setup);
	return pcm->fast_ops->delay(pcm->fast_op_arg, delayp);
}

int snd_pcm_prepare(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->setup);
	return pcm->fast_ops->prepare(pcm->fast_op_arg);
}

int snd_pcm_reset(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->setup);
	return pcm->fast_ops->reset(pcm->fast_op_arg);
}

int snd_pcm_start(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->setup);
	return pcm->fast_ops->start(pcm->fast_op_arg);
}

int snd_pcm_drop(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->setup);
	return pcm->fast_ops->drop(pcm->fast_op_arg);
}

int snd_pcm_drain(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->setup);
	return pcm->fast_ops->drain(pcm->fast_op_arg);
}

int snd_pcm_pause(snd_pcm_t *pcm, int enable)
{
	assert(pcm);
	assert(pcm->setup);
	return pcm->fast_ops->pause(pcm->fast_op_arg, enable);
}


ssize_t snd_pcm_rewind(snd_pcm_t *pcm, size_t frames)
{
	assert(pcm);
	assert(pcm->setup);
	assert(frames > 0);
	return pcm->fast_ops->rewind(pcm->fast_op_arg, frames);
}

int snd_pcm_set_avail_min(snd_pcm_t *pcm, size_t frames)
{
	int err;
	assert(pcm);
	assert(pcm->setup);
	assert(frames > 0);
	err = pcm->fast_ops->set_avail_min(pcm->fast_op_arg, frames);
	if (err < 0)
		return err;
	pcm->avail_min = frames;
	return 0;
}

ssize_t snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	assert(pcm);
	assert(size == 0 || buffer);
	assert(pcm->setup);
	assert(pcm->access == SND_PCM_ACCESS_RW_INTERLEAVED);
	return _snd_pcm_writei(pcm, buffer, size);
}

ssize_t snd_pcm_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	assert(pcm);
	assert(size == 0 || bufs);
	assert(pcm->setup);
	assert(pcm->access == SND_PCM_ACCESS_RW_NONINTERLEAVED);
	return _snd_pcm_writen(pcm, bufs, size);
}

ssize_t snd_pcm_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	assert(pcm);
	assert(size == 0 || buffer);
	assert(pcm->setup);
	assert(pcm->access == SND_PCM_ACCESS_RW_INTERLEAVED);
	return _snd_pcm_readi(pcm, buffer, size);
}

ssize_t snd_pcm_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	assert(pcm);
	assert(size == 0 || bufs);
	assert(pcm->setup);
	assert(pcm->access == SND_PCM_ACCESS_RW_NONINTERLEAVED);
	return _snd_pcm_readn(pcm, bufs, size);
}

ssize_t snd_pcm_writev(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	void **bufs;
	int k;
	assert(pcm);
	assert(pcm->setup);
	assert((int)pcm->channels == count);
	bufs = alloca(sizeof(*bufs) * count);
	for (k = 0; k < count; ++k) {
		bufs[k] = vector[k].iov_base;
		assert(vector[k].iov_len == vector[0].iov_len);
	}
	return snd_pcm_writen(pcm, bufs, vector[0].iov_len);
}

ssize_t snd_pcm_readv(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	void **bufs;
	int k;
	assert(pcm);
	assert(pcm->setup);
	assert((int)pcm->channels == count);
	bufs = alloca(sizeof(*bufs) * count);
	for (k = 0; k < count; ++k) {
		bufs[k] = vector[k].iov_base;
		assert(vector[k].iov_len == vector[0].iov_len);
	}
	return snd_pcm_readn(pcm, bufs, vector[0].iov_len);
}

/* FIXME */
#define snd_pcm_link_descriptor snd_pcm_poll_descriptor

int snd_pcm_link(snd_pcm_t *pcm1, snd_pcm_t *pcm2)
{
	int fd1 = snd_pcm_link_descriptor(pcm1);
	int fd2 = snd_pcm_link_descriptor(pcm2);
	if (fd1 < 0 || fd2 < 0)
		return -ENOSYS;
	if (ioctl(fd1, SND_PCM_IOCTL_LINK, fd2) < 0) {
		SYSERR("SND_PCM_IOCTL_LINK failed");
		return -errno;
	}
	return 0;
}

int snd_pcm_unlink(snd_pcm_t *pcm)
{
	int fd;
	switch (pcm->type) {
	case SND_PCM_TYPE_HW:
	case SND_PCM_TYPE_MULTI:
		fd = snd_pcm_poll_descriptor(pcm);
		break;
	default:
		return -ENOSYS;
	}
	if (ioctl(fd, SND_PCM_IOCTL_UNLINK) < 0) {
		SYSERR("SND_PCM_IOCTL_UNLINK failed");
		return -errno;
	}
	return 0;
}

int snd_pcm_poll_descriptor(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->poll_fd;
}

typedef struct {
	int value;
	const char* name;
	const char* desc;
} assoc_t;

static assoc_t *assoc_value(int value, assoc_t *alist)
{
	while (alist->name) {
		if (value == alist->value)
			return alist;
		alist++;
	}
	return 0;
}

static assoc_t *assoc_name(const char *name, assoc_t *alist)
{
	while (alist->name) {
		if (strcasecmp(name, alist->name) == 0)
			return alist;
		alist++;
	}
	return 0;
}

static const char *assoc(int value, assoc_t *alist)
{
	assoc_t *a;
	a = assoc_value(value, alist);
	if (a)
		return a->name;
	return "UNKNOWN";
}

#define STATE(v) { SND_PCM_STATE_##v, #v, #v }
#define STREAM(v) { SND_PCM_STREAM_##v, #v, #v }
#define READY(v) { SND_PCM_READY_##v, #v, #v }
#define XRUN(v) { SND_PCM_XRUN_##v, #v, #v }
#define ACCESS(v) { SND_PCM_ACCESS_##v, #v, #v }
#define FORMAT(v, d) { SND_PCM_FORMAT_##v, #v, d }
#define SUBFORMAT(v, d) { SND_PCM_SUBFORMAT_##v, #v, d }
#define XRUN_ACT(v) { SND_PCM_XRUN_ACT_##v, #v, #v }
#define START(v) { SND_PCM_START_##v, #v, #v }
#define FILL(v) { SND_PCM_FILL_##v, #v, #v }
#define HW_PARAM(v) { SND_PCM_HW_PARAM_##v, #v, #v }
#define SW_PARAM(v) { SND_PCM_SW_PARAM_##v, #v, #v }
#define END { 0, NULL, NULL }

static assoc_t streams[] = {
	STREAM(PLAYBACK),
	STREAM(CAPTURE),
	END
};

static assoc_t states[] = {
	STATE(OPEN),
	STATE(SETUP),
	STATE(PREPARED),
	STATE(RUNNING),
	STATE(XRUN),
	STATE(PAUSED),
	END
};

#if 0
static assoc_t hw_params[] = {
	HW_PARAM(ACCESS),
	HW_PARAM(FORMAT),
	HW_PARAM(SUBFORMAT),
	HW_PARAM(CHANNELS),
	HW_PARAM(RATE),
	HW_PARAM(FRAGMENT_SIZE),
	HW_PARAM(FRAGMENTS),
	HW_PARAM(BUFFER_SIZE),
	END
};

static assoc_t sw_params[] = {
	SW_PARAM(START_MODE),
	SW_PARAM(READY_MODE),
	SW_PARAM(AVAIL_MIN),
	SW_PARAM(XFER_MIN),
	SW_PARAM(XFER_ALIGN),
	SW_PARAM(XRUN_MODE),
	SW_PARAM(TIME),
	END
};
#endif

static assoc_t accesses[] = {
	ACCESS(MMAP_INTERLEAVED), 
	ACCESS(MMAP_NONINTERLEAVED),
	ACCESS(MMAP_COMPLEX),
	ACCESS(RW_INTERLEAVED),
	ACCESS(RW_NONINTERLEAVED),
	END
};

static assoc_t formats[] = {
	FORMAT(S8, "Signed 8-bit"), 
	FORMAT(U8, "Unsigned 8-bit"),
	FORMAT(S16_LE, "Signed 16-bit Little Endian"),
	FORMAT(S16_BE, "Signed 16-bit Big Endian"),
	FORMAT(U16_LE, "Unsigned 16-bit Little Endian"),
	FORMAT(U16_BE, "Unsigned 16-bit Big Endian"),
	FORMAT(S24_LE, "Signed 24-bit Little Endian"),
	FORMAT(S24_BE, "Signed 24-bit Big Endian"),
	FORMAT(U24_LE, "Unsigned 24-bit Little Endian"),
	FORMAT(U24_BE, "Unsigned 24-bit Big Endian"),
	FORMAT(S32_LE, "Signed 32-bit Little Endian"),
	FORMAT(S32_BE, "Signed 32-bit Big Endian"),
	FORMAT(U32_LE, "Unsigned 32-bit Little Endian"),
	FORMAT(U32_BE, "Unsigned 32-bit Big Endian"),
	FORMAT(FLOAT_LE, "Float Little Endian"),
	FORMAT(FLOAT_BE, "Float Big Endian"),
	FORMAT(FLOAT64_LE, "Float64 Little Endian"),
	FORMAT(FLOAT64_BE, "Float64 Big Endian"),
	FORMAT(IEC958_SUBFRAME_LE, "IEC-958 Little Endian"),
	FORMAT(IEC958_SUBFRAME_BE, "IEC-958 Big Endian"),
	FORMAT(MU_LAW, "Mu-Law"),
	FORMAT(A_LAW, "A-Law"),
	FORMAT(IMA_ADPCM, "Ima-ADPCM"),
	FORMAT(MPEG, "MPEG"),
	FORMAT(GSM, "GSM"),
	FORMAT(SPECIAL, "Special"),
	END 
};

static assoc_t subformats[] = {
	SUBFORMAT(STD, "Standard"), 
	END
};

static assoc_t starts[] = {
	START(EXPLICIT),
	START(DATA),
	END
};
static assoc_t readys[] = {
	READY(FRAGMENT),
	READY(ASAP),
	END
};

static assoc_t xruns[] = {
	XRUN(ASAP),
	XRUN(FRAGMENT),
	XRUN(NONE),
	END
};

static assoc_t onoff[] = {
	{0, "OFF", NULL},
	{1, "ON", NULL},
	{-1, "ON", NULL},
	END
};

int snd_pcm_dump_hw_setup(snd_pcm_t *pcm, FILE *fp)
{
	assert(pcm);
	assert(fp);
	assert(pcm->setup);
        fprintf(fp, "stream       : %s\n", assoc(pcm->stream, streams));
	fprintf(fp, "access       : %s\n", assoc(pcm->access, accesses));
	fprintf(fp, "format       : %s\n", assoc(pcm->format, formats));
	fprintf(fp, "subformat    : %s\n", assoc(pcm->subformat, subformats));
	fprintf(fp, "channels     : %d\n", pcm->channels);
	fprintf(fp, "rate         : %d\n", pcm->rate);
	fprintf(fp, "rate         : %g (%d/%d)\n", (double) pcm->rate_master / pcm->rate_divisor, pcm->rate_master, pcm->rate_divisor);
	fprintf(fp, "msbits       : %d\n", pcm->msbits);
	fprintf(fp, "fragment_size: %ld\n", (long)pcm->fragment_size);
	fprintf(fp, "fragments    : %d\n", pcm->fragments);
	return 0;
}

int snd_pcm_dump_sw_setup(snd_pcm_t *pcm, FILE *fp)
{
	assert(pcm);
	assert(fp);
	assert(pcm->setup);
	fprintf(fp, "start_mode   : %s\n", assoc(pcm->start_mode, starts));
	fprintf(fp, "ready_mode   : %s\n", assoc(pcm->ready_mode, readys));
	fprintf(fp, "xrun_mode    : %s\n", assoc(pcm->xrun_mode, xruns));
	fprintf(fp, "avail_min    : %ld\n", (long)pcm->avail_min);
	fprintf(fp, "xfer_min     : %ld\n", (long)pcm->xfer_min);
	fprintf(fp, "xfer_align   : %ld\n", (long)pcm->xfer_align);
	fprintf(fp, "time         : %s\n", assoc(pcm->time, onoff));
	fprintf(fp, "boundary     : %ld\n", (long)pcm->boundary);
	return 0;
}

int snd_pcm_dump_setup(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_dump_hw_setup(pcm, fp);
	snd_pcm_dump_sw_setup(pcm, fp);
	return 0;
}

int snd_pcm_dump_hw_info(snd_pcm_hw_info_t *info, FILE *fp)
{
	unsigned int k;
	fputs("access:", fp);
	if (info->access_mask == ~0U)
		fputs(" ALL", fp);
	else if (info->access_mask) {
		for (k = 0; k <= SND_PCM_ACCESS_LAST; ++k)
			if (info->access_mask & (1U << k)) {
				putc(' ', fp);
				fputs(assoc(k, accesses), fp);
			}
	} else
		fputs(" NONE", fp);
	putc('\n', fp);

	fputs("format:", fp);
	if (info->format_mask == ~0U)
		fputs(" ALL", fp);
	else if (info->format_mask) {
		for (k = 0; k <= SND_PCM_FORMAT_LAST; ++k)
			if (info->format_mask & (1U << k)) {
				putc(' ', fp);
				fputs(assoc(k, formats), fp);
			}
	} else
		fputs(" NONE", fp);
	putc('\n', fp);
	
	fputs("subformat:", fp);
	if (info->subformat_mask == ~0U)
		fputs(" ALL", fp);
	else if (info->subformat_mask) {
		for (k = 0; k <= SND_PCM_SUBFORMAT_LAST; ++k)
			if (info->subformat_mask & (1U << k)) {
				putc(' ', fp);
				fputs(assoc(k, subformats), fp);
			}
	} else
		fputs(" NONE", fp);
	putc('\n', fp);

	fputs("channels: ", fp);
	if (info->channels_min <= 1 && info->channels_max == UINT_MAX)
		fputs("ALL", fp);
	else if (info->channels_min > info->channels_max)
		fputs("NONE", fp);
	else {
		fprintf(fp, "%u", info->channels_min);
		if (info->channels_min < info->channels_max)
			fprintf(fp, " - %u", info->channels_max);
	}
	putc('\n', fp);

	fputs("rate: ", fp);
	if (info->rate_min <= 1 && info->rate_max == UINT_MAX)
		fputs("ALL", fp);
	else if (info->rate_min > info->rate_max)
		fputs("NONE", fp);
	else {
		fprintf(fp, "%u", info->rate_min);
		if (info->rate_min < info->rate_max)
			fprintf(fp, " - %u", info->rate_max);
	}
	putc('\n', fp);

	fputs("fragment_size: ", fp);
	if (info->fragment_size_min <= 1 && 
	    info->fragment_size_max == ULONG_MAX)
		fputs("ALL", fp);
	else if (info->fragment_size_min > info->fragment_size_max)
		fputs("NONE", fp);
	else {
		fprintf(fp, "%lu", (unsigned long)info->fragment_size_min);
		if (info->fragment_size_min < info->fragment_size_max)
			fprintf(fp, " - %lu", (unsigned long)info->fragment_size_max);
	}
	putc('\n', fp);

	fputs("fragments: ", fp);
	if (info->fragments_min <= 1 && info->fragments_max == UINT_MAX)
		fputs("ALL", fp);
	else if (info->fragments_min > info->fragments_max)
		fputs("NONE", fp);
	else {
		fprintf(fp, "%u", info->fragments_min);
		if (info->fragments_min < info->fragments_max)
			fprintf(fp, " - %u", info->fragments_max);
	}
	putc('\n', fp);

	fputs("buffer_size: ", fp);
	if (info->buffer_size_min <= 1 && 
	    info->buffer_size_max == ULONG_MAX)
		fputs("ALL", fp);
	else if (info->buffer_size_min > info->buffer_size_max)
		fputs("NONE", fp);
	else {
		fprintf(fp, "%lu", (unsigned long)info->buffer_size_min);
		if (info->buffer_size_min < info->buffer_size_max)
			fprintf(fp, " - %lu", (unsigned long)info->buffer_size_max);
	}
	putc('\n', fp);

	return 0;
}

int snd_pcm_dump_hw_params_fail(snd_pcm_hw_params_t *params, FILE *fp)
{
	int k;
	if (params->fail_mask == 0)
		return 0;
	fprintf(fp, "hw_params failed on the following field value(s):\n");
	for (k = 0; k <= SND_PCM_HW_PARAM_LAST; ++k) {
		if (!(params->fail_mask & (1U << k)))
			continue;
		switch (k) {
		case SND_PCM_HW_PARAM_ACCESS:
			fprintf(fp, "access: %s\n", assoc(params->access, accesses));
			break;
		case SND_PCM_HW_PARAM_FORMAT:
			fprintf(fp, "format: %s\n", assoc(params->format, formats));
			break;
		case SND_PCM_HW_PARAM_SUBFORMAT:
			fprintf(fp, "subformat: %s\n", assoc(params->subformat, subformats));
			break;
		case SND_PCM_HW_PARAM_CHANNELS:
			fprintf(fp, "channels: %d\n", params->channels);
			break;
		case SND_PCM_HW_PARAM_RATE:
			fprintf(fp, "rate: %d\n", params->rate);
			break;
		case SND_PCM_HW_PARAM_FRAGMENT_SIZE:
			fprintf(fp, "fragment_size: %ld\n", (long)params->fragment_size);
			break;
		case SND_PCM_HW_PARAM_FRAGMENTS:
			fprintf(fp, "fragments: %d\n", params->fragments);
			break;
		default:
			assert(0);
			break;
		}
	}
	return 0;
}

int snd_pcm_dump_sw_params_fail(snd_pcm_sw_params_t *params, FILE *fp)
{
	int k;
	if (params->fail_mask == 0)
		return 0;
	fprintf(fp, "sw_params failed on the following field value(s):\n");
	for (k = 0; k <= SND_PCM_SW_PARAM_LAST; ++k) {
		if (!(params->fail_mask & (1U << k)))
			continue;
		switch (k) {
		case SND_PCM_SW_PARAM_START_MODE:
			fprintf(fp, "start_mode: %s\n", assoc(params->start_mode, starts));
			break;
		case SND_PCM_SW_PARAM_READY_MODE:
			fprintf(fp, "ready_mode: %s\n", assoc(params->ready_mode, readys));
			break;
		case SND_PCM_SW_PARAM_XRUN_MODE:
			fprintf(fp, "xrun_mode: %s\n", assoc(params->xrun_mode, xruns));
			break;
		case SND_PCM_SW_PARAM_AVAIL_MIN:
			fprintf(fp, "avail_min: %ld\n", (long)params->avail_min);
			break;
		case SND_PCM_SW_PARAM_XFER_MIN:
			fprintf(fp, "xfer_min: %ld\n", (long)params->xfer_min);
			break;
		case SND_PCM_SW_PARAM_XFER_ALIGN:
			fprintf(fp, "xfer_align: %ld\n", (long)params->xfer_align);
			break;
		case SND_PCM_SW_PARAM_TIME:
			fprintf(fp, "time: %d\n", params->time);
			break;
		default:
			assert(0);
			break;
		}
	}
	return 0;
}

int snd_pcm_dump_status(snd_pcm_status_t *status, FILE *fp)
{
	assert(status);
	fprintf(fp, "state       : %s\n", assoc(status->state, states));
	fprintf(fp, "trigger_time: %ld.%06ld\n",
		status->trigger_time.tv_sec, status->trigger_time.tv_usec);
	fprintf(fp, "tstamp      : %ld.%06ld\n",
		status->tstamp.tv_sec, status->tstamp.tv_usec);
	fprintf(fp, "delay       : %ld\n", (long)status->delay);
	fprintf(fp, "avail       : %ld\n", (long)status->avail);
	fprintf(fp, "avail_max   : %ld\n", (long)status->avail_max);
	return 0;
}

int snd_pcm_dump(snd_pcm_t *pcm, FILE *fp)
{
	assert(pcm);
	assert(fp);
	pcm->ops->dump(pcm->op_arg, fp);
	return 0;
}

const char *snd_pcm_format_name(int format)
{
	assoc_t *a = assoc_value(format, formats);
	if (a)
		return a->name;
	return 0;
}

const char *snd_pcm_format_description(int format)
{
	assoc_t *a = assoc_value(format, formats);
	if (a)
		return a->desc;
	return "Unknown";
}

int snd_pcm_format_value(const char* name)
{
	assoc_t *a = assoc_name(name, formats);
	if (a)
		return a->value;
	return -1;
}

ssize_t snd_pcm_bytes_to_frames(snd_pcm_t *pcm, ssize_t bytes)
{
	assert(pcm);
	assert(pcm->setup);
	return bytes * 8 / pcm->bits_per_frame;
}

ssize_t snd_pcm_frames_to_bytes(snd_pcm_t *pcm, ssize_t frames)
{
	assert(pcm);
	assert(pcm->setup);
	return frames * pcm->bits_per_frame / 8;
}

ssize_t snd_pcm_bytes_to_samples(snd_pcm_t *pcm, ssize_t bytes)
{
	assert(pcm);
	assert(pcm->setup);
	return bytes * 8 / pcm->bits_per_sample;
}

ssize_t snd_pcm_samples_to_bytes(snd_pcm_t *pcm, ssize_t samples)
{
	assert(pcm);
	assert(pcm->setup);
	return samples * pcm->bits_per_sample / 8;
}

int snd_pcm_open(snd_pcm_t **pcmp, char *name, 
		 int stream, int mode)
{
	char *str;
	int err;
	snd_config_t *pcm_conf, *conf, *type_conf;
	snd_config_iterator_t i;
	char *lib = NULL, *open = NULL;
	int (*open_func)(snd_pcm_t **pcmp, char *name, snd_config_t *conf, 
			 int stream, int mode);
	void *h;
	assert(pcmp && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	err = snd_config_searchv(snd_config, &pcm_conf, "pcm", name, 0);
	if (err < 0) {
		int card, dev, subdev;
		char socket[256], sname[256];
		char format[16], file[256];
		err = sscanf(name, "hw:%d,%d,%d", &card, &dev, &subdev);
		if (err == 3)
			return snd_pcm_hw_open(pcmp, name, card, dev, subdev, stream, mode);
		err = sscanf(name, "hw:%d,%d", &card, &dev);
		if (err == 2)
			return snd_pcm_hw_open(pcmp, name, card, dev, -1, stream, mode);
		err = sscanf(name, "plug:%d,%d,%d", &card, &dev, &subdev);
		if (err == 3)
			return snd_pcm_plug_open_hw(pcmp, name, card, dev, subdev, stream, mode);
		err = sscanf(name, "plug:%d,%d", &card, &dev);
		if (err == 2)
			return snd_pcm_plug_open_hw(pcmp, name, card, dev, -1, stream, mode);
		err = sscanf(name, "shm:%256s,%256s", socket, sname);
		if (err == 2)
			return snd_pcm_shm_open(pcmp, NULL, socket, sname, stream, mode);
		err = sscanf(name, "file:%256s,%16s", file, format);
		if (err == 2) {
			snd_pcm_t *slave;
			err = snd_pcm_null_open(&slave, NULL, stream, mode);
			if (err < 0)
				return err;
			return snd_pcm_file_open(pcmp, NULL, file, -1, format, slave, 1);
		}
		err = sscanf(name, "file:%256s", file);
		if (err == 1) {
			snd_pcm_t *slave;
			err = snd_pcm_null_open(&slave, NULL, stream, mode);
			if (err < 0)
				return err;
			return snd_pcm_file_open(pcmp, NULL, file, -1, "raw", slave, 1);
		}
		if (strcmp(name, "null") == 0)
			return snd_pcm_null_open(pcmp, NULL, stream, mode);
		ERR("Unknown PCM %s", name);
		return -ENOENT;
	}
	if (snd_config_type(pcm_conf) != SND_CONFIG_TYPE_COMPOUND) {
		ERR("Invalid type for PCM definition");
		return -EINVAL;
	}
	err = snd_config_search(pcm_conf, "stream", &conf);
	if (err >= 0) {
		err = snd_config_string_get(conf, &str);
		if (err < 0) {
			ERR("Invalid type for stream");
			return err;
		}
		if (strcmp(str, "playback") == 0) {
			if (stream != SND_PCM_STREAM_PLAYBACK)
				return -EINVAL;
		} else if (strcmp(str, "capture") == 0) {
			if (stream != SND_PCM_STREAM_CAPTURE)
				return -EINVAL;
		} else {
			ERR("Invalid value for stream");
			return -EINVAL;
		}
	}
	err = snd_config_search(pcm_conf, "type", &conf);
	if (err < 0) {
		ERR("type is not defined");
		return err;
	}
	err = snd_config_string_get(conf, &str);
	if (err < 0) {
		ERR("Invalid type for type");
		return err;
	}
	err = snd_config_searchv(snd_config, &type_conf, "pcmtype", str, 0);
	if (err < 0) {
		ERR("Unknown PCM type %s", str);
		return err;
	}
	snd_config_foreach(i, type_conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "lib") == 0) {
			err = snd_config_string_get(n, &lib);
			if (err < 0) {
				ERR("Invalid type for lib");
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "open") == 0) {
			err = snd_config_string_get(n, &open);
			if (err < 0) {
				ERR("Invalid type for open");
				return -EINVAL;
			}
			continue;
			ERR("Unknown field: %s", n->id);
			return -EINVAL;
		}
	}
	if (!open) {
		ERR("open is not defined");
		return -EINVAL;
	}
	if (!lib)
		lib = "libasound.so";
	h = dlopen(lib, RTLD_NOW);
	if (!h) {
		ERR("Cannot open shared library %s", lib);
		return -ENOENT;
	}
	open_func = dlsym(h, open);
	dlclose(h);
	if (!open_func) {
		ERR("symbol %s is not defined inside %s", open, lib);
		return -ENXIO;
	}
	return open_func(pcmp, name, pcm_conf, stream, mode);
}

void snd_pcm_areas_from_buf(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, 
			    void *buf)
{
	unsigned int channel;
	unsigned int channels = pcm->channels;
	for (channel = 0; channel < channels; ++channel, ++areas) {
		areas->addr = buf;
		areas->first = channel * pcm->bits_per_sample;
		areas->step = pcm->bits_per_frame;
	}
}

void snd_pcm_areas_from_bufs(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, 
			     void **bufs)
{
	unsigned int channel;
	unsigned int channels = pcm->channels;
	for (channel = 0; channel < channels; ++channel, ++areas, ++bufs) {
		areas->addr = *bufs;
		areas->first = 0;
		areas->step = pcm->bits_per_sample;
	}
}

int snd_pcm_wait(snd_pcm_t *pcm, int timeout)
{
	struct pollfd pfd;
	int err;
	pfd.fd = snd_pcm_poll_descriptor(pcm);
	pfd.events = pcm->stream == SND_PCM_STREAM_PLAYBACK ? POLLOUT : POLLIN;
	err = poll(&pfd, 1, timeout);
	if (err < 0)
		return err;
	return 0;
}

ssize_t snd_pcm_avail_update(snd_pcm_t *pcm)
{
	return pcm->fast_ops->avail_update(pcm->fast_op_arg);
}

ssize_t snd_pcm_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	assert(size > 0);
	assert(size <= snd_pcm_mmap_avail(pcm));
	return pcm->fast_ops->mmap_forward(pcm->fast_op_arg, size);
}

int snd_pcm_area_silence(snd_pcm_channel_area_t *dst_area, size_t dst_offset,
			 size_t samples, int format)
{
	/* FIXME: sub byte resolution and odd dst_offset */
	char *dst;
	unsigned int dst_step;
	int width;
	u_int64_t silence;
	if (!dst_area->addr)
		return 0;
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	width = snd_pcm_format_physical_width(format);
	silence = snd_pcm_format_silence_64(format);
	if (dst_area->step == (unsigned int) width) {
		size_t dwords = samples * width / 64;
		samples -= dwords * 64 / width;
		while (dwords-- > 0)
			*((u_int64_t*)dst)++ = silence;
		if (samples == 0)
			return 0;
	}
	dst_step = dst_area->step / 8;
	switch (width) {
	case 4: {
		u_int8_t s0 = silence & 0xf0;
		u_int8_t s1 = silence & 0x0f;
		int dstbit = dst_area->first % 8;
		int dstbit_step = dst_area->step % 8;
		while (samples-- > 0) {
			if (dstbit) {
				*dst &= 0xf0;
				*dst |= s1;
			} else {
				*dst &= 0x0f;
				*dst |= s0;
			}
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8: {
		u_int8_t sil = silence;
		while (samples-- > 0) {
			*dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 16: {
		u_int16_t sil = silence;
		while (samples-- > 0) {
			*(u_int16_t*)dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 32: {
		u_int32_t sil = silence;
		while (samples-- > 0) {
			*(u_int32_t*)dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 64: {
		while (samples-- > 0) {
			*(u_int64_t*)dst = silence;
			dst += dst_step;
		}
		break;
	}
	default:
		assert(0);
	}
	return 0;
}

int snd_pcm_areas_silence(snd_pcm_channel_area_t *dst_areas, size_t dst_offset,
			  size_t channels, size_t frames, int format)
{
	int width = snd_pcm_format_physical_width(format);
	while (channels > 0) {
		void *addr = dst_areas->addr;
		unsigned int step = dst_areas->step;
		snd_pcm_channel_area_t *begin = dst_areas;
		int channels1 = channels;
		unsigned int chns = 0;
		int err;
		while (1) {
			channels1--;
			chns++;
			dst_areas++;
			if (channels1 == 0 ||
			    dst_areas->addr != addr ||
			    dst_areas->step != step ||
			    dst_areas->first != dst_areas[-1].first + width)
				break;
		}
		if (chns > 1 && chns * width == step) {
			/* Collapse the areas */
			snd_pcm_channel_area_t d;
			d.addr = begin->addr;
			d.first = begin->first;
			d.step = width;
			err = snd_pcm_area_silence(&d, dst_offset * chns, frames * chns, format);
			channels -= chns;
		} else {
			err = snd_pcm_area_silence(begin, dst_offset, frames, format);
			dst_areas = begin + 1;
			channels--;
		}
		if (err < 0)
			return err;
	}
	return 0;
}


int snd_pcm_area_copy(snd_pcm_channel_area_t *src_area, size_t src_offset,
		      snd_pcm_channel_area_t *dst_area, size_t dst_offset,
		      size_t samples, int format)
{
	/* FIXME: sub byte resolution and odd dst_offset */
	char *src, *dst;
	int width;
	int src_step, dst_step;
	if (!src_area->addr)
		return snd_pcm_area_silence(dst_area, dst_offset, samples, format);
	src = snd_pcm_channel_area_addr(src_area, src_offset);
	if (!dst_area->addr)
		return 0;
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	width = snd_pcm_format_physical_width(format);
	if (src_area->step == (unsigned int) width &&
	    dst_area->step == (unsigned int) width) {
		size_t bytes = samples * width / 8;
		samples -= bytes * 8 / width;
		memcpy(dst, src, bytes);
		if (samples == 0)
			return 0;
	}
	src_step = src_area->step / 8;
	dst_step = dst_area->step / 8;
	switch (width) {
	case 4: {
		int srcbit = src_area->first % 8;
		int srcbit_step = src_area->step % 8;
		int dstbit = dst_area->first % 8;
		int dstbit_step = dst_area->step % 8;
		while (samples-- > 0) {
			unsigned char srcval;
			if (srcbit)
				srcval = *src & 0x0f;
			else
				srcval = *src & 0xf0;
			if (dstbit)
				*dst &= 0xf0;
			else
				*dst &= 0x0f;
			*dst |= srcval;
			src += src_step;
			srcbit += srcbit_step;
			if (srcbit == 8) {
				src++;
				srcbit = 0;
			}
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8: {
		while (samples-- > 0) {
			*dst = *src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 16: {
		while (samples-- > 0) {
			*(u_int16_t*)dst = *(u_int16_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 32: {
		while (samples-- > 0) {
			*(u_int32_t*)dst = *(u_int32_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 64: {
		while (samples-- > 0) {
			*(u_int64_t*)dst = *(u_int64_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	default:
		assert(0);
	}
	return 0;
}

int snd_pcm_areas_copy(snd_pcm_channel_area_t *src_areas, size_t src_offset,
		       snd_pcm_channel_area_t *dst_areas, size_t dst_offset,
		       size_t channels, size_t frames, int format)
{
	int width = snd_pcm_format_physical_width(format);
	while (channels > 0) {
		unsigned int step = src_areas->step;
		void *src_addr = src_areas->addr;
		snd_pcm_channel_area_t *src_start = src_areas;
		void *dst_addr = dst_areas->addr;
		snd_pcm_channel_area_t *dst_start = dst_areas;
		int channels1 = channels;
		unsigned int chns = 0;
		while (dst_areas->step == step) {
			channels1--;
			chns++;
			src_areas++;
			dst_areas++;
			if (channels1 == 0 ||
			    src_areas->step != step ||
			    src_areas->addr != src_addr ||
			    dst_areas->addr != dst_addr ||
			    src_areas->first != src_areas[-1].first + width ||
			    dst_areas->first != dst_areas[-1].first + width)
				break;
		}
		if (chns > 1 && chns * width == step) {
			/* Collapse the areas */
			snd_pcm_channel_area_t s, d;
			s.addr = src_start->addr;
			s.first = src_start->first;
			s.step = width;
			d.addr = dst_start->addr;
			d.first = dst_start->first;
			d.step = width;
			snd_pcm_area_copy(&s, src_offset * chns, &d, dst_offset * chns, frames * chns, format);
			channels -= chns;
		} else {
			snd_pcm_area_copy(src_start, src_offset, dst_start, dst_offset, frames, format);
			src_areas = src_start + 1;
			dst_areas = dst_start + 1;
			channels--;
		}
	}
	return 0;
}

ssize_t snd_pcm_read_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas,
			   size_t offset, size_t size,
			   snd_pcm_xfer_areas_func_t func)
{
	size_t xfer = 0;
	ssize_t err = 0;
	int state = snd_pcm_state(pcm);
	assert(size > 0);
	assert(state >= SND_PCM_STATE_PREPARED);
	if (state == SND_PCM_STATE_PREPARED &&
	    pcm->start_mode != SND_PCM_START_EXPLICIT) {
		err = snd_pcm_start(pcm);
		if (err < 0)
			return err;
		state = SND_PCM_STATE_RUNNING;
	}
	while (xfer < size) {
		ssize_t avail;
		size_t frames;
	again:
		avail = snd_pcm_avail_update(pcm);
		if (avail < 0) {
			err = avail;
			break;
		}
		if ((size_t)avail < pcm->avail_min) {
			if (state != SND_PCM_STATE_RUNNING) {
				err = -EPIPE;
				break;
			}
			if (pcm->mode & SND_PCM_NONBLOCK) {
				err = -EAGAIN;
				break;
			}
			err = snd_pcm_wait(pcm, -1);
			if (err < 0)
				break;
			state = snd_pcm_state(pcm);
			goto again;
		}
		frames = size - xfer;
		if (frames > (size_t)avail)
			frames = avail;
		err = func(pcm, areas, offset, frames, 0);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		xfer += err;
		offset += err;
	}
	if (xfer > 0)
		return xfer;
	return err;
}

ssize_t snd_pcm_write_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas,
			    size_t offset, size_t size,
			    snd_pcm_xfer_areas_func_t func)
{
	size_t xfer = 0;
	ssize_t err = 0;
	int state = snd_pcm_state(pcm);
	assert(size > 0);
	assert(state >= SND_PCM_STATE_PREPARED);
	while (xfer < size) {
		ssize_t avail;
		size_t frames;
	again:
		if (state == SND_PCM_STATE_XRUN) {
			err = -EPIPE;
			break;
		}
		avail = snd_pcm_avail_update(pcm);
		if (avail < 0) {
			err = avail;
			break;
		}
		if ((size_t)avail < pcm->avail_min) {
			if (state != SND_PCM_STATE_RUNNING) {
				err = -EPIPE;
				break;
			}
			if (pcm->mode & SND_PCM_NONBLOCK) {
				err = -EAGAIN;
				break;
			}
			err = snd_pcm_wait(pcm, -1);
			if (err < 0)
				break;
			state = snd_pcm_state(pcm);
			goto again;
		}
		frames = size - xfer;
		if (frames > (size_t)avail)
			frames = avail;
		err = func(pcm, areas, offset, frames, 0);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		xfer += err;
		offset += err;
		if (state == SND_PCM_STATE_PREPARED &&
		    pcm->start_mode != SND_PCM_START_EXPLICIT) {
			err = snd_pcm_start(pcm);
			if (err < 0)
				break;
			state = SND_PCM_STATE_RUNNING;
		}
	}
	if (xfer > 0)
		return xfer;
	return err;
}

#if 0
int snd_pcm_alloc_user_mmap(snd_pcm_t *pcm, snd_pcm_mmap_info_t *i)
{
	i->type = SND_PCM_MMAP_USER;
	i->size = snd_pcm_frames_to_bytes(pcm, pcm->buffer_size);
	i->u.user.shmid = shmget(IPC_PRIVATE, i->size, 0666);
	if (i->u.user.shmid < 0) {
		SYSERR("shmget failed");
		return -errno;
	}
	i->addr = shmat(i->u.user.shmid, 0, 0);
	if (i->addr == (void*) -1) {
		SYSERR("shmat failed");
		return -errno;
	}
	return 0;
}

int snd_pcm_alloc_kernel_mmap(snd_pcm_t *pcm, snd_pcm_mmap_info_t *i, int fd)
{
	i->type = SND_PCM_MMAP_KERNEL;
	/* FIXME */
	i->size = PAGE_ALIGN(snd_pcm_frames_to_bytes(pcm, pcm->buffer_size));
	i->addr = mmap(NULL, i->size,
		       PROT_WRITE | PROT_READ,
		       MAP_FILE|MAP_SHARED, 
		       fd, SND_PCM_MMAP_OFFSET_DATA);
	if (i->addr == MAP_FAILED ||
	    i->addr == NULL) {
		SYSERR("data mmap failed");
		return -errno;
	}
	i->u.kernel.fd = fd;
	return 0;
}

int snd_pcm_free_mmap(snd_pcm_t *pcm, snd_pcm_mmap_info_t *i)
{
	if (i->type == SND_PCM_MMAP_USER) {
		if (shmdt(i->addr) < 0) {
			SYSERR("shmdt failed");
			return -errno;
		}
		if (shmctl(i->u.user.shmid, IPC_RMID, 0) < 0) {
			SYSERR("shmctl IPC_RMID failed");
			return -errno;
		}
	} else {
		if (munmap(pcm->mmap_info->addr, pcm->mmap_info->size) < 0) {
			SYSERR("data munmap failed");
			return -errno;
		}
	}
	return 0;
}
#endif

int snd_pcm_hw_info_bits_per_sample(snd_pcm_hw_info_t *info,
				    unsigned int *min, unsigned int *max)
{
	int k;
	unsigned int bits_min = UINT_MAX, bits_max = 0;
	int changed = 0;
	for (k = 0; k <= SND_PCM_FORMAT_LAST; ++k) {
		int bits;
		if (!(info->format_mask & (1U << k)))
			continue;
		bits = snd_pcm_format_physical_width(k);
		assert(bits > 0);
		if ((unsigned) bits < *min || (unsigned)bits > *max) {
			info->format_mask &= ~(1U << k);
			changed++;
			continue;
		}
		if ((unsigned)bits < bits_min)
			bits_min = bits;
		if ((unsigned)bits > bits_max)
			bits_max = bits;
	}
	*min = bits_min;
	*max = bits_max;
	if (info->format_mask == 0)
		return -EINVAL;
	return changed;
}


int snd_pcm_hw_info_complete(snd_pcm_hw_info_t *info)
{
	if (info->msbits == 0) {
		unsigned int bits_min = 0, bits_max = UINT_MAX;
		snd_pcm_hw_info_bits_per_sample(info, &bits_min, &bits_max);
		if (bits_min == bits_max)
			info->msbits = bits_min;
	}
	if (info->rate_divisor == 0 &&
	    info->rate_min == info->rate_max) {
		info->rate_master = info->rate_min;
		info->rate_divisor = 1;
	}
	return 0;
}

struct {
	unsigned int rate;
	unsigned int flag;
} snd_pcm_rates[] = {
	{ 5512, SND_PCM_RATE_5512 },
	{ 8000, SND_PCM_RATE_8000 },
	{ 11025, SND_PCM_RATE_11025 },
	{ 16000, SND_PCM_RATE_16000 },
	{ 22050, SND_PCM_RATE_22050 },
	{ 32000, SND_PCM_RATE_32000 },
	{ 44100, SND_PCM_RATE_44100 },
	{ 48000, SND_PCM_RATE_48000 },
	{ 64000, SND_PCM_RATE_64000 },
	{ 88200, SND_PCM_RATE_88200 },
	{ 96000, SND_PCM_RATE_96000 },
	{ 176400, SND_PCM_RATE_176400 },
	{ 192000, SND_PCM_RATE_192000 }
};

#define SND_PCM_RATES (sizeof(snd_pcm_rates) / sizeof(snd_pcm_rates[0]))

int snd_pcm_hw_info_rules_access(snd_pcm_t *pcm, 
				 snd_pcm_hw_info_t *info,
				 snd_pcm_hw_params_t *params,
				 unsigned int count, int *rules)
{
	int k;
	unsigned int rel, mask;
	snd_pcm_hw_info_t i;
	rel = *rules & SND_PCM_RULE_REL_MASK;
	switch (rel) {
	case SND_PCM_RULE_REL_LT:
		mask = (1U << params->access) - 1;
		break;
	case SND_PCM_RULE_REL_LE:
		mask = (1U << (params->access + 1)) - 1;
		break;
	case SND_PCM_RULE_REL_GT:
		mask = ~((1U << (params->access + 1)) - 1);
		break;
	case SND_PCM_RULE_REL_GE:
		mask = ~((1U << params->access) - 1);
		break;
	case SND_PCM_RULE_REL_EQ:
		mask = 1U << params->access;
		break;
	case SND_PCM_RULE_REL_NEAR:
	{
		unsigned int diff = 0;
		int n;
		for (diff = 0; diff < 32; ++diff) {
			n = (int)params->access - (int)diff;
			if (n >= 0) {
				unsigned int bit = 1U << n;
				if (info->access_mask & bit) {
					i = *info;
					i.access_mask = bit;
					if (snd_pcm_hw_info(pcm, &i) >= 0 &&
					    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
						*info = i;
						return 0;
					}
				}
			} else if (params->access + diff > SND_PCM_ACCESS_LAST)
				break;
			if (diff == 0)
				continue;
			n = params->access + diff;
			if (n <= SND_PCM_ACCESS_LAST) {
				unsigned int bit = 1U << n;
				if (info->access_mask & bit) {
					i = *info;
					i.access_mask = bit;
					if (snd_pcm_hw_info(pcm, &i) >= 0 &&
					    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
						*info = i;
						return 0;
					}
				}
			}
		}
		info->access_mask = 0;
		return -EINVAL;
	}
	case SND_PCM_RULE_REL_BITS:
		mask = params->access;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	info->access_mask &= mask;
	if (info->access_mask == 0)
		return -EINVAL;
	switch (rel) {
	case SND_PCM_RULE_REL_LE:
	case SND_PCM_RULE_REL_LT:
		for (k = SND_PCM_ACCESS_LAST; k >= 0; --k) {
			if (!(info->access_mask & (1U << k)))
				continue;
			i = *info;
			i.access_mask = 1U << k;
			if (snd_pcm_hw_info(pcm, &i) >= 0 &&
			    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		info->access_mask = 0;
		return -EINVAL;
	default:
		for (k = 0; k <= SND_PCM_ACCESS_LAST; ++k) {
			if (!(info->access_mask & (1U << k)))
				continue;
			i = *info;
			i.access_mask = 1U << k;
			if (snd_pcm_hw_info(pcm, &i) >= 0 &&
			    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		info->access_mask = 0;
		return -EINVAL;
	}		
	return 0;
}

int snd_pcm_hw_info_rules_format(snd_pcm_t *pcm, 
				 snd_pcm_hw_info_t *info,
				 snd_pcm_hw_params_t *params,
				 unsigned int count, int *rules)
{
	int k;
	unsigned int rel, mask;
	snd_pcm_hw_info_t i;
	rel = *rules & SND_PCM_RULE_REL_MASK;
	switch (rel) {
	case SND_PCM_RULE_REL_LT:
		mask = (1U << params->format) - 1;
		break;
	case SND_PCM_RULE_REL_LE:
		mask = (1U << (params->format + 1)) - 1;
		break;
	case SND_PCM_RULE_REL_GT:
		mask = ~((1U << (params->format + 1)) - 1);
		break;
	case SND_PCM_RULE_REL_GE:
		mask = ~((1U << params->format) - 1);
		break;
	case SND_PCM_RULE_REL_EQ:
		mask = 1U << params->format;
		break;
	case SND_PCM_RULE_REL_NEAR:
	{
		unsigned int diff = 0;
		int n;
		for (diff = 0; diff < 32; ++diff) {
			n = (int)params->format - (int)diff;
			if (n >= 0) {
				unsigned int bit = 1U << n;
				if (info->format_mask & bit) {
					i = *info;
					i.format_mask = bit;
					if (snd_pcm_hw_info(pcm, &i) >= 0 &&
					    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
						*info = i;
						return 0;
					}
				}
			} else if (params->format + diff > SND_PCM_FORMAT_LAST)
				break;
			if (diff == 0)
				continue;
			n = params->format + diff;
			if (n <= SND_PCM_FORMAT_LAST) {
				unsigned int bit = 1U << n;
				if (info->format_mask & bit) {
					i = *info;
					i.format_mask = bit;
					if (snd_pcm_hw_info(pcm, &i) >= 0 &&
					    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
						*info = i;
						return 0;
					}
				}
			}
		}
		info->format_mask = 0;
		return -EINVAL;
	}
	case SND_PCM_RULE_REL_BITS:
		mask = params->format;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	info->format_mask &= mask;
	if (info->format_mask == 0)
		return -EINVAL;
	switch (rel) {
	case SND_PCM_RULE_REL_LE:
	case SND_PCM_RULE_REL_LT:
		for (k = SND_PCM_FORMAT_LAST; k >= 0; --k) {
			if (!(info->format_mask & (1U << k)))
				continue;
			i = *info;
			i.format_mask = 1U << k;
			if (snd_pcm_hw_info(pcm, &i) >= 0 &&
			    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		info->format_mask = 0;
		return -EINVAL;
	default:
		for (k = 0; k <= SND_PCM_FORMAT_LAST; ++k) {
			if (!(info->format_mask & (1U << k)))
				continue;
			i = *info;
			i.format_mask = 1U << k;
			if (snd_pcm_hw_info(pcm, &i) >= 0 &&
			    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		info->format_mask = 0;
		return -EINVAL;
	}		
	return 0;
}

int snd_pcm_hw_info_rules_subformat(snd_pcm_t *pcm, 
				    snd_pcm_hw_info_t *info,
				    snd_pcm_hw_params_t *params,
				    unsigned int count, int *rules)
{
	int k;
	unsigned int rel, mask;
	snd_pcm_hw_info_t i;
	rel = *rules & SND_PCM_RULE_REL_MASK;
	switch (rel) {
	case SND_PCM_RULE_REL_LT:
		mask = (1U << params->subformat) - 1;
		break;
	case SND_PCM_RULE_REL_LE:
		mask = (1U << (params->subformat + 1)) - 1;
		break;
	case SND_PCM_RULE_REL_GT:
		mask = ~((1U << (params->subformat + 1)) - 1);
		break;
	case SND_PCM_RULE_REL_GE:
		mask = ~((1U << params->subformat) - 1);
		break;
	case SND_PCM_RULE_REL_EQ:
		mask = 1U << params->subformat;
		break;
	case SND_PCM_RULE_REL_NEAR:
	{
		unsigned int diff = 0;
		int n;
		for (diff = 0; diff < 32; ++diff) {
			n = (int)params->subformat - (int)diff;
			if (n >= 0) {
				unsigned int bit = 1U << n;
				if (info->subformat_mask & bit) {
					i = *info;
					i.subformat_mask = bit;
					if (snd_pcm_hw_info(pcm, &i) >= 0 &&
					    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
						*info = i;
						return 0;
					}
				}
			} else if (params->subformat + diff > SND_PCM_SUBFORMAT_LAST)
				break;
			if (diff == 0)
				continue;
			n = params->subformat + diff;
			if (n <= SND_PCM_SUBFORMAT_LAST) {
				unsigned int bit = 1U << n;
				if (info->subformat_mask & bit) {
					i = *info;
					i.subformat_mask = bit;
					if (snd_pcm_hw_info(pcm, &i) >= 0 &&
					    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
						*info = i;
						return 0;
					}
				}
			}
		}
		info->subformat_mask = 0;
		return -EINVAL;
	}
	case SND_PCM_RULE_REL_BITS:
		mask = params->subformat;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	info->subformat_mask &= mask;
	if (info->subformat_mask == 0)
		return -EINVAL;
	switch (rel) {
	case SND_PCM_RULE_REL_LE:
	case SND_PCM_RULE_REL_LT:
		for (k = SND_PCM_SUBFORMAT_LAST; k >= 0; --k) {
			if (!(info->subformat_mask & (1U << k)))
				continue;
			i = *info;
			i.subformat_mask = 1U << k;
			if (snd_pcm_hw_info(pcm, &i) >= 0 &&
			    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		info->subformat_mask = 0;
		return -EINVAL;
	default:
		for (k = 0; k <= SND_PCM_SUBFORMAT_LAST; ++k) {
			if (!(info->subformat_mask & (1U << k)))
				continue;
			i = *info;
			i.subformat_mask = 1U << k;
			if (snd_pcm_hw_info(pcm, &i) >= 0 &&
			    snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		info->subformat_mask = 0;
		return -EINVAL;
	}		
	return 0;
}

int snd_pcm_hw_info_rules_channels(snd_pcm_t *pcm, 
				   snd_pcm_hw_info_t *info,
				   snd_pcm_hw_params_t *params,
				   unsigned int count, int *rules)
{
	int err;
	unsigned int rel;
	snd_pcm_hw_info_t i;
	rel = *rules & SND_PCM_RULE_REL_MASK;
	switch (rel) {
	case SND_PCM_RULE_REL_LT:
		if (info->channels_max > params->channels - 1)
			info->channels_max = params->channels - 1;
		goto _le;
	case SND_PCM_RULE_REL_LE:
		if (info->channels_max > params->channels)
			info->channels_max = params->channels;
	_le:
		while (1) {
			if (info->channels_min > info->channels_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->channels_min = i.channels_min;
				info->channels_max = i.channels_max;
				return err;
			}
			i.channels_min = i.channels_max;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			info->channels_max--;
		}
		break;
	case SND_PCM_RULE_REL_GT:
		if (info->channels_min < params->channels + 1)
			info->channels_min = params->channels + 1;
		goto _ge;
	case SND_PCM_RULE_REL_GE:
		if (info->channels_min < params->channels)
			info->channels_min = params->channels;
	_ge:
		while (1) {
			if (info->channels_min > info->channels_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->channels_min = i.channels_min;
				info->channels_max = i.channels_max;
				return err;
			}
			i.channels_max = i.channels_min;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			info->channels_min++;
		}
		break;
	case SND_PCM_RULE_REL_EQ:
		info->channels_min = params->channels;
		info->channels_max = params->channels;
		return snd_pcm_hw_info_rules(pcm, info, params, count - 1, rules + 1);
		break;
	case SND_PCM_RULE_REL_NEAR:
	{
		unsigned int max1, min2;
		int err1 = -EINVAL, err2 = -EINVAL;
		max1 = params->channels;
		min2 = params->channels+1;
		if (info->channels_min <= max1) {
			i = *info;
			i.channels_max = max1;
			err1 = snd_pcm_hw_info(pcm, &i);
			/* shortcut for common case */
			if (err1 >= 0 && max1 == i.channels_max) {
				i.channels_min = max1;
				if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
					*info = i;
					return 0;
				}
				i.channels_max = max1 - 1;
				err1 = snd_pcm_hw_info(pcm, &i);
			}
			max1 = i.channels_max;
		}
		if (min2 <= info->channels_max) {
			i = *info;
			i.channels_min = min2;
			err2 = snd_pcm_hw_info(pcm, &i);
			min2 = i.channels_min;
		}
		while (1) {
			unsigned int channels;
			if (err1 >= 0) {
				if (err2 >= 0) {
					if (params->channels - max1 < 
					    min2 - params->channels)
						channels = max1;
					else
						channels = min2;
				} else
					channels = max1;
			} else if (err2 >= 0)
				channels = min2;
			else {
				info->channels_min = UINT_MAX;
				info->channels_max = 0;
				return -EINVAL;
			}
			i = *info;
			i.channels_min = i.channels_max = channels;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			if (channels == max1) {
				max1--;
				i = *info;
				i.channels_max = max1;
				err1 = snd_pcm_hw_info(pcm, &i);
				max1 = i.channels_max;
			} else {
				min2++;
				i = *info;
				i.channels_min = min2;
				err2 = snd_pcm_hw_info(pcm, &i);
				min2 = i.channels_min;
			}
				
		}
		break;
	}
	case SND_PCM_RULE_REL_BITS:
	{
		unsigned int k;
		for (k = info->channels_min; k < 32; ++k) {
			if (!(params->channels & (1U << k)))
				continue;
			info->channels_min = k;
			if (info->channels_min > info->channels_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->channels_min = i.channels_min;
				info->channels_max = i.channels_max;
				return err;
			}
			i.channels_max = i.channels_min;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		break;
	}
	default:
		assert(0);
	        return -EINVAL;
	}
	return 0;
}

int snd_pcm_hw_info_rules_rate(snd_pcm_t *pcm, 
			       snd_pcm_hw_info_t *info,
			       snd_pcm_hw_params_t *params,
			       unsigned int count, int *rules)
{
	int err;
	unsigned int rel;
	snd_pcm_hw_info_t i;
	rel = *rules & SND_PCM_RULE_REL_MASK;
	switch (rel) {
	case SND_PCM_RULE_REL_LT:
		if (info->rate_max > params->rate - 1)
			info->rate_max = params->rate - 1;
		goto _le;
	case SND_PCM_RULE_REL_LE:
		if (info->rate_max > params->rate)
			info->rate_max = params->rate;
	_le:
		while (1) {
			if (info->rate_min > info->rate_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->rate_min = i.rate_min;
				info->rate_max = i.rate_max;
				return err;
			}
			i.rate_min = i.rate_max;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			info->rate_max--;
		}
		break;
	case SND_PCM_RULE_REL_GT:
		if (info->rate_min < params->rate + 1)
			info->rate_min = params->rate + 1;
		goto _ge;
	case SND_PCM_RULE_REL_GE:
		if (info->rate_min < params->rate)
			info->rate_min = params->rate;
	_ge:
		while (1) {
			if (info->rate_min > info->rate_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->rate_min = i.rate_min;
				info->rate_max = i.rate_max;
				return err;
			}
			i.rate_max = i.rate_min;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			info->rate_min++;
		}
		break;
	case SND_PCM_RULE_REL_EQ:
		info->rate_min = params->rate;
		info->rate_max = params->rate;
		return snd_pcm_hw_info_rules(pcm, info, params, count - 1, rules + 1);
		break;
	case SND_PCM_RULE_REL_NEAR:
	{
		unsigned int max1, min2;
		int err1 = -EINVAL, err2 = -EINVAL;
		max1 = params->rate;
		min2 = params->rate+1;
		if (info->rate_min <= max1) {
			i = *info;
			i.rate_max = max1;
			err1 = snd_pcm_hw_info(pcm, &i);
			/* shortcut for common case */
			if (err1 >= 0 && max1 == i.rate_max) {
				i.rate_min = max1;
				if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
					*info = i;
					return 0;
				}
				i.rate_max = max1 - 1;
				err1 = snd_pcm_hw_info(pcm, &i);
			}
			max1 = i.rate_max;
		}
		if (min2 <= info->rate_max) {
			i = *info;
			i.rate_min = min2;
			err2 = snd_pcm_hw_info(pcm, &i);
			min2 = i.rate_min;
		}
		while (1) {
			unsigned int rate;
			if (err1 >= 0) {
				if (err2 >= 0) {
					if (params->rate - max1 < 
					    min2 - params->rate)
						rate = max1;
					else
						rate = min2;
				} else
					rate = max1;
			} else if (err2 >= 0)
				rate = min2;
			else {
				info->rate_min = UINT_MAX;
				info->rate_max = 0;
				return -EINVAL;
			}
			i = *info;
			i.rate_min = i.rate_max = rate;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			if (rate == max1) {
				max1--;
				i = *info;
				i.rate_max = max1;
				err1 = snd_pcm_hw_info(pcm, &i);
				max1 = i.rate_max;
			} else {
				min2++;
				i = *info;
				i.rate_min = min2;
				err2 = snd_pcm_hw_info(pcm, &i);
				min2 = i.rate_min;
			}
				
		}
		break;
	}
	case SND_PCM_RULE_REL_BITS:
	{
		unsigned int k;
		for (k = 0; k < SND_PCM_RATES; ++k) {
			if (snd_pcm_rates[k].rate < info->rate_min)
				continue;
			if (!(params->rate & snd_pcm_rates[k].flag))
				continue;
			info->rate_min = snd_pcm_rates[k].rate;
			if (info->rate_min > info->rate_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->rate_min = i.rate_min;
				info->rate_max = i.rate_max;
				return err;
			}
			i.rate_max = i.rate_min;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		break;
	}
	default:
		assert(0);
	        return -EINVAL;
	}
	return 0;
}

int snd_pcm_hw_info_rules_fragment_size(snd_pcm_t *pcm, 
					snd_pcm_hw_info_t *info,
					snd_pcm_hw_params_t *params,
					unsigned int count, int *rules)
{
	int err;
	unsigned int rel;
	snd_pcm_hw_info_t i;
	rel = *rules & SND_PCM_RULE_REL_MASK;
	switch (rel) {
	case SND_PCM_RULE_REL_LT:
		if (info->fragment_size_max > params->fragment_size - 1)
			info->fragment_size_max = params->fragment_size - 1;
		goto _le;
	case SND_PCM_RULE_REL_LE:
		if (info->fragment_size_max > params->fragment_size)
			info->fragment_size_max = params->fragment_size;
	_le:
		while (1) {
			if (info->fragment_size_min > info->fragment_size_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->fragment_size_min = i.fragment_size_min;
				info->fragment_size_max = i.fragment_size_max;
				return err;
			}
			i.fragment_size_min = i.fragment_size_max;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			info->fragment_size_max--;
		}
		break;
	case SND_PCM_RULE_REL_GT:
		if (info->fragment_size_min < params->fragment_size + 1)
			info->fragment_size_min = params->fragment_size + 1;
		goto _ge;
	case SND_PCM_RULE_REL_GE:
		if (info->fragment_size_min < params->fragment_size)
			info->fragment_size_min = params->fragment_size;
	_ge:
		while (1) {
			if (info->fragment_size_min > info->fragment_size_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->fragment_size_min = i.fragment_size_min;
				info->fragment_size_max = i.fragment_size_max;
				return err;
			}
			i.fragment_size_max = i.fragment_size_min;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			info->fragment_size_min++;
		}
		break;
	case SND_PCM_RULE_REL_EQ:
		info->fragment_size_min = params->fragment_size;
		info->fragment_size_max = params->fragment_size;
		return snd_pcm_hw_info_rules(pcm, info, params, count - 1, rules + 1);
		break;
	case SND_PCM_RULE_REL_NEAR:
	{
		size_t max1, min2;
		int err1 = -EINVAL, err2 = -EINVAL;
		max1 = params->fragment_size;
		min2 = params->fragment_size+1;
		if (info->fragment_size_min <= max1) {
			i = *info;
			i.fragment_size_max = max1;
			err1 = snd_pcm_hw_info(pcm, &i);
			/* shortcut for common case */
			if (err1 >= 0 && max1 == i.fragment_size_max) {
				i.fragment_size_min = max1;
				if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
					*info = i;
					return 0;
				}
				i.fragment_size_max = max1 - 1;
				err1 = snd_pcm_hw_info(pcm, &i);
			}
			max1 = i.fragment_size_max;
		}
		if (min2 <= info->fragment_size_max) {
			i = *info;
			i.fragment_size_min = min2;
			err2 = snd_pcm_hw_info(pcm, &i);
			min2 = i.fragment_size_min;
		}
		while (1) {
			size_t fragment_size;
			if (err1 >= 0) {
				if (err2 >= 0) {
					if (params->fragment_size - max1 < 
					    min2 - params->fragment_size)
						fragment_size = max1;
					else
						fragment_size = min2;
				} else
					fragment_size = max1;
			} else if (err2 >= 0)
				fragment_size = min2;
			else {
				info->fragment_size_min = ULONG_MAX;
				info->fragment_size_max = 0;
				return -EINVAL;
			}
			i = *info;
			i.fragment_size_min = i.fragment_size_max = fragment_size;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			if (fragment_size == max1) {
				max1--;
				i = *info;
				i.fragment_size_max = max1;
				err1 = snd_pcm_hw_info(pcm, &i);
				max1 = i.fragment_size_max;
			} else {
				min2++;
				i = *info;
				i.fragment_size_min = min2;
				err2 = snd_pcm_hw_info(pcm, &i);
				min2 = i.fragment_size_min;
			}
				
		}
		break;
	}
	case SND_PCM_RULE_REL_BITS:
	{
		unsigned int k;
		for (k = info->fragment_size_min; k < 32; ++k) {
			if (!(params->fragment_size & (1U << k)))
				continue;
			info->fragment_size_min = 1U << k;
			if (info->fragment_size_min > info->fragment_size_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->fragment_size_min = i.fragment_size_min;
				info->fragment_size_max = i.fragment_size_max;
				return err;
			}
			i.fragment_size_max = i.fragment_size_min;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		break;
	}
	default:
		assert(0);
		return -EINVAL;
	}
	return 0;
}

int snd_pcm_hw_info_rules_fragments(snd_pcm_t *pcm, 
				    snd_pcm_hw_info_t *info,
				    snd_pcm_hw_params_t *params,
				    unsigned int count, int *rules)
{
	int err;
	unsigned int rel;
	snd_pcm_hw_info_t i;
	rel = *rules & SND_PCM_RULE_REL_MASK;
	switch (rel) {
	case SND_PCM_RULE_REL_LT:
		if (info->fragments_max > params->fragments - 1)
			info->fragments_max = params->fragments - 1;
		goto _le;
	case SND_PCM_RULE_REL_LE:
		if (info->fragments_max > params->fragments)
			info->fragments_max = params->fragments;
	_le:
		while (1) {
			if (info->fragments_min > info->fragments_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->fragments_min = i.fragments_min;
				info->fragments_max = i.fragments_max;
				return err;
			}
			i.fragments_min = i.fragments_max;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			info->fragments_max--;
		}
		break;
	case SND_PCM_RULE_REL_GT:
		if (info->fragments_min < params->fragments + 1)
			info->fragments_min = params->fragments + 1;
		goto _ge;
	case SND_PCM_RULE_REL_GE:
		if (info->fragments_min < params->fragments)
			info->fragments_min = params->fragments;
	_ge:
		while (1) {
			if (info->fragments_min > info->fragments_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->fragments_min = i.fragments_min;
				info->fragments_max = i.fragments_max;
				return err;
			}
			i.fragments_max = i.fragments_min;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			info->fragments_min++;
		}
		break;
	case SND_PCM_RULE_REL_EQ:
		info->fragments_min = params->fragments;
		info->fragments_max = params->fragments;
		return snd_pcm_hw_info_rules(pcm, info, params, count - 1, rules + 1);
		break;
	case SND_PCM_RULE_REL_NEAR:
	{
		unsigned int max1, min2;
		int err1 = -EINVAL, err2 = -EINVAL;
		max1 = params->fragments;
		min2 = params->fragments+1;
		if (info->fragments_min <= max1) {
			i = *info;
			i.fragments_max = max1;
			err1 = snd_pcm_hw_info(pcm, &i);
			/* shortcut for common case */
			if (err1 >= 0 && max1 == i.fragments_max) {
				i.fragments_min = max1;
				if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
					*info = i;
					return 0;
				}
				i.fragments_max = max1 - 1;
				err1 = snd_pcm_hw_info(pcm, &i);
			}
			max1 = i.fragments_max;
		}
		if (min2 <= info->fragments_max) {
			i = *info;
			i.fragments_min = min2;
			err2 = snd_pcm_hw_info(pcm, &i);
			min2 = i.fragments_min;
		}
		while (1) {
			unsigned int fragments;
			if (err1 >= 0) {
				if (err2 >= 0) {
					if (params->fragments - max1 < 
					    min2 - params->fragments)
						fragments = max1;
					else
						fragments = min2;
				} else
					fragments = max1;
			} else if (err2 >= 0)
				fragments = min2;
			else {
				info->fragments_min = UINT_MAX;
				info->fragments_max = 0;
				return -EINVAL;
			}
			i = *info;
			i.fragments_min = i.fragments_max = fragments;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
			if (fragments == max1) {
				max1--;
				i = *info;
				i.fragments_max = max1;
				err1 = snd_pcm_hw_info(pcm, &i);
				max1 = i.fragments_max;
			} else {
				min2++;
				i = *info;
				i.fragments_min = min2;
				err2 = snd_pcm_hw_info(pcm, &i);
				min2 = i.fragments_min;
			}
				
		}
		break;
	}
	case SND_PCM_RULE_REL_BITS:
	{
		unsigned int k;
		for (k = info->fragments_min; k < 32; ++k) {
			if (!(params->fragments & (1U << k)))
				continue;
			info->fragments_min = k;
			if (info->fragments_min > info->fragments_max)
				return -EINVAL;
			i = *info;
			err = snd_pcm_hw_info(pcm, &i);
			if (err < 0) {
				info->fragments_min = i.fragments_min;
				info->fragments_max = i.fragments_max;
				return err;
			}
			i.fragments_max = i.fragments_min;
			if (snd_pcm_hw_info_rules(pcm, &i, params, count - 1, rules + 1) >= 0) {
				*info = i;
				return 0;
			}
		}
		break;
	}
	default:
		assert(0);
		return -EINVAL;
	}
	return 0;
}

int snd_pcm_hw_info_rules(snd_pcm_t *pcm, 
			  snd_pcm_hw_info_t *info,
			  snd_pcm_hw_params_t *params,
			  unsigned int count, int *rules)
{
	unsigned int par;
	if (count == 0)
		return snd_pcm_hw_info(pcm, info);
	par = rules[0] & SND_PCM_RULE_PAR_MASK;
	switch (par) {
	case SND_PCM_HW_PARAM_ACCESS:
		return snd_pcm_hw_info_rules_access(pcm, info, params, count, rules);
	case SND_PCM_HW_PARAM_FORMAT:
		return snd_pcm_hw_info_rules_format(pcm, info, params, count, rules);
	case SND_PCM_HW_PARAM_SUBFORMAT:
		return snd_pcm_hw_info_rules_subformat(pcm, info, params, count, rules);
	case SND_PCM_HW_PARAM_CHANNELS:
		return snd_pcm_hw_info_rules_channels(pcm, info, params, count, rules);
	case SND_PCM_HW_PARAM_RATE:
		return snd_pcm_hw_info_rules_rate(pcm, info, params, count, rules);
	case SND_PCM_HW_PARAM_FRAGMENT_SIZE:
		return snd_pcm_hw_info_rules_fragment_size(pcm, info, params, count, rules);
	case SND_PCM_HW_PARAM_FRAGMENTS:
		return snd_pcm_hw_info_rules_fragments(pcm, info, params, count, rules);
	default:
		assert(0);
		return -EINVAL;
	}
}

int snd_pcm_hw_info_rulesv(snd_pcm_t *pcm, 
			   snd_pcm_hw_info_t *info,
			   snd_pcm_hw_params_t *params, ...)
{
	va_list arg;
	unsigned int count = 0;
	int rules[32];
	va_start(arg, params);
	while (1) {
		int i = va_arg(arg, int);
		if (i == -1)
			break;
		rules[count++] = i;
	}
	va_end(arg);
	return snd_pcm_hw_info_rules(pcm, info, params, count, rules);
}

int snd_pcm_hw_params_rules(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			    unsigned int count, int *rules)
{
	int err;
	snd_pcm_hw_info_t info;
	unsigned int k;
	snd_pcm_hw_params_to_info(params, &info);
	for (k = 0; k < count; ++k) {
		switch (rules[k] & SND_PCM_RULE_PAR_MASK) {
		case SND_PCM_HW_PARAM_ACCESS:
			info.access_mask = ~0;
			break;
		case SND_PCM_HW_PARAM_FORMAT:
			info.format_mask = ~0;
			break;
		case SND_PCM_HW_PARAM_SUBFORMAT:
			info.subformat_mask = ~0;
			break;
		case SND_PCM_HW_PARAM_CHANNELS:
			info.channels_min = 1;
			info.channels_max = UINT_MAX;
			break;
		case SND_PCM_HW_PARAM_RATE:
			info.rate_min = 0;
			info.rate_max = UINT_MAX;
			break;
		case SND_PCM_HW_PARAM_FRAGMENT_SIZE:
			info.fragment_size_min = 1;
			info.fragment_size_max = ULONG_MAX;
			info.buffer_size_min = 1;
			info.buffer_size_max = ULONG_MAX;
			break;
		case SND_PCM_HW_PARAM_FRAGMENTS:
			info.fragments_min = 1;
			info.fragments_max = UINT_MAX;
			info.buffer_size_min = 1;
			info.buffer_size_max = ULONG_MAX;
			break;
		default:
			assert(0);
			break;
		}
	}
	err = snd_pcm_hw_info_rules(pcm, &info, params, count, rules);
	if (err < 0) {
		snd_pcm_hw_info_to_params_fail(&info, params);
		return err;
	}
	snd_pcm_hw_info_to_params(&info, params);
	return snd_pcm_hw_params(pcm, params);
}

int snd_pcm_hw_params_rulesv(snd_pcm_t *pcm, 
			     snd_pcm_hw_params_t *params, ...)
{
	va_list arg;
	unsigned int count = 0;
	int rules[32];
	va_start(arg, params);
	while (1) {
		int i = va_arg(arg, int);
		if (i == -1)
			break;
		rules[count++] = i;
	}
	va_end(arg);
	return snd_pcm_hw_params_rules(pcm, params, count, rules);
}

struct _snd_pcm_strategy {
	int (*choose_param)(const snd_pcm_hw_info_t *info,
			    snd_pcm_t *pcm,
			    const snd_pcm_strategy_t *strategy);
	long (*next_value)(const snd_pcm_hw_info_t *info,
			   unsigned int param,
			   long value,
			   snd_pcm_t *pcm,
			   const snd_pcm_strategy_t *strategy);
	int (*min_badness)(const snd_pcm_hw_info_t *info,
			   unsigned int max_badness,
			   snd_pcm_t *pcm,
			   const snd_pcm_strategy_t *strategy);
	void *private;
	void (*free)(snd_pcm_strategy_t *strategy);
};

/* Independent badness */
typedef struct _snd_pcm_strategy_simple snd_pcm_strategy_simple_t;

struct _snd_pcm_strategy_simple {
	int valid;
	long (*next_value)(const snd_pcm_hw_info_t *info,
			   unsigned int param,
			   long value,
			   snd_pcm_t *pcm,
			   const snd_pcm_strategy_simple_t *par);
	unsigned int (*min_badness)(const snd_pcm_hw_info_t *info,
				    unsigned int param,
				    snd_pcm_t *pcm,
				    const snd_pcm_strategy_simple_t *par);
	void *private;
	void (*free)(snd_pcm_strategy_simple_t *strategy);
};

typedef struct _snd_pcm_strategy_simple_near {
	long best;
	unsigned int mul;
} snd_pcm_strategy_simple_near_t;

typedef struct _snd_pcm_strategy_simple_choices {
	unsigned int count;
	/* choices need to be sorted on ascending badness */
	snd_pcm_strategy_simple_choices_list_t *choices;
} snd_pcm_strategy_simple_choices_t;

static inline unsigned int hweight32(u_int32_t v)
{
        v = (v & 0x55555555) + ((v >> 1) & 0x55555555);
        v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
        v = (v & 0x0F0F0F0F) + ((v >> 4) & 0x0F0F0F0F);
        v = (v & 0x00FF00FF) + ((v >> 8) & 0x00FF00FF);
        return (v & 0x0000FFFF) + ((v >> 16) & 0x0000FFFF);
}

static inline unsigned int ld2(u_int32_t v)
{
        unsigned r = 0;

        if (v >= 0x10000) {
                v >>= 16;
                r += 16;
        }
        if (v >= 0x100) {
                v >>= 8;
                r += 8;
        }
        if (v >= 0x10) {
                v >>= 4;
                r += 4;
        }
        if (v >= 4) {
                v >>= 2;
                r += 2;
        }
        if (v >= 2)
                r++;
        return r;
}


static unsigned long par_choices(const snd_pcm_hw_info_t *info, unsigned int param)
{
	switch (param) {
	case SND_PCM_HW_PARAM_ACCESS:
		return hweight32(info->access_mask);
	case SND_PCM_HW_PARAM_FORMAT:
		return hweight32(info->format_mask);
	case SND_PCM_HW_PARAM_SUBFORMAT:
		return hweight32(info->subformat_mask);
	case SND_PCM_HW_PARAM_CHANNELS:
		return info->channels_max - info->channels_min + 1;
	case SND_PCM_HW_PARAM_RATE:
		return info->rate_max - info->rate_min + 1;
	case SND_PCM_HW_PARAM_FRAGMENT_SIZE:
		return info->fragment_size_max - info->fragment_size_min + 1;
	case SND_PCM_HW_PARAM_FRAGMENTS:
		return info->fragments_max - info->fragments_min + 1;
	case SND_PCM_HW_PARAM_BUFFER_SIZE:
		return info->buffer_size_max - info->buffer_size_min + 1;
	default:
		assert(0);
		return 0;
	}
}

static unsigned long par_refine_min(snd_pcm_hw_info_t *info,
				    unsigned int param,
				    unsigned long value)
{
	int i;
	switch (param) {
	case SND_PCM_HW_PARAM_ACCESS:
		if (value >= 32) {
			info->access_mask = 0;
			return 32;
		} else
			info->access_mask &= ~((1 << value) - 1);
		i = ffs(info->access_mask);
		if (i == 0)
			return 32;
		return i - 1;
	case SND_PCM_HW_PARAM_FORMAT:
		if (value >= 32) {
			info->format_mask = 0;
			return 32;
		} else
			info->format_mask &= ~((1 << value) - 1);
		i = ffs(info->format_mask);
		if (i == 0)
			return 32;
		return i - 1;
	case SND_PCM_HW_PARAM_SUBFORMAT:
		if (value >= 32) {
			info->subformat_mask = 0;
			return 32;
		} else
			info->subformat_mask &= ~((1 << value) - 1);
		i = ffs(info->subformat_mask);
		if (i == 0)
			return 32;
		return i - 1;
	case SND_PCM_HW_PARAM_CHANNELS:
		if (value > info->channels_min)
			info->channels_min = value;
		return info->channels_min;
	case SND_PCM_HW_PARAM_RATE:
		if (value > info->rate_min)
			info->rate_min = value;
		return info->rate_min;
	case SND_PCM_HW_PARAM_FRAGMENT_SIZE:
		if (value > info->fragment_size_min)
			info->fragment_size_min = value;
		return info->fragment_size_min;
	case SND_PCM_HW_PARAM_FRAGMENTS:
		if (value > info->fragments_min)
			info->fragments_min = value;
		return info->fragments_min;
	case SND_PCM_HW_PARAM_BUFFER_SIZE:
		if (value > info->buffer_size_min)
			info->buffer_size_min = value;
		return info->buffer_size_min;
	default:
		assert(0);
		return 0;
	}
}

static unsigned long par_refine_max(snd_pcm_hw_info_t *info,
				    unsigned int param,
				    unsigned long value)
{
	switch (param) {
	case SND_PCM_HW_PARAM_ACCESS:
		if (value < 31)
			info->access_mask &= (1 << (value + 1)) - 1;
		return ld2(info->access_mask);
	case SND_PCM_HW_PARAM_FORMAT:
		if (value < 31)
			info->format_mask &= (1 << (value + 1)) - 1;
		return ld2(info->format_mask);
	case SND_PCM_HW_PARAM_SUBFORMAT:
		if (value < 31)
			info->subformat_mask &= (1 << (value + 1)) - 1;
		return ld2(info->subformat_mask);
	case SND_PCM_HW_PARAM_CHANNELS:
		if (value < info->channels_max)
			info->channels_max = value;
		return info->channels_max;
	case SND_PCM_HW_PARAM_RATE:
		if (value < info->rate_max)
			info->rate_max = value;
		return info->rate_max;
	case SND_PCM_HW_PARAM_FRAGMENT_SIZE:
		if (value < info->fragment_size_max)
			info->fragment_size_max = value;
		return info->fragment_size_max;
	case SND_PCM_HW_PARAM_FRAGMENTS:
		if (value < info->fragments_max)
			info->fragments_max = value;
		return info->fragments_max;
	case SND_PCM_HW_PARAM_BUFFER_SIZE:
		if (value < info->buffer_size_max)
			info->buffer_size_max = value;
		return info->buffer_size_max;
	default:
		assert(0);
		return 0;
	}
}

static void par_set(snd_pcm_hw_info_t *info, unsigned int param,
		    unsigned long value)
{
	switch (param) {
	case SND_PCM_HW_PARAM_ACCESS:
		info->access_mask = 1 << value;
		break;
	case SND_PCM_HW_PARAM_FORMAT:
		info->format_mask = 1 << value;
		break;
	case SND_PCM_HW_PARAM_SUBFORMAT:
		info->subformat_mask = 1 << value;
		break;
	case SND_PCM_HW_PARAM_CHANNELS:
		info->channels_min = info->channels_max = value;
		break;
	case SND_PCM_HW_PARAM_RATE:
		info->rate_min = info->rate_max = value;
		break;
	case SND_PCM_HW_PARAM_FRAGMENT_SIZE:
		info->fragment_size_min = info->fragment_size_max = value;
		break;
	case SND_PCM_HW_PARAM_FRAGMENTS:
		info->fragments_min = info->fragments_max = value;
		break;
	case SND_PCM_HW_PARAM_BUFFER_SIZE:
		info->buffer_size_min = info->buffer_size_max = value;
		break;
	default:
		assert(0);
		break;
	}
}

static int par_check(const snd_pcm_hw_info_t *info, unsigned int param,
		     unsigned long value)
{
	switch (param) {
	case SND_PCM_HW_PARAM_ACCESS:
		return info->access_mask & (1 << value);
	case SND_PCM_HW_PARAM_FORMAT:
		return info->format_mask & (1 << value);
	case SND_PCM_HW_PARAM_SUBFORMAT:
		return info->subformat_mask & (1 << value);
	case SND_PCM_HW_PARAM_CHANNELS:
		return value >= info->channels_min && 
			value <= info->channels_max;
	case SND_PCM_HW_PARAM_RATE:
		return value >= info->rate_min && 
			value <= info->rate_max;
	case SND_PCM_HW_PARAM_FRAGMENT_SIZE:
		return value >= info->fragment_size_min && 
			value <= info->fragment_size_max;
	case SND_PCM_HW_PARAM_FRAGMENTS:
		return value >= info->fragments_min && 
			value <= info->fragments_max;
	case SND_PCM_HW_PARAM_BUFFER_SIZE:
		return value >= info->buffer_size_min && 
			value <= info->buffer_size_max;
	default:
		assert(0);
		return 0;
	}
}

static long par_nearest_next(const snd_pcm_hw_info_t *info, unsigned int param,
			     unsigned long best, long value, snd_pcm_t *pcm)
{
	unsigned long min, max;
	unsigned long d1, d2;
	unsigned long max1, min2;
	snd_pcm_hw_info_t i1, i2;
	int err1 = -EINVAL;
	int err2 = -EINVAL;
	
	i1 = *info;
	i2 = *info;
	max = par_refine_max(&i1, param, ULONG_MAX);
	min = par_refine_min(&i2, param, 0);
	if (value < 0) {
		d1 = 0;
		d2 = 0;
	} else {
		long diff = value - best;
		if (diff < 0) {
			d1 = -diff + 1;
			d2 = -diff;
		} else {
			d1 = diff + 1;
			d2 = diff + 1;
		}
	}
	if (best > d1)
		max1 = best - d1;
	else
		max1 = 0;
	min2 = best + d2;
	max1 = par_refine_max(&i1, param, max1);
	min2 = par_refine_min(&i2, param, min2);
	if (min <= max1) {
		err1 = snd_pcm_hw_info(pcm, &i1);
		if (err1 >= 0)
			max1 = par_refine_max(&i1, param, max1);
	}
	if (min2 <= max && (err1 < 0 || best - max1 > min2 - best)) {
		err2 = snd_pcm_hw_info(pcm, &i2);
		if (err2 >= 0)
			min2 = par_refine_min(&i2, param, min2);
	}
	if (err1 < 0) {
		if (err2 < 0)
			return -1;
		return min2;
	} else if (err2 < 0)
		return max1;
	if (best - max1 <= min2 - best)
		return max1;
	return min2;
}

int snd_pcm_hw_info_strategy1(snd_pcm_t *pcm, snd_pcm_hw_info_t *info,
			      const snd_pcm_strategy_t *strategy,
			      unsigned int min_badness, unsigned int max_badness)
{
	snd_pcm_hw_info_t best_info;
	int param;
	long value;
	unsigned int best_badness;
	int badness;
	badness = strategy->min_badness(info, max_badness, pcm, strategy);
#if 0
	printf("\nBadness: %d\n", badness);
	snd_pcm_dump_hw_info(info, stdout);
#endif
	if (badness < 0)
		return -EINVAL;
	if ((unsigned int)badness > min_badness)
		min_badness = badness;
	param = strategy->choose_param(info, pcm, strategy);
	if (param < 0)
		return badness;
	best_badness = UINT_MAX;
	value = -1;
	while (1) {
		snd_pcm_hw_info_t info1;
		int err;
		value = strategy->next_value(info, param, value, pcm, strategy);
		if (value < 0)
			break;
		info1 = *info;
		par_set(&info1, param, value);
		err = snd_pcm_hw_info(pcm, &info1);
		if (err < 0)
			continue;
		badness = snd_pcm_hw_info_strategy1(pcm, &info1, strategy, min_badness, max_badness);
		if (badness < 0)
			continue;
		if ((unsigned int) badness <= min_badness) {
			*info = info1;
			return badness;
		}
		best_badness = badness;
		best_info = info1;
		max_badness = badness - 1;
	}
	if (best_badness == UINT_MAX)
		return -EINVAL;
	*info = best_info;
	return best_badness;
}

int snd_pcm_hw_info_strategy(snd_pcm_t *pcm, snd_pcm_hw_info_t *info,
			     const snd_pcm_strategy_t *strategy,
			     unsigned int min_badness, unsigned int max_badness)
{
	int err;
	err = snd_pcm_hw_info(pcm, info);
	if (err < 0)
		return err;
	return snd_pcm_hw_info_strategy1(pcm, info, strategy, min_badness, max_badness);
}


void snd_pcm_strategy_simple_free(snd_pcm_strategy_t *strategy)
{
	snd_pcm_strategy_simple_t *pars = strategy->private;
	int k;
	for (k = 0; k <= SND_PCM_HW_PARAM_LAST; ++k) {
		if (pars[k].valid && pars[k].free)
			pars[k].free(&pars[k]);
	}
	free(pars);
}

int snd_pcm_strategy_simple_choose_param(const snd_pcm_hw_info_t *info,
					 snd_pcm_t *pcm ATTRIBUTE_UNUSED,
					 const snd_pcm_strategy_t *strategy)
{
	unsigned int param;
	int best_param = -1;
	const snd_pcm_strategy_simple_t *pars = strategy->private;
	unsigned long min_choices = ULONG_MAX;
	for (param = 0; param <= SND_PCM_HW_PARAM_LAST; ++param) {
		unsigned int choices;
		if (!pars[param].valid)
			continue;
		choices = par_choices(info, param);
		if (choices == 1)
			continue;
		assert(choices != 0);
		if (choices < min_choices) {
			min_choices = choices;
			best_param = param;
		}
	}
	return best_param;
}

long snd_pcm_strategy_simple_next_value(const snd_pcm_hw_info_t *info,
					unsigned int param,
					long value,
					snd_pcm_t *pcm,
					const snd_pcm_strategy_t *strategy)
{
	const snd_pcm_strategy_simple_t *pars = strategy->private;
	assert(pars[param].valid);
	return pars[param].next_value(info, param, value, pcm, &pars[param]);
}


int snd_pcm_strategy_simple_min_badness(const snd_pcm_hw_info_t *info,
					unsigned int max_badness,
					snd_pcm_t *pcm,
					const snd_pcm_strategy_t *strategy)
{
	unsigned int param;
	unsigned int badness = 0;
	const snd_pcm_strategy_simple_t *pars = strategy->private;
	for (param = 0; param <= SND_PCM_HW_PARAM_LAST; ++param) {
		unsigned int b;
		if (!pars[param].valid)
			continue;
		b = pars[param].min_badness(info, param, pcm, &pars[param]);
		if (b > max_badness || max_badness - b < badness)
			return -EINVAL;
		badness += b;
	}
	return badness;
}


void snd_pcm_strategy_simple_near_free(snd_pcm_strategy_simple_t *par)
{
	snd_pcm_strategy_simple_near_t *p = par->private;
	free(p);
}

unsigned int snd_pcm_strategy_simple_near_min_badness(const snd_pcm_hw_info_t *info,
						      unsigned int param,
						      snd_pcm_t *pcm,
						      const snd_pcm_strategy_simple_t *par)
{
	const snd_pcm_strategy_simple_near_t *p = par->private;
	long value = par_nearest_next(info, param, p->best, -1, pcm);
	long diff;
	assert(value >= 0);
	diff = p->best - value;
	if (diff < 0)
		diff = -diff;
	return diff * p->mul;
}
	
long snd_pcm_strategy_simple_near_next_value(const snd_pcm_hw_info_t *info,
					     unsigned int param,
					     long value,
					     snd_pcm_t *pcm,
					     const snd_pcm_strategy_simple_t *par)
{
	const snd_pcm_strategy_simple_near_t *p = par->private;
	return par_nearest_next(info, param, p->best, value, pcm);
}

void snd_pcm_strategy_simple_choices_free(snd_pcm_strategy_simple_t *par)
{
	snd_pcm_strategy_simple_choices_t *p = par->private;
//	free(p->choices);
	free(p);
}

unsigned int snd_pcm_strategy_simple_choices_min_badness(const snd_pcm_hw_info_t *info,
							 unsigned int param,
							 snd_pcm_t *pcm ATTRIBUTE_UNUSED,
							 const snd_pcm_strategy_simple_t *par)
{
	const snd_pcm_strategy_simple_choices_t *p = par->private;
	unsigned int k;
	for (k = 0; k < p->count; ++k) {
		if (par_check(info, param, p->choices[k].value))
			return p->choices[k].badness;
	}
	assert(0);
	return UINT_MAX;
}
	
long snd_pcm_strategy_simple_choices_next_value(const snd_pcm_hw_info_t *info,
						unsigned int param,
						long value,
						snd_pcm_t *pcm ATTRIBUTE_UNUSED,
						const snd_pcm_strategy_simple_t *par)
{
	const snd_pcm_strategy_simple_choices_t *p = par->private;
	unsigned int k = 0;
	if (value >= 0) {
		for (; k < p->count; ++k) {
			if (p->choices[k].value == (unsigned long) value) {
				k++;
				break;
			}
		}
	}
	for (; k < p->count; ++k) {
		unsigned long v = p->choices[k].value;
		if (par_check(info, param, v))
			return v;
	}
	return -1;
}

int snd_pcm_strategy_free(snd_pcm_strategy_t *strategy)
{
	if (strategy->free)
		strategy->free(strategy);
	free(strategy);
	return 0;
}

int snd_pcm_strategy_simple(snd_pcm_strategy_t **strategyp)
{
	snd_pcm_strategy_simple_t *data;
	snd_pcm_strategy_t *s;
	assert(strategyp);
	data = calloc(SND_PCM_HW_PARAM_LAST + 1, sizeof(*data));
	if (!data)
		return -ENOMEM;
	s = calloc(1, sizeof(*s));
	if (!s) {
		free(data);
		return -ENOMEM;
	}
	s->choose_param = snd_pcm_strategy_simple_choose_param;
	s->next_value = snd_pcm_strategy_simple_next_value;
	s->min_badness = snd_pcm_strategy_simple_min_badness;
	s->private = data;
	s->free = snd_pcm_strategy_simple_free;
	*strategyp = s;
	return 0;
}

int snd_pcm_strategy_simple_near(snd_pcm_strategy_t *strategy,
				 unsigned int param,
				 unsigned long best,
				 unsigned int mul)
{
	snd_pcm_strategy_simple_t *s = strategy->private;
	snd_pcm_strategy_simple_near_t *data;
	assert(strategy);
	assert(param <= SND_PCM_HW_PARAM_LAST);
	assert(!s->valid);
	data = calloc(1, sizeof(*data));
	if (!data)
		return -ENOMEM;
	data->best = best;
	data->mul = mul;
	s += param;
	s->valid = 1;
	s->next_value = snd_pcm_strategy_simple_near_next_value;
	s->min_badness = snd_pcm_strategy_simple_near_min_badness;
	s->private = data;
	s->free = snd_pcm_strategy_simple_near_free;
	return 0;
}

int snd_pcm_strategy_simple_choices(snd_pcm_strategy_t *strategy,
				    unsigned int param,
				    unsigned int count,
				    snd_pcm_strategy_simple_choices_list_t *choices)
{
	snd_pcm_strategy_simple_t *s = strategy->private;
	snd_pcm_strategy_simple_choices_t *data;
	assert(strategy);
	assert(param <= SND_PCM_HW_PARAM_LAST);
	assert(!s->valid);
	data = calloc(1, sizeof(*data));
	if (!data)
		return -ENOMEM;
	data->count = count;
	data->choices = choices;
	s += param;
	s->valid = 1;
	s->next_value = snd_pcm_strategy_simple_choices_next_value;
	s->min_badness = snd_pcm_strategy_simple_choices_min_badness;
	s->private = data;
	s->free = snd_pcm_strategy_simple_choices_free;
	return 0;
}

size_t _snd_pcm_mmap_hw_ptr(snd_pcm_t *pcm)
{
	return *pcm->hw_ptr;
}

