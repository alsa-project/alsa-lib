/*
 * connect / disconnect two subscriber ports
 *   ver.0.1.2
 *
 * Copyright (C) 1999 Takashi Iwai
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/asoundlib.h>

#define DEFAULT_QUEUE	0

static void usage(void)
{
	fprintf(stderr, "connect / disconnect two subscriber ports\n");
	fprintf(stderr, "copyright (C) 1999 Takashi Iwai\n");
	fprintf(stderr, "usage: aconnect [-d] [-q queue] [-g group] sender receiver\n");
	fprintf(stderr, "            -d = disconnect\n");
	fprintf(stderr, "            sender, receiver = client:port\n");
	fprintf(stderr, "       aconnect -i [-g group] [-l]\n");
	fprintf(stderr, "            list input ports\n");
	fprintf(stderr, "       aconnect -o [-g group] [-l]\n");
	fprintf(stderr, "            list output ports\n");
}

/*
 * parse command line to client:port
 */
static void parse_address(snd_seq_addr_t *addr, char *arg)
{
	char *p;

	addr->client = atoi(arg);
	if ((p = strchr(arg, ':')) != NULL)
		addr->port = atoi(p + 1);
	else
		addr->port = 0;
}

/*
 * check permission (capability) of specified port
 */
static int check_permission(snd_seq_port_info_t *pinfo, char *group, int perm)
{
	if ((pinfo->capability & perm) == perm &&
	    ! (pinfo->capability & SND_SEQ_PORT_CAP_NO_EXPORT))
		return 1;
	if (*group && strcmp(pinfo->group, group) == 0 &&
	    (pinfo->cap_group & perm) == perm &&
	    ! (pinfo->cap_group & SND_SEQ_PORT_CAP_NO_EXPORT))
		return 1;
	return 0;
}

/*
 * list subscribers of specified type
 */
static void list_each_subs(snd_seq_t *seq, snd_seq_query_subs_t *subs, int type, char *msg)
{
	subs->type = type;
	subs->index = 0;
	while (snd_seq_query_port_subscribers(seq, subs) >= 0) {
		if (subs->index == 0)
			printf("\t%s: %d:%d:%d", msg,
			       subs->addr.queue, subs->addr.client, subs->addr.port);
		else
			printf(", %d:%d:%d", subs->addr.queue, subs->addr.client, subs->addr.port);
		subs->index++;
	}
	if (subs->index)
		printf("\n");
}

/*
 * list subscribers
 */
static void list_subscribers(snd_seq_t *seq, int client, int port)
{
	snd_seq_query_subs_t subs;
	memset(&subs, 0, sizeof(subs));
	subs.client = client;
	subs.port = port;
	list_each_subs(seq, &subs, SND_SEQ_QUERY_SUBS_READ, "Connecting To");
	list_each_subs(seq, &subs, SND_SEQ_QUERY_SUBS_WRITE, "Connected From");
}

/*
 * list all ports
 */
static void list_ports(snd_seq_t *seq, char *group, int perm, int list_subs)
{
	snd_seq_client_info_t cinfo;
	snd_seq_port_info_t pinfo;
	int client_printed;

	cinfo.client = -1;
	cinfo.name[0] = 0;
	cinfo.group[0] = 0;
	while (snd_seq_query_next_client(seq, &cinfo) >= 0) {
		/* reset query info */
		pinfo.client = cinfo.client;
		pinfo.port = -1;
		pinfo.name[0] = 0;
		strncpy(pinfo.group, group, sizeof(pinfo.group));
		client_printed = 0;
		while (snd_seq_query_next_port(seq, &pinfo) >= 0) {
			if (check_permission(&pinfo, group, perm)) {
				if (! client_printed) {
					printf("client %d: '%s' [group=%s] [type=%s]\n",
					       cinfo.client, cinfo.name, cinfo.group,
					       (cinfo.type == USER_CLIENT ? "user" : "kernel"));
					client_printed = 1;
				}
				printf("  %3d '%-16s' [group=%s]\n", pinfo.port, pinfo.name, pinfo.group);
				if (list_subs)
					list_subscribers(seq, pinfo.client, pinfo.port);
			}
			/* reset query names */
			pinfo.name[0] = 0;
			strncpy(pinfo.group, group, sizeof(pinfo.group));
		}
		/* reset query names */
		cinfo.name[0] = 0;
		cinfo.group[0] = 0;
	}
}


