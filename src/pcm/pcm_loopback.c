/*
 *  PCM LoopBack Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@suse.cz>
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
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "asoundlib.h"

#define SND_FILE_PCM_LB		"/proc/asound/%i/pcmloopD%iS%i%s"
#define SND_PCM_LB_VERSION_MAX	SND_PROTOCOL_VERSION(1, 0, 0)

struct snd_pcm_loopback {
	int card;
	int device;
	int fd;
	long mode;
	size_t buffer_size;
	char *buffer;
};

int snd_pcm_loopback_open(snd_pcm_loopback_t **handle, int card, int device, int subchn, int mode)
{
	int fd, ver;
	char filename[32];
	snd_pcm_loopback_t *lb;

	*handle = NULL;

	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	sprintf(filename, SND_FILE_PCM_LB, card, device, subchn,
		mode == SND_PCM_LB_OPEN_CAPTURE ? "c" : "p");
	if ((fd = open(filename, mode)) < 0) {
		snd_card_load(card);
		if ((fd = open(filename, mode)) < 0) 
			return -errno;
	}
	if (ioctl(fd, SND_PCM_LB_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_PCM_LB_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	lb = (snd_pcm_loopback_t *) calloc(1, sizeof(snd_pcm_loopback_t));
	if (lb == NULL) {
		close(fd);
		return -ENOMEM;
	}
	lb->card = card;
	lb->device = device;
	lb->fd = fd;
	lb->mode = SND_PCM_LB_STREAM_MODE_RAW;
	*handle = lb;
	return 0;
}

int snd_pcm_loopback_close(snd_pcm_loopback_t *lb)
{
	int res;

	if (!lb)
		return -EINVAL;
	if (lb->buffer)
		free(lb->buffer);
	res = close(lb->fd) < 0 ? -errno : 0;
	free(lb);
	return res;
}

int snd_pcm_loopback_file_descriptor(snd_pcm_loopback_t *lb)
{
	if (!lb)
		return -EINVAL;
	return lb->fd;
}

int snd_pcm_loopback_block_mode(snd_pcm_loopback_t *lb, int enable)
{
	long flags;

	if (!lb)
		return -EINVAL;
	if ((flags = fcntl(lb->fd, F_GETFL)) < 0)
		return -errno;
	if (enable)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	if (fcntl(lb->fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

int snd_pcm_loopback_stream_mode(snd_pcm_loopback_t *lb, int mode)
{
	if (!lb || (mode != SND_PCM_LB_STREAM_MODE_RAW &&
	            mode != SND_PCM_LB_STREAM_MODE_PACKET))
		return -EINVAL;
	lb->mode = mode;
	if (ioctl(lb->fd, SND_PCM_LB_IOCTL_STREAM_MODE, &lb->mode) < 0)
		return -errno;
	return 0;
}

int snd_pcm_loopback_format(snd_pcm_loopback_t *lb, snd_pcm_format_t * format)
{
	if (!lb)
		return -EINVAL;
	if (ioctl(lb->fd, SND_PCM_LB_IOCTL_FORMAT, format) < 0)
		return -errno;
	return 0;
}

int snd_pcm_loopback_status(snd_pcm_loopback_t *lb, snd_pcm_loopback_status_t * status)
{
	if (!lb)
		return -EINVAL;
	if (ioctl(lb->fd, SND_PCM_LB_IOCTL_FORMAT, status) < 0)
		return -errno;
	return 0;
}

ssize_t snd_pcm_loopback_read(snd_pcm_loopback_t *lb, snd_pcm_loopback_callbacks_t *callbacks)
{
	ssize_t result = 0, res, count;
	size_t size;
	char *buf;
	snd_pcm_loopback_header_t header;

	if (!lb || !callbacks)
		return -EINVAL;
	if (callbacks->max_buffer_size == 0)
		size = 64 * 1024;
	else
		size = callbacks->max_buffer_size < 16 ? 16 : callbacks->max_buffer_size;
	while (1) {
		if (lb->mode == SND_PCM_LB_STREAM_MODE_RAW) {
			header.size = size;
			header.type = SND_PCM_LB_TYPE_DATA;
		} else {
			res = read(lb->fd, &header, sizeof(header));
			if (res < 0)
				return -errno;
			if (res == 0)
				break;
			if (res != sizeof(header))
				return -EBADFD;
			result += res;
		}
		switch (header.type) {
		case SND_PCM_LB_TYPE_DATA:
			if (lb->buffer_size < size) {
				buf = (char *) realloc(lb->buffer, size);
				if (buf == NULL)
					return -ENOMEM;
				lb->buffer = buf;
				lb->buffer_size = size;
			} else {
				buf = lb->buffer;
			}
			while (header.size > 0) {
				count = header.size;
				if (count > size)
					count = size;
				res = read(lb->fd, buf, count);
				if (res < 0)
					return -errno;
				result += res;
				if (lb->mode == SND_PCM_LB_STREAM_MODE_PACKET && res != count)
					return -EBADFD;
				if (res == 0)
					break;
				if (callbacks->data)
					callbacks->data(callbacks->private_data, buf, res);
				if (res < count && lb->mode == SND_PCM_LB_STREAM_MODE_RAW)
					break;
				header.size -= res;
			}
			break;
		case SND_PCM_LB_TYPE_FORMAT:
			{
				snd_pcm_format_t format;
				
				res = read(lb->fd, &format, sizeof(format));
				if (res < 0)
					return -errno;
				result += res;
				if (res != sizeof(format))
					return -EBADFD;
				if (callbacks->format_change)
					callbacks->format_change(callbacks->private_data, &format);
			}
			break;
		case SND_PCM_LB_TYPE_POSITION:
			{
				unsigned int pos;
				
				res = read(lb->fd, &pos, sizeof(pos));
				if (res < 0)
					return -errno;
				result += res;
				if (res != sizeof(pos))
					return -EBADFD;
				if (callbacks->position_change)
					callbacks->position_change(callbacks->private_data, pos);
			}
			break;
		}
	}
	return result;
}
