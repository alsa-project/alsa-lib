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
#include <sys/uio.h>
#include "pcm_local.h"
#include "list.h"

snd_pcm_type_t snd_pcm_type(snd_pcm_t *handle)
{
	assert(handle);
	return handle->type;
}

snd_pcm_type_t snd_pcm(snd_pcm_t *handle)
{
	assert(handle);
	return handle->stream;
}

int snd_pcm_close(snd_pcm_t *handle)
{
	int ret = 0;
	int err;
	assert(handle);
	if (handle->mmap_status) {
		if ((err = snd_pcm_munmap_status(handle)) < 0)
			ret = err;
	}
	if (handle->mmap_control) {
		if ((err = snd_pcm_munmap_control(handle)) < 0)
			ret = err;
	}
	if (handle->mmap_data) {
		if ((err = snd_pcm_munmap_data(handle)) < 0)
			ret = err;
	}
	if ((err = handle->ops->close(handle->op_arg)) < 0)
		ret = err;
	handle->valid_setup = 0;
	free(handle);
	return ret;
}	

int snd_pcm_nonblock(snd_pcm_t *handle, int nonblock)
{
	int err;
	assert(handle);
	if ((err = handle->fast_ops->nonblock(handle->fast_op_arg, nonblock)) < 0)
		return err;
	if (nonblock)
		handle->mode |= SND_PCM_NONBLOCK;
	else
		handle->mode &= ~SND_PCM_NONBLOCK;
	return 0;
}

int snd_pcm_info(snd_pcm_t *handle, snd_pcm_info_t *info)
{
	assert(handle && info);
	return handle->ops->info(handle->op_arg, info);
}

int snd_pcm_params_info(snd_pcm_t *handle, snd_pcm_params_info_t *info)
{
	assert(handle && info);
	return handle->ops->params_info(handle->op_arg, info);
}

int snd_pcm_setup(snd_pcm_t *handle, snd_pcm_setup_t *setup)
{
	int err;
	assert(handle && setup);
	if (handle->valid_setup) {
		*setup = handle->setup;
		return 0;
	}
	if ((err = handle->ops->setup(handle->op_arg, &handle->setup)) < 0)
		return err;
	*setup = handle->setup;
	handle->bits_per_sample = snd_pcm_format_physical_width(setup->format.format);
        handle->bits_per_frame = handle->bits_per_sample * setup->format.channels;
	handle->valid_setup = 1;
	return 0;
}

int snd_pcm_channel_info(snd_pcm_t *handle, snd_pcm_channel_info_t *info)
{
	assert(handle && info);
	return handle->fast_ops->channel_info(handle->fast_op_arg, info);
}

int snd_pcm_channel_params(snd_pcm_t *handle, snd_pcm_channel_params_t *params)
{
	assert(handle && params);
	return handle->fast_ops->channel_params(handle->fast_op_arg, params);
}

int snd_pcm_channel_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup)
{
	assert(handle && setup);
	assert(handle->valid_setup);
	return handle->fast_ops->channel_setup(handle->fast_op_arg, setup);
}

int snd_pcm_params(snd_pcm_t *handle, snd_pcm_params_t *params)
{
	int err;
	snd_pcm_setup_t setup;
	assert(handle && params);
	assert(!handle->mmap_data);
	if ((err = handle->ops->params(handle->op_arg, params)) < 0)
		return err;
	handle->valid_setup = 0;
	return snd_pcm_setup(handle, &setup);
}

int snd_pcm_status(snd_pcm_t *handle, snd_pcm_status_t *status)
{
	assert(handle && status);
	return handle->fast_ops->status(handle->fast_op_arg, status);
}

int snd_pcm_state(snd_pcm_t *handle)
{
	assert(handle);
	if (handle->mmap_status)
		return handle->mmap_status->state;
	return handle->fast_ops->state(handle->fast_op_arg);
}

ssize_t snd_pcm_frame_io(snd_pcm_t *handle, int update)
{
	assert(handle);
	assert(handle->valid_setup);
	if (handle->mmap_status && !update)
		return handle->mmap_status->frame_io;
	return handle->fast_ops->frame_io(handle->fast_op_arg, update);
}

