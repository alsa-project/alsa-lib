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
#include <string.h>
#include <errno.h>
#include <sys/poll.h>
#include "pcm_local.h"

size_t snd_pcm_mmap_avail(snd_pcm_t *pcm)
{
        assert(pcm);
	assert(pcm->mmap_status && pcm->mmap_control);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_mmap_playback_avail(pcm);
	else
		return snd_pcm_mmap_capture_avail(pcm);
	return 0;
}

static int snd_pcm_mmap_playback_ready(snd_pcm_t *pcm)
{
	if (pcm->mmap_status->state == SND_PCM_STATE_XRUN)
		return -EPIPE;
	return snd_pcm_mmap_playback_avail(pcm) >= pcm->setup.avail_min;
}

static int snd_pcm_mmap_capture_ready(snd_pcm_t *pcm)
{
	int ret = 0;
	if (pcm->mmap_status->state == SND_PCM_STATE_XRUN) {
		ret = -EPIPE;
		if (pcm->setup.xrun_act == SND_PCM_XRUN_ACT_DROP)
			return -EPIPE;
	}
	if (snd_pcm_mmap_capture_avail(pcm) >= pcm->setup.avail_min)
		return 1;
	return ret;
}

int snd_pcm_mmap_ready(snd_pcm_t *pcm)
{
        assert(pcm);
	assert(pcm->mmap_status && pcm->mmap_control);
	assert(pcm->mmap_status->state >= SND_PCM_STATE_PREPARED);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		return snd_pcm_mmap_playback_ready(pcm);
	} else {
		return snd_pcm_mmap_capture_ready(pcm);
	}
}

size_t snd_pcm_mmap_playback_xfer(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_mmap_control_t *control = pcm->mmap_control;
	size_t cont;
	size_t avail = snd_pcm_mmap_playback_avail(pcm);
	if (avail < frames)
		frames = avail;
	cont = pcm->setup.buffer_size - control->appl_ptr % pcm->setup.buffer_size;
	if (cont < frames)
		frames = cont;
	return frames;
}

size_t snd_pcm_mmap_capture_xfer(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_mmap_control_t *control = pcm->mmap_control;
	size_t cont;
	size_t avail = snd_pcm_mmap_capture_avail(pcm);
	if (avail < frames)
		frames = avail;
	cont = pcm->setup.buffer_size - control->appl_ptr % pcm->setup.buffer_size;
	if (cont < frames)
		frames = cont;
	return frames;
}

size_t snd_pcm_mmap_xfer(snd_pcm_t *pcm, size_t frames)
{
        assert(pcm);
	assert(pcm->mmap_status && pcm->mmap_control);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_mmap_playback_xfer(pcm, frames);
	else
		return snd_pcm_mmap_capture_xfer(pcm, frames);
}

size_t snd_pcm_mmap_offset(snd_pcm_t *pcm)
{
        assert(pcm);
	assert(pcm->mmap_control);
	return pcm->mmap_control->appl_ptr % pcm->setup.buffer_size;
}

size_t snd_pcm_mmap_hw_offset(snd_pcm_t *pcm)
{
        assert(pcm);
	assert(pcm->mmap_status);
	return pcm->mmap_status->hw_ptr % pcm->setup.buffer_size;
}

int snd_pcm_mmap_state(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->mmap_status);
	return pcm->mmap_status->state;
}

ssize_t snd_pcm_mmap_hw_ptr(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->mmap_status);
	return pcm->mmap_status->hw_ptr;
}

ssize_t snd_pcm_mmap_appl_ptr(snd_pcm_t *pcm, off_t offset)
{
	ssize_t appl_ptr;
	assert(pcm);
	assert(pcm->mmap_status && pcm->mmap_control);
	assert(offset == 0 || pcm->type == SND_PCM_TYPE_HW);
	appl_ptr = pcm->mmap_control->appl_ptr;
	if (offset == 0)
		return appl_ptr;
	switch (pcm->mmap_status->state) {
	case SND_PCM_STATE_RUNNING:
		if (pcm->setup.xrun_mode == SND_PCM_XRUN_ASAP)
			snd_pcm_avail_update(pcm);
		break;
	case SND_PCM_STATE_READY:
	case SND_PCM_STATE_NOTREADY:
		return -EBADFD;
	}
	if (offset < 0) {
		if (offset < -(ssize_t)pcm->setup.buffer_size)
			offset = -(ssize_t)pcm->setup.buffer_size;
		appl_ptr += offset;
		if (appl_ptr < 0)
			appl_ptr += pcm->setup.boundary;
	} else {
		size_t avail;
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
			avail = snd_pcm_mmap_playback_avail(pcm);
		else
			avail = snd_pcm_mmap_capture_avail(pcm);
		if ((size_t)offset > avail)
			offset = avail;
		appl_ptr += offset;
		if ((size_t)appl_ptr >= pcm->setup.boundary)
			appl_ptr -= pcm->setup.boundary;
	}
	pcm->mmap_control->appl_ptr = appl_ptr;
	return appl_ptr;
}

