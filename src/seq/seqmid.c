/*
 *  Sequencer Interface - middle-level routines
 *
 *  Copyright (c) 1999 by Takashi Iwai <tiwai@suse.de>
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

#include "seq_local.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/ioctl.h>

/**
 * \brief queue controls - start/stop/continue
 * \param seq sequencer handle
 * \param q queue id to control
 * \param type event type
 * \param value event value
 * \param ev event instance
 *
 * This function sets up general queue control event and sends it.
 * To send at scheduled time, set the schedule in \a ev.
 * If \a ev is NULL, the event is composed locally and sent immediately
 * to the specified queue.  In any cases, you need to call #snd_seq_drain_output()
 * appropriately to feed the event.
 *
 * \sa snd_seq_alloc_queue()
 */
int snd_seq_control_queue(snd_seq_t *seq, int q, int type, int value, snd_seq_event_t *ev)
{
	snd_seq_event_t tmpev;
	if (ev == NULL) {
		snd_seq_ev_clear(&tmpev);
		ev = &tmpev;
		snd_seq_ev_set_direct(ev);
	}
	snd_seq_ev_set_queue_control(ev, type, q, value);
	return snd_seq_event_output(seq, ev);
}


/**
 * \brief create a port - simple version
 * \param seq sequencer handle
 * \param name the name of the port
 * \param caps capability bits
 * \param type type bits
 * \return the created port number or negative error code
 *
 * Creates a port with the given capability and type bits.
 *
 * \sa snd_seq_create_port(), snd_seq_delete_simple_port()
 */
int snd_seq_create_simple_port(snd_seq_t *seq, const char *name,
			       unsigned int caps, unsigned int type)
{
	snd_seq_port_info_t pinfo;
	int result;

	memset(&pinfo, 0, sizeof(pinfo));
	if (name)
		strncpy(pinfo.name, name, sizeof(pinfo.name) - 1);
	pinfo.capability = caps;
	pinfo.type = type;
	pinfo.midi_channels = 16;
	pinfo.midi_voices = 64; /* XXX */
	pinfo.synth_voices = 0; /* XXX */

	result = snd_seq_create_port(seq, &pinfo);
	if (result < 0)
		return result;
	else
		return pinfo.addr.port;
}

/**
 * \brief delete the port
 * \param seq sequencer handle
 * \param port port id
 * \return 0 on success or negative error code
 *
 * \sa snd_seq_delete_port(), snd_seq_create_simple_port()
 */
int snd_seq_delete_simple_port(snd_seq_t *seq, int port)
{
	return snd_seq_delete_port(seq, port);
}

/**
 * \brief simple subscription (w/o exclusive & time conversion)
 * \param seq sequencer handle
 * \param myport the port id as receiver
 * \param src_client sender client id
 * \param src_port sender port id
 * \return 0 on success or negative error code
 *
 * Connect from the given sender client:port to the given destination port in the
 * current client.
 *
 * \sa snd_seq_subscribe_port(), snd_seq_disconnect_from()
 */
int snd_seq_connect_from(snd_seq_t *seq, int myport, int src_client, int src_port)
{
	snd_seq_port_subscribe_t subs;
	
	memset(&subs, 0, sizeof(subs));
	subs.sender.client = src_client;
	subs.sender.port = src_port;
	/*subs.dest.client = seq->client;*/
	subs.dest.client = snd_seq_client_id(seq);
	subs.dest.port = myport;

	return snd_seq_subscribe_port(seq, &subs);
}

/**
 * \brief simple subscription (w/o exclusive & time conversion)
 * \param seq sequencer handle
 * \param myport the port id as sender
 * \param dest_client destination client id
 * \param dest_port destination port id
 * \return 0 on success or negative error code
 *
 * Connect from the given receiver port in the current client
 * to the given destination client:port.
 *
 * \sa snd_seq_subscribe_port(), snd_seq_disconnect_to()
 */
