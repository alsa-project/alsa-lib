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
#include <sys/time.h>
#include "asoundlib.h"
#include "seq_priv.h"

#define SND_FILE_SEQ		"/dev/snd/seq"
#define SND_FILE_ALOADSEQ	"/dev/aloadSEQ"
#define SND_SEQ_VERSION_MAX	SND_PROTOCOL_VERSION(1, 0, 0)
#define SND_SEQ_OBUF_SIZE	(16*1024)	/* default size */
#define SND_SEQ_IBUF_SIZE	500		/* in event_size aligned */
#define DEFAULT_TMPBUF_SIZE	20

/*
 * open a sequencer device so that it creates a user-client.
 */
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
		seq->ibuf = (snd_seq_event_t *) calloc(sizeof(snd_seq_event_t), seq->ibufsize = SND_SEQ_IBUF_SIZE);
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
	seq->tmpbuf = NULL;
	seq->tmpbufsize = 0;
	*handle = seq;
	return 0;
}

/*
 * release sequencer client
 */
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
	if (seq->tmpbuf)
		free(seq->tmpbuf);
	free(seq);
	return res;
}

/*
 * returns the file descriptor of the client
 */
int snd_seq_poll_descriptor(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	return seq->fd;
}

/*
 * set blocking behavior
 */
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

/*
 * return the client id
 */
int snd_seq_client_id(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	return seq->client;
}

/*
 * return buffer size
 */
int snd_seq_output_buffer_size(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	if (!seq->obuf)
		return 0;
	return seq->obufsize;
}

/*
 * return buffer size
 */
int snd_seq_input_buffer_size(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	if (!seq->ibuf)
		return 0;
	return seq->ibufsize * sizeof(snd_seq_event_t);
}

/*
 * resize output buffer
 */
int snd_seq_resize_output_buffer(snd_seq_t *seq, size_t size)
{
	if (!seq || !seq->obuf)
		return -EINVAL;
	if (size < sizeof(snd_seq_event_t))
		return -EINVAL;
	snd_seq_drop_output(seq);
	if (size != seq->obufsize) {
		char *newbuf;
		newbuf = calloc(1, size);
		if (newbuf == NULL)
			return -ENOMEM;
		free(seq->obuf);
		seq->obuf = newbuf;
		seq->obufsize = size;
	}
	return 0;
}

/*
 * resize input buffer
 */
int snd_seq_resize_input_buffer(snd_seq_t *seq, size_t size)
{
	if (!seq || !seq->ibuf)
		return -EINVAL;
	if (size < sizeof(snd_seq_event_t))
		return -EINVAL;
	snd_seq_drop_input(seq);
	size = (size + sizeof(snd_seq_event_t) - 1) / sizeof(snd_seq_event_t);
	if (size != seq->ibufsize) {
		snd_seq_event_t *newbuf;
		newbuf = calloc(sizeof(snd_seq_event_t), size);
		if (newbuf == NULL)
			return -ENOMEM;
		free(seq->ibuf);
		seq->ibuf = newbuf;
		seq->ibufsize = size;
	}
	return 0;
}

/*
 * obtain system information
 */
int snd_seq_system_info(snd_seq_t *seq, snd_seq_system_info_t * info)
{
	if (!seq || !info)
		return -EINVAL;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_SYSTEM_INFO, info) < 0)
		return -errno;
	return 0;
}

/*
 * obtain the current client information
 */
int snd_seq_get_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	if (!seq || !info)
		return -EINVAL;
	return snd_seq_get_any_client_info(seq, seq->client, info);
}

/*
 * obtain the information of given client
 */
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

/*
 * set the current client information
 */
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

/*----------------------------------------------------------------*/

/*
 * sequencer port handlers
 */

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

/*----------------------------------------------------------------*/

/*
 * subscription
 */

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

/*----------------------------------------------------------------*/

/*
 * queue handlers
 */

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