int snd_pcm_prepare(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->prepare(handle->fast_op_arg);
}

int snd_pcm_go(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->go(handle->fast_op_arg);
}

int snd_pcm_drain(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->drain(handle->fast_op_arg);
}

int snd_pcm_flush(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->flush(handle->fast_op_arg);
}

int snd_pcm_pause(snd_pcm_t *handle, int enable)
{
	assert(handle);
	return handle->fast_ops->pause(handle->fast_op_arg, enable);
}


ssize_t snd_pcm_frame_data(snd_pcm_t *handle, off_t offset)
{
	assert(handle);
	assert(handle->valid_setup);
	if (handle->mmap_control) {
		if (offset == 0)
			return handle->mmap_control->frame_data;
	}
	return handle->fast_ops->frame_data(handle->fast_op_arg, offset);
}

ssize_t snd_pcm_write(snd_pcm_t *handle, const void *buffer, size_t size)
{
	assert(handle);
	assert(size == 0 || buffer);
	assert(handle->valid_setup);
	assert(size % handle->setup.frames_align == 0);
	return handle->fast_ops->write(handle->fast_op_arg, 0, buffer, size);
}

ssize_t snd_pcm_writev(snd_pcm_t *handle, const struct iovec *vector, unsigned long count)
{
	assert(handle);
	assert(count == 0 || vector);
	assert(handle->valid_setup);
	assert(handle->setup.format.interleave || 
	       count % handle->setup.format.channels == 0);
	return handle->fast_ops->writev(handle->fast_op_arg, 0, vector, count);
}

ssize_t snd_pcm_read(snd_pcm_t *handle, void *buffer, size_t size)
{
	assert(handle);
	assert(size == 0 || buffer);
	assert(handle->valid_setup);
	assert(size % handle->setup.frames_align == 0);
	return handle->fast_ops->read(handle->fast_op_arg, 0, buffer, size);
}

ssize_t snd_pcm_readv(snd_pcm_t *handle, const struct iovec *vector, unsigned long count)
{
	assert(handle);
	assert(count == 0 || vector);
	assert(handle->valid_setup);
	return handle->fast_ops->readv(handle->fast_op_arg, 0, vector, count);
}

int snd_pcm_link(snd_pcm_t *handle1, snd_pcm_t *handle2)
{
	int fd1, fd2;
	switch (handle1->type) {
	case SND_PCM_TYPE_HW:
	case SND_PCM_TYPE_PLUG:
	case SND_PCM_TYPE_MULTI:
		fd1 = snd_pcm_file_descriptor(handle1);
		break;
	default:
		errno = -ENOSYS;
		return -1;
	}
	switch (handle2->type) {
	case SND_PCM_TYPE_HW:
	case SND_PCM_TYPE_PLUG:
	case SND_PCM_TYPE_MULTI:
		fd2 = snd_pcm_file_descriptor(handle2);
		break;
	default:
		errno = -ENOSYS;
		return -1;
	}
	if (ioctl(fd1, SND_PCM_IOCTL_LINK, fd2) < 0)
		return -errno;
	return 0;
}

int snd_pcm_unlink(snd_pcm_t *handle)
{
	int fd;
	switch (handle->type) {
	case SND_PCM_TYPE_HW:
	case SND_PCM_TYPE_PLUG:
	case SND_PCM_TYPE_MULTI:
		fd = snd_pcm_file_descriptor(handle);
		break;
	default:
		errno = -ENOSYS;
		return -1;
	}
	if (ioctl(fd, SND_PCM_IOCTL_UNLINK) < 0)
		return -errno;
	return 0;
}

int snd_pcm_file_descriptor(snd_pcm_t *handle)
{
	assert(handle);
	return handle->fast_ops->file_descriptor(handle->fast_op_arg);
}

