/**
 * \file seq/seq.c
 * \author Jaroslav Kysela <perex@suse.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 2000-2001
 *
 * THE SEQUENCER INTERFACE WILL BE CHANGED IN NEAR FUTURE.
 * This interface is still not compliant to alsa 1.0 encapsulation.
 * 
 */

/* 
 *  Sequencer Interface - main file
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

/**
 * \brief get identifier of sequencer handle
 * \param seq sequencer handle
 * \return ascii identifier of sequencer handle
 *
 * Returns the ASCII identifier of the given sequencer handle. It's the same
 * identifier specified in snd_seq_open().
 */
const char *snd_seq_name(snd_seq_t *seq)
{
	assert(seq);
	return seq->name;
}

/**
 * \brief get type of sequencer handle
 * \param seq sequencer handle
 * \return type of sequencer handle
 *
 * Returns the type #snd_seq_type_t of the given sequencer handle.
 */
snd_seq_type_t snd_seq_type(snd_seq_t *seq)
{
	assert(seq);
	return seq->type;
}

static int snd_seq_open_conf(snd_seq_t **seqp, const char *name,
			     snd_config_t *seq_conf,
			     int streams, int mode)
{
	const char *str;
	char buf[256];
	int err;
	snd_config_t *conf, *type_conf = NULL;
	snd_config_iterator_t i, next;
	const char *lib = NULL, *open_name = NULL;
	int (*open_func)(snd_seq_t **, const char *, snd_config_t *, 
			 int, int);
	void *h;
	if (snd_config_get_type(seq_conf) != SND_CONFIG_TYPE_COMPOUND) {
		if (name)
			SNDERR("Invalid type for SEQ %s definition", name);
		else
			SNDERR("Invalid type for SEQ definition");
		return -EINVAL;
	}
	err = snd_config_search(seq_conf, "type", &conf);
	if (err < 0) {
		SNDERR("type is not defined");
		return err;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", snd_config_get_id(conf));
		return err;
	}
	err = snd_config_search_definition(snd_config, "seq_type", str, &type_conf);
	if (err >= 0) {
		if (snd_config_get_type(type_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for SEQ type %s definition", str);
			goto _err;
		}
		snd_config_for_each(i, next, type_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			if (strcmp(id, "open") == 0) {
				err = snd_config_get_string(n, &open_name);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
		_err:
			snd_config_delete(type_conf);
			return -EINVAL;
		}
	}
	if (!open_name) {
		open_name = buf;
		snprintf(buf, sizeof(buf), "_snd_seq_%s_open", str);
	}
	if (!lib)
		lib = ALSA_LIB;
	h = dlopen(lib, RTLD_NOW);
	if (h)
		open_func = dlsym(h, open_name);
	if (type_conf)
		snd_config_delete(type_conf);
	if (!h) {
		SNDERR("Cannot open shared library %s", lib);
		return -ENOENT;
	}
	if (!open_func) {
		SNDERR("symbol %s is not defined inside %s", open_name, lib);
		dlclose(h);
		return -ENXIO;
	}
	return open_func(seqp, name, seq_conf, streams, mode);
}

static int snd_seq_open_noupdate(snd_seq_t **seqp, snd_config_t *root,
				 const char *name, int streams, int mode)
{
	int err;
	snd_config_t *seq_conf;
	err = snd_config_search_definition(root, "seq", name, &seq_conf);
	if (err < 0) {
		SNDERR("Unknown SEQ %s", name);
		return err;
	}
	err = snd_seq_open_conf(seqp, name, seq_conf, streams, mode);
	snd_config_delete(seq_conf);
	return err;
}


/**
 * \brief Open the ALSA sequencer
 *
 * \param seqp Pointer to a snd_seq_t pointer.  This pointer must be
 * kept and passed to most of the other sequencer functions.
 * \param name The sequencer's "name".  This is \em not a name you make
 * up for your own purposes; it has special significance to the ALSA
 * library.  So far only \c "hw" type is supported.
 * Just pass \c "hw" for this.
 * \param streams The read/write mode of the sequencer.  Can be one of
 * three values:
 * - \c SND_SEQ_OPEN_OUTPUT - open the sequencer for output only
 * - \c SND_SEQ_OPEN_INPUT - open the sequencer for input only
 * - \c SND_SEQ_OPEN_DUPLEX - open the sequencer for output and input
 * I suppose that you'd always want to use \c SND_SEQ_OPEN_DUPLEX here;
 * I can't think of a reason for using either of the other two.
 * \note Internally, these are translated to \c O_WRONLY, \c O_RDONLY and
 * \O_RDWR respectively and used as the second argument to the C library
 * open() call.
 * \param mode Optional modifier.  Can be either 0, or \c
 * SND_SEQ_NONBLOCK, which will make read/write operations
 * non-blocking.  This can also be set later using snd_seq_nonblock().
 * \return 0 on success otherwise a negative error code
 *
 * Creates a new handle and opens a connection to the kernel
 * sequencer interface.
 * After a client is created successfully, an event
 * with \c SND_SEQ_EVENT_CLIENT_START is broadcasted to announce port.
 */
int snd_seq_open(snd_seq_t **seqp, const char *name, 
		 int streams, int mode)
{
	int err;
	assert(seqp && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	return snd_seq_open_noupdate(seqp, snd_config, name, streams, mode);
}

/**
 * \brief Close the sequencer
 * \param handle Handle returned from snd_seq_open()
 * \return 0 on success otherwise a negative error code
 *
 * Closes the sequencer client and releases its resources.
 * After a client is closed, an event with
 * \c SND_SEQ_EVENT_CLIENT_EXIT is broadcasted to announce port.
 * The connection between other clients are disconnected.
 * Call this just before exiting your program.
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

/**
 * \brief Returns the number of poll descriptors
 * \param seq sequencer handle
 * \param events the poll events to be checked (\c POLLIN and \c POLLOUT)
 * \return the number of poll descriptors.
 *
 * Get the number of poll descriptors.  The polling events to be checked
 * can be specifed by the second argument.  When both input and output
 * are checked, pass \c POLLIN|POLLOUT
 */
int snd_seq_poll_descriptors_count(snd_seq_t *seq, short events)
{
	int result = 0;
	assert(seq);
	if (events & POLLIN) {
		assert(seq->streams & SND_SEQ_OPEN_INPUT);
		result++;
	}
	if (events & POLLOUT) {
		assert(seq->streams & SND_SEQ_OPEN_OUTPUT);
		result++;
	}
	return result ? 1 : 0;
}

/**
 * \brief get poll descriptors
 * \param seq sequencer handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \param events polling events to be checked (\c POLLIN and \c POLLOUT)
 * \return count of filled descriptors
 *
 * Get poll descriptors assigned to the sequencer handle.
 */
int snd_seq_poll_descriptors(snd_seq_t *seq, struct pollfd *pfds, unsigned int space, short events)
{
	short revents = 0;

	assert(seq);
	if ((events & POLLIN) && space >= 1) {
		assert(seq->streams & SND_SEQ_OPEN_INPUT);
		revents |= POLLIN;
	}
	if ((events & POLLOUT) && space >= 1) {
		assert(seq->streams & SND_SEQ_OPEN_INPUT);
		revents |= POLLOUT;
	}
	if (!revents)
		return 0;
	pfds->fd = seq->poll_fd;
	pfds->events = revents;
	return 1;
}

/**
 * \brief set nonblock mode
 * \param seq sequencer handle
 * \param nonblock 0 = block, 1 = nonblock mode
 * \return 0 on success otherwise a negative error code
 *
 * Change the blocking mode of the given client.
 * In block mode, the client falls into sleep when it fills the
 * output memory pool with full events.  The client will be woken up
 *after a certain amount of free space becomes available.
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

/**
 * \brief get the client id
 * \param seq sequencer handle
 * \return the client id
 *
 * Returns the id of the specified client.
 * If an error occurs, function returns the negative error code.
 * A client id is necessary to inquiry or to set the client information.
 * A user client is assigned from 128 to 191.
 */
int snd_seq_client_id(snd_seq_t *seq)
{
	assert(seq);
	return seq->client;
}

/**
 * \brief return the size of output buffer
 * \param seq sequencer handle
 * \return the size of output buffer in bytes
 *
 * Obtains the size of output buffer.
 * This buffer is used to store decoded byte-stream of output events
 * before transferring to sequencer.
 */
int snd_seq_output_buffer_size(snd_seq_t *seq)
{
	assert(seq);
	if (!seq->obuf)
		return 0;
	return seq->obufsize;
}

/**
 * \brief return the size of input buffer
 * \param seq sequencer handle
 * \return the size of input buffer in bytes
 *
 * Obtains the size of input buffer.
 * This buffer is used to read byte-stream of input events from sequencer.
 */
int snd_seq_input_buffer_size(snd_seq_t *seq)
{
	assert(seq);
	if (!seq->ibuf)
		return 0;
	return seq->ibufsize * sizeof(snd_seq_event_t);
}

/**
 * \brief change the size of output buffer
 * \param seq sequencer handle
 * \param size the size of output buffer to be changed in bytes
 * \return 0 on success otherwise a negative error code
 *
 * Changes the size of output buffer.
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

/**
 * \brief resize the input buffer
 * \param seq sequencer handle
 * \param size the size of input buffer to be changed in bytes
 * \return 0 on success otherwise a negative error code
 *
 * Changes the size of input buffer.
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

/**
 * \brief obtain the sequencer system information
 * \param seq sequencer handle
 * \param info the pointer to be stored
 * \return 0 on success otherwise a negative error code
 *
 * Stores the global system information of ALSA sequencer system.
 * The returned data contains
 * the maximum available numbers of queues, clients, ports and channels.
 */
int snd_seq_system_info(snd_seq_t *seq, snd_seq_system_info_t * info)
{
	assert(seq && info);
	return seq->ops->system_info(seq, info);
}

/**
 * \brief obtain the information of the given client
 * \param seq sequencer handle
 * \param client client id
 * \param info the pointer to be stored
 * \return 0 on success otherwise a negative error code
 * 
 * Obtains the information of the client with a client id specified by
 * info argument.
 * The obtained information is written on info parameter.
 */
int snd_seq_get_any_client_info(snd_seq_t *seq, int client, snd_seq_client_info_t * info)
{
	assert(seq && info && client >= 0);
	memset(info, 0, sizeof(snd_seq_client_info_t));
	info->client = client;
	return seq->ops->get_client_info(seq, info);
}

/**
 * \brief obtain the current client information
 * \param seq sequencer handle
 * \param info the pointer to be stored
 * \return 0 on success otherwise a negative error code
 *
 * Obtains the information of the current client stored on info.
 * client and type fields are ignored.
 */
int snd_seq_get_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	return snd_seq_get_any_client_info(seq, seq->client, info);
}

/**
 * \brief set the current client information
 * \param seq sequencer handle
 * \param info the client info data to set
 * \return 0 on success otherwise a negative error code
 *
 * Obtains the information of the current client stored on info.
 * client and type fields are ignored.
 */
int snd_seq_set_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	assert(seq && info);
	info->client = seq->client;
	info->type = USER_CLIENT;
	return seq->ops->set_client_info(seq, info);
}

/*----------------------------------------------------------------*/

/**
 * \brief create a sequencer port on the current client
 * \param seq sequencer handle
 * \param port port information for the new port
 * \return 0 on success otherwise a negative error code
 *
 * Creates a sequencer port on the current client.
 * The attributes of created port is specified in info argument.
 *
 * The client field in info argument is overwritten with the current client id.
 * Behavior of port creation depends on a flag defined in
 * flags field. The flags field is a bit mask containing
 * miscellaneous conditions.
 * If \c SND_SEQ_PORT_FLG_GIVEN_PORT is included in flags field,
 * the port number in port field in info argument
 * is used as the id of created port.
 * Otherwise, the first empty port id is searched and used.
 * The obtained id index of the created port is stored on
 * port field in return.
 *
 * The capability and cap_group are bit-masks to specify the access
 * capability of the port from other clients and from the same group,
 * respectively.  The capability bit flags are defined as follows:
 * - \c SND_SEQ_PORT_CAP_READ Readable from this port
 * - \c SND_SEQ_PORT_CAP_WRITE Writable to this port.
 * - \c SND_SEQ_PORT_CAP_SYNC_READ For synchronization (not implemented)
 * - \c SND_SEQ_PORT_CAP_SYNC_WRITE For synchronization (not implemented)
 * - \c SND_SEQ_PORT_CAP_DUPLEX Read/write duplex access is supported
 * - \c SND_SEQ_PORT_SUBS_READ Read subscription is allowed
 * - \c SND_SEQ_PORT_SUBS_WRITE Write subscription is allowed
 * - \c SND_SEQ_PORT_SUBS_NO_EXPORT Subscription management from 3rd client is disallowed
 *
 * The type field is used to specify the type of the port.
 * It is a bitmask defined as follows:
 * - \c SND_SEQ_PORT_TYPE_SPECIFIC Hardware specific port
 * - \c SND_SEQ_PORT_TYPE_MIDI_GENERIC Generic MIDI device
 * - \c SND_SEQ_PORT_TYPE_MIDI_GM General MIDI compatible device
 * - \c SND_SEQ_PORT_TYPE_MIDI_GS GS compatible device
 * - \c SND_SEQ_PORT_TYPE_MIDI_XG XG compatible device
 * - \c SND_SEQ_PORT_TYPE_MIDI_MT32 MT-32 compatible device
 * - \c SND_SEQ_PORT_TYPE_SYNTH Synth device
 * - \c SND_SEQ_PORT_TYPE_DIRECT_SAMPLE Sampling device (supporting download)
 * - \c SND_SEQ_PORT_TYPE_SAMPLE Sampling device (sample can be downloaded at any time)
 * - \c SND_SEQ_PORT_TYPE_APPLICATION Application (suquencer/editor)
 *
 * The midi_channels, midi_voices and synth_voices fields are number of channels and
 * voices of this port.  These values could be zero as default.
 * The read_use, write_use and kernel fields are at creation.
 * They should be zero-cleared.
 */
int snd_seq_create_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	assert(seq && port);
	port->client = seq->client;
	return seq->ops->create_port(seq, port);
}