int snd_seq_create_queue(snd_seq_t *seq, snd_seq_queue_info_t *info)
{
	if (!seq)
		return -EINVAL;	
	info->owner = seq->client;
	if (ioctl(seq->fd, SND_SEQ_IOCTL_CREATE_QUEUE, info) < 0)
		return -errno;
	return info->queue;
}

int snd_seq_alloc_named_queue(snd_seq_t *seq, char *name)
{
	snd_seq_queue_info_t info;

	if (!seq)
		return -EINVAL;	

	memset(&info, 0, sizeof(info));
	info.locked = 1;
	if (name)
		strncpy(info.name, name, sizeof(info.name) - 1);
	return snd_seq_create_queue(seq, &info);
}

int snd_seq_alloc_queue(snd_seq_t *seq)
{
	return snd_seq_alloc_named_queue(seq, NULL);
}

#ifdef SND_SEQ_SYNC_SUPPORT
int snd_seq_alloc_sync_queue(snd_seq_t *seq, char *name)
{
	snd_seq_queue_info_t info;

	if (!seq)
		return -EINVAL;	

	memset(&info, 0, sizeof(info));
	info.locked = 1;
	if (name)
		strncpy(info.name, name, sizeof(info.name) - 1);
	info.flags = SND_SEQ_QUEUE_FLG_SYNC;
	return snd_seq_create_queue(seq, &info);
}
#endif

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

/*----------------------------------------------------------------*/

#ifdef SND_SEQ_SYNC_SUPPORT
/*
 * sync stuff
 */

int snd_seq_add_sync_master(snd_seq_t *seq,
			    int queue,
			    snd_seq_addr_t *dest,
			    snd_seq_queue_sync_t *info)
{
	snd_seq_port_subscribe_t subs;

	memset(&subs, 0, sizeof(subs));
	subs.convert_time = 1;
	if (info->format & SND_SEQ_SYNC_TIME)
		subs.realtime = 1;
	subs.sync = 1;
	subs.sender.client = SND_SEQ_CLIENT_SYSTEM;
	subs.sender.port = snd_seq_queue_sync_port(queue);
	subs.dest = *dest;
	subs.queue = queue;
	subs.opt.sync_info = *info;
	return snd_seq_subscribe_port(seq, &subs);
}

int snd_seq_add_sync_std_master(snd_seq_t *seq,
				int queue,
				snd_seq_addr_t *dest,
				int format, int time_format,
				unsigned char *optinfo)
{
	snd_seq_queue_sync_t sync_info;

	memset(&sync_info, 0, sizeof(sync_info));
	sync_info.format = format;
	sync_info.time_format = time_format;
	if (optinfo)
		memcpy(sync_info.info, optinfo, sizeof(sync_info.info));

	return snd_seq_add_sync_master(seq, queue, dest, &sync_info);
}


int snd_seq_remove_sync_master(snd_seq_t *seq, int queue, snd_seq_addr_t *dest)
{
	snd_seq_port_subscribe_t subs;

	memset(&subs, 0, sizeof(subs));
	subs.sync = 1;
	subs.sender.client = SND_SEQ_CLIENT_SYSTEM;
	subs.sender.port = snd_seq_queue_sync_port(queue);
	subs.dest = *dest;
	subs.queue = queue;
	return snd_seq_unsubscribe_port(seq, &subs);
}


int snd_seq_set_sync_slave(snd_seq_t *seq,
			   int queue,
			   snd_seq_addr_t *src,
			   snd_seq_queue_sync_t *info)
{
	snd_seq_port_subscribe_t subs;

	memset(&subs, 0, sizeof(subs));
	subs.convert_time = 1;
	if (info->format & SND_SEQ_SYNC_TIME)
		subs.realtime = 1;
	subs.sync = 1;
	subs.sender = *src;
	subs.dest.client = SND_SEQ_CLIENT_SYSTEM;
	subs.dest.port = snd_seq_queue_sync_port(queue);
	subs.queue = queue;
	subs.opt.sync_info = *info;
	return snd_seq_subscribe_port(seq, &subs);
}

