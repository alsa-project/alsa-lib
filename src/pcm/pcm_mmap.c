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

static void snd_pcm_mmap_clear(snd_pcm_t *pcm, int stream)
{
	struct snd_pcm_stream *str = &pcm->stream[stream];
	str->mmap_control->byte_io = 0;
	str->mmap_control->byte_data = 0;
}

void snd_pcm_mmap_status_streamge(snd_pcm_t *pcm, int stream, int newstatus)
{
	struct snd_pcm_stream *str = &pcm->stream[stream];

	if (!str->mmap_control_emulation)
		return;
	if (newstatus < 0) {
		snd_pcm_stream_status_t status;
		status.stream = stream;
		if (snd_pcm_stream_status(pcm, &status) < 0)
			newstatus = SND_PCM_STATUS_NOTREADY;
		else
			newstatus = status.status;
	}
	if (str->mmap_control->status != newstatus) {
		if (newstatus == SND_PCM_STATUS_READY ||
		    (newstatus == SND_PCM_STATUS_PREPARED &&
		     str->mmap_control->status != SND_PCM_STATUS_READY))
			snd_pcm_mmap_clear(pcm, stream);
		str->mmap_control->status = newstatus;
		pthread_mutex_lock(&str->mutex);
		pthread_cond_signal(&str->status_cond);
		pthread_mutex_unlock(&str->mutex);
	}
}

static inline ssize_t snd_pcm_mmap_playback_bytes_used(struct snd_pcm_stream *str)
{
	ssize_t bytes_used;
	bytes_used = str->mmap_control->byte_data - str->mmap_control->byte_io;
	if (bytes_used < (ssize_t)(str->setup.buffer_size - str->setup.byte_boundary))
		bytes_used += str->setup.byte_boundary;
	return bytes_used;
}

static ssize_t snd_pcm_mmap_playback_frames_used(snd_pcm_t *pcm)
{
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	ssize_t bytes = snd_pcm_mmap_playback_bytes_used(str);
	return bytes * 8 / str->bits_per_frame;
}

static inline size_t snd_pcm_mmap_capture_bytes_used(struct snd_pcm_stream *str)
{
	ssize_t bytes_used;
	bytes_used = str->mmap_control->byte_io - str->mmap_control->byte_data;
	if (bytes_used < 0)
		bytes_used += str->setup.byte_boundary;
	return bytes_used;
}

static size_t snd_pcm_mmap_capture_frames_used(snd_pcm_t *pcm)
{
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	size_t bytes = snd_pcm_mmap_capture_bytes_used(str);
	return bytes * 8 / str->bits_per_frame;
}

int snd_pcm_mmap_frames_used(snd_pcm_t *pcm, int stream, ssize_t *frames)
{
	struct snd_pcm_stream *str;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open || !str->mmap_control)
		return -EBADFD;
	if (stream == SND_PCM_STREAM_PLAYBACK)
		*frames = snd_pcm_mmap_playback_frames_used(pcm);
	else
		*frames = snd_pcm_mmap_capture_frames_used(pcm);
	return 0;
}

static inline size_t snd_pcm_mmap_playback_bytes_free(struct snd_pcm_stream *str)
{
	return str->setup.buffer_size - snd_pcm_mmap_playback_bytes_used(str);
}

static size_t snd_pcm_mmap_playback_frames_free(snd_pcm_t *pcm)
{
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	size_t bytes = snd_pcm_mmap_playback_bytes_free(str);
	return bytes * 8 / str->bits_per_frame;
}

static inline ssize_t snd_pcm_mmap_capture_bytes_free(struct snd_pcm_stream *str)
{
	return str->setup.buffer_size - snd_pcm_mmap_capture_bytes_used(str);
}

static ssize_t snd_pcm_mmap_capture_frames_free(snd_pcm_t *pcm)
{
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	ssize_t bytes = snd_pcm_mmap_capture_bytes_free(str);
	return bytes * 8 / str->bits_per_frame;
}

int snd_pcm_mmap_frames_free(snd_pcm_t *pcm, int stream, ssize_t *frames)
{
	struct snd_pcm_stream *str;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open || !str->mmap_control)
		return -EBADFD;
	if (stream == SND_PCM_STREAM_PLAYBACK)
		*frames = snd_pcm_mmap_playback_frames_free(pcm);
	else
		*frames = snd_pcm_mmap_capture_frames_free(pcm);
	return 0;
}

