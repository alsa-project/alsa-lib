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
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include "pcm_local.h"

snd_pcm_type_t snd_pcm_type(snd_pcm_t *handle)
{
	assert(handle);
	return handle->type;
}

snd_pcm_type_t snd_pcm(snd_pcm_t *handle)
{
	assert(handle);
	return handle->stream;
}

int snd_pcm_close(snd_pcm_t *handle)
{
	int ret = 0;
	int err;
	assert(handle);
	if (handle->mmap_status) {
		if ((err = snd_pcm_munmap_status(handle)) < 0)
			ret = err;
	}
	if (handle->mmap_control) {
		if ((err = snd_pcm_munmap_control(handle)) < 0)
			ret = err;
	}
	if (handle->mmap_data) {
		if ((err = snd_pcm_munmap_data(handle)) < 0)
			ret = err;
	}
	if ((err = handle->ops->close(handle->op_arg)) < 0)
		ret = err;
	handle->valid_setup = 0;
	free(handle);
	return ret;
}	

int snd_pcm_nonblock(snd_pcm_t *handle, int nonblock)
{
	int err;
	assert(handle);
	if ((err = handle->fast_ops->nonblock(handle->fast_op_arg, nonblock)) < 0)
		return err;
	if (nonblock)
		handle->mode |= SND_PCM_NONBLOCK;
	else
		handle->mode &= ~SND_PCM_NONBLOCK;
	return 0;
}

int snd_pcm_info(snd_pcm_t *handle, snd_pcm_info_t *info)
{
	assert(handle && info);
	return handle->ops->info(handle->op_arg, info);
}

int snd_pcm_params_info(snd_pcm_t *handle, snd_pcm_params_info_t *info)
{
	assert(handle && info);
	return handle->ops->params_info(handle->op_arg, info);
}

int snd_pcm_setup(snd_pcm_t *handle, snd_pcm_setup_t *setup)
{
	int err;
	assert(handle && setup);
	if (handle->valid_setup) {
		*setup = handle->setup;
		return 0;
	}
	if ((err = handle->ops->setup(handle->op_arg, &handle->setup)) < 0)
		return err;
	*setup = handle->setup;
	handle->bits_per_sample = snd_pcm_format_physical_width(setup->format.format);
        handle->bits_per_frame = handle->bits_per_sample * setup->format.channels;
	handle->valid_setup = 1;
	return 0;
}

int snd_pcm_channel_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup)
{
	assert(handle && setup);
	assert(handle->valid_setup);
	return handle->fast_ops->channel_setup(handle->fast_op_arg, setup);
}

int snd_pcm_params(snd_pcm_t *handle, snd_pcm_params_t *params)
{
	int err;
	snd_pcm_setup_t setup;
	assert(handle && params);
	assert(!handle->mmap_data);
	if ((err = handle->ops->params(handle->op_arg, params)) < 0)
		return err;
	handle->valid_setup = 0;
	return snd_pcm_setup(handle, &setup);
}

int snd_pcm_status(snd_pcm_t *handle, snd_pcm_status_t *status)
{
	assert(handle && status);
	return handle->fast_ops->status(handle->fast_op_arg, status);
}

int snd_pcm_state(snd_pcm_t *handle)
{
	assert(handle);
	if (handle->mmap_status)
		return handle->mmap_status->state;
	return handle->fast_ops->state(handle->fast_op_arg);
}

int snd_pcm_frame_io(snd_pcm_t *handle, int update)
{
	assert(handle);
	assert(handle->valid_setup);
	if (handle->mmap_status && !update)
		return handle->mmap_status->frame_io;
	return handle->fast_ops->frame_io(handle->fast_op_arg, update);
}

int snd_pcm_prepare(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->prepare(handle->fast_op_arg);
}

int snd_pcm_go(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->go(handle->fast_op_arg);
}

