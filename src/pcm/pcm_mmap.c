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
	chan->mmap_control->frag_io = 0;
	chan->mmap_control->frag_data = 0;
	chan->mmap_control->pos_io = 0;
	chan->mmap_control->pos_data = 0;
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

static ssize_t snd_pcm_mmap_playback_frags_used(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan;
	ssize_t frags_used;
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	frags_used = chan->mmap_control->frag_data - chan->mmap_control->frag_io;
	if (frags_used < (ssize_t)(chan->setup.frags - chan->setup.frag_boundary))
		frags_used += chan->setup.frag_boundary;
	return frags_used;
}

static size_t snd_pcm_mmap_capture_frags_used(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan;
	ssize_t frags_used;
	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	frags_used = chan->mmap_control->frag_io - chan->mmap_control->frag_data;
	if (frags_used < 0)
		frags_used += chan->setup.frag_boundary;
	return frags_used;
}

static size_t snd_pcm_mmap_playback_frags_free(snd_pcm_t *pcm)
{
	return pcm->chan[SND_PCM_CHANNEL_PLAYBACK].setup.frags - snd_pcm_mmap_playback_frags_used(pcm);
}

static ssize_t snd_pcm_mmap_capture_frags_free(snd_pcm_t *pcm)
{
	return pcm->chan[SND_PCM_CHANNEL_CAPTURE].setup.frags - snd_pcm_mmap_capture_frags_used(pcm);
}

int snd_pcm_mmap_frags_used(snd_pcm_t *pcm, int channel, ssize_t *frags)
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
		*frags = snd_pcm_mmap_playback_frags_used(pcm);
	else
		*frags = snd_pcm_mmap_capture_frags_used(pcm);
	return 0;
}

int snd_pcm_mmap_frags_free(snd_pcm_t *pcm, int channel, ssize_t *frags)
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
		*frags = snd_pcm_mmap_playback_frags_free(pcm);
	else
		*frags = snd_pcm_mmap_capture_frags_free(pcm);
	return 0;
}

static ssize_t snd_pcm_mmap_playback_bytes_used(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan;
	ssize_t bytes_used;
	// snd_pcm_update_pos(pcm, SND_PCM_CHANNEL_PLAYBACK);
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	bytes_used = chan->mmap_control->pos_data - chan->mmap_control->pos_io;
	if (bytes_used < (ssize_t)(chan->setup.buffer_size - chan->setup.pos_boundary))
		bytes_used += chan->setup.pos_boundary;
	return bytes_used;
}

static size_t snd_pcm_mmap_capture_bytes_used(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan;
	ssize_t bytes_used;
	// snd_pcm_update_pos(pcm, SND_PCM_CHANNEL_CAPTURE);
	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	bytes_used = chan->mmap_control->pos_io - chan->mmap_control->pos_data;
	if (bytes_used < 0)
		bytes_used += chan->setup.pos_boundary;
	return bytes_used;
}

int snd_pcm_mmap_bytes_used(snd_pcm_t *pcm, int channel, ssize_t *frags)
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
		*frags = snd_pcm_mmap_playback_bytes_used(pcm);
	else
		*frags = snd_pcm_mmap_capture_bytes_used(pcm);
	return 0;
}

static size_t snd_pcm_mmap_playback_bytes_free(snd_pcm_t *pcm)
{
	return pcm->chan[SND_PCM_CHANNEL_PLAYBACK].setup.buffer_size - snd_pcm_mmap_playback_bytes_used(pcm);
}

static ssize_t snd_pcm_mmap_capture_bytes_free(snd_pcm_t *pcm)
{
	return pcm->chan[SND_PCM_CHANNEL_CAPTURE].setup.buffer_size - snd_pcm_mmap_capture_bytes_used(pcm);
}

int snd_pcm_mmap_bytes_free(snd_pcm_t *pcm, int channel, ssize_t *frags)
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
		*frags = snd_pcm_mmap_playback_bytes_free(pcm);
	else
		*frags = snd_pcm_mmap_capture_bytes_free(pcm);
	return 0;
}

