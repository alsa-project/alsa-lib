/*
 *  PCM - Multi
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
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/uio.h>
#include "pcm_local.h"

typedef struct {
	snd_pcm_t *handle;
	unsigned int channels_total;
	int close_slave;
	char *buf;
	snd_pcm_channel_area_t *areas;
	struct iovec *iovec;
} snd_pcm_multi_slave_t;

typedef struct {
	unsigned int client_channel;
	unsigned int slave;
	unsigned int slave_channel;
} snd_pcm_multi_bind_t;

typedef struct {
	snd_pcm_t *handle;
	size_t slaves_count;
	snd_pcm_multi_slave_t *slaves;
	size_t bindings_count;
	snd_pcm_multi_bind_t *bindings;
	size_t channels_count;
	size_t frames_alloc;
	int interleave;
	int one_to_many;
} snd_pcm_multi_t;

static int snd_pcm_multi_close(void *private)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	int ret = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		int err;
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		if (slave->close_slave) {
			err = snd_pcm_close(slave->handle);
			if (err < 0)
				ret = err;
		} else
			snd_pcm_unlink(slave->handle);
		if (slave->buf) {
			free(slave->buf);
			free(slave->areas);
		}
		if (slave->iovec)
			free(slave->iovec);
	}
	free(multi->slaves);
	free(multi->bindings);
	free(private);
	return ret;
}

static int snd_pcm_multi_nonblock(void *private, int nonblock)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_nonblock(handle, nonblock);
}

static int snd_pcm_multi_info(void *private, snd_pcm_info_t *info)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	int err;
	snd_pcm_t *handle_0 = multi->slaves[0].handle;
	err = snd_pcm_info(handle_0, info);
	if (err < 0)
		return err;
	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle_i = multi->slaves[i].handle;
		snd_pcm_info_t info_i;
		memset(&info_i, 0, sizeof(info_i));
		err = snd_pcm_info(handle_i, &info_i);
		if (err < 0)
			return err;
		info->flags &= info_i.flags;
	}
	if (multi->one_to_many)
		info->flags &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_multi_params_info(void *private, snd_pcm_params_info_t *info)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	int err;
	snd_pcm_t *handle_0 = multi->slaves[0].handle;
	unsigned int old_mask = info->req_mask;
	info->req_mask &= ~SND_PCM_PARAMS_CHANNELS;
	err = snd_pcm_params_info(handle_0, info);
	if (err < 0)
		return err;
	info->min_channels = multi->channels_count;
	info->max_channels = multi->channels_count;
	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle_i = multi->slaves[i].handle;
		snd_pcm_params_info_t info_i;
		info_i = *info;
		err = snd_pcm_params_info(handle_i, &info_i);
		if (err < 0)
			return err;
		info->formats &= info_i.formats;
		info->rates &= info_i.rates;
		if (info_i.min_rate > info->min_rate)
			info->min_rate = info_i.min_rate;
		if (info_i.max_rate < info->max_rate)
			info->max_rate = info_i.max_rate;
		if (info_i.buffer_size < info->buffer_size)
			info->buffer_size = info_i.buffer_size;
		if (info_i.min_fragment_size > info->min_fragment_size)
			info->min_fragment_size = info_i.min_fragment_size;
		if (info_i.max_fragment_size < info->max_fragment_size)
			info->max_fragment_size = info_i.max_fragment_size;
		if (info_i.min_fragments > info->min_fragments)
			info->min_fragments = info_i.min_fragments;
		if (info_i.max_fragments < info->max_fragments)
			info->max_fragments = info_i.max_fragments;
	}
	info->req_mask = old_mask;
	return 0;
}

static int snd_pcm_multi_params(void *private, snd_pcm_params_t *params)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	snd_pcm_params_t p;
	if (params->format.channels != multi->channels_count)
		return -EINVAL;
	p = *params;
	multi->interleave = params->format.interleave;
	for (i = 0; i < multi->slaves_count; ++i) {
		int err;
		snd_pcm_t *handle = multi->slaves[i].handle;
		snd_pcm_info_t info;
		err = snd_pcm_info(handle, &info);
		if (err < 0)
			return err;
		p.format.interleave = params->format.interleave;
		if (!(info.flags & SND_PCM_INFO_INTERLEAVE))
			p.format.interleave = 0;
		else if (!(info.flags & SND_PCM_INFO_NONINTERLEAVE))
			p.format.interleave = 1;
		p.format.channels = multi->slaves[i].channels_total;
#if 1
		p.frames_xrun_max = ~0;
#endif
		err = snd_pcm_params(handle, &p);
		if (err < 0)
			return err;
		if (i == 0 && params->mode == SND_PCM_MODE_FRAGMENT) {
			snd_pcm_setup_t s;
			err = snd_pcm_setup(handle, &s);
			if (err < 0)
				return err;
			p.frag_size = s.frag_size;
			p.buffer_size = s.buffer_size;
		}
	}
	return 0;
}

static int snd_pcm_multi_setup(void *private, snd_pcm_setup_t *setup)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	int err;
	size_t frames_alloc;
	err = snd_pcm_setup(multi->slaves[0].handle, setup);
	if (err < 0)
		return err;
	frames_alloc = multi->slaves[0].handle->setup.frag_size;
	multi->frames_alloc = 0;
	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_setup_t s;
		snd_pcm_t *sh = multi->slaves[i].handle;
		err = snd_pcm_setup(sh, &s);
		if (err < 0)
			return err;
		if (setup->format.rate != s.format.rate)
			return -EINVAL;
		if (setup->frames_align % s.frames_align != 0)
			return -EINVAL;
	}
	setup->format.interleave = multi->interleave;
	setup->format.channels = multi->channels_count;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_multi_slave_t *s = &multi->slaves[i];
		snd_pcm_t *sh = s->handle;
		unsigned int c;
		if (s->buf) {
			free(s->buf);
			s->buf = 0;
			free(s->areas);
			s->areas = 0;
		}
		if (s->iovec)
			free(s->iovec);
		if (!sh->setup.format.interleave) {
			s->iovec = calloc(s->channels_total, sizeof(*s->iovec));
			if (!multi->handle->setup.format.interleave)
				continue;
		}
		s->buf = malloc(frames_alloc * sh->bits_per_frame / 8);
		if (!s->buf)
			return -ENOMEM;
		snd_pcm_format_set_silence(sh->setup.format.format, s->buf,
					   sh->setup.frag_size * sh->setup.format.channels);
		s->areas = calloc(s->channels_total, sizeof(*s->areas));
		if (!s->areas)
			return -ENOMEM;
		for (c = 0; c < s->channels_total; ++c) {
			snd_pcm_channel_area_t *a = &s->areas[c];
			if (sh->setup.format.interleave) {
				a->addr = s->buf;
				a->first = c * sh->bits_per_sample;
				a->step = sh->bits_per_frame;
			} else {
				a->addr = s->buf + sh->setup.frag_size * sh->bits_per_sample / 8;
				a->first = 0;
				a->step = sh->bits_per_sample;
				s->iovec[c].iov_base = a->addr;
			}
		}
	}
	multi->frames_alloc = frames_alloc;
	/* Loaded with a value != 0 if mmap is feasible */
	setup->mmap_bytes = !multi->one_to_many;
	return 0;
}

