/*
 *  PCM - File plugin
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
  
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

enum {
	SND_PCM_FILE_FORMAT_RAW
};

typedef struct {
	snd_pcm_t *slave;
	int close_slave;
	char *fname;
	int fd;
	int format;
} snd_pcm_file_t;

static int snd_pcm_file_close(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	int err = 0;
	if (file->close_slave)
		err = snd_pcm_close(file->slave);
	if (file->fname) {
		free(file->fname);
		close(file->fd);
	}
	free(file);
	return 0;
}

static int snd_pcm_file_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_nonblock(file->slave, nonblock);
}

static int snd_pcm_file_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_async(file->slave, sig, pid);
}

static int snd_pcm_file_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_info(file->slave, info);
}

static int snd_pcm_file_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_channel_info(file->slave, info);
}

static int snd_pcm_file_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_status(file->slave, status);
}

static int snd_pcm_file_state(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_state(file->slave);
}

static int snd_pcm_file_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_delay(file->slave, delayp);
}

static int snd_pcm_file_prepare(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_prepare(file->slave);
}

static int snd_pcm_file_reset(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	/* FIXME */
	return snd_pcm_reset(file->slave);
}

static int snd_pcm_file_start(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_start(file->slave);
}

static int snd_pcm_file_drop(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_drop(file->slave);
}

static int snd_pcm_file_drain(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_drain(file->slave);
}

static int snd_pcm_file_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_pause(file->slave, enable);
}

static ssize_t snd_pcm_file_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_file_t *file = pcm->private;
	ssize_t f = snd_pcm_rewind(file->slave, frames);
	off_t err;
	if (f > 0) {
		err = lseek(file->fd, -snd_pcm_frames_to_bytes(pcm, f), SEEK_CUR);
		if (err < 0)
			return err;
	}
	return f;
}

static void snd_pcm_file_write_areas(snd_pcm_t *pcm, 
				     const snd_pcm_channel_area_t *areas,
				     size_t offset, size_t frames)
{
	snd_pcm_file_t *file = pcm->private;
	size_t bytes = snd_pcm_frames_to_bytes(pcm, frames);
	char *buf;
	size_t channels = pcm->channels;
	snd_pcm_channel_area_t buf_areas[channels];
	size_t channel;
	ssize_t r;
	switch (pcm->access) {
	case SND_PCM_ACCESS_MMAP_INTERLEAVED:
	case SND_PCM_ACCESS_RW_INTERLEAVED:
		buf = snd_pcm_channel_area_addr(areas, offset);
		break;
	default:
		buf = alloca(bytes);
		for (channel = 0; channel < channels; ++channel) {
			snd_pcm_channel_area_t *a = &buf_areas[channel];
			a->addr = buf;
			a->first = pcm->bits_per_sample * channel;
			a->step = pcm->bits_per_frame;
		}
		snd_pcm_areas_copy(areas, offset, buf_areas, 0, 
				   channels, frames, pcm->format);
	}
	r = write(file->fd, buf, bytes);
	assert(r == (ssize_t)bytes);
}

static ssize_t snd_pcm_file_writei(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	ssize_t n = snd_pcm_writei(file->slave, buffer, size);
	if (n > 0) {
		size_t bytes = snd_pcm_frames_to_bytes(pcm, n);
		ssize_t r = write(file->fd, buffer, bytes);
		assert(r == (ssize_t)bytes);
	}
	return n;
}

static ssize_t snd_pcm_file_writen(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	snd_pcm_channel_area_t areas[pcm->channels];
	ssize_t n = snd_pcm_writen(file->slave, bufs, size);
	if (n > 0) {
		snd_pcm_areas_from_bufs(pcm, areas, bufs);
		snd_pcm_file_write_areas(pcm, areas, 0, n);
	}
	return n;
}

static ssize_t snd_pcm_file_readi(snd_pcm_t *pcm, void *buffer, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	ssize_t n = snd_pcm_readi(file->slave, buffer, size);
	if (n > 0) {
		size_t bytes = snd_pcm_frames_to_bytes(pcm, n);
		ssize_t r = write(file->fd, buffer, bytes);
		assert(r == (ssize_t)bytes);
	}
	return n;
}

static ssize_t snd_pcm_file_readn(snd_pcm_t *pcm, void **bufs, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	snd_pcm_channel_area_t areas[pcm->channels];
	ssize_t n = snd_pcm_writen(file->slave, bufs, size);
	if (n > 0) {
		snd_pcm_areas_from_bufs(pcm, areas, bufs);
		snd_pcm_file_write_areas(pcm, areas, 0, n);
	}
	return n;
}

static ssize_t snd_pcm_file_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_file_t *file = pcm->private;
	size_t ofs = snd_pcm_mmap_offset(pcm);
	ssize_t n = snd_pcm_mmap_forward(file->slave, size);
	size_t xfer = 0;
	if (n <= 0)
		return n;
	while (xfer < (size_t)n) {
		size_t frames = size - xfer;
		size_t cont = pcm->buffer_size - ofs;
		if (cont < frames)
			frames = cont;
		snd_pcm_file_write_areas(pcm, snd_pcm_mmap_areas(file->slave), ofs, frames);
		ofs += frames;
		if (ofs == pcm->buffer_size)
			ofs = 0;
		xfer += frames;
	}
	return n;
}

static ssize_t snd_pcm_file_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_avail_update(file->slave);
}