static int snd_pcm_mmap_playback_ready(snd_pcm_t *pcm)
{
	struct snd_pcm_stream *str;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (str->mmap_control->status == SND_PCM_STATUS_XRUN)
		return -EPIPE;
	return (str->setup.buffer_size - snd_pcm_mmap_playback_bytes_used(str)) >= str->setup.bytes_min;
}

static int snd_pcm_mmap_capture_ready(snd_pcm_t *pcm)
{
	struct snd_pcm_stream *str;
	int ret = 0;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (str->mmap_control->status == SND_PCM_STATUS_XRUN) {
		ret = -EPIPE;
		if (str->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
			return -EPIPE;
	}
	if (snd_pcm_mmap_capture_bytes_used(str) >= str->setup.bytes_min)
		return 1;
	return ret;
}

int snd_pcm_mmap_ready(snd_pcm_t *pcm, int stream)
{
	struct snd_pcm_stream *str;
	snd_pcm_mmap_control_t *ctrl;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open || !str->mmap_control)
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
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	snd_pcm_mmap_control_t *ctrl = str->mmap_control;
	size_t bytes_cont;
	size_t byte_data = ctrl->byte_data;
	size_t bytes_free = snd_pcm_mmap_playback_bytes_free(str);
	if (bytes_free < bytes)
		bytes = bytes_free;
	bytes_cont = str->setup.buffer_size - (byte_data % str->setup.buffer_size);
	if (bytes_cont < bytes)
		bytes = bytes_cont;
	bytes -= bytes % str->setup.bytes_align;
	return bytes;
}

static size_t snd_pcm_mmap_capture_bytes_xfer(snd_pcm_t *pcm, size_t bytes)
{
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	snd_pcm_mmap_control_t *ctrl = str->mmap_control;
	size_t bytes_cont;
	size_t byte_data = ctrl->byte_data;
	size_t bytes_used = snd_pcm_mmap_capture_bytes_used(str);
	if (bytes_used < bytes)
		bytes = bytes_used;
	bytes_cont = str->setup.buffer_size - (byte_data % str->setup.buffer_size);
	if (bytes_cont < bytes)
		bytes = bytes_cont;
	bytes -= bytes % str->setup.bytes_align;
	return bytes;
}

static ssize_t snd_pcm_mmap_playback_frames_xfer(snd_pcm_t *pcm, size_t frames)
{
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	size_t bytes = frames * str->bits_per_frame / 8;
	bytes = snd_pcm_mmap_playback_bytes_xfer(pcm, bytes);
	return bytes * 8 / str->bits_per_frame;
}

static ssize_t snd_pcm_mmap_capture_frames_xfer(snd_pcm_t *pcm, size_t frames)
{
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	size_t bytes = frames * str->bits_per_frame / 8;
	bytes = snd_pcm_mmap_capture_bytes_xfer(pcm, bytes);
	return bytes * 8 / str->bits_per_frame;
}

ssize_t snd_pcm_mmap_frames_xfer(snd_pcm_t *pcm, int stream, size_t frames)
{
	struct snd_pcm_stream *str;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open || !str->mmap_control)
		return -EBADFD;
	if (stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_mmap_playback_frames_xfer(pcm, frames);
	else
		return snd_pcm_mmap_capture_frames_xfer(pcm, frames);
}

ssize_t snd_pcm_mmap_frames_offset(snd_pcm_t *pcm, int stream)
{
	struct snd_pcm_stream *str;
	snd_pcm_mmap_control_t *ctrl;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	ctrl = str->mmap_control;
	if (!ctrl)
		return -EBADFD;
	return (ctrl->byte_data % str->setup.buffer_size) * 8 / str->bits_per_frame;
}

