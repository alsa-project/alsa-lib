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
#include <sys/poll.h>
#include <sys/uio.h>
#include "pcm_local.h"

int snd_pcm_frames_avail(snd_pcm_t *pcm, int stream, ssize_t *frames)
{
	snd_pcm_stream_t *str;
        assert(pcm);
        assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_status && str->mmap_control);
	if (stream == SND_PCM_STREAM_PLAYBACK)
		*frames = snd_pcm_mmap_playback_frames_avail(str);
	else
		*frames = snd_pcm_mmap_capture_frames_avail(str);
	return 0;
}

static int snd_pcm_mmap_playback_ready(snd_pcm_t *pcm)
{
	snd_pcm_stream_t *str;
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	if (str->mmap_status->state == SND_PCM_STATE_XRUN)
		return -EPIPE;
	return snd_pcm_mmap_playback_frames_avail(str) >= str->setup.frames_min;
}

static int snd_pcm_mmap_capture_ready(snd_pcm_t *pcm)
{
	snd_pcm_stream_t *str;
	int ret = 0;
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	if (str->mmap_status->state == SND_PCM_STATE_XRUN) {
		ret = -EPIPE;
		if (str->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
			return -EPIPE;
	}
	if (snd_pcm_mmap_capture_frames_avail(str) >= str->setup.frames_min)
		return 1;
	return ret;
}

int snd_pcm_mmap_ready(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
        assert(pcm);
        assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_status && str->mmap_control);
	assert(str->mmap_status->state >= SND_PCM_STATE_PREPARED);
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		return snd_pcm_mmap_playback_ready(pcm);
	} else {
		return snd_pcm_mmap_capture_ready(pcm);
	}
}

static size_t snd_pcm_mmap_playback_frames_xfer(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	snd_pcm_mmap_control_t *control = str->mmap_control;
	size_t frames_cont;
	size_t frames_avail = snd_pcm_mmap_playback_frames_avail(str);
	if (frames_avail < frames)
		frames = frames_avail;
	frames_cont = str->setup.buffer_size - control->frame_data % str->setup.buffer_size;
	if (frames_cont < frames)
		frames = frames_cont;
	return frames;
}

static size_t snd_pcm_mmap_capture_frames_xfer(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	snd_pcm_mmap_control_t *control = str->mmap_control;
	size_t frames_cont;
	size_t frames_avail = snd_pcm_mmap_capture_frames_avail(str);
	if (frames_avail < frames)
		frames = frames_avail;
	frames_cont = str->setup.buffer_size - control->frame_data % str->setup.buffer_size;
	if (frames_cont < frames)
		frames = frames_cont;
	return frames;
}

ssize_t snd_pcm_mmap_frames_xfer(snd_pcm_t *pcm, int stream, size_t frames)
{
	snd_pcm_stream_t *str;
        assert(pcm);
        assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_status && str->mmap_control);
	if (stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_mmap_playback_frames_xfer(pcm, frames);
	else
		return snd_pcm_mmap_capture_frames_xfer(pcm, frames);
}

ssize_t snd_pcm_mmap_frames_offset(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
        assert(pcm);
        assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_control);
	return str->mmap_control->frame_data % str->setup.buffer_size;
}

int snd_pcm_mmap_stream_state(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_status);
	return str->mmap_status->state;
}

int snd_pcm_mmap_stream_frame_io(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_status);
	return str->mmap_status->frame_io;
}

ssize_t snd_pcm_mmap_stream_frame_data(snd_pcm_t *pcm, int stream, off_t offset)
{
	snd_pcm_stream_t *str;
	ssize_t frame_data;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_status && str->mmap_control);
	frame_data = str->mmap_control->frame_data;
	if (offset == 0)
		return frame_data;
	switch (str->mmap_status->state) {
	case SND_PCM_STATE_RUNNING:
		if (str->setup.mode == SND_PCM_MODE_FRAME)
			snd_pcm_stream_frame_io(pcm, stream, 1);
		break;
	case SND_PCM_STATE_PREPARED:
		break;
	default:
		return -EBADFD;
	}
	if (offset < 0) {
		if (offset < -(ssize_t)str->setup.buffer_size)
			offset = -(ssize_t)str->setup.buffer_size;
		else
			offset -= offset % str->setup.frames_align;
		frame_data += offset;
		if (frame_data < 0)
			frame_data += str->setup.frame_boundary;
	} else {
		size_t frames_avail;
		if (stream == SND_PCM_STREAM_PLAYBACK)
			frames_avail = snd_pcm_mmap_playback_frames_avail(str);
		else
			frames_avail = snd_pcm_mmap_capture_frames_avail(str);
		if ((size_t)offset > frames_avail)
			offset = frames_avail;
		offset -= offset % str->setup.frames_align;
		frame_data += offset;
		if ((size_t)frame_data >= str->setup.frame_boundary)
			frame_data -= str->setup.frame_boundary;
	}
	str->mmap_control->frame_data = frame_data;
	return frame_data;
}