/**
 * \brief delete a sequencer port on the current client
 * \param seq sequencer handle
 * \param port port to be deleted
 * \return 0 on success otherwise a negative error code
 *
 * Deletes the existing sequencer port on the current client.
 * The port id must be specified in port field in info argument.
 * The client field is ignored.
 */
int snd_seq_delete_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	assert(seq && port);
	port->client = seq->client;
	return seq->ops->delete_port(seq, port);
}

/**
 * \brief obatin the information of a port on an arbitrary client
 * \param seq sequencer handle
 * \param client client id to get
 * \param port port id to get
 * \param info pointer information returns
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_get_any_port_info(snd_seq_t *seq, int client, int port, snd_seq_port_info_t * info)
{
	assert(seq && info && client >= 0 && port >= 0);
	memset(info, 0, sizeof(snd_seq_port_info_t));
	info->client = client;
	info->port = port;
	return seq->ops->get_port_info(seq, info);
}

/**
 * \brief obatin the information of a port on the current client
 * \param seq sequencer handle
 * \param port port id to get
 * \param info pointer information returns
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_get_port_info(snd_seq_t *seq, int port, snd_seq_port_info_t * info)
{
	return snd_seq_get_any_port_info(seq, seq->client, port, info);
}

/**
 * \brief set the information of a port on the current client
 * \param seq sequencer handle
 * \param port port to be set
 * \param info port information to be set
 * \return 0 on success otherwise a negative error code
 */
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

