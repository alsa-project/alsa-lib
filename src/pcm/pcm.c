/*
 *  PCM Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@suse.cz>
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

int snd_pcm_abstract_open(snd_pcm_t **handle, int mode,
			  snd_pcm_type_t type, size_t extra)
{
	snd_pcm_t *pcm;

	if (!handle)
		return -EFAULT;
	*handle = NULL;

	pcm = (snd_pcm_t *) calloc(1, sizeof(snd_pcm_t) + extra);
	if (pcm == NULL)
		return -ENOMEM;
	if (mode & SND_PCM_OPEN_PLAYBACK) {
		struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_PLAYBACK];
		chan->open = 1;
		chan->mode = (mode & SND_PCM_NONBLOCK_PLAYBACK) ? SND_PCM_NONBLOCK : 0;
	}
	if (mode & SND_PCM_OPEN_CAPTURE) {
		struct snd_pcm_chan *chan = &pcm->chan[SND_PCM_CHANNEL_CAPTURE];
		chan->open = 1;
		chan->mode = (mode & SND_PCM_NONBLOCK_CAPTURE) ? SND_PCM_NONBLOCK : 0;
	}
	pcm->type = type;
	pcm->mode = mode & SND_PCM_OPEN_DUPLEX;
	*handle = pcm;
	return 0;
}

snd_pcm_type_t snd_pcm_type(snd_pcm_t *handle)
{
	return handle->type;
}

int snd_pcm_channel_close(snd_pcm_t *pcm, int channel)
{
	int ret = 0;
	int err;
	struct snd_pcm_chan *chan;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open)
		return -EBADFD;
	if (chan->mmap_control) {
		if ((err = snd_pcm_munmap_control(pcm, channel)) < 0)
			ret = err;
	}
	if (chan->mmap_data) {
		if ((err = snd_pcm_munmap_data(pcm, channel)) < 0)
			ret = err;
	}
	if ((err = pcm->ops->channel_close(pcm, channel)) < 0)
		ret = err;
	chan->open = 0;
	chan->valid_setup = 0;
	return ret;
}	

int snd_pcm_close(snd_pcm_t *pcm)
{
	int err, ret = 0;
	int channel;

	if (!pcm)
		return -EFAULT;
	for (channel = 0; channel < 2; ++channel) {
		if (pcm->chan[channel].open) {
			if ((err = snd_pcm_channel_close(pcm, channel)) < 0)
				ret = err;
		}
	}
	free(pcm);
	return ret;
}

int snd_pcm_channel_nonblock(snd_pcm_t *pcm, int channel, int nonblock)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;
	if ((err = pcm->ops->channel_nonblock(pcm, channel, nonblock)) < 0)
		return err;
	if (nonblock)
		pcm->chan[channel].mode |= SND_PCM_NONBLOCK;
	else
		pcm->chan[channel].mode &= ~SND_PCM_NONBLOCK;
	return 0;
}

int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	if (!pcm || !info)
		return -EFAULT;
	return pcm->ops->info(pcm, info);
}

int snd_pcm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	if (!pcm || !info)
		return -EFAULT;
	if (info->channel < 0 || info->channel > 1)
		return -EINVAL;
	if (!pcm->chan[info->channel].open)
		return -EBADFD;
	return pcm->ops->channel_info(pcm, info);
}

int snd_pcm_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	int err;
	snd_pcm_channel_setup_t setup;
	struct snd_pcm_chan *chan;
	if (!pcm || !params)
		return -EFAULT;
	if (params->channel < 0 || params->channel > 1)
		return -EINVAL;
	chan = &pcm->chan[params->channel];
	if (!chan->open)
		return -EBADFD;
	if (chan->mmap_control)
		return -EBADFD;
	if ((err = pcm->ops->channel_params(pcm, params)) < 0)
		return err;
	chan->valid_setup = 0;
	setup.channel = params->channel;
	return snd_pcm_channel_setup(pcm, &setup);
}

int snd_pcm_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	int err;
	struct snd_pcm_chan *chan;
	if (!pcm || !setup)
		return -EFAULT;
	if (setup->channel < 0 || setup->channel > 1)
		return -EINVAL;
	chan = &pcm->chan[setup->channel];
	if (!chan->open)
		return -EBADFD;
	if (chan->valid_setup) {
		memcpy(setup, &chan->setup, sizeof(*setup));
		return 0;
	}
	if ((err = pcm->ops->channel_setup(pcm, setup)) < 0)
		return err;
	memcpy(&chan->setup, setup, sizeof(*setup));
	chan->sample_width = snd_pcm_format_physical_width(setup->format.format);
	chan->bits_per_sample = chan->sample_width * setup->format.voices;
	chan->samples_per_frag = setup->frag_size * 8 / chan->bits_per_sample;
	chan->valid_setup = 1;
	return 0;
}

const snd_pcm_channel_setup_t* snd_pcm_channel_cached_setup(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan;
	if (!pcm)
		return 0;
	if (channel < 0 || channel > 1)
		return 0;
	chan = &pcm->chan[channel];
	if (!chan->open || !chan->valid_setup)
		return 0;
	return &chan->setup;
}

int snd_pcm_voice_setup(snd_pcm_t *pcm, int channel, snd_pcm_voice_setup_t *setup)
{
	struct snd_pcm_chan *chan;
	if (!pcm || !setup)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open || !chan->valid_setup)
		return -EBADFD;
	return pcm->ops->voice_setup(pcm, channel, setup);
}

int snd_pcm_channel_status(snd_pcm_t *pcm, snd_pcm_channel_status_t *status)
{
	if (!pcm || !status)
		return -EFAULT;
	if (status->channel < 0 || status->channel > 1)
		return -EINVAL;
	if (!pcm->chan[status->channel].open)
		return -EBADFD;
	return pcm->ops->channel_status(pcm, status);
}

int snd_pcm_channel_update(snd_pcm_t *pcm, int channel)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;
	err = pcm->ops->channel_update(pcm, channel);
	if (err < 0)
		return err;
	snd_pcm_mmap_status_change(pcm, channel, -1);
	return 0;
}

int snd_pcm_channel_prepare(snd_pcm_t *pcm, int channel)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;
	err = pcm->ops->channel_prepare(pcm, channel);
	if (err < 0)
		return err;
	snd_pcm_mmap_status_change(pcm, channel, SND_PCM_STATUS_PREPARED);
	return 0;
}

int snd_pcm_playback_prepare(snd_pcm_t *pcm)
{
	return snd_pcm_channel_prepare(pcm, SND_PCM_CHANNEL_PLAYBACK);
}

int snd_pcm_capture_prepare(snd_pcm_t *pcm)
{
	return snd_pcm_channel_prepare(pcm, SND_PCM_CHANNEL_CAPTURE);
}

static int mmap_playback_go(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan = &pcm->chan[channel];
	if (chan->mmap_control->status != SND_PCM_STATUS_PREPARED)
		return -EBADFD;
	if (chan->mmap_control->byte_data == 0)
		return -EIO;
	chan->mmap_control->status = SND_PCM_STATUS_RUNNING;
	pthread_mutex_lock(&chan->mutex);
	pthread_cond_signal(&chan->status_cond);
	pthread_cond_wait(&chan->ready_cond, &chan->mutex);
	pthread_mutex_unlock(&chan->mutex);
	return 0;
}

int snd_pcm_channel_go(snd_pcm_t *pcm, int channel)
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
	if (channel == SND_PCM_CHANNEL_PLAYBACK &&
	    chan->mmap_data_emulation) {
		err = mmap_playback_go(pcm, channel);
		if (err < 0)
			return err;
	}
	err = pcm->ops->channel_go(pcm, channel);
	if (err < 0)
		return err;
	if (channel == SND_PCM_CHANNEL_CAPTURE)
		snd_pcm_mmap_status_change(pcm, channel, SND_PCM_STATUS_RUNNING);
	return 0;
}

int snd_pcm_playback_go(snd_pcm_t *pcm)
{
	return snd_pcm_channel_go(pcm, SND_PCM_CHANNEL_PLAYBACK);
}

int snd_pcm_capture_go(snd_pcm_t *pcm)
{
	return snd_pcm_channel_go(pcm, SND_PCM_CHANNEL_CAPTURE);
}

int snd_pcm_sync_go(snd_pcm_t *pcm, snd_pcm_sync_t *sync)
{
	int err;
	if (!pcm || !sync)
		return -EFAULT;
	if (!pcm->chan[SND_PCM_CHANNEL_PLAYBACK].open &&
	    !pcm->chan[SND_PCM_CHANNEL_CAPTURE].open)
		return -EBADFD;
	err = pcm->ops->sync_go(pcm, sync);
	if (err < 0)
		return err;
	/* NYI: mmap emulation */
	return 0;
}