static int snd_pcm_multi_status(void *private, snd_pcm_status_t *status)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_status(handle, status);
}

static int snd_pcm_multi_state(void *private)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_state(handle);
}

static ssize_t snd_pcm_multi_frame_io(void *private, int update)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_frame_io(handle, update);
}

static int snd_pcm_multi_prepare(void *private)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	return snd_pcm_prepare(multi->slaves[0].handle);
}

static int snd_pcm_multi_go(void *private)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	return snd_pcm_go(multi->slaves[0].handle);
}

static int snd_pcm_multi_drain(void *private)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	return snd_pcm_drain(multi->slaves[0].handle);
}

static int snd_pcm_multi_flush(void *private)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	return snd_pcm_flush(multi->slaves[0].handle);
}

static int snd_pcm_multi_pause(void *private, int enable)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	return snd_pcm_pause(multi->slaves[0].handle, enable);
}

static int snd_pcm_multi_channel_setup(void *private, snd_pcm_channel_setup_t *setup)
{
	int err;
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int channel = setup->channel;
	unsigned int i;
	for (i = 0; i < multi->bindings_count; ++i) {
		if (multi->bindings[i].client_channel == channel) {
			setup->channel = multi->bindings[i].slave_channel;
			err = snd_pcm_channel_setup(multi->slaves[multi->bindings[i].slave].handle, setup);
			setup->channel = channel;
			return err;
		}
	}
	memset(setup, 0, sizeof(*setup));
	setup->channel = channel;
	return 0;
}