int snd_pcm_mmap_commit_frames(snd_pcm_t *pcm, int stream, int frames)
{
	struct snd_pcm_stream *str;
	snd_pcm_mmap_control_t *ctrl;
	size_t byte_data, bytes;
        if (!pcm)
                return -EFAULT;
        if (stream < 0 || stream > 1)
                return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	ctrl = str->mmap_control;
	if (!ctrl)
		return -EBADFD;
	bytes = frames * str->bits_per_frame;
	if (bytes % 8)
		return -EINVAL;
	bytes /= 8;
	byte_data = ctrl->byte_data + bytes;
	if (byte_data == str->setup.byte_boundary) {
		ctrl->byte_data = 0;
	} else {
		ctrl->byte_data = byte_data;
	}
	return 0;
}

ssize_t snd_pcm_mmap_write_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *channels, size_t frames)
{
	struct snd_pcm_stream *str;
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
			snd_pcm_stream_update(pcm, SND_PCM_STREAM_PLAYBACK);
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
		snd_pcm_mmap_commit_frames(pcm, SND_PCM_STREAM_PLAYBACK, frames1);
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
	struct snd_pcm_stream *str;
	unsigned int nchannels;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
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
	struct snd_pcm_stream *str;
	unsigned int nchannels;
	ssize_t frames;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
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
	struct snd_pcm_stream *str;
	size_t result = 0;
	unsigned int nchannels;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
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
	struct snd_pcm_stream *str;
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
			snd_pcm_stream_update(pcm, SND_PCM_STREAM_CAPTURE);
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
		snd_pcm_mmap_commit_frames(pcm, SND_PCM_STREAM_CAPTURE, frames1);
		frames -= frames1;
		offset += frames1;
		result += frames1;
	}
	return result;
}