static int snd_pcm_mmap_playback_ready(snd_pcm_t *pcm)
{
	struct snd_pcm_chan *chan;
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	if (chan->mmap_control->status == SND_PCM_STATUS_XRUN)
		return -EPIPE;
	if (chan->setup.mode == SND_PCM_MODE_BLOCK) {
		return (chan->setup.frags - snd_pcm_mmap_playback_frags_used(pcm)) >= chan->setup.buf.block.frags_min;
	} else {
		return (chan->setup.buffer_size - snd_pcm_mmap_playback_bytes_used(pcm)) >= chan->setup.buf.stream.bytes_min;
	}
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
	if (chan->setup.mode == SND_PCM_MODE_BLOCK) {
		if (snd_pcm_mmap_capture_frags_used(pcm) >= chan->setup.buf.block.frags_min)
			return 1;
	} else {
		if (snd_pcm_mmap_capture_bytes_used(pcm) >= chan->setup.buf.stream.bytes_min)
			return 1;
	}
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

/* Bytes transferrable */
static size_t snd_pcm_mmap_bytes_playback(snd_pcm_t *pcm, size_t size)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	snd_pcm_mmap_control_t *ctrl = chan->mmap_control;
	size_t bytes_cont, bytes_free;
	unsigned int pos_data = ctrl->pos_data;
	unsigned int pos_io = ctrl->pos_io;
	int bytes_used = pos_data - pos_io;
	if (bytes_used < -(int)(chan->setup.buf.stream.bytes_xrun_max + chan->setup.frag_size))
		bytes_used += chan->setup.pos_boundary;
	bytes_cont = chan->setup.buffer_size - (pos_data % chan->setup.buffer_size);
	if (bytes_cont < size)
		size = bytes_cont;
	bytes_free = chan->setup.buffer_size - bytes_used;
	if (bytes_free < size)
		size = (bytes_free / chan->setup.buf.stream.bytes_align) * chan->setup.buf.stream.bytes_align;
	return size;
}

/* Bytes transferrable */
static size_t snd_pcm_mmap_bytes_capture(snd_pcm_t *pcm, size_t size)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	snd_pcm_mmap_control_t *ctrl = chan->mmap_control;
	size_t bytes_cont;
	unsigned int pos_data = ctrl->pos_data;
	unsigned int pos_io = ctrl->pos_io;
	int bytes_used = pos_io - pos_data;
	if (bytes_used < 0)
		bytes_used += chan->setup.pos_boundary;
	bytes_cont = chan->setup.buffer_size - (pos_data % chan->setup.buffer_size);
	if (bytes_cont < size)
		size = bytes_cont;
	if ((size_t) bytes_used < size)
		size = (bytes_used / chan->setup.buf.stream.bytes_align) * chan->setup.buf.stream.bytes_align;
	return size;
}

typedef int (*transfer_f)(snd_pcm_t *pcm, size_t hwoff, void *data, size_t off, size_t size);