static ssize_t snd_pcm_multi_frame_data(void *private, off_t offset)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	ssize_t pos, newpos;
	unsigned int i;
	snd_pcm_t *handle_0 = multi->slaves[0].handle;

	pos = snd_pcm_frame_data(handle_0, 0);
	newpos = snd_pcm_frame_data(handle_0, offset);
	if (newpos < 0)
		return newpos;
	offset = newpos - pos;
	if (offset < 0)
		offset += handle_0->setup.frame_boundary;

	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle_i = multi->slaves[i].handle;
		ssize_t newpos_i;
		newpos_i = snd_pcm_frame_data(handle_i, offset);
		if (newpos_i < 0)
			return newpos_i;
		if (newpos_i != newpos)
			return -EBADFD;
	}
	return newpos;
}

static int snd_pcm_multi_write_copy(snd_pcm_multi_t *multi, const void *buf,
				    size_t offset, size_t count)
{
	unsigned int i;
	snd_pcm_channel_area_t area;
	snd_pcm_t *handle = multi->handle;
	area.addr = (void *) buf + offset * handle->bits_per_frame;
	area.step = handle->bits_per_frame;
	for (i = 0; i < multi->bindings_count; ++i) {
		snd_pcm_multi_bind_t *bind = &multi->bindings[i];
		snd_pcm_multi_slave_t *slave = &multi->slaves[bind->slave];
		int err;
		assert(slave->buf);
		area.first = handle->bits_per_sample * bind->client_channel;
		err = snd_pcm_area_copy(&area, 0, &slave->areas[bind->slave_channel], 0, count, handle->setup.format.format);
		if (err < 0)
			return err;
		if (!slave->handle->setup.format.interleave) {
			struct iovec *vec = &slave->iovec[bind->slave_channel];
			vec->iov_len = count;
		}
	}
	return 0;
}

static int snd_pcm_multi_writev_copy(snd_pcm_multi_t *multi, const struct iovec *vec,
				     size_t offset, size_t count)
{
	unsigned int i;
	snd_pcm_channel_area_t area;
	snd_pcm_t *handle = multi->handle;
	area.first = 0;
	area.step = handle->bits_per_sample;
	for (i = 0; i < multi->bindings_count; ++i) {
		snd_pcm_multi_bind_t *bind = &multi->bindings[i];
		snd_pcm_multi_slave_t *slave = &multi->slaves[bind->slave];
		int err;
		area.addr = vec[bind->client_channel].iov_base + 
			offset * handle->bits_per_sample;
		if (slave->handle->setup.format.interleave) {
			assert(slave->buf);
			err = snd_pcm_area_copy(&area, 0, &slave->areas[bind->slave_channel], 0, count, handle->setup.format.format);
			if (err < 0)
				return err;
		} else {
			struct iovec *vec = &slave->iovec[bind->slave_channel];
			vec->iov_base = area.addr;
			vec->iov_len = count;
		}
	}
	return 0;
}

static ssize_t snd_pcm_multi_write_io(snd_pcm_multi_t *multi, size_t count)
{
	unsigned int i;
	ssize_t frames = count;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		snd_pcm_t *sh = slave->handle;
		if (sh->setup.format.interleave) {
			frames = snd_pcm_write(sh, slave->buf, frames);
		} else {
			int channels = sh->setup.format.channels;
			frames = snd_pcm_writev(sh, slave->iovec, channels);
		}
		if (frames <= 0)
			break;
	}
	return frames;
}

