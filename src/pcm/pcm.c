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
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <dlfcn.h>
#include "pcm_local.h"
#include "list.h"

int snd_pcm_init(snd_pcm_t *pcm)
{
	int err;
	err = snd_pcm_mmap_status(pcm, NULL);
	if (err < 0)
		return err;
	err = snd_pcm_mmap_control(pcm, NULL);
	if (err < 0)
		return err;
	return 0;
}

snd_pcm_type_t snd_pcm_type(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->type;
}

snd_pcm_type_t snd_pcm(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->stream;
}

int snd_pcm_close(snd_pcm_t *pcm)
{
	int ret = 0;
	int err;
	assert(pcm);
	if (pcm->mmap_status) {
		if ((err = snd_pcm_munmap_status(pcm)) < 0)
			ret = err;
	}
	if (pcm->mmap_control) {
		if ((err = snd_pcm_munmap_control(pcm)) < 0)
			ret = err;
	}
	if (pcm->mmap_data) {
		if ((err = snd_pcm_munmap_data(pcm)) < 0)
			ret = err;
	}
	if ((err = pcm->ops->close(pcm->op_arg)) < 0)
		ret = err;
	pcm->valid_setup = 0;
	if (pcm->name)
		free(pcm->name);
	free(pcm);
	return ret;
}	

int snd_pcm_nonblock(snd_pcm_t *pcm, int nonblock)
{
	int err;
	assert(pcm);
	if ((err = pcm->ops->nonblock(pcm->fast_op_arg, nonblock)) < 0)
		return err;
	if (nonblock)
		pcm->mode |= SND_PCM_NONBLOCK;
	else
		pcm->mode &= ~SND_PCM_NONBLOCK;
	return 0;
}

int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	assert(pcm && info);
	return pcm->ops->info(pcm->op_arg, info);
}

int snd_pcm_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t *info)
{
	assert(pcm && info);
	return pcm->ops->params_info(pcm->op_arg, info);
}

int snd_pcm_setup(snd_pcm_t *pcm, snd_pcm_setup_t *setup)
{
	int err;
	assert(pcm && setup);
	if (pcm->valid_setup) {
		*setup = pcm->setup;
		return 0;
	}
	if ((err = pcm->ops->setup(pcm->op_arg, &pcm->setup)) < 0)
		return err;
	*setup = pcm->setup;
	pcm->bits_per_sample = snd_pcm_format_physical_width(setup->format.sfmt);
        pcm->bits_per_frame = pcm->bits_per_sample * setup->format.channels;
	pcm->valid_setup = 1;
	return 0;
}

int snd_pcm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	assert(pcm && info);
	assert(pcm->valid_setup);
	assert(info->channel < pcm->setup.format.channels);
	return pcm->ops->channel_info(pcm->op_arg, info);
}

int snd_pcm_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
	assert(pcm && params);
	assert(pcm->valid_setup);
	assert(params->channel < pcm->setup.format.channels);
	return pcm->ops->channel_params(pcm->op_arg, params);
}

int snd_pcm_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
	assert(pcm && setup);
	assert(pcm->valid_setup);
	assert(setup->channel < pcm->setup.format.channels);
	return pcm->ops->channel_setup(pcm->op_arg, setup);
}

int snd_pcm_params(snd_pcm_t *pcm, snd_pcm_params_t *params)
{
	int err;
	snd_pcm_setup_t setup;
	assert(pcm && params);
	assert(!pcm->mmap_data);
	if ((err = pcm->ops->params(pcm->op_arg, params)) < 0)
		return err;
	pcm->valid_setup = 0;
	return snd_pcm_setup(pcm, &setup);
}

int snd_pcm_status(snd_pcm_t *pcm, snd_pcm_status_t *status)
{
	assert(pcm && status);
	return pcm->fast_ops->status(pcm->fast_op_arg, status);
}

int snd_pcm_state(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->fast_ops->state(pcm->fast_op_arg);
}

int snd_pcm_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return pcm->fast_ops->delay(pcm->fast_op_arg, delayp);
}

int snd_pcm_prepare(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return pcm->fast_ops->prepare(pcm->fast_op_arg);
}

int snd_pcm_start(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return pcm->fast_ops->start(pcm->fast_op_arg);
}

int snd_pcm_drop(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return pcm->fast_ops->drop(pcm->fast_op_arg);
}