int snd_seq_connect_to(snd_seq_t *seq, int myport, int dest_client, int dest_port)
{
	snd_seq_port_subscribe_t subs;
	
	memset(&subs, 0, sizeof(subs));
	/*subs.sender.client = seq->client;*/
	subs.sender.client = snd_seq_client_id(seq);
	subs.sender.port = myport;
	subs.dest.client = dest_client;
	subs.dest.port = dest_port;

	return snd_seq_subscribe_port(seq, &subs);
}

/**
 * \brief simple disconnection
 * \param seq sequencer handle
 * \param myport the port id as receiver
 * \param src_client sender client id
 * \param src_port sender port id
 * \return 0 on success or negative error code
 *
 * Remove connection from the given sender client:port
 * to the given destination port in the current client.
 *
 * \sa snd_seq_unsubscribe_port(), snd_seq_connect_from()
 */
int snd_seq_disconnect_from(snd_seq_t *seq, int myport, int src_client, int src_port)
{
	snd_seq_port_subscribe_t subs;
	
	memset(&subs, 0, sizeof(subs));
	subs.sender.client = src_client;
	subs.sender.port = src_port;
	/*subs.dest.client = seq->client;*/
	subs.dest.client = snd_seq_client_id(seq);
	subs.dest.port = myport;

	return snd_seq_unsubscribe_port(seq, &subs);
}

/**
 * \brief simple disconnection
 * \param seq sequencer handle
 * \param myport the port id as sender
 * \param dest_client destination client id
 * \param dest_port destination port id
 * \return 0 on success or negative error code
 *
 * Remove connection from the given sender client:port
 * to the given destination port in the current client.
 *
 * \sa snd_seq_unsubscribe_port(), snd_seq_connect_to()
 */
int snd_seq_disconnect_to(snd_seq_t *seq, int myport, int dest_client, int dest_port)
{
	snd_seq_port_subscribe_t subs;
	
	memset(&subs, 0, sizeof(subs));
	/*subs.sender.client = seq->client;*/
	subs.sender.client = snd_seq_client_id(seq);
	subs.sender.port = myport;
	subs.dest.client = dest_client;
	subs.dest.port = dest_port;

	return snd_seq_unsubscribe_port(seq, &subs);
}

/*
 * set client information
 */

/**
 * \brief set client name
 * \param seq sequencer handle
 * \param name name string
 * \return 0 on success or negative error code
 *
 * \sa snd_seq_set_client_info()
 */
int snd_seq_set_client_name(snd_seq_t *seq, const char *name)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	strncpy(info.name, name, sizeof(info.name) - 1);
	return snd_seq_set_client_info(seq, &info);
}

/**
 * \brief add client event filter
 * \param seq sequencer handle
 * \param event_type event type to be added
 * \return 0 on success or negative error code
 *
 * \sa snd_seq_set_client_info()
 */
int snd_seq_set_client_event_filter(snd_seq_t *seq, int event_type)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	snd_seq_client_info_event_filter_add(&info, event_type);
	return snd_seq_set_client_info(seq, &info);
}

/**
 * \brief set client MIDI protocol version
 * \param seq sequencer handle
 * \param midi_version MIDI protocol version to set
 * \return 0 on success or negative error code
 *
 * \sa snd_seq_set_client_info()
 */
int snd_seq_set_client_midi_version(snd_seq_t *seq, int midi_version)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	snd_seq_client_info_set_midi_version(&info, midi_version);
	return snd_seq_set_client_info(seq, &info);
}

/**
 * \brief enable/disable client's automatic conversion of UMP/legacy events
 * \param seq sequencer handle
 * \param enable 0 or 1 to disable/enable the conversion
 * \return 0 on success or negative error code
 *
 * \sa snd_seq_set_client_info()
 */