static ssize_t snd_pcm_multi_write(void *private, snd_timestamp_t *timestamp ATTRIBUTE_UNUSED, const void *buf, size_t count)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	size_t result = 0;
	while (count > 0) {
		int err;
		ssize_t ret;
		size_t frames = count;
		if (frames > multi->frames_alloc)
			frames = multi->frames_alloc;
		err = snd_pcm_multi_write_copy(multi, buf, result, frames);
		if (err < 0)
			return err;
		ret = snd_pcm_multi_write_io(multi, frames);
		if (ret > 0)
			result += ret;
		if (ret != (ssize_t)frames) {
			if (result > 0)
				return result;
			return ret;
		}
		count -= ret;
	}
	return result;
}

static ssize_t snd_pcm_multi_writev1(snd_pcm_multi_t *multi, const struct iovec *vector, size_t count)
{
	size_t result = 0;
	while (count > 0) {
		int err;
		ssize_t ret;
		size_t frames = count;
		if (frames > multi->frames_alloc)
			frames = multi->frames_alloc;
		err = snd_pcm_multi_writev_copy(multi, vector, result, frames);
		if (err < 0)
			return err;
		ret = snd_pcm_multi_write_io(multi, frames);
		if (ret > 0)
			result += ret;
		if (ret != (ssize_t) frames) {
			if (result > 0)
				return result;
			return ret;
		}
		count -= ret;
	}
	return result;
}

static ssize_t snd_pcm_multi_writev(void *private, snd_timestamp_t *timestamp ATTRIBUTE_UNUSED, const struct iovec *vector, unsigned long count)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	snd_pcm_t *handle = multi->handle;
	unsigned int k, step;
	size_t result = 0;
	if (handle->setup.format.interleave)
		step = 1;
	else
		step = handle->setup.format.channels;
	for (k = 0; k < count; k += step) {
		ssize_t ret;
		if (handle->setup.format.interleave)
			ret = snd_pcm_multi_write(private, timestamp, vector->iov_base, vector->iov_len);
		else
			ret = snd_pcm_multi_writev1(multi, vector, vector->iov_len);
		if (ret > 0)
			result += ret;
		if (ret != (ssize_t) vector->iov_len) {
			if (result > 0)
				return result;
			return ret;
		}
		vector += step;
	}
	return result;
}

static ssize_t snd_pcm_multi_read(void *private ATTRIBUTE_UNUSED, snd_timestamp_t *timestamp ATTRIBUTE_UNUSED, void *buf ATTRIBUTE_UNUSED, size_t count ATTRIBUTE_UNUSED)
{
	// snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	return -ENOSYS;
}

static ssize_t snd_pcm_multi_readv(void *private ATTRIBUTE_UNUSED, snd_timestamp_t *timestamp ATTRIBUTE_UNUSED, const struct iovec *vector ATTRIBUTE_UNUSED, unsigned long count ATTRIBUTE_UNUSED)
{
	// snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	return -ENOSYS;
}

static int snd_pcm_multi_mmap_status(void *private, snd_pcm_mmap_status_t **status)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_mmap_status(handle, status);
		if (err < 0)
			return err;
	}
	*status = multi->slaves[0].handle->mmap_status;
	return 0;
}

static int snd_pcm_multi_mmap_control(void *private, snd_pcm_mmap_control_t **control)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	snd_pcm_setup_t *setup_0 = &multi->slaves[0].handle->setup;
	unsigned int i;
	for (i = 1; i < multi->slaves_count; ++i) {
		snd_pcm_setup_t *setup = &multi->slaves[i].handle->setup;
		/* Don't permit mmap if frame_data's have
		   different ranges */
		if (setup->buffer_size != setup_0->buffer_size)
			return -EBADFD;
	}
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_mmap_control(handle, control);
		if (err < 0)
			return err;
	}
	*control = multi->slaves[0].handle->mmap_control;
	return 0;
}

static int snd_pcm_multi_mmap_data(void *private, void **buffer, size_t bsize ATTRIBUTE_UNUSED)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_mmap_data(handle, 0);
		snd_pcm_setup_t *setup;
		if (err < 0)
			return err;
		setup = &handle->setup;
		{
			snd_pcm_channel_area_t areas[setup->format.channels];
			err = snd_pcm_mmap_get_areas(handle, areas);
			if (err < 0)
				return err;
			err = snd_pcm_areas_silence(areas, 0, setup->format.channels, setup->buffer_size, setup->format.format);
			if (err < 0)
				return err;
		}
	}
	*buffer = multi->slaves[0].handle->mmap_data;
	return 0;
}