void snd_pcm_mmap_appl_backward(snd_pcm_t *pcm, size_t frames)
{
	ssize_t appl_ptr = pcm->mmap_control->appl_ptr;
	appl_ptr -= frames;
	if (appl_ptr < 0)
		appl_ptr += pcm->setup.boundary;
	pcm->mmap_control->appl_ptr = appl_ptr;
}

void snd_pcm_mmap_appl_forward(snd_pcm_t *pcm, size_t frames)
{
	size_t appl_ptr = pcm->mmap_control->appl_ptr;
	appl_ptr += frames;
	if (appl_ptr >= pcm->setup.boundary)
		appl_ptr -= pcm->setup.boundary;
	pcm->mmap_control->appl_ptr = appl_ptr;
}

void snd_pcm_mmap_hw_forward(snd_pcm_t *pcm, size_t frames)
{
	size_t hw_ptr = pcm->mmap_status->hw_ptr;
	hw_ptr += frames;
	if (hw_ptr >= pcm->setup.boundary)
		hw_ptr -= pcm->setup.boundary;
	pcm->mmap_status->hw_ptr = hw_ptr;
}

ssize_t snd_pcm_mmap_write_areas(snd_pcm_t *pcm,
				 snd_pcm_channel_area_t *areas,
				 size_t offset,
				 size_t size,
				 size_t *slave_sizep)
{
	size_t xfer;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	xfer = 0;
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_playback_xfer(pcm, size - xfer);
		snd_pcm_areas_copy(areas, offset, 
				   pcm->mmap_areas, snd_pcm_mmap_offset(pcm),
				   pcm->setup.format.channels, 
				   frames, pcm->setup.format.sfmt);
		err = snd_pcm_mmap_forward(pcm, frames);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		offset += err;
		xfer += err;
	}
	if (xfer > 0) {
		if (slave_sizep)
			*slave_sizep = xfer;
		return xfer;
	}
	return err;
}

ssize_t snd_pcm_mmap_read_areas(snd_pcm_t *pcm,
				snd_pcm_channel_area_t *areas,
				size_t offset,
				size_t size,
				size_t *slave_sizep)
{
	size_t xfer;
	ssize_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	xfer = 0;
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_capture_xfer(pcm, size - xfer);
		snd_pcm_areas_copy(pcm->mmap_areas, snd_pcm_mmap_offset(pcm),
				   areas, offset, 
				   pcm->setup.format.channels, 
				   frames, pcm->setup.format.sfmt);
		err = snd_pcm_mmap_forward(pcm, frames);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		offset += err;
		xfer += err;
	}
	if (xfer > 0) {
		if (slave_sizep)
			*slave_sizep = xfer;
		return xfer;
	}
	return err;
}

ssize_t snd_pcm_mmap_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	snd_pcm_areas_from_buf(pcm, areas, (void*)buffer);
	return snd_pcm_write_areas(pcm, areas, 0, size,
				   snd_pcm_mmap_write_areas);
}

ssize_t snd_pcm_mmap_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	snd_pcm_areas_from_bufs(pcm, areas, bufs);
	return snd_pcm_write_areas(pcm, areas, 0, size,
				   snd_pcm_mmap_write_areas);
}

ssize_t snd_pcm_mmap_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	snd_pcm_areas_from_buf(pcm, areas, buffer);
	return snd_pcm_read_areas(pcm, areas, 0, size,
				  snd_pcm_mmap_read_areas);
}

ssize_t snd_pcm_mmap_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	snd_pcm_areas_from_bufs(pcm, areas, bufs);
	return snd_pcm_read_areas(pcm, areas, 0, size,
				  snd_pcm_mmap_read_areas);
}

int snd_pcm_mmap_status(snd_pcm_t *pcm, snd_pcm_mmap_status_t **status)
{
	int err;
	assert(pcm);
	if (pcm->mmap_status) {
		if (status)
			*status = pcm->mmap_status;
		return 0;
	}

	if ((err = pcm->ops->mmap_status(pcm->op_arg)) < 0)
		return err;
	if (status)
		*status = pcm->mmap_status;
	return 0;
}

int snd_pcm_mmap_control(snd_pcm_t *pcm, snd_pcm_mmap_control_t **control)
{
	int err;
	assert(pcm);
	if (pcm->mmap_control) {
		if (control)
			*control = pcm->mmap_control;
		return 0;
	}

	if ((err = pcm->ops->mmap_control(pcm->op_arg)) < 0)
		return err;
	if (control)
		*control = pcm->mmap_control;
	return 0;
}