/**
 * \brief obtain subscription information
 * \param seq sequencer handle
 * \param sub pointer to return the subscription information
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_get_port_subscription(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	assert(seq && sub);
	return seq->ops->get_port_subscription(seq, sub);
}

/**
 * \brief subscribe a port connection
 * \param seq sequencer handle
 * \param sub subscription information
 * \return 0 on success otherwise a negative error code
 *
 * Subscribes a connection between two ports.
 * The subscription information is stored in sub argument.
 */
int snd_seq_subscribe_port(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	assert(seq && sub);
	return seq->ops->subscribe_port(seq, sub);
}

/**
 * \brief unsubscribe a connection between ports
 * \param seq sequencer handle
 * \param sub subscription information to disconnect
 * \return 0 on success otherwise a negative error code
 *
 * Unsubscribes a connection between two ports,
 * described in sender and dest fields in sub argument.
 */
int snd_seq_unsubscribe_port(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	assert(seq && sub);
	return seq->ops->unsubscribe_port(seq, sub);
}

/**
 * \brief query port subscriber list
 * \param seq sequencer handle
 * \param subs subscription to query
 * \return 0 on success otherwise a negative error code
 *
 * Queries the subscribers accessing to a port.
 * The query information is specified in subs argument.
 *
 */
int snd_seq_query_port_subscribers(snd_seq_t *seq, snd_seq_query_subs_t * subs)
{
	assert(seq && subs);
	return seq->ops->query_port_subscribers(seq, subs);
}