int snd_seq_reset_sync_slave(snd_seq_t *seq, int queue, snd_seq_addr_t *src)
{
	snd_seq_port_subscribe_t subs;

	memset(&subs, 0, sizeof(subs));
	subs.sync = 1;
	subs.sender = *src;
	subs.dest.client = SND_SEQ_CLIENT_SYSTEM;
	subs.dest.port = snd_seq_queue_sync_port(queue);
	subs.queue = queue;
	return snd_seq_unsubscribe_port(seq, &subs);
}


#endif

/*----------------------------------------------------------------*/

/*
 * create an event cell
 */
snd_seq_event_t *snd_seq_create_event(void)
{
	return (snd_seq_event_t *) calloc(1, sizeof(snd_seq_event_t));
}

/*
 * free an event - only for compatibility
 */
int snd_seq_free_event(snd_seq_event_t *ev ATTRIBUTE_UNUSED)
{
	return 0;
}

/*
 * calculates the (encoded) byte-stream size of the event
 */
ssize_t snd_seq_event_length(snd_seq_event_t *ev)
{
	ssize_t len = sizeof(snd_seq_event_t);

	if (!ev)
		return -EINVAL;
	if (snd_seq_ev_is_variable(ev))
		len += ev->data.ext.len;
	return len;
}

/*----------------------------------------------------------------*/

/*
 * output to sequencer
 */

/*
 * output an event - an event is once expanded on the output buffer.
 * output buffer may be flushed if it becomes full.
 */
int snd_seq_event_output(snd_seq_t *seq, snd_seq_event_t *ev)
{
	int result;

	result = snd_seq_event_output_buffer(seq, ev);
	if (result == -EAGAIN) {
		result = snd_seq_drain_output(seq);
		if (result < 0)
			return result;
		return snd_seq_event_output_buffer(seq, ev);
	}
	return result;
}

/*
 * output an event onto the lib buffer without flushing buffer.
 * returns -EAGAIN if the buffer becomes full.
 */
int snd_seq_event_output_buffer(snd_seq_t *seq, snd_seq_event_t *ev)
{
	int len;

	if (!seq || !ev)
		return -EINVAL;
	len = snd_seq_event_length(ev);
	if (len < 0)
		return -EINVAL;
	if ((size_t) len >= seq->obufsize)
		return -EINVAL;
	if ((seq->obufsize - seq->obufused) < (size_t) len)
		return -EAGAIN;
	memcpy(seq->obuf + seq->obufused, ev, sizeof(snd_seq_event_t));
	seq->obufused += sizeof(snd_seq_event_t);
	if (snd_seq_ev_is_variable(ev)) {
		memcpy(seq->obuf + seq->obufused, ev->data.ext.ptr, ev->data.ext.len);
		seq->obufused += ev->data.ext.len;
	}
	return seq->obufused;
}

/*
 * allocate the temporary buffer
 */
static int alloc_tmpbuf(snd_seq_t *seq, size_t len)
{
	size_t size = ((len + sizeof(snd_seq_event_t) - 1) / sizeof(snd_seq_event_t));
	if (seq->tmpbuf == NULL) {
		if (size > DEFAULT_TMPBUF_SIZE)
			seq->tmpbufsize = size;
		else
			seq->tmpbufsize = DEFAULT_TMPBUF_SIZE;
		seq->tmpbuf = malloc(seq->tmpbufsize * sizeof(snd_seq_event_t));
		if (seq->tmpbuf == NULL)
			return -ENOMEM;
	}  else if (len > seq->tmpbufsize) {
		seq->tmpbuf = realloc(seq->tmpbuf, size * sizeof(snd_seq_event_t));
		if (seq->tmpbuf == NULL)
			return -ENOMEM;
		seq->tmpbufsize = size;
	}
	return 0;
}

