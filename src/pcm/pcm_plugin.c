/*
 *  PCM - Common plugin code
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
  
#include <sys/shm.h>
#include <limits.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

int snd_pcm_plugin_close(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	int err = 0;
	if (plugin->close_slave)
		err = snd_pcm_close(plugin->slave);
	free(plugin);
	return 0;
}

int snd_pcm_plugin_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_nonblock(plugin->slave, nonblock);
}

int snd_pcm_plugin_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_async(plugin->slave, sig, pid);
}

int snd_pcm_plugin_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_info(plugin->slave, info);
}

int snd_pcm_plugin_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_hw_free(plugin->slave);
}

int snd_pcm_plugin_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_sw_params(plugin->slave, params);
}

int snd_pcm_plugin_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_channel_info_shm(pcm, info, plugin->shmid);
}

int snd_pcm_plugin_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	int err = snd_pcm_status(plugin->slave, status);
	if (err < 0)
		return err;
	status->avail = snd_pcm_mmap_avail(pcm);
	if (plugin->client_frames)
		status->avail_max = plugin->client_frames(pcm, status->avail_max);
	return 0;
}

snd_pcm_state_t snd_pcm_plugin_state(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_state(plugin->slave);
}

int snd_pcm_plugin_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	snd_pcm_sframes_t sd;
	int err = snd_pcm_delay(plugin->slave, &sd);
	if (err < 0)
		return err;
	if (plugin->client_frames)
		sd = plugin->client_frames(pcm, sd);
	*delayp = sd + snd_pcm_mmap_delay(pcm);
	return 0;
}

int snd_pcm_plugin_prepare(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	int err = snd_pcm_prepare(plugin->slave);
	if (err < 0)
		return err;
	plugin->hw_ptr = 0;
	plugin->appl_ptr = 0;
	if (plugin->init) {
		err = plugin->init(pcm);
		if (err < 0)
			return err;
	}
	return 0;
}

int snd_pcm_plugin_reset(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	int err = snd_pcm_reset(plugin->slave);
	if (err < 0)
		return err;
	plugin->hw_ptr = 0;
	plugin->appl_ptr = 0;
	if (plugin->init) {
		err = plugin->init(pcm);
		if (err < 0)
			return err;
	}
	return 0;
}

int snd_pcm_plugin_start(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_start(plugin->slave);
}

int snd_pcm_plugin_drop(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_drop(plugin->slave);
}

int snd_pcm_plugin_drain(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_drain(plugin->slave);
}

int snd_pcm_plugin_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_pause(plugin->slave, enable);
}

snd_pcm_sframes_t snd_pcm_plugin_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	snd_pcm_sframes_t n = snd_pcm_mmap_hw_avail(pcm);
	assert(n >= 0);
	if (n > 0) {
		if ((snd_pcm_uframes_t)n > frames)
			n = frames;
		frames -= n;
	}
	if (frames > 0) {
		snd_pcm_sframes_t err;
		/* FIXME: rate plugin */
		if (plugin->slave_frames)
			frames = plugin->slave_frames(pcm, frames);
		err = snd_pcm_rewind(plugin->slave, frames);
		if (err < 0) {
			if (n <= 0)
				return err;
			goto _end;
		}
		if (plugin->client_frames)
			err = plugin->client_frames(pcm, err);
		snd_pcm_mmap_hw_backward(pcm, err);
		n += err;
	}
 _end:
	snd_pcm_mmap_appl_backward(pcm, n);
	return n;
}

snd_pcm_sframes_t snd_pcm_plugin_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_sframes_t frames;
	snd_pcm_areas_from_buf(pcm, areas, (void*)buffer);
	frames = snd_pcm_write_areas(pcm, areas, 0, size, plugin->write);
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

snd_pcm_sframes_t snd_pcm_plugin_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_sframes_t frames;
	snd_pcm_areas_from_bufs(pcm, areas, bufs);
	frames = snd_pcm_write_areas(pcm, areas, 0, size, plugin->write);
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

snd_pcm_sframes_t snd_pcm_plugin_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_sframes_t frames;
	snd_pcm_areas_from_buf(pcm, areas, buffer);
	frames = snd_pcm_read_areas(pcm, areas, 0, size, plugin->read);
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

snd_pcm_sframes_t snd_pcm_plugin_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_sframes_t frames;
	snd_pcm_areas_from_bufs(pcm, areas, bufs);
	frames = snd_pcm_read_areas(pcm, areas, 0, size, plugin->read);
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