/*----------------------------------------------------------------*/

/*
 * queue handlers
 */

/**
 * \brief obtain the running state of the queue
 * \param seq sequencer handle
 * \param q queue id to query
 * \param status pointer to store the current status
 * \return 0 on success otherwise a negative error code
 *
 * Obtains the running state of the specified queue q.
 */
int snd_seq_get_queue_status(snd_seq_t *seq, int q, snd_seq_queue_status_t * status)
{
	assert(seq && status);
	memset(status, 0, sizeof(snd_seq_queue_status_t));
	status->queue = q;
	return seq->ops->get_queue_status(seq, status);
}

/**
 * \brief obtain the current tempo of the queue
 * \param seq sequencer handle
 * \param q queue id to be queried
 * \param tempo pointer to store the current tempo
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_get_queue_tempo(snd_seq_t *seq, int q, snd_seq_queue_tempo_t * tempo)
{
	assert(seq && tempo);
	memset(tempo, 0, sizeof(snd_seq_queue_tempo_t));
	tempo->queue = q;
	return seq->ops->get_queue_tempo(seq, tempo);
}

/**
 * \brief set the tempo of the queue
 * \param seq sequencer handle
 * \param q queue id to change the tempo
 * \param tempo tempo information
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_set_queue_tempo(snd_seq_t *seq, int q, snd_seq_queue_tempo_t * tempo)
{
	assert(seq && tempo);
	tempo->queue = q;
	return seq->ops->set_queue_tempo(seq, tempo);
}

/**
 * \brief obtain the owner information of the queue
 * \param seq sequencer handle
 * \param q queue id to query
 * \param owner pointer to store the owner information
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_get_queue_owner(snd_seq_t *seq, int q, snd_seq_queue_owner_t * owner)
{
	assert(seq && owner);
	memset(owner, 0, sizeof(snd_seq_queue_owner_t));
	owner->queue = q;
	return seq->ops->get_queue_owner(seq, owner);
}

/**
 * \biref set the owner information of the queue
 * \param seq sequencer handle
 * \param q queue id to change the ownership
 * \param owner owner information
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_set_queue_owner(snd_seq_t *seq, int q, snd_seq_queue_owner_t * owner)
{
	assert(seq && owner);
	owner->queue = q;
	return seq->ops->set_queue_owner(seq, owner);
}

/**
 * \brief obtain the queue timer information
 * \param seq sequencer handle
 * \param q queue id to query
 * \param timer pointer to store the timer information
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_get_queue_timer(snd_seq_t *seq, int q, snd_seq_queue_timer_t * timer)
{
	assert(seq && timer);
	memset(timer, 0, sizeof(snd_seq_queue_timer_t));
	timer->queue = q;
	return seq->ops->get_queue_timer(seq, timer);
}

/**
 * \brief set the queue timer information
 * \param seq sequencer handle
 * \param q queue id to change the timer
 * \param timer timer information
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_set_queue_timer(snd_seq_t *seq, int q, snd_seq_queue_timer_t * timer)
{
	assert(seq && timer);
	timer->queue = q;
	return seq->ops->set_queue_timer(seq, timer);
}

/**
 * \brief obtain queue access information
 * \param seq sequencer handle
 * \param q queue id to query
 * \param info pointer to store the queue access information
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_get_queue_client(snd_seq_t *seq, int q, snd_seq_queue_client_t * info)
{
	assert(seq && info);
	memset(info, 0, sizeof(snd_seq_queue_client_t));
	info->queue = q;
	info->client = seq->client;
	return seq->ops->get_queue_client(seq, info);
}

/**
 * \brief set queue access information
 * \param seq sequencer handle
 * \param q queue id to change access information
 * \param info access information
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_set_queue_client(snd_seq_t *seq, int q, snd_seq_queue_client_t * info)
{
	assert(seq && info);
	info->queue = q;
	info->client = seq->client;
	return seq->ops->set_queue_client(seq, info);
}

/**
 * \brief create a queue
 * \param seq sequencer handle
 * \param info queue information to initialize
 * \return the queue id (zero or positive) on success otherwise a negative error code
 */
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

