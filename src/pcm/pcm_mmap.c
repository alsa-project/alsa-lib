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

static void snd_pcm_mmap_clear(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan = &pcm->chan[channel];
	chan->mmap_control->byte_io = 0;
	chan->mmap_control->byte_data = 0;
}

void snd_pcm_mmap_status_change(snd_pcm_t *pcm, int channel, int newstatus)
{
	struct snd_pcm_chan *chan = &pcm->chan[channel];

	if (!chan->mmap_control_emulation)
		return;
	if (newstatus < 0) {
		snd_pcm_channel_status_t status;
		status.channel = channel;
		if (snd_pcm_channel_status(pcm, &status) < 0)
			newstatus = SND_PCM_STATUS_NOTREADY;
		else
			newstatus = status.status;
	}
	if (chan->mmap_control->status != newstatus) {
		if (newstatus == SND_PCM_STATUS_READY ||
		    (newstatus == SND_PCM_STATUS_PREPARED &&
		     chan->mmap_control->status != SND_PCM_STATUS_READY))
			snd_pcm_mmap_clear(pcm, channel);
		chan->mmap_control->status = newstatus;
		pthread_mutex_lock(&chan->mutex);
		pthread_cond_signal(&chan->status_cond);
		pthread_mutex_unlock(&chan->mutex);
	}
}

static inline ssize_t snd_pcm_mmap_playback_bytes_used(struct snd_pcm_chan *chan)
{
	ssize_t bytes_used;
	bytes_used = chan->mmap_control->byte_data - chan->mmap_control->byte_io;
	if (bytes_used < (ssize_t)(chan->setup.buffer_size - chan->setup.byte_boundary))
		bytes_used += chan->setup.byte_boundary;
	return bytes_used;
}

static ssize_t snd_pcm_mmap_playback_frames_used(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	ssize_t bytes = snd_pcm_mmap_playback_bytes_used(chan);
	return bytes * 8 / chan->bits_per_frame;
}

static inline size_t snd_pcm_mmap_capture_bytes_used(struct snd_pcm_chan *chan)
{
	ssize_t bytes_used;
	bytes_used = chan->mmap_control->byte_io - chan->mmap_control->byte_data;
	if (bytes_used < 0)
		bytes_used += chan->setup.byte_boundary;
	return bytes_used;
}

static size_t snd_pcm_mmap_capture_frames_used(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	size_t bytes = snd_pcm_mmap_capture_bytes_used(chan);
	return bytes * 8 / chan->bits_per_frame;
}

int snd_pcm_mmap_frames_used(snd_pcm_t *pcm, int channel, ssize_t *frames)
{
	struct snd_pcm_chan *chan;
        if (!pcm)
                return -EFAULT;
        if (channel < 0 || channel > 1)
                return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open || !chan->mmap_control)
		return -EBADFD;
	if (channel == SND_PCM_CHANNEL_PLAYBACK)
		*frames = snd_pcm_mmap_playback_frames_used(pcm);
	else
		*frames = snd_pcm_mmap_capture_frames_used(pcm);
	return 0;
}

static inline size_t snd_pcm_mmap_playback_bytes_free(struct snd_pcm_chan *chan)
{
	return chan->setup.buffer_size - snd_pcm_mmap_playback_bytes_used(chan);
}

static size_t snd_pcm_mmap_playback_frames_free(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	size_t bytes = snd_pcm_mmap_playback_bytes_free(chan);
	return bytes * 8 / chan->bits_per_frame;
}

static inline ssize_t snd_pcm_mmap_capture_bytes_free(struct snd_pcm_chan *chan)
{
	return chan->setup.buffer_size - snd_pcm_mmap_capture_bytes_used(chan);
}

static ssize_t snd_pcm_mmap_capture_frames_free(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	ssize_t bytes = snd_pcm_mmap_capture_bytes_free(chan);
	return bytes * 8 / chan->bits_per_frame;
}

