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
#include <string.h>
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
		snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_PLAYBACK];
		str->open = 1;
		str->mode = (mode & SND_PCM_NONBLOCK_PLAYBACK) ? SND_PCM_NONBLOCK : 0;
	}
	if (mode & SND_PCM_OPEN_CAPTURE) {
		snd_pcm_stream_t *str = &pcm->stream[SND_PCM_STREAM_CAPTURE];
		str->open = 1;
		str->mode = (mode & SND_PCM_NONBLOCK_CAPTURE) ? SND_PCM_NONBLOCK : 0;
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

int snd_pcm_stream_close(snd_pcm_t *pcm, int stream)
{
	int ret = 0;
	int err;
	snd_pcm_stream_t *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (str->mmap_control) {
		if ((err = snd_pcm_munmap_control(pcm, stream)) < 0)
			ret = err;
	}
	if (str->mmap_data) {
		if ((err = snd_pcm_munmap_data(pcm, stream)) < 0)
			ret = err;
	}
	if ((err = pcm->ops->stream_close(pcm, stream)) < 0)
		ret = err;
	str->open = 0;
	str->valid_setup = 0;
	return ret;
}	

int snd_pcm_close(snd_pcm_t *pcm)
{
	int err, ret = 0;
	int stream;

	if (!pcm)
		return -EFAULT;
	for (stream = 0; stream < 2; ++stream) {
		if (pcm->stream[stream].open) {
			if ((err = snd_pcm_stream_close(pcm, stream)) < 0)
				ret = err;
		}
	}
	free(pcm);
	return ret;
}

int snd_pcm_stream_nonblock(snd_pcm_t *pcm, int stream, int nonblock)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	if (!pcm->stream[stream].open)
		return -EBADFD;
	if ((err = pcm->ops->stream_nonblock(pcm, stream, nonblock)) < 0)
		return err;
	if (nonblock)
		pcm->stream[stream].mode |= SND_PCM_NONBLOCK;
	else
		pcm->stream[stream].mode &= ~SND_PCM_NONBLOCK;
	return 0;
}

int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	int stream;
	if (!pcm || !info)
		return -EFAULT;
	for (stream = 0; stream < 2; ++stream) {
		if (pcm->stream[stream].open)
			return pcm->ops->info(pcm, stream, info);
	}
	return -EBADFD;
}

int snd_pcm_stream_info(snd_pcm_t *pcm, snd_pcm_stream_info_t *info)
{
	if (!pcm || !info)
		return -EFAULT;
	if (info->stream < 0 || info->stream > 1)
		return -EINVAL;
	if (!pcm->stream[info->stream].open)
		return -EBADFD;
	return pcm->ops->stream_info(pcm, info);
}

int snd_pcm_stream_params(snd_pcm_t *pcm, snd_pcm_stream_params_t *params)
{
	int err;
	snd_pcm_stream_setup_t setup;
	snd_pcm_stream_t *str;
	if (!pcm || !params)
		return -EFAULT;
	if (params->stream < 0 || params->stream > 1)
		return -EINVAL;
	str = &pcm->stream[params->stream];
	if (!str->open)
		return -EBADFD;
	if (str->mmap_control)
		return -EBADFD;
	if ((err = pcm->ops->stream_params(pcm, params)) < 0)
		return err;
	str->valid_setup = 0;
	setup.stream = params->stream;
	return snd_pcm_stream_setup(pcm, &setup);
}

int snd_pcm_stream_setup(snd_pcm_t *pcm, snd_pcm_stream_setup_t *setup)
{
	int err;
	snd_pcm_stream_t *str;
	if (!pcm || !setup)
		return -EFAULT;
	if (setup->stream < 0 || setup->stream > 1)
		return -EINVAL;
	str = &pcm->stream[setup->stream];
	if (!str->open)
		return -EBADFD;
	if (str->valid_setup) {
		memcpy(setup, &str->setup, sizeof(*setup));
		return 0;
	}
	if ((err = pcm->ops->stream_setup(pcm, setup)) < 0)
		return err;
	memcpy(&str->setup, setup, sizeof(*setup));
	str->sample_width = snd_pcm_format_physical_width(setup->format.format);
	str->bits_per_frame = str->sample_width * setup->format.channels;
	str->frames_per_frag = setup->frag_size * 8 / str->bits_per_frame;
	str->valid_setup = 1;
	return 0;
}

