/*
 *  Sequencer Interface - main file
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
#include "seq_priv.h"

#define SND_FILE_SEQ		"/dev/snd/seq"
#define SND_FILE_ALOADSEQ	"/dev/aloadSEQ"
#define SND_SEQ_VERSION_MAX	SND_PROTOCOL_VERSION(1, 0, 0)
#define SND_SEQ_OBUF_SIZE	(16*1024)	/* should be configurable */
#define SND_SEQ_IBUF_SIZE	(4*1024)	/* should be configurable */

int snd_seq_open(snd_seq_t **handle, int mode)
{
	int fd, ver, client, flg;
	char filename[32];
	snd_seq_t *seq;

	*handle = NULL;

	sprintf(filename, SND_FILE_SEQ);
	if ((fd = open(filename, mode)) < 0) {
		close(open(SND_FILE_ALOADSEQ, O_RDWR));
		if ((fd = open(filename, mode)) < 0)
			return -errno;
	}
	if (ioctl(fd, SND_SEQ_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_SEQ_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
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

int snd_seq_close(snd_seq_t *seq)
{
	int res;

	if (!seq)
		return -EINVAL;
	res = close(seq->fd) < 0 ? -errno : 0;
	if (seq->obuf)
		free(seq->obuf);
	if (seq->ibuf)
		free(seq->ibuf);
	free(seq);
	return res;
}

int snd_seq_file_descriptor(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	return seq->fd;
}

int snd_seq_block_mode(snd_seq_t *seq, int enable)
{
	long flags;

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

int snd_seq_client_id(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	return seq->client;
}

int snd_seq_system_info(snd_seq_t *seq, snd_seq_system_info_t * info)
{
	if (!seq || !info)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SYSTEM_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	if (!seq || !info)
		return -EINVAL;
	return snd_seq_get_any_client_info(seq, seq->client, info);
}

int snd_seq_get_any_client_info(snd_seq_t *seq, int client, snd_seq_client_info_t * info)
{
	if (!seq || !info || client < 0)
		return -EINVAL;
	bzero(info, sizeof(snd_seq_client_info_t));
	info->client = client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_CLIENT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	if (!seq || !info)
		return -EINVAL;
	info->client = seq->client;
	info->type = USER_CLIENT;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_CLIENT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_create_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	if (!seq || !port)
		return -EINVAL;
	port->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_CREATE_PORT, port) < 0)
		return -errno;
	return 0;
}

int snd_seq_delete_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	if (!seq || !port)
		return -EINVAL;
	port->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_DELETE_PORT, port) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_port_info(snd_seq_t *seq, int port, snd_seq_port_info_t * info)
{
	if (!seq || !info || port < 0)
		return -EINVAL;
	return snd_seq_get_any_port_info(seq, seq->client, port, info);
}

int snd_seq_get_any_port_info(snd_seq_t *seq, int client, int port, snd_seq_port_info_t * info)
{
	if (!seq || !info || client < 0 || port < 0)
		return -EINVAL;
	bzero(info, sizeof(snd_seq_port_info_t));
	info->client = client;
	info->port = port;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_PORT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_port_info(snd_seq_t *seq, int port, snd_seq_port_info_t * info)
{
	if (!seq || !info || port < 0)
		return -EINVAL;
	info->port = port;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_PORT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_port_subscription(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	if (!seq || !sub)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_SUBSCRIPTION, sub) < 0)
		return -errno;
	return 0;
}

int snd_seq_subscribe_port(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	if (!seq || !sub)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SUBSCRIBE_PORT, sub) < 0)
		return -errno;
	return 0;
}

int snd_seq_unsubscribe_port(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	if (!seq || !sub)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_UNSUBSCRIBE_PORT, sub) < 0)
		return -errno;
	return 0;
}