static int snd_pcm_multi_munmap_status(void *private, snd_pcm_mmap_status_t *status ATTRIBUTE_UNUSED)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	int ret = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_munmap_status(handle);
		if (err < 0)
			ret = err;
	}
	return ret;
}
		
static int snd_pcm_multi_munmap_control(void *private, snd_pcm_mmap_control_t *control ATTRIBUTE_UNUSED)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	int ret = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_munmap_control(handle);
		if (err < 0)
			ret = err;
	}
	return ret;
}
		
static int snd_pcm_multi_munmap_data(void *private, void *buffer ATTRIBUTE_UNUSED, size_t size ATTRIBUTE_UNUSED)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	int ret = 0;
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		int err = snd_pcm_munmap_data(handle);
		if (err < 0)
			ret = err;
	}
	return ret;
}
		
static int snd_pcm_multi_channels_mask(void *private, bitset_t *client_vmask)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	unsigned int i;
	bitset_t *vmasks[multi->slaves_count];
	int err;
	for (i = 0; i < multi->slaves_count; ++i)
		vmasks[i] = bitset_alloc(multi->slaves[i].channels_total);
	for (i = 0; i < multi->bindings_count; ++i) {
		snd_pcm_multi_bind_t *b = &multi->bindings[i];
		if (bitset_get(client_vmask, b->client_channel))
			bitset_set(vmasks[b->slave], b->slave_channel);
	}
	for (i = 0; i < multi->slaves_count; ++i) {
		snd_pcm_t *handle = multi->slaves[i].handle;
		err = snd_pcm_channels_mask(handle, vmasks[i]);
		if (err < 0) {
			for (i = 0; i <= multi->slaves_count; ++i)
				free(vmasks[i]);
			return err;
		}
	}
	bitset_zero(client_vmask, multi->handle->setup.format.channels);
	for (i = 0; i < multi->bindings_count; ++i) {
		snd_pcm_multi_bind_t *b = &multi->bindings[i];
		if (bitset_get(vmasks[b->slave], b->slave_channel))
			bitset_set(client_vmask, b->client_channel);
	}
	for (i = 0; i < multi->slaves_count; ++i)
		free(vmasks[i]);
	return 0;
}
		
int snd_pcm_multi_file_descriptor(void *private)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	snd_pcm_t *handle = multi->slaves[0].handle;
	return snd_pcm_file_descriptor(handle);
}

static void snd_pcm_multi_dump(void *private, FILE *fp)
{
	snd_pcm_multi_t *multi = (snd_pcm_multi_t*) private;
	snd_pcm_t *handle = multi->handle;
	unsigned int k;
	fprintf(fp, "Multi PCM\n");
	if (handle->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(handle, fp);
	}
	for (k = 0; k < multi->slaves_count; ++k) {
		fprintf(fp, "\nSlave #%d: ", k);
		snd_pcm_dump(multi->slaves[k].handle, fp);
	}
	fprintf(fp, "\nBindings:\n");
	for (k = 0; k < multi->bindings_count; ++k) {
		fprintf(fp, "Channel #%d: slave %d[%d]\n", 
			multi->bindings[k].client_channel,
			multi->bindings[k].slave,
			multi->bindings[k].slave_channel);
	}
}

struct snd_pcm_ops snd_pcm_multi_ops = {
	close: snd_pcm_multi_close,
	info: snd_pcm_multi_info,
	params_info: snd_pcm_multi_params_info,
	params: snd_pcm_multi_params,
	setup: snd_pcm_multi_setup,
	dump: snd_pcm_multi_dump,
};