int snd_pcm_mmap_frames_free(snd_pcm_t *pcm, int channel, ssize_t *frames)
{
	struct snd_pcm_chan *chan;
        if (!pcm)
                return -EFAULT;
        if (channel < 0 || channel > 1)
                return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open || !chan->mmap_control)
		return -EBADFD;
	if (channel == SND_PCM_CHANNEL_PLAYBACK)
		*frames = snd_pcm_mmap_playback_frames_free(pcm);
	else
		*frames = snd_pcm_mmap_capture_frames_free(pcm);
	return 0;
}

static int snd_pcm_mmap_playback_ready(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan;
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	if (chan->mmap_control->status == SND_PCM_STATUS_XRUN)
		return -EPIPE;
	return (chan->setup.buffer_size - snd_pcm_mmap_playback_bytes_used(chan)) >= chan->setup.bytes_min;
}

static int snd_pcm_mmap_capture_ready(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan;
	int ret = 0;
	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	if (chan->mmap_control->status == SND_PCM_STATUS_XRUN) {
		ret = -EPIPE;
		if (chan->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
			return -EPIPE;
	}
	if (snd_pcm_mmap_capture_bytes_used(chan) >= chan->setup.bytes_min)
		return 1;
	return ret;
}

int snd_pcm_mmap_ready(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan;
	snd_pcm_mmap_control_t *ctrl;
        if (!pcm)
                return -EFAULT;
        if (channel < 0 || channel > 1)
                return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open || !chan->mmap_control)
		return -EBADFD;
	ctrl = chan->mmap_control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		return snd_pcm_mmap_playback_ready(pcm);
	} else {
		return snd_pcm_mmap_capture_ready(pcm);
	}
}

static size_t snd_pcm_mmap_playback_bytes_xfer(snd_pcm_t *pcm, size_t bytes)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	snd_pcm_mmap_control_t *ctrl = chan->mmap_control;
	size_t bytes_cont;
	size_t byte_data = ctrl->byte_data;
	size_t bytes_free = snd_pcm_mmap_playback_bytes_free(chan);
	if (bytes_free < bytes)
		bytes = bytes_free;
	bytes_cont = chan->setup.buffer_size - (byte_data % chan->setup.buffer_size);
	if (bytes_cont < bytes)
		bytes = bytes_cont;
	bytes -= bytes % chan->setup.bytes_align;
	return bytes;
}

static size_t snd_pcm_mmap_capture_bytes_xfer(snd_pcm_t *pcm, size_t bytes)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	snd_pcm_mmap_control_t *ctrl = chan->mmap_control;
	size_t bytes_cont;
	size_t byte_data = ctrl->byte_data;
	size_t bytes_used = snd_pcm_mmap_capture_bytes_used(chan);
	if (bytes_used < bytes)
		bytes = bytes_used;
	bytes_cont = chan->setup.buffer_size - (byte_data % chan->setup.buffer_size);
	if (bytes_cont < bytes)
		bytes = bytes_cont;
	bytes -= bytes % chan->setup.bytes_align;
	return bytes;
}

static ssize_t snd_pcm_mmap_playback_frames_xfer(snd_pcm_t *pcm, size_t frames)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	size_t bytes = frames * chan->bits_per_frame / 8;
	bytes = snd_pcm_mmap_playback_bytes_xfer(pcm, bytes);
	return bytes * 8 / chan->bits_per_frame;
}

static ssize_t snd_pcm_mmap_capture_frames_xfer(snd_pcm_t *pcm, size_t frames)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	size_t bytes = frames * chan->bits_per_frame / 8;
	bytes = snd_pcm_mmap_capture_bytes_xfer(pcm, bytes);
	return bytes * 8 / chan->bits_per_frame;
}

ssize_t snd_pcm_mmap_frames_xfer(snd_pcm_t *pcm, int channel, size_t frames)
{
	struct snd_pcm_chan *chan;
        if (!pcm)
                return -EFAULT;
        if (channel < 0 || channel > 1)
                return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open || !chan->mmap_control)
		return -EBADFD;
	if (channel == SND_PCM_CHANNEL_PLAYBACK)
		return snd_pcm_mmap_playback_frames_xfer(pcm, frames);
	else
		return snd_pcm_mmap_capture_frames_xfer(pcm, frames);
}