const snd_pcm_stream_setup_t* snd_pcm_stream_cached_setup(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	if (!pcm)
		return 0;
	if (stream < 0 || stream > 1)
		return 0;
	str = &pcm->stream[stream];
	if (!str->open || !str->valid_setup)
		return 0;
	return &str->setup;
}

int snd_pcm_channel_setup(snd_pcm_t *pcm, int stream, snd_pcm_channel_setup_t *setup)
{
	snd_pcm_stream_t *str;
	if (!pcm || !setup)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
	return pcm->ops->channel_setup(pcm, stream, setup);
}

int snd_pcm_stream_status(snd_pcm_t *pcm, snd_pcm_stream_status_t *status)
{
	if (!pcm || !status)
		return -EFAULT;
	if (status->stream < 0 || status->stream > 1)
		return -EINVAL;
	if (!pcm->stream[status->stream].open)
		return -EBADFD;
	return pcm->ops->stream_status(pcm, status);
}

int snd_pcm_stream_state(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (str->mmap_control)
		return str->mmap_control->status;
	return pcm->ops->stream_state(pcm, stream);
}

int snd_pcm_stream_byte_io(snd_pcm_t *pcm, int stream, int update)
{
	snd_pcm_stream_t *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (str->mmap_control && !update)
		return str->mmap_control->byte_io;
	return pcm->ops->stream_byte_io(pcm, stream, update);
}

int snd_pcm_stream_prepare(snd_pcm_t *pcm, int stream)
{
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	if (!pcm->stream[stream].open)
		return -EBADFD;
	return pcm->ops->stream_prepare(pcm, stream);
}

int snd_pcm_playback_prepare(snd_pcm_t *pcm)
{
	return snd_pcm_stream_prepare(pcm, SND_PCM_STREAM_PLAYBACK);
}

int snd_pcm_capture_prepare(snd_pcm_t *pcm)
{
	return snd_pcm_stream_prepare(pcm, SND_PCM_STREAM_CAPTURE);
}

int snd_pcm_stream_go(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	return pcm->ops->stream_go(pcm, stream);
}

int snd_pcm_playback_go(snd_pcm_t *pcm)
{
	return snd_pcm_stream_go(pcm, SND_PCM_STREAM_PLAYBACK);
}

int snd_pcm_capture_go(snd_pcm_t *pcm)
{
	return snd_pcm_stream_go(pcm, SND_PCM_STREAM_CAPTURE);
}

int snd_pcm_sync_go(snd_pcm_t *pcm, snd_pcm_sync_t *sync)
{
	int stream;
	if (!pcm || !sync)
		return -EFAULT;
	for (stream = 0; stream < 2; ++stream) {
		if (pcm->stream[stream].open)
			return pcm->ops->sync_go(pcm, stream, sync);
	}
	return -EBADFD;
}

int snd_pcm_stream_drain(snd_pcm_t *pcm, int stream)
{
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	if (!pcm->stream[stream].open)
		return -EBADFD;
	if (stream != SND_PCM_STREAM_PLAYBACK)
		return -EBADFD;
	return pcm->ops->stream_drain(pcm, stream);
}

int snd_pcm_playback_drain(snd_pcm_t *pcm)
{
	return snd_pcm_stream_drain(pcm, SND_PCM_STREAM_PLAYBACK);
}

int snd_pcm_stream_flush(snd_pcm_t *pcm, int stream)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	if (!pcm->stream[stream].open)
		return -EBADFD;
	return pcm->ops->stream_flush(pcm, stream);
}

int snd_pcm_playback_flush(snd_pcm_t *pcm)
{
	return snd_pcm_stream_flush(pcm, SND_PCM_STREAM_PLAYBACK);
}

int snd_pcm_capture_flush(snd_pcm_t *pcm)
{
	return snd_pcm_stream_flush(pcm, SND_PCM_STREAM_CAPTURE);
}

int snd_pcm_stream_pause(snd_pcm_t *pcm, int stream, int enable)
{
	int err;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	if (!pcm->stream[stream].open)
		return -EBADFD;
	if (stream != SND_PCM_STREAM_PLAYBACK)
		return -EBADFD;
	return pcm->ops->stream_pause(pcm, stream, enable);
}

int snd_pcm_playback_pause(snd_pcm_t *pcm, int enable)
{
	return snd_pcm_stream_pause(pcm, SND_PCM_STREAM_PLAYBACK, enable);
}

ssize_t snd_pcm_transfer_size(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (!str->valid_setup)
		return -EBADFD;
	if (str->setup.mode != SND_PCM_MODE_FRAGMENT)
		return -EBADFD;
	return str->setup.frag_size;
}