/*
 * output an event directly to the sequencer NOT through output buffer.
 */
int snd_seq_event_output_direct(snd_seq_t *seq, snd_seq_event_t *ev)
{
	ssize_t len, result;
	void *buf;

	len = snd_seq_event_length(ev);
	if (len < 0)
		return len;
	else if (len == sizeof(*ev)) {
		buf = ev;
	} else {
		if (alloc_tmpbuf(seq, (size_t)len) < 0)
			return -ENOMEM;
		*seq->tmpbuf = *ev;
		memcpy(seq->tmpbuf + 1, ev->data.ext.ptr, ev->data.ext.len);
		buf = seq->tmpbuf;
	}

	result = write(seq->fd, buf, len);

	return (result < 0) ? -errno : (int)result;
}

/*
 * return the size of pending events on output buffer
 */
int snd_seq_event_output_pending(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	return seq->obufused;
}

/*
 * drain output buffer to sequencer
 */
int snd_seq_drain_output(snd_seq_t *seq)
{
	int result;

	if (!seq)
		return -EINVAL;
	while (seq->obufused > 0) {
		result = write(seq->fd, seq->obuf, seq->obufused);
		if (result < 0)
			return -errno;
		if ((size_t)result < seq->obufused)
			memmove(seq->obuf, seq->obuf + result, seq->obufused - result);
		seq->obufused -= result;
	}
	return 0;
}

/*
 * extract the first event in output buffer
 * if ev_res is NULL, just remove the event.
 */
int snd_seq_extract_output(snd_seq_t *seq, snd_seq_event_t **ev_res)
{
	size_t len, olen;
	snd_seq_event_t ev;

	if (!seq)
		return -EINVAL;
	if (ev_res)
		*ev_res = NULL;
	if ((olen = seq->obufused) < sizeof(snd_seq_event_t))
		return -ENOENT;
	memcpy(&ev, (snd_seq_event_t*)seq->obuf, sizeof(snd_seq_event_t));
	len = snd_seq_event_length(&ev);
	if (ev_res) {
		/* extract the event */
		if (alloc_tmpbuf(seq, len) < 0)
			return -ENOMEM;
		memcpy(seq->tmpbuf, seq->obuf, len);
		*ev_res = seq->tmpbuf;
	}
	seq->obufused = olen - len;
	memmove(seq->obuf, seq->obuf + len, seq->obufused);
	return 0;
}

/*----------------------------------------------------------------*/

/*
 * input from sequencer
 */

/*
 * read from sequencer to input buffer
 */
static ssize_t snd_seq_event_read_buffer(snd_seq_t *seq)
{
	ssize_t len;
	len = read(seq->fd, seq->ibuf, seq->ibufsize * sizeof(snd_seq_event_t));
	if (len < 0)
		return -errno;
	seq->ibuflen = len / sizeof(snd_seq_event_t);
	seq->ibufptr = 0;
	return seq->ibuflen;
}

static int snd_seq_event_retrieve_buffer(snd_seq_t *seq, snd_seq_event_t **retp)
{
	size_t ncells;
	snd_seq_event_t *ev;

	*retp = ev = &seq->ibuf[seq->ibufptr];
	seq->ibufptr++;
	seq->ibuflen--;
	if (! snd_seq_ev_is_variable(ev))
		return 1;
	ncells = (ev->data.ext.len + sizeof(snd_seq_event_t) - 1) / sizeof(snd_seq_event_t);
	if (seq->ibuflen < ncells) {
		seq->ibuflen = 0; /* clear buffer */
		*retp = NULL;
		return -EINVAL;
	}
	ev->data.ext.ptr = ev + 1;
	seq->ibuflen -= ncells;
	seq->ibufptr += ncells;
	return 1;
}

/*
 * retrieve an event from sequencer
 */
