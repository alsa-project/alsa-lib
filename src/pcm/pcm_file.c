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

typedef enum _snd_pcm_file_format {
	SND_PCM_FILE_FORMAT_RAW
} snd_pcm_file_format_t;

typedef struct {
	snd_pcm_t *slave;
	int close_slave;
	const char *fname;
	int fd;
	int format;
	snd_pcm_uframes_t appl_ptr;
	snd_pcm_uframes_t file_ptr_bytes;
	snd_pcm_uframes_t wbuf_size;
	size_t wbuf_size_bytes;
	size_t wbuf_used_bytes;
	char *wbuf;
	snd_pcm_channel_area_t *wbuf_areas;
	size_t buffer_bytes;
} snd_pcm_file_t;

static void snd_pcm_file_write_bytes(snd_pcm_t *pcm, size_t bytes)
{
	snd_pcm_file_t *file = pcm->private_data;
	assert(bytes <= file->wbuf_used_bytes);
	while (bytes > 0) {
		snd_pcm_sframes_t err;
		size_t n = bytes;
		size_t cont = file->wbuf_size_bytes - file->file_ptr_bytes;
		if (n > cont)
			n = cont;
		err = write(file->fd, file->wbuf + file->file_ptr_bytes, n);
		if (err < 0) {
			SYSERR("write failed");
			break;
		}
		bytes -= err;
		file->wbuf_used_bytes -= err;
		file->file_ptr_bytes += err;
		if (file->file_ptr_bytes == file->wbuf_size_bytes)
			file->file_ptr_bytes = 0;
		if ((snd_pcm_uframes_t)err != n)
			break;
	}
}

static void snd_pcm_file_add_frames(snd_pcm_t *pcm, 
				    const snd_pcm_channel_area_t *areas,
				    snd_pcm_uframes_t offset, snd_pcm_uframes_t frames)
{
	snd_pcm_file_t *file = pcm->private_data;
	while (frames > 0) {
		snd_pcm_uframes_t n = frames;
		snd_pcm_uframes_t cont = file->wbuf_size - file->appl_ptr;
		snd_pcm_uframes_t avail = file->wbuf_size - snd_pcm_bytes_to_frames(pcm, file->wbuf_used_bytes);
		if (n > cont)
			n = cont;
		if (n > avail)
			n = avail;
		snd_pcm_areas_copy(file->wbuf_areas, file->appl_ptr, 
				   areas, offset,
				   pcm->channels, n, pcm->format);
		frames -= n;
		file->appl_ptr += n;
		if (file->appl_ptr == file->wbuf_size)
			file->appl_ptr = 0;
		file->wbuf_used_bytes += snd_pcm_frames_to_bytes(pcm, n);
		if (file->wbuf_used_bytes > file->buffer_bytes)
			snd_pcm_file_write_bytes(pcm, file->wbuf_used_bytes - file->buffer_bytes);
		assert(file->wbuf_used_bytes < file->wbuf_size_bytes);
	}
}

static int snd_pcm_file_close(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	int err = 0;
	if (file->close_slave)
		err = snd_pcm_close(file->slave);
	if (file->fname) {
		free((void *)file->fname);
		close(file->fd);
	}
	free(file);
	return 0;
}

static int snd_pcm_file_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_nonblock(file->slave, nonblock);
}

static int snd_pcm_file_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_async(file->slave, sig, pid);
}

static int snd_pcm_file_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_info(file->slave, info);
}

static int snd_pcm_file_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_channel_info(file->slave, info);
}

static int snd_pcm_file_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_status(file->slave, status);
}

static snd_pcm_state_t snd_pcm_file_state(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_state(file->slave);
}

static int snd_pcm_file_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_delay(file->slave, delayp);
}

static int snd_pcm_file_prepare(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_prepare(file->slave);
}

static int snd_pcm_file_reset(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	int err = snd_pcm_reset(file->slave);
	if (err >= 0) {
		/* FIXME: Questionable here */
		snd_pcm_file_write_bytes(pcm, file->wbuf_used_bytes);
		assert(file->wbuf_used_bytes == 0);
	}
	return err;
}

static int snd_pcm_file_start(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_start(file->slave);
}

static int snd_pcm_file_drop(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	int err = snd_pcm_drop(file->slave);
	if (err >= 0) {
		/* FIXME: Questionable here */
		snd_pcm_file_write_bytes(pcm, file->wbuf_used_bytes);
		assert(file->wbuf_used_bytes == 0);
	}
	return err;
}

static int snd_pcm_file_drain(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	int err = snd_pcm_drain(file->slave);
	if (err >= 0) {
		snd_pcm_file_write_bytes(pcm, file->wbuf_used_bytes);
		assert(file->wbuf_used_bytes == 0);
	}
	return err;
}

static int snd_pcm_file_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_pause(file->slave, enable);
}

static snd_pcm_sframes_t snd_pcm_file_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_sframes_t err = snd_pcm_rewind(file->slave, frames);
	if (err > 0) {
		snd_pcm_uframes_t n = snd_pcm_frames_to_bytes(pcm, frames);
		snd_pcm_sframes_t ptr;
		assert(n >= file->wbuf_used_bytes);
		ptr = file->appl_ptr - err;
		if (ptr < 0)
			ptr += file->wbuf_size;
		file->wbuf_used_bytes -= n;
	}
	return err;
}

static snd_pcm_sframes_t snd_pcm_file_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_sframes_t n = snd_pcm_writei(file->slave, buffer, size);
	if (n > 0) {
		snd_pcm_areas_from_buf(pcm, areas, (void*) buffer);
		snd_pcm_file_add_frames(pcm, areas, 0, n);
	}
	return n;
}