struct snd_pcm_fast_ops snd_pcm_multi_fast_ops = {
	nonblock: snd_pcm_multi_nonblock,
	channel_setup: snd_pcm_multi_channel_setup,
	status: snd_pcm_multi_status,
	frame_io: snd_pcm_multi_frame_io,
	state: snd_pcm_multi_state,
	prepare: snd_pcm_multi_prepare,
	go: snd_pcm_multi_go,
	drain: snd_pcm_multi_drain,
	flush: snd_pcm_multi_flush,
	pause: snd_pcm_multi_pause,
	write: snd_pcm_multi_write,
	writev: snd_pcm_multi_writev,
	read: snd_pcm_multi_read,
	readv: snd_pcm_multi_readv,
	frame_data: snd_pcm_multi_frame_data,
	mmap_status: snd_pcm_multi_mmap_status,
	mmap_control: snd_pcm_multi_mmap_control,
	mmap_data: snd_pcm_multi_mmap_data,
	munmap_status: snd_pcm_multi_munmap_status,
	munmap_control: snd_pcm_multi_munmap_control,
	munmap_data: snd_pcm_multi_munmap_data,
	file_descriptor: snd_pcm_multi_file_descriptor,
	channels_mask: snd_pcm_multi_channels_mask,
};

int snd_pcm_multi_create(snd_pcm_t **handlep, size_t slaves_count,
			 snd_pcm_t **slaves_handle, size_t *schannels_count,
			 size_t bindings_count,  unsigned int *bindings_cchannel,
			 unsigned int *bindings_slave, unsigned int *bindings_schannel,
			 int close_slaves)
{
	snd_pcm_t *handle;
	snd_pcm_multi_t *multi;
	size_t channels = 0;
	unsigned int i;
	int stream;
	char client_map[32] = { 0 };
	char slave_map[32][32] = { { 0 } };

	assert(handlep);
	assert(slaves_count > 0 && slaves_handle && schannels_count);
	assert(bindings_count > 0 && bindings_slave && bindings_cchannel && bindings_schannel);

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle)
		return -ENOMEM;
	multi = calloc(1, sizeof(snd_pcm_multi_t));
	if (!multi) {
		free(handle);
		return -ENOMEM;
	}

	stream = slaves_handle[0]->stream;
	
	multi->handle = handle;
	multi->slaves_count = slaves_count;
	multi->slaves = calloc(slaves_count, sizeof(*multi->slaves));
	multi->bindings_count = bindings_count;
	multi->bindings = calloc(bindings_count, sizeof(*multi->bindings));
	for (i = 0; i < slaves_count; ++i) {
		snd_pcm_multi_slave_t *slave = &multi->slaves[i];
		assert(slaves_handle[i]->stream == stream);
		slave->handle = slaves_handle[i];
		slave->channels_total = schannels_count[i];
		slave->close_slave = close_slaves;
		if (i != 0)
			snd_pcm_link(slaves_handle[i-1], slaves_handle[i]);
	}
	for (i = 0; i < bindings_count; ++i) {
		snd_pcm_multi_bind_t *bind = &multi->bindings[i];
		assert(bindings_slave[i] < slaves_count);
		assert(bindings_schannel[i] < schannels_count[bindings_slave[i]]);
		bind->client_channel = bindings_cchannel[i];
		bind->slave = bindings_slave[i];
		bind->slave_channel = bindings_schannel[i];
		if (slave_map[bindings_slave[i]][bindings_schannel[i]]) {
			assert(stream == SND_PCM_STREAM_CAPTURE);
			multi->one_to_many = 1;
		}
		slave_map[bindings_slave[i]][bindings_schannel[i]] = 1;
		if (client_map[bindings_cchannel[i]]) {
			assert(stream == SND_PCM_STREAM_PLAYBACK);
			multi->one_to_many = 1;
		}
		client_map[bindings_cchannel[i]] = 1;
		if (bindings_cchannel[i] >= channels)
			channels = bindings_cchannel[i] + 1;
	}
	multi->channels_count = channels;

	handle->type = SND_PCM_TYPE_MULTI;
	handle->stream = stream;
	handle->mode = multi->slaves[0].handle->mode;
	handle->ops = &snd_pcm_multi_ops;
	handle->op_arg = multi;
	handle->fast_ops = &snd_pcm_multi_fast_ops;
	handle->fast_op_arg = multi;
	handle->private = multi;
	*handlep = handle;
	return 0;
}
