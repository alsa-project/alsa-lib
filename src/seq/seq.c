/*
 *  Sequencer Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@jcu.cz>
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

#define SND_FILE_SEQ		"/dev/snd/seq"
#define SND_SEQ_VERSION_MAX	SND_PROTOCOL_VERSION( 0, 0, 1 )
#define SND_SEQ_OBUF_SIZE	(16*1024)	/* should be configurable */
#define SND_SEQ_IBUF_SIZE	(4*1024)	/* should be configurable */

typedef struct snd_stru_seq_cell {
	snd_seq_event_t ev;
	struct snd_stru_seq_cell *next;
} snd_seq_cell_t;

typedef struct {
	int client;		/* client number */
	int fd;
	/* buffers */
	char *obuf;		/* output buffer */
	int obufsize;		/* output buffer size */
	int obufused;		/* output buffer used size */
	char *ibuf;		/* input buffer */
	int ibufsize;		/* input buffer size */
	/* input queue */
	int cells;
	snd_seq_cell_t *head;
	snd_seq_cell_t *tail;
} snd_seq_t;

int snd_seq_open(void **handle, int mode)
{
	int fd, ver, client, flg;
	char filename[32];
	snd_seq_t *seq;

	*handle = NULL;

	sprintf(filename, SND_FILE_SEQ);
	if ((fd = open(filename, mode)) < 0) {
		/* try load all soundcard modules */
		snd_cards_mask();
		if ((fd = open(filename, mode)) < 0)
			return -errno;
	}
	if (ioctl(fd, SND_SEQ_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_UNCOMPATIBLE(ver, SND_SEQ_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_UNCOMPATIBLE_VERSION;
	}
	if (ioctl(fd, SND_SEQ_IOCTL_CLIENT_ID, &client) < 0) {
		close(fd);
		return -errno;
	}
	seq = (snd_seq_t *) calloc(1, sizeof(snd_seq_t));
	if (seq == NULL) {
		close(fd);
		return -ENOMEM;
	}
	seq->client = client;
	seq->fd = fd;
	flg = 3;
	if (mode == SND_SEQ_OPEN_OUT || mode == SND_SEQ_OPEN)
		seq->obuf = (char *) malloc(seq->obufsize = SND_SEQ_OBUF_SIZE);
	else
		flg &= ~1;
	if (mode == SND_SEQ_OPEN_IN || mode == SND_SEQ_OPEN)
		seq->ibuf = (char *) malloc(seq->ibufsize = SND_SEQ_IBUF_SIZE);
	else
		flg &= ~2;
	if ((!seq->obuf && (flg & 1)) || (!seq->ibuf && (flg & 2))) {
		if (seq->obuf)
			free(seq->obuf);
		if (seq->ibuf)
			free(seq->ibuf);
		free(seq);
		return -ENOMEM;
	}
	*handle = seq;
	return 0;
}

int snd_seq_close(void *handle)
{
	snd_seq_t *seq;
	int res;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;
	res = close(seq->fd) < 0 ? -errno : 0;
	free(seq);
	return res;
}

int snd_seq_file_descriptor(void *handle)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;
	return seq->fd;
}

int snd_seq_block_mode(void *handle, int enable)
{
	snd_seq_t *seq;
	long flags;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;
	if ((flags = fcntl(seq->fd, F_GETFL)) < 0)
		return -errno;
	if (enable)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	if (fcntl(seq->fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

int snd_seq_client_id(void *handle)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;
	return seq->client;
}

int snd_seq_system_info(void *handle, snd_seq_system_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SYSTEM_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_client_info(void *handle, snd_seq_client_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info)
		return -EINVAL;
	return snd_seq_get_any_client_info(handle, seq->client, info);
}

int snd_seq_get_any_client_info(void *handle, int client, snd_seq_client_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info || client < 0)
		return -EINVAL;
	bzero(info, sizeof(snd_seq_client_info_t));
	info->client = client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_CLIENT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_client_info(void *handle, snd_seq_client_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_CLIENT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_create_port(void *handle, snd_seq_port_info_t * port)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !port)
		return -EINVAL;
	port->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_CREATE_PORT, port) < 0)
		return -errno;
	return 0;
}

int snd_seq_delete_port(void *handle, snd_seq_port_info_t * port)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !port)
		return -EINVAL;
	port->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_DELETE_PORT, port) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_port_info(void *handle, int port, snd_seq_port_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info || port < 0)
		return -EINVAL;
	return snd_seq_get_any_port_info(handle, seq->client, port, info);
}

int snd_seq_get_any_port_info(void *handle, int client, int port, snd_seq_port_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info || client < 0 || port < 0)
		return -EINVAL;
	bzero(info, sizeof(snd_seq_port_info_t));
	info->client = client;
	info->port = port;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_PORT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_port_info(void *handle, int port, snd_seq_port_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info || port < 0)
		return -EINVAL;
	info->port = port;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_PORT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_subscribe_port(void *handle, snd_seq_port_subscribe_t * sub)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !sub)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SUBSCRIBE_PORT, sub) < 0)
		return -errno;
	return 0;
}