ssize_t snd_pcm_mmap_read_frames(snd_pcm_t *pcm, const void *buffer, size_t frames)
{
	struct snd_pcm_stream *str;
	unsigned int nchannels;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
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
	struct snd_pcm_stream *str;
	unsigned int nchannels;
	ssize_t frames;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
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
	struct snd_pcm_stream *str;
	size_t result = 0;
	unsigned int nchannels;
	if (!pcm)
		return -EFAULT;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
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

static ssize_t mmap_playback_bytes_xfer(struct snd_pcm_stream *str)
{
	snd_pcm_mmap_control_t *ctrl = str->mmap_control;
	size_t bytes_cont;
	size_t byte_io = ctrl->byte_io;
	ssize_t bytes = snd_pcm_mmap_playback_bytes_used(str);
	bytes_cont = str->setup.buffer_size - (byte_io % str->setup.buffer_size);
	if ((ssize_t)bytes_cont < bytes)
		bytes = bytes_cont;
	bytes -= bytes % str->setup.bytes_align;
	return bytes;
}

static ssize_t mmap_capture_bytes_xfer(struct snd_pcm_stream *str)
{
	snd_pcm_mmap_control_t *ctrl = str->mmap_control;
	size_t bytes_cont;
	size_t byte_io = ctrl->byte_io;
	ssize_t bytes = snd_pcm_mmap_capture_bytes_free(str);
	bytes_cont = str->setup.buffer_size - (byte_io % str->setup.buffer_size);
	if ((ssize_t)bytes_cont < bytes)
		bytes = bytes_cont;
	bytes -= bytes % str->setup.bytes_align;
	return bytes;
}

static void *playback_mmap(void *d)
{
	snd_pcm_t *pcm = d;
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	snd_pcm_mmap_control_t *control;
	char *data;
	size_t channel_size;
	int channels;
	control = str->mmap_control;
	data = str->mmap_data;
	channels = str->setup.format.channels;
	channel_size = str->mmap_data_size / channels;
	while (1) {
		int err;
		struct pollfd pfd;
		size_t pos, p, bytes;
		if (str->mmap_thread_stop)
			break;

		pthread_mutex_lock(&str->mutex);
		if (control->status != SND_PCM_STATUS_RUNNING) {
			pthread_cond_wait(&str->status_cond, &str->mutex);
			pthread_mutex_unlock(&str->mutex);
			continue;
		}
		pthread_mutex_unlock(&str->mutex);

		pfd.fd = snd_pcm_file_descriptor(pcm, SND_PCM_STREAM_PLAYBACK);
		pfd.events = POLLOUT | POLLERR;
		err = poll(&pfd, 1, -1);
		if (err < 0) {
			fprintf(stderr, "poll err=%d\n", err);
			continue;
		}
		if (pfd.revents & POLLERR) {
			snd_pcm_mmap_status_streamge(pcm, SND_PCM_STREAM_PLAYBACK, -1);
			fprintf(stderr, "pollerr %d\n", control->status);
			continue;
		}

		pos = control->byte_io;
		bytes = mmap_playback_bytes_xfer(str);
		if (bytes <= 0) {
			fprintf(stderr, "underrun\n");
			usleep(10000);
			continue;
		}
		p = pos % str->setup.buffer_size;
		if (str->setup.format.interleave) {
			err = snd_pcm_write(pcm, data + pos, bytes);
		} else {
			struct iovec vector[channels];
			struct iovec *v = vector;
			int channel;
			size_t size = bytes / channels;
			size_t posv = p / channels;
			for (channel = 0; channel < channels; ++channel) {
				v->iov_base = data + channel_size * channel + posv;
				v->iov_len = size;
				v++;
			}
			err = snd_pcm_writev(pcm, vector, channels);
		}
		if (err <= 0) {
			fprintf(stderr, "write err=%d\n", err);
			snd_pcm_mmap_status_streamge(pcm, SND_PCM_STREAM_PLAYBACK, -1);
			continue;
		}
		pthread_mutex_lock(&str->mutex);
		pthread_cond_signal(&str->ready_cond);
		pthread_mutex_unlock(&str->mutex);
		pos += bytes;
		if (pos == str->setup.byte_boundary)
			pos = 0;
		control->byte_io = pos;
	}
	return 0;
}

static void *capture_mmap(void *d)
{
	snd_pcm_t *pcm = d;
	struct snd_pcm_stream *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	snd_pcm_mmap_control_t *control;
	char *data;
	int frags;
	int frag_size, channel_size, channel_frag_size;
	int channels;
	control = str->mmap_control;
	data = str->mmap_data;
	frags = str->setup.frags;
	frag_size = str->setup.frag_size;
	channels = str->setup.format.channels;
	channel_size = str->mmap_data_size / channels;
	channel_frag_size = channel_size / frags;
	while (1) {
		int err;
		struct pollfd pfd;
		size_t pos, p, bytes;
		if (str->mmap_thread_stop)
			break;

		pthread_mutex_lock(&str->mutex);
		if (control->status != SND_PCM_STATUS_RUNNING) {
			pthread_cond_wait(&str->status_cond, &str->mutex);
			pthread_mutex_unlock(&str->mutex);
			continue;
		}
		pthread_mutex_unlock(&str->mutex);

		pfd.fd = snd_pcm_file_descriptor(pcm, SND_PCM_STREAM_CAPTURE);
		pfd.events = POLLIN | POLLERR;
		err = poll(&pfd, 1, -1);
		if (err < 0) {
			fprintf(stderr, "poll err=%d\n", err);
			continue;
		}
		if (pfd.revents & POLLERR) {
			snd_pcm_mmap_status_streamge(pcm, SND_PCM_STREAM_CAPTURE, -1);
			fprintf(stderr, "pollerr %d\n", control->status);
			continue;
		}

		pos = control->byte_io;
		bytes = mmap_capture_bytes_xfer(str);
		if (bytes <= 0) {
			fprintf(stderr, "overrun\n");
			usleep(10000);
			continue;
		}
		p = pos % str->setup.buffer_size;
		if (str->setup.format.interleave) {
			err = snd_pcm_read(pcm, data + pos, bytes);
		} else {
			struct iovec vector[channels];
			struct iovec *v = vector;
			int channel;
			size_t size = bytes / channels;
			size_t posv = p / channels;
			for (channel = 0; channel < channels; ++channel) {
				v->iov_base = data + channel_size * channel + posv;
				v->iov_len = size;
				v++;
			}
			err = snd_pcm_readv(pcm, vector, channels);
		}
		if (err < 0) {
			fprintf(stderr, "read err=%d\n", err);
			snd_pcm_mmap_status_streamge(pcm, SND_PCM_STREAM_CAPTURE, -1);
			continue;
		}
		pthread_mutex_lock(&str->mutex);
		pthread_cond_signal(&str->ready_cond);
		pthread_mutex_unlock(&str->mutex);
		pos += bytes;
		if (pos == str->setup.byte_boundary)
			pos = 0;
		control->byte_io = pos;
	}
	return 0;
}

int snd_pcm_mmap_control(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t **control)
{
	struct snd_pcm_stream *str;
	snd_pcm_stream_info_t info;
	size_t csize;
	int err;
	if (!pcm || !control)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (str->mmap_control) {
		*control = str->mmap_control;
		return 0;
	}
	if (!str->valid_setup)
		return -EBADFD;
	csize = sizeof(snd_pcm_mmap_control_t);

	info.stream = stream;
	err = snd_pcm_stream_info(pcm, &info);
	if (err < 0)
		return err;
	if (info.flags & SND_PCM_STREAM_INFO_MMAP) {
		if ((err = pcm->ops->mmap_control(pcm, stream, control, csize)) < 0)
			return err;
	} else {
		*control = calloc(1, csize);
		str->mmap_control_emulation = 1;
	}
	str->mmap_control = *control;
	str->mmap_control_size = csize;
	return 0;
}

int snd_pcm_mmap_get_areas(snd_pcm_t *pcm, int stream, snd_pcm_channel_area_t *areas)
{
	struct snd_pcm_stream *str;
	snd_pcm_channel_setup_t s;
	snd_pcm_channel_area_t *a, *ap;
	unsigned int channel;
	int err;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open || !str->valid_setup || !str->mmap_data)
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
	}
	str->channels = a;
	return 0;
}