int snd_pcm_mmap_get_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas)
{
	snd_pcm_channel_setup_t s;
	snd_pcm_channel_area_t *a, *ap;
	unsigned int channel;
	int err;
	assert(pcm);
	assert(pcm->mmap_data);
	a = calloc(pcm->setup.format.channels, sizeof(*areas));
	for (channel = 0, ap = a; channel < pcm->setup.format.channels; ++channel, ++ap) {
		s.channel = channel;
		err = snd_pcm_channel_setup(pcm, &s);
		if (err < 0) {
			free(a);
			return err;
		}
		if (areas)
			areas[channel] = s.area;
		*ap = s.area;
	}
	pcm->mmap_areas = a;
	return 0;
}

int snd_pcm_mmap_data(snd_pcm_t *pcm, void **data)
{
	int err;
	assert(pcm);
	assert(pcm->valid_setup);
	if (pcm->mmap_data) {
		if (data)
			*data = pcm->mmap_data;
		return 0;
	}

	if ((err = pcm->ops->mmap_data(pcm->op_arg)) < 0)
		return err;
	if (data) 
		*data = pcm->mmap_data;
	err = snd_pcm_mmap_get_areas(pcm, NULL);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_munmap_status(snd_pcm_t *pcm)
{
	int err;
	assert(pcm);
	assert(pcm->mmap_status);
	if ((err = pcm->ops->munmap_status(pcm->op_arg)) < 0)
		return err;
	pcm->mmap_status = 0;
	return 0;
}

int snd_pcm_munmap_control(snd_pcm_t *pcm)
{
	int err;
	assert(pcm);
	assert(pcm->mmap_control);
	if ((err = pcm->ops->munmap_control(pcm->op_arg)) < 0)
		return err;
	pcm->mmap_control = 0;
	return 0;
}

int snd_pcm_munmap_data(snd_pcm_t *pcm)
{
	int err;
	assert(pcm);
	assert(pcm->mmap_data);
	if ((err = pcm->ops->munmap_data(pcm->op_arg)) < 0)
		return err;
	free(pcm->mmap_areas);
	pcm->mmap_areas = 0;
	pcm->mmap_data = 0;
	return 0;
}

int snd_pcm_mmap(snd_pcm_t *pcm, void **data)
{
	return snd_pcm_mmap_data(pcm, data);
}

int snd_pcm_munmap(snd_pcm_t *pcm)
{
	return snd_pcm_munmap_data(pcm);
}


ssize_t snd_pcm_write_mmap(snd_pcm_t *pcm, size_t size)
{
	size_t xfer = 0;
	ssize_t err = 0;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = size - xfer;
		size_t offset = snd_pcm_mmap_hw_offset(pcm);
		size_t cont = pcm->setup.buffer_size - offset;
		if (cont < frames)
			frames = cont;
		if (pcm->setup.xfer_mode == SND_PCM_XFER_INTERLEAVED) {
			snd_pcm_channel_area_t *a = pcm->mmap_areas;
			char *buf = snd_pcm_channel_area_addr(a, offset);
			assert(pcm->setup.mmap_shape == SND_PCM_MMAP_INTERLEAVED);
			err = _snd_pcm_writei(pcm, buf, size);
		} else {
			size_t channels = pcm->setup.format.channels;
			unsigned int c;
			void *bufs[channels];
			assert(pcm->setup.mmap_shape == SND_PCM_MMAP_NONINTERLEAVED);
			for (c = 0; c < channels; ++c) {
				snd_pcm_channel_area_t *a = &pcm->mmap_areas[c];
				bufs[c] = snd_pcm_channel_area_addr(a, offset);
			}
			err = _snd_pcm_writen(pcm, bufs, size);
		}
		if (err < 0)
			break;
		xfer += frames;
	}
	if (xfer > 0)
		return xfer;
	return err;
}

ssize_t snd_pcm_read_mmap(snd_pcm_t *pcm, size_t size)
{
	size_t xfer = 0;
	ssize_t err = 0;
	assert(size > 0);
	while (xfer < size) {
		size_t frames = size - xfer;
		size_t offset = snd_pcm_mmap_hw_offset(pcm);
		size_t cont = pcm->setup.buffer_size - offset;
		if (cont < frames)
			frames = cont;
		if (pcm->setup.xfer_mode == SND_PCM_XFER_INTERLEAVED) {
			snd_pcm_channel_area_t *a = pcm->mmap_areas;
			char *buf = snd_pcm_channel_area_addr(a, offset);
			assert(pcm->setup.mmap_shape == SND_PCM_MMAP_INTERLEAVED);
			err = _snd_pcm_readi(pcm, buf, size);
		} else {
			size_t channels = pcm->setup.format.channels;
			unsigned int c;
			void *bufs[channels];
			assert(pcm->setup.mmap_shape == SND_PCM_MMAP_NONINTERLEAVED);
			for (c = 0; c < channels; ++c) {
				snd_pcm_channel_area_t *a = &pcm->mmap_areas[c];
				bufs[c] = snd_pcm_channel_area_addr(a, offset);
			}
			err = _snd_pcm_readn(pcm->fast_op_arg, bufs, size);
		}
		if (err < 0)
			break;
		xfer += frames;
	}
	if (xfer > 0)
		return xfer;
	return err;
}