static int snd_pcm_file_set_avail_min(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_set_avail_min(file->slave, frames);
}

static int snd_pcm_file_hw_info(snd_pcm_t *pcm, snd_pcm_hw_info_t * info)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_hw_info(file->slave, info);
}

static int snd_pcm_file_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_hw_params(file->slave, params);
}

static int snd_pcm_file_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_sw_params(file->slave, params);
}

static int snd_pcm_file_dig_info(snd_pcm_t *pcm, snd_pcm_dig_info_t * info)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_dig_info(file->slave, info);
}

static int snd_pcm_file_dig_params(snd_pcm_t *pcm, snd_pcm_dig_params_t * params)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_dig_params(file->slave, params);
}

static int snd_pcm_file_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_mmap(file->slave);
}

static int snd_pcm_file_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	snd_pcm_file_t *file = pcm->private;
	return snd_pcm_munmap(file->slave);
}

static void snd_pcm_file_dump(snd_pcm_t *pcm, FILE *fp)
{
	snd_pcm_file_t *file = pcm->private;
	if (file->fname)
		fprintf(fp, "File PCM (file=%s)\n", file->fname);
	else
		fprintf(fp, "File PCM (fd=%d)\n", file->fd);
	if (pcm->setup) {
		fprintf(fp, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
	fprintf(fp, "Slave: ");
	snd_pcm_dump(file->slave, fp);
}

snd_pcm_ops_t snd_pcm_file_ops = {
	close: snd_pcm_file_close,
	info: snd_pcm_file_info,
	hw_info: snd_pcm_file_hw_info,
	hw_params: snd_pcm_file_hw_params,
	sw_params: snd_pcm_file_sw_params,
	dig_info: snd_pcm_file_dig_info,
	dig_params: snd_pcm_file_dig_params,
	channel_info: snd_pcm_file_channel_info,
	dump: snd_pcm_file_dump,
	nonblock: snd_pcm_file_nonblock,
	async: snd_pcm_file_async,
	mmap: snd_pcm_file_mmap,
	munmap: snd_pcm_file_munmap,
};

snd_pcm_fast_ops_t snd_pcm_file_fast_ops = {
	status: snd_pcm_file_status,
	state: snd_pcm_file_state,
	delay: snd_pcm_file_delay,
	prepare: snd_pcm_file_prepare,
	reset: snd_pcm_file_reset,
	start: snd_pcm_file_start,
	drop: snd_pcm_file_drop,
	drain: snd_pcm_file_drain,
	pause: snd_pcm_file_pause,
	rewind: snd_pcm_file_rewind,
	writei: snd_pcm_file_writei,
	writen: snd_pcm_file_writen,
	readi: snd_pcm_file_readi,
	readn: snd_pcm_file_readn,
	avail_update: snd_pcm_file_avail_update,
	mmap_forward: snd_pcm_file_mmap_forward,
	set_avail_min: snd_pcm_file_set_avail_min,
};

int snd_pcm_file_open(snd_pcm_t **pcmp, char *name, char *fname, int fd, char *fmt, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_file_t *file;
	assert(pcmp);
	if (fname) {
		fd = open(fname, O_WRONLY|O_CREAT, 0666);
		if (fd < 0) {
			SYSERR("open %s failed", fname);
			return -errno;
		}
	}
	file = calloc(1, sizeof(snd_pcm_file_t));
	if (!file) {
		if (fname)
			close(fd);
		return -ENOMEM;
	}
	if (fmt == NULL ||
	    strcmp(fmt, "raw") == 0)
		file->format = SND_PCM_FILE_FORMAT_RAW;
	else
		ERR("file format %s is unknown", fmt);

	file->fname = fname;
	file->fd = fd;
	file->slave = slave;
	file->close_slave = close_slave;

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(file);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_FILE;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_file_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_file_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = file;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = slave->hw_ptr;
	pcm->appl_ptr = slave->appl_ptr;
	*pcmp = pcm;

	return 0;
}

int _snd_pcm_file_open(snd_pcm_t **pcmp, char *name,
		       snd_config_t *conf, 
		       int stream, int mode)
{
	snd_config_iterator_t i;
	char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	char *fname = NULL;
	char *format = NULL;
	long fd = -1;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "sname") == 0) {
			err = snd_config_string_get(n, &sname);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "format") == 0) {
			err = snd_config_string_get(n, &format);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "file") == 0) {
			err = snd_config_string_get(n, &fname);
			if (err < 0) {
				err = snd_config_integer_get(n, &fd);
				if (err < 0) {
					ERR("Invalid type for %s", n->id);
					return -EINVAL;
				}
			}
			continue;
		}
		ERR("Unknown field %s", n->id);
		return -EINVAL;
	}
	if (!sname) {
		ERR("sname is not defined");
		return -EINVAL;
	}
	if (!fname && fd < 0) {
		ERR("file is not defined");
		return -EINVAL;
	}
	if (fname) {
		fname = strdup(fname);
		if (!fname)
			return -ENOMEM;
	}
	/* This is needed cause snd_config_update may destroy config */
	sname = strdup(sname);
	if (!sname)
		return  -ENOMEM;
	err = snd_pcm_open(&spcm, sname, stream, mode);
	free(sname);
	if (err < 0)
		return err;
	err = snd_pcm_file_open(pcmp, name, fname, fd, format, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