int snd_pcm_channels_mask(snd_pcm_t *handle, bitset_t *client_vmask)
{
	assert(handle);
	assert(handle->valid_setup);
	return handle->fast_ops->channels_mask(handle->fast_op_arg, client_vmask);
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

int snd_pcm_dump_setup(snd_pcm_t *handle, FILE *fp)
{
	snd_pcm_setup_t *setup;
	assert(handle);
	assert(fp);
	assert(handle->valid_setup);
	setup = &handle->setup;
        fprintf(fp, "stream: %s\n", assoc(handle->stream, streams));
	fprintf(fp, "mode: %s\n", assoc(setup->mode, modes));
	fprintf(fp, "format: %s\n", assoc(setup->format.format, fmts));
	fprintf(fp, "channels: %d\n", setup->format.channels);
	fprintf(fp, "rate: %d (%d/%d=%g)\n", setup->format.rate, setup->rate_master, setup->rate_divisor, (double) setup->rate_master / setup->rate_divisor);
	// digital
	fprintf(fp, "start_mode: %s\n", assoc(setup->start_mode, starts));
	fprintf(fp, "xrun_mode: %s\n", assoc(setup->xrun_mode, xruns));
	fprintf(fp, "time: %s\n", assoc(setup->time, onoff));
	// ust_time
	fprintf(fp, "buffer_size: %ld\n", (long)setup->buffer_size);
	fprintf(fp, "frag_size: %ld\n", (long)setup->frag_size);
	fprintf(fp, "frags: %ld\n", (long)setup->frags);
	fprintf(fp, "frame_boundary: %ld\n", (long)setup->frame_boundary);
	fprintf(fp, "msbits_per_sample: %d\n", setup->msbits_per_sample);
	fprintf(fp, "frames_min: %ld\n", (long)setup->frames_min);
	fprintf(fp, "frames_align: %ld\n", (long)setup->frames_align);
	fprintf(fp, "frames_xrun_max: %ld\n", (long)setup->frames_xrun_max);
	fprintf(fp, "fill_mode: %s\n", assoc(setup->fill_mode, fills));
	fprintf(fp, "frames_fill_max: %ld\n", (long)setup->frames_fill_max);
	return 0;
}

int snd_pcm_dump(snd_pcm_t *handle, FILE *fp)
{
	assert(handle);
	assert(fp);
	handle->ops->dump(handle->op_arg, fp);
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

ssize_t snd_pcm_bytes_to_frames(snd_pcm_t *handle, ssize_t bytes)
{
	assert(handle);
	assert(handle->valid_setup);
	return bytes * 8 / handle->bits_per_frame;
}

ssize_t snd_pcm_frames_to_bytes(snd_pcm_t *handle, ssize_t frames)
{
	assert(handle);
	assert(handle->valid_setup);
	return frames * handle->bits_per_frame / 8;
}

ssize_t snd_pcm_bytes_to_samples(snd_pcm_t *handle, ssize_t bytes)
{
	assert(handle);
	assert(handle->valid_setup);
	return bytes * 8 / handle->bits_per_sample;
}

ssize_t snd_pcm_samples_to_bytes(snd_pcm_t *handle, ssize_t samples)
{
	assert(handle);
	assert(handle->valid_setup);
	return samples * handle->bits_per_sample / 8;
}

static int _snd_pcm_open_hw(snd_pcm_t **handlep, snd_config_t *conf, 
			    int stream, int mode)
{
	snd_config_iterator_t i;
	long card = -1, device = -1, subdevice = -1;
	char *str;
	int err;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "card") == 0) {
			err = snd_config_integer_get(n, &card);
			if (err < 0) {
				err = snd_config_string_get(n, &str);
				if (err < 0)
					return -EINVAL;
				card = snd_card_get_index(str);
				if (card < 0)
					return card;
			}
			continue;
		}
		if (strcmp(n->id, "device") == 0) {
			err = snd_config_integer_get(n, &device);
			if (err < 0)
				return err;
			continue;
		}
		if (strcmp(n->id, "subdevice") == 0) {
			err = snd_config_integer_get(n, &subdevice);
			if (err < 0)
				return err;
			continue;
		}
		return -EINVAL;
	}
	if (card < 0 || device < 0)
		return -EINVAL;
	return snd_pcm_hw_open_subdevice(handlep, card, device, subdevice, stream, mode);
}
				