static ssize_t snd_pcm_mmap_write1(snd_pcm_t *pcm, const void *data, size_t count, transfer_f transfer)
{
	struct snd_pcm_chan *chan;
	snd_pcm_mmap_control_t *ctrl;
	size_t frag_size;
	size_t offset = 0;
	size_t result = 0;
	int err;

	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	ctrl = chan->mmap_control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	frag_size = chan->setup.frag_size;
	if (chan->setup.mode == SND_PCM_MODE_BLOCK) {
		if (count % frag_size != 0)
			return -EINVAL;
	} else {
		int tmp = snd_pcm_format_size(chan->setup.format.format, chan->setup.format.voices);
		if (tmp > 0) {
	                int r = count % tmp;
			if (r > 0) {
				count -= r;
				if (count == 0)
					return -EINVAL;
			}
                }
	}
	while (count > 0) {
		size_t bytes;
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
		if (chan->setup.mode == SND_PCM_MODE_BLOCK) {
			size_t frag_data, frag;
			frag_data = ctrl->frag_data;
			frag = frag_data % chan->setup.frags;
			err = transfer(pcm, frag_size * frag, (void *) data, offset, frag_size);
			if (err < 0) 
				return result > 0 ? result : err;
			if (ctrl->status == SND_PCM_STATUS_XRUN)
				return result > 0 ? result : -EPIPE;
			frag_data++;
			if (frag_data == chan->setup.frag_boundary) {
				ctrl->frag_data = 0;
				ctrl->pos_data = 0;
			} else {
				ctrl->frag_data = frag_data;
				ctrl->pos_data += frag_size;
			}
			bytes = frag_size;
		} else {
			size_t pos_data;
			bytes = snd_pcm_mmap_bytes_playback(pcm, count);
			pos_data = ctrl->pos_data;
			err = transfer(pcm, pos_data % chan->setup.buffer_size, (void *) data, offset, bytes);
			if (err < 0) 
				return result > 0 ? result : err;
			if (ctrl->status == SND_PCM_STATUS_XRUN)
				return result > 0 ? result : -EPIPE;
			pos_data += bytes;
			if (pos_data == chan->setup.pos_boundary) {
				ctrl->pos_data = 0;
				ctrl->frag_data = 0;
			} else {
				ctrl->pos_data = pos_data;
				ctrl->frag_data = pos_data / chan->setup.frags;
			}
		}
		offset += bytes;
		count -= bytes;
		result += bytes;
	}
	
	if (ctrl->status == SND_PCM_STATUS_PREPARED &&
	    (chan->setup.start_mode == SND_PCM_START_DATA ||
	     (chan->setup.start_mode == SND_PCM_START_FULL &&
	      !snd_pcm_mmap_playback_ready(pcm)))) {
		err = snd_pcm_channel_go(pcm, SND_PCM_CHANNEL_PLAYBACK);
		if (err < 0)
			return result > 0 ? result : err;
	}
	return result;
}

static int transfer_write(snd_pcm_t *pcm, size_t hwoff, void* data, size_t off, size_t size)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	const char *buf = data;
	unsigned int v, voices;
#define COPY_LABELS
#include "plugin/plugin_ops.h"
#undef COPY_LABELS
	void *copy;
	snd_pcm_voice_setup_t *vsetup;
	int idx;
	size_t vsize, ssize;
	idx = copy_index(chan->setup.format.format);
	if (idx < 0)
		return idx;
	copy = copy_labels[idx];
	voices = chan->setup.format.voices;
	vsetup = chan->voices_setup;
	vsize = snd_pcm_format_size(chan->setup.format.format, 1);
	ssize = vsize * chan->setup.format.voices;
	hwoff /= ssize;
	size /= ssize;
	for (v = 0; v < voices; ++v, ++vsetup) {
		const char *src;
		char *dst;
		size_t dst_step;
		size_t samples = size;
		if (vsetup->first % 8 != 0 ||
		    vsetup->step % 8 != 0)
			return -EINVAL;
		src = buf + off + v * vsize;
		dst_step = vsetup->step / 8;
		dst = chan->mmap_data + vsetup->addr + (vsetup->first + vsetup->step * hwoff) / 8;
		while (samples-- > 0) {
			goto *copy;
#define COPY_END after
#include "plugin/plugin_ops.h"
#undef COPY_END
		after:
			src += ssize;
			dst += dst_step;
		}
	}
	return 0;
}

ssize_t snd_pcm_mmap_write(snd_pcm_t *pcm, const void *buffer, size_t count)
{
	struct snd_pcm_chan *chan;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	if (!chan->open || !chan->valid_setup || !chan->valid_voices_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (count > 0 && !buffer)
		return -EFAULT;
	if (!chan->setup.format.interleave && chan->setup.format.voices > 1)
		return -EINVAL;
	return snd_pcm_mmap_write1(pcm, buffer, count, transfer_write);
}

static int transfer_writev(snd_pcm_t *pcm, size_t hwoff, void* data, size_t off, size_t size)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	struct iovec *vec = data;
	unsigned int v, voices;
#define COPY_LABELS
#include "plugin/plugin_ops.h"
#undef COPY_LABELS
	void *copy;
	snd_pcm_voice_setup_t *vsetup;
	int idx;
	size_t ssize;
	idx = copy_index(chan->setup.format.format);
	if (idx < 0)
		return idx;
	copy = copy_labels[idx];
	voices = chan->setup.format.voices;
	vsetup = chan->voices_setup;
	ssize = snd_pcm_format_size(chan->setup.format.format, chan->setup.format.voices);
	hwoff /= ssize;
	size /= ssize;
	off /= voices;
	for (v = 0; v < voices; ++v, ++vsetup, ++vec) {
		const char *src;
		char *dst;
		size_t dst_step;
		size_t samples = size;
		if (vsetup->first % 8 != 0 ||
		    vsetup->step % 8 != 0)
			return -EINVAL;
		src = vec->iov_base + off;
		dst_step = vsetup->step / 8;
		dst = chan->mmap_data + vsetup->addr + (vsetup->first + vsetup->step * hwoff) / 8;
		while (samples-- > 0) {
			goto *copy;
#define COPY_END after
#include "plugin/plugin_ops.h"
#undef COPY_END
		after:
			src += ssize;
			dst += dst_step;
		}
	}
	return 0;
}