/**
 * \brief allocate a queue with the speicified name
 * \param seq sequencer handle
 * \param name the name of the new queue
 * \return the queue id (zero or positive) on success otherwise a negative error code
 */ 
int snd_seq_alloc_named_queue(snd_seq_t *seq, const char *name)
{
	snd_seq_queue_info_t info;
	memset(&info, 0, sizeof(info));
	info.locked = 1;
	if (name)
		strncpy(info.name, name, sizeof(info.name) - 1);
	return snd_seq_create_queue(seq, &info);
}

/**
 * \brief allocate a queue
 * \param seq sequencer handle
 * \return the queue id (zero or positive) on success otherwise a negative error code
 */ 
int snd_seq_alloc_queue(snd_seq_t *seq)
{
	return snd_seq_alloc_named_queue(seq, NULL);
}

#ifdef SND_SEQ_SYNC_SUPPORT
/**
 * \brief allocate a synchronizable queue
 * \param seq sequencer handle
 * \param name the name of the new queue
 * \return the queue id (zero or positive) on success otherwise a negative error code
 */
int snd_seq_alloc_sync_queue(snd_seq_t *seq, const char *name)
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

/**
 * \breif delete the specified queue
 * \param seq sequencer handle
 * \param q queue id to delete
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_free_queue(snd_seq_t *seq, int q)
{
	snd_seq_queue_info_t info;
	assert(seq);
	memset(&info, 0, sizeof(info));
	info.queue = q;
	return seq->ops->delete_queue(seq, &info);
}

/**
 * \brief obtain queue attributes
 * \param seq sequencer handle
 * \param q queue id to query
 * \param info information returned
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_get_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info)
{
	assert(seq && info);
	info->queue = q;
	return seq->ops->get_queue_info(seq, info);
}

/**
 * \brief change the queue attributes
 * \param seq sequencer handle
 * \param q queue id to change
 * \param info information changed
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_set_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info)
{
	assert(seq && info);
	info->queue = q;
	return seq->ops->set_queue_info(seq, info);
}

/**
 * \brief query the queue with the specified name
 * \param seq sequencer handle
 * \param name name to query
 * \return the queue id if found or a negative error code
 */
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