int snd_seq_event_input(snd_seq_t *seq, snd_seq_event_t **ev)
{
	int err;

	*ev = NULL;
	if (!seq)
		return -EINVAL;

	if (seq->ibuflen <= 0) {
		if ((err = snd_seq_event_read_buffer(seq)) < 0)
			return err;
	}

	return snd_seq_event_retrieve_buffer(seq, ev);
}

/*
 * read input data from sequencer if available
 */
static int snd_seq_event_input_feed(snd_seq_t *seq, struct timeval *timeout)
{
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(seq->fd, &rfds);
	if (select(seq->fd + 1, &rfds, NULL, NULL, timeout) < 0)
		return -errno;
	if (FD_ISSET(seq->fd, &rfds))
		return snd_seq_event_read_buffer(seq);
	return seq->ibuflen;
}

/*
 * check events in input queue
 */
int snd_seq_event_input_pending(snd_seq_t *seq, int fetch_sequencer)
{
	if (seq->ibuflen == 0 && fetch_sequencer) {
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		return snd_seq_event_input_feed(seq, &tv);
	}
	return seq->ibuflen;
}

/*----------------------------------------------------------------*/

/*
 * clear event buffers
 */

/*
 * clear output buffer
 */
int snd_seq_drop_output_buffer(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	seq->obufused = 0;
	return 0;
}

/*
 * clear input buffer
 */
int snd_seq_drop_input_buffer(snd_seq_t *seq)
{
	if (!seq)
		return -EINVAL;
	seq->ibufptr = 0;
	seq->ibuflen = 0;
	return 0;
}

/*
 * clear output buffer and remove events in sequencer queue
 */
int snd_seq_drop_output(snd_seq_t *seq)
{
	snd_seq_remove_events_t rminfo;

	if (!seq)
		return -EINVAL;

	seq->obufused = 0; /* drain output buffer */

	memset(&rminfo, 0, sizeof(rminfo));
	rminfo.output = 1;

	return snd_seq_remove_events(seq, &rminfo);
}

/*
 * clear input buffer and and remove events in sequencer queue
 */
int snd_seq_drop_input(snd_seq_t *seq)
{
	snd_seq_remove_events_t rminfo;

	if (!seq)
		return -EINVAL;

	seq->ibufptr = 0;	/* drain input buffer */
	seq->ibuflen = 0;

	memset(&rminfo, 0, sizeof(rminfo));
	rminfo.input = 1;

	return snd_seq_remove_events(seq, &rminfo);
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
			res = snd_seq_compare_real_time(&ev->time.time, &info->time.time);
		if (!res)
			return 0;
	}
	if (info->remove_mode & SND_SEQ_REMOVE_TIME_BEFORE) {
		if (info->tick)
			res = snd_seq_compare_tick_time(&ev->time.tick, &info->time.tick);
		else
			res = snd_seq_compare_real_time(&ev->time.time, &info->time.time);
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
		if (rmp->remove_mode == 0)
			snd_seq_drop_input_buffer(seq);
		/* other modes are not supported yet */
	}

	if (rmp->output) {
		/*
		 * First deal with any events that are still buffered
		 * in the library.
		 */
		 if (rmp->remove_mode == 0) {
			 /* The simple case - remove all */
			 snd_seq_drop_output_buffer(seq);
		} else {
			char *ep;
			size_t len;
			snd_seq_event_t *ev;

			ep = seq->obuf;
			while (ep - seq->obuf < (ssize_t)seq->obufused) {

				ev = (snd_seq_event_t *) ep;
				len = snd_seq_event_length(ev);

				if (remove_match(rmp, ev)) {
					/* Remove event */
					seq->obufused -= len;
					memmove(ep, ep + len, seq->obufused - (ep - seq->obuf));
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

/*----------------------------------------------------------------*/

/*
 * client memory pool
 */

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

/*----------------------------------------------------------------*/

/*
 * query functions
 */

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

/*----------------------------------------------------------------*/

/*
 * misc.
 */

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