int snd_pcm_drain(snd_pcm_t *pcm)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return pcm->fast_ops->drain(pcm->fast_op_arg);
}

int snd_pcm_pause(snd_pcm_t *pcm, int enable)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return pcm->fast_ops->pause(pcm->fast_op_arg, enable);
}


ssize_t snd_pcm_rewind(snd_pcm_t *pcm, size_t frames)
{
	assert(pcm);
	assert(pcm->valid_setup);
	assert(frames > 0);
	return pcm->fast_ops->rewind(pcm->fast_op_arg, frames);
}

ssize_t snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	assert(pcm);
	assert(size == 0 || buffer);
	assert(pcm->valid_setup);
	assert(pcm->setup.xfer_mode == SND_PCM_XFER_INTERLEAVED);
	assert(!pcm->mmap_data);
	return _snd_pcm_writei(pcm, buffer, size);
}

ssize_t snd_pcm_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	assert(pcm);
	assert(size == 0 || bufs);
	assert(pcm->valid_setup);
	assert(pcm->setup.xfer_mode == SND_PCM_XFER_NONINTERLEAVED);
	assert(!pcm->mmap_data);
	return _snd_pcm_writen(pcm, bufs, size);
}

ssize_t snd_pcm_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	assert(pcm);
	assert(size == 0 || buffer);
	assert(pcm->valid_setup);
	assert(pcm->setup.xfer_mode == SND_PCM_XFER_INTERLEAVED);
	assert(!pcm->mmap_data);
	return _snd_pcm_readi(pcm, buffer, size);
}

ssize_t snd_pcm_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	assert(pcm);
	assert(size == 0 || bufs);
	assert(pcm->valid_setup);
	assert(pcm->setup.xfer_mode == SND_PCM_XFER_NONINTERLEAVED);
	assert(!pcm->mmap_data);
	return _snd_pcm_readn(pcm, bufs, size);
}

ssize_t snd_pcm_writev(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	void **bufs;
	int k;
	assert(pcm);
	assert(pcm->valid_setup);
	assert((int)pcm->setup.format.channels == count);
	bufs = alloca(sizeof(*bufs) * count);
	for (k = 0; k < count; ++k) {
		bufs[k] = vector[k].iov_base;
		assert(vector[k].iov_len == vector[0].iov_len);
	}
	return snd_pcm_writen(pcm, bufs, vector[0].iov_len);
}

ssize_t snd_pcm_readv(snd_pcm_t *pcm, const struct iovec *vector, int count)
{
	void **bufs;
	int k;
	assert(pcm);
	assert(pcm->valid_setup);
	assert((int)pcm->setup.format.channels == count);
	bufs = alloca(sizeof(*bufs) * count);
	for (k = 0; k < count; ++k) {
		bufs[k] = vector[k].iov_base;
		assert(vector[k].iov_len == vector[0].iov_len);
	}
	return snd_pcm_readn(pcm, bufs, vector[0].iov_len);
}

/* FIXME */
#define snd_pcm_link_descriptor snd_pcm_poll_descriptor

int snd_pcm_link(snd_pcm_t *pcm1, snd_pcm_t *pcm2)
{
	int fd1 = snd_pcm_link_descriptor(pcm1);
	int fd2 = snd_pcm_link_descriptor(pcm2);
	if (fd1 < 0 || fd2 < 0)
		return -ENOSYS;
	if (ioctl(fd1, SND_PCM_IOCTL_LINK, fd2) < 0)
		return -errno;
	return 0;
}