ssize_t snd_pcm_mmap_frames_offset(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan;
	snd_pcm_mmap_control_t *ctrl;
        if (!pcm)
                return -EFAULT;
        if (channel < 0 || channel > 1)
                return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open)
		return -EBADFD;
	ctrl = chan->mmap_control;
	if (!ctrl)
		return -EBADFD;
	return (ctrl->byte_data % chan->setup.buffer_size) * 8 / chan->bits_per_frame;
}

int snd_pcm_mmap_commit_frames(snd_pcm_t *pcm, int channel, int frames)
{
	struct snd_pcm_chan *chan;
	snd_pcm_mmap_control_t *ctrl;
	size_t byte_data, bytes;
        if (!pcm)
                return -EFAULT;
        if (channel < 0 || channel > 1)
                return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open)
		return -EBADFD;
	ctrl = chan->mmap_control;
	if (!ctrl)
		return -EBADFD;
	bytes = frames * chan->bits_per_frame;
	if (bytes % 8)
		return -EINVAL;
	bytes /= 8;
	byte_data = ctrl->byte_data + bytes;
	if (byte_data == chan->setup.byte_boundary) {
		ctrl->byte_data = 0;
	} else {
		ctrl->byte_data = byte_data;
	}
	return 0;
}

ssize_t snd_pcm_mmap_write_areas(snd_pcm_t *pcm, snd_pcm_voice_area_t *voices, size_t frames)
{
	struct snd_pcm_chan *chan;
	snd_pcm_mmap_control_t *ctrl;
	size_t offset = 0;
	size_t result = 0;
	int err;

	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	ctrl = chan->mmap_control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	if (chan->setup.mode == SND_PCM_MODE_BLOCK) {
		if (frames % chan->frames_per_frag != 0)
			return -EINVAL;
	} else {
		if (ctrl->status == SND_PCM_STATUS_RUNNING &&
		    chan->mode & SND_PCM_NONBLOCK)
			snd_pcm_channel_update(pcm, SND_PCM_CHANNEL_PLAYBACK);
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
			if (chan->mode & SND_PCM_NONBLOCK)
				return result > 0 ? result : -EAGAIN;
			pfd.fd = snd_pcm_file_descriptor(pcm, SND_PCM_CHANNEL_PLAYBACK);
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
		mmap_offset = snd_pcm_mmap_frames_offset(pcm, SND_PCM_CHANNEL_PLAYBACK);
		snd_pcm_areas_copy(voices, offset, chan->voices, mmap_offset, chan->setup.format.voices, frames1, chan->setup.format.format);
		if (ctrl->status == SND_PCM_STATUS_XRUN)
			return result > 0 ? result : -EPIPE;
		snd_pcm_mmap_commit_frames(pcm, SND_PCM_CHANNEL_PLAYBACK, frames1);
		frames -= frames1;
		offset += frames1;
		result += frames1;
		if (ctrl->status == SND_PCM_STATUS_PREPARED &&
		    (chan->setup.start_mode == SND_PCM_START_DATA ||
		     (chan->setup.start_mode == SND_PCM_START_FULL &&
		      !snd_pcm_mmap_playback_ready(pcm)))) {
			err = snd_pcm_channel_go(pcm, SND_PCM_CHANNEL_PLAYBACK);
			if (err < 0)
				return result > 0 ? result : err;
		}
	}
	return result;
}