int snd_pcm_channel_drain(snd_pcm_t *pcm, int channel)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;
	if (channel != SND_PCM_CHANNEL_PLAYBACK)
		return -EBADFD;
	err = pcm->ops->channel_drain(pcm, channel);
	if (err < 0)
		return err;
	snd_pcm_mmap_status_change(pcm, channel, SND_PCM_STATUS_READY);
	return 0;
}

int snd_pcm_playback_drain(snd_pcm_t *pcm)
{
	return snd_pcm_channel_drain(pcm, SND_PCM_CHANNEL_PLAYBACK);
}

int snd_pcm_channel_flush(snd_pcm_t *pcm, int channel)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;
	err = pcm->ops->channel_flush(pcm, channel);
	if (err < 0)
		return err;
	snd_pcm_mmap_status_change(pcm, channel, SND_PCM_STATUS_READY);
	return 0;
}

int snd_pcm_playback_flush(snd_pcm_t *pcm)
{
	return snd_pcm_channel_flush(pcm, SND_PCM_CHANNEL_PLAYBACK);
}

int snd_pcm_capture_flush(snd_pcm_t *pcm)
{
	return snd_pcm_channel_flush(pcm, SND_PCM_CHANNEL_CAPTURE);
}

int snd_pcm_channel_pause(snd_pcm_t *pcm, int channel, int enable)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;
	if (channel != SND_PCM_CHANNEL_PLAYBACK)
		return -EBADFD;
	err = pcm->ops->channel_pause(pcm, channel, enable);
	if (err < 0)
		return err;
	snd_pcm_mmap_status_change(pcm, channel, SND_PCM_STATUS_PAUSED);
	return 0;
}