int snd_seq_set_client_ump_conversion(snd_seq_t *seq, int enable)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	snd_seq_client_info_set_ump_conversion(&info, enable);
	return snd_seq_set_client_info(seq, &info);
}

/**
 * \brief change the output pool size of the given client
 * \param seq sequencer handle
 * \param size output pool size
 * \return 0 on success or negative error code
 *
 * \sa snd_seq_set_client_pool()
 */
int snd_seq_set_client_pool_output(snd_seq_t *seq, size_t size)
{
	snd_seq_client_pool_t info;
	int err;

	if ((err = snd_seq_get_client_pool(seq, &info)) < 0)
		return err;
	info.output_pool = size;
	return snd_seq_set_client_pool(seq, &info);
}

/**
 * \brief change the output room size of the given client
 * \param seq sequencer handle
 * \param size output room size
 * \return 0 on success or negative error code
 *
 * \sa snd_seq_set_client_pool()
 */
int snd_seq_set_client_pool_output_room(snd_seq_t *seq, size_t size)
{
	snd_seq_client_pool_t info;
	int err;

	if ((err = snd_seq_get_client_pool(seq, &info)) < 0)
		return err;
	info.output_room = size;
	return snd_seq_set_client_pool(seq, &info);
}

/**
 * \brief change the input pool size of the given client
 * \param seq sequencer handle
 * \param size input pool size
 * \return 0 on success or negative error code
 *
 * \sa snd_seq_set_client_pool()
 */
int snd_seq_set_client_pool_input(snd_seq_t *seq, size_t size)
{
	snd_seq_client_pool_t info;
	int err;

	if ((err = snd_seq_get_client_pool(seq, &info)) < 0)
		return err;
	info.input_pool = size;
	return snd_seq_set_client_pool(seq, &info);
}

/**
 * \brief reset client output pool
 * \param seq sequencer handle
 * \return 0 on success or negative error code
 *
 * So far, this works identically like #snd_seq_drop_output().
 */
int snd_seq_reset_pool_output(snd_seq_t *seq)
{
	return snd_seq_drop_output(seq);
}

/**
 * \brief reset client input pool
 * \param seq sequencer handle
 * \return 0 on success or negative error code
 *
 * So far, this works identically like #snd_seq_drop_input().
 */
int snd_seq_reset_pool_input(snd_seq_t *seq)
{
	return snd_seq_drop_input(seq);
}

/**
 * \brief wait until all events are processed
 * \param seq sequencer handle
 * \return 0 on success or negative error code
 *
 * This function waits until all events of this client are processed.
 *
 * \sa snd_seq_drain_output()
 */
int snd_seq_sync_output_queue(snd_seq_t *seq)
{
	int err;
	snd_seq_client_pool_t info;
	int saved_room;
	struct pollfd pfd;

	assert(seq);
	/* reprogram the room size to full */
	if ((err = snd_seq_get_client_pool(seq, &info)) < 0)
		return err;
	saved_room = info.output_room;
	info.output_room = info.output_pool; /* wait until all gone */
	if ((err = snd_seq_set_client_pool(seq, &info)) < 0)
		return err;
	/* wait until all events are purged */
	pfd.fd = seq->poll_fd;
	pfd.events = POLLOUT;
	err = poll(&pfd, 1, -1);
	/* restore the room size */ 
	info.output_room = saved_room;
	snd_seq_set_client_pool(seq, &info);
	return err;
}

/**
 * \brief parse the given string and get the sequencer address
 * \param seq sequencer handle
 * \param addr the address pointer to be returned
 * \param arg the string to be parsed
 * \return 0 on success or negative error code
 *
 * This function parses the sequencer client and port numbers from the given string.
 * The client and port tokens are separated by either colon or period, e.g. 128:1.
 * When \a seq is not NULL, the function accepts also a client name not only
 * digit numbers.
 * Actually \a arg need to be only a prefix of the wanted client.
 * That is, if a client named "Foobar XXL Master 2012" with number 128 is available,
 * then parsing "Foobar" will return the address 128:0 if no other client is
 * an exact match.
 */