ssize_t snd_pcm_mmap_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long vcount)
{
	struct snd_pcm_chan *chan;
	size_t result = 0;
	unsigned int b;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
	if (!chan->open || !chan->valid_setup || !chan->valid_voices_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (vcount > 0 && !vector)
		return -EFAULT;
	if (chan->setup.format.interleave) {
		for (b = 0; b < vcount; b++) {
			int ret;
			ret = snd_pcm_mmap_write1(pcm, vector[b].iov_base, vector[b].iov_len, transfer_write);
			if (ret < 0)
				return result > 0 ? result : ret;
			result += ret;
		}
	} else {
		unsigned int voices = chan->setup.format.voices;
		unsigned long bcount;
		if (vcount % voices)
			return -EINVAL;
		bcount = vcount / voices;
		for (b = 0; b < bcount; b++) {
			unsigned int v;
			int ret;
			size_t count = 0;
			count = vector[0].iov_len;
			for (v = 0; v < voices; ++v) {
				if (vector[v].iov_len != count)
					return -EINVAL;
			}
			ret = snd_pcm_mmap_write1(pcm, vector, count * voices, transfer_writev);
			if (ret < 0)
				return result > 0 ? result : ret;
			result += ret;
			if ((size_t)ret != count * voices)
				break;
			vector += voices;
		}
	}
	return result;
}

static ssize_t snd_pcm_mmap_read1(snd_pcm_t *pcm, void *data, size_t count, transfer_f transfer)
{
	struct snd_pcm_chan *chan;
	snd_pcm_mmap_control_t *ctrl;
	size_t frag_size;
	size_t offset = 0;
	size_t result = 0;
	int err;

	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	ctrl = chan->mmap_control;
	if (ctrl->status < SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	frag_size = chan->setup.frag_size;
	if (chan->setup.mode == SND_PCM_MODE_BLOCK) {
		if (count % frag_size != 0)
			return -EINVAL;
	} else {
		int tmp = snd_pcm_format_size(chan->setup.format.format, chan->setup.format.voices);
		if (tmp > 0) {
	                int r = count % tmp;
			if (r > 0) {
				count -= r;
				if (count == 0)
					return -EINVAL;
			}
                }
	}
	if (ctrl->status == SND_PCM_STATUS_PREPARED &&
	    chan->setup.start_mode == SND_PCM_START_DATA) {
		err = snd_pcm_channel_go(pcm, SND_PCM_CHANNEL_CAPTURE);
		if (err < 0)
			return err;
	}
	while (count > 0) {
		size_t bytes;
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
		if (chan->setup.mode == SND_PCM_MODE_BLOCK) {
			size_t frag_data, frag;
			frag_data = ctrl->frag_data;
			frag = frag_data % chan->setup.frags;
			err = transfer(pcm, frag_size * frag, data, offset, frag_size);
			if (err < 0) 
				return result > 0 ? result : err;
			if (ctrl->status == SND_PCM_STATUS_XRUN &&
			    chan->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
				return result > 0 ? result : -EPIPE;
			frag_data++;
			if (frag_data == chan->setup.frag_boundary) {
				ctrl->frag_data = 0;
				ctrl->pos_data = 0;
			} else {
				ctrl->frag_data = frag_data;
				ctrl->pos_data += frag_size;
			}
			bytes = frag_size;
		} else {
			size_t pos_data;
			bytes = snd_pcm_mmap_bytes_capture(pcm, count);
			pos_data = ctrl->pos_data;
			err = transfer(pcm, pos_data % chan->setup.buffer_size, data, offset, bytes);
			if (err < 0) 
				return result > 0 ? result : err;
			if (ctrl->status == SND_PCM_STATUS_XRUN &&
			    chan->setup.xrun_mode == SND_PCM_XRUN_DRAIN)
				return result > 0 ? result : -EPIPE;
			pos_data += bytes;
			if (pos_data == chan->setup.pos_boundary) {
				ctrl->pos_data = 0;
				ctrl->frag_data = 0;
			} else {
				ctrl->pos_data = pos_data;
				ctrl->frag_data = pos_data / chan->setup.frags;
			}
		}
		offset += bytes;
		count -= bytes;
		result += bytes;
	}
	
	return result;
}

static int transfer_read(snd_pcm_t *pcm, size_t hwoff, void* data, size_t off, size_t size)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	char *buf = data;
	unsigned int v, voices;
#define COPY_LABELS
#include "plugin/plugin_ops.h"
#undef COPY_LABELS
	void *copy;
	snd_pcm_voice_setup_t *vsetup;
	int idx;
	size_t vsize, ssize;
	idx = copy_index(chan->setup.format.format);
	if (idx < 0)
		return idx;
	copy = copy_labels[idx];
	voices = chan->setup.format.voices;
	vsetup = chan->voices_setup;
	vsize = snd_pcm_format_size(chan->setup.format.format, 1);
	ssize = vsize * chan->setup.format.voices;
	hwoff /= ssize;
	size /= ssize;
	for (v = 0; v < voices; ++v, ++vsetup) {
		const char *src;
		size_t src_step;
		char *dst;
		size_t samples = size;
		if (vsetup->first % 8 != 0 ||
		    vsetup->step % 8 != 0)
			return -EINVAL;
		src_step = vsetup->step / 8;
		src = chan->mmap_data + vsetup->addr + (vsetup->first + vsetup->step * hwoff) / 8;
		dst = buf + off + v * vsize;
		while (samples-- > 0) {
			goto *copy;
#define COPY_END after
#include "plugin/plugin_ops.h"
#undef COPY_END
		after:
			src += src_step;
			dst += ssize;
		}
	}
	return 0;
}

ssize_t snd_pcm_mmap_read(snd_pcm_t *pcm, void *buffer, size_t count)
{
	struct snd_pcm_chan *chan;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	if (!chan->open || !chan->valid_setup || !chan->valid_voices_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (count > 0 && !buffer)
		return -EFAULT;
	if (!chan->setup.format.interleave && chan->setup.format.voices > 1)
		return -EINVAL;
	return snd_pcm_mmap_read1(pcm, buffer, count, transfer_read);
}

static int transfer_readv(snd_pcm_t *pcm, size_t hwoff, void* data, size_t off, size_t size)
{
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	struct iovec *vec = data;
	unsigned int v, voices;
#define COPY_LABELS
#include "plugin/plugin_ops.h"
#undef COPY_LABELS
	void *copy;
	snd_pcm_voice_setup_t *vsetup;
	int idx;
	size_t ssize;
	idx = copy_index(chan->setup.format.format);
	if (idx < 0)
		return idx;
	copy = copy_labels[idx];
	voices = chan->setup.format.voices;
	vsetup = chan->voices_setup;
	ssize = snd_pcm_format_size(chan->setup.format.format, chan->setup.format.voices);
	hwoff /= ssize;
	size /= ssize;
	off /= voices;
	for (v = 0; v < voices; ++v, ++vsetup, ++vec) {
		const char *src;
		size_t src_step;
		char *dst;
		size_t samples = size;
		if (vsetup->first % 8 != 0 ||
		    vsetup->step % 8 != 0)
			return -EINVAL;
		src_step = vsetup->step / 8;
		src = chan->mmap_data + vsetup->addr + (vsetup->first + vsetup->step * hwoff) / 8;
		dst = vec->iov_base + off;
		while (samples-- > 0) {
			goto *copy;
#define COPY_END after
#include "plugin/plugin_ops.h"
#undef COPY_END
		after:
			src += src_step;
			dst += ssize;
		}
	}
	return 0;
}

ssize_t snd_pcm_mmap_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long vcount)
{
	struct snd_pcm_chan *chan;
	size_t result = 0;
	unsigned int b;
	if (!pcm)
		return -EFAULT;
	chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
	if (!chan->open || !chan->valid_setup || !chan->valid_voices_setup)
		return -EBADFD;
	if (!chan->mmap_data || !chan->mmap_control)
		return -EBADFD;
	if (vcount > 0 && !vector)
		return -EFAULT;
	if (chan->setup.format.interleave) {
		for (b = 0; b < vcount; b++) {
			int ret;
			ret = snd_pcm_mmap_read1(pcm, vector[b].iov_base, vector[b].iov_len, transfer_write);
			if (ret < 0)
				return result > 0 ? result : ret;
			result += ret;
		}
	} else {
		unsigned int voices = chan->setup.format.voices;
		unsigned long bcount;
		if (vcount % voices)
			return -EINVAL;
		bcount = vcount / voices;
		for (b = 0; b < bcount; b++) {
			unsigned int v;
			int ret;
			size_t count = 0;
			count = vector[0].iov_len;
			for (v = 0; v < voices; ++v) {
				if (vector[v].iov_len != count)
					return -EINVAL;
			}
			ret = snd_pcm_mmap_read1(pcm, (void *) vector, count * voices, transfer_readv);
			if (ret < 0)
				return result > 0 ? result : ret;
			result += ret;
			if ((size_t)ret != count * voices)
				break;
			vector += voices;
		}
	}
	return result;
}

static void *playback_mmap(void *d)
{
	snd_pcm_t *pcm = d;
	struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
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
		unsigned int f, frag;
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

		frag = control->frag_io;
		if (snd_pcm_mmap_playback_frags_used(pcm) <= 0) {
			fprintf(stderr, "underrun\n");
			usleep(10000);
			continue;
		}
		f = frag % frags;
		if (chan->setup.format.interleave) {
			err = snd_pcm_write(pcm, data + f * frag_size, frag_size);
		} else {
			struct iovec vector[voices];
			struct iovec *v = vector;
			int voice;
			for (voice = 0; voice < voices; ++voice) {
				v->iov_base = data + voice_size * voice + f * voice_frag_size;
				v->iov_len = voice_frag_size;
				v++;
			}
			err = snd_pcm_writev(pcm, vector, voice_frag_size);
		}
		if (err <= 0) {
			fprintf(stderr, "write err=%d\n", err);
			snd_pcm_mmap_status_change(pcm, SND_PCM_CHANNEL_PLAYBACK, -1);
			continue;
		}
		pthread_mutex_lock(&chan->mutex);
		pthread_cond_signal(&chan->ready_cond);
		pthread_mutex_unlock(&chan->mutex);
		frag++;
		if (frag == chan->setup.frag_boundary)
			frag = 0;
		control->frag_io = frag;
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
		unsigned int f, frag;
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

		frag = control->frag_io;
		if (snd_pcm_mmap_capture_frags_free(pcm) <= 0) {
			fprintf(stderr, "overrun\n");
			usleep(10000);
			continue;
		}
		f = frag % frags;
		if (chan->setup.format.interleave) {
			err = snd_pcm_read(pcm, data + f * frag_size, frag_size);
		} else {
			struct iovec vector[voices];
			struct iovec *v = vector;
			int voice;
			for (voice = 0; voice < voices; ++voice) {
				v->iov_base = data + voice_size * voice + f * voice_frag_size;
				v->iov_len = voice_frag_size;
				v++;
			}
			err = snd_pcm_readv(pcm, vector, voice_frag_size);
		}
		if (err < 0) {
			fprintf(stderr, "read err=%d\n", err);
			snd_pcm_mmap_status_change(pcm, SND_PCM_CHANNEL_CAPTURE, -1);
			continue;
		}
		frag++;
		if (frag == chan->setup.frag_boundary)
			frag = 0;
		control->frag_io = frag;
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
	snd_pcm_all_voices_setup(pcm, channel, NULL);
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