int snd_seq_unsubscribe_port(void *handle, snd_seq_port_subscribe_t * sub)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !sub)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_UNSUBSCRIBE_PORT, sub) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_queue_info(void *handle, int q, snd_seq_queue_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info)
		return -EINVAL;
	bzero(info, sizeof(snd_seq_queue_info_t));
	info->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_queue_info(void *handle, int q, snd_seq_queue_info_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info)
		return -EINVAL;
	info->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_QUEUE_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_queue_client(void *handle, int q, snd_seq_queue_client_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info)
		return -EINVAL;
	bzero(info, sizeof(snd_seq_queue_client_t));
	info->queue = q;
	info->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_CLIENT, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_queue_client(void *handle, int q, snd_seq_queue_client_t * info)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !info)
		return -EINVAL;
	info->queue = q;
	info->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_QUEUE_CLIENT, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_alloc_queue(void *handle, snd_seq_queue_info_t *info)
{
	int i, err;
	snd_seq_system_info_t sysinfo;
	snd_seq_queue_info_t inf;
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;	
	if ((err = snd_seq_system_info(handle, &sysinfo))<0)
		return err;
	for (i = 0; i < sysinfo.queues; i++) {
		if ((err = snd_seq_get_queue_info(handle, i, &inf))<0)
			continue;
		if (inf.locked)
			continue;
		inf.locked = 1;
		inf.owner = seq->client;
		if ((err = snd_seq_set_queue_info(handle, i, &inf))<0)
			continue;
		if (info)
			memcpy(info, &inf, sizeof(snd_seq_queue_info_t));
		return i;
	}
	return -EBUSY;
}

int snd_seq_free_queue(void *handle, int q)
{
	int err;
	snd_seq_t *seq;
	snd_seq_queue_info_t inf;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;	
	if ((err = snd_seq_get_queue_info(handle, q, &inf))<0)
		return err;
	if (inf.locked && inf.owner == seq->client) {
		inf.locked = 0;
		inf.owner = -1;
		if ((err = snd_seq_set_queue_info(handle, q, &inf))<0)
			return err;
	}
	return 0;
}

snd_seq_event_t *snd_seq_create_event(void)
{
	return (snd_seq_event_t *) calloc(1, sizeof(snd_seq_event_t));
}

static int snd_seq_free_event_static(snd_seq_event_t *ev)
{
	if (!ev)
		return -EINVAL;
	switch (ev->flags & SND_SEQ_EVENT_LENGTH_MASK) {
	case SND_SEQ_EVENT_LENGTH_VARIABLE:
		if (ev->data.ext.ptr)
			free(ev->data.ext.ptr);
		break;
	}
	return 0;
}

int snd_seq_free_event(snd_seq_event_t *ev)
{
	int err;

	if ((err = snd_seq_free_event_static(ev))<0)
		return err;
	free(ev);
	return 0;
}

int snd_seq_event_length(snd_seq_event_t *ev)
{
	int len = sizeof(snd_seq_event_t);

	if (!ev)
		return -EINVAL;
	switch (ev->flags & SND_SEQ_EVENT_LENGTH_MASK) {
	case SND_SEQ_EVENT_LENGTH_VARIABLE:
		len += ev->data.ext.len;
		break;
	}
	return len;
}

int snd_seq_event_output(void *handle, snd_seq_event_t *ev)
{
	int len;
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq || !ev)
		return -EINVAL;
	len = snd_seq_event_length(ev);
	if ((seq->obufsize - seq->obufused) < len) {
		snd_seq_flush_output(handle);
		if ((seq->obufsize - seq->obufused) < len)
			return -ENOMEM;
	}
	memcpy(seq->obuf + seq->obufused, ev, sizeof(snd_seq_event_t));
	seq->obufused += sizeof(snd_seq_event_t);
	switch (ev->flags & SND_SEQ_EVENT_LENGTH_MASK) {
	case SND_SEQ_EVENT_LENGTH_VARIABLE:
		memcpy(seq->obuf + seq->obufused, ev->data.ext.ptr, ev->data.ext.len);
		seq->obufused += ev->data.ext.len;
		break;
	}
	return seq->obufused;
}

static snd_seq_cell_t *snd_seq_create_cell(snd_seq_event_t *ev)
{
	snd_seq_cell_t *cell;

	cell = (snd_seq_cell_t *) calloc(1, sizeof(snd_seq_cell_t));
	if (!cell)
		return NULL;
	if (ev)
		memcpy(&cell->ev, ev, sizeof(snd_seq_event_t));
	return cell;
}

static int snd_seq_free_cell(snd_seq_cell_t *cell)
{
	if (!cell)
		return -EINVAL;
	snd_seq_free_event_static(&cell->ev);
	free(cell);
	return 0;
}