int snd_seq_parse_address(snd_seq_t *seq, snd_seq_addr_t *addr, const char *arg)
{
	char *p, *buf;
	const char *s;
	char c;
	long client, port = 0;
	int len;

	assert(addr && arg);

	c = *arg;
	if (c == '"' || c == '\'') {
		s = ++arg;
		while (*s && *s != c) s++;
		len = s - arg;
		if (*s)
			s++;
		if (*s) {
			if (*s != '.' && *s != ':')
				return -EINVAL;
			if ((port = atoi(s + 1)) < 0)
				return -EINVAL;
		}
	} else {
		if ((p = strpbrk(arg, ":.")) != NULL) {
			if ((port = atoi(p + 1)) < 0)
				return -EINVAL;
			len = (int)(p - arg); /* length of client name */
		} else {
			len = strlen(arg);
		}
	}
	if (len == 0)
		return -EINVAL;
	buf = alloca(len + 1);
	strncpy(buf, arg, len);
	buf[len] = '\0';
	addr->port = port;
	if (safe_strtol(buf, &client) == 0) {
		addr->client = client;
	} else {
		/* convert from the name */
		snd_seq_client_info_t cinfo;

		if (! seq)
			return -EINVAL;
		if (len <= 0)
			return -EINVAL;
		client = -1;
		cinfo.client = -1;
		while (snd_seq_query_next_client(seq, &cinfo) >= 0) {
			if (!strncmp(arg, cinfo.name, len)) {
				if (strlen(cinfo.name) == (size_t)len) {
					/* exact match */
					addr->client = cinfo.client;
					return 0;
				}
				if (client < 0)
					client = cinfo.client;
			}
		}
		if (client >= 0) {
			/* prefix match */
			addr->client = client;
			return 0;
		}
		return -ENOENT; /* not found */
	}
	return 0;
}

/**
 * \brief create a UMP Endpoint for the given sequencer client
 * \param seq sequencer handle
 * \param info UMP Endpoint information to initialize
 * \param num_groups max number of groups in the endpoint
 * \return 0 on success or negative error code
 *
 * This function initializes the sequencer client to the corresponding
 * MIDI 2.0 mode (either MIDI 1.0 or MIDI 2.0 protocol) depending on the
 * given snd_ump_endpoint_info_t info.
 *
 * This function should be called right after opening a sequencer client.
 * The client name is updated from the UMP Endpoint name, and a primary
 * MIDI 2.0 UMP port and each UMP Group port are created.
 * The application should pass each UMP block info via succeeding
 * snd_seq_create_ump_block() call.
 */