/**
 * \brief add sync master
 * \param seq sequencer handle
 * \param queue queue id to change
 * \param dest destination of the sync slave
 * \param info sync information
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

/**
 * \brief add a standard sync master
 * \param seq sequencer handle
 * \param queue queue id
 * \param dest destination of the sync slave
 * \param format sync format
 * \param time_format time format
 * \param optinfo optional information
 */
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

/**
 * \brief remove the specified sync master
 */
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


/**
 * \brief set the sync slave mode
 */
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

/**
 * \brief reset the sync slave mode
 */
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

/**
 * \brief create an event cell
 * \return the cell pointer allocated
 */
snd_seq_event_t *snd_seq_create_event(void)
{
	return (snd_seq_event_t *) calloc(1, sizeof(snd_seq_event_t));
}

/**
 * \brief free an event
 *
 * this is obsolete.  only for compatibility
 */
int snd_seq_free_event(snd_seq_event_t *ev ATTRIBUTE_UNUSED)
{
	return 0;
}

/**
 * \brief calculates the (encoded) byte-stream size of the event
 * \param ev the event
 * \return the size of decoded bytes
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

/**
 * \brief output an event
 * \param seq sequencer handle
 * \param ev event to be output
 * \return the number of remaining events or a negative error code
 *
 * An event is once expanded on the output buffer.
 * output buffer may be flushed if it becomes full.
 *
 * If events remain unprocessed on output buffer before flush,
 * the size of total byte data on output buffer is returned.
 * If the output buffer is empty, this returns zero.
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

/**
 * \brief output an event onto the lib buffer without flushing buffer
 * \param seq sequencer handle
 * \param ev event ot be output
 * \return the number of remaining events. \c -EAGAIN if the buffer becomes full.
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

/**
 * \brief output an event directly to the sequencer NOT through output buffer
 * \param seq sequencer handle
 * \param ev event to be output
 * \return the number of remaining events or a negative error code
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
	return seq->ops->write(seq, buf, (size_t) len);
}

/**
 * \brief return the size of pending events on output buffer
 * \param seq sequencer handle
 * \return the number of pending events
 */
