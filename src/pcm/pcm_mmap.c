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
#include <sys/mman.h>
#include <sys/shm.h>
#include <asm/page.h>
#include "pcm_local.h"


#ifndef PAGE_ALIGN
#define PAGE_ALIGN(addr)        (((addr)+PAGE_SIZE-1)&PAGE_MASK)
#endif


snd_pcm_channel_area_t *snd_pcm_mmap_areas(snd_pcm_t *pcm)
{
	if (pcm->stopped_areas &&
	    snd_pcm_state(pcm) != SND_PCM_STATE_RUNNING) 
		return pcm->stopped_areas;
	return pcm->running_areas;
}

size_t snd_pcm_mmap_playback_xfer(snd_pcm_t *pcm, size_t frames)
{
	size_t cont;
	size_t avail = snd_pcm_mmap_playback_avail(pcm);
	if (avail < frames)
		frames = avail;
	cont = pcm->buffer_size - *pcm->appl_ptr % pcm->buffer_size;
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
	cont = pcm->buffer_size - *pcm->appl_ptr % pcm->buffer_size;
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
	return *pcm->appl_ptr % pcm->buffer_size;
}

size_t snd_pcm_mmap_hw_offset(snd_pcm_t *pcm)
{
        assert(pcm);
	return *pcm->hw_ptr % pcm->buffer_size;
}

void snd_pcm_mmap_appl_backward(snd_pcm_t *pcm, size_t frames)
{
	ssize_t appl_ptr = *pcm->appl_ptr;
	appl_ptr -= frames;
	if (appl_ptr < 0)
		appl_ptr += pcm->boundary;
	*pcm->appl_ptr = appl_ptr;
}

void snd_pcm_mmap_appl_forward(snd_pcm_t *pcm, size_t frames)
{
	size_t appl_ptr = *pcm->appl_ptr;
	appl_ptr += frames;
	if (appl_ptr >= pcm->boundary)
		appl_ptr -= pcm->boundary;
	*pcm->appl_ptr = appl_ptr;
}

void snd_pcm_mmap_hw_backward(snd_pcm_t *pcm, size_t frames)
{
	ssize_t hw_ptr = *pcm->hw_ptr;
	hw_ptr -= frames;
	if (hw_ptr < 0)
		hw_ptr += pcm->boundary;
	*pcm->hw_ptr = hw_ptr;
}

void snd_pcm_mmap_hw_forward(snd_pcm_t *pcm, size_t frames)
{
	size_t hw_ptr = *pcm->hw_ptr;
	hw_ptr += frames;
	if (hw_ptr >= pcm->boundary)
		hw_ptr -= pcm->boundary;
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
		ssize_t err;
		snd_pcm_areas_copy(areas, offset, 
				   snd_pcm_mmap_areas(pcm), snd_pcm_mmap_offset(pcm),
				   pcm->channels, 
				   frames, pcm->format);
		err = snd_pcm_mmap_forward(pcm, frames);
		assert(err == (ssize_t)frames);
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
		ssize_t err;
		snd_pcm_areas_copy(snd_pcm_mmap_areas(pcm), snd_pcm_mmap_offset(pcm),
				   areas, offset, 
				   pcm->channels, 
				   frames, pcm->format);
		err = snd_pcm_mmap_forward(pcm, frames);
		assert(err == (ssize_t)frames);
		offset += frames;
		xfer += frames;
	}
	if (slave_sizep)
		*slave_sizep = xfer;
	return xfer;
}

ssize_t snd_pcm_mmap_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_areas_from_buf(pcm, areas, (void*)buffer);
	return snd_pcm_write_areas(pcm, areas, 0, size,
				   snd_pcm_mmap_write_areas);
}

ssize_t snd_pcm_mmap_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_areas_from_bufs(pcm, areas, bufs);
	return snd_pcm_write_areas(pcm, areas, 0, size,
				   snd_pcm_mmap_write_areas);
}

ssize_t snd_pcm_mmap_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_areas_from_buf(pcm, areas, buffer);
	return snd_pcm_read_areas(pcm, areas, 0, size,
				  snd_pcm_mmap_read_areas);
}

ssize_t snd_pcm_mmap_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_areas_from_bufs(pcm, areas, bufs);
	return snd_pcm_read_areas(pcm, areas, 0, size,
				  snd_pcm_mmap_read_areas);
}

