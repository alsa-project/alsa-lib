/*
 *  Sequencer Interface - main file
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
 *                        Abramo Bagnara <abramo@alsa-project.org>
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

#include <sys/poll.h>
#include <dlfcn.h>
#include "seq_local.h"

const char *snd_seq_name(snd_seq_t *seq)
{
	assert(seq);
	return seq->name;
}

snd_seq_type_t snd_seq_type(snd_seq_t *seq)
{
	assert(seq);
	return seq->type;
}

int snd_seq_open(snd_seq_t **seqp, const char *name, 
		 int streams, int mode)
{
	const char *str;
	int err;
	snd_config_t *seq_conf, *conf, *type_conf;
	snd_config_iterator_t i, next;
	const char *lib = NULL, *open = NULL;
	int (*open_func)(snd_seq_t **seqp, const char *name, snd_config_t *conf, 
			 int streams, int mode);
	void *h;
	assert(seqp && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	err = snd_config_searchv(snd_config, &seq_conf, "seq", name, 0);
	if (err < 0) {
		if (strcmp(name, "hw") == 0)
			return snd_seq_hw_open(seqp, name, streams, mode);
		ERR("Unknown SEQ %s", name);
		return -ENOENT;
	}
	if (snd_config_get_type(seq_conf) != SND_CONFIG_TYPE_COMPOUND) {
		ERR("Invalid type for SEQ %s definition", name);
		return -EINVAL;
	}
	err = snd_config_search(seq_conf, "type", &conf);
	if (err < 0) {
		ERR("type is not defined");
		return err;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		ERR("Invalid type for %s", snd_config_get_id(conf));
		return err;
	}
	err = snd_config_searchv(snd_config, &type_conf, "seqtype", str, 0);
	if (err < 0) {
		ERR("Unknown SEQ type %s", str);
		return err;
	}
	snd_config_for_each(i, next, type_conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "lib") == 0) {
			err = snd_config_get_string(n, &lib);
			if (err < 0) {
				ERR("Invalid type for %s", id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(id, "open") == 0) {
			err = snd_config_get_string(n, &open);
			if (err < 0) {
				ERR("Invalid type for %s", id);
				return -EINVAL;
			}
			continue;
			ERR("Unknown field %s", id);
			return -EINVAL;
		}
	}
	if (!open) {
		ERR("open is not defined");
		return -EINVAL;
	}
	if (!lib)
		lib = "libasound.so";
	h = dlopen(lib, RTLD_NOW);
	if (!h) {
		ERR("Cannot open shared library %s", lib);
		return -ENOENT;
	}
	open_func = dlsym(h, open);
	dlclose(h);
	if (!open_func) {
		ERR("symbol %s is not defined inside %s", open, lib);
		return -ENXIO;
	}
	return open_func(seqp, name, seq_conf, streams, mode);
}

/*
 * release sequencer client
 */
int snd_seq_close(snd_seq_t *seq)
{
	int err;
	assert(seq);
	err = seq->ops->close(seq);
	if (err < 0)
		return err;
	if (seq->obuf)
		free(seq->obuf);
	if (seq->ibuf)
		free(seq->ibuf);
	if (seq->tmpbuf)
		free(seq->tmpbuf);
	if (seq->name)
		free(seq->name);
	free(seq);
	return 0;
}

/*
 * returns the file descriptor of the client
 */
int _snd_seq_poll_descriptor(snd_seq_t *seq)
{
	assert(seq);
	return seq->poll_fd;
}

int snd_seq_poll_descriptors(snd_seq_t *seq, struct pollfd *pfds, unsigned int space)
{
	assert(seq);
	if (space >= 1) {
		pfds->fd = seq->poll_fd;
		pfds->events = 0;
		if (seq->streams & SND_SEQ_OPEN_INPUT)
			pfds->events |= POLLIN;
		if (seq->streams & SND_SEQ_OPEN_OUTPUT)
			pfds->events |= POLLOUT;
	}
	return 1;
}

/*
 * set blocking behavior
 */
int snd_seq_nonblock(snd_seq_t *seq, int nonblock)
{
	int err;
	assert(seq);
	err = seq->ops->nonblock(seq, nonblock);
	if (err < 0)
		return err;
	if (nonblock)
		seq->mode |= SND_SEQ_NONBLOCK;
	else
		seq->mode &= ~SND_SEQ_NONBLOCK;
	return 0;
}

/*
 * return the client id
 */
int snd_seq_client_id(snd_seq_t *seq)
{
	assert(seq);
	return seq->client;
}

/*
 * return buffer size
 */
int snd_seq_output_buffer_size(snd_seq_t *seq)
{
	assert(seq);
	if (!seq->obuf)
		return 0;
	return seq->obufsize;
}

/*
 * return buffer size
 */
int snd_seq_input_buffer_size(snd_seq_t *seq)
{
	assert(seq);
	if (!seq->ibuf)
		return 0;
	return seq->ibufsize * sizeof(snd_seq_event_t);
}