int snd_seq_event_output_pending(snd_seq_t *seq)
{
	assert(seq);
	return seq->obufused;
}

/**
 * \brief drain output buffer to sequencer
 * \param seq sequencer handle
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

/**
 * \brief extract the first event in output buffer
 * \param seq sequencer handle
 * \param ev_res event pointer to be extracted
 * \return 0 on success otherwise a negative error code
 *
 * Extracts the first event in output buffer.
 * If ev_res is NULL, just remove the event.
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

/**
 * \brief retrieve an event from sequencer
 * \param seq sequencer handle
 * \param ev event pointer to be stored
 * \return 
 *
 * Obtains an input event from sequencer.
 * The event is created via snd_seq_create_event(), and its pointer is stored on
 * ev argument.
 *
 * This function firstly recives the event byte-stream data from sequencer
 * as much as possible at once.  Then it retrieves the first event record
 * and store the pointer on ev.
 * By calling this function succeedingly, events are extract from the input buffer.
 *
 * If there is no input from sequencer, function falls into sleep
 * in blocking mode until an event is received,
 * or returns \c -EAGAIN error in non-blocking mode.
 * Occasionally, this function may return \c -ENOSPC error.
 * This means that the input FIFO of sequencer overran, and some events are
 * lost.
 * Once this error is returned, the input FIFO is cleared automatically.
 *
 * Function returns the number of remaining event sizes on the input buffer
 * if an event is successfully received.
 * Application can determine from the returned value whether to call
 * input once more or not.
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

/**
 * \brief check events in input queue
 *
 * Returns the number of remaining input events.
 * If events remain on the input buffer of user-space, function returns
 * the number of events on it.
 * If fetch_sequencer argument is non-zero,
 * this function checks the presence of events on sequencer FIFO via
 * select syscall.  When events exist, they are
 * transferred to the input buffer, and the number of received events are returned.
 * If fetch_sequencer argument is zero and
 * no events remain on the input buffer, function simplly returns zero.
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

/**
 * \brief remove all events on user-space output buffer
 * \param seq sequencer handle
 *
 * Removes all events on user-space output buffer.
 * Unlike snd_seq_drain_output(0, this function doesn't remove
 * events on output memory pool of sequencer.
 */