int snd_pcm_unlink(snd_pcm_t *pcm)
{
	int fd;
	switch (pcm->type) {
	case SND_PCM_TYPE_HW:
	case SND_PCM_TYPE_MULTI:
		fd = snd_pcm_poll_descriptor(pcm);
		break;
	default:
		errno = -ENOSYS;
		return -1;
	}
	if (ioctl(fd, SND_PCM_IOCTL_UNLINK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_poll_descriptor(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->fast_ops->poll_descriptor(pcm->fast_op_arg);
}

int snd_pcm_channels_mask(snd_pcm_t *pcm, bitset_t *cmask)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return pcm->fast_ops->channels_mask(pcm->fast_op_arg, cmask);
}

typedef struct {
	int value;
	const char* name;
	const char* desc;
} assoc_t;

static assoc_t *assoc_value(int value, assoc_t *alist)
{
	while (alist->name) {
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

#define STATE(v) { SND_PCM_STATE_##v, #v, #v }
#define STREAM(v) { SND_PCM_STREAM_##v, #v, #v }
#define READY(v) { SND_PCM_READY_##v, #v, #v }
#define XRUN(v) { SND_PCM_XRUN_##v, #v, #v }
#define XFER(v) { SND_PCM_XFER_##v, #v, #v }
#define MMAP(v) { SND_PCM_MMAP_##v, #v, #v }
#define SFMT(v, d) { SND_PCM_SFMT_##v, #v, d }
#define XRUN_ACT(v) { SND_PCM_XRUN_ACT_##v, #v, #v }
#define START(v) { SND_PCM_START_##v, #v, #v }
#define FILL(v) { SND_PCM_FILL_##v, #v, #v }
#define END { 0, NULL, NULL }

static assoc_t states[] = { STATE(OPEN), STATE(SETUP), STATE(PREPARED),
			    STATE(RUNNING), STATE(XRUN), STATE(PAUSED), END };
static assoc_t streams[] = { STREAM(PLAYBACK), STREAM(CAPTURE), END };
static assoc_t xruns[] = { XRUN(ASAP), XRUN(FRAGMENT), XRUN(NONE), END };
static assoc_t fmts[] = {
	SFMT(S8, "Signed 8-bit"), 
	SFMT(U8, "Unsigned 8-bit"),
	SFMT(S16_LE, "Signed 16-bit Little Endian"),
	SFMT(S16_BE, "Signed 16-bit Big Endian"),
	SFMT(U16_LE, "Unsigned 16-bit Little Endian"),
	SFMT(U16_BE, "Unsigned 16-bit Big Endian"),
	SFMT(S24_LE, "Signed 24-bit Little Endian"),
	SFMT(S24_BE, "Signed 24-bit Big Endian"),
	SFMT(U24_LE, "Unsigned 24-bit Little Endian"),
	SFMT(U24_BE, "Unsigned 24-bit Big Endian"),
	SFMT(S32_LE, "Signed 32-bit Little Endian"),
	SFMT(S32_BE, "Signed 32-bit Big Endian"),
	SFMT(U32_LE, "Unsigned 32-bit Little Endian"),
	SFMT(U32_BE, "Unsigned 32-bit Big Endian"),
	SFMT(FLOAT_LE, "Float Little Endian"),
	SFMT(FLOAT_BE, "Float Big Endian"),
	SFMT(FLOAT64_LE, "Float64 Little Endian"),
	SFMT(FLOAT64_BE, "Float64 Big Endian"),
	SFMT(IEC958_SUBFRAME_LE, "IEC-958 Little Endian"),
	SFMT(IEC958_SUBFRAME_BE, "IEC-958 Big Endian"),
	SFMT(MU_LAW, "Mu-Law"),
	SFMT(A_LAW, "A-Law"),
	SFMT(IMA_ADPCM, "Ima-ADPCM"),
	SFMT(MPEG, "MPEG"),
	SFMT(GSM, "GSM"),
	SFMT(SPECIAL, "Special"),
	END 
};

static assoc_t starts[] = { START(EXPLICIT), START(DATA), END };
static assoc_t readys[] = { READY(FRAGMENT), READY(ASAP), END };
static assoc_t xfers[] = { XFER(INTERLEAVED), XFER(NONINTERLEAVED), END };
static assoc_t mmaps[] = { MMAP(INTERLEAVED), MMAP(NONINTERLEAVED), END };
static assoc_t onoff[] = { {0, "OFF", NULL}, {1, "ON", NULL}, {-1, "ON", NULL}, END };

int snd_pcm_dump_setup(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_setup_t *setup;
	assert(pcm);
	assert(fp);
	assert(pcm->valid_setup);
	setup = &pcm->setup;
        fprintf(fp, "stream     : %s\n", assoc(pcm->stream, streams));
	fprintf(fp, "format     : %s\n", assoc(setup->format.sfmt, fmts));
	fprintf(fp, "channels   : %d\n", setup->format.channels);
	fprintf(fp, "rate       : %d (%d/%d=%g)\n", setup->format.rate, setup->rate_master, setup->rate_divisor, (double) setup->rate_master / setup->rate_divisor);
	// digital
	fprintf(fp, "start_mode : %s\n", assoc(setup->start_mode, starts));
	fprintf(fp, "ready_mode : %s\n", assoc(setup->ready_mode, readys));
	fprintf(fp, "avail_min  : %ld\n", (long)setup->avail_min);
	fprintf(fp, "xfer_mode  : %s\n", assoc(setup->xfer_mode, xfers));
	fprintf(fp, "xfer_min   : %ld\n", (long)setup->xfer_min);
	fprintf(fp, "xfer_align : %ld\n", (long)setup->xfer_align);
	fprintf(fp, "xrun_mode  : %s\n", assoc(setup->xrun_mode, xruns));
	fprintf(fp, "mmap_shape : %s\n", assoc(setup->mmap_shape, mmaps));
	fprintf(fp, "buffer_size: %ld\n", (long)setup->buffer_size);
	fprintf(fp, "frag_size  : %ld\n", (long)setup->frag_size);
	fprintf(fp, "boundary   : %ld\n", (long)setup->boundary);
	fprintf(fp, "time       : %s\n", assoc(setup->time, onoff));
	fprintf(fp, "frags      : %ld\n", (long)setup->frags);
	fprintf(fp, "msbits     : %d\n", setup->msbits);
	return 0;
}

int snd_pcm_dump_status(snd_pcm_status_t *status, FILE *fp)
{
	assert(status);
	fprintf(fp, "state       : %s\n", assoc(status->state, states));
	fprintf(fp, "trigger_time: %ld.%06ld\n",
		status->trigger_time.tv_sec, status->trigger_time.tv_usec);
	fprintf(fp, "tstamp      : %ld.%06ld\n",
		status->tstamp.tv_sec, status->tstamp.tv_usec);
	fprintf(fp, "delay       : %ld\n", (long)status->delay);
	fprintf(fp, "avail       : %ld\n", (long)status->avail);
	fprintf(fp, "avail_max   : %ld\n", (long)status->avail_max);
	return 0;
}

int snd_pcm_dump(snd_pcm_t *pcm, FILE *fp)
{
	assert(pcm);
	assert(fp);
	pcm->ops->dump(pcm->op_arg, fp);
	return 0;
}

const char *snd_pcm_format_name(int format)
{
	assoc_t *a = assoc_value(format, fmts);
	if (a)
		return a->name;
	return 0;
}

const char *snd_pcm_format_description(int format)
{
	assoc_t *a = assoc_value(format, fmts);
	if (a)
		return a->desc;
	return "Unknown";
}

int snd_pcm_format_value(const char* name)
{
	assoc_t *a = assoc_name(name, fmts);
	if (a)
		return a->value;
	return -1;
}

ssize_t snd_pcm_bytes_to_frames(snd_pcm_t *pcm, ssize_t bytes)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return bytes * 8 / pcm->bits_per_frame;
}

ssize_t snd_pcm_frames_to_bytes(snd_pcm_t *pcm, ssize_t frames)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return frames * pcm->bits_per_frame / 8;
}

ssize_t snd_pcm_bytes_to_samples(snd_pcm_t *pcm, ssize_t bytes)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return bytes * 8 / pcm->bits_per_sample;
}

ssize_t snd_pcm_samples_to_bytes(snd_pcm_t *pcm, ssize_t samples)
{
	assert(pcm);
	assert(pcm->valid_setup);
	return samples * pcm->bits_per_sample / 8;
}

int snd_pcm_open(snd_pcm_t **pcmp, char *name, 
		 int stream, int mode)
{
	char *str;
	int err;
	snd_config_t *pcm_conf, *conf, *type_conf;
	snd_config_iterator_t i;
	char *lib = NULL, *open = NULL;
	int (*open_func)(snd_pcm_t **pcmp, char *name, snd_config_t *conf, 
			 int stream, int mode);
	void *h;
	assert(pcmp && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	err = snd_config_searchv(snd_config, &pcm_conf, "pcm", name, 0);
	if (err < 0)
		return err;
	if (snd_config_type(pcm_conf) != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;
	err = snd_config_search(pcm_conf, "stream", &conf);
	if (err >= 0) {
		err = snd_config_string_get(conf, &str);
		if (err < 0)
			return err;
		if (strcmp(str, "playback") == 0) {
			if (stream != SND_PCM_STREAM_PLAYBACK)
				return -EINVAL;
		} else if (strcmp(str, "capture") == 0) {
			if (stream != SND_PCM_STREAM_CAPTURE)
				return -EINVAL;
		} else
			return -EINVAL;
	}
	err = snd_config_search(pcm_conf, "type", &conf);
	if (err < 0)
		return err;
	err = snd_config_string_get(conf, &str);
	if (err < 0)
		return err;
	err = snd_config_searchv(snd_config, &type_conf, "pcmtype", str, 0);
	if (err < 0)
		return err;
	snd_config_foreach(i, type_conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "lib") == 0) {
			err = snd_config_string_get(n, &lib);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "open") == 0) {
			err = snd_config_string_get(n, &open);
			if (err < 0)
				return -EINVAL;
			continue;
			return -EINVAL;
		}
	}
	if (!open)
		return -EINVAL;
	if (!lib)
		lib = "libasound.so";
	h = dlopen(lib, RTLD_NOW);
	if (!h)
		return -ENOENT;
	open_func = dlsym(h, open);
	dlclose(h);
	if (!open_func)
		return -ENXIO;
	return open_func(pcmp, name, pcm_conf, stream, mode);
}