static snd_pcm_sframes_t snd_pcm_file_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_sframes_t n = snd_pcm_writen(file->slave, bufs, size);
	if (n > 0) {
		snd_pcm_areas_from_bufs(pcm, areas, bufs);
		snd_pcm_file_add_frames(pcm, areas, 0, n);
	}
	return n;
}

static snd_pcm_sframes_t snd_pcm_file_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_sframes_t n = snd_pcm_readi(file->slave, buffer, size);
	if (n > 0) {
		snd_pcm_areas_from_buf(pcm, areas, buffer);
		snd_pcm_file_add_frames(pcm, areas, 0, n);
	}
	return n;
}

static snd_pcm_sframes_t snd_pcm_file_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_channel_area_t areas[pcm->channels];
	snd_pcm_sframes_t n = snd_pcm_writen(file->slave, bufs, size);
	if (n > 0) {
		snd_pcm_areas_from_bufs(pcm, areas, bufs);
		snd_pcm_file_add_frames(pcm, areas, 0, n);
	}
	return n;
}

static snd_pcm_sframes_t snd_pcm_file_mmap_forward(snd_pcm_t *pcm, snd_pcm_uframes_t size)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_uframes_t ofs = snd_pcm_mmap_offset(pcm);
	snd_pcm_sframes_t n = snd_pcm_mmap_forward(file->slave, size);
	snd_pcm_uframes_t xfer = 0;
	if (n <= 0)
		return n;
	while (xfer < (snd_pcm_uframes_t)n) {
		snd_pcm_uframes_t frames = n - xfer;
		snd_pcm_uframes_t cont = pcm->buffer_size - ofs;
		if (frames > cont)
			frames = cont;
		snd_pcm_file_add_frames(pcm, snd_pcm_mmap_areas(file->slave), ofs, frames);
		ofs += frames;
		if (ofs == pcm->buffer_size)
			ofs = 0;
		xfer += frames;
	}
	return n;
}

static snd_pcm_sframes_t snd_pcm_file_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_avail_update(file->slave);
}

static int snd_pcm_file_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_hw_refine(file->slave, params);
}

static int snd_pcm_file_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_file_t *file = pcm->private_data;
	unsigned int channel;
	snd_pcm_t *slave = file->slave;
	int err = _snd_pcm_hw_params(slave, params);
	if (err < 0)
		return err;
	file->buffer_bytes = snd_pcm_frames_to_bytes(slave, slave->buffer_size);
	file->wbuf_size = slave->buffer_size * 2;
	file->wbuf_size_bytes = snd_pcm_frames_to_bytes(slave, file->wbuf_size);
	assert(!file->wbuf);
	file->wbuf = malloc(file->wbuf_size_bytes);
	file->wbuf_areas = malloc(sizeof(*file->wbuf_areas) * slave->channels);
	file->appl_ptr = file->file_ptr_bytes = 0;
	for (channel = 0; channel < slave->channels; ++channel) {
		snd_pcm_channel_area_t *a = &file->wbuf_areas[channel];
		a->addr = file->wbuf;
		a->first = slave->sample_bits * channel;
		a->step = slave->frame_bits;
	}
	return 0;
}

static int snd_pcm_file_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	if (file->wbuf) {
		free(file->wbuf);
		free(file->wbuf_areas);
		file->wbuf = 0;
		file->wbuf_areas = 0;
	}
	return snd_pcm_hw_free(file->slave);
}

static int snd_pcm_file_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_sw_params(file->slave, params);
}

static int snd_pcm_file_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_file_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_file_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_file_t *file = pcm->private_data;
	if (file->fname)
		snd_output_printf(out, "File PCM (file=%s)\n", file->fname);
	else
		snd_output_printf(out, "File PCM (fd=%d)\n", file->fd);
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(file->slave, out);
}

snd_pcm_ops_t snd_pcm_file_ops = {
	close: snd_pcm_file_close,
	info: snd_pcm_file_info,
	hw_refine: snd_pcm_file_hw_refine,
	hw_params: snd_pcm_file_hw_params,
	hw_free: snd_pcm_file_hw_free,
	sw_params: snd_pcm_file_sw_params,
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
};

int snd_pcm_file_open(snd_pcm_t **pcmp, const char *name, const char *fname, int fd, const char *fmt, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_file_t *file;
	snd_pcm_file_format_t format;
	assert(pcmp);
	if (fmt == NULL ||
	    strcmp(fmt, "raw") == 0)
		format = SND_PCM_FILE_FORMAT_RAW;
	else {
		SNDERR("file format %s is unknown", fmt);
		return -EINVAL;
	}
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

	file->fname = fname;
	file->fd = fd;
	file->format = format;
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
	pcm->private_data = file;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = slave->hw_ptr;
	pcm->appl_ptr = slave->appl_ptr;
	*pcmp = pcm;

	return 0;
}

int _snd_pcm_file_open(snd_pcm_t **pcmp, const char *name,
		       snd_config_t *conf, 
		       snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	const char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	const char *fname = NULL;
	const char *format = NULL;
	long fd = -1;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "type") == 0)
			continue;
		if (strcmp(id, "sname") == 0) {
			err = snd_config_get_string(n, &sname);
			if (err < 0) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(id, "format") == 0) {
			err = snd_config_get_string(n, &format);
			if (err < 0) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(id, "file") == 0) {
			err = snd_config_get_string(n, &fname);
			if (err < 0) {
				err = snd_config_get_integer(n, &fd);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					return -EINVAL;
				}
			}
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!sname) {
		SNDERR("sname is not defined");
		return -EINVAL;
	}
	if (!fname && fd < 0) {
		SNDERR("file is not defined");
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
	free((void *) sname);
	if (err < 0)
		return err;
	err = snd_pcm_file_open(pcmp, name, fname, fd, format, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
