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
  
#include "pcm_local.h"
#include "pcm_plugin.h"
#include <limits.h>

int snd_pcm_plugin_close(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	int err = 0;
	if (plugin->close_slave)
		err = snd_pcm_close(plugin->slave);
	free(plugin);
	return 0;
}

int snd_pcm_plugin_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_nonblock(plugin->slave, nonblock);
}

int snd_pcm_plugin_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_info(plugin->slave, info);
}

int snd_pcm_plugin_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_channel_info(plugin->slave, info);
}

int snd_pcm_plugin_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t * params)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_channel_params(plugin->slave, params);
}

int snd_pcm_plugin_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t * setup)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	int err;
	err = snd_pcm_channel_setup(plugin->slave, setup);
	if (err < 0)
		return err;
	if (pcm->setup.mmap_shape == SND_PCM_MMAP_INTERLEAVED) {
		setup->area.addr = pcm->mmap_data;
		setup->area.first = setup->channel * pcm->bits_per_sample;
		setup->area.step = pcm->bits_per_frame;
	} else {
		setup->area.addr = pcm->mmap_data + setup->channel * pcm->setup.buffer_size * pcm->bits_per_sample / 8;
		setup->area.first = 0;
		setup->area.step = pcm->bits_per_sample;
	}
	return 0;
}

int snd_pcm_plugin_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	int err = snd_pcm_status(plugin->slave, status);
	if (err < 0)
		return err;
	status->hw_ptr = plugin->mmap_status.hw_ptr;
	status->appl_ptr = plugin->mmap_control.appl_ptr;
	status->avail = (pcm->stream == SND_PCM_STREAM_PLAYBACK ?
			 snd_pcm_mmap_playback_avail(pcm) :
			 snd_pcm_mmap_capture_avail(pcm));
	return 0;
}

int snd_pcm_plugin_state(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_state(plugin->slave);
}

int snd_pcm_plugin_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	ssize_t sd;
	int err = snd_pcm_delay(plugin->slave, &sd);
	int d;
	if (err < 0)
		return err;
	if (plugin->client_frames)
		sd = plugin->client_frames(pcm, sd);
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK)
		d = pcm->setup.buffer_size - snd_pcm_mmap_playback_avail(pcm);
	else
		d = snd_pcm_mmap_capture_avail(pcm);
	*delayp = sd + d;
	return 0;
}

int snd_pcm_plugin_prepare(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	int err = snd_pcm_prepare(plugin->slave);
	if (err < 0)
		return err;
	plugin->mmap_status.hw_ptr = 0;
	plugin->mmap_control.appl_ptr = 0;
	if (plugin->init) {
		err = plugin->init(pcm);
		if (err < 0)
			return err;
	}
	return 0;
}

int snd_pcm_plugin_start(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_start(plugin->slave);
}

int snd_pcm_plugin_drop(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_drop(plugin->slave);
}

int snd_pcm_plugin_drain(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_drain(plugin->slave);
}

int snd_pcm_plugin_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_pause(plugin->slave, enable);
}

ssize_t snd_pcm_plugin_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	ssize_t n = pcm->setup.buffer_size - snd_pcm_mmap_avail(pcm);
	if (n > 0) {
		if ((size_t)n > frames)
			n = frames;
		snd_pcm_mmap_appl_backward(pcm, n);
		frames -= n;
	}
	if (frames > 0) {
		ssize_t err = snd_pcm_rewind(plugin->slave, frames);
		if (err < 0) {
			if (n > 0)
				return n;
			return err;
		}
		if (plugin->client_frames)
			err = plugin->client_frames(pcm, err);
		n += err;
	}
	return n;
}

ssize_t snd_pcm_plugin_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	ssize_t frames;
	snd_pcm_areas_from_buf(pcm, areas, (void*)buffer);
	frames = snd_pcm_write_areas(pcm, areas, 0, size, plugin->write);
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

ssize_t snd_pcm_plugin_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	ssize_t frames;
	snd_pcm_areas_from_bufs(pcm, areas, bufs);
	frames = snd_pcm_write_areas(pcm, areas, 0, size, plugin->write);
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

ssize_t snd_pcm_plugin_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	ssize_t frames;
	snd_pcm_areas_from_buf(pcm, areas, buffer);
	frames = snd_pcm_read_areas(pcm, areas, 0, size, plugin->read);
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

ssize_t snd_pcm_plugin_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	snd_pcm_channel_area_t areas[pcm->setup.format.channels];
	ssize_t frames;
	snd_pcm_areas_from_bufs(pcm, areas, bufs);
	frames = snd_pcm_read_areas(pcm, areas, 0, size, plugin->read);
	if (frames > 0)
		snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