static snd_seq_cell_t *snd_seq_input_cell_out(snd_seq_t *seq)
{
	snd_seq_cell_t *cell;

	if (!seq)
		return NULL;
	if (seq->head) {
		cell = seq->head;
		seq->head = cell->next;
		seq->cells--;
		if (!seq->head)
			seq->tail = NULL;
		return cell;
	}
	return NULL;
}

static int snd_seq_input_cell_in(snd_seq_t *seq, snd_seq_cell_t *cell)
{
	if (!seq)
		return -EINVAL;
	cell->next = NULL;
	if (!seq->tail) {
		seq->head = seq->tail = cell;
	} else {
		seq->tail->next = cell;
		seq->tail = cell;
	}
	seq->cells++;
	return 0;
}

static int snd_seq_input_cell_available(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	return seq->cells > 0;
}

static int snd_seq_decode_event(char **buf, int *len, snd_seq_event_t *ev)
{
	if (!ev || !buf || !*buf || !len )
		return -EINVAL;
	if (*len < sizeof(snd_seq_event_t)) {
		*len = 0;
		return -ENOENT;
	}
	memcpy(ev, *buf, sizeof(snd_seq_event_t));
	*buf += sizeof(snd_seq_event_t);
	*len -= sizeof(snd_seq_event_t);
	switch (ev->flags & SND_SEQ_EVENT_LENGTH_MASK) {
	case SND_SEQ_EVENT_LENGTH_VARIABLE:
		if (*len < ev->data.ext.len) {
			*len = 0;
			return -ENOENT;
		}
		if (ev->data.ext.len > 0) {
			ev->data.ext.ptr = (char *) malloc(ev->data.ext.len);
			if (!(ev->data.ext.ptr)) {
				*buf += ev->data.ext.len;
				*len -= ev->data.ext.len;
				return -ENOENT;
			}
			memcpy(ev->data.ext.ptr, *buf, ev->data.ext.len);
			*buf += ev->data.ext.len;
			*len -= ev->data.ext.len;
		}
		break;
	}
	return 0;
}

/*
 *  Current implementation uses FIFO cache.
 */

int snd_seq_event_input(void *handle, snd_seq_event_t **ev)
{
	snd_seq_t *seq;
	snd_seq_cell_t *cell;
	char *buf;
	int count;

	*ev = NULL;
	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;
	if (snd_seq_input_cell_available(seq)) {
		*ev = snd_seq_create_event();
		if (*ev == NULL)
			return -ENOMEM;
		cell = snd_seq_input_cell_out(seq);
		memcpy(*ev, &cell->ev, sizeof(snd_seq_event_t));
		return seq->cells;
	}
	count = read(seq->fd, seq->ibuf, seq->ibufsize);
	if (count < 0)
		return -errno;
	buf = seq->ibuf;
	while (count > 0) {
		if (*ev == NULL) {	/* first event */
			*ev = snd_seq_create_event();
			if (*ev == NULL)
				return -ENOMEM;
			if (snd_seq_decode_event(&buf, &count, *ev)<0) {
				snd_seq_free_event(*ev);
				*ev = NULL;
			}
		} else {
			cell = snd_seq_create_cell(NULL);
			if (cell == NULL)
				return -ENOMEM;
			if (snd_seq_decode_event(&buf, &count, &cell->ev)<0) {
				snd_seq_free_cell(cell);
			} else {
				snd_seq_input_cell_in(seq, cell);
			}
		}
	}
	return seq->cells;
}

int snd_seq_flush_output(void *handle)
{
	snd_seq_t *seq;
	int result;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;
	if (seq->obufused <= 0)
		return 0;
	result = write(seq->fd, seq->obuf, seq->obufused);
	if (result < 0)	{
		snd_seq_drain_output(handle);
		return -errno;
	}
	if (result < seq->obufused)
		memmove(seq->obuf, seq->obuf + result, seq->obufused - result);
	seq->obufused -= result;
	return seq->obufused;
}

int snd_seq_drain_output(void *handle)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;
	seq->obufused = 0;
	return 0;
}

int snd_seq_drain_input(void *handle)
{
	snd_seq_t *seq;

	seq = (snd_seq_t *) handle;
	if (!seq)
		return -EINVAL;
	while (snd_seq_input_cell_available(seq))
		snd_seq_free_cell(snd_seq_input_cell_out(seq));
	return 0;
}

void snd_seq_set_bit(int nr, void *array)
{
	((unsigned int *)array)[nr >> 5] |= 1UL << (nr & 31);
}

int snd_seq_change_bit(int nr, void *array)
{
	int result;

	result = ((((unsigned int *)array)[nr >> 5]) & (1UL << (nr & 31))) ? 1 : 0;
	((unsigned int *)array)[nr >> 5] |= 1UL << (nr & 31);
	return result;
}

int snd_seq_get_bit(int nr, void *array)
{
	return ((((unsigned int *)array)[nr >> 5]) & (1UL << (nr & 31))) ? 1 : 0;
}

