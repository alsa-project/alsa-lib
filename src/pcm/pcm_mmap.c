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

snd_pcm_channel_area_t *snd_pcm_mmap_areas(snd_pcm_t *pcm)
{
  int state = snd_pcm_state(pcm);
  if (state == SND_PCM_STATE_RUNNING)
	  return pcm->running_areas;
  else
	  return pcm->stopped_areas;
}

size_t snd_pcm_mmap_playback_xfer(snd_pcm_t *pcm, size_t frames)
{
	size_t cont;
	size_t avail = snd_pcm_mmap_playback_avail(pcm);
	if (avail < frames)
		frames = avail;
	cont = pcm->setup.buffer_size - *pcm->appl_ptr % pcm->setup.buffer_size;
	if (cont < frames)
		frames = cont;
	return frames;
}

size_t snd_pcm_mmap_capture_xfer(snd_pcm_t *pcm, size_t frames)
{
	size_t cont;
	size_t avail = snd_pcm_mmap_capture_avail(pcm);
	if (avail < frames)
		frames = avail;
	cont = pcm->setup.buffer_size - *pcm->appl_ptr % pcm->setup.buffer_size;
	if (cont < frames)
		frames = cont;
	return frames;
}

size_t snd_pcm_mmap_xfer(snd_pcm_t *pcm, size_t frames)
{
        assert(pcm);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		return snd_pcm_mmap_playback_xfer(pcm, frames);
	else
		return snd_pcm_mmap_capture_xfer(pcm, frames);
}

size_t snd_pcm_mmap_offset(snd_pcm_t *pcm)
{
        assert(pcm);
	return *pcm->appl_ptr % pcm->setup.buffer_size;
}

size_t snd_pcm_mmap_hw_offset(snd_pcm_t *pcm)
{
        assert(pcm);
	return *pcm->hw_ptr % pcm->setup.buffer_size;
}

void snd_pcm_mmap_appl_backward(snd_pcm_t *pcm, size_t frames)
{
	ssize_t appl_ptr = *pcm->appl_ptr;
	appl_ptr -= frames;
	if (appl_ptr < 0)
		appl_ptr += pcm->setup.boundary;
	*pcm->appl_ptr = appl_ptr;
}

void snd_pcm_mmap_appl_forward(snd_pcm_t *pcm, size_t frames)
{
	size_t appl_ptr = *pcm->appl_ptr;
	appl_ptr += frames;
	if (appl_ptr >= pcm->setup.boundary)
		appl_ptr -= pcm->setup.boundary;
	*pcm->appl_ptr = appl_ptr;
}

void snd_pcm_mmap_hw_backward(snd_pcm_t *pcm, size_t frames)
{
	ssize_t hw_ptr = *pcm->hw_ptr;
	hw_ptr -= frames;
	if (hw_ptr < 0)
		hw_ptr += pcm->setup.boundary;
	*pcm->hw_ptr = hw_ptr;
}

void snd_pcm_mmap_hw_forward(snd_pcm_t *pcm, size_t frames)
{
	size_t hw_ptr = *pcm->hw_ptr;
	hw_ptr += frames;
	if (hw_ptr >= pcm->setup.boundary)
		hw_ptr -= pcm->setup.boundary;
	*pcm->hw_ptr = hw_ptr;
}

ssize_t snd_pcm_mmap_write_areas(snd_pcm_t *pcm,
				 snd_pcm_channel_area_t *areas,
				 size_t offset,
				 size_t size,
				 size_t *slave_sizep)
{
	size_t xfer;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	xfer = 0;
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_playback_xfer(pcm, size - xfer);
		snd_pcm_areas_copy(areas, offset, 
				   snd_pcm_mmap_areas(pcm), snd_pcm_mmap_offset(pcm),
				   pcm->setup.format.channels, 
				   frames, pcm->setup.format.sfmt);
		snd_pcm_mmap_forward(pcm, frames);
		offset += frames;
		xfer += frames;
	}
	if (slave_sizep)
		*slave_sizep = xfer;
	return xfer;
}

