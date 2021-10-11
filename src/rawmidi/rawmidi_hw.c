/*
 *  RawMIDI - Hardware
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@perex.cz>
 *                        Abramo Bagnara <abramo@alsa-project.org>
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "../control/control_local.h"
#include "rawmidi_local.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_rawmidi_hw = "";
#endif

#define SNDRV_FILE_RAWMIDI		ALSA_DEVICE_DIRECTORY "midiC%iD%i"
#define SNDRV_RAWMIDI_VERSION_MAX	SNDRV_PROTOCOL_VERSION(2, 0, 0)

#ifndef DOC_HIDDEN
typedef struct {
	int open;
	int fd;
	int card, device, subdevice;
	unsigned char *buf;
	size_t buf_size;	/* total buffer size in bytes */
	size_t buf_fill;	/* filled buffer size in bytes */
	size_t buf_pos;		/* offset to frame in the read buffer (bytes) */
	size_t buf_fpos;	/* offset to the frame data array (bytes 0-16) */
} snd_rawmidi_hw_t;
#endif

static void buf_reset(snd_rawmidi_hw_t *hw)
{
	hw->buf_fill = 0;
	hw->buf_pos = 0;
	hw->buf_fpos = 0;
}

static int snd_rawmidi_hw_close(snd_rawmidi_t *rmidi)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	int err = 0;

	hw->open--;
	if (hw->open)
		return 0;
	if (close(hw->fd)) {
		err = -errno;
		SYSERR("close failed\n");
	}
	free(hw->buf);
	free(hw);
	return err;
}

static int snd_rawmidi_hw_nonblock(snd_rawmidi_t *rmidi, int nonblock)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	long flags;

	if ((flags = fcntl(hw->fd, F_GETFL)) < 0) {
		SYSERR("F_GETFL failed");
		return -errno;
	}
	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if (fcntl(hw->fd, F_SETFL, flags) < 0) {
		SYSERR("F_SETFL for O_NONBLOCK failed");
		return -errno;
	}
	return 0;
}

static int snd_rawmidi_hw_info(snd_rawmidi_t *rmidi, snd_rawmidi_info_t * info)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	info->stream = rmidi->stream;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_INFO, info) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_INFO failed");
		return -errno;
	}
	return 0;
}

static int snd_rawmidi_hw_params(snd_rawmidi_t *rmidi, snd_rawmidi_params_t * params)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	int tstamp;
	params->stream = rmidi->stream;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_PARAMS, params) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_PARAMS failed");
		return -errno;
	}
	buf_reset(hw);
	tstamp = (params->mode & SNDRV_RAWMIDI_MODE_FRAMING_MASK) == SNDRV_RAWMIDI_MODE_FRAMING_TSTAMP;
	if (hw->buf && !tstamp) {
		free(hw->buf);
		hw->buf = NULL;
		hw->buf_size = 0;
	} else if (tstamp) {
		size_t alloc_size;
		void *buf;

		alloc_size = page_size();
		if (params->buffer_size > alloc_size)
			alloc_size = params->buffer_size;
		if (alloc_size != hw->buf_size) {
			buf = realloc(hw->buf, alloc_size);
			if (buf == NULL)
				return -ENOMEM;
			hw->buf = buf;
			hw->buf_size = alloc_size;
		}
	}
	return 0;
}

static int snd_rawmidi_hw_status(snd_rawmidi_t *rmidi, snd_rawmidi_status_t * status)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	status->stream = rmidi->stream;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_STATUS, status) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_STATUS failed");
		return -errno;
	}
	return 0;
}

static int snd_rawmidi_hw_drop(snd_rawmidi_t *rmidi)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	int str = rmidi->stream;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_DROP, &str) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_DROP failed");
		return -errno;
	}
	buf_reset(hw);
	return 0;
}

static int snd_rawmidi_hw_drain(snd_rawmidi_t *rmidi)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	int str = rmidi->stream;
	if (ioctl(hw->fd, SNDRV_RAWMIDI_IOCTL_DRAIN, &str) < 0) {
		SYSERR("SNDRV_RAWMIDI_IOCTL_DRAIN failed");
		return -errno;
	}
	return 0;
}