ssize_t snd_pcm_mmap_write_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *channels, size_t frames)
{
	snd_pcm_stream_t *str;
	snd_pcm_mmap_status_t *status;
	size_t offset = 0;
	size_t result = 0;
	int err;

	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	assert(str->mmap_data && str->mmap_status && str->mmap_control);
	status = str->mmap_status;
	assert(status->state >= SND_PCM_STATE_PREPARED);
	if (str->setup.mode == SND_PCM_MODE_FRAGMENT) {
		assert(frames % str->setup.frag_size == 0);
	} else {
		if (status->state == SND_PCM_STATE_RUNNING &&
		    str->mode & SND_PCM_NONBLOCK)
			snd_pcm_stream_frame_io(pcm, SND_PCM_STREAM_PLAYBACK, 1);
	}
	while (frames > 0) {
		ssize_t mmap_offset;
		size_t frames1;
		int ready = snd_pcm_mmap_playback_ready(pcm);
		if (ready < 0)
			return ready;
		if (!ready) {
			struct pollfd pfd;
			if (status->state != SND_PCM_STATE_RUNNING)
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
		if (status->state == SND_PCM_STATE_XRUN)
			return result > 0 ? result : -EPIPE;
		snd_pcm_stream_frame_data(pcm, SND_PCM_STREAM_PLAYBACK, frames1);
		frames -= frames1;
		offset += frames1;
		result += frames1;
		if (status->state == SND_PCM_STATE_PREPARED &&
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

ssize_t snd_pcm_mmap_write(snd_pcm_t *pcm, const void *buffer, size_t frames)
{
	snd_pcm_stream_t *str;
	unsigned int nchannels;
	assert(pcm);
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	assert(str->mmap_data && str->mmap_status && str->mmap_control);
	assert(frames == 0 || buffer);
	nchannels = str->setup.format.channels;
	assert(str->setup.format.interleave || nchannels == 1);
	{
		snd_pcm_channel_area_t channels[nchannels];
		unsigned int channel;
		for (channel = 0; channel < nchannels; ++channel) {
			channels[channel].addr = (char*)buffer;
			channels[channel].first = str->bits_per_sample * channel;
			channels[channel].step = str->bits_per_frame;
		}
		return snd_pcm_mmap_write_areas(pcm, channels, frames);
	}
}

ssize_t snd_pcm_mmap_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long vcount)
{
	snd_pcm_stream_t *str;
	size_t result = 0;
	unsigned int nchannels;
	assert(pcm);
	str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
	assert(str->mmap_data && str->mmap_status && str->mmap_control);
	assert(vcount == 0 || vector);
	nchannels = str->setup.format.channels;
	if (str->setup.format.interleave) {
		unsigned int b;
		for (b = 0; b < vcount; b++) {
			ssize_t ret;
			size_t frames = vector[b].iov_len;
			ret = snd_pcm_mmap_write(pcm, vector[b].iov_base, frames);
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
		assert(vcount % nchannels == 0);
		bcount = vcount / nchannels;
		for (b = 0; b < bcount; b++) {
			unsigned int v;
			ssize_t ret;
			size_t frames = vector[0].iov_len;
			for (v = 0; v < nchannels; ++v) {
				assert(vector[v].iov_len == frames);
				channels[v].addr = vector[v].iov_base;
				channels[v].first = 0;
				channels[v].step = str->bits_per_sample;
			}
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
	return result;
}

ssize_t snd_pcm_mmap_read_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *channels, size_t frames)
{
	snd_pcm_stream_t *str;
	snd_pcm_mmap_status_t *status;
	size_t offset = 0;
	size_t result = 0;
	int err;

	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	assert(str->mmap_data && str->mmap_status && str->mmap_control);
	status = str->mmap_status;
	assert(status->state >= SND_PCM_STATE_PREPARED);
	if (str->setup.mode == SND_PCM_MODE_FRAGMENT) {
		assert(frames % str->setup.frag_size == 0);
	} else {
		if (status->state == SND_PCM_STATE_RUNNING &&
		    str->mode & SND_PCM_NONBLOCK)
			snd_pcm_stream_frame_io(pcm, SND_PCM_STREAM_CAPTURE, 1);
	}
	if (status->state == SND_PCM_STATE_PREPARED &&
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
			if (status->state != SND_PCM_STATE_RUNNING)
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
		if (status->state == SND_PCM_STATE_XRUN &&
		    str->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
			return result > 0 ? result : -EPIPE;
		snd_pcm_stream_frame_data(pcm, SND_PCM_STREAM_CAPTURE, frames1);
		frames -= frames1;
		offset += frames1;
		result += frames1;
	}
	return result;
}

ssize_t snd_pcm_mmap_read(snd_pcm_t *pcm, void *buffer, size_t frames)
{
	snd_pcm_stream_t *str;
	unsigned int nchannels;
	assert(pcm);
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	assert(str->mmap_data && str->mmap_status && str->mmap_control);
	assert(frames == 0 || buffer);
	nchannels = str->setup.format.channels;
	assert(str->setup.format.interleave || nchannels == 1);
	{
		snd_pcm_channel_area_t channels[nchannels];
		unsigned int channel;
		for (channel = 0; channel < nchannels; ++channel) {
			channels[channel].addr = (char*)buffer;
			channels[channel].first = str->bits_per_sample * channel;
			channels[channel].step = str->bits_per_frame;
		}
		return snd_pcm_mmap_read_areas(pcm, channels, frames);
	}
}

ssize_t snd_pcm_mmap_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long vcount)
{
	snd_pcm_stream_t *str;
	size_t result = 0;
	unsigned int nchannels;
	assert(pcm);
	str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
	assert(str->mmap_data && str->mmap_status && str->mmap_control);
	assert(vcount == 0 || vector);
	nchannels = str->setup.format.channels;
	if (str->setup.format.interleave) {
		unsigned int b;
		for (b = 0; b < vcount; b++) {
			ssize_t ret;
			size_t frames = vector[b].iov_len;
			ret = snd_pcm_mmap_read(pcm, vector[b].iov_base, frames);
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
		assert(vcount % nchannels == 0);
		bcount = vcount / nchannels;
		for (b = 0; b < bcount; b++) {
			unsigned int v;
			ssize_t ret;
			size_t frames = vector[0].iov_len;
			for (v = 0; v < nchannels; ++v) {
				assert(vector[v].iov_len == frames);
				channels[v].addr = vector[v].iov_base;
				channels[v].first = 0;
				channels[v].step = str->bits_per_sample;
			}
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
	return result;
}

int snd_pcm_mmap_status(snd_pcm_t *pcm, int stream, snd_pcm_mmap_status_t **status)
{
	snd_pcm_stream_t *str;
	int err;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->valid_setup);
	if (str->mmap_status) {
		if (status)
			*status = str->mmap_status;
		return 0;
	}

	if ((err = pcm->ops->mmap_status(pcm, stream, &str->mmap_status)) < 0)
		return err;
	if (status)
		*status = str->mmap_status;
	return 0;
}

int snd_pcm_mmap_control(snd_pcm_t *pcm, int stream, snd_pcm_mmap_control_t **control)
{
	snd_pcm_stream_t *str;
	int err;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->valid_setup);
	if (str->mmap_control) {
		if (control)
			*control = str->mmap_control;
		return 0;
	}

	if ((err = pcm->ops->mmap_control(pcm, stream, &str->mmap_control)) < 0)
		return err;
	if (control)
		*control = str->mmap_control;
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
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_data);
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
		if (ap->step != str->bits_per_sample || ap->first != 0)
			noninterleaved = 0;
		if (ap->addr != a[0].addr || 
		    ap->step != str->bits_per_frame || 
		    ap->first != channel * str->bits_per_sample)
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
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->valid_setup);
	if (str->mmap_data) {
		if (data)
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
	if ((err = pcm->ops->mmap_data(pcm, stream, (void**)&str->mmap_data, bsize)) < 0)
		return err;
	if (data) 
		*data = str->mmap_data;
	str->mmap_data_size = bsize;
	err = snd_pcm_mmap_get_areas(pcm, stream, NULL);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_mmap(snd_pcm_t *pcm, int stream, snd_pcm_mmap_status_t **status, snd_pcm_mmap_control_t **control, void **data)
{
	int err;
	err = snd_pcm_mmap_status(pcm, stream, status);
	if (err < 0)
		return err;
	err = snd_pcm_mmap_control(pcm, stream, control);
	if (err < 0) {
		snd_pcm_munmap_status(pcm, stream);
		return err;
	}
	err = snd_pcm_mmap_data(pcm, stream, data);
	if (err < 0) {
		snd_pcm_munmap_status(pcm, stream);
		snd_pcm_munmap_control(pcm, stream);
		return err;
	}
	return 0;
}

int snd_pcm_munmap_status(snd_pcm_t *pcm, int stream)
{
	int err;
	snd_pcm_stream_t *str;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_status);
	if ((err = pcm->ops->munmap_status(pcm, stream, str->mmap_status)) < 0)
		return err;
	str->mmap_status = 0;
	return 0;
}

int snd_pcm_munmap_control(snd_pcm_t *pcm, int stream)
{
	int err;
	snd_pcm_stream_t *str;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_control);
	if ((err = pcm->ops->munmap_control(pcm, stream, str->mmap_control)) < 0)
		return err;
	str->mmap_control = 0;
	return 0;
}

int snd_pcm_munmap_data(snd_pcm_t *pcm, int stream)
{
	int err;
	snd_pcm_stream_t *str;
	assert(pcm);
	assert(stream >= 0 && stream <= 1);
	str = &pcm->stream[stream];
	assert(str->mmap_data);
	if ((err = pcm->ops->munmap_data(pcm, stream, str->mmap_data, str->mmap_data_size)) < 0)
		return err;
	free(str->channels);
	str->channels = 0;
	str->mmap_data = 0;
	str->mmap_data_size = 0;
	return 0;
}

int snd_pcm_munmap(snd_pcm_t *pcm, int stream)
{
	int err;
	err = snd_pcm_munmap_status(pcm, stream);
	if (err < 0)
		return err;
	err = snd_pcm_munmap_control(pcm, stream);
	if (err < 0)
		return err;
	return snd_pcm_munmap_data(pcm, stream);
}