ssize_t snd_pcm_stream_seek(snd_pcm_t *pcm, int stream, off_t offset)
{
	snd_pcm_stream_t *str;
	size_t bytes_per_frame;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open)
		return -EBADFD;
	if (!str->valid_setup)
		return -EBADFD;
#if 0
	/* TODO */
	if (str->mmap_control) {
	} else
#endif
		return pcm->ops->stream_seek(pcm, stream, offset);
}

ssize_t snd_pcm_write(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	if (!pcm)
		return -EFAULT;
	if (!pcm->stream[SND_PCM_STREAM_PLAYBACK].open ||
	    !pcm->stream[SND_PCM_STREAM_PLAYBACK].valid_setup)
		return -EBADFD;
	if (size > 0 && !buffer)
		return -EFAULT;
	return pcm->ops->write(pcm, buffer, size);
}

ssize_t snd_pcm_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	if (!pcm)
		return -EFAULT;
	if (!pcm->stream[SND_PCM_STREAM_PLAYBACK].open ||
	    !pcm->stream[SND_PCM_STREAM_PLAYBACK].valid_setup)
		return -EBADFD;
	if (count > 0 && !vector)
		return -EFAULT;
	return pcm->ops->writev(pcm, vector, count);
}

ssize_t snd_pcm_read(snd_pcm_t *pcm, void *buffer, size_t size)
{
	if (!pcm)
		return -EFAULT;
	if (!pcm->stream[SND_PCM_STREAM_CAPTURE].open ||
	    !pcm->stream[SND_PCM_STREAM_CAPTURE].valid_setup)
		return -EBADFD;
	if (size > 0 && !buffer)
		return -EFAULT;
	return pcm->ops->read(pcm, buffer, size);
}

ssize_t snd_pcm_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	if (!pcm)
		return -EFAULT;
	if (!pcm->stream[SND_PCM_STREAM_CAPTURE].open ||
	    !pcm->stream[SND_PCM_STREAM_CAPTURE].valid_setup)
		return -EBADFD;
	if (count > 0 && !vector)
		return -EFAULT;
	return pcm->ops->readv(pcm, vector, count);
}

int snd_pcm_file_descriptor(snd_pcm_t* pcm, int stream)
{
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	if (!pcm->stream[stream].open)
		return -EBADFD;
	return pcm->ops->file_descriptor(pcm, stream);
}

int snd_pcm_channels_mask(snd_pcm_t *pcm, int stream, bitset_t *client_vmask)
{
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	if (!pcm->stream[stream].open)
		return -EBADFD;
	return pcm->ops->channels_mask(pcm, stream, client_vmask);
}

ssize_t snd_pcm_bytes_per_second(snd_pcm_t *pcm, int stream)
{
	snd_pcm_stream_t *str;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
	return snd_pcm_format_bytes_per_second(&str->setup.format);
}

typedef struct {
	int value;
	const char* name;
	const char* desc;
} assoc_t;

static assoc_t *assoc_value(int value, assoc_t *alist)
{
	while (alist->desc) {
		if (value == alist->value)
			return alist;
		alist++;
	}
	return 0;
}

static assoc_t *assoc_name(const char *name, assoc_t *alist)
{
	while (alist->name) {
		if (strcasecmp(name, alist->name) == 0)
			return alist;
		alist++;
	}
	return 0;
}

static const char *assoc(int value, assoc_t *alist)
{
	assoc_t *a;
	a = assoc_value(value, alist);
	if (a)
		return a->name;
	return "UNKNOWN";
}

#define STREAM(v) { SND_PCM_STREAM_##v, #v, #v }
#define MODE(v) { SND_PCM_MODE_##v, #v, #v }
#define FMT(v, d) { SND_PCM_SFMT_##v, #v, d }
#define XRUN(v) { SND_PCM_XRUN_##v, #v, #v }
#define START(v) { SND_PCM_START_##v, #v, #v }
#define FILL(v) { SND_PCM_FILL_##v, #v, #v }
#define END { 0, NULL, NULL }