static ssize_t snd_rawmidi_hw_write(snd_rawmidi_t *rmidi, const void *buffer, size_t size)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	ssize_t result;
	result = write(hw->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_rawmidi_hw_read(snd_rawmidi_t *rmidi, void *buffer, size_t size)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	ssize_t result;
	result = read(hw->fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t read_from_ts_buf(snd_rawmidi_hw_t *hw, struct timespec *tstamp,
				void *buffer, size_t size)
{
	struct snd_rawmidi_framing_tstamp *f;
	size_t flen;
	ssize_t result = 0;

	f = (struct snd_rawmidi_framing_tstamp *)(hw->buf + hw->buf_pos);
	while (hw->buf_fill >= sizeof(*f)) {
		if (f->frame_type == 0) {
			tstamp->tv_sec = f->tv_sec;
			tstamp->tv_nsec = f->tv_nsec;
			break;
		}
		hw->buf_pos += sizeof(*f);
		hw->buf_fill -= sizeof(*f);
		f++;
	}
	while (size > 0 && hw->buf_fill >= sizeof(*f)) {
		/* skip other frames */
		if (f->frame_type != 0)
			goto __next;
		if (f->length == 0 || f->length > SNDRV_RAWMIDI_FRAMING_DATA_LENGTH)
			return -EINVAL;
		if (tstamp->tv_sec != (time_t)f->tv_sec ||
		    tstamp->tv_nsec != f->tv_nsec)
			break;
		flen = f->length - hw->buf_fpos;
		if (size < flen) {
			/* partial copy */
			memcpy(buffer, f->data + hw->buf_fpos, size);
			hw->buf_fpos += size;
			result += size;
			break;
		}
		memcpy(buffer, f->data + hw->buf_fpos, flen);
		hw->buf_fpos = 0;
		buffer += flen;
		size -= flen;
		result += flen;
	     __next:
		hw->buf_pos += sizeof(*f);
		hw->buf_fill -= sizeof(*f);
		f++;
	}
	return result;
}

static ssize_t snd_rawmidi_hw_tread(snd_rawmidi_t *rmidi, struct timespec *tstamp,
				    void *buffer, size_t size)
{
	snd_rawmidi_hw_t *hw = rmidi->private_data;
	ssize_t result = 0, ret;

	/* no timestamp */
	tstamp->tv_sec = tstamp->tv_nsec = 0;

	/* copy buffered frames */
	if (hw->buf_fill > 0) {
		result = read_from_ts_buf(hw, tstamp, buffer, size);
		if (result < 0 || size == (size_t)result ||
		    hw->buf_fill >= sizeof(struct snd_rawmidi_framing_tstamp))
			return result;
		buffer += result;
		size -= result;
	}

	buf_reset(hw);
	ret = read(hw->fd, hw->buf, hw->buf_size);
	if (ret < 0)
		return result > 0 ? result : -errno;
	if (ret < (ssize_t)sizeof(struct snd_rawmidi_framing_tstamp))
		return result;
	hw->buf_fill = ret;
	ret = read_from_ts_buf(hw, tstamp, buffer, size);
	if (ret < 0 && result > 0)
		return result;
	return ret + result;
}

static const snd_rawmidi_ops_t snd_rawmidi_hw_ops = {
	.close = snd_rawmidi_hw_close,
	.nonblock = snd_rawmidi_hw_nonblock,
	.info = snd_rawmidi_hw_info,
	.params = snd_rawmidi_hw_params,
	.status = snd_rawmidi_hw_status,
	.drop = snd_rawmidi_hw_drop,
	.drain = snd_rawmidi_hw_drain,
	.write = snd_rawmidi_hw_write,
	.read = snd_rawmidi_hw_read,
	.tread = snd_rawmidi_hw_tread
};


int snd_rawmidi_hw_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp,
			const char *name, int card, int device, int subdevice,
			int mode)
{
	int fd, ver, ret;
	int attempt = 0;
	char filename[sizeof(SNDRV_FILE_RAWMIDI) + 20];
	snd_ctl_t *ctl;
	snd_rawmidi_t *rmidi;
	snd_rawmidi_hw_t *hw = NULL;
	snd_rawmidi_info_t info;
	int fmode;

	if (inputp)
		*inputp = NULL;
	if (outputp)
		*outputp = NULL;
	if (!inputp && !outputp)
		return -EINVAL;
	
	if ((ret = snd_ctl_hw_open(&ctl, NULL, card, 0)) < 0)
		return ret;
	sprintf(filename, SNDRV_FILE_RAWMIDI, card, device);

      __again:
      	if (attempt++ > 3) {
      		snd_ctl_close(ctl);
      		return -EBUSY;
      	}
      	ret = snd_ctl_rawmidi_prefer_subdevice(ctl, subdevice);
	if (ret < 0) {
		snd_ctl_close(ctl);
		return ret;
	}

	if (!inputp)
		fmode = O_WRONLY;
	else if (!outputp)
		fmode = O_RDONLY;
	else
		fmode = O_RDWR;

	if (mode & SND_RAWMIDI_APPEND) {
		assert(outputp);
		fmode |= O_APPEND;
	}

	if (mode & SND_RAWMIDI_NONBLOCK) {
		fmode |= O_NONBLOCK;
	}
	
	if (mode & SND_RAWMIDI_SYNC) {
		fmode |= O_SYNC;
	}

	assert(!(mode & ~(SND_RAWMIDI_APPEND|SND_RAWMIDI_NONBLOCK|SND_RAWMIDI_SYNC)));

	fd = snd_open_device(filename, fmode);
	if (fd < 0) {
		snd_card_load(card);
		fd = snd_open_device(filename, fmode);
		if (fd < 0) {
			snd_ctl_close(ctl);
			SYSERR("open %s failed", filename);
			return -errno;
		}
	}
	if (ioctl(fd, SNDRV_RAWMIDI_IOCTL_PVERSION, &ver) < 0) {
		ret = -errno;
		SYSERR("SNDRV_RAWMIDI_IOCTL_PVERSION failed");
		close(fd);
		snd_ctl_close(ctl);
		return ret;
	}
	if (SNDRV_PROTOCOL_INCOMPATIBLE(ver, SNDRV_RAWMIDI_VERSION_MAX)) {
		close(fd);
		snd_ctl_close(ctl);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	if (SNDRV_PROTOCOL_VERSION(2, 0, 2) <= ver) {
		/* inform the protocol version we're supporting */
		unsigned int user_ver = SNDRV_RAWMIDI_VERSION;
		ioctl(fd, SNDRV_RAWMIDI_IOCTL_USER_PVERSION, &user_ver);
	}
	if (subdevice >= 0) {
		memset(&info, 0, sizeof(info));
		info.stream = outputp ? SNDRV_RAWMIDI_STREAM_OUTPUT : SNDRV_RAWMIDI_STREAM_INPUT;
		if (ioctl(fd, SNDRV_RAWMIDI_IOCTL_INFO, &info) < 0) {
			SYSERR("SNDRV_RAWMIDI_IOCTL_INFO failed");
			ret = -errno;
			close(fd);
			snd_ctl_close(ctl);
			return ret;
		}
		if (info.subdevice != (unsigned int) subdevice) {
			close(fd);
			goto __again;
		}
	}
	snd_ctl_close(ctl);

	hw = calloc(1, sizeof(snd_rawmidi_hw_t));
	if (hw == NULL)
		goto _nomem;
	hw->card = card;
	hw->device = device;
	hw->subdevice = subdevice;
	hw->fd = fd;

	if (inputp) {
		rmidi = calloc(1, sizeof(snd_rawmidi_t));
		if (rmidi == NULL)
			goto _nomem;
		if (name)
			rmidi->name = strdup(name);
		rmidi->type = SND_RAWMIDI_TYPE_HW;
		rmidi->stream = SND_RAWMIDI_STREAM_INPUT;
		rmidi->mode = mode;
		rmidi->poll_fd = fd;
		rmidi->ops = &snd_rawmidi_hw_ops;
		rmidi->private_data = hw;
		rmidi->version = ver;
		hw->open++;
		*inputp = rmidi;
	}
	if (outputp) {
		rmidi = calloc(1, sizeof(snd_rawmidi_t));
		if (rmidi == NULL)
			goto _nomem;
		if (name)
			rmidi->name = strdup(name);
		rmidi->type = SND_RAWMIDI_TYPE_HW;
		rmidi->stream = SND_RAWMIDI_STREAM_OUTPUT;
		rmidi->mode = mode;
		rmidi->poll_fd = fd;
		rmidi->ops = &snd_rawmidi_hw_ops;
		rmidi->private_data = hw;
		rmidi->version = ver;
		hw->open++;
		*outputp = rmidi;
	}
	return 0;

 _nomem:
	close(fd);
	free(hw);
	if (inputp)
		free(*inputp);
	if (outputp)
		free(*outputp);
	return -ENOMEM;
}

int _snd_rawmidi_hw_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp,
			 char *name, snd_config_t *root ATTRIBUTE_UNUSED,
			 snd_config_t *conf, int mode)
{
	snd_config_iterator_t i, next;
	long card = -1, device = 0, subdevice = -1;
	int err;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_rawmidi_conf_generic_id(id))
			continue;
		if (strcmp(id, "card") == 0) {
			err = snd_config_get_card(n);
			if (err < 0)
				return err;
			card = err;
			continue;
		}
		if (strcmp(id, "device") == 0) {
			err = snd_config_get_integer(n, &device);
			if (err < 0)
				return err;
			continue;
		}
		if (strcmp(id, "subdevice") == 0) {
			err = snd_config_get_integer(n, &subdevice);
			if (err < 0)
				return err;
			continue;
		}
		return -EINVAL;
	}
	if (card < 0)
		return -EINVAL;
	return snd_rawmidi_hw_open(inputp, outputp, name, card, device, subdevice, mode);
}
SND_DLSYM_BUILD_VERSION(_snd_rawmidi_hw_open, SND_RAWMIDI_DLSYM_VERSION);