ssize_t snd_pcm_plugin_mmap_forward(snd_pcm_t *pcm, size_t client_size)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	snd_pcm_t *slave = plugin->slave;
	size_t client_xfer = 0;
	size_t slave_xfer = 0;
	ssize_t err = 0;
	ssize_t slave_size;
	if (pcm->stream == SND_PCM_STREAM_CAPTURE) {
		snd_pcm_mmap_appl_forward(pcm, client_size);
		return client_size;
	}
	slave_size = snd_pcm_avail_update(slave);
	if (slave_size <= 0)
		return slave_size;
	while (client_xfer < client_size &&
	       slave_xfer < (size_t)slave_size) {
		size_t slave_frames = slave_size - slave_xfer;
		size_t client_frames = client_size - client_xfer;
		size_t cont = pcm->setup.buffer_size - snd_pcm_mmap_hw_offset(pcm);
		if (cont < client_frames)
			client_frames = cont;
		err = plugin->write(pcm, pcm->mmap_areas, 
				    snd_pcm_mmap_hw_offset(pcm),
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

ssize_t snd_pcm_plugin_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	snd_pcm_t *slave = plugin->slave;
	size_t client_xfer;
	size_t slave_xfer = 0;
	ssize_t err = 0;
	size_t client_size;
	ssize_t slave_size = snd_pcm_avail_update(slave);
	if (slave_size <= 0)
		return slave_size;
	if (pcm->stream == SND_PCM_STREAM_PLAYBACK ||
	    !pcm->mmap_data)
		return plugin->client_frames ?
			plugin->client_frames(pcm, slave_size) : slave_size;
	client_xfer = snd_pcm_mmap_capture_avail(pcm);
	client_size = pcm->setup.buffer_size;
	while (slave_xfer < (size_t)slave_size &&
	       client_xfer < client_size) {
		size_t slave_frames = slave_size - slave_xfer;
		size_t client_frames = client_size - client_xfer;
		size_t cont = pcm->setup.buffer_size - snd_pcm_mmap_hw_offset(pcm);
		if (cont < client_frames)
			client_frames = cont;
		err = plugin->read(pcm, pcm->mmap_areas, 
				   snd_pcm_mmap_hw_offset(pcm),
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

int snd_pcm_plugin_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	pcm->mmap_status = &plugin->mmap_status;
	return 0;
}

int snd_pcm_plugin_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	pcm->mmap_control = &plugin->mmap_control;
	return 0;
}

int snd_pcm_plugin_mmap_data(snd_pcm_t *pcm)
{
	void *ptr = malloc(snd_pcm_frames_to_bytes(pcm, pcm->setup.buffer_size));
	if (!ptr)
		return -ENOMEM;
	pcm->mmap_data = ptr;
	return 0;
}

int snd_pcm_plugin_munmap_status(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

int snd_pcm_plugin_munmap_control(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

int snd_pcm_plugin_munmap_data(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	free(pcm->mmap_data);
	return 0;
}

int snd_pcm_plugin_poll_descriptor(snd_pcm_t *pcm)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_poll_descriptor(plugin->slave);
}

int snd_pcm_plugin_channels_mask(snd_pcm_t *pcm, bitset_t *cmask)
{
	snd_pcm_plugin_t *plugin = pcm->private;
	return snd_pcm_channels_mask(plugin->slave, cmask);
}

int conv_index(int src_format, int dst_format)
{
	int src_endian, dst_endian, sign, src_width, dst_width;

	sign = (snd_pcm_format_signed(src_format) !=
		snd_pcm_format_signed(dst_format));
#ifdef SND_LITTLE_ENDIAN
	src_endian = snd_pcm_format_big_endian(src_format);
	dst_endian = snd_pcm_format_big_endian(dst_format);
#else
	src_endian = snd_pcm_format_little_endian(src_format);
	dst_endian = snd_pcm_format_little_endian(dst_format);
#endif

	if (src_endian < 0)
		src_endian = 0;
	if (dst_endian < 0)
		dst_endian = 0;

	src_width = snd_pcm_format_width(src_format) / 8 - 1;
	dst_width = snd_pcm_format_width(dst_format) / 8 - 1;

	return src_width * 32 + src_endian * 16 + sign * 8 + dst_width * 2 + dst_endian;
}

int getput_index(int format)
{
	int sign, width, endian;
	sign = !snd_pcm_format_signed(format);
	width = snd_pcm_format_width(format) / 8 - 1;
#ifdef SND_LITTLE_ENDIAN
	endian = snd_pcm_format_big_endian(format);
#else
	endian = snd_pcm_format_little_endian(format);
#endif
	if (endian < 0)
		endian = 0;
	return width * 4 + endian * 2 + sign;
}

struct snd_pcm_fast_ops snd_pcm_plugin_fast_ops = {
	status: snd_pcm_plugin_status,
	state: snd_pcm_plugin_state,
	delay: snd_pcm_plugin_delay,
	prepare: snd_pcm_plugin_prepare,
	start: snd_pcm_plugin_start,
	drop: snd_pcm_plugin_drop,
	drain: snd_pcm_plugin_drain,
	pause: snd_pcm_plugin_pause,
	rewind: snd_pcm_plugin_rewind,
	writei: snd_pcm_plugin_writei,
	writen: snd_pcm_plugin_writen,
	readi: snd_pcm_plugin_readi,
	readn: snd_pcm_plugin_readn,
	poll_descriptor: snd_pcm_plugin_poll_descriptor,
	channels_mask: snd_pcm_plugin_channels_mask,
	avail_update: snd_pcm_plugin_avail_update,
	mmap_forward: snd_pcm_plugin_mmap_forward,
};