static assoc_t streams[] = { STREAM(PLAYBACK), STREAM(CAPTURE), END };
static assoc_t modes[] = { MODE(FRAME), MODE(FRAGMENT), END };
static assoc_t fmts[] = {
	FMT(S8, "Signed 8-bit"), 
	FMT(U8, "Unsigned 8-bit"),
	FMT(S16_LE, "Signed 16-bit Little Endian"),
	FMT(S16_BE, "Signed 16-bit Big Endian"),
	FMT(U16_LE, "Unsigned 16-bit Little Endian"),
	FMT(U16_BE, "Unsigned 16-bit Big Endian"),
	FMT(S24_LE, "Signed 24-bit Little Endian"),
	FMT(S24_BE, "Signed 24-bit Big Endian"),
	FMT(U24_LE, "Unsigned 24-bit Little Endian"),
	FMT(U24_BE, "Unsigned 24-bit Big Endian"),
	FMT(S32_LE, "Signed 32-bit Little Endian"),
	FMT(S32_BE, "Signed 32-bit Big Endian"),
	FMT(U32_LE, "Unsigned 32-bit Little Endian"),
	FMT(U32_BE, "Unsigned 32-bit Big Endian"),
	FMT(FLOAT_LE, "Float Little Endian"),
	FMT(FLOAT_BE, "Float Big Endian"),
	FMT(FLOAT64_LE, "Float64 Little Endian"),
	FMT(FLOAT64_BE, "Float64 Big Endian"),
	FMT(IEC958_SUBFRAME_LE, "IEC-958 Little Endian"),
	FMT(IEC958_SUBFRAME_BE, "IEC-958 Big Endian"),
	FMT(MU_LAW, "Mu-Law"),
	FMT(A_LAW, "A-Law"),
	FMT(IMA_ADPCM, "Ima-ADPCM"),
	FMT(MPEG, "MPEG"),
	FMT(GSM, "GSM"),
	FMT(SPECIAL, "Special"),
	END 
};

static assoc_t starts[] = { START(GO), START(DATA), START(FULL), END };
static assoc_t xruns[] = { XRUN(FLUSH), XRUN(DRAIN), END };
static assoc_t fills[] = { FILL(NONE), FILL(SILENCE_WHOLE), FILL(SILENCE), END };
static assoc_t onoff[] = { {0, "OFF", NULL}, {1, "ON", NULL}, {-1, "ON", NULL}, END };

int snd_pcm_dump_setup(snd_pcm_t *pcm, int stream, FILE *fp)
{
	snd_pcm_stream_t *str;
	snd_pcm_stream_setup_t *setup;
	if (!pcm)
		return -EFAULT;
	if (stream < 0 || stream > 1)
		return -EINVAL;
	str = &pcm->stream[stream];
	if (!str->open || !str->valid_setup)
		return -EBADFD;
	setup = &str->setup;
	fprintf(fp, "stream: %s\n", assoc(setup->stream, streams));
	fprintf(fp, "mode: %s\n", assoc(setup->mode, modes));
	fprintf(fp, "format: %s\n", assoc(setup->format.format, fmts));
	fprintf(fp, "channels: %d\n", setup->format.channels);
	fprintf(fp, "rate: %d\n", setup->format.rate);
	// digital
	fprintf(fp, "start_mode: %s\n", assoc(setup->start_mode, starts));
	fprintf(fp, "xrun_mode: %s\n", assoc(setup->xrun_mode, xruns));
	fprintf(fp, "time: %s\n", assoc(setup->time, onoff));
	// ust_time
	// sync
	fprintf(fp, "buffer_size: %d\n", setup->buffer_size);
	fprintf(fp, "frag_size: %d\n", setup->frag_size);
	fprintf(fp, "frags: %d\n", setup->frags);
	fprintf(fp, "byte_boundary: %d\n", setup->byte_boundary);
	fprintf(fp, "msbits_per_sample: %d\n", setup->msbits_per_sample);
	fprintf(fp, "bytes_min: %d\n", setup->bytes_min);
	fprintf(fp, "bytes_align: %d\n", setup->bytes_align);
	fprintf(fp, "bytes_xrun_max: %d\n", setup->bytes_xrun_max);
	fprintf(fp, "fill_mode: %s\n", assoc(setup->fill_mode, fills));
	fprintf(fp, "bytes_fill_max: %d\n", setup->bytes_fill_max);
	return 0;
}

const char *snd_pcm_get_format_name(int format)
{
	assoc_t *a = assoc_value(format, fmts);
	if (a)
		return a->name;
	return 0;
}

const char *snd_pcm_get_format_description(int format)
{
	assoc_t *a = assoc_value(format, fmts);
	if (a)
		return a->desc;
	return "Unknown";
}

int snd_pcm_get_format_value(const char* name)
{
	assoc_t *a = assoc_name(name, fmts);
	if (a)
		return a->value;
	return -1;
}