int snd_pcm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	return pcm->ops->channel_info(pcm, info);
}

int snd_pcm_channel_info_shm(snd_pcm_t *pcm, snd_pcm_channel_info_t *info,
			     int shmid)
{
	switch (pcm->access) {
	case SND_PCM_ACCESS_MMAP_INTERLEAVED:
	case SND_PCM_ACCESS_RW_INTERLEAVED:
		info->first = info->channel * pcm->bits_per_sample;
		info->step = pcm->bits_per_frame;
		break;
	case SND_PCM_ACCESS_MMAP_NONINTERLEAVED:
	case SND_PCM_ACCESS_RW_NONINTERLEAVED:
		info->first = 0;
		info->step = pcm->bits_per_sample;
		break;
	default:
		assert(0);
		break;
	}
	info->addr = 0;
	info->type = SND_PCM_AREA_SHM;
	info->u.shm.shmid = shmid;
	return 0;
}	

int snd_pcm_mmap(snd_pcm_t *pcm)
{
	int err;
	unsigned int c;
	assert(pcm);
	assert(pcm->setup);
	assert(!pcm->mmap_channels);
	err = pcm->ops->mmap(pcm);
	if (err < 0)
		return err;
	pcm->mmap_channels = calloc(pcm->channels, sizeof(pcm->mmap_channels[0]));
	if (!pcm->mmap_channels)
		return -ENOMEM;
	assert(!pcm->running_areas);
	pcm->running_areas = calloc(pcm->channels, sizeof(pcm->running_areas[0]));
	if (!pcm->running_areas) {
		free(pcm->mmap_channels);
		pcm->mmap_channels = NULL;
		return -ENOMEM;
	}
	for (c = 0; c < pcm->channels; ++c) {
		snd_pcm_channel_info_t *i = &pcm->mmap_channels[c];
		i->channel = c;
		err = snd_pcm_channel_info(pcm, i);
		if (err < 0)
			return err;
	}
	for (c = 0; c < pcm->channels; ++c) {
		snd_pcm_channel_info_t *i = &pcm->mmap_channels[c];
		snd_pcm_channel_area_t *a = &pcm->running_areas[c];
		unsigned int c1;
		if (!i->addr) {
			char *ptr;
			size_t size = i->first + i->step * pcm->buffer_size;
			for (c1 = c + 1; c1 < pcm->channels; ++c1) {
				snd_pcm_channel_info_t *i1 = &pcm->mmap_channels[c1];
				size_t s;
				if (i1->type != i->type)
					continue;
				switch (i1->type) {
				case SND_PCM_AREA_MMAP:
					if (i1->u.mmap.fd != i->u.mmap.fd ||
					    i1->u.mmap.offset != i->u.mmap.offset)
						continue;
					break;
				case SND_PCM_AREA_SHM:
					if (i1->u.shm.shmid != i->u.shm.shmid)
						continue;
					break;
				default:
					assert(0);
				}
				s = i1->first + i1->step * pcm->buffer_size;
				if (s > size)
					size = s;
			}
			size = (size + 7) / 8;
			size = PAGE_ALIGN(size);
			switch (i->type) {
			case SND_PCM_AREA_MMAP:
				ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, i->u.mmap.fd, i->u.mmap.offset);
				if (ptr == MAP_FAILED) {
					SYSERR("mmap failed");
					return -errno;
				}
				i->addr = ptr;
				break;
			case SND_PCM_AREA_SHM:
				if (i->u.shm.shmid < 0) {
					int id;
					id = shmget(IPC_PRIVATE, size, 0666);
					if (id < 0) {
						SYSERR("shmget failed");
						return -errno;
					}
					i->u.shm.shmid = id;
				}
				ptr = shmat(i->u.shm.shmid, 0, 0);
				if (ptr == (void*) -1) {
					SYSERR("shmat failed");
					return -errno;
				}
				i->addr = ptr;
				break;
			default:
				assert(0);
			}
		}
		for (c1 = c + 1; c1 < pcm->channels; ++c1) {
			snd_pcm_channel_info_t *i1 = &pcm->mmap_channels[c1];
			if (i1->type != i->type)
				continue;
			switch (i1->type) {
			case SND_PCM_AREA_MMAP:
				if (i1->u.mmap.fd != i->u.mmap.fd ||
				    i1->u.mmap.offset != i->u.mmap.offset)
					continue;
				break;
			case SND_PCM_AREA_SHM:
				if (i1->u.shm.shmid != i->u.shm.shmid)
					continue;
				break;
			default:
				assert(0);
			}
			i1->addr = i->addr;
		}
		a->addr = i->addr;
		a->first = i->first;
		a->step = i->step;
	}
	return 0;
}