snd_pcm_sframes_t snd_pcm_plugin_mmap_forward(snd_pcm_t *pcm, snd_pcm_uframes_t client_size)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	snd_pcm_t *slave = plugin->slave;
	snd_pcm_uframes_t client_xfer = 0;
	snd_pcm_uframes_t slave_xfer = 0;
	snd_pcm_sframes_t err = 0;
	snd_pcm_sframes_t slave_size;
	if (pcm->stream == SND_PCM_STREAM_CAPTURE) {
		snd_pcm_mmap_appl_forward(pcm, client_size);
		// snd_pcm_plugin_avail_update(pcm);
		return client_size;
	}
	slave_size = snd_pcm_avail_update(slave);
	if (slave_size <= 0)
		return slave_size;
	while (client_xfer < client_size &&
	       slave_xfer < (snd_pcm_uframes_t)slave_size) {
		snd_pcm_uframes_t slave_frames = slave_size - slave_xfer;
		snd_pcm_uframes_t client_frames = client_size - client_xfer;
		snd_pcm_uframes_t offset = snd_pcm_mmap_hw_offset(pcm);
		snd_pcm_uframes_t cont = pcm->buffer_size - offset;
		if (cont < client_frames)
			client_frames = cont;
		err = plugin->write(pcm, pcm->running_areas, offset,
				    client_frames, &slave_frames);
		if (err < 0)
			break;
		snd_pcm_mmap_appl_forward(pcm, err);
		client_xfer += err;
		slave_xfer += slave_frames;
	}
	if (client_xfer > 0)
		return client_xfer;
	return err;
}

snd_pcm_sframes_t snd_pcm_plugin_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	snd_pcm_t *slave = plugin->slave;
	snd_pcm_uframes_t client_xfer;
	snd_pcm_uframes_t slave_xfer = 0;
	snd_pcm_sframes_t err = 0;
	snd_pcm_uframes_t client_size;
	snd_pcm_sframes_t slave_size = snd_pcm_avail_update(slave);
	if (slave_size <= 0)
		return slave_size;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK ||
	    pcm->access == SND_PCM_ACCESS_RW_INTERLEAVED ||
	    pcm->access == SND_PCM_ACCESS_RW_NONINTERLEAVED)
		return plugin->client_frames ?
			plugin->client_frames(pcm, slave_size) : slave_size;
	client_xfer = snd_pcm_mmap_capture_avail(pcm);
	client_size = pcm->buffer_size;
	while (slave_xfer < (snd_pcm_uframes_t)slave_size &&
	       client_xfer < client_size) {
		snd_pcm_uframes_t slave_frames = slave_size - slave_xfer;
		snd_pcm_uframes_t client_frames = client_size - client_xfer;
		snd_pcm_uframes_t offset = snd_pcm_mmap_hw_offset(pcm);
		snd_pcm_uframes_t cont = pcm->buffer_size - offset;
		if (cont < client_frames)
			client_frames = cont;
		err = plugin->read(pcm, pcm->running_areas, offset,
				   client_frames, &slave_frames);
		if (err < 0)
			break;
		client_xfer += err;
		slave_xfer += slave_frames;
	}
	if (client_xfer > 0)
		return client_xfer;
	return err;
}

int snd_pcm_plugin_mmap(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plug = pcm->private_data;
	size_t size = snd_pcm_frames_to_bytes(pcm, pcm->buffer_size);
	int id = shmget(IPC_PRIVATE, size, 0666);
	if (id < 0) {
		SYSERR("shmget failed");
		return -errno;
	}
	plug->shmid = id;
	return 0;
}

int snd_pcm_plugin_munmap(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plug = pcm->private_data;
	if (shmctl(plug->shmid, IPC_RMID, 0) < 0) {
		SYSERR("shmctl IPC_RMID failed");
			return -errno;
	}
	return 0;
}

int snd_pcm_plugin_poll_descriptor(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return _snd_pcm_poll_descriptor(plugin->slave);
}

int snd_pcm_plugin_hw_refine_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return snd_pcm_hw_refine(plugin->slave, params);
}

int snd_pcm_plugin_hw_params_slave(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_plugin_t *plugin = pcm->private_data;
	return _snd_pcm_hw_params(plugin->slave, params);
}

snd_pcm_fast_ops_t snd_pcm_plugin_fast_ops = {
	status: snd_pcm_plugin_status,
	state: snd_pcm_plugin_state,
	delay: snd_pcm_plugin_delay,
	prepare: snd_pcm_plugin_prepare,
	reset: snd_pcm_plugin_reset,
	start: snd_pcm_plugin_start,
	drop: snd_pcm_plugin_drop,
	drain: snd_pcm_plugin_drain,
	pause: snd_pcm_plugin_pause,
	rewind: snd_pcm_plugin_rewind,
	writei: snd_pcm_plugin_writei,
	writen: snd_pcm_plugin_writen,
	readi: snd_pcm_plugin_readi,
	readn: snd_pcm_plugin_readn,
	avail_update: snd_pcm_plugin_avail_update,
	mmap_forward: snd_pcm_plugin_mmap_forward,
};

