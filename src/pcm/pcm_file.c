/**
 * \file pcm/pcm_file.c
 * \ingroup PCM_Plugins
 * \brief PCM File Plugin Interface
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2000-2001
 */
/*
 *  PCM - File plugin
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
  
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_file = "";
#endif

#ifndef DOC_HIDDEN

typedef enum _snd_pcm_file_format {
	SND_PCM_FILE_FORMAT_RAW
} snd_pcm_file_format_t;

typedef struct {
	snd_pcm_t *slave;
	int close_slave;
	char *fname;
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

#endif /* DOC_HIDDEN */

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
				    snd_pcm_uframes_t offset,
				    snd_pcm_uframes_t frames)
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

static int snd_pcm_file_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_poll_descriptors_revents(file->slave, pfds, nfds, revents);
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

static int snd_pcm_file_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_hwsync(file->slave);
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
	snd_pcm_sframes_t err;
	snd_pcm_uframes_t n;
	
	n = snd_pcm_frames_to_bytes(pcm, frames);
	if (n > file->wbuf_used_bytes)
		frames = snd_pcm_bytes_to_frames(pcm, file->wbuf_used_bytes);
	err = snd_pcm_rewind(file->slave, frames);
	if (err > 0) {
		n = snd_pcm_frames_to_bytes(pcm, err);
		file->wbuf_used_bytes -= n;
	}
	return err;
}

static snd_pcm_sframes_t snd_pcm_file_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_sframes_t err;
	snd_pcm_uframes_t n;
	
	n = snd_pcm_frames_to_bytes(pcm, frames);
	if (file->wbuf_used_bytes + n > file->wbuf_size_bytes)
		frames = snd_pcm_bytes_to_frames(pcm, file->wbuf_size_bytes - file->wbuf_used_bytes);
	err = INTERNAL(snd_pcm_forward)(file->slave, frames);
	if (err > 0) {
		snd_pcm_uframes_t n = snd_pcm_frames_to_bytes(pcm, err);
		file->wbuf_used_bytes += n;
	}
	return err;
}

static int snd_pcm_file_resume(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_resume(file->slave);
}

static int snd_pcm_file_poll_ask(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	if (file->slave->fast_ops->poll_ask)
		return file->slave->fast_ops->poll_ask(file->slave->fast_op_arg);
	return 0;
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

static snd_pcm_sframes_t snd_pcm_file_mmap_commit(snd_pcm_t *pcm,
					          snd_pcm_uframes_t offset,
						  snd_pcm_uframes_t size)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_uframes_t ofs;
	snd_pcm_uframes_t siz = size;
	const snd_pcm_channel_area_t *areas;
	snd_pcm_sframes_t result;

	snd_pcm_mmap_begin(file->slave, &areas, &ofs, &siz);
	assert(ofs == offset && siz == size);
	result = snd_pcm_mmap_commit(file->slave, ofs, siz);
	if (result > 0)
		snd_pcm_file_add_frames(pcm, areas, ofs, result);
	return result;
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

static int snd_pcm_file_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_file_t *file = pcm->private_data;
	if (file->wbuf) {
		free(file->wbuf);
		if (file->wbuf_areas)
			free(file->wbuf_areas);
		file->wbuf = 0;
		file->wbuf_areas = 0;
	}
	return snd_pcm_hw_free(file->slave);
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
	if (file->wbuf == NULL) {
		snd_pcm_file_hw_free(pcm);
		return -ENOMEM;
	}
	file->wbuf_areas = malloc(sizeof(*file->wbuf_areas) * slave->channels);
	if (file->wbuf_areas == NULL) {
		snd_pcm_file_hw_free(pcm);
		return -ENOMEM;
	}
	file->appl_ptr = file->file_ptr_bytes = 0;
	for (channel = 0; channel < slave->channels; ++channel) {
		snd_pcm_channel_area_t *a = &file->wbuf_areas[channel];
		a->addr = file->wbuf;
		a->first = slave->sample_bits * channel;
		a->step = slave->frame_bits;
	}
	return 0;
}

static int snd_pcm_file_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_file_t *file = pcm->private_data;
	return snd_pcm_sw_params(file->slave, params);
}

static int snd_pcm_file_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	snd_pcm_file_t *file = pcm->private_data;
	snd_pcm_t *slave = file->slave;
	pcm->running_areas = slave->running_areas;
	pcm->stopped_areas = slave->stopped_areas;
	pcm->mmap_channels = slave->mmap_channels;
	pcm->mmap_shadow = 1;
	return 0;
}

static int snd_pcm_file_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	pcm->mmap_channels = NULL;
	pcm->running_areas = NULL;
	pcm->stopped_areas = NULL;
	pcm->mmap_shadow = 0;
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

