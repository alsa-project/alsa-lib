/*
 * connect / disconnect two subscriber ports
 *   ver.0.1
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
#include <linux/asound.h>
#include <linux/asequencer.h>

#define DEVICE_FILE	"/dev/snd/seq"
#define DEFAULT_QUEUE	0

static void usage(void)
{
	fprintf(stderr, "connect / disconnect two subscriber ports\n");
	fprintf(stderr, "copyright (C) 1999 Takashi Iwai\n");
	fprintf(stderr, "usage: aconnect [-d] [-q queue] [-g group] sender receiver\n");
	fprintf(stderr, "            -d = disconnect\n");
	fprintf(stderr, "            sender, receiver = client:port\n");
	fprintf(stderr, "       aconnect -i [-g group]\n");
	fprintf(stderr, "            list input ports\n");
	fprintf(stderr, "       aconnect -o [-g group]\n");
	fprintf(stderr, "            list output ports\n");
}

static void parse_address(snd_seq_addr_t *addr, char *arg)
{
	char *p;

	addr->client = atoi(arg);
	if ((p = strchr(arg, ':')) != NULL)
		addr->port = atoi(p + 1);
	else
		addr->port = 0;
}

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
 * list all ports
 */
static void list_ports(int fd, char *group, int perm)
{
	snd_seq_client_info_t cinfo;
	snd_seq_port_info_t pinfo;
	int client_printed;

	cinfo.client = -1;
	cinfo.name[0] = 0;
	cinfo.group[0] = 0;
	while (ioctl(fd, SND_SEQ_IOCTL_QUERY_NEXT_CLIENT, &cinfo) >= 0) {
		/* reset query info */
		pinfo.client = cinfo.client;
		pinfo.port = -1;
		pinfo.name[0] = 0;
		strncpy(pinfo.group, group, sizeof(pinfo.group));
		client_printed = 0;
		while (ioctl(fd, SND_SEQ_IOCTL_QUERY_NEXT_PORT, &pinfo) >= 0) {
			if (check_permission(&pinfo, group, perm)) {
				if (! client_printed) {
					printf("client %d: '%s' [group=%s] [type=%s]\n",
					       cinfo.client, cinfo.name, cinfo.group,
					       (cinfo.type == USER_CLIENT ? "user" : "kernel"));
					client_printed = 1;
				}
				printf("  %3d '%-16s' [group=%s]\n", pinfo.port, pinfo.name, pinfo.group);
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
	int c, fd;
	int queue = DEFAULT_QUEUE;
	int command = SUBSCRIBE;
	char *device = DEVICE_FILE;
	char *group = "";
	int client;
	snd_seq_client_info_t cinfo;
	snd_seq_port_subscribe_t subs;

	while ((c = getopt(argc, argv, "diog:D:q:")) != -1) {
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
		case 'D':
			device = optarg;
			break;
		case 'q':
			queue = atoi(optarg);
			break;
		default:
			usage();
			exit(1);
		}
	}

	if ((fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "can't open sequencer\n");
		return 1;
	}
	
	if (command == LIST_INPUT) {
		list_ports(fd, group, SND_SEQ_PORT_CAP_IN|SND_SEQ_PORT_CAP_SUBS_IN);
		return 0;
	} else if (command == LIST_OUTPUT) {
		list_ports(fd, group, SND_SEQ_PORT_CAP_OUT|SND_SEQ_PORT_CAP_SUBS_OUT);
		return 0;
	}

	if (optind + 2 > argc) {
		usage();
		exit(1);
	}

	if (ioctl(fd, SND_SEQ_IOCTL_CLIENT_ID, &client) < 0) {
		fprintf(stderr, "can't get client id\n");
		return 1;
	}

	/* set client info */
	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.client = client;
	cinfo.type = USER_CLIENT;
	strcpy(cinfo.name, "ALSA Connector");
	strncpy(cinfo.group, group, sizeof(cinfo.group) - 1);
	if (ioctl(fd, SND_SEQ_IOCTL_SET_CLIENT_INFO, &cinfo) < 0) {
		fprintf(stderr, "can't set client info\n");
		return 0;
	}

	/* set subscription */
	parse_address(&subs.sender, argv[optind]);
	parse_address(&subs.dest, argv[optind + 1]);
	subs.sender.queue = subs.dest.queue = queue;
	subs.exclusive = 0;
	subs.realtime = 0;

	if (command == UNSUBSCRIBE) {
		if (ioctl(fd, SND_SEQ_IOCTL_GET_SUBSCRIPTION, &subs) < 0) {
			fprintf(stderr, "No subscription is found\n");
			return 1;
		}
		if (ioctl(fd, SND_SEQ_IOCTL_UNSUBSCRIBE_PORT, &subs) < 0) {
			fprintf(stderr, "Disconnection failed (errno=%d)\n", errno);
			return 1;
		}
	} else {
		if (ioctl(fd, SND_SEQ_IOCTL_GET_SUBSCRIPTION, &subs) >= 0) {
			fprintf(stderr, "Connection is already subscribed\n");
			return 1;
		}
		if (ioctl(fd, SND_SEQ_IOCTL_SUBSCRIBE_PORT, &subs) < 0) {
			fprintf(stderr, "Connection failed (errno=%d)\n", errno);
			return 1;
		}
	}

	return 0;
}
