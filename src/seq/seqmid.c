/*
 *  Sequencer Interface - middle-level routines
 *
 *  Copyright (c) 1999 by Takashi Iwai <iwai@ww.uni-erlangen.de>
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

/* direct passing (without queued) */
void snd_seq_ev_set_direct(snd_seq_event_t *ev)
{
	ev->flags &= ~SND_SEQ_DEST_MASK;
	ev->flags |= SND_SEQ_DEST_DIRECT;
	ev->dest.queue = SND_SEQ_ADDRESS_UNKNOWN; /* XXX */
}

/* queued on tick */
void snd_seq_ev_schedule_tick(snd_seq_event_t *ev, int q, int relative,
			      unsigned long tick)
{
	ev->flags &= ~(SND_SEQ_DEST_MASK | SND_SEQ_TIME_STAMP_MASK |
		       SND_SEQ_TIME_MODE_MASK);
	ev->flags |= SND_SEQ_DEST_QUEUE;
	ev->flags |= SND_SEQ_TIME_STAMP_TICK;
	ev->flags |= relative ? SND_SEQ_TIME_MODE_REL : SND_SEQ_TIME_MODE_ABS;
	ev->time.tick = tick;
}

/* queued on real-time */
void snd_seq_ev_schedule_real(snd_seq_event_t *ev, int q, int relative,
			      snd_seq_real_time_t *real)
{
	ev->flags &= ~(SND_SEQ_DEST_MASK | SND_SEQ_TIME_STAMP_MASK |
		       SND_SEQ_TIME_MODE_MASK);
	ev->flags |= SND_SEQ_DEST_QUEUE;
	ev->flags |= SND_SEQ_TIME_STAMP_REAL;
	ev->flags |= relative ? SND_SEQ_TIME_MODE_REL : SND_SEQ_TIME_MODE_ABS;
	ev->time.real = *real;
}

/* set event priority */
void snd_seq_ev_set_priority(snd_seq_event_t *ev, int high_prior)
{
	ev->flags &= ~SND_SEQ_PRIORITY_MASK;
	ev->flags |= high_prior ? SND_SEQ_PRIORITY_HIGH : SND_SEQ_PRIORITY_NORMAL;
}

/* set fixed data */
void snd_seq_ev_set_fixed(snd_seq_event_t *ev)
{
	ev->flags &= ~SND_SEQ_EVENT_LENGTH_MASK;
	ev->flags |= SND_SEQ_EVENT_LENGTH_FIXED;
}

/* set variable data */
void snd_seq_ev_set_variable(snd_seq_event_t *ev, int len, void *ptr)
{
	ev->flags &= ~SND_SEQ_EVENT_LENGTH_MASK;
	ev->flags |= SND_SEQ_EVENT_LENGTH_VARIABLE;
	ev->data.ext.len = len;
	ev->data.ext.ptr = ptr;
}

/* queue controls - start/stop/continue */
/* if ev is NULL, send events immediately.
   otherwise, duplicate the given event data.
   destination is overwritten to Timer port (0:0)
   */
int snd_seq_control_queue(snd_seq_t *seq, int q, int type, int value, snd_seq_event_t *ev)
{
	snd_seq_event_t tmpev;
	if (ev == NULL) {
		snd_seq_ev_clear(&tmpev);
		ev = &tmpev;
		snd_seq_ev_set_direct(ev);
	}

	ev->type = type;
	snd_seq_ev_set_dest(ev, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_TIMER);
#if 1
	/* new type */
	ev->data.queue.addr.queue = q;
	ev->data.queue.value = value;
#else
	/* old type */
	ev->dest.queue = q;
	ev->data.control.value = value;
#endif

	return snd_seq_event_output(seq, ev);
}


/* create a port - simple version
 * return the port number
 */
int snd_seq_create_simple_port(snd_seq_t *seq, char *name,
			       unsigned int caps, unsigned int type)
{
	snd_seq_port_info_t pinfo;
	int result;

	memset(&pinfo, 0, sizeof(pinfo));
	if (name)
		strncpy(pinfo.name, name, sizeof(pinfo.name) - 1);
	pinfo.capability = caps;
	pinfo.cap_group = caps;
	pinfo.type = type;
	pinfo.midi_channels = 16;
	pinfo.midi_voices = 64; /* XXX */
	pinfo.synth_voices = 0; /* XXX */
	pinfo.kernel = NULL;

	result = snd_seq_create_port(seq, &pinfo);
	if (result < 0)
		return result;
	else
		return pinfo.port;
}

/* delete the port */
int snd_seq_delete_simple_port(snd_seq_t *seq, int port)
{
	snd_seq_port_info_t pinfo;

	memset(&pinfo, 0, sizeof(pinfo));
	pinfo.port = port;

	return snd_seq_delete_port(seq, &pinfo);
}

/*
 * sipmle subscription (w/o exclusive & time conversion)
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
int snd_seq_set_client_name(snd_seq_t *seq, char *name)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	strncpy(info.name, name, sizeof(info.name) - 1);
	return snd_seq_set_client_info(seq, &info);
}

int snd_seq_set_client_group(snd_seq_t *seq, char *name)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	strncpy(info.group, name, sizeof(info.group) - 1);
	return snd_seq_set_client_info(seq, &info);
}

int snd_seq_set_client_filter(snd_seq_t *seq, unsigned int filter)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	info.filter = filter;
	return snd_seq_set_client_info(seq, &info);
}

int snd_seq_set_client_event_filter(snd_seq_t *seq, int event_type)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	info.filter |= SND_SEQ_FILTER_USE_EVENT;
	snd_seq_set_bit(event_type, info.event_filter);
	return snd_seq_set_client_info(seq, &info);
}

int snd_seq_set_client_pool_output(snd_seq_t *seq, int size)
{
	snd_seq_client_pool_t info;
	int err;

	if ((err = snd_seq_get_client_pool(seq, &info)) < 0)
		return err;
	info.output_pool = size;
	return snd_seq_set_client_pool(seq, &info);
}

int snd_seq_set_client_pool_output_room(snd_seq_t *seq, int size)
{
	snd_seq_client_pool_t info;
	int err;

	if ((err = snd_seq_get_client_pool(seq, &info)) < 0)
		return err;
	info.output_room = size;
	return snd_seq_set_client_pool(seq, &info);
}

int snd_seq_set_client_pool_input(snd_seq_t *seq, int size)
{
	snd_seq_client_pool_t info;
	int err;

	if ((err = snd_seq_get_client_pool(seq, &info)) < 0)
		return err;
	info.input_pool = size;
	return snd_seq_set_client_pool(seq, &info);
}

/*
 * reset client input/output pool
 * use REMOVE_EVENTS ioctl instead of RESET_POOL
 */
int snd_seq_reset_pool_output(snd_seq_t *seq)
{
	snd_seq_remove_events_t rmp;

	memset(&rmp, 0, sizeof(rmp));
	rmp.output = 1;
	rmp.remove_mode = 0; /* remove all */
	return snd_seq_remove_events(seq, &rmp);
}

int snd_seq_reset_pool_input(snd_seq_t *seq)
{
	snd_seq_remove_events_t rmp;

	memset(&rmp, 0, sizeof(rmp));
	rmp.input = 1;
	rmp.remove_mode = 0; /* remove all */
	return snd_seq_remove_events(seq, &rmp);
}