static snd_pcm_ops_t snd_pcm_file_ops = {
	.close = snd_pcm_file_close,
	.info = snd_pcm_file_info,
	.hw_refine = snd_pcm_file_hw_refine,
	.hw_params = snd_pcm_file_hw_params,
	.hw_free = snd_pcm_file_hw_free,
	.sw_params = snd_pcm_file_sw_params,
	.channel_info = snd_pcm_file_channel_info,
	.dump = snd_pcm_file_dump,
	.nonblock = snd_pcm_file_nonblock,
	.async = snd_pcm_file_async,
	.poll_revents = snd_pcm_file_poll_revents,
	.mmap = snd_pcm_file_mmap,
	.munmap = snd_pcm_file_munmap,
};

static snd_pcm_fast_ops_t snd_pcm_file_fast_ops = {
	.status = snd_pcm_file_status,
	.state = snd_pcm_file_state,
	.hwsync = snd_pcm_file_hwsync,
	.delay = snd_pcm_file_delay,
	.prepare = snd_pcm_file_prepare,
	.reset = snd_pcm_file_reset,
	.start = snd_pcm_file_start,
	.drop = snd_pcm_file_drop,
	.drain = snd_pcm_file_drain,
	.pause = snd_pcm_file_pause,
	.rewind = snd_pcm_file_rewind,
	.forward = snd_pcm_file_forward,
	.resume = snd_pcm_file_resume,
	.poll_ask = snd_pcm_file_poll_ask,
	.writei = snd_pcm_file_writei,
	.writen = snd_pcm_file_writen,
	.readi = snd_pcm_file_readi,
	.readn = snd_pcm_file_readn,
	.avail_update = snd_pcm_file_avail_update,
	.mmap_commit = snd_pcm_file_mmap_commit,
};

/**
 * \brief Creates a new File PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param fname Filename (or NULL if file descriptor is available)
 * \param fd File descriptor
 * \param fmt File format ("raw" is supported only)
 * \param slave Slave PCM handle
 * \param close_slave When set, the slave PCM handle is closed with copy PCM
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_file_open(snd_pcm_t **pcmp, const char *name,
		      const char *fname, int fd, const char *fmt,
		      snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_file_t *file;
	snd_pcm_file_format_t format;
	int err;
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
	
	if (fname)
		file->fname = strdup(fname);
	file->fd = fd;
	file->format = format;
	file->slave = slave;
	file->close_slave = close_slave;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_FILE, name, slave->stream, slave->mode);
	if (err < 0) {
		if (fname)
			free(file->fname);
		free(file);
		return err;
	}
	pcm->ops = &snd_pcm_file_ops;
	pcm->fast_ops = &snd_pcm_file_fast_ops;
	pcm->private_data = file;
	pcm->poll_fd = slave->poll_fd;
	pcm->poll_events = slave->poll_events;
	pcm->mmap_shadow = 1;
	snd_pcm_link_hw_ptr(pcm, slave);
	snd_pcm_link_appl_ptr(pcm, slave);
	*pcmp = pcm;

	return 0;
}

/*! \page pcm_plugins

\section pcm_plugins_file Plugin: File

This plugin stores contents of a PCM stream to file.

\code
pcm.name {
        type file               # File PCM
        slave STR               # Slave name
        # or
        slave {                 # Slave definition
                pcm STR         # Slave PCM name
                # or
                pcm { }         # Slave PCM definition
        }
	file STR		# Filename
	or
	file INT		# File descriptor number
	[format STR]		# File format (only "raw" at the moment)
}
\endcode

\subsection pcm_plugins_file_funcref Function reference

<UL>
  <LI>snd_pcm_file_open()
  <LI>_snd_pcm_file_open()
</UL>

*/

/**
 * \brief Creates a new File PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with File PCM description
 * \param stream Stream type
 * \param mode Stream mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_file_open(snd_pcm_t **pcmp, const char *name,
		       snd_config_t *root, snd_config_t *conf, 
		       snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	int err;
	snd_pcm_t *spcm;
	snd_config_t *slave = NULL, *sconf;
	const char *fname = NULL;
	const char *format = NULL;
	long fd = -1;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "slave") == 0) {
			slave = n;
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
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}
	err = snd_pcm_slave_conf(root, slave, &sconf, 0);
	if (err < 0)
		return err;
	if (!fname && fd < 0) {
		snd_config_delete(sconf);
		SNDERR("file is not defined");
		return -EINVAL;
	}
	err = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
	snd_config_delete(sconf);
	if (err < 0)
		return err;
	err = snd_pcm_file_open(pcmp, name, fname, fd, format, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_file_open, SND_PCM_DLSYM_VERSION);
#endif