int snd_seq_create_ump_endpoint(snd_seq_t *seq,
				const snd_ump_endpoint_info_t *info,
				unsigned int num_groups)
{
	int err, version;
	unsigned int i;
	snd_seq_port_info_t *pinfo;

	if (seq->ump_ep)
		return -EBUSY;

	if (num_groups < 1 || num_groups > SND_UMP_MAX_GROUPS)
		return -EINVAL;

	if (!(info->protocol_caps & info->protocol)) {
		SNDERR("Inconsistent UMP protocol_caps and protocol\n");
		return -EINVAL;
	}

	if (info->protocol & SND_UMP_EP_INFO_PROTO_MIDI2) {
		version = SND_SEQ_CLIENT_UMP_MIDI_2_0;
	} else if (info->protocol & SND_UMP_EP_INFO_PROTO_MIDI1) {
		version = SND_SEQ_CLIENT_UMP_MIDI_1_0;
	} else {
		SNDERR("Invalid UMP protocol set 0x%x\n", info->protocol);
		return -EINVAL;
	}

	err = snd_seq_set_client_midi_version(seq, version);
	if (err < 0) {
		SNDERR("Failed to set to MIDI protocol 0x%x\n", version);
		return err;
	}

	seq->ump_ep = malloc(sizeof(*info));
	if (!seq->ump_ep)
		return -ENOMEM;

	*seq->ump_ep = *info;
	if (!seq->ump_ep->version)
		seq->ump_ep->version = SND_UMP_EP_INFO_DEFAULT_VERSION;

	if (info->name) {
		err = snd_seq_set_client_name(seq, (const char *)info->name);
		if (err < 0)
			goto error_free;
	}

	err = snd_seq_set_ump_endpoint_info(seq, seq->ump_ep);
	if (err < 0) {
		SNDERR("Failed to set UMP EP info\n");
		goto error_free;
	}

	snd_seq_port_info_alloca(&pinfo);

	snd_seq_port_info_set_port(pinfo, 0);
	snd_seq_port_info_set_port_specified(pinfo, 1);
	snd_seq_port_info_set_name(pinfo, "MIDI 2.0");
	snd_seq_port_info_set_capability(pinfo,
					 SNDRV_SEQ_PORT_CAP_READ |
					 SNDRV_SEQ_PORT_CAP_SYNC_READ |
					 SNDRV_SEQ_PORT_CAP_SUBS_READ |
					 SNDRV_SEQ_PORT_CAP_WRITE |
					 SNDRV_SEQ_PORT_CAP_SYNC_WRITE |
					 SNDRV_SEQ_PORT_CAP_SUBS_WRITE |
					 SNDRV_SEQ_PORT_CAP_DUPLEX);
	snd_seq_port_info_set_type(pinfo,
				   SND_SEQ_PORT_TYPE_MIDI_GENERIC |
				   SNDRV_SEQ_PORT_TYPE_MIDI_UMP |
				   SND_SEQ_PORT_TYPE_APPLICATION |
				   SNDRV_SEQ_PORT_TYPE_PORT);
	snd_seq_port_info_set_ump_group(pinfo,
					SND_SEQ_PORT_TYPE_MIDI_GENERIC |
				   SNDRV_SEQ_PORT_TYPE_MIDI_UMP |
				   SND_SEQ_PORT_TYPE_APPLICATION |
				   SNDRV_SEQ_PORT_TYPE_PORT);
	err = snd_seq_create_port(seq, pinfo);
	if (err < 0) {
		SNDERR("Failed to create MIDI 2.0 port\n");
		goto error_free;
	}

	for (i = 0; i < num_groups; i++) {
		char name[32];

		snd_seq_port_info_set_port(pinfo, i + 1);
		snd_seq_port_info_set_port_specified(pinfo, 1);
		sprintf(name, "Group %d", i + 1);
		snd_seq_port_info_set_capability(pinfo, 0); /* set later */
		snd_seq_port_info_set_name(pinfo, name);
		snd_seq_port_info_set_ump_group(pinfo, i + 1);
		err = snd_seq_create_port(seq, pinfo);
		if (err < 0) {
			SNDERR("Failed to create Group port %d\n", i + 1);
			goto error;
		}
	}

	seq->num_ump_groups = num_groups;
	return 0;

 error:
	/* delete all ports including port 0 */
	for (i = 0; i <= num_groups; i++)
		snd_seq_delete_port(seq, i);
 error_free:
	free(seq->ump_ep);
	seq->ump_ep = NULL;
	return err;
}