int snd_pcm_playback_pause(snd_pcm_t *pcm, int enable)
{
	return snd_pcm_channel_pause(pcm, SND_PCM_CHANNEL_PLAYBACK, enable);
}

ssize_t snd_pcm_transfer_size(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open)
		return -EBADFD;
	if (!chan->valid_setup)
		return -EBADFD;
	if (chan->setup.mode != SND_PCM_MODE_BLOCK)
		return -EBADFD;
	return chan->setup.frag_size;
}

ssize_t snd_pcm_write(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	if (!pcm)
		return -EFAULT;
	if (!pcm->chan[SND_PCM_CHANNEL_PLAYBACK].open ||
	    !pcm->chan[SND_PCM_CHANNEL_PLAYBACK].valid_setup)
		return -EBADFD;
	if (size > 0 && !buffer)
		return -EFAULT;
	return pcm->ops->write(pcm, buffer, size);
}

ssize_t snd_pcm_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	if (!pcm)
		return -EFAULT;
	if (!pcm->chan[SND_PCM_CHANNEL_PLAYBACK].open ||
	    !pcm->chan[SND_PCM_CHANNEL_PLAYBACK].valid_setup)
		return -EBADFD;
	if (count > 0 && !vector)
		return -EFAULT;
	return pcm->ops->writev(pcm, vector, count);
}

ssize_t snd_pcm_read(snd_pcm_t *pcm, void *buffer, size_t size)
{
	if (!pcm)
		return -EFAULT;
	if (!pcm->chan[SND_PCM_CHANNEL_CAPTURE].open ||
	    !pcm->chan[SND_PCM_CHANNEL_CAPTURE].valid_setup)
		return -EBADFD;
	if (size > 0 && !buffer)
		return -EFAULT;
	return pcm->ops->read(pcm, buffer, size);
}

ssize_t snd_pcm_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	if (!pcm)
		return -EFAULT;
	if (!pcm->chan[SND_PCM_CHANNEL_CAPTURE].open ||
	    !pcm->chan[SND_PCM_CHANNEL_CAPTURE].valid_setup)
		return -EBADFD;
	if (count > 0 && !vector)
		return -EFAULT;
	return pcm->ops->readv(pcm, vector, count);
}

int snd_pcm_file_descriptor(snd_pcm_t* pcm, int channel)
{
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;
	return pcm->ops->file_descriptor(pcm, channel);
}

int snd_pcm_voices_mask(snd_pcm_t *pcm, int channel, bitset_t *client_vmask)
{
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	if (!pcm->chan[channel].open)
		return -EBADFD;
	return pcm->ops->voices_mask(pcm, channel, client_vmask);
}

ssize_t snd_pcm_bytes_per_second(snd_pcm_t *pcm, int channel)
{
	struct snd_pcm_chan *chan;
	if (!pcm)
		return -EFAULT;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	chan = &pcm->chan[channel];
	if (!chan->open || !chan->valid_setup)
		return -EBADFD;
	return snd_pcm_format_bytes_per_second(&chan->setup.format);
}