enum {
	SUBSCRIBE, UNSUBSCRIBE, LIST_INPUT, LIST_OUTPUT
};

int main(int argc, char **argv)
{
	int c;
	snd_seq_t *seq;
	int queue = DEFAULT_QUEUE;
	int command = SUBSCRIBE;
	char *group = "";
	int client;
	int list_subs = 0;
	snd_seq_client_info_t cinfo;
	snd_seq_port_subscribe_t subs;

	while ((c = getopt(argc, argv, "diog:D:q:l")) != -1) {
		switch (c) {
		case 'd':
			command = UNSUBSCRIBE;
			break;
		case 'i':
			command = LIST_INPUT;
			break;
		case 'o':
			command = LIST_OUTPUT;
			break;
		case 'g':
			group = optarg;
			break;
		case 'q':
			queue = atoi(optarg);
			break;
		case 'l':
			list_subs = 1;
			break;
		default:
			usage();
			exit(1);
		}
	}

	if (snd_seq_open(&seq, SND_SEQ_OPEN) < 0) {
		fprintf(stderr, "can't open sequencer\n");
		return 1;
	}
	
	if (command == LIST_INPUT) {
		list_ports(seq, group, SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ, list_subs);
		snd_seq_close(seq);
		return 0;
	} else if (command == LIST_OUTPUT) {
		list_ports(seq, group, SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE, list_subs);
		snd_seq_close(seq);
		return 0;
	}

	if (optind + 2 > argc) {
		snd_seq_close(seq);
		usage();
		exit(1);
	}

	if ((client = snd_seq_client_id(seq)) < 0) {
		snd_seq_close(seq);
		fprintf(stderr, "can't get client id\n");
		return 1;
	}

	/* set client info */
	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.client = client;
	cinfo.type = USER_CLIENT;
	strcpy(cinfo.name, "ALSA Connector");
	strncpy(cinfo.group, group, sizeof(cinfo.group) - 1);
	if (snd_seq_set_client_info(seq, &cinfo) < 0) {
		snd_seq_close(seq);
		fprintf(stderr, "can't set client info\n");
		return 0;
	}

	/* set subscription */
	memset(&subs, 0, sizeof(subs));
	parse_address(&subs.sender, argv[optind]);
	parse_address(&subs.dest, argv[optind + 1]);
	subs.sender.queue = subs.dest.queue = queue;
	subs.exclusive = 0;
	subs.convert_time = 0;
	subs.realtime = 0;

	if (command == UNSUBSCRIBE) {
		if (snd_seq_get_port_subscription(seq, &subs) < 0) {
			snd_seq_close(seq);
			fprintf(stderr, "No subscription is found\n");
			return 1;
		}
		if (snd_seq_unsubscribe_port(seq, &subs) < 0) {
			snd_seq_close(seq);
			fprintf(stderr, "Disconnection failed (errno=%d)\n", errno);
			return 1;
		}
	} else {
		if (snd_seq_get_port_subscription(seq, &subs) == 0) {
			snd_seq_close(seq);
			fprintf(stderr, "Connection is already subscribed\n");
			return 1;
		}
		if (snd_seq_subscribe_port(seq, &subs) < 0) {
			snd_seq_close(seq);
			fprintf(stderr, "Connection failed (errno=%d)\n", errno);
			return 1;
		}
	}

	snd_seq_close(seq);

	return 0;
}