int snd_seq_query_port_subscribers(snd_seq_t *seq, snd_seq_query_subs_t * subs)
{
	if (!seq || !subs)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_QUERY_SUBS, subs) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_queue_status(snd_seq_t *seq, int q, snd_seq_queue_status_t * status)
{
	if (!seq || !status)
		return -EINVAL;
	bzero(status, sizeof(snd_seq_queue_status_t));
	status->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_STATUS, status) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_queue_tempo(snd_seq_t *seq, int q, snd_seq_queue_tempo_t * tempo)
{
	if (!seq || !tempo)
		return -EINVAL;
	bzero(tempo, sizeof(snd_seq_queue_tempo_t));
	tempo->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_TEMPO, tempo) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_queue_tempo(snd_seq_t *seq, int q, snd_seq_queue_tempo_t * tempo)
{
	if (!seq || !tempo)
		return -EINVAL;
	tempo->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_QUEUE_TEMPO, tempo) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_queue_owner(snd_seq_t *seq, int q, snd_seq_queue_owner_t * owner)
{
	if (!seq || !owner)
		return -EINVAL;
	bzero(owner, sizeof(snd_seq_queue_owner_t));
	owner->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_OWNER, owner) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_queue_owner(snd_seq_t *seq, int q, snd_seq_queue_owner_t * owner)
{
	if (!seq || !owner)
		return -EINVAL;
	owner->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_QUEUE_OWNER, owner) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_queue_timer(snd_seq_t *seq, int q, snd_seq_queue_timer_t * timer)
{
	if (!seq || !timer)
		return -EINVAL;
	bzero(timer, sizeof(snd_seq_queue_timer_t));
	timer->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_TIMER, timer) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_queue_timer(snd_seq_t *seq, int q, snd_seq_queue_timer_t * timer)
{
	if (!seq || !timer)
		return -EINVAL;
	timer->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_QUEUE_TIMER, timer) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_queue_sync(snd_seq_t *seq, int q, snd_seq_queue_sync_t * sync)
{
	if (!seq || !sync)
		return -EINVAL;
	bzero(sync, sizeof(snd_seq_queue_sync_t));
	sync->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_SYNC, sync) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_queue_sync(snd_seq_t *seq, int q, snd_seq_queue_sync_t * sync)
{
	if (!seq || !sync)
		return -EINVAL;
	sync->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_QUEUE_SYNC, sync) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_queue_client(snd_seq_t *seq, int q, snd_seq_queue_client_t * info)
{
	if (!seq || !info)
		return -EINVAL;
	bzero(info, sizeof(snd_seq_queue_client_t));
	info->queue = q;
	info->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_CLIENT, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_queue_client(snd_seq_t *seq, int q, snd_seq_queue_client_t * info)
{
	if (!seq || !info)
		return -EINVAL;
	info->queue = q;
	info->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_QUEUE_CLIENT, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_alloc_named_queue(snd_seq_t *seq, char *name)
{
	snd_seq_queue_info_t info;

	if (!seq)
		return -EINVAL;	

	memset(&info, 0, sizeof(info));
	info.owner = seq->client;
	info.locked = 1;
	if (name)
		strncpy(info.name, name, sizeof(info.name) - 1);
	if (ioctl(seq->fd, SND_SEQ_IOCTL_CREATE_QUEUE, &info) < 0)
		return -errno;

	return info.queue;
}

int snd_seq_alloc_queue(snd_seq_t *seq)
{
	return snd_seq_alloc_named_queue(seq, NULL);
}

int snd_seq_free_queue(snd_seq_t *seq, int q)
{
	snd_seq_queue_info_t info;

	if (!seq)
		return -EINVAL;	

	memset(&info, 0, sizeof(info));
	info.queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_DELETE_QUEUE, &info) < 0)
		return -errno;

	return 0;
}

int snd_seq_get_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info)
{
	if (!seq || !info)
		return -EINVAL;
	info->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_QUEUE_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info)
{
	if (!seq || !info)
		return -EINVAL;
	info->queue = q;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_QUEUE_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_get_named_queue(snd_seq_t *seq, char *name)
{
	snd_seq_queue_info_t info;

	if (!seq)
		return -EINVAL;
	strncpy(info.name, name, sizeof(info.name));
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_NAMED_QUEUE, &info) < 0)
		return -errno;
	return info.queue;
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