/*
 * resize output buffer
 */
int snd_seq_resize_output_buffer(snd_seq_t *seq, size_t size)
{
	assert(seq && seq->obuf);
	assert(size >= sizeof(snd_seq_event_t));
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
	assert(seq && seq->ibuf);
	assert(size >= sizeof(snd_seq_event_t));
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
	assert(seq && info);
	return seq->ops->system_info(seq, info);
}

/*
 * obtain the information of given client
 */
int snd_seq_get_any_client_info(snd_seq_t *seq, int client, snd_seq_client_info_t * info)
{
	assert(seq && info && client >= 0);
	memset(info, 0, sizeof(snd_seq_client_info_t));
	info->client = client;
	return seq->ops->get_client_info(seq, info);
}

/*
 * obtain the current client information
 */
int snd_seq_get_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	return snd_seq_get_any_client_info(seq, seq->client, info);
}

/*
 * set the current client information
 */
int snd_seq_set_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	assert(seq && info);
	info->client = seq->client;
	info->type = USER_CLIENT;
	return seq->ops->set_client_info(seq, info);
}

/*----------------------------------------------------------------*/

/*
 * sequencer port handlers
 */

int snd_seq_create_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	assert(seq && port);
	port->client = seq->client;
	return seq->ops->create_port(seq, port);
}

int snd_seq_delete_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	assert(seq && port);
	port->client = seq->client;
	return seq->ops->delete_port(seq, port);
}

int snd_seq_get_any_port_info(snd_seq_t *seq, int client, int port, snd_seq_port_info_t * info)
{
	assert(seq && info && client >= 0 && port >= 0);
	memset(info, 0, sizeof(snd_seq_port_info_t));
	info->client = client;
	info->port = port;
	return seq->ops->get_port_info(seq, info);
}

int snd_seq_get_port_info(snd_seq_t *seq, int port, snd_seq_port_info_t * info)
{
	return snd_seq_get_any_port_info(seq, seq->client, port, info);
}

int snd_seq_set_port_info(snd_seq_t *seq, int port, snd_seq_port_info_t * info)
{
	assert(seq && info && port >= 0);
	info->port = port;
	return seq->ops->set_port_info(seq, info);
}

/*----------------------------------------------------------------*/

/*
 * subscription
 */

int snd_seq_get_port_subscription(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	assert(seq && sub);
	return seq->ops->get_port_subscription(seq, sub);
}

int snd_seq_subscribe_port(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	assert(seq && sub);
	return seq->ops->subscribe_port(seq, sub);
}

int snd_seq_unsubscribe_port(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	assert(seq && sub);
	return seq->ops->unsubscribe_port(seq, sub);
}

int snd_seq_query_port_subscribers(snd_seq_t *seq, snd_seq_query_subs_t * subs)
{
	assert(seq && subs);
	return seq->ops->query_port_subscribers(seq, subs);
}

/*----------------------------------------------------------------*/

/*
 * queue handlers
 */

int snd_seq_get_queue_status(snd_seq_t *seq, int q, snd_seq_queue_status_t * status)
{
	assert(seq && status);
	memset(status, 0, sizeof(snd_seq_queue_status_t));
	status->queue = q;
	return seq->ops->get_queue_status(seq, status);
}

int snd_seq_get_queue_tempo(snd_seq_t *seq, int q, snd_seq_queue_tempo_t * tempo)
{
	assert(seq && tempo);
	memset(tempo, 0, sizeof(snd_seq_queue_tempo_t));
	tempo->queue = q;
	return seq->ops->get_queue_tempo(seq, tempo);
}

int snd_seq_set_queue_tempo(snd_seq_t *seq, int q, snd_seq_queue_tempo_t * tempo)
{
	assert(seq && tempo);
	tempo->queue = q;
	return seq->ops->set_queue_tempo(seq, tempo);
}

int snd_seq_get_queue_owner(snd_seq_t *seq, int q, snd_seq_queue_owner_t * owner)
{
	assert(seq && owner);
	memset(owner, 0, sizeof(snd_seq_queue_owner_t));
	owner->queue = q;
	return seq->ops->get_queue_owner(seq, owner);
}

int snd_seq_set_queue_owner(snd_seq_t *seq, int q, snd_seq_queue_owner_t * owner)
{
	assert(seq && owner);
	owner->queue = q;
	return seq->ops->set_queue_owner(seq, owner);
}

int snd_seq_get_queue_timer(snd_seq_t *seq, int q, snd_seq_queue_timer_t * timer)
{
	assert(seq && timer);
	memset(timer, 0, sizeof(snd_seq_queue_timer_t));
	timer->queue = q;
	return seq->ops->get_queue_timer(seq, timer);
}

int snd_seq_set_queue_timer(snd_seq_t *seq, int q, snd_seq_queue_timer_t * timer)
{
	assert(seq && timer);
	timer->queue = q;
	return seq->ops->set_queue_timer(seq, timer);
}