int snd_pcm_mmap_data(snd_pcm_t *pcm, int stream, void **data)
{
	struct snd_pcm_stream *str;
	snd_pcm_stream_info_t info;
	size_t bsize;
	int err;
	if (!pcm || !data)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (str->mmap_data) {
		*data = str->mmap_data;
		return 0;
	}
	if (!str->valid_setup)
		return -EBADFD;

	info.stream = stream;
	err = snd_pcm_stream_info(pcm, &info);
	if (err < 0)
		return err;
	bsize = info.mmap_size;
	if (info.flags & SND_PCM_STREAM_INFO_MMAP) {
		if ((err = pcm->ops->mmap_data(pcm, stream, data, bsize)) < 0)
			return err;
	} else {
		*data = calloc(1, bsize);

		pthread_mutex_init(&str->mutex, NULL);
		pthread_cond_init(&str->status_cond, NULL);
		pthread_cond_init(&str->ready_cond, NULL);
		str->mmap_thread_stop = 0;
		if (stream == SND_PCM_STREAM_PLAYBACK)
			err = pthread_create(&str->mmap_thread, NULL, playback_mmap, pcm);
		else
			err = pthread_create(&str->mmap_thread, NULL, capture_mmap, pcm);
		if (err < 0) {
			pthread_cond_destroy(&str->status_cond);
			pthread_cond_destroy(&str->ready_cond);
			pthread_mutex_destroy(&str->mutex);
			free(*data);
			*data = 0;
			return err;
		}
		str->mmap_data_emulation = 1;
	}
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
	struct snd_pcm_stream *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (!str->mmap_control)
		return -EINVAL;
	if (str->mmap_control_emulation) {
		free(str->mmap_control);
		str->mmap_control_emulation = 0;
	} else {
		if ((err = pcm->ops->munmap_control(pcm, stream, str->mmap_control, str->mmap_control_size)) < 0)
			return err;
	}
	str->mmap_control = 0;
	str->mmap_control_size = 0;
	return 0;
}

int snd_pcm_munmap_data(snd_pcm_t *pcm, int stream)
{
	int err;
	struct snd_pcm_stream *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (!str->mmap_data)
		return -EINVAL;
	if (str->mmap_data_emulation) {
		str->mmap_thread_stop = 1;
		pthread_mutex_lock(&str->mutex);
		pthread_cond_signal(&str->status_cond);
		pthread_mutex_unlock(&str->mutex);
		pthread_join(str->mmap_thread, NULL);
		pthread_cond_destroy(&str->status_cond);
		pthread_cond_destroy(&str->ready_cond);
		pthread_mutex_destroy(&str->mutex);
		free(str->mmap_data);
		str->mmap_data_emulation = 0;
	} else {
		if ((err = pcm->ops->munmap_data(pcm, stream, str->mmap_data, str->mmap_data_size)) < 0)
			return err;
	}
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