int snd_seq_drop_output_buffer(snd_seq_t *seq)
{
	assert(seq);
	seq->obufused = 0;
	return 0;
}

/**
 * \brief remove all events on user-space input FIFO
 * \param seq sequencer handle
 */
int snd_seq_drop_input_buffer(snd_seq_t *seq)
{
	assert(seq);
	seq->ibufptr = 0;
	seq->ibuflen = 0;
	return 0;
}

/**
 * \brief remove all events on output buffer
 * \param seq sequencer handle
 *
 * Removes all events on both user-space output buffer and
 * output memory pool on kernel.
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

/**
 * \brief clear input buffer and and remove events in sequencer queue
 * \param seq sequencer handle
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

/**
 * \brief remove events on input/output buffers
 * \param seq sequencer handle
 *
 * Removes matching events with the given condition from input/output buffers.
 * The removal condition is specified in rmp argument.
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

/**
 * \brief obtain the pool information of the current client
 * \param seq sequencer handle
 * \param info information to be stored
 */
int snd_seq_get_client_pool(snd_seq_t *seq, snd_seq_client_pool_t *info)
{
	assert(seq && info);
	info->client = seq->client;
	return seq->ops->get_client_pool(seq, info);
}

/**
 * \brief set the pool information
 * \param seq sequencer handle
 * \param info information to update
 *
 * Sets the pool information of the current client.
 * The client field in info is replaced automatically with the current id.
 */
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

/**
 * \brief query the next matching client
 * \param seq sequencer handle
 * \param info query pattern and result
 *
 * Queries the next matching client with the given condition in
 * info argument.
 * The search begins at the client with an id one greater than
 * client field in info.
 * If name field in info is not empty, the client name is compared.
 * If a matching client is found, its attributes are stored o
 * info and returns zero.
 * Otherwise returns a negative error code.
 */
int snd_seq_query_next_client(snd_seq_t *seq, snd_seq_client_info_t *info)
{
	assert(seq && info);
	return seq->ops->query_next_client(seq, info);
}

/**
 * \brief query the next matching port
 * \param seq sequencer handle
 * \param info query pattern and result

 * Queries the next matching port from the port
 * on the client given in info argument.
 * The search begins at the next port specified in
 * port field of info.
 * For finding the first port at a certain client, give -1 to
 * port field.
 * If name field is not empty, the name is checked.
 * If a matching port is found, its attributes are stored on
 * info and function returns zero.
 * Otherwise, a negative error code is returned.
 */
int snd_seq_query_next_port(snd_seq_t *seq, snd_seq_port_info_t *info)
{
	assert(seq && info);
	return seq->ops->query_next_port(seq, info);
}

/*----------------------------------------------------------------*/

/*
 * misc.
 */

/**
 * \brief set a bit flag
 */
void snd_seq_set_bit(int nr, void *array)
{
	((unsigned int *)array)[nr >> 5] |= 1UL << (nr & 31);
}

/**
 * \brief change a bit flag
 */
int snd_seq_change_bit(int nr, void *array)
{
	int result;

	result = ((((unsigned int *)array)[nr >> 5]) & (1UL << (nr & 31))) ? 1 : 0;
	((unsigned int *)array)[nr >> 5] |= 1UL << (nr & 31);
	return result;
}

/**
 * \brief get a bit flag state
 */
int snd_seq_get_bit(int nr, void *array)
{
	return ((((unsigned int *)array)[nr >> 5]) & (1UL << (nr & 31))) ? 1 : 0;
}