int snd_seq_get_queue_client(snd_seq_t *seq, int q, snd_seq_queue_client_t * info)
{
	assert(seq && info);
	memset(info, 0, sizeof(snd_seq_queue_client_t));
	info->queue = q;
	info->client = seq->client;
	return seq->ops->get_queue_client(seq, info);
}

int snd_seq_set_queue_client(snd_seq_t *seq, int q, snd_seq_queue_client_t * info)
{
	assert(seq && info);
	info->queue = q;
	info->client = seq->client;
	return seq->ops->set_queue_client(seq, info);
}

int snd_seq_create_queue(snd_seq_t *seq, snd_seq_queue_info_t *info)
{
	int err;
	assert(seq && info);
	info->owner = seq->client;
	err = seq->ops->create_queue(seq, info);
	if (err < 0)
		return err;
	return info->queue;
}

int snd_seq_alloc_named_queue(snd_seq_t *seq, const char *name)
{
	snd_seq_queue_info_t info;
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
	assert(seq);
	memset(&info, 0, sizeof(info));
	info.queue = q;
	return seq->ops->delete_queue(seq, &info);
}

int snd_seq_get_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info)
{
	assert(seq && info);
	info->queue = q;
	return seq->ops->get_queue_info(seq, info);
}

int snd_seq_set_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info)
{
	assert(seq && info);
	info->queue = q;
	return seq->ops->set_queue_info(seq, info);
}

int snd_seq_get_named_queue(snd_seq_t *seq, const char *name)
{
	int err;
	snd_seq_queue_info_t info;
	assert(seq && name);
	strncpy(info.name, name, sizeof(info.name));
	err = seq->ops->get_named_queue(seq, &info);
	if (err < 0)
		return err;
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
	assert(ev);
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
	assert(seq && ev);
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
	ssize_t len;
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

	return seq->ops->write(seq, buf, len);
}

/*
 * return the size of pending events on output buffer
 */
int snd_seq_event_output_pending(snd_seq_t *seq)
{
	assert(seq);
	return seq->obufused;
}

/*
 * drain output buffer to sequencer
 */
int snd_seq_drain_output(snd_seq_t *seq)
{
	ssize_t result;
	assert(seq);
	while (seq->obufused > 0) {
		result = seq->ops->write(seq, seq->obuf, seq->obufused);
		if (result < 0)
			return -result;
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
	assert(seq);
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
	len = seq->ops->read(seq, seq->ibuf, seq->ibufsize * sizeof(snd_seq_event_t));
	if (len < 0)
		return len;
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
	assert(seq);
	*ev = NULL;
	if (seq->ibuflen <= 0) {
		if ((err = snd_seq_event_read_buffer(seq)) < 0)
			return err;
	}

	return snd_seq_event_retrieve_buffer(seq, ev);
}

/*
 * read input data from sequencer if available
 */
static int snd_seq_event_input_feed(snd_seq_t *seq, int timeout)
{
	struct pollfd pfd;
	int err;
	pfd.fd = seq->poll_fd;
	pfd.events = POLLIN;
	err = poll(&pfd, 1, timeout);
	if (err < 0) {
		SYSERR("poll");
		return -errno;
	}
	if (pfd.revents & POLLIN) 
		return snd_seq_event_read_buffer(seq);
	return seq->ibuflen;
}

/*
 * check events in input queue
 */
int snd_seq_event_input_pending(snd_seq_t *seq, int fetch_sequencer)
{
	if (seq->ibuflen == 0 && fetch_sequencer) {
		return snd_seq_event_input_feed(seq, 0);
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
	assert(seq);
	seq->obufused = 0;
	return 0;
}

/*
 * clear input buffer
 */
int snd_seq_drop_input_buffer(snd_seq_t *seq)
{
	assert(seq);
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
	assert(seq);
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
	assert(seq);

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

	return seq->ops->remove_events(seq, rmp);
}

/*----------------------------------------------------------------*/

/*
 * client memory pool
 */

int snd_seq_get_client_pool(snd_seq_t *seq, snd_seq_client_pool_t *info)
{
	assert(seq && info);
	info->client = seq->client;
	return seq->ops->get_client_pool(seq, info);
}

int snd_seq_set_client_pool(snd_seq_t *seq, snd_seq_client_pool_t *info)
{
	assert(seq && info);
	info->client = seq->client;
	return seq->ops->set_client_pool(seq, info);
}

/*----------------------------------------------------------------*/

/*
 * query functions
 */

int snd_seq_query_next_client(snd_seq_t *seq, snd_seq_client_info_t *info)
{
	assert(seq && info);
	return seq->ops->query_next_client(seq, info);
}

int snd_seq_query_next_port(snd_seq_t *seq, snd_seq_port_info_t *info)
{
	assert(seq && info);
	return seq->ops->query_next_port(seq, info);
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

