#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/asoundlib.h"

void show_status(void *handle)
{
	int err;
	snd_timer_status_t status;
	
	if ((err = snd_timer_status(handle, &status)) < 0) {
		fprintf(stderr, "timer status %i (%s)\n", err, snd_strerror(err));
		return;
	}
	printf("STATUS:\n");
	printf("  resolution = %lu\n", status.resolution);
	printf("  lost = %lu\n", status.lost);
	printf("  overrun = %lu\n", status.overrun);
	printf("  queue = %i\n", status.queue);
}

void read_loop(void *handle, int master_ticks, int timeout)
{
	int err, max;
	fd_set in;
	struct timeval tv;
	snd_timer_read_t tr;
	
	while (master_ticks-- > 0) {
		FD_ZERO(&in);
		max = snd_timer_poll_descriptor(handle);
		FD_SET(max, &in);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		if ((err = select(max + 1, &in, NULL, NULL, &tv)) < 0) {
			fprintf(stderr, "select error %i (%s)\n", err, strerror(err));
			exit(0);
		}
		if (err == 0) {
			fprintf(stderr, "timer time out!!\n");
			exit(0);
		}
		while (snd_timer_read(handle, &tr, sizeof(tr)) == sizeof(tr)) {
			printf("TIMER: resolution = %lu, ticks = %lu\n",
				tr.resolution, tr.ticks);
		}
	}
}

int main(int argc, char *argv[])
{
	int idx, err;
	int type = SND_TIMER_TYPE_GLOBAL;
	int stype = SND_TIMER_TYPE_NONE;
	int card = 0;
	int device = SND_TIMER_GLOBAL_SYSTEM;
	int subdevice = 0;
	int list = 0;
	snd_timer_t *handle;
	snd_timer_select_t sel;
	snd_timer_info_t info;
	snd_timer_params_t params;

	idx = 1;
	while (idx < argc) {
		if (!strncmp(argv[idx], "type=", 5)) {
			type = atoi(argv[idx]+5);
		} else if (!strncmp(argv[idx], "stype=", 6)) {
			stype = atoi(argv[idx]+6);
		} else if (!strncmp(argv[idx], "card=", 5)) {
			card = atoi(argv[idx]+5);
		} else if (!strncmp(argv[idx], "device=", 7)) {
			device = atoi(argv[idx]+7);
		} else if (!strncmp(argv[idx], "subdevice=", 10)) {
			subdevice = atoi(argv[idx]+10);
		} else if (!strcmp(argv[idx], "list")) {
			list = 1;
		}
		idx++;
	}
	if (type == SND_TIMER_TYPE_SLAVE && stype == SND_TIMER_STYPE_NONE) {
		fprintf(stderr, "sync_type is not set\n");
		exit(0);
	}
	if ((err = snd_timer_open(&handle))<0) {
		fprintf(stderr, "timer open %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	if (list) {
		bzero(&sel.id, sizeof(sel.id));
		sel.id.type = -1;
		while (1) {
			if ((err = snd_timer_next_device(handle, &sel.id)) < 0) {
				fprintf(stderr, "timer next device error: %s\n", snd_strerror(err));
				break;
			}
			if (sel.id.type < 0)
				break;
			printf("Timer device: type %i, stype %i, card %i, device %i, subdevice %i\n",
					sel.id.type, sel.id.stype, sel.id.card, sel.id.device, sel.id.subdevice);
		}
	}
	printf("Using timer type %i, slave type %i, card %i, device %i, subdevice %i\n", type, stype, card, device, subdevice);
	bzero(&sel, sizeof(sel));
	sel.id.type = type;
	sel.id.stype = stype;
	sel.id.card = card;
	sel.id.device = device;
	sel.id.subdevice = subdevice;
	if ((err = snd_timer_select(handle, &sel)) < 0) {
		fprintf(stderr, "timer select %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	if (type != SND_TIMER_TYPE_SLAVE) {
		if ((err = snd_timer_info(handle, &info)) < 0) {
			fprintf(stderr, "timer info %i (%s)\n", err, snd_strerror(err));
			exit(0);
		}
		printf("Timer info:\n");
		printf("  flags = 0x%x\n", info.flags);
		printf("  id = '%s'\n", info.id);
		printf("  name = '%s'\n", info.name);
		printf("  max ticks = %lu\n", info.ticks);
		printf("  average resolution = %lu\n", info.resolution);
		bzero(&params, sizeof(params));
		params.flags = SND_TIMER_PSFLG_AUTO;
		params.ticks = (1000000000 / info.resolution) / 50;	/* 50Hz */
		if (params.ticks < 1)
			params.ticks = 1;
		printf("Using %lu tick(s)\n", params.ticks);
		if ((err = snd_timer_params(handle, &params)) < 0) {
			fprintf(stderr, "timer params %i (%s)\n", err, snd_strerror(err));
			exit(0);
		}
	}
	show_status(handle);
	if ((err = snd_timer_start(handle)) < 0) {
		fprintf(stderr, "timer start %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	read_loop(handle, 25, type == SND_TIMER_TYPE_SLAVE ? 10000 : 1);
	show_status(handle);
	snd_timer_close(handle);
	return 0;
}
