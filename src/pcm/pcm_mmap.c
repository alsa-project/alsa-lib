/*
 *  PCM Interface - mmap
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
#include <malloc.h>
#include <errno.h>
#include <assert.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include "pcm_local.h"

static ssize_t snd_pcm_mmap_playback_frames_avail(snd_pcm_t *pcm)
{
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	ssize_t bytes = snd_pcm_mmap_playback_bytes_avail(str);
	return bytes * 8 / str->bits_per_frame;
}

static size_t snd_pcm_mmap_capture_frames_avail(snd_pcm_t *pcm)
{
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	size_t bytes = snd_pcm_mmap_capture_bytes_avail(str);
	return bytes * 8 / str->bits_per_frame;
}

int snd_pcm_frames_avail(snd_pcm_t *pcm, int stream, ssize_t *frames)
{
	snd_pcm_stream_t *str;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->mmap_control)
		return -EBADFD;
	if (stream == SND_PCM_STREAM_PLAYBACK)
		*frames = snd_pcm_mmap_playback_frames_avail(pcm);
	else
		*frames = snd_pcm_mmap_capture_frames_avail(pcm);
	return 0;
}

static int snd_pcm_mmap_playback_ready(snd_pcm_t *pcm)
{
	snd_pcm_stream_t *str;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (str->mmap_control->status == SND_PCM_STATUS_XRUN)
		return -EPIPE;
	return snd_pcm_mmap_playback_bytes_avail(str) >= str->setup.bytes_min;
}

static int snd_pcm_mmap_capture_ready(snd_pcm_t *pcm)
{
	snd_pcm_stream_t *str;
	int ret = 0;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (str->mmap_control->status == SND_PCM_STATUS_XRUN) {
		ret = -EPIPE;
		if (str->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
			return -EPIPE;
	}
	if (snd_pcm_mmap_capture_bytes_avail(str) >= str->setup.bytes_min)
		return 1;
	return ret;
}

int snd_pcm_mmap_ready(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	snd_pcm_mmap_control_t *ctrl;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->mmap_control)
		return -EBADFD;
	ctrl = str->mmap_control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		return snd_pcm_mmap_playback_ready(pcm);
	} else {
		return snd_pcm_mmap_capture_ready(pcm);
	}
}

static size_t snd_pcm_mmap_playback_bytes_xfer(snd_pcm_t *pcm, size_t bytes)
{
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	snd_pcm_mmap_control_t *ctrl = str->mmap_control;
	size_t bytes_cont;
	size_t bytes_avail = snd_pcm_mmap_playback_bytes_avail(str);
	if (bytes_avail < bytes)
		bytes = bytes_avail;
	bytes_cont = str->setup.buffer_size - ctrl->byte_data % str->setup.buffer_size;
	if (bytes_cont < bytes)
		bytes = bytes_cont;
	return bytes;
}

static size_t snd_pcm_mmap_capture_bytes_xfer(snd_pcm_t *pcm, size_t bytes)
{
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	snd_pcm_mmap_control_t *ctrl = str->mmap_control;
	size_t bytes_cont;
	size_t bytes_avail = snd_pcm_mmap_capture_bytes_avail(str);
	if (bytes_avail < bytes)
		bytes = bytes_avail;
	bytes_cont = str->setup.buffer_size - ctrl->byte_data % str->setup.buffer_size;
	if (bytes_cont < bytes)
		bytes = bytes_cont;
	return bytes;
}

static ssize_t snd_pcm_mmap_playback_frames_xfer(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	size_t bytes = frames * str->bits_per_frame / 8;
	bytes = snd_pcm_mmap_playback_bytes_xfer(pcm, bytes);
	return bytes * 8 / str->bits_per_frame;
}

static ssize_t snd_pcm_mmap_capture_frames_xfer(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	size_t bytes = frames * str->bits_per_frame / 8;
	bytes = snd_pcm_mmap_capture_bytes_xfer(pcm, bytes);
	return bytes * 8 / str->bits_per_frame;
}

ssize_t snd_pcm_mmap_frames_xfer(snd_pcm_t *pcm, int stream, size_t frames)
{
	snd_pcm_stream_t *str;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->mmap_control)
		return -EBADFD;
	if (stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_mmap_playback_frames_xfer(pcm, frames);
	else
		return snd_pcm_mmap_capture_frames_xfer(pcm, frames);
}

ssize_t snd_pcm_mmap_frames_offset(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	snd_pcm_mmap_control_t *ctrl;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->mmap_control)
		return -EBADFD;
	ctrl = str->mmap_control;
	if (!ctrl)
		return -EBADFD;
	return (ctrl->byte_data % str->setup.buffer_size) * 8 / str->bits_per_frame;
}

int snd_pcm_mmap_stream_state(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_control);
	return str->mmap_control->status;
}

int snd_pcm_mmap_stream_byte_io(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_control);
	return str->mmap_control->byte_io;
}

int snd_pcm_mmap_stream_byte_data(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_control);
	return str->mmap_control->byte_data;
}

ssize_t snd_pcm_mmap_stream_seek(snd_pcm_t *pcm, int stream, off_t offset)
{
	snd_pcm_stream_t *str;
	ssize_t byte_data;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_control);
	byte_data = str->mmap_control->byte_data;
	if (offset == 0)
		return byte_data;
	switch (str->mmap_control->status) {
	case SND_PCM_STATUS_RUNNING:
		if (str->setup.mode == SND_PCM_MODE_FRAME)
			snd_pcm_stream_byte_io(pcm, stream, 1);
		break;
	case SND_PCM_STATUS_PREPARED:
		break;
	default:
		return -EBADFD;
	}
	if (offset < 0) {
		if (offset < -(ssize_t)str->setup.buffer_size)
			offset = -(ssize_t)str->setup.buffer_size;
		else
			offset -= offset % str->setup.bytes_align;
		byte_data += offset;
		if (byte_data < 0)
			byte_data += str->setup.byte_boundary;
	} else {
		size_t bytes_avail;
		if (stream == SND_PCM_STREAM_PLAYBACK)
			bytes_avail = snd_pcm_mmap_playback_bytes_avail(str);
		else
			bytes_avail = snd_pcm_mmap_capture_bytes_avail(str);
		if ((size_t)offset > bytes_avail)
			offset = bytes_avail;
		offset -= offset % str->setup.bytes_align;
		byte_data += offset;
		if ((size_t)byte_data >= str->setup.byte_boundary)
			byte_data -= str->setup.byte_boundary;
	}
	str->mmap_control->byte_data = byte_data;
	return byte_data;
}

ssize_t snd_pcm_mmap_write_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *channels, size_t frames)
{
	snd_pcm_stream_t *str;
	snd_pcm_mmap_control_t *ctrl;
	size_t offset = 0;
	size_t result = 0;
	int err;

	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	ctrl = str->mmap_control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	if (str->setup.mode == SND_PCM_MODE_FRAGMENT) {
		if (frames % str->frames_per_frag != 0)
			return -EINVAL;
	} else {
		if (ctrl->status == SND_PCM_STATUS_RUNNING &&
		    str->mode & SND_PCM_NONBLOCK)
			snd_pcm_stream_byte_io(pcm, SND_PCM_STREAM_PLAYBACK, 1);
	}
	while (frames > 0) {
		ssize_t mmap_offset;
		size_t frames1;
		int ready = snd_pcm_mmap_playback_ready(pcm);
		if (ready < 0)
			return ready;
		if (!ready) {
			struct pollfd pfd;
			if (ctrl->status != SND_PCM_STATUS_RUNNING)
				return result > 0 ? result : -EPIPE;
			if (str->mode & SND_PCM_NONBLOCK)
				return result > 0 ? result : -EAGAIN;
			pfd.fd = snd_pcm_file_descriptor(pcm, SND_PCM_STREAM_PLAYBACK);
			pfd.events = POLLOUT | POLLERR;
			ready = poll(&pfd, 1, 10000);
			if (ready < 0)
				return result > 0 ? result : ready;
			if (ready && pfd.revents & POLLERR)
				return result > 0 ? result : -EPIPE;
			assert(snd_pcm_mmap_playback_ready(pcm));
		}
		frames1 = snd_pcm_mmap_playback_frames_xfer(pcm, frames);
		assert(frames1 > 0);
		mmap_offset = snd_pcm_mmap_frames_offset(pcm, SND_PCM_STREAM_PLAYBACK);
		snd_pcm_areas_copy(channels, offset, str->channels, mmap_offset, str->setup.format.channels, frames1, str->setup.format.format);
		if (ctrl->status == SND_PCM_STATUS_XRUN)
			return result > 0 ? result : -EPIPE;
		snd_pcm_stream_seek(pcm, SND_PCM_STREAM_PLAYBACK, frames1 * str->bits_per_frame / 8);
		frames -= frames1;
		offset += frames1;
		result += frames1;
		if (ctrl->status == SND_PCM_STATUS_PREPARED &&
		    (str->setup.start_mode == SND_PCM_START_DATA ||
		     (str->setup.start_mode == SND_PCM_START_FULL &&
		      !snd_pcm_mmap_playback_ready(pcm)))) {
			err = snd_pcm_stream_go(pcm, SND_PCM_STREAM_PLAYBACK);
			if (err < 0)
				return result > 0 ? result : err;
		}
	}
	return result;
}

ssize_t snd_pcm_mmap_write_frames(snd_pcm_t *pcm, const void *buffer, size_t frames)
{
	snd_pcm_stream_t *str;
	unsigned int nchannels;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (!str->mmap_data || !str->mmap_control)
		return -EBADFD;
	if (frames > 0 && !buffer)
		return -EFAULT;
	nchannels = str->setup.format.channels;
	if (!str->setup.format.interleave && nchannels > 1)
		return -EINVAL;
	{
		snd_pcm_channel_area_t channels[nchannels];
		unsigned int channel;
		for (channel = 0; channel < nchannels; ++channel) {
			channels[channel].addr = (char*)buffer;
			channels[channel].first = str->sample_width * channel;
			channels[channel].step = str->bits_per_frame;
		}
		return snd_pcm_mmap_write_areas(pcm, channels, frames);
	}
}

ssize_t snd_pcm_mmap_write(snd_pcm_t *pcm, const void *buffer, size_t bytes)
{
	snd_pcm_stream_t *str;
	unsigned int nchannels;
	ssize_t frames;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (!str->mmap_data || !str->mmap_control)
		return -EBADFD;
	if (bytes > 0 && !buffer)
		return -EFAULT;
	nchannels = str->setup.format.channels;
	if (!str->setup.format.interleave && nchannels > 1)
		return -EINVAL;
	frames = bytes * 8 / str->bits_per_frame;
	frames = snd_pcm_mmap_write_frames(pcm, buffer, frames);
	if (frames <= 0)
		return frames;
	return frames * str->bits_per_frame / 8;
}

ssize_t snd_pcm_mmap_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long vcount)
{
	snd_pcm_stream_t *str;
	size_t result = 0;
	unsigned int nchannels;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (!str->mmap_data || !str->mmap_control)
		return -EBADFD;
	if (vcount > 0 && !vector)
		return -EFAULT;
	nchannels = str->setup.format.channels;
	if (str->setup.format.interleave) {
		unsigned int b;
		for (b = 0; b < vcount; b++) {
			ssize_t ret;
			size_t frames = vector[b].iov_len * 8 / str->bits_per_frame;
			ret = snd_pcm_mmap_write_frames(pcm, vector[b].iov_base, frames);
			if (ret < 0) {
				if (result <= 0)
					return ret;
				break;
			}
			result += ret;
		}
	} else {
		snd_pcm_channel_area_t channels[nchannels];
		unsigned long bcount;
		unsigned int b;
		if (vcount % nchannels)
			return -EINVAL;
		bcount = vcount / nchannels;
		for (b = 0; b < bcount; b++) {
			unsigned int v;
			ssize_t ret;
			size_t bytes = 0;
			size_t frames;
			bytes = vector[0].iov_len;
			for (v = 0; v < nchannels; ++v) {
				if (vector[v].iov_len != bytes)
					return -EINVAL;
				channels[v].addr = vector[v].iov_base;
				channels[v].first = 0;
				channels[v].step = str->sample_width;
			}
			frames = bytes * 8 / str->sample_width;
			ret = snd_pcm_mmap_write_areas(pcm, channels, frames);
			if (ret < 0) {
				if (result <= 0)
					return ret;
				break;
			}
			result += ret;
			if ((size_t)ret != frames)
				break;
			vector += nchannels;
		}
	}
	return result * str->bits_per_frame / 8;
}

ssize_t snd_pcm_mmap_read_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *channels, size_t frames)
{
	snd_pcm_stream_t *str;
	snd_pcm_mmap_control_t *ctrl;
	size_t offset = 0;
	size_t result = 0;
	int err;

	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	ctrl = str->mmap_control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	if (str->setup.mode == SND_PCM_MODE_FRAGMENT) {
		if (frames % str->frames_per_frag != 0)
			return -EINVAL;
	} else {
		if (ctrl->status == SND_PCM_STATUS_RUNNING &&
		    str->mode & SND_PCM_NONBLOCK)
			snd_pcm_stream_byte_io(pcm, SND_PCM_STREAM_CAPTURE, 1);
	}
	if (ctrl->status == SND_PCM_STATUS_PREPARED &&
	    str->setup.start_mode == SND_PCM_START_DATA) {
		err = snd_pcm_stream_go(pcm, SND_PCM_STREAM_CAPTURE);
		if (err < 0)
			return err;
	}
	while (frames > 0) {
		ssize_t mmap_offset;
		size_t frames1;
		int ready = snd_pcm_mmap_capture_ready(pcm);
		if (ready < 0)
			return ready;
		if (!ready) {
			struct pollfd pfd;
			if (ctrl->status != SND_PCM_STATUS_RUNNING)
				return result > 0 ? result : -EPIPE;
			if (str->mode & SND_PCM_NONBLOCK)
				return result > 0 ? result : -EAGAIN;
			pfd.fd = snd_pcm_file_descriptor(pcm, SND_PCM_STREAM_CAPTURE);
			pfd.events = POLLIN | POLLERR;
			ready = poll(&pfd, 1, 10000);
			if (ready < 0)
				return result > 0 ? result : ready;
			if (ready && pfd.revents & POLLERR)
				return result > 0 ? result : -EPIPE;
			assert(snd_pcm_mmap_capture_ready(pcm));
		}
		frames1 = snd_pcm_mmap_capture_frames_xfer(pcm, frames);
		assert(frames1 > 0);
		mmap_offset = snd_pcm_mmap_frames_offset(pcm, SND_PCM_STREAM_CAPTURE);
		snd_pcm_areas_copy(str->channels, mmap_offset, channels, offset, str->setup.format.channels, frames1, str->setup.format.format);
		if (ctrl->status == SND_PCM_STATUS_XRUN &&
		    str->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
			return result > 0 ? result : -EPIPE;
		snd_pcm_stream_seek(pcm, SND_PCM_STREAM_CAPTURE, frames1 * str->bits_per_frame / 8);
		frames -= frames1;
		offset += frames1;
		result += frames1;
	}
	return result;
}

ssize_t snd_pcm_mmap_read_frames(snd_pcm_t *pcm, const void *buffer, size_t frames)
{
	snd_pcm_stream_t *str;
	unsigned int nchannels;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (!str->mmap_data || !str->mmap_control)
		return -EBADFD;
	if (frames > 0 && !buffer)
		return -EFAULT;
	nchannels = str->setup.format.channels;
	if (!str->setup.format.interleave && nchannels > 1)
		return -EINVAL;
	{
		snd_pcm_channel_area_t channels[nchannels];
		unsigned int channel;
		for (channel = 0; channel < nchannels; ++channel) {
			channels[channel].addr = (char*)buffer;
			channels[channel].first = str->sample_width * channel;
			channels[channel].step = str->bits_per_frame;
		}
		return snd_pcm_mmap_read_areas(pcm, channels, frames);
	}
}

ssize_t snd_pcm_mmap_read(snd_pcm_t *pcm, void *buffer, size_t bytes)
{
	snd_pcm_stream_t *str;
	unsigned int nchannels;
	ssize_t frames;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (!str->mmap_data || !str->mmap_control)
		return -EBADFD;
	if (bytes > 0 && !buffer)
		return -EFAULT;
	nchannels = str->setup.format.channels;
	if (!str->setup.format.interleave && nchannels > 1)
		return -EINVAL;
	frames = bytes * 8 / str->bits_per_frame;
	frames = snd_pcm_mmap_read_frames(pcm, buffer, frames);
	if (frames <= 0)
		return frames;
	return frames * str->bits_per_frame / 8;
}

ssize_t snd_pcm_mmap_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long vcount)
{
	snd_pcm_stream_t *str;
	size_t result = 0;
	unsigned int nchannels;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (!str->mmap_data || !str->mmap_control)
		return -EBADFD;
	if (vcount > 0 && !vector)
		return -EFAULT;
	nchannels = str->setup.format.channels;
	if (str->setup.format.interleave) {
		unsigned int b;
		for (b = 0; b < vcount; b++) {
			ssize_t ret;
			size_t frames = vector[b].iov_len * 8 / str->bits_per_frame;
			ret = snd_pcm_mmap_read_frames(pcm, vector[b].iov_base, frames);
			if (ret < 0) {
				if (result <= 0)
					return ret;
				break;
			}
			result += ret;
		}
	} else {
		snd_pcm_channel_area_t channels[nchannels];
		unsigned long bcount;
		unsigned int b;
		if (vcount % nchannels)
			return -EINVAL;
		bcount = vcount / nchannels;
		for (b = 0; b < bcount; b++) {
			unsigned int v;
			ssize_t ret;
			size_t bytes = 0;
			size_t frames;
			bytes = vector[0].iov_len;
			for (v = 0; v < nchannels; ++v) {
				if (vector[v].iov_len != bytes)
					return -EINVAL;
				channels[v].addr = vector[v].iov_base;
				channels[v].first = 0;
				channels[v].step = str->sample_width;
			}
			frames = bytes * 8 / str->sample_width;
			ret = snd_pcm_mmap_read_areas(pcm, channels, frames);
			if (ret < 0) {
				if (result <= 0)
					return ret;
				break;
			}
			result += ret;
			if ((size_t)ret != frames)
				break;
			vector += nchannels;
		}
	}
	return result * str->bits_per_frame / 8;
}

int snd_pcm_mmap_control(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t **control)
{
	snd_pcm_stream_t *str;
	size_t csize;
	int err;
	if (!pcm || !control)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->valid_setup)
		return -EBADFD;
	if (str->mmap_control) {
		*control = str->mmap_control;
		return 0;
	}
	csize = sizeof(snd_pcm_mmap_control_t);

	if ((err = pcm->ops->mmap_control(pcm, stream, control, csize)) < 0)
		return err;
	str->mmap_control = *control;
	str->mmap_control_size = csize;
	return 0;
}

int snd_pcm_mmap_get_areas(snd_pcm_t *pcm, int stream, snd_pcm_channel_area_t *areas)
{
	snd_pcm_stream_t *str;
	snd_pcm_channel_setup_t s;
	snd_pcm_channel_area_t *a, *ap;
	unsigned int channel;
	int interleaved = 1, noninterleaved = 1;
	int err;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->mmap_data)
		return -EBADFD;
	a = calloc(str->setup.format.channels, sizeof(*areas));
	for (channel = 0, ap = a; channel < str->setup.format.channels; ++channel, ++ap) {
		s.channel = channel;
		err = snd_pcm_channel_setup(pcm, stream, &s);
		if (err < 0) {
			free(a);
			return err;
		}
		if (areas)
			areas[channel] = s.area;
		*ap = s.area;
		if (ap->step != str->sample_width || ap->first != 0)
			noninterleaved = 0;
		if (ap->addr != a[0].addr || 
		    ap->step != str->bits_per_frame || 
		    ap->first != channel * str->sample_width)
			interleaved = 0;
	}
	if (noninterleaved)
		str->mmap_type = _NONINTERLEAVED;
	else if (interleaved)
		str->mmap_type = _INTERLEAVED;
	else
		str->mmap_type = _COMPLEX;
	str->channels = a;
	return 0;
}

int snd_pcm_mmap_data(snd_pcm_t *pcm, int stream, void **data)
{
	snd_pcm_stream_t *str;
	snd_pcm_stream_info_t info;
	size_t bsize;
	int err;
	if (!pcm || !data)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->valid_setup)
		return -EBADFD;
	if (str->mmap_data) {
		*data = str->mmap_data;
		return 0;
	}

	info.stream = stream;
	err = snd_pcm_stream_info(pcm, &info);
	if (err < 0)
		return err;
	bsize = info.mmap_size;
	if (!(info.flags & SND_PCM_STREAM_INFO_MMAP))
		return -ENXIO;
	if ((err = pcm->ops->mmap_data(pcm, stream, data, bsize)) < 0)
		return err;
	str->mmap_data = *data;
	str->mmap_data_size = bsize;
	err = snd_pcm_mmap_get_areas(pcm, stream, NULL);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_mmap(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t **control, void **data)
{
	int err;
	err = snd_pcm_mmap_control(pcm, stream, control);
	if (err < 0)
		return err;
	err = snd_pcm_mmap_data(pcm, stream, data);
	if (err < 0) {
		snd_pcm_munmap_control(pcm, stream);
		return err;
	}
	return 0;
}

int snd_pcm_munmap_control(snd_pcm_t *pcm, int stream)
{
	int err;
	snd_pcm_stream_t *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->mmap_control)
		return -EBADFD;
	if ((err = pcm->ops->munmap_control(pcm, stream, str->mmap_control, str->mmap_control_size)) < 0)
		return err;
	str->mmap_control = 0;
	str->mmap_control_size = 0;
	return 0;
}

int snd_pcm_munmap_data(snd_pcm_t *pcm, int stream)
{
	int err;
	snd_pcm_stream_t *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->mmap_data)
		return -EBADFD;
	if ((err = pcm->ops->munmap_data(pcm, stream, str->mmap_data, str->mmap_data_size)) < 0)
		return err;
	free(str->channels);
	str->mmap_data = 0;
	str->mmap_data_size = 0;
	return 0;
}

int snd_pcm_munmap(snd_pcm_t *pcm, int stream)
{
	int err;
	err = snd_pcm_munmap_control(pcm, stream);
	if (err < 0)
		return err;
	return snd_pcm_munmap_data(pcm, stream);
}