int snd_seq_event_output(snd_seq_t *seq, snd_seq_event_t *ev)
{
	int len;

	if (!seq || !ev)
		return -EINVAL;
	len = snd_seq_event_length(ev);
	if ((seq->obufsize - seq->obufused) < len) {
		snd_seq_flush_output(seq);
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
			ev->flags &= ~SND_SEQ_EVENT_LENGTH_MASK; /* clear flag */
			return -ENOENT;
		}
		if (ev->data.ext.len > 0) {
			ev->data.ext.ptr = (char *) malloc(ev->data.ext.len);
			if (!(ev->data.ext.ptr)) {
			ev->flags &= ~SND_SEQ_EVENT_LENGTH_MASK; /* clear flag */
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

int snd_seq_event_input(snd_seq_t *seq, snd_seq_event_t **ev)
{
	snd_seq_cell_t *cell;
	char *buf;
	int count;

	*ev = NULL;
	if (!seq)
		return -EINVAL;
	if (snd_seq_input_cell_available(seq)) {
		*ev = snd_seq_create_event();
		if (*ev == NULL)
			return -ENOMEM;
		cell = snd_seq_input_cell_out(seq);
		memcpy(*ev, &cell->ev, sizeof(snd_seq_event_t));
		/* clear flag to avoid free copied data */
		cell->ev.flags &= ~SND_SEQ_EVENT_LENGTH_MASK;
		snd_seq_free_cell(cell);
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

int snd_seq_flush_output(snd_seq_t *seq)
{
	int result;

	if (!seq)
		return -EINVAL;
	if (seq->obufused <= 0)
		return 0;
	result = write(seq->fd, seq->obuf, seq->obufused);
	if (result < 0)
		return -errno;

	if (result < seq->obufused)
		memmove(seq->obuf, seq->obuf + result, seq->obufused - result);
	seq->obufused -= result;
	return seq->obufused;
}

int snd_seq_drain_output(snd_seq_t *seq)
{
	snd_seq_remove_events_t rminfo;

	if (!seq)
		return -EINVAL;

	memset(&rminfo, 0, sizeof(rminfo));
	rminfo.output = 1;

	snd_seq_remove_events(seq, &rminfo);
	return 0;
}

/* compare timestamp between events */
/* return 1 if a >= b; otherwise return 0 */
static inline int snd_seq_compare_tick_time(snd_seq_tick_time_t *a,
	snd_seq_tick_time_t *b)
{
	/* compare ticks */
	return (*a >= *b);
}

static inline int snd_seq_compare_real_time(snd_seq_real_time_t *a,
	snd_seq_real_time_t *b)
{
	/* compare real time */
	if (a->tv_sec > b->tv_sec)
		return 1;
	if ((a->tv_sec == b->tv_sec) && (a->tv_nsec >= b->tv_nsec))
		return 1;
	return 0;
}

/* Routine to match events to be removed */
static int remove_match(snd_seq_remove_events_t *info,
	snd_seq_event_t *ev)
{
	int res;

	if (info->remove_mode & SND_SEQ_REMOVE_DEST) {
		if (ev->dest.client != info->dest.client ||
				ev->dest.port != info->dest.port)
			return 0;
	}
	if (info->remove_mode & SND_SEQ_REMOVE_DEST_CHANNEL) {
		if (! snd_seq_ev_is_channel_type(ev))
			return 0;
		/* data.note.channel and data.control.channel are identical */
		if (ev->data.note.channel != info->channel)
			return 0;
	}
	if (info->remove_mode & SND_SEQ_REMOVE_TIME_AFTER) {
		if (info->tick)
			res = snd_seq_compare_tick_time(&ev->time.tick, &info->time.tick);
		else
			res = snd_seq_compare_real_time(&ev->time.real, &info->time.real);
		if (!res)
			return 0;
	}
	if (info->remove_mode & SND_SEQ_REMOVE_TIME_BEFORE) {
		if (info->tick)
			res = snd_seq_compare_tick_time(&ev->time.tick, &info->time.tick);
		else
			res = snd_seq_compare_real_time(&ev->time.real, &info->time.real);
		if (res)
			return 0;
	}
	if (info->remove_mode & SND_SEQ_REMOVE_EVENT_TYPE) {
		if (ev->type != info->type)
			return 0;
	}
	if (info->remove_mode & SND_SEQ_REMOVE_IGNORE_OFF) {
		/* Do not remove off events */
		switch (ev->type) {
		case SND_SEQ_EVENT_NOTEOFF:
		/* case SND_SEQ_EVENT_SAMPLE_STOP: */
			return 0;
		default:
			break;
		}
	}
	if (info->remove_mode & SND_SEQ_REMOVE_TAG_MATCH) {
		if (info->tag != ev->tag)
			return 0;
	}

	return 1;
}

/*
 * Remove events from the sequencer queues.
 */
int snd_seq_remove_events(snd_seq_t *seq, snd_seq_remove_events_t *rmp)
{
	if (rmp->input) {
		/*
		 * First deal with any events that are still buffered
		 * in the library.
		 */
		/* Input not implemented */
	}

	if (rmp->output) {
		/*
		 * First deal with any events that are still buffered
		 * in the library.
		 */
		 if (rmp->remove_mode == 0) {
			/* The simple case - remove all */
			seq->obufused = 0;
		} else {
			char *ep;
			int  len;
			snd_seq_event_t *ev;

			ep = seq->obuf;
			while (ep - seq->obuf < seq->obufused) {

				ev = (snd_seq_event_t *) ep;
				len = snd_seq_event_length(ev);

				if (remove_match(rmp, ev)) {
					/* Remove event */
					seq->obufused -= len;
					memmove(ep, ep + len, seq->obufused - (seq->obuf - ep));
				} else {
					ep += len;
				}
			}
		}
	}

	if (ioctl(seq->fd, SND_SEQ_IOCTL_REMOVE_EVENTS, rmp) < 0)
		return -errno;

	return 0;
}
int snd_seq_drain_input(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	while (snd_seq_input_cell_available(seq))
		snd_seq_free_cell(snd_seq_input_cell_out(seq));
	return 0;
}

int snd_seq_get_client_pool(snd_seq_t *seq, snd_seq_client_pool_t *info)
{
	if (!seq || !info)
		return -EINVAL;
	info->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_GET_CLIENT_POOL, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_set_client_pool(snd_seq_t *seq, snd_seq_client_pool_t *info)
{
	if (!seq || !info)
		return -EINVAL;
	info->client = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SET_CLIENT_POOL, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_query_next_client(snd_seq_t *seq, snd_seq_client_info_t *info)
{
	if (!seq || !info)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_QUERY_NEXT_CLIENT, info) < 0)
		return -errno;
	return 0;
}

int snd_seq_query_next_port(snd_seq_t *seq, snd_seq_port_info_t *info)
{
	if (!seq || !info)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_QUERY_NEXT_PORT, info) < 0)
		return -errno;
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

