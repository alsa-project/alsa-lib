/**
 * \file seq/seq.c
 * \brief Sequencer Interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 2000-2001
 *
 * See \ref seq page for more details.
 */

/* 
 *  Sequencer Interface - main file
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

/*! \page seq Sequencer interface

<P>Description...

*/

#include <dlfcn.h>
#include <sys/poll.h>
#include "seq_local.h"

/****************************************************************************
 *                                                                          *
 *                                seq.h                                     *
 *                              Sequencer                                   *
 *                                                                          *
 ****************************************************************************/

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
			     snd_config_t *seq_root, snd_config_t *seq_conf,
			     int streams, int mode)
{
	const char *str;
	char buf[256];
	int err;
	snd_config_t *conf, *type_conf = NULL;
	snd_config_iterator_t i, next;
	const char *id;
	const char *lib = NULL, *open_name = NULL;
	int (*open_func)(snd_seq_t **, const char *,
			 snd_config_t *, snd_config_t *, 
			 int, int) = NULL;
#ifndef PIC
	extern void *snd_seq_open_symbols(void);
#endif
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
	err = snd_config_get_id(conf, &id);
	if (err < 0) {
		SNDERR("unable to get id");
		return err;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", id);
		return err;
	}
	err = snd_config_search_definition(seq_root, "seq_type", str, &type_conf);
	if (err >= 0) {
		if (snd_config_get_type(type_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for SEQ type %s definition", str);
			goto _err;
		}
		snd_config_for_each(i, next, type_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			if (snd_config_get_id(n, &id) < 0)
				continue;
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
			err = -EINVAL;
			goto _err;
		}
	}
	if (!open_name) {
		open_name = buf;
		snprintf(buf, sizeof(buf), "_snd_seq_%s_open", str);
	}
#ifndef PIC
	snd_seq_open_symbols();
#endif
	h = snd_dlopen(lib, RTLD_NOW);
	if (h)
		open_func = snd_dlsym(h, open_name, SND_DLSYM_VERSION(SND_SEQ_DLSYM_VERSION));
	err = 0;
	if (!h) {
		SNDERR("Cannot open shared library %s", lib);
		err = -ENOENT;
	} else if (!open_func) {
		SNDERR("symbol %s is not defined inside %s", open_name, lib);
		snd_dlclose(h);
		err = -ENXIO;
	}
       _err:
	if (type_conf)
		snd_config_delete(type_conf);
	return err >= 0 ? open_func(seqp, name, seq_root, seq_conf, streams, mode) : err;
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
	err = snd_seq_open_conf(seqp, name, root, seq_conf, streams, mode);
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
 * library.  Usually you need to pass \c "default" here.
 * \param streams The read/write mode of the sequencer.  Can be one of
 * three values:
 * - #SND_SEQ_OPEN_OUTPUT - open the sequencer for output only
 * - #SND_SEQ_OPEN_INPUT - open the sequencer for input only
 * - #SND_SEQ_OPEN_DUPLEX - open the sequencer for output and input
 * \note Internally, these are translated to \c O_WRONLY, \c O_RDONLY and
 * \O_RDWR respectively and used as the second argument to the C library
 * open() call.
 * \param mode Optional modifier.  Can be either 0, or
 * #SND_SEQ_NONBLOCK, which will make read/write operations
 * non-blocking.  This can also be set later using #snd_seq_nonblock().
 * \return 0 on success otherwise a negative error code
 *
 * Creates a new handle and opens a connection to the kernel
 * sequencer interface.
 * After a client is created successfully, an event
 * with #SND_SEQ_EVENT_CLIENT_START is broadcast to announce port.
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
 * \brief Open the ALSA sequencer using local configuration
 *
 * \param seqp Pointer to a snd_seq_t pointer.
 * \param streams The read/write mode of the sequencer.
 * \param mode Optional modifier
 * \param lconf Local configuration
 * \return 0 on success otherwise a negative error code
 *
 * See the snd_seq_open() function for further details. The extension
 * is that the given configuration is used to resolve abstract name.
 */
int snd_seq_open_lconf(snd_seq_t **seqp, const char *name, 
		       int streams, int mode, snd_config_t *lconf)
{
	assert(seqp && name && lconf);
	return snd_seq_open_noupdate(seqp, lconf, name, streams, mode);
}

/**
 * \brief Close the sequencer
 * \param handle Handle returned from #snd_seq_open()
 * \return 0 on success otherwise a negative error code
 *
 * Closes the sequencer client and releases its resources.
 * After a client is closed, an event with
 * #SND_SEQ_EVENT_CLIENT_EXIT is broadcast to announce port.
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
 * can be specified by the second argument.  When both input and output
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
 * \brief Get poll descriptors
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
		revents |= POLLIN|POLLERR;
	}
	if ((events & POLLOUT) && space >= 1) {
		assert(seq->streams & SND_SEQ_OPEN_INPUT);
		revents |= POLLOUT|POLLERR;
	}
	if (!revents)
		return 0;
	pfds->fd = seq->poll_fd;
	pfds->events = revents;
	return 1;
}

/**
 * \brief get returned events from poll descriptors
 * \param seq sequencer handle
 * \param pfds array of poll descriptors
 * \param nfds count of poll descriptors
 * \param revents returned events
 * \return zero if success, otherwise a negative error code
 */
int snd_seq_poll_descriptors_revents(snd_seq_t *seq, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
        assert(seq && pfds && revents);
        if (nfds == 1) {
                *revents = pfds->revents;
                return 0;
        }
        return -EINVAL;
}

/**
 * \brief Set nonblock mode
 * \param seq sequencer handle
 * \param nonblock 0 = block, 1 = nonblock mode
 * \return 0 on success otherwise a negative error code
 *
 * Change the blocking mode of the given client.
 * In block mode, the client falls into sleep when it fills the
 * output memory pool with full events.  The client will be woken up
 * after a certain amount of free space becomes available.
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
 * \brief Get the client id
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
 * \brief Return the size of output buffer
 * \param seq sequencer handle
 * \return the size of output buffer in bytes
 *
 * Obtains the size of output buffer.
 * This buffer is used to store decoded byte-stream of output events
 * before transferring to sequencer.
 */
size_t snd_seq_get_output_buffer_size(snd_seq_t *seq)
{
	assert(seq);
	if (!seq->obuf)
		return 0;
	return seq->obufsize;
}

/**
 * \brief Return the size of input buffer
 * \param seq sequencer handle
 * \return the size of input buffer in bytes
 *
 * Obtains the size of input buffer.
 * This buffer is used to read byte-stream of input events from sequencer.
 */
size_t snd_seq_get_input_buffer_size(snd_seq_t *seq)
{
	assert(seq);
	if (!seq->ibuf)
		return 0;
	return seq->ibufsize * sizeof(snd_seq_event_t);
}

/**
 * \brief Change the size of output buffer
 * \param seq sequencer handle
 * \param size the size of output buffer to be changed in bytes
 * \return 0 on success otherwise a negative error code
 *
 * Changes the size of output buffer.
 */
int snd_seq_set_output_buffer_size(snd_seq_t *seq, size_t size)
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
 * \brief Resize the input buffer
 * \param seq sequencer handle
 * \param size the size of input buffer to be changed in bytes
 * \return 0 on success otherwise a negative error code
 *
 * Changes the size of input buffer.
 */
int snd_seq_set_input_buffer_size(snd_seq_t *seq, size_t size)
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
 * \brief Get size of #snd_seq_system_info_t
 * \return size in bytes
 */
size_t snd_seq_system_info_sizeof()
{
	return sizeof(snd_seq_system_info_t);
}

/**
 * \brief Allocate an empty #snd_seq_system_info_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_system_info_malloc(snd_seq_system_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_system_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief Frees a previously allocated #snd_seq_system_info_t
 * \param pointer to object to free
 */
void snd_seq_system_info_free(snd_seq_system_info_t *obj)
{
	free(obj);
}

/**
 * \brief Copy one #snd_seq_system_info_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_system_info_copy(snd_seq_system_info_t *dst, const snd_seq_system_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get maximum number of queues
 * \param info #snd_seq_system_info_t container
 * \return maximum number of queues
 */
int snd_seq_system_info_get_queues(const snd_seq_system_info_t *info)
{
	assert(info);
	return info->queues;
}

/**
 * \brief Get maximum number of clients
 * \param info #snd_seq_system_info_t container
 * \return maximum number of clients
 */
int snd_seq_system_info_get_clients(const snd_seq_system_info_t *info)
{
	assert(info);
	return info->clients;
}

/**
 * \brief Get maximum number of ports
 * \param info #snd_seq_system_info_t container
 * \return maximum number of ports
 */
int snd_seq_system_info_get_ports(const snd_seq_system_info_t *info)
{
	assert(info);
	return info->ports;
}

/**
 * \brief Get maximum number of channels
 * \param info #snd_seq_system_info_t container
 * \return maximum number of channels
 */
int snd_seq_system_info_get_channels(const snd_seq_system_info_t *info)
{
	assert(info);
	return info->channels;
}

/**
 * \brief Get the current number of clients
 * \param info #snd_seq_system_info_t container
 * \return current number of clients
 */
int snd_seq_system_info_get_cur_clients(const snd_seq_system_info_t *info)
{
	assert(info);
	return info->cur_clients;
}

/**
 * \brief Get the current number of queues
 * \param info #snd_seq_system_info_t container
 * \return current number of queues
 */
int snd_seq_system_info_get_cur_queues(const snd_seq_system_info_t *info)
{
	assert(info);
	return info->cur_queues;
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


/*----------------------------------------------------------------*/

/**
 * \brief get size of #snd_seq_client_info_t
 * \return size in bytes
 */
size_t snd_seq_client_info_sizeof()
{
	return sizeof(snd_seq_client_info_t);
}

/**
 * \brief allocate an empty #snd_seq_client_info_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_client_info_malloc(snd_seq_client_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_client_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_client_info_t
 * \param pointer to object to free
 */
void snd_seq_client_info_free(snd_seq_client_info_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_client_info_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_client_info_copy(snd_seq_client_info_t *dst, const snd_seq_client_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get client id of a client_info container
 * \param info client_info container
 * \return client id
 */
int snd_seq_client_info_get_client(const snd_seq_client_info_t *info)
{
	assert(info);
	return info->client;
}

/**
 * \brief Get client type of a client_info container
 * \param info client_info container
 * \return client type
 *
 * The client type is either #SEQ_CLIENT_TYPE_KERNEL or #SEQ_CLIENT_TYPE_USER
 * for kernel or user client respectively.
 */
snd_seq_client_type_t snd_seq_client_info_get_type(const snd_seq_client_info_t *info)
{
	assert(info);
	return info->type;
}

/**
 * \brief Get the name of a client_info container
 * \param info client_info container
 * \return name string
 */
const char *snd_seq_client_info_get_name(snd_seq_client_info_t *info)
{
	assert(info);
	return info->name;
}

/**
 * \brief Get the broadcast filter usage of a client_info container
 * \param info client_info container
 * \return 1 if broadcast is accepted
 */
int snd_seq_client_info_get_broadcast_filter(const snd_seq_client_info_t *info)
{
	assert(info);
	return (info->filter & SNDRV_SEQ_FILTER_BROADCAST) ? 1 : 0;
}

/**
 * \brief Get the error-bounce usage of a client_info container
 * \param info client_info container
 * \return 1 if error-bounce is enabled
 */
int snd_seq_client_info_get_error_bounce(const snd_seq_client_info_t *info)
{
	assert(info);
	return (info->filter & SNDRV_SEQ_FILTER_BOUNCE) ? 1 : 0;
}

/**
 * \brief Get the event filter bitmap of a client_info container
 * \param info client_info container
 * \return NULL if no event filter, or pointer to event filter bitmap
 */
const unsigned char *snd_seq_client_info_get_event_filter(const snd_seq_client_info_t *info)
{
	assert(info);
	if (info->filter & SNDRV_SEQ_FILTER_USE_EVENT)
		return info->event_filter;
	else
		return NULL;
}

/**
 * \brief Get the number of opened ports of a client_info container
 * \param info client_info container
 * \return number of opened ports
 */
int snd_seq_client_info_get_num_ports(const snd_seq_client_info_t *info)
{
	assert(info);
	return info->num_ports;
}

/**
 * \brief Get the number of lost events of a client_info container
 * \param info client_info container
 * \return number of lost events
 */
int snd_seq_client_info_get_event_lost(const snd_seq_client_info_t *info)
{
	assert(info);
	return info->event_lost;
}

/**
 * \brief Set the client id of a client_info container
 * \param info client_info container
 * \param client client id
 */
void snd_seq_client_info_set_client(snd_seq_client_info_t *info, int client)
{
	assert(info);
	info->client = client;
}

/**
 * \brief Set the name of a client_info container
 * \param info client_info container
 * \param name name string
 */
void snd_seq_client_info_set_name(snd_seq_client_info_t *info, const char *name)
{
	assert(info && name);
	strncpy(info->name, name, sizeof(info->name));
}

/**
 * \brief Set the broadcast filter usage of a client_info container
 * \param info client_info container
 * \param val non-zero if broadcast is accepted
 */
void snd_seq_client_info_set_broadcast_filter(snd_seq_client_info_t *info, int val)
{
	assert(info);
	if (val)
		info->filter |= SNDRV_SEQ_FILTER_BROADCAST;
	else
		info->filter &= ~SNDRV_SEQ_FILTER_BROADCAST;
}

/**
 * \brief Set the error-bounce usage of a client_info container
 * \param info client_info container
 * \param val non-zero if error is bounced
 */
void snd_seq_client_info_set_error_bounce(snd_seq_client_info_t *info, int val)
{
	assert(info);
	if (val)
		info->filter |= SNDRV_SEQ_FILTER_BOUNCE;
	else
		info->filter &= ~SNDRV_SEQ_FILTER_BOUNCE;
}

/**
 * \brief Set the event filter bitmap of a client_info container
 * \param info client_info container
 * \param filter event filter bitmap
 */
void snd_seq_client_info_set_event_filter(snd_seq_client_info_t *info, unsigned char *filter)
{
	assert(info);
	if (! filter)
		info->filter &= ~SNDRV_SEQ_FILTER_USE_EVENT;
	else {
		info->filter |= SNDRV_SEQ_FILTER_USE_EVENT;
		memcpy(info->event_filter, filter, sizeof(info->event_filter));
	}
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
int snd_seq_get_any_client_info(snd_seq_t *seq, int client, snd_seq_client_info_t *info)
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
int snd_seq_get_client_info(snd_seq_t *seq, snd_seq_client_info_t *info)
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
int snd_seq_set_client_info(snd_seq_t *seq, snd_seq_client_info_t *info)
{
	assert(seq && info);
	info->client = seq->client;
	info->type = USER_CLIENT;
	return seq->ops->set_client_info(seq, info);
}

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


/*----------------------------------------------------------------*/


/*
 * Port
 */

/**
 * \brief get size of #snd_seq_port_info_t
 * \return size in bytes
 */
size_t snd_seq_port_info_sizeof()
{
	return sizeof(snd_seq_port_info_t);
}

/**
 * \brief allocate an empty #snd_seq_port_info_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_port_info_malloc(snd_seq_port_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_port_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_port_info_t
 * \param pointer to object to free
 */
void snd_seq_port_info_free(snd_seq_port_info_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_port_info_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_port_info_copy(snd_seq_port_info_t *dst, const snd_seq_port_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get client id of a port_info container
 * \param info port_info container
 * \return client id
 */
int snd_seq_port_info_get_client(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->addr.client;
}

/**
 * \brief Get port id of a port_info container
 * \param info port_info container
 * \return port id
 */
int snd_seq_port_info_get_port(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->addr.port;
}

/**
 * \brief Get client/port address of a port_info container
 * \param info port_info container
 * \return client/port address pointer
 */
const snd_seq_addr_t *snd_seq_port_info_get_addr(const snd_seq_port_info_t *info)
{
	assert(info);
	return (snd_seq_addr_t *)&info->addr;
}

/**
 * \brief Get the name of a port_info container
 * \param info port_info container
 * \return name string
 */
const char *snd_seq_port_info_get_name(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->name;
}

/**
 * \brief Get the capability bits of a port_info container
 * \param info port_info container
 * \return capability bits
 */
unsigned int snd_seq_port_info_get_capability(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->capability;
}

/**
 * \brief Get the type bits of a port_info container
 * \param info port_info container
 * \return port type bits
 */
unsigned int snd_seq_port_info_get_type(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->type;
}

/**
 * \brief Get the number of read subscriptions of a port_info container
 * \param info port_info container
 * \return number of read subscriptions
 */
int snd_seq_port_info_get_read_use(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->read_use;
}

/**
 * \brief Get the number of write subscriptions of a port_info container
 * \param info port_info container
 * \return number of write subscriptions
 */
int snd_seq_port_info_get_write_use(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->write_use;
}

/**
 * \brief Get the midi channels of a port_info container
 * \param info port_info container
 * \return number of midi channels (default 0)
 */
int snd_seq_port_info_get_midi_channels(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->midi_channels;
}

/**
 * \brief Get the midi voices of a port_info container
 * \param info port_info container
 * \return number of midi voices (default 0)
 */
int snd_seq_port_info_get_midi_voices(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->midi_voices;
}

/**
 * \brief Get the synth voices of a port_info container
 * \param info port_info container
 * \return number of synth voices (default 0)
 */
int snd_seq_port_info_get_synth_voices(const snd_seq_port_info_t *info)
{
	assert(info);
	return info->synth_voices;
}

/**
 * \brief Get the port-specified mode of a port_info container
 * \param info port_info container
 * \return 1 if port id is specified at creation
 */
int snd_seq_port_info_get_port_specified(const snd_seq_port_info_t *info)
{
	assert(info);
	return (info->flags & SNDRV_SEQ_PORT_FLG_GIVEN_PORT) ? 1 : 0;
}

/**
 * \brief Set the client id of a port_info container
 * \param info port_info container
 * \param client client id
 */
void snd_seq_port_info_set_client(snd_seq_port_info_t *info, int client)
{
	assert(info);
	info->addr.client = client;
}

/**
 * \brief Set the port id of a port_info container
 * \param info port_info container
 * \param port port id
 */
void snd_seq_port_info_set_port(snd_seq_port_info_t *info, int port)
{
	assert(info);
	info->addr.port = port;
}

/**
 * \brief Set the client/port address of a port_info container
 * \param info port_info container
 * \param addr client/port address
 */
void snd_seq_port_info_set_addr(snd_seq_port_info_t *info, const snd_seq_addr_t *addr)
{
	assert(info);
	info->addr = *(struct sndrv_seq_addr *)addr;
}

/**
 * \brief Set the name of a port_info container
 * \param info port_info container
 * \param name name string
 */
void snd_seq_port_info_set_name(snd_seq_port_info_t *info, const char *name)
{
	assert(info && name);
	strncpy(info->name, name, sizeof(info->name));
}

/**
 * \brief set the capability bits of a port_info container
 * \param info port_info container
 * \param capability capability bits
 */
void snd_seq_port_info_set_capability(snd_seq_port_info_t *info, unsigned int capability)
{
	assert(info);
	info->capability = capability;
}

/**
 * \brief Get the type bits of a port_info container
 * \param info port_info container
 * \return port type bits
 */
void snd_seq_port_info_set_type(snd_seq_port_info_t *info, unsigned int type)
{
	assert(info);
	info->type = type;
}

/**
 * \brief set the midi channels of a port_info container
 * \param info port_info container
 * \param channels midi channels (default 0)
 */
void snd_seq_port_info_set_midi_channels(snd_seq_port_info_t *info, int channels)
{
	assert(info);
	info->midi_channels = channels;
}

/**
 * \brief set the midi voices of a port_info container
 * \param info port_info container
 * \param voices midi voices (default 0)
 */
void snd_seq_port_info_set_midi_voices(snd_seq_port_info_t *info, int voices)
{
	assert(info);
	info->midi_voices = voices;
}

/**
 * \brief set the synth voices of a port_info container
 * \param info port_info container
 * \param voices synth voices (default 0)
 */
void snd_seq_port_info_set_synth_voices(snd_seq_port_info_t *info, int voices)
{
	assert(info);
	info->synth_voices = voices;
}

/**
 * \brief Set the port-specified mode of a port_info container
 * \param info port_info container
 * \param val non-zero if specifying the port id at creation
 */
void snd_seq_port_info_set_port_specified(snd_seq_port_info_t *info, int val)
{
	assert(info);
	if (val)
		info->flags |= SNDRV_SEQ_PORT_FLG_GIVEN_PORT;
	else
		info->flags &= ~SNDRV_SEQ_PORT_FLG_GIVEN_PORT;
}


/**
 * \brief create a sequencer port on the current client
 * \param seq sequencer handle
 * \param port port information for the new port
 * \return 0 on success otherwise a negative error code
 *
 * Creates a sequencer port on the current client.
 * The attributes of created port is specified in \a info argument.
 *
 * The client field in \a info argument is overwritten with the current client id.
 * The port id to be created can be specified via #snd_seq_port_info_set_port_specified.
 * You can get the created port id by reading the port pointer via #snd_seq_port_info_get_port.
 *
 * Each port has the capability bit-masks to specify the access capability
 * of the port from other clients.
 * The capability bit flags are defined as follows:
 * - #SND_SEQ_PORT_CAP_READ Readable from this port
 * - #SND_SEQ_PORT_CAP_WRITE Writable to this port.
 * - #SND_SEQ_PORT_CAP_SYNC_READ For synchronization (not implemented)
 * - #SND_SEQ_PORT_CAP_SYNC_WRITE For synchronization (not implemented)
 * - #SND_SEQ_PORT_CAP_DUPLEX Read/write duplex access is supported
 * - #SND_SEQ_PORT_CAP_SUBS_READ Read subscription is allowed
 * - #SND_SEQ_PORT_CAP_SUBS_WRITE Write subscription is allowed
 * - #SND_SEQ_PORT_CAP_SUBS_NO_EXPORT Subscription management from 3rd client is disallowed
 *
 * Each port has also the type bitmasks defined as follows:
 * - #SND_SEQ_PORT_TYPE_SPECIFIC Hardware specific port
 * - #SND_SEQ_PORT_TYPE_MIDI_GENERIC Generic MIDI device
 * - #SND_SEQ_PORT_TYPE_MIDI_GM General MIDI compatible device
 * - #SND_SEQ_PORT_TYPE_MIDI_GS GS compatible device
 * - #SND_SEQ_PORT_TYPE_MIDI_XG XG compatible device
 * - #SND_SEQ_PORT_TYPE_MIDI_MT32 MT-32 compatible device
 * - #SND_SEQ_PORT_TYPE_SYNTH Synth device
 * - #SND_SEQ_PORT_TYPE_DIRECT_SAMPLE Sampling device (supporting download)
 * - #SND_SEQ_PORT_TYPE_SAMPLE Sampling device (sample can be downloaded at any time)
 * - #SND_SEQ_PORT_TYPE_APPLICATION Application (sequencer/editor)
 *
 * A port may contain specific midi channels, midi voices and synth voices.
 * These values could be zero as default.
 */
int snd_seq_create_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	assert(seq && port);
	port->addr.client = seq->client;
	return seq->ops->create_port(seq, port);
}

/**
 * \brief delete a sequencer port on the current client
 * \param seq sequencer handle
 * \param port port to be deleted
 * \return 0 on success otherwise a negative error code
 *
 * Deletes the existing sequencer port on the current client.
 */
int snd_seq_delete_port(snd_seq_t *seq, int port)
{
	snd_seq_port_info_t pinfo;
	assert(seq);
	memset(&pinfo, 0, sizeof(pinfo));
	pinfo.addr.client = seq->client;
	pinfo.addr.port = port;
	return seq->ops->delete_port(seq, &pinfo);
}

/**
 * \brief obtain the information of a port on an arbitrary client
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
	info->addr.client = client;
	info->addr.port = port;
	return seq->ops->get_port_info(seq, info);
}

/**
 * \brief obtain the information of a port on the current client
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
	info->addr.client = seq->client;
	info->addr.port = port;
	return seq->ops->set_port_info(seq, info);
}

/**
 * \brief query the next matching port
 * \param seq sequencer handle
 * \param info query pattern and result

 * Queries the next matching port on the client specified in
 * \a info argument.
 * The search begins at the next port specified in
 * port field of \a info argument.
 * For finding the first port at a certain client, give -1.
 *
 * If a matching port is found, its attributes are stored on
 * \a info and function returns zero.
 * Otherwise, a negative error code is returned.
 */
int snd_seq_query_next_port(snd_seq_t *seq, snd_seq_port_info_t *info)
{
	assert(seq && info);
	return seq->ops->query_next_port(seq, info);
}


/*----------------------------------------------------------------*/

/*
 * subscription
 */


/**
 * \brief get size of #snd_seq_port_subscribe_t
 * \return size in bytes
 */
size_t snd_seq_port_subscribe_sizeof()
{
	return sizeof(snd_seq_port_subscribe_t);
}

/**
 * \brief allocate an empty #snd_seq_port_subscribe_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_port_subscribe_malloc(snd_seq_port_subscribe_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_port_subscribe_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_port_subscribe_t
 * \param pointer to object to free
 */
void snd_seq_port_subscribe_free(snd_seq_port_subscribe_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_port_subscribe_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_port_subscribe_copy(snd_seq_port_subscribe_t *dst, const snd_seq_port_subscribe_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get sender address of a port_subscribe container
 * \param info port_subscribe container
 * \param addr sender address
 */
const snd_seq_addr_t *snd_seq_port_subscribe_get_sender(const snd_seq_port_subscribe_t *info)
{
	assert(info);
	return (snd_seq_addr_t *)&info->sender;
}

/**
 * \brief Get destination address of a port_subscribe container
 * \param info port_subscribe container
 * \param addr destination address
 */
const snd_seq_addr_t *snd_seq_port_subscribe_get_dest(const snd_seq_port_subscribe_t *info)
{
	assert(info);
	return (snd_seq_addr_t *)&info->dest;
}

/**
 * \brief Get the queue id of a port_subscribe container
 * \param info port_subscribe container
 * \return queue id
 */
int snd_seq_port_subscribe_get_queue(const snd_seq_port_subscribe_t *info)
{
	assert(info);
	return info->queue;
}

/**
 * \brief Get the exclusive mode of a port_subscribe container
 * \param info port_subscribe container
 * \return 1 if exclusive mode
 */
int snd_seq_port_subscribe_get_exclusive(const snd_seq_port_subscribe_t *info)
{
	assert(info);
	return (info->flags & SNDRV_SEQ_PORT_SUBS_EXCLUSIVE) ? 1 : 0;
}

/**
 * \brief Get the time-update mode of a port_subscribe container
 * \param info port_subscribe container
 * \return 1 if update timestamp
 */
int snd_seq_port_subscribe_get_time_update(const snd_seq_port_subscribe_t *info)
{
	assert(info);
	return (info->flags & SNDRV_SEQ_PORT_SUBS_TIMESTAMP) ? 1 : 0;
}

/**
 * \brief Get the real-time update mode of a port_subscribe container
 * \param info port_subscribe container
 * \return 1 if real-time update mode
 */
int snd_seq_port_subscribe_get_time_real(const snd_seq_port_subscribe_t *info)
{
	assert(info);
	return (info->flags & SNDRV_SEQ_PORT_SUBS_TIME_REAL) ? 1 : 0;
}

/**
 * \brief Set sender address of a port_subscribe container
 * \param info port_subscribe container
 * \param addr sender address
 */
void snd_seq_port_subscribe_set_sender(snd_seq_port_subscribe_t *info, const snd_seq_addr_t *addr)
{
	assert(info);
	memcpy(&info->sender, addr, sizeof(*addr));
}
      
/**
 * \brief Set destination address of a port_subscribe container
 * \param info port_subscribe container
 * \param addr destination address
 */
void snd_seq_port_subscribe_set_dest(snd_seq_port_subscribe_t *info, const snd_seq_addr_t *addr)
{
	assert(info);
	memcpy(&info->dest, addr, sizeof(*addr));
}

/**
 * \brief Set the queue id of a port_subscribe container
 * \param info port_subscribe container
 * \param q queue id
 */
void snd_seq_port_subscribe_set_queue(snd_seq_port_subscribe_t *info, int q)
{
	assert(info);
	info->queue = q;
}

/**
 * \brief Set the voices of a port_subscribe container
 * \param info port_subscribe container
 * \param voices voices to be allocated (0 = don't care)
 */
void snd_seq_port_subscribe_set_voices(snd_seq_port_subscribe_t *info, unsigned int voices)
{
	assert(info);
	info->voices = voices;
}

/**
 * \brief Set the exclusive mode of a port_subscribe container
 * \param info port_subscribe container
 * \param val non-zero to enable
 */
void snd_seq_port_subscribe_set_exclusive(snd_seq_port_subscribe_t *info, int val)
{
	assert(info);
	if (val)
		info->flags |= SNDRV_SEQ_PORT_SUBS_EXCLUSIVE;
	else
		info->flags &= ~SNDRV_SEQ_PORT_SUBS_EXCLUSIVE;
}

/**
 * \brief Set the time-update mode of a port_subscribe container
 * \param info port_subscribe container
 * \param val non-zero to enable
 */
void snd_seq_port_subscribe_set_time_update(snd_seq_port_subscribe_t *info, int val)
{
	assert(info);
	if (val)
		info->flags |= SNDRV_SEQ_PORT_SUBS_TIMESTAMP;
	else
		info->flags &= ~SNDRV_SEQ_PORT_SUBS_TIMESTAMP;
}

/**
 * \brief Set the real-time mode of a port_subscribe container
 * \param info port_subscribe container
 * \param val non-zero to enable
 */
void snd_seq_port_subscribe_set_time_real(snd_seq_port_subscribe_t *info, int val)
{
	assert(info);
	if (val)
		info->flags |= SNDRV_SEQ_PORT_SUBS_TIME_REAL;
	else
		info->flags &= ~SNDRV_SEQ_PORT_SUBS_TIME_REAL;
}


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
 * \brief get size of #snd_seq_query_subscribe_t
 * \return size in bytes
 */
size_t snd_seq_query_subscribe_sizeof()
{
	return sizeof(snd_seq_query_subscribe_t);
}

/**
 * \brief allocate an empty #snd_seq_query_subscribe_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_query_subscribe_malloc(snd_seq_query_subscribe_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_query_subscribe_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_query_subscribe_t
 * \param pointer to object to free
 */
void snd_seq_query_subscribe_free(snd_seq_query_subscribe_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_query_subscribe_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_query_subscribe_copy(snd_seq_query_subscribe_t *dst, const snd_seq_query_subscribe_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get the client id of a query_subscribe container
 * \param info query_subscribe container
 * \return client id
 */
int snd_seq_query_subscribe_get_client(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return info->root.client;
}

/**
 * \brief Get the port id of a query_subscribe container
 * \param info query_subscribe container
 * \return port id
 */
int snd_seq_query_subscribe_get_port(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return info->root.port;
}

/**
 * \brief Get the client/port address of a query_subscribe container
 * \param info query_subscribe container
 * \return client/port address pointer
 */
const snd_seq_addr_t *snd_seq_query_subscribe_get_root(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return (snd_seq_addr_t *)&info->root;
}

/**
 * \brief Get the query type of a query_subscribe container
 * \param info query_subscribe container
 * \return query type
 */
snd_seq_query_subs_type_t snd_seq_query_subscribe_get_type(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return info->type;
}

/**
 * \brief Get the index of subscriber of a query_subscribe container
 * \param info query_subscribe container
 * \return subscriber's index
 */
int snd_seq_query_subscribe_get_index(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return info->index;
}

/**
 * \brief Get the number of subscriptions of a query_subscribe container
 * \param info query_subscribe container
 * \return number of subscriptions
 */
int snd_seq_query_subscribe_get_num_subs(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return info->num_subs;
}	

/**
 * \brief Get the address of subscriber of a query_subscribe container
 * \param info query_subscribe container
 * \return subscriber's address pointer
 */
const snd_seq_addr_t *snd_seq_query_subscribe_get_addr(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return (snd_seq_addr_t *)&info->addr;
}

/**
 * \brief Get the queue id of subscriber of a query_subscribe container
 * \param info query_subscribe container
 * \return subscriber's queue id
 */
int snd_seq_query_subscribe_get_queue(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return info->queue;
}

/**
 * \brief Get the exclusive mode of a query_subscribe container
 * \param info query_subscribe container
 * \return 1 if exclusive mode
 */
int snd_seq_query_subscribe_get_exclusive(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return (info->flags & SNDRV_SEQ_PORT_SUBS_EXCLUSIVE) ? 1 : 0;
}

/**
 * \brief Get the time-update mode of a query_subscribe container
 * \param info query_subscribe container
 * \return 1 if update timestamp
 */
int snd_seq_query_subscribe_get_time_update(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return (info->flags & SNDRV_SEQ_PORT_SUBS_TIMESTAMP) ? 1 : 0;
}

/**
 * \brief Get the real-time update mode of a query_subscribe container
 * \param info query_subscribe container
 * \return 1 if real-time update mode
 */
int snd_seq_query_subscribe_get_time_real(const snd_seq_query_subscribe_t *info)
{
	assert(info);
	return (info->flags & SNDRV_SEQ_PORT_SUBS_TIMESTAMP) ? 1 : 0;
}

/**
 * \brief Set the client id of a query_subscribe container
 * \param info query_subscribe container
 * \param client client id
 */
void snd_seq_query_subscribe_set_client(snd_seq_query_subscribe_t *info, int client)
{
	assert(info);
	info->root.client = client;
}

/**
 * \brief Set the port id of a query_subscribe container
 * \param info query_subscribe container
 * \param port port id
 */
void snd_seq_query_subscribe_set_port(snd_seq_query_subscribe_t *info, int port)
{
	assert(info);
	info->root.port = port;
}

/**
 * \brief Set the client/port address of a query_subscribe container
 * \param info query_subscribe container
 * \param addr client/port address pointer
 */
void snd_seq_query_subscribe_set_root(snd_seq_query_subscribe_t *info, const snd_seq_addr_t *addr)
{
	assert(info);
	info->root = *(struct sndrv_seq_addr *)addr;
}

/**
 * \brief Set the query type of a query_subscribe container
 * \param info query_subscribe container
 * \param type query type
 */
void snd_seq_query_subscribe_set_type(snd_seq_query_subscribe_t *info, snd_seq_query_subs_type_t type)
{
	assert(info);
	info->type = type;
}

/**
 * \brief Set the subscriber's index to be queried
 * \param info query_subscribe container
 * \param index index to be queried
 */
void snd_seq_query_subscribe_set_index(snd_seq_query_subscribe_t *info, int index)
{
	assert(info);
	info->index = index;
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
int snd_seq_query_port_subscribers(snd_seq_t *seq, snd_seq_query_subscribe_t * subs)
{
	assert(seq && subs);
	return seq->ops->query_port_subscribers(seq, subs);
}

/*----------------------------------------------------------------*/

/*
 * queue handlers
 */

/**
 * \brief get size of #snd_seq_queue_info_t
 * \return size in bytes
 */
size_t snd_seq_queue_info_sizeof()
{
	return sizeof(snd_seq_queue_info_t);
}

/**
 * \brief allocate an empty #snd_seq_queue_info_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_queue_info_malloc(snd_seq_queue_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_queue_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_queue_info_t
 * \param pointer to object to free
 */
void snd_seq_queue_info_free(snd_seq_queue_info_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_queue_info_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_queue_info_copy(snd_seq_queue_info_t *dst, const snd_seq_queue_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get the queue id of a queue_info container
 * \param info queue_info container
 * \return queue id
 */
int snd_seq_queue_info_get_queue(const snd_seq_queue_info_t *info)
{
	assert(info);
	return info->queue;
}

/**
 * \brief Get the name of a queue_info container
 * \param info queue_info container
 * \return name string
 */
const char *snd_seq_queue_info_get_name(const snd_seq_queue_info_t *info)
{
	assert(info);
	return info->name;
}

/**
 * \brief Get the owner client id of a queue_info container
 * \param info queue_info container
 * \return owner client id
 */
int snd_seq_queue_info_get_owner(const snd_seq_queue_info_t *info)
{
	assert(info);
	return info->owner;
}

/**
 * \brief Get the lock status of a queue_info container
 * \param info queue_info container
 * \return lock status --- non-zero = locked
 */
int snd_seq_queue_info_get_locked(const snd_seq_queue_info_t *info)
{
	assert(info);
	return info->locked;
}

/**
 * \brief Get the conditional bit flags of a queue_info container
 * \param info queue_info container
 * \return conditional bit flags
 */
unsigned int snd_seq_queue_info_get_flags(const snd_seq_queue_info_t *info)
{
	assert(info);
	return info->flags;
}

/**
 * \brief Set the name of a queue_info container
 * \param info queue_info container
 * \param name name string
 */
void snd_seq_queue_info_set_name(snd_seq_queue_info_t *info, const char *name)
{
	assert(info && name);
	strncpy(info->name, name, sizeof(info->name));
}

/**
 * \brief Set the owner client id of a queue_info container
 * \param info queue_info container
 * \param owner client id
 */
void snd_seq_queue_info_set_owner(snd_seq_queue_info_t *info, int owner)
{
	assert(info);
	info->owner = owner;
}

/**
 * \brief Set the lock status of a queue_info container
 * \param info queue_info container
 * \param locked lock status
 */
void snd_seq_queue_info_set_locked(snd_seq_queue_info_t *info, int locked)
{
	assert(info);
	info->locked = locked;
}

/**
 * \brief Set the conditional bit flags of a queue_info container
 * \param info queue_info container
 * \param flags conditional bit flags
 */
void snd_seq_queue_info_set_flags(snd_seq_queue_info_t *info, unsigned int flags)
{
	assert(info);
	info->flags = flags;
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
 * \brief allocate a queue with the specified name
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

/**
 * \brief delete the specified queue
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
 * \brief query the matching queue with the specified name
 * \param seq sequencer handle
 * \param name the name string to query
 * \return the queue id if found or negative error code
 *
 * Searches the matching queue with the specified name string.
 */
int snd_seq_query_named_queue(snd_seq_t *seq, const char *name)
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

/**
 * \brief Get the queue usage flag to the client
 * \param seq sequencer handle
 * \param q queue id
 * \param client client id
 * \return 1 = client is allowed to access the queue, 0 = not allowed, 
 *     otherwise a negative error code
 */
int snd_seq_get_queue_usage(snd_seq_t *seq, int q)
{
	struct sndrv_seq_queue_client info;
	int err;
	assert(seq);
	memset(&info, 0, sizeof(info));
	info.queue = q;
	info.client = seq->client;
	if ((err = seq->ops->get_queue_client(seq, &info)) < 0)
		return err;
	return info.used;
}

/**
 * \brief Set the queue usage flag to the client
 * \param seq sequencer handle
 * \param q queue id
 * \param client client id
 * \param used non-zero if the client is allowed
 * \return 0 on success otherwise a negative error code
 */
int snd_seq_set_queue_usage(snd_seq_t *seq, int q, int used)
{
	struct sndrv_seq_queue_client info;
	assert(seq);
	memset(&info, 0, sizeof(info));
	info.queue = q;
	info.client = seq->client;
	info.used = used ? 1 : 0;
	return seq->ops->set_queue_client(seq, &info);
}


/**
 * \brief get size of #snd_seq_queue_status_t
 * \return size in bytes
 */
size_t snd_seq_queue_status_sizeof()
{
	return sizeof(snd_seq_queue_status_t);
}

/**
 * \brief allocate an empty #snd_seq_queue_status_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_queue_status_malloc(snd_seq_queue_status_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_queue_status_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_queue_status_t
 * \param pointer to object to free
 */
void snd_seq_queue_status_free(snd_seq_queue_status_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_queue_status_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_queue_status_copy(snd_seq_queue_status_t *dst, const snd_seq_queue_status_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get the queue id of a queue_status container
 * \param info queue_status container
 * \return queue id
 */
int snd_seq_queue_status_get_queue(const snd_seq_queue_status_t *info)
{
	assert(info);
	return info->queue;
}

/**
 * \brief Get the number of events of a queue_status container
 * \param info queue_status container
 * \return number of events
 */
int snd_seq_queue_status_get_events(const snd_seq_queue_status_t *info)
{
	assert(info);
	return info->events;
}

/**
 * \brief Get the tick time of a queue_status container
 * \param info queue_status container
 * \return tick time
 */
snd_seq_tick_time_t snd_seq_queue_status_get_tick_time(const snd_seq_queue_status_t *info)
{
	assert(info);
	return info->tick;
}

/**
 * \brief Get the real time of a queue_status container
 * \param info queue_status container
 * \param time real time
 */
const snd_seq_real_time_t *snd_seq_queue_status_get_real_time(const snd_seq_queue_status_t *info)
{
	assert(info);
	return (snd_seq_real_time_t *)&info->time;
}

/**
 * \brief Get the running status bits of a queue_status container
 * \param info queue_status container
 * \return running status bits
 */
unsigned int snd_seq_queue_status_get_status(const snd_seq_queue_status_t *info)
{
	assert(info);
	return info->running;
}


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
 * \brief get size of #snd_seq_queue_tempo_t
 * \return size in bytes
 */
size_t snd_seq_queue_tempo_sizeof()
{
	return sizeof(snd_seq_queue_tempo_t);
}

/**
 * \brief allocate an empty #snd_seq_queue_tempo_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_queue_tempo_malloc(snd_seq_queue_tempo_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_queue_tempo_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_queue_tempo_t
 * \param pointer to object to free
 */
void snd_seq_queue_tempo_free(snd_seq_queue_tempo_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_queue_tempo_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_queue_tempo_copy(snd_seq_queue_tempo_t *dst, const snd_seq_queue_tempo_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get the queue id of a queue_status container
 * \param info queue_status container
 * \return queue id
 */
int snd_seq_queue_tempo_get_queue(const snd_seq_queue_tempo_t *info)
{
	assert(info);
	return info->queue;
}

/**
 * \brief Get the tempo of a queue_status container
 * \param info queue_status container
 * \return tempo value
 */
unsigned int snd_seq_queue_tempo_get_tempo(const snd_seq_queue_tempo_t *info)
{
	assert(info);
	return info->tempo;
}

/**
 * \brief Get the ppq of a queue_status container
 * \param info queue_status container
 * \return ppq value
 */
int snd_seq_queue_tempo_get_ppq(const snd_seq_queue_tempo_t *info)
{
	assert(info);
	return info->ppq;
}

/**
 * \brief Get the timer skew value of a queue_status container
 * \param info queue_status container
 * \return timer skew value
 */
unsigned int snd_seq_queue_tempo_get_skew(const snd_seq_queue_tempo_t *info)
{
	assert(info);
	return info->skew_value;
}

/**
 * \brief Get the timer skew base value of a queue_status container
 * \param info queue_status container
 * \return timer skew base value
 */
unsigned int snd_seq_queue_tempo_get_skew_base(const snd_seq_queue_tempo_t *info)
{
	assert(info);
	return info->skew_base;
}

/**
 * \brief Set the tempo of a queue_status container
 * \param info queue_status container
 * \param tempo tempo value
 */
void snd_seq_queue_tempo_set_tempo(snd_seq_queue_tempo_t *info, unsigned int tempo)
{
	assert(info);
	info->tempo = tempo;
}

/**
 * \brief Set the ppq of a queue_status container
 * \param info queue_status container
 * \param ppq ppq value
 */
void snd_seq_queue_tempo_set_ppq(snd_seq_queue_tempo_t *info, int ppq)
{
	assert(info);
	info->ppq = ppq;
}

/**
 * \brief Set the timer skew value of a queue_status container
 * \param info queue_status container
 * \param skew timer skew value
 *
 * The skew of timer is calculated as skew / base.
 * For example, to play with double speed, pass base * 2 as the skew value.
 */
void snd_seq_queue_tempo_set_skew(snd_seq_queue_tempo_t *info, unsigned int skew)
{
	assert(info);
	info->skew_value = skew;
}

/**
 * \brief Set the timer skew base value of a queue_status container
 * \param info queue_status container
 * \param base timer skew base value
 */
void snd_seq_queue_tempo_set_skew_base(snd_seq_queue_tempo_t *info, unsigned int base)
{
	assert(info);
	info->skew_base = base;
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


/*----------------------------------------------------------------*/

/**
 * \brief get size of #snd_seq_queue_timer_t
 * \return size in bytes
 */
size_t snd_seq_queue_timer_sizeof()
{
	return sizeof(snd_seq_queue_timer_t);
}

/**
 * \brief allocate an empty #snd_seq_queue_timer_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_queue_timer_malloc(snd_seq_queue_timer_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_queue_timer_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_queue_timer_t
 * \param pointer to object to free
 */
void snd_seq_queue_timer_free(snd_seq_queue_timer_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_queue_timer_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_queue_timer_copy(snd_seq_queue_timer_t *dst, const snd_seq_queue_timer_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get the queue id of a queue_timer container
 * \param info queue_timer container
 * \return queue id
 */
int snd_seq_queue_timer_get_queue(const snd_seq_queue_timer_t *info)
{
	assert(info);
	return info->queue;
}

/**
 * \brief Get the timer type of a queue_timer container
 * \param info queue_timer container
 * \return timer type
 */
snd_seq_queue_timer_type_t snd_seq_queue_timer_get_type(const snd_seq_queue_timer_t *info)
{
	assert(info);
	return (snd_seq_queue_timer_type_t)info->type;
}

/**
 * \brief Get the timer id of a queue_timer container
 * \param info queue_timer container
 * \return timer id pointer
 */
const snd_timer_id_t *snd_seq_queue_timer_get_id(const snd_seq_queue_timer_t *info)
{
	assert(info);
	return &info->u.alsa.id;
}

/**
 * \brief Get the timer resolution of a queue_timer container
 * \param info queue_timer container
 * \return timer resolution
 */
unsigned int snd_seq_queue_timer_get_resolution(const snd_seq_queue_timer_t *info)
{
	assert(info);
	return info->u.alsa.resolution;
}

/**
 * \brief Set the timer type of a queue_timer container
 * \param info queue_timer container
 * \param type timer type
 */
void snd_seq_queue_timer_set_type(snd_seq_queue_timer_t *info, snd_seq_queue_timer_type_t type)
{
	assert(info);
	info->type = (int)type;
}
	
/**
 * \brief Set the timer id of a queue_timer container
 * \param info queue_timer container
 * \param id timer id pointer
 */
void snd_seq_queue_timer_set_id(snd_seq_queue_timer_t *info, const snd_timer_id_t *id)
{
	assert(info && id);
	info->u.alsa.id = *id;
}

/**
 * \brief Set the timer resolution of a queue_timer container
 * \param info queue_timer container
 * \param resolution timer resolution
 */
void snd_seq_queue_timer_set_resolution(snd_seq_queue_timer_t *info, unsigned int resolution)
{
	assert(info);
	info->u.alsa.resolution = resolution;
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

/*----------------------------------------------------------------*/

#ifndef DOC_HIDDEN
/**
 * \brief (DEPRECATED) create an event cell
 * \return the cell pointer allocated
 *
 * create an event cell via malloc.  the returned pointer must be released
 * by the application itself via normal free() call,
 * not via snd_seq_free_event().
 */
snd_seq_event_t *snd_seq_create_event(void)
{
	return (snd_seq_event_t *) calloc(1, sizeof(snd_seq_event_t));
}
#endif

/**
 * \brief (DEPRECATED) free an event
 *
 * releases the event pointer which was allocated by snd_seq_event_input().
 * this function is obsolete and does nothing inside actually.
 * used only for compatibility with the older version.
 */
#ifndef DOXYGEN
int snd_seq_free_event(snd_seq_event_t *ev ATTRIBUTE_UNUSED)
#else
int snd_seq_free_event(snd_seq_event_t *ev)
#endif
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
	if (sndrv_seq_ev_is_variable(ev))
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
 * The output buffer will be drained automatically if it becomes full.
 *
 * If events remain unprocessed on output buffer before drained,
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
 * \brief output an event onto the lib buffer without draining buffer
 * \param seq sequencer handle
 * \param ev event to be output
 * \return the byte size of remaining events. \c -EAGAIN if the buffer becomes full.
 *
 * This function doesn't drain buffer unlike snd_seq_event_output().
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
	if (sndrv_seq_ev_is_variable(ev)) {
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
 * \return the byte size sent to sequencer or a negative error code
 *
 * This function sends an event to the sequencer directly not through the
 * output buffer.  When the event is a variable length event, a temporary
 * buffer is allocated inside alsa-lib and the data is copied there before
 * actually sent.
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
 * \return the byte size of total of pending events
 */
int snd_seq_event_output_pending(snd_seq_t *seq)
{
	assert(seq);
	return seq->obufused;
}

/**
 * \brief drain output buffer to sequencer
 * \param seq sequencer handle
 * \return 0 when all events are drained and sent to sequencer.
 *         When events still remain on the buffer, the byte size of remaining
 *         events are returned.  On error a negative error code is returned.
 */
int snd_seq_drain_output(snd_seq_t *seq)
{
	ssize_t result, processed = 0;
	assert(seq);
	while (seq->obufused > 0) {
		result = seq->ops->write(seq, seq->obuf, seq->obufused);
		if (result < 0) {
			if (result == -EAGAIN && processed)
				return seq->obufused;
			return result;
		}
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
	if (! sndrv_seq_ev_is_variable(ev))
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
 * This function firstly receives the event byte-stream data from sequencer
 * as much as possible at once.  Then it retrieves the first event record
 * and store the pointer on ev.
 * By calling this function sequentially, events are extracted from the input buffer.
 *
 * If there is no input from sequencer, function falls into sleep
 * in blocking mode until an event is received,
 * or returns \c -EAGAIN error in non-blocking mode.
 * Occasionally, this function may return \c -ENOSPC error.
 * This means that the input FIFO of sequencer overran, and some events are
 * lost.
 * Once this error is returned, the input FIFO is cleared automatically.
 *
 * Function returns the byte size of remaining events on the input buffer
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
 * \brief check events in input buffer
 * \return the byte size of remaining input events on input buffer.
 *
 * If events remain on the input buffer of user-space, function returns
 * the total byte size of events on it.
 * If fetch_sequencer argument is non-zero,
 * this function checks the presence of events on sequencer FIFO
 * When events exist, they are transferred to the input buffer,
 * and the number of received events are returned.
 * If fetch_sequencer argument is zero and
 * no events remain on the input buffer, function simply returns zero.
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
 * Unlike snd_seq_drain_output(), this function doesn't remove
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
	rminfo.remove_mode = SNDRV_SEQ_REMOVE_OUTPUT;

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
	rminfo.remove_mode = SNDRV_SEQ_REMOVE_INPUT;

	return snd_seq_remove_events(seq, &rminfo);
}


/**
 * \brief get size of #snd_seq_remove_events_t
 * \return size in bytes
 */
size_t snd_seq_remove_events_sizeof()
{
	return sizeof(snd_seq_remove_events_t);
}

/**
 * \brief allocate an empty #snd_seq_remove_events_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_remove_events_malloc(snd_seq_remove_events_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_remove_events_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_remove_events_t
 * \param pointer to object to free
 */
void snd_seq_remove_events_free(snd_seq_remove_events_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_remove_events_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_remove_events_copy(snd_seq_remove_events_t *dst, const snd_seq_remove_events_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get the removal condition bits
 * \param info remove_events container
 * \return removal condition bits
 */
unsigned int snd_seq_remove_events_get_condition(const snd_seq_remove_events_t *info)
{
	assert(info);
	return info->remove_mode;
}

/**
 * \brief Get the queue as removal condition
 * \param info remove_events container
 * \return queue id
 */
int snd_seq_remove_events_get_queue(const snd_seq_remove_events_t *info)
{
	assert(info);
	return info->queue;
}

/**
 * \brief Get the event timestamp as removal condition
 * \param info remove_events container
 * \return time stamp
 */
const snd_seq_timestamp_t *snd_seq_remove_events_get_time(const snd_seq_remove_events_t *info)
{
	assert(info);
	return (snd_seq_timestamp_t *)&info->time;
}

/**
 * \brief Get the event destination address as removal condition
 * \param info remove_events container
 * \return destination address
 */
const snd_seq_addr_t *snd_seq_remove_events_get_dest(const snd_seq_remove_events_t *info)
{
	assert(info);
	return (snd_seq_addr_t *)&info->dest;
}

/**
 * \brief Get the event channel as removal condition
 * \param info remove_events container
 * \return channel number
 */
int snd_seq_remove_events_get_channel(const snd_seq_remove_events_t *info)
{
	assert(info);
	return info->channel;
}

/**
 * \brief Get the event type as removal condition
 * \param info remove_events container
 * \return event type
 */
int snd_seq_remove_events_get_event_type(const snd_seq_remove_events_t *info)
{
	assert(info);
	return info->type;
}

/**
 * \brief Get the event tag id as removal condition
 * \param info remove_events container
 * \return tag id
 */
int snd_seq_remove_events_get_tag(const snd_seq_remove_events_t *info)
{
	assert(info);
	return info->tag;
}

/**
 * \brief Set the removal condition bits
 * \param info remove_events container
 * \param flags removal condition bits
 */
void snd_seq_remove_events_set_condition(snd_seq_remove_events_t *info, unsigned int flags)
{
	assert(info);
	info->remove_mode = flags;
}

/**
 * \brief Set the queue as removal condition
 * \param info remove_events container
 * \param queue queue id
 */
void snd_seq_remove_events_set_queue(snd_seq_remove_events_t *info, int queue)
{
	assert(info);
	info->queue = queue;
}

/**
 * \brief Set the timestamp as removal condition
 * \param info remove_events container
 * \param time timestamp pointer
 */
void snd_seq_remove_events_set_time(snd_seq_remove_events_t *info, const snd_seq_timestamp_t *time)
{
	assert(info);
	info->time = *(union sndrv_seq_timestamp *)time;
}

/**
 * \brief Set the destination address as removal condition
 * \param info remove_events container
 * \param addr destination address
 */
void snd_seq_remove_events_set_dest(snd_seq_remove_events_t *info, const snd_seq_addr_t *addr)
{
	assert(info);
	info->dest = *(struct sndrv_seq_addr *)addr;
}

/**
 * \brief Set the channel as removal condition
 * \param info remove_events container
 * \param channel channel number
 */
void snd_seq_remove_events_set_channel(snd_seq_remove_events_t *info, int channel)
{
	assert(info);
	info->channel = channel;
}

/**
 * \brief Set the event type as removal condition
 * \param info remove_events container
 * \param type event type
 */
void snd_seq_remove_events_set_event_type(snd_seq_remove_events_t *info, int type)
{
	assert(info);
	info->type = type;
}

/**
 * \brief Set the event tag as removal condition
 * \param info remove_events container
 * \param tag tag id
 */
void snd_seq_remove_events_set_tag(snd_seq_remove_events_t *info, int tag)
{
	assert(info);
	info->tag = tag;
}


/* compare timestamp between events */
/* return 1 if a >= b; otherwise return 0 */
static inline int snd_seq_compare_tick_time(snd_seq_tick_time_t *a, snd_seq_tick_time_t *b)
{
	/* compare ticks */
	return (*a >= *b);
}

static inline int snd_seq_compare_real_time(snd_seq_real_time_t *a, snd_seq_real_time_t *b)
{
	/* compare real time */
	if (a->tv_sec > b->tv_sec)
		return 1;
	if ((a->tv_sec == b->tv_sec) && (a->tv_nsec >= b->tv_nsec))
		return 1;
	return 0;
}

/* Routine to match events to be removed */
static int remove_match(snd_seq_remove_events_t *info, snd_seq_event_t *ev)
{
	int res;

	if (info->remove_mode & SNDRV_SEQ_REMOVE_DEST) {
		if (ev->dest.client != info->dest.client ||
				ev->dest.port != info->dest.port)
			return 0;
	}
	if (info->remove_mode & SNDRV_SEQ_REMOVE_DEST_CHANNEL) {
		if (! sndrv_seq_ev_is_channel_type(ev))
			return 0;
		/* data.note.channel and data.control.channel are identical */
		if (ev->data.note.channel != info->channel)
			return 0;
	}
	if (info->remove_mode & SNDRV_SEQ_REMOVE_TIME_AFTER) {
		if (info->remove_mode & SNDRV_SEQ_REMOVE_TIME_TICK)
			res = snd_seq_compare_tick_time(&ev->time.tick, (snd_seq_tick_time_t *)&info->time.tick);
		else
			res = snd_seq_compare_real_time(&ev->time.time, (snd_seq_real_time_t *)&info->time.time);
		if (!res)
			return 0;
	}
	if (info->remove_mode & SNDRV_SEQ_REMOVE_TIME_BEFORE) {
		if (info->remove_mode & SNDRV_SEQ_REMOVE_TIME_TICK)
			res = snd_seq_compare_tick_time(&ev->time.tick, (snd_seq_tick_time_t *)&info->time.tick);
		else
			res = snd_seq_compare_real_time(&ev->time.time, (snd_seq_real_time_t *)&info->time.time);
		if (res)
			return 0;
	}
	if (info->remove_mode & SNDRV_SEQ_REMOVE_EVENT_TYPE) {
		if (ev->type != info->type)
			return 0;
	}
	if (info->remove_mode & SNDRV_SEQ_REMOVE_IGNORE_OFF) {
		/* Do not remove off events */
		switch (ev->type) {
		case SNDRV_SEQ_EVENT_NOTEOFF:
		/* case SNDRV_SEQ_EVENT_SAMPLE_STOP: */
			return 0;
		default:
			break;
		}
	}
	if (info->remove_mode & SNDRV_SEQ_REMOVE_TAG_MATCH) {
		if (info->tag != ev->tag)
			return 0;
	}

	return 1;
}

/**
 * \brief remove events on input/output buffers
 * \param seq sequencer handle
 * \param rmp remove event container
 *
 * Removes matching events with the given condition from input/output buffers.
 * The removal condition is specified in rmp argument.
 */
int snd_seq_remove_events(snd_seq_t *seq, snd_seq_remove_events_t *rmp)
{
	if (rmp->remove_mode & SNDRV_SEQ_REMOVE_INPUT) {
		/*
		 * First deal with any events that are still buffered
		 * in the library.
		 */
		snd_seq_drop_input_buffer(seq);
	}

	if (rmp->remove_mode & SNDRV_SEQ_REMOVE_OUTPUT) {
		/*
		 * First deal with any events that are still buffered
		 * in the library.
		 */
		 if (rmp->remove_mode & ~(SNDRV_SEQ_REMOVE_INPUT|SNDRV_SEQ_REMOVE_OUTPUT)) {
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
 * \brief get size of #snd_seq_client_pool_t
 * \return size in bytes
 */
size_t snd_seq_client_pool_sizeof()
{
	return sizeof(snd_seq_client_pool_t);
}

/**
 * \brief allocate an empty #snd_seq_client_pool_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_seq_client_pool_malloc(snd_seq_client_pool_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_seq_client_pool_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_seq_client_pool_t
 * \param pointer to object to free
 */
void snd_seq_client_pool_free(snd_seq_client_pool_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_seq_client_pool_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_seq_client_pool_copy(snd_seq_client_pool_t *dst, const snd_seq_client_pool_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Get the client id of a queue_info container
 * \param info client_pool container
 * \return client id
 */
int snd_seq_client_pool_get_client(const snd_seq_client_pool_t *info)
{
	assert(info);
	return info->client;
}

/**
 * \brief Get the output pool size of a queue_info container
 * \param info client_pool container
 * \return output pool size
 */
size_t snd_seq_client_pool_get_output_pool(const snd_seq_client_pool_t *info)
{
	assert(info);
	return info->output_pool;
}

/**
 * \brief Get the input pool size of a queue_info container
 * \param info client_pool container
 * \return input pool size
 */
size_t snd_seq_client_pool_get_input_pool(const snd_seq_client_pool_t *info)
{
	assert(info);
	return info->input_pool;
}

/**
 * \brief Get the output room size of a queue_info container
 * \param info client_pool container
 * \return output room size
 */
size_t snd_seq_client_pool_get_output_room(const snd_seq_client_pool_t *info)
{
	assert(info);
	return info->output_room;
}

/**
 * \brief Get the available size on output pool of a queue_info container
 * \param info client_pool container
 * \return available output size
 */
size_t snd_seq_client_pool_get_output_free(const snd_seq_client_pool_t *info)
{
	assert(info);
	return info->output_free;
}

/**
 * \brief Get the available size on input pool of a queue_info container
 * \param info client_pool container
 * \return available input size
 */
size_t snd_seq_client_pool_get_input_free(const snd_seq_client_pool_t *info)
{
	assert(info);
	return info->input_free;
}

/**
 * \brief Set the output pool size of a queue_info container
 * \param info client_pool container
 * \param size output pool size
 */
void snd_seq_client_pool_set_output_pool(snd_seq_client_pool_t *info, size_t size)
{
	assert(info);
	info->output_pool = size;
}

/**
 * \brief Set the input pool size of a queue_info container
 * \param info client_pool container
 * \param size input pool size
 */
void snd_seq_client_pool_set_input_pool(snd_seq_client_pool_t *info, size_t size)
{
	assert(info);
	info->input_pool = size;
}

/**
 * \brief Set the output room size of a queue_info container
 * \param info client_pool container
 * \param size output room size
 */
void snd_seq_client_pool_set_output_room(snd_seq_client_pool_t *info, size_t size)
{
	assert(info);
	info->output_room = size;
}


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
 * The client field in \a info is replaced automatically with the current id.
 */
int snd_seq_set_client_pool(snd_seq_t *seq, snd_seq_client_pool_t *info)
{
	assert(seq && info);
	info->client = seq->client;
	return seq->ops->set_client_pool(seq, info);
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


/**
 * instrument layer
 */

/**
 * \brief get size of #snd_instr_header_t
 * \return size in bytes
 */
size_t snd_instr_header_sizeof(void)
{
	return sizeof(snd_instr_header_t);
}

/**
 * \brief allocate an empty #snd_instr_header_t using standard malloc
 * \param ptr returned pointer
 * \param len additional data length
 * \return 0 on success otherwise negative error code
 */
int snd_instr_header_malloc(snd_instr_header_t **ptr, size_t len)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_instr_header_t) + len);
	if (!*ptr)
		return -ENOMEM;
	(*ptr)->len = len;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_instr_header_t
 * \param pointer to object to free
 */
void snd_instr_header_free(snd_instr_header_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_instr_header_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_instr_header_copy(snd_instr_header_t *dst, const snd_instr_header_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Get the instrument id of an instr_header container
 * \param info instr_header container
 * \return instrument id pointer
 */
const snd_seq_instr_t *snd_instr_header_get_id(const snd_instr_header_t *info)
{
	assert(info);
	return (snd_seq_instr_t *)&info->id.instr;
}

/**
 * \brief Get the cluster id of an instr_header container
 * \param info instr_header container
 * \return cluster id
 */
snd_seq_instr_cluster_t snd_instr_header_get_cluster(const snd_instr_header_t *info)
{
	assert(info);
	return info->id.cluster;
}

/**
 * \brief Get the command of an instr_header container
 * \param info instr_header container
 * \return command type
 */
unsigned int snd_instr_header_get_cmd(const snd_instr_header_t *info)
{
	assert(info);
	return info->cmd;
}

/**
 * \brief Get the length of extra data of an instr_header container
 * \param info instr_header container
 * \return the length in bytes
 */
size_t snd_instr_header_get_len(const snd_instr_header_t *info)
{
	assert(info);
	return info->len;
}

/**
 * \brief Get the data name of an instr_header container
 * \param info instr_header container
 * \return the name string
 */
const char *snd_instr_header_get_name(const snd_instr_header_t *info)
{
	assert(info);
	return info->data.name;
}

/**
 * \brief Get the data type of an instr_header container
 * \param info instr_header container
 * \return the data type
 */
int snd_instr_header_get_type(const snd_instr_header_t *info)
{
	assert(info);
	return info->data.type;
}

/**
 * \brief Get the data format of an instr_header container
 * \param info instr_header container
 * \return the data format string
 */
const char *snd_instr_header_get_format(const snd_instr_header_t *info)
{
	assert(info);
	return info->data.data.format;
}

/**
 * \brief Get the data alias of an instr_header container
 * \param info instr_header container
 * \return the data alias id
 */
const snd_seq_instr_t *snd_instr_header_get_alias(const snd_instr_header_t *info)
{
	assert(info);
	return (snd_seq_instr_t *)&info->data.data.alias;
}

/**
 * \brief Get the extra data pointer of an instr_header container
 * \param info instr_header container
 * \return the extra data pointer
 */
void *snd_instr_header_get_data(const snd_instr_header_t *info)
{
	assert(info);
	return (void*)((char*)info + sizeof(*info));
}

/**
 * \brief Get the flag to follow alias of an instr_header container
 * \param info instr_header container
 * \return 1 if follow alias
 */
int snd_instr_header_get_follow_alias(const snd_instr_header_t *info)
{
	assert(info);
	return (info->flags & SNDRV_SEQ_INSTR_QUERY_FOLLOW_ALIAS) ? 1 : 0;
}

/**
 * \brief Set the instrument id of an instr_header container
 * \param info instr_header container
 * \param id instrument id pointer
 */
void snd_instr_header_set_id(snd_instr_header_t *info, const snd_seq_instr_t *id)
{
	assert(info && id);
	info->id.instr = *(struct sndrv_seq_instr *)id;
}

/**
 * \brief Set the cluster id of an instr_header container
 * \param info instr_header container
 * \param cluster cluster id
 */
void snd_instr_header_set_cluster(snd_instr_header_t *info, snd_seq_instr_cluster_t cluster)
{
	assert(info);
	info->id.cluster = cluster;
}

/**
 * \brief Set the command of an instr_header container
 * \param info instr_header container
 * \param cmd command type
 */
void snd_instr_header_set_cmd(snd_instr_header_t *info, unsigned int cmd)
{
	assert(info);
	info->cmd = cmd;
}

/**
 * \brief Set the length of extra data of an instr_header container
 * \param info instr_header container
 * \param len size of extra data in bytes
 */
void snd_instr_header_set_len(snd_instr_header_t *info, size_t len)
{
	assert(info);
	info->len = len;
}

/**
 * \brief Set the data name of an instr_header container
 * \param info instr_header container
 * \param name the name string
 */
void snd_instr_header_set_name(snd_instr_header_t *info, const char *name)
{
	assert(info && name);
	strncpy(info->data.name, name, sizeof(info->data.name));
}

/**
 * \brief Set the data type of an instr_header container
 * \param info instr_header container
 * \param type the data type
 */
void snd_instr_header_set_type(snd_instr_header_t *info, int type)
{
	assert(info);
	info->data.type = type;
}

/**
 * \brief Set the data format of an instr_header container
 * \param info instr_header container
 * \param format the data format string
 */
void snd_instr_header_set_format(snd_instr_header_t *info, const char *format)
{
	assert(info && format);
	strncpy(info->data.data.format, format, sizeof(info->data.data.format));
}

/**
 * \brief Set the data alias id of an instr_header container
 * \param info instr_header container
 * \param instr alias instrument id
 */
void snd_instr_header_set_alias(snd_instr_header_t *info, const snd_seq_instr_t *instr)
{
	assert(info && instr);
	info->data.data.alias = *(struct sndrv_seq_instr *)instr;
}

/**
 * \brief Set the flag to follow alias of an instr_header container
 * \param info instr_header container
 * \param val 1 if follow alias
 */
void snd_instr_header_set_follow_alias(snd_instr_header_t *info, int val)
{
	assert(info);
	if (val)
		info->flags |= SNDRV_SEQ_INSTR_QUERY_FOLLOW_ALIAS;
	else
		info->flags &= ~SNDRV_SEQ_INSTR_QUERY_FOLLOW_ALIAS;
}