static int _snd_pcm_open_plug(snd_pcm_t **handlep, snd_config_t *conf, 
			      int stream, int mode)
{
	snd_config_iterator_t i;
	char *slave = NULL;
	int err;
	snd_pcm_t *slave_handle;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "slave") == 0) {
			err = snd_config_string_get(n, &slave);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		return -EINVAL;
	}
	if (!slave)
		return -EINVAL;
	/* This is needed cause snd_config_update may destroy config */
	slave = strdup(slave);
	if (!slave)
		return  -ENOMEM;
	err = snd_pcm_open(&slave_handle, slave, stream, mode);
	free(slave);
	if (err < 0)
		return err;
	err = snd_pcm_plug_create(handlep, slave_handle, 1);
	if (err < 0)
		snd_pcm_close(slave_handle);
	return err;
}
				
static int _snd_pcm_open_multi(snd_pcm_t **handlep, snd_config_t *conf, 
			      int stream, int mode)
{
	snd_config_iterator_t i, j;
	snd_config_t *slave = NULL;
	snd_config_t *binding = NULL;
	int err;
	unsigned int idx;
	char **slaves_id = NULL;
	char **slaves_name = NULL;
	snd_pcm_t **slaves_handle = NULL;
	size_t *slaves_channels = NULL;
	unsigned int *bindings_cchannel = NULL;
	unsigned int *bindings_slave = NULL;
	unsigned int *bindings_schannel = NULL;
	size_t slaves_count = 0;
	size_t bindings_count = 0;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "slave") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_COMPOUND)
				return -EINVAL;
			slave = n;
			continue;
		}
		if (strcmp(n->id, "binding") == 0) {
			if (snd_config_type(n) != SND_CONFIG_TYPE_COMPOUND)
				return -EINVAL;
			binding = n;
			continue;
		}
		return -EINVAL;
	}
	if (!slave || !binding)
		return -EINVAL;
	snd_config_foreach(i, slave) {
		++slaves_count;
	}
	snd_config_foreach(i, binding) {
		++bindings_count;
	}
	slaves_id = calloc(slaves_count, sizeof(*slaves_id));
	slaves_name = calloc(slaves_count, sizeof(*slaves_name));
	slaves_handle = calloc(slaves_count, sizeof(*slaves_handle));
	slaves_channels = calloc(slaves_count, sizeof(*slaves_channels));
	bindings_cchannel = calloc(bindings_count, sizeof(*bindings_cchannel));
	bindings_slave = calloc(bindings_count, sizeof(*bindings_slave));
	bindings_schannel = calloc(bindings_count, sizeof(*bindings_schannel));
	idx = 0;
	snd_config_foreach(i, slave) {
		snd_config_t *m = snd_config_entry(i);
		char *pcm = NULL;
		long channels = -1;
		slaves_id[idx] = snd_config_id(m);
		snd_config_foreach(j, m) {
			snd_config_t *n = snd_config_entry(j);
			if (strcmp(n->id, "comment") == 0)
				continue;
			if (strcmp(n->id, "pcm") == 0) {
				err = snd_config_string_get(n, &pcm);
				if (err < 0)
					goto _free;
				continue;
			}
			if (strcmp(n->id, "channels") == 0) {
				err = snd_config_integer_get(n, &channels);
				if (err < 0)
					goto _free;
				continue;
			}
			err = -EINVAL;
			goto _free;
		}
		if (!pcm || channels < 0) {
			err = -EINVAL;
			goto _free;
		}
		slaves_name[idx] = strdup(pcm);
		slaves_channels[idx] = channels;
		++idx;
	}

	idx = 0;
	snd_config_foreach(i, binding) {
		snd_config_t *m = snd_config_entry(i);
		long cchannel = -1, schannel = -1;
		int slave = -1;
		long val;
		char *str;
		snd_config_foreach(j, m) {
			snd_config_t *n = snd_config_entry(j);
			if (strcmp(n->id, "comment") == 0)
				continue;
			if (strcmp(n->id, "client_channel") == 0) {
				err = snd_config_integer_get(n, &cchannel);
				if (err < 0)
					goto _free;
				continue;
			}
			if (strcmp(n->id, "slave") == 0) {
				char buf[32];
				unsigned int k;
				err = snd_config_string_get(n, &str);
				if (err < 0) {
					err = snd_config_integer_get(n, &val);
					if (err < 0)
						goto _free;
					sprintf(buf, "%ld", val);
					str = buf;
				}
				for (k = 0; k < slaves_count; ++k) {
					if (strcmp(slaves_id[k], str) == 0)
						slave = k;
				}
				continue;
			}
			if (strcmp(n->id, "slave_channel") == 0) {
				err = snd_config_integer_get(n, &schannel);
				if (err < 0)
					goto _free;
				continue;
			}
			err = -EINVAL;
			goto _free;
		}
		if (cchannel < 0 || slave < 0 || schannel < 0) {
			err = -EINVAL;
			goto _free;
		}
		if ((size_t)slave >= slaves_count) {
			err = -EINVAL;
			goto _free;
		}
		if ((unsigned int) schannel >= slaves_channels[slave]) {
			err = -EINVAL;
			goto _free;
		}
		bindings_cchannel[idx] = cchannel;
		bindings_slave[idx] = slave;
		bindings_schannel[idx] = schannel;
		++idx;
	}
	
	for (idx = 0; idx < slaves_count; ++idx) {
		err = snd_pcm_open(&slaves_handle[idx], slaves_name[idx], stream, mode);
		if (err < 0)
			goto _free;
	}
	err = snd_pcm_multi_create(handlep, slaves_count, slaves_handle,
				   slaves_channels,
				   bindings_count, bindings_cchannel,
				   bindings_slave, bindings_schannel,
				   1);