ssize_t snd_pcm_mmap_read_areas(snd_pcm_t *pcm,
				snd_pcm_channel_area_t *areas,
				size_t offset,
				size_t size,
				size_t *slave_sizep)
{
	size_t xfer;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	xfer = 0;
	while (xfer < size) {
		size_t frames = snd_pcm_mmap_capture_xfer(pcm, size - xfer);
		snd_pcm_areas_copy(snd_pcm_mmap_areas(pcm), snd_pcm_mmap_offset(pcm),
				   areas, offset, 
				   pcm->setup.format.channels, 
				   frames, pcm->setup.format.sfmt);
		snd_pcm_mmap_forward(pcm, frames);
		offset += frames;
		xfer += frames;
	}
	if (slave_sizep)
		*slave_sizep = xfer;
	return xfer;
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

int snd_pcm_mmap_get_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *stopped_areas, snd_pcm_channel_area_t *running_areas)
{
	snd_pcm_channel_setup_t setup;
	snd_pcm_channel_area_t *r, *rp, *s, *sp;
	unsigned int channel;
	int err;
	assert(pcm);
	assert(pcm->mmap_info);
	if (!pcm->running_areas) {
		r = calloc(pcm->setup.format.channels, sizeof(*r));
		s = calloc(pcm->setup.format.channels, sizeof(*s));
		for (channel = 0, rp = r, sp = s; channel < pcm->setup.format.channels; ++channel, ++rp, ++sp) {
			setup.channel = channel;
			err = snd_pcm_channel_setup(pcm, &setup);
			if (err < 0) {
				free(r);
				free(s);
				return err;
			}
			*rp = setup.running_area;
			*sp = setup.stopped_area;
		}
		pcm->running_areas = r;
		pcm->stopped_areas = s;
	}
	if (running_areas)
		memcpy(running_areas, pcm->running_areas, pcm->setup.format.channels * sizeof(*running_areas));
	if (stopped_areas)
		memcpy(stopped_areas, pcm->stopped_areas, pcm->setup.format.channels * sizeof(*stopped_areas));
	return 0;
}

int snd_pcm_mmap(snd_pcm_t *pcm)
{
	int err;
	assert(pcm);
	assert(pcm->valid_setup);
	if (pcm->mmap_info)
		return 0;

	if ((err = pcm->ops->mmap(pcm->op_arg)) < 0)
		return err;
	err = snd_pcm_mmap_get_areas(pcm, NULL, NULL);
	if (err < 0)
		return err;
	return 0;
}

int snd_pcm_munmap(snd_pcm_t *pcm)
{
	int err;
	assert(pcm);
	assert(pcm->mmap_info);
	if ((err = pcm->ops->munmap(pcm->op_arg)) < 0)
		return err;
	free(pcm->stopped_areas);
	free(pcm->running_areas);
	pcm->stopped_areas = 0;
	pcm->running_areas = 0;
	return 0;
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
			snd_pcm_channel_area_t *a = snd_pcm_mmap_areas(pcm);
			char *buf = snd_pcm_channel_area_addr(a, offset);
			assert(pcm->setup.mmap_shape == SND_PCM_MMAP_INTERLEAVED);
			err = _snd_pcm_writei(pcm, buf, size);
		} else {
			size_t channels = pcm->setup.format.channels;
			unsigned int c;
			void *bufs[channels];
			snd_pcm_channel_area_t *areas = snd_pcm_mmap_areas(pcm);
			assert(pcm->setup.mmap_shape == SND_PCM_MMAP_NONINTERLEAVED);
			for (c = 0; c < channels; ++c) {
				snd_pcm_channel_area_t *a = &areas[c];
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
			snd_pcm_channel_area_t *a = snd_pcm_mmap_areas(pcm);
			char *buf = snd_pcm_channel_area_addr(a, offset);
			assert(pcm->setup.mmap_shape == SND_PCM_MMAP_INTERLEAVED);
			err = _snd_pcm_readi(pcm, buf, size);
		} else {
			size_t channels = pcm->setup.format.channels;
			unsigned int c;
			void *bufs[channels];
			snd_pcm_channel_area_t *areas = snd_pcm_mmap_areas(pcm);
			assert(pcm->setup.mmap_shape == SND_PCM_MMAP_NONINTERLEAVED);
			for (c = 0; c < channels; ++c) {
				snd_pcm_channel_area_t *a = &areas[c];
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