#if 0
int snd_pcm_synchro(snd_pcm_synchro_cmd_t cmd, 
		    unsigned int reqs_count, snd_pcm_synchro_request_t *reqs,
		    snd_pcm_synchro_mode_t mode)
{
	snd_pcm_sync_request_t *sync_reqs;
	snd_pcm_sync_t sync;
	unsigned int k;
	int ret;
	assert(reqs_count > 0 && reqs);
	sync_reqs = __builtin_alloca(sizeof(*sync_reqs) * reqs_count);
	switch (cmd) {
	case SND_PCM_SYNCHRO_GO:
		sync.cmd = SND_PCM_IOCTL_GO;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	sync.mode = mode;
	sync.requests_count = reqs_count;
	sync.requests = sync_reqs;
	for (k = 0; k < reqs_count; ++k) {
		switch (snd_pcm_type(reqs[k].handle)) {
		case SND_PCM_TYPE_HW:
		case SND_PCM_TYPE_PLUG:
			sync_reqs[k].fd = snd_pcm_file_descriptor(reqs[k].handle);
			break;
		default:
			/* Not yet implemented */
			assert(0);
			return -ENOSYS;
		}
	}
	if (ioctl(sync_reqs[0].fd, SND_PCM_IOCTL_SYNC, &sync) < 0)
		ret = -errno;
	else
		ret = 0;
	for (k = 0; k < reqs_count; ++k) {
		reqs[k].tstamp = sync_reqs[k].tstamp;
		reqs[k].result = sync_reqs[k].result;
	}
	return ret;
}
#endif

int snd_pcm_drain(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->drain(handle->fast_op_arg);
}

int snd_pcm_flush(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->flush(handle->fast_op_arg);
}

int snd_pcm_pause(snd_pcm_t *handle, int enable)
{
	assert(handle);
	return handle->fast_ops->pause(handle->fast_op_arg, enable);
}


ssize_t snd_pcm_frame_data(snd_pcm_t *handle, off_t offset)
{
	assert(handle);
	assert(handle->valid_setup);
	if (handle->mmap_control) {
		if (offset == 0)
			return handle->mmap_control->frame_data;
	}
	return handle->fast_ops->frame_data(handle->fast_op_arg, offset);
}

ssize_t snd_pcm_write(snd_pcm_t *handle, const void *buffer, size_t size)
{
	assert(handle);
	assert(size == 0 || buffer);
	assert(handle->valid_setup);
	assert(size % handle->setup.frames_align == 0);
	return handle->fast_ops->write(handle->fast_op_arg, 0, buffer, size);
}

ssize_t snd_pcm_writev(snd_pcm_t *handle, const struct iovec *vector, unsigned long count)
{
	assert(handle);
	assert(count == 0 || vector);
	assert(handle->valid_setup);
	assert(handle->setup.format.interleave || 
	       count % handle->setup.format.channels == 0);
	return handle->fast_ops->writev(handle->fast_op_arg, 0, vector, count);
}

ssize_t snd_pcm_read(snd_pcm_t *handle, void *buffer, size_t size)
{
	assert(handle);
	assert(size == 0 || buffer);
	assert(handle->valid_setup);
	assert(size % handle->setup.frames_align == 0);
	return handle->fast_ops->read(handle->fast_op_arg, 0, buffer, size);
}

ssize_t snd_pcm_readv(snd_pcm_t *handle, const struct iovec *vector, unsigned long count)
{
	assert(handle);
	assert(count == 0 || vector);
	assert(handle->valid_setup);
	return handle->fast_ops->readv(handle->fast_op_arg, 0, vector, count);
}

int snd_pcm_link(snd_pcm_t *handle1, snd_pcm_t *handle2)
{
	int fd1, fd2;
	switch (handle1->type) {
	case SND_PCM_TYPE_HW:
	case SND_PCM_TYPE_PLUG:
	case SND_PCM_TYPE_MULTI:
		fd1 = snd_pcm_file_descriptor(handle1);
		break;
	default:
		errno = -ENOSYS;
		return -1;
	}
	switch (handle2->type) {
	case SND_PCM_TYPE_HW:
	case SND_PCM_TYPE_PLUG:
	case SND_PCM_TYPE_MULTI:
		fd2 = snd_pcm_file_descriptor(handle2);
		break;
	default:
		errno = -ENOSYS;
		return -1;
	}
	if (ioctl(fd1, SND_PCM_IOCTL_LINK, fd2) < 0)
		return -errno;
	return 0;
}

int snd_pcm_unlink(snd_pcm_t *handle)
{
	int fd;
	switch (handle->type) {
	case SND_PCM_TYPE_HW:
	case SND_PCM_TYPE_PLUG:
	case SND_PCM_TYPE_MULTI:
		fd = snd_pcm_file_descriptor(handle);
		break;
	default:
		errno = -ENOSYS;
		return -1;
	}
	if (ioctl(fd, SND_PCM_IOCTL_UNLINK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_file_descriptor(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->file_descriptor(handle->fast_op_arg);
}

int snd_pcm_channels_mask(snd_pcm_t *handle, bitset_t *client_vmask)
{
	assert(handle);
	assert(handle->valid_setup);
	return handle->fast_ops->channels_mask(handle->fast_op_arg, client_vmask);
}

typedef struct {
	int value;
	const char* name;
	const char* desc;
} assoc_t;

static assoc_t *assoc_value(int value, assoc_t *alist)
{
	while (alist->desc) {
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

#define STREAM(v) { SND_PCM_STREAM_##v, #v, #v }
#define MODE(v) { SND_PCM_MODE_##v, #v, #v }
#define FMT(v, d) { SND_PCM_SFMT_##v, #v, d }
#define XRUN(v) { SND_PCM_XRUN_##v, #v, #v }
#define START(v) { SND_PCM_START_##v, #v, #v }
#define FILL(v) { SND_PCM_FILL_##v, #v, #v }
#define END { 0, NULL, NULL }

static assoc_t streams[] = { STREAM(PLAYBACK), STREAM(CAPTURE), END };
static assoc_t modes[] = { MODE(FRAME), MODE(FRAGMENT), END };
static assoc_t fmts[] = {
	FMT(S8, "Signed 8-bit"), 
	FMT(U8, "Unsigned 8-bit"),
	FMT(S16_LE, "Signed 16-bit Little Endian"),
	FMT(S16_BE, "Signed 16-bit Big Endian"),
	FMT(U16_LE, "Unsigned 16-bit Little Endian"),
	FMT(U16_BE, "Unsigned 16-bit Big Endian"),
	FMT(S24_LE, "Signed 24-bit Little Endian"),
	FMT(S24_BE, "Signed 24-bit Big Endian"),
	FMT(U24_LE, "Unsigned 24-bit Little Endian"),
	FMT(U24_BE, "Unsigned 24-bit Big Endian"),
	FMT(S32_LE, "Signed 32-bit Little Endian"),
	FMT(S32_BE, "Signed 32-bit Big Endian"),
	FMT(U32_LE, "Unsigned 32-bit Little Endian"),
	FMT(U32_BE, "Unsigned 32-bit Big Endian"),
	FMT(FLOAT_LE, "Float Little Endian"),
	FMT(FLOAT_BE, "Float Big Endian"),
	FMT(FLOAT64_LE, "Float64 Little Endian"),
	FMT(FLOAT64_BE, "Float64 Big Endian"),
	FMT(IEC958_SUBFRAME_LE, "IEC-958 Little Endian"),
	FMT(IEC958_SUBFRAME_BE, "IEC-958 Big Endian"),
	FMT(MU_LAW, "Mu-Law"),
	FMT(A_LAW, "A-Law"),
	FMT(IMA_ADPCM, "Ima-ADPCM"),
	FMT(MPEG, "MPEG"),
	FMT(GSM, "GSM"),
	FMT(SPECIAL, "Special"),
	END 
};

static assoc_t starts[] = { START(GO), START(DATA), START(FULL), END };
static assoc_t xruns[] = { XRUN(FLUSH), XRUN(DRAIN), END };
static assoc_t fills[] = { FILL(NONE), FILL(SILENCE_WHOLE), FILL(SILENCE), END };
static assoc_t onoff[] = { {0, "OFF", NULL}, {1, "ON", NULL}, {-1, "ON", NULL}, END };

int snd_pcm_dump_setup(snd_pcm_t *handle, FILE *fp)
{
	snd_pcm_setup_t *setup;
	assert(handle);
	assert(fp);
	assert(handle->valid_setup);
	setup = &handle->setup;
        fprintf(fp, "stream: %s\n", assoc(handle->stream, streams));
	fprintf(fp, "mode: %s\n", assoc(setup->mode, modes));
	fprintf(fp, "format: %s\n", assoc(setup->format.format, fmts));
	fprintf(fp, "channels: %d\n", setup->format.channels);
	fprintf(fp, "rate: %d (%d/%d=%g)\n", setup->format.rate, setup->rate_master, setup->rate_divisor, (double) setup->rate_master / setup->rate_divisor);
	// digital
	fprintf(fp, "start_mode: %s\n", assoc(setup->start_mode, starts));
	fprintf(fp, "xrun_mode: %s\n", assoc(setup->xrun_mode, xruns));
	fprintf(fp, "time: %s\n", assoc(setup->time, onoff));
	// ust_time
	fprintf(fp, "buffer_size: %d\n", setup->buffer_size);
	fprintf(fp, "frag_size: %d\n", setup->frag_size);
	fprintf(fp, "frags: %d\n", setup->frags);
	fprintf(fp, "frame_boundary: %d\n", setup->frame_boundary);
	fprintf(fp, "msbits_per_sample: %d\n", setup->msbits_per_sample);
	fprintf(fp, "frames_min: %d\n", setup->frames_min);
	fprintf(fp, "frames_align: %d\n", setup->frames_align);
	fprintf(fp, "frames_xrun_max: %d\n", setup->frames_xrun_max);
	fprintf(fp, "fill_mode: %s\n", assoc(setup->fill_mode, fills));
	fprintf(fp, "frames_fill_max: %d\n", setup->frames_fill_max);
	return 0;
}

int snd_pcm_dump(snd_pcm_t *handle, FILE *fp)
{
	assert(handle);
	assert(fp);
	handle->ops->dump(handle->op_arg, fp);
	return 0;
}

const char *snd_pcm_format_name(int format)
{
	assoc_t *a = assoc_value(format, fmts);
	if (a)
		return a->name;
	return 0;
}

const char *snd_pcm_format_description(int format)
{
	assoc_t *a = assoc_value(format, fmts);
	if (a)
		return a->desc;
	return "Unknown";
}

int snd_pcm_format_value(const char* name)
{
	assoc_t *a = assoc_name(name, fmts);
	if (a)
		return a->value;
	return -1;
}

ssize_t snd_pcm_bytes_to_frames(snd_pcm_t *handle, int bytes)
{
	assert(handle);
	assert(handle->valid_setup);
	return bytes * 8 / handle->bits_per_frame;
}

ssize_t snd_pcm_frames_to_bytes(snd_pcm_t *handle, int frames)
{
	assert(handle);
	assert(handle->valid_setup);
	return frames * handle->bits_per_frame / 8;
}

ssize_t snd_pcm_bytes_to_samples(snd_pcm_t *handle, int bytes)
{
	assert(handle);
	assert(handle->valid_setup);
	return bytes * 8 / handle->bits_per_sample;
}

ssize_t snd_pcm_samples_to_bytes(snd_pcm_t *handle, int samples)
{
	assert(handle);
	assert(handle->valid_setup);
	return samples * handle->bits_per_sample / 8;
}