_free:
	if (err < 0) {
		for (idx = 0; idx < slaves_count; ++idx) {
			if (slaves_handle[idx])
				snd_pcm_close(slaves_handle[idx]);
			if (slaves_name[idx])
				free(slaves_name[idx]);
		}
	}
	if (slaves_name)
		free(slaves_name);
	if (slaves_handle)
		free(slaves_handle);
	if (slaves_channels)
		free(slaves_channels);
	if (bindings_cchannel)
		free(bindings_cchannel);
	if (bindings_slave)
		free(bindings_slave);
	if (bindings_schannel)
		free(bindings_schannel);
	return err;
}

static int _snd_pcm_open_client(snd_pcm_t **handlep, snd_config_t *conf, 
				int stream, int mode)
{
	snd_config_iterator_t i;
	char *socket = NULL;
	char *name = NULL;
	char *host = NULL;
	long port = -1;
	int err;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "socket") == 0) {
			err = snd_config_string_get(n, &socket);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "host") == 0) {
			err = snd_config_string_get(n, &host);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "port") == 0) {
			err = snd_config_integer_get(n, &port);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "name") == 0) {
			err = snd_config_string_get(n, &name);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		return -EINVAL;
	}
	if (!name)
		return -EINVAL;
	if (socket) {
		if (port >= 0 || host)
			return -EINVAL;
		return snd_pcm_client_create(handlep, socket, -1, SND_TRANSPORT_TYPE_SHM, name, stream, mode);
	} else  {
		if (port < 0 || !name)
			return -EINVAL;
		return snd_pcm_client_create(handlep, host, port, SND_TRANSPORT_TYPE_SHM, name, stream, mode);
	}
}
				
int snd_pcm_open(snd_pcm_t **handlep, char *name, 
		 int stream, int mode)
{
	char *str;
	int err;
	snd_config_t *pcm_conf, *conf;
	assert(handlep && name);
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
	if (strcmp(str, "hw") == 0)
		return _snd_pcm_open_hw(handlep, pcm_conf, stream, mode);
	else if (strcmp(str, "plug") == 0)
		return _snd_pcm_open_plug(handlep, pcm_conf, stream, mode);
	else if (strcmp(str, "multi") == 0)
		return _snd_pcm_open_multi(handlep, pcm_conf, stream, mode);
	else if (strcmp(str, "client") == 0)
		return _snd_pcm_open_client(handlep, pcm_conf, stream, mode);
	else
		return -EINVAL;
}