int snd_pcm_munmap(snd_pcm_t *pcm)
{
	int err;
	unsigned int c;
	assert(pcm);
	assert(pcm->mmap_channels);
	for (c = 0; c < pcm->channels; ++c) {
		snd_pcm_channel_info_t *i = &pcm->mmap_channels[c];
		unsigned int c1;
		size_t size = i->first + i->step * pcm->buffer_size;
		if (!i->addr)
			continue;
		for (c1 = c + 1; c1 < pcm->channels; ++c1) {
			snd_pcm_channel_info_t *i1 = &pcm->mmap_channels[c1];
			size_t s;
			if (i1->addr != i->addr)
				continue;
			i1->addr = NULL;
			s = i1->first + i1->step * pcm->buffer_size;
			if (s > size)
				size = s;
		}
		size = (size + 7) / 8;
		size = PAGE_ALIGN(size);
		switch (i->type) {
		case SND_PCM_AREA_MMAP:
			err = munmap(i->addr, size);
			if (err < 0) {
				SYSERR("mmap failed");
				return -errno;
			}
			break;
		case SND_PCM_AREA_SHM:
			err = shmdt(i->addr);
			if (err < 0) {
				SYSERR("shmdt failed");
				return -errno;
			}
			break;
		default:
			assert(0);
		}
		i->addr = NULL;
	}
	err = pcm->ops->munmap(pcm);
	if (err < 0)
		return err;
	free(pcm->mmap_channels);
	pcm->mmap_channels = 0;
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
		size_t cont = pcm->buffer_size - offset;
		if (cont < frames)
			frames = cont;
		switch (pcm->access) {
		case SND_PCM_ACCESS_MMAP_INTERLEAVED:
		{
			snd_pcm_channel_area_t *a = snd_pcm_mmap_areas(pcm);
			char *buf = snd_pcm_channel_area_addr(a, offset);
			err = _snd_pcm_writei(pcm, buf, size);
			break;
		}
		case SND_PCM_ACCESS_MMAP_NONINTERLEAVED:
		{
			size_t channels = pcm->channels;
			unsigned int c;
			void *bufs[channels];
			snd_pcm_channel_area_t *areas = snd_pcm_mmap_areas(pcm);
			for (c = 0; c < channels; ++c) {
				snd_pcm_channel_area_t *a = &areas[c];
				bufs[c] = snd_pcm_channel_area_addr(a, offset);
			}
			err = _snd_pcm_writen(pcm, bufs, size);
			break;
		}
		default:
			assert(0);
			err = -EINVAL;
			break;
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
		size_t cont = pcm->buffer_size - offset;
		if (cont < frames)
			frames = cont;
		switch (pcm->access) {
		case SND_PCM_ACCESS_MMAP_INTERLEAVED:
		{
			snd_pcm_channel_area_t *a = snd_pcm_mmap_areas(pcm);
			char *buf = snd_pcm_channel_area_addr(a, offset);
			err = _snd_pcm_readi(pcm, buf, size);
			break;
		}
		case SND_PCM_ACCESS_MMAP_NONINTERLEAVED:
		{
			size_t channels = pcm->channels;
			unsigned int c;
			void *bufs[channels];
			snd_pcm_channel_area_t *areas = snd_pcm_mmap_areas(pcm);
			for (c = 0; c < channels; ++c) {
				snd_pcm_channel_area_t *a = &areas[c];
				bufs[c] = snd_pcm_channel_area_addr(a, offset);
			}
			err = _snd_pcm_readn(pcm->fast_op_arg, bufs, size);
		}
		default:
			assert(0);
			err = -EINVAL;
			break;
		}
		if (err < 0)
			break;
		xfer += frames;
	}
	if (xfer > 0)
		return xfer;
	return err;
}