/* update each port name and capability from the block list */
static void update_group_ports(snd_seq_t *seq, snd_ump_endpoint_info_t *ep)
{
	unsigned int i, b;
	snd_seq_port_info_t *pinfo;
	snd_ump_block_info_t *bp;

	snd_seq_port_info_alloca(&pinfo);

	for (i = 0; i < seq->num_ump_groups; i++) {
		char blknames[64];
		char name[64];
		unsigned int caps = 0;
		int len;

		blknames[0] = 0;
		for (b = 0; b < ep->num_blocks; b++) {
			bp = seq->ump_blks[b];
			if (!bp)
				continue;
			if (i < bp->first_group ||
			    i >= bp->first_group + bp->num_groups)
				continue;
			switch (bp->direction) {
			case SNDRV_UMP_DIR_INPUT: /* sink, receiver */
				caps |= SNDRV_SEQ_PORT_CAP_WRITE |
					SNDRV_SEQ_PORT_CAP_SYNC_WRITE |
					SNDRV_SEQ_PORT_CAP_SUBS_WRITE;
				break;
			case SNDRV_UMP_DIR_OUTPUT: /* source, transmitter */
				caps |= SNDRV_SEQ_PORT_CAP_READ |
					SNDRV_SEQ_PORT_CAP_SYNC_READ |
					SNDRV_SEQ_PORT_CAP_SUBS_READ;
				break;
			case SNDRV_UMP_DIR_BIDIRECTION:
				caps |= SNDRV_SEQ_PORT_CAP_READ |
					SNDRV_SEQ_PORT_CAP_SYNC_READ |
					SNDRV_SEQ_PORT_CAP_SUBS_READ |
					SNDRV_SEQ_PORT_CAP_WRITE |
					SNDRV_SEQ_PORT_CAP_SYNC_WRITE |
					SNDRV_SEQ_PORT_CAP_SUBS_WRITE |
					SNDRV_SEQ_PORT_CAP_DUPLEX;
				break;
			}

			if (!*bp->name)
				continue;
			len = strlen(blknames);
			if (len)
				snprintf(blknames + len, sizeof(blknames) - len,
					 ", %s", bp->name);
			else
				snd_strlcpy(blknames, (const char *)bp->name,
					    sizeof(blknames));
		}

		if (!*blknames)
			continue;

		snprintf(name, sizeof(name), "Group %d (%s)", i + 1, blknames);
		if (snd_seq_get_port_info(seq, i + 1, pinfo) < 0)
			continue;

		if (strcmp(name, snd_seq_port_info_get_name(pinfo)) ||
		    snd_seq_port_info_get_capability(pinfo) != caps) {
			snd_seq_port_info_set_name(pinfo, name);
			snd_seq_port_info_set_capability(pinfo, caps);
			snd_seq_set_port_info(seq, i + 1, pinfo);
		}
	}
}

/**
 * \brief create a UMP block for the given sequencer client
 * \param seq sequencer handle
 * \param blkid 0-based block id
 * \param info UMP block info to initialize
 * \return 0 on success or negative error code
 *
 * This function sets up the UMP block info of the given block id.
 * The sequencer port name is updated accordingly with the associated
 * block name automatically.
 */
int snd_seq_create_ump_block(snd_seq_t *seq, int blkid,
			     const snd_ump_block_info_t *info)
{
	snd_ump_block_info_t *bp;
	snd_ump_endpoint_info_t *ep = seq->ump_ep;
	int err;

	if (!ep)
		return -EINVAL;
	if (info->first_group >= seq->num_ump_groups ||
	    info->first_group + info->num_groups > seq->num_ump_groups)
		return -EINVAL;
	if (blkid < 0 || blkid >= (int)ep->num_blocks)
		return -EINVAL;

	if (seq->ump_blks[blkid])
		return -EBUSY;
	seq->ump_blks[blkid] = bp = malloc(sizeof(*info));
	if (!bp)
		return -ENOMEM;
	*bp = *info;

	if (!bp->midi_ci_version)
		bp->midi_ci_version = SND_UMP_BLOCK_INFO_DEFAULT_MIDI_CI_VERSION;
	bp->active = 1;

	err = snd_seq_set_ump_block_info(seq, blkid, bp);
	if (err < 0) {
		SNDERR("Failed to set UMP EP info\n");
		free(bp);
		seq->ump_blks[blkid] = NULL;
		return err;
	}

	update_group_ports(seq, ep);
	return 0;
}