void snd_pcm_areas_from_buf(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, 
			    void *buf)
{
	unsigned int channel;
	unsigned int channels = pcm->setup.format.channels;
	for (channel = 0; channel < channels; ++channel, ++areas) {
		areas->addr = buf;
		areas->first = channel * pcm->bits_per_sample;
		areas->step = pcm->bits_per_frame;
	}
}

void snd_pcm_areas_from_bufs(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, 
			     void **bufs)
{
	unsigned int channel;
	unsigned int channels = pcm->setup.format.channels;
	for (channel = 0; channel < channels; ++channel, ++areas, ++bufs) {
		areas->addr = *bufs;
		areas->first = 0;
		areas->step = pcm->bits_per_sample;
	}
}

int snd_pcm_wait(snd_pcm_t *pcm, int timeout)
{
	struct pollfd pfd;
	int err;
#if 0
	size_t bavail, aavail;
	struct timeval before, after, diff;
	bavail = snd_pcm_avail_update(pcm);
	gettimeofday(&before, 0);
#endif
	pfd.fd = snd_pcm_poll_descriptor(pcm);
	pfd.events = pcm->stream == SND_PCM_STREAM_PLAYBACK ? POLLOUT : POLLIN;
	err = poll(&pfd, 1, timeout);
	if (err < 0)
		return err;
#if 0
	aavail = snd_pcm_avail_update(pcm);
	gettimeofday(&after, 0);
	timersub(&after, &before, &diff);
	fprintf(stderr, "%s %ld.%06ld: get=%d (%d-%d)\n", pcm->stream == SND_PCM_STREAM_PLAYBACK ? "playback" : "capture", diff.tv_sec, diff.tv_usec, aavail - bavail, aavail, bavail);
#endif
	return 0;
}