ssize_t snd_pcm_mmap_write_frames(snd_pcm_t *pcm, const void *buffer, size_t frames)
{
	struct snd_pcm_chan *chan;
	unsigned int nvoices;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	if (!chan->open || !chan->valid_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (frames > 0 && !buffer)
		return -EFAULT;
	nvoices = chan->setup.format.voices;
	if (!chan->setup.format.interleave && nvoices > 1)
		return -EINVAL;
	{
		snd_pcm_voice_area_t voices[nvoices];
		unsigned int voice;
		for (voice = 0; voice < nvoices; ++voice) {
			voices[voice].addr = (char*)buffer;
			voices[voice].first = chan->sample_width * voice;
			voices[voice].step = chan->bits_per_frame;
		}
		return snd_pcm_mmap_write_areas(pcm, voices, frames);
	}
}

ssize_t snd_pcm_mmap_write(snd_pcm_t *pcm, const void *buffer, size_t bytes)
{
	struct snd_pcm_chan *chan;
	unsigned int nvoices;
	ssize_t frames;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	if (!chan->open || !chan->valid_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (bytes > 0 && !buffer)
		return -EFAULT;
	nvoices = chan->setup.format.voices;
	if (!chan->setup.format.interleave && nvoices > 1)
		return -EINVAL;
	frames = bytes * 8 / chan->bits_per_frame;
	frames = snd_pcm_mmap_write_frames(pcm, buffer, frames);
	if (frames <= 0)
		return frames;
	return frames * chan->bits_per_frame / 8;
}

ssize_t snd_pcm_mmap_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long vcount)
{
	struct snd_pcm_chan *chan;
	size_t result = 0;
	unsigned int nvoices;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	if (!chan->open || !chan->valid_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (vcount > 0 && !vector)
		return -EFAULT;
	nvoices = chan->setup.format.voices;
	if (chan->setup.format.interleave) {
		unsigned int b;
		for (b = 0; b < vcount; b++) {
			ssize_t ret;
			size_t frames = vector[b].iov_len * 8 / chan->bits_per_frame;
			ret = snd_pcm_mmap_write_frames(pcm, vector[b].iov_base, frames);
			if (ret < 0) {
				if (result <= 0)
					return ret;
				break;
			}
			result += ret;
		}
	} else {
		snd_pcm_voice_area_t voices[nvoices];
		unsigned long bcount;
		unsigned int b;
		if (vcount % nvoices)
			return -EINVAL;
		bcount = vcount / nvoices;
		for (b = 0; b < bcount; b++) {
			unsigned int v;
			ssize_t ret;
			size_t bytes = 0;
			size_t frames;
			bytes = vector[0].iov_len;
			for (v = 0; v < nvoices; ++v) {
				if (vector[v].iov_len != bytes)
					return -EINVAL;
				voices[v].addr = vector[v].iov_base;
				voices[v].first = 0;
				voices[v].step = chan->sample_width;
			}
			frames = bytes * 8 / chan->sample_width;
			ret = snd_pcm_mmap_write_areas(pcm, voices, frames);
			if (ret < 0) {
				if (result <= 0)
					return ret;
				break;
			}
			result += ret;
			if ((size_t)ret != frames)
				break;
			vector += nvoices;
		}
	}
	return result * chan->bits_per_frame / 8;
}

ssize_t snd_pcm_mmap_read_areas(snd_pcm_t *pcm, snd_pcm_voice_area_t *voices, size_t frames)
{
	struct snd_pcm_chan *chan;
	snd_pcm_mmap_control_t *ctrl;
	size_t offset = 0;
	size_t result = 0;
	int err;

	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	ctrl = chan->mmap_control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	if (chan->setup.mode == SND_PCM_MODE_BLOCK) {
		if (frames % chan->frames_per_frag != 0)
			return -EINVAL;
	} else {
		if (ctrl->status == SND_PCM_STATUS_RUNNING &&
		    chan->mode & SND_PCM_NONBLOCK)
			snd_pcm_channel_update(pcm, SND_PCM_CHANNEL_CAPTURE);
	}
	if (ctrl->status == SND_PCM_STATUS_PREPARED &&
	    chan->setup.start_mode == SND_PCM_START_DATA) {
		err = snd_pcm_channel_go(pcm, SND_PCM_CHANNEL_CAPTURE);
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
			if (chan->mode & SND_PCM_NONBLOCK)
				return result > 0 ? result : -EAGAIN;
			pfd.fd = snd_pcm_file_descriptor(pcm, SND_PCM_CHANNEL_CAPTURE);
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
		mmap_offset = snd_pcm_mmap_frames_offset(pcm, SND_PCM_CHANNEL_CAPTURE);
		snd_pcm_areas_copy(chan->voices, mmap_offset, voices, offset, chan->setup.format.voices, frames1, chan->setup.format.format);
		if (ctrl->status == SND_PCM_STATUS_XRUN &&
		    chan->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
			return result > 0 ? result : -EPIPE;
		snd_pcm_mmap_commit_frames(pcm, SND_PCM_CHANNEL_CAPTURE, frames1);
		frames -= frames1;
		offset += frames1;
		result += frames1;
	}
	return result;
}

ssize_t snd_pcm_mmap_read_frames(snd_pcm_t *pcm, const void *buffer, size_t frames)
{
	struct snd_pcm_chan *chan;
	unsigned int nvoices;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	if (!chan->open || !chan->valid_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (frames > 0 && !buffer)
		return -EFAULT;
	nvoices = chan->setup.format.voices;
	if (!chan->setup.format.interleave && nvoices > 1)
		return -EINVAL;
	{
		snd_pcm_voice_area_t voices[nvoices];
		unsigned int voice;
		for (voice = 0; voice < nvoices; ++voice) {
			voices[voice].addr = (char*)buffer;
			voices[voice].first = chan->sample_width * voice;
			voices[voice].step = chan->bits_per_frame;
		}
		return snd_pcm_mmap_read_areas(pcm, voices, frames);
	}
}

ssize_t snd_pcm_mmap_read(snd_pcm_t *pcm, void *buffer, size_t bytes)
{
	struct snd_pcm_chan *chan;
	unsigned int nvoices;
	ssize_t frames;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	if (!chan->open || !chan->valid_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (bytes > 0 && !buffer)
		return -EFAULT;
	nvoices = chan->setup.format.voices;
	if (!chan->setup.format.interleave && nvoices > 1)
		return -EINVAL;
	frames = bytes * 8 / chan->bits_per_frame;
	frames = snd_pcm_mmap_read_frames(pcm, buffer, frames);
	if (frames <= 0)
		return frames;
	return frames * chan->bits_per_frame / 8;
}

ssize_t snd_pcm_mmap_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long vcount)
{
	struct snd_pcm_chan *chan;
	size_t result = 0;
	unsigned int nvoices;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	if (!chan->open || !chan->valid_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (vcount > 0 && !vector)
		return -EFAULT;
	nvoices = chan->setup.format.voices;
	if (chan->setup.format.interleave) {
		unsigned int b;
		for (b = 0; b < vcount; b++) {
			ssize_t ret;
			size_t frames = vector[b].iov_len * 8 / chan->bits_per_frame;
			ret = snd_pcm_mmap_read_frames(pcm, vector[b].iov_base, frames);
			if (ret < 0) {
				if (result <= 0)
					return ret;
				break;
			}
			result += ret;
		}
	} else {
		snd_pcm_voice_area_t voices[nvoices];
		unsigned long bcount;
		unsigned int b;
		if (vcount % nvoices)
			return -EINVAL;
		bcount = vcount / nvoices;
		for (b = 0; b < bcount; b++) {
			unsigned int v;
			ssize_t ret;
			size_t bytes = 0;
			size_t frames;
			bytes = vector[0].iov_len;
			for (v = 0; v < nvoices; ++v) {
				if (vector[v].iov_len != bytes)
					return -EINVAL;
				voices[v].addr = vector[v].iov_base;
				voices[v].first = 0;
				voices[v].step = chan->sample_width;
			}
			frames = bytes * 8 / chan->sample_width;
			ret = snd_pcm_mmap_read_areas(pcm, voices, frames);
			if (ret < 0) {
				if (result <= 0)
					return ret;
				break;
			}
			result += ret;
			if ((size_t)ret != frames)
				break;
			vector += nvoices;
		}
	}
	return result * chan->bits_per_frame / 8;
}

static ssize_t mmap_playback_bytes_xfer(struct snd_pcm_chan *chan)
{
	snd_pcm_mmap_control_t *ctrl = chan->mmap_control;
	size_t bytes_cont;
	size_t byte_io = ctrl->byte_io;
	ssize_t bytes = snd_pcm_mmap_playback_bytes_used(chan);
	bytes_cont = chan->setup.buffer_size - (byte_io % chan->setup.buffer_size);
	if ((ssize_t)bytes_cont < bytes)
		bytes = bytes_cont;
	bytes -= bytes % chan->setup.bytes_align;
	return bytes;
}

static ssize_t mmap_capture_bytes_xfer(struct snd_pcm_chan *chan)
{
	snd_pcm_mmap_control_t *ctrl = chan->mmap_control;
	size_t bytes_cont;
	size_t byte_io = ctrl->byte_io;
	ssize_t bytes = snd_pcm_mmap_capture_bytes_free(chan);
	bytes_cont = chan->setup.buffer_size - (byte_io % chan->setup.buffer_size);
	if ((ssize_t)bytes_cont < bytes)
		bytes = bytes_cont;
	bytes -= bytes % chan->setup.bytes_align;
	return bytes;
}

static void *playback_mmap(void *d)
{
	snd_pcm_t *pcm = d;
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	snd_pcm_mmap_control_t *control;
	char *data;
	size_t voice_size;
	int voices;
	control = chan->mmap_control;
	data = chan->mmap_data;
	voices = chan->setup.format.voices;
	voice_size = chan->mmap_data_size / voices;
	while (1) {
		int err;
		struct pollfd pfd;
		size_t pos, p, bytes;
		if (chan->mmap_thread_stop)
			break;

		pthread_mutex_lock(&chan->mutex);
		if (control->status != SND_PCM_STATUS_RUNNING) {
			pthread_cond_wait(&chan->status_cond, &chan->mutex);
			pthread_mutex_unlock(&chan->mutex);
			continue;
		}
		pthread_mutex_unlock(&chan->mutex);

		pfd.fd = snd_pcm_file_descriptor(pcm, SND_PCM_CHANNEL_PLAYBACK);
		pfd.events = POLLOUT | POLLERR;
		err = poll(&pfd, 1, -1);
		if (err < 0) {
			fprintf(stderr, "poll err=%d\n", err);
			continue;
		}
		if (pfd.revents & POLLERR) {
			snd_pcm_mmap_status_change(pcm, SND_PCM_CHANNEL_PLAYBACK, -1);
			fprintf(stderr, "pollerr %d\n", control->status);
			continue;
		}

		pos = control->byte_io;
		bytes = mmap_playback_bytes_xfer(chan);
		if (bytes <= 0) {
			fprintf(stderr, "underrun\n");
			usleep(10000);
			continue;
		}
		p = pos % chan->setup.buffer_size;
		if (chan->setup.format.interleave) {
			err = snd_pcm_write(pcm, data + pos, bytes);
		} else {
			struct iovec vector[voices];
			struct iovec *v = vector;
			int voice;
			size_t size = bytes / voices;
			size_t posv = p / voices;
			for (voice = 0; voice < voices; ++voice) {
				v->iov_base = data + voice_size * voice + posv;
				v->iov_len = size;
				v++;
			}
			err = snd_pcm_writev(pcm, vector, voices);
		}
		if (err <= 0) {
			fprintf(stderr, "write err=%d\n", err);
			snd_pcm_mmap_status_change(pcm, SND_PCM_CHANNEL_PLAYBACK, -1);
			continue;
		}
		pthread_mutex_lock(&chan->mutex);
		pthread_cond_signal(&chan->ready_cond);
		pthread_mutex_unlock(&chan->mutex);
		pos += bytes;
		if (pos == chan->setup.byte_boundary)
			pos = 0;
		control->byte_io = pos;
	}
	return 0;
}

static void *capture_mmap(void *d)
{
	snd_pcm_t *pcm = d;
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	snd_pcm_mmap_control_t *control;
	char *data;
	int frags;
	int frag_size, voice_size, voice_frag_size;
	int voices;
	control = chan->mmap_control;
	data = chan->mmap_data;
	frags = chan->setup.frags;
	frag_size = chan->setup.frag_size;
	voices = chan->setup.format.voices;
	voice_size = chan->mmap_data_size / voices;
	voice_frag_size = voice_size / frags;
	while (1) {
		int err;
		struct pollfd pfd;
		size_t pos, p, bytes;
		if (chan->mmap_thread_stop)
			break;

		pthread_mutex_lock(&chan->mutex);
		if (control->status != SND_PCM_STATUS_RUNNING) {
			pthread_cond_wait(&chan->status_cond, &chan->mutex);
			pthread_mutex_unlock(&chan->mutex);
			continue;
		}
		pthread_mutex_unlock(&chan->mutex);

		pfd.fd = snd_pcm_file_descriptor(pcm, SND_PCM_CHANNEL_CAPTURE);
		pfd.events = POLLIN | POLLERR;
		err = poll(&pfd, 1, -1);
		if (err < 0) {
			fprintf(stderr, "poll err=%d\n", err);
			continue;
		}
		if (pfd.revents & POLLERR) {
			snd_pcm_mmap_status_change(pcm, SND_PCM_CHANNEL_CAPTURE, -1);
			fprintf(stderr, "pollerr %d\n", control->status);
			continue;
		}

		pos = control->byte_io;
		bytes = mmap_capture_bytes_xfer(chan);
		if (bytes <= 0) {
			fprintf(stderr, "overrun\n");
			usleep(10000);
			continue;
		}
		p = pos % chan->setup.buffer_size;
		if (chan->setup.format.interleave) {
			err = snd_pcm_read(pcm, data + pos, bytes);
		} else {
			struct iovec vector[voices];
			struct iovec *v = vector;
			int voice;
			size_t size = bytes / voices;
			size_t posv = p / voices;
			for (voice = 0; voice < voices; ++voice) {
				v->iov_base = data + voice_size * voice + posv;
				v->iov_len = size;
				v++;
			}
			err = snd_pcm_readv(pcm, vector, voices);
		}
		if (err < 0) {
			fprintf(stderr, "read err=%d\n", err);
			snd_pcm_mmap_status_change(pcm, SND_PCM_CHANNEL_CAPTURE, -1);
			continue;
		}
		pthread_mutex_lock(&chan->mutex);
		pthread_cond_signal(&chan->ready_cond);
		pthread_mutex_unlock(&chan->mutex);
		pos += bytes;
		if (pos == chan->setup.byte_boundary)
			pos = 0;
		control->byte_io = pos;
	}
	return 0;
}

int snd_pcm_mmap_control(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t **control)
{
	struct snd_pcm_chan *chan;
	snd_pcm_channel_info_t info;
	size_t csize;
	int err;
	if (!pcm || !control)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open)
		return -EBADFD;
	if (chan->mmap_control) {
		*control = chan->mmap_control;
		return 0;
	}
	if (!chan->valid_setup)
		return -EBADFD;
	csize = sizeof(snd_pcm_mmap_control_t);

	info.channel = channel;
	err = snd_pcm_channel_info(pcm, &info);
	if (err < 0)
		return err;
	if (info.flags & SND_PCM_CHNINFO_MMAP) {
		if ((err = pcm->ops->mmap_control(pcm, channel, control, csize)) < 0)
			return err;
	} else {
		*control = calloc(1, csize);
		chan->mmap_control_emulation = 1;
	}
	chan->mmap_control = *control;
	chan->mmap_control_size = csize;
	return 0;
}

int snd_pcm_mmap_get_areas(snd_pcm_t *pcm, int channel, snd_pcm_voice_area_t *areas)
{
	struct snd_pcm_chan *chan;
	snd_pcm_voice_setup_t s;
	snd_pcm_voice_area_t *a, *ap;
	unsigned int voice;
	int err;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open || !chan->valid_setup || !chan->mmap_data)
		return -EBADFD;
	a = calloc(chan->setup.format.voices, sizeof(*areas));
	for (voice = 0, ap = a; voice < chan->setup.format.voices; ++voice, ++ap) {
		s.voice = voice;
		err = snd_pcm_voice_setup(pcm, channel, &s);
		if (err < 0) {
			free(a);
			return err;
		}
		if (areas)
			areas[voice] = s.area;
		*ap = s.area;
	}
	chan->voices = a;
	return 0;
}

int snd_pcm_mmap_data(snd_pcm_t *pcm, int channel, void **data)
{
	struct snd_pcm_chan *chan;
	snd_pcm_channel_info_t info;
	size_t bsize;
	int err;
	if (!pcm || !data)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open)
		return -EBADFD;
	if (chan->mmap_data) {
		*data = chan->mmap_data;
		return 0;
	}
	if (!chan->valid_setup)
		return -EBADFD;

	info.channel = channel;
	err = snd_pcm_channel_info(pcm, &info);
	if (err < 0)
		return err;
	bsize = info.mmap_size;
	if (info.flags & SND_PCM_CHNINFO_MMAP) {
		if ((err = pcm->ops->mmap_data(pcm, channel, data, bsize)) < 0)
			return err;
	} else {
		*data = calloc(1, bsize);

		pthread_mutex_init(&chan->mutex, NULL);
		pthread_cond_init(&chan->status_cond, NULL);
		pthread_cond_init(&chan->ready_cond, NULL);
		chan->mmap_thread_stop = 0;
		if (channel == SND_PCM_CHANNEL_PLAYBACK)
			err = pthread_create(&chan->mmap_thread, NULL, playback_mmap, pcm);
		else
			err = pthread_create(&chan->mmap_thread, NULL, capture_mmap, pcm);
		if (err < 0) {
			pthread_cond_destroy(&chan->status_cond);
			pthread_cond_destroy(&chan->ready_cond);
			pthread_mutex_destroy(&chan->mutex);
			free(*data);
			*data = 0;
			return err;
		}
		chan->mmap_data_emulation = 1;
	}
	chan->mmap_data = *data;
	chan->mmap_data_size = bsize;
	err = snd_pcm_mmap_get_areas(pcm, channel, NULL);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_mmap(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t **control, void **data)
{
	int err;
	err = snd_pcm_mmap_control(pcm, channel, control);
	if (err < 0)
		return err;
	err = snd_pcm_mmap_data(pcm, channel, data);
	if (err < 0) {
		snd_pcm_munmap_control(pcm, channel);
		return err;
	}
	return 0;
}

int snd_pcm_munmap_control(snd_pcm_t *pcm, int channel)
{
	int err;
	struct snd_pcm_chan *chan;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open)
		return -EBADFD;
	if (!chan->mmap_control)
		return -EINVAL;
	if (chan->mmap_control_emulation) {
		free(chan->mmap_control);
		chan->mmap_control_emulation = 0;
	} else {
		if ((err = pcm->ops->munmap_control(pcm, channel, chan->mmap_control, chan->mmap_control_size)) < 0)
			return err;
	}
	chan->mmap_control = 0;
	chan->mmap_control_size = 0;
	return 0;
}

int snd_pcm_munmap_data(snd_pcm_t *pcm, int channel)
{
	int err;
	struct snd_pcm_chan *chan;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open)
		return -EBADFD;
	if (!chan->mmap_data)
		return -EINVAL;
	if (chan->mmap_data_emulation) {
		chan->mmap_thread_stop = 1;
		pthread_mutex_lock(&chan->mutex);
		pthread_cond_signal(&chan->status_cond);
		pthread_mutex_unlock(&chan->mutex);
		pthread_join(chan->mmap_thread, NULL);
		pthread_cond_destroy(&chan->status_cond);
		pthread_cond_destroy(&chan->ready_cond);
		pthread_mutex_destroy(&chan->mutex);
		free(chan->mmap_data);
		chan->mmap_data_emulation = 0;
	} else {
		if ((err = pcm->ops->munmap_data(pcm, channel, chan->mmap_data, chan->mmap_data_size)) < 0)
			return err;
	}
	free(chan->voices);
	chan->mmap_data = 0;
	chan->mmap_data_size = 0;
	return 0;
}

int snd_pcm_munmap(snd_pcm_t *pcm, int channel)
{
	int err;
	err = snd_pcm_munmap_control(pcm, channel);
	if (err < 0)
		return err;
	return snd_pcm_munmap_data(pcm, channel);
}