ssize_t snd_pcm_avail_update(snd_pcm_t *pcm)
{
	return pcm->fast_ops->avail_update(pcm->fast_op_arg);
}

ssize_t snd_pcm_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	assert(size > 0);
	assert(size <= snd_pcm_mmap_avail(pcm));
	return pcm->fast_ops->mmap_forward(pcm->fast_op_arg, size);
}

int snd_pcm_area_silence(snd_pcm_channel_area_t *dst_area, size_t dst_offset,
			 size_t samples, int format)
{
	/* FIXME: sub byte resolution and odd dst_offset */
	char *dst;
	unsigned int dst_step;
	int width;
	u_int64_t silence;
	if (!dst_area->addr)
		return 0;
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	width = snd_pcm_format_physical_width(format);
	silence = snd_pcm_format_silence_64(format);
	if (dst_area->step == (unsigned int) width) {
		size_t dwords = samples * width / 64;
		samples -= dwords * 64 / width;
		while (dwords-- > 0)
			*((u_int64_t*)dst)++ = silence;
		if (samples == 0)
			return 0;
	}
	dst_step = dst_area->step / 8;
	switch (width) {
	case 4: {
		u_int8_t s0 = silence & 0xf0;
		u_int8_t s1 = silence & 0x0f;
		int dstbit = dst_area->first % 8;
		int dstbit_step = dst_area->step % 8;
		while (samples-- > 0) {
			if (dstbit) {
				*dst &= 0xf0;
				*dst |= s1;
			} else {
				*dst &= 0x0f;
				*dst |= s0;
			}
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8: {
		u_int8_t sil = silence;
		while (samples-- > 0) {
			*dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 16: {
		u_int16_t sil = silence;
		while (samples-- > 0) {
			*(u_int16_t*)dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 32: {
		u_int32_t sil = silence;
		while (samples-- > 0) {
			*(u_int32_t*)dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 64: {
		while (samples-- > 0) {
			*(u_int64_t*)dst = silence;
			dst += dst_step;
		}
		break;
	}
	default:
		assert(0);
	}
	return 0;
}

int snd_pcm_areas_silence(snd_pcm_channel_area_t *dst_areas, size_t dst_offset,
			  size_t channels, size_t frames, int format)
{
	int width = snd_pcm_format_physical_width(format);
	while (channels > 0) {
		void *addr = dst_areas->addr;
		unsigned int step = dst_areas->step;
		snd_pcm_channel_area_t *begin = dst_areas;
		int channels1 = channels;
		unsigned int chns = 0;
		int err;
		while (1) {
			channels1--;
			chns++;
			dst_areas++;
			if (channels1 == 0 ||
			    dst_areas->addr != addr ||
			    dst_areas->step != step ||
			    dst_areas->first != dst_areas[-1].first + width)
				break;
		}
		if (chns > 1 && chns * width == step) {
			/* Collapse the areas */
			snd_pcm_channel_area_t d;
			d.addr = begin->addr;
			d.first = begin->first;
			d.step = width;
			err = snd_pcm_area_silence(&d, dst_offset * chns, frames * chns, format);
			channels -= chns;
		} else {
			err = snd_pcm_area_silence(begin, dst_offset, frames, format);
			dst_areas = begin + 1;
			channels--;
		}
		if (err < 0)
			return err;
	}
	return 0;
}


int snd_pcm_area_copy(snd_pcm_channel_area_t *src_area, size_t src_offset,
		      snd_pcm_channel_area_t *dst_area, size_t dst_offset,
		      size_t samples, int format)
{
	/* FIXME: sub byte resolution and odd dst_offset */
	char *src, *dst;
	int width;
	int src_step, dst_step;
	if (!src_area->addr)
		return snd_pcm_area_silence(dst_area, dst_offset, samples, format);
	src = snd_pcm_channel_area_addr(src_area, src_offset);
	if (!dst_area->addr)
		return 0;
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	width = snd_pcm_format_physical_width(format);
	if (src_area->step == (unsigned int) width &&
	    dst_area->step == (unsigned int) width) {
		size_t bytes = samples * width / 8;
		samples -= bytes * 8 / width;
		memcpy(dst, src, bytes);
		if (samples == 0)
			return 0;
	}
	src_step = src_area->step / 8;
	dst_step = dst_area->step / 8;
	switch (width) {
	case 4: {
		int srcbit = src_area->first % 8;
		int srcbit_step = src_area->step % 8;
		int dstbit = dst_area->first % 8;
		int dstbit_step = dst_area->step % 8;
		while (samples-- > 0) {
			unsigned char srcval;
			if (srcbit)
				srcval = *src & 0x0f;
			else
				srcval = *src & 0xf0;
			if (dstbit)
				*dst &= 0xf0;
			else
				*dst &= 0x0f;
			*dst |= srcval;
			src += src_step;
			srcbit += srcbit_step;
			if (srcbit == 8) {
				src++;
				srcbit = 0;
			}
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8: {
		while (samples-- > 0) {
			*dst = *src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 16: {
		while (samples-- > 0) {
			*(u_int16_t*)dst = *(u_int16_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 32: {
		while (samples-- > 0) {
			*(u_int32_t*)dst = *(u_int32_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 64: {
		while (samples-- > 0) {
			*(u_int64_t*)dst = *(u_int64_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	default:
		assert(0);
	}
	return 0;
}

int snd_pcm_areas_copy(snd_pcm_channel_area_t *src_areas, size_t src_offset,
		       snd_pcm_channel_area_t *dst_areas, size_t dst_offset,
		       size_t channels, size_t frames, int format)
{
	int width = snd_pcm_format_physical_width(format);
	while (channels > 0) {
		unsigned int step = src_areas->step;
		void *src_addr = src_areas->addr;
		snd_pcm_channel_area_t *src_start = src_areas;
		void *dst_addr = dst_areas->addr;
		snd_pcm_channel_area_t *dst_start = dst_areas;
		int channels1 = channels;
		unsigned int chns = 0;
		while (dst_areas->step == step) {
			channels1--;
			chns++;
			src_areas++;
			dst_areas++;
			if (channels1 == 0 ||
			    src_areas->step != step ||
			    src_areas->addr != src_addr ||
			    dst_areas->addr != dst_addr ||
			    src_areas->first != src_areas[-1].first + width ||
			    dst_areas->first != dst_areas[-1].first + width)
				break;
		}
		if (chns > 1 && chns * width == step) {
			/* Collapse the areas */
			snd_pcm_channel_area_t s, d;
			s.addr = src_start->addr;
			s.first = src_start->first;
			s.step = width;
			d.addr = dst_start->addr;
			d.first = dst_start->first;
			d.step = width;
			snd_pcm_area_copy(&s, src_offset * chns, &d, dst_offset * chns, frames * chns, format);
			channels -= chns;
		} else {
			snd_pcm_area_copy(src_start, src_offset, dst_start, dst_offset, frames, format);
			src_areas = src_start + 1;
			dst_areas = dst_start + 1;
			channels--;
		}
	}
	return 0;
}

ssize_t snd_pcm_read_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas,
			   size_t offset, size_t size,
			   snd_pcm_xfer_areas_func_t func)
{
	size_t xfer = 0;
	ssize_t err = 0;
	int state = snd_pcm_state(pcm);
	assert(size > 0);
	assert(state >= SND_PCM_STATE_PREPARED);
	if (state == SND_PCM_STATE_PREPARED &&
	    pcm->setup.start_mode != SND_PCM_START_EXPLICIT) {
		err = snd_pcm_start(pcm);
		if (err < 0)
			return err;
		state = SND_PCM_STATE_RUNNING;
	}
	while (xfer < size) {
		ssize_t avail;
		size_t frames;
	again:
		avail = snd_pcm_avail_update(pcm);
		if (avail < 0) {
			err = avail;
			break;
		}
		if ((size_t)avail < pcm->setup.avail_min) {
			if (state != SND_PCM_STATE_RUNNING) {
				err = -EPIPE;
				break;
			}
			if (pcm->mode & SND_PCM_NONBLOCK) {
				err = -EAGAIN;
				break;
			}
			err = snd_pcm_wait(pcm, -1);
			if (err < 0)
				break;
			state = snd_pcm_state(pcm);
			goto again;
		}
		frames = size - xfer;
		if (frames > (size_t)avail)
			frames = avail;
		err = func(pcm, areas, offset, frames, 0);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		xfer += err;
		offset += err;
	}
	if (xfer > 0)
		return xfer;
	return err;
}

ssize_t snd_pcm_write_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas,
			    size_t offset, size_t size,
			    snd_pcm_xfer_areas_func_t func)
{
	size_t xfer = 0;
	ssize_t err = 0;
	int state = snd_pcm_state(pcm);
	assert(size > 0);
	assert(state >= SND_PCM_STATE_PREPARED);
	while (xfer < size) {
		ssize_t avail;
		size_t frames;
	again:
		if (state == SND_PCM_STATE_XRUN) {
			err = -EPIPE;
			break;
		}
		avail = snd_pcm_avail_update(pcm);
		if (avail < 0) {
			err = avail;
			break;
		}
		if ((size_t)avail < pcm->setup.avail_min) {
			if (state != SND_PCM_STATE_RUNNING) {
				err = -EPIPE;
				break;
			}
			if (pcm->mode & SND_PCM_NONBLOCK) {
				err = -EAGAIN;
				break;
			}
			err = snd_pcm_wait(pcm, -1);
			if (err < 0)
				break;
			state = snd_pcm_state(pcm);
			goto again;
		}
		frames = size - xfer;
		if (frames > (size_t)avail)
			frames = avail;
		err = func(pcm, areas, offset, frames, 0);
		if (err < 0)
			break;
		assert((size_t)err == frames);
		xfer += err;
		offset += err;
		if (state == SND_PCM_STATE_PREPARED &&
		    pcm->setup.start_mode != SND_PCM_START_EXPLICIT) {
			err = snd_pcm_start(pcm);
			if (err < 0)
				break;
		}
	}
	if (xfer > 0)
		return xfer;
	return err;
}

