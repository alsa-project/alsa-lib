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
		max = snd_timer_file_descriptor(handle);
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
	int timer = SND_TIMER_GLOBAL(SND_TIMER_GLOBAL_SYSTEM);
	int slave = 0;
	int slave_type = SND_TIMER_STYPE_SEQUENCER, slave_id = 0;
	snd_timer_t *handle;
	snd_timer_general_info_t ginfo;
	snd_timer_select_t sel;
	snd_timer_info_t info;
	snd_timer_params_t params;

	idx = 1;
	while (idx < argc) {
		if (!strncmp(argv[idx], "timer=", 6)) {
			timer = atoi(argv[idx]+6);
		} else if (!strcmp(argv[idx], "slave")) {
			slave = 1;
		} else if (!strncmp(argv[idx], "slave_type=", 11)) {
			slave_type = atoi(argv[idx]+11);
		} else if (!strncmp(argv[idx], "slave_id=", 9)) {
			slave_id = atoi(argv[idx]+9);
		}
		idx++;
	}
	if (slave && slave_type == SND_TIMER_STYPE_NONE) {
		fprintf(stderr, "sync_type is not set\n");
		exit(0);
	}
	if ((err = snd_timer_open(&handle))<0) {
		fprintf(stderr, "timer open %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	if (snd_timer_general_info(handle, &ginfo)<0) {
		fprintf(stderr, "timer general info %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	printf("Global timers = %i\n", ginfo.count);
	printf("Using timer %i, slave %i, slave_type %i, slave_id %i\n", timer, slave, slave_type, slave_id);
	bzero(&sel, sizeof(sel));
	sel.slave = slave;
	if (!sel.slave) {
		sel.data.number = timer;
	} else {
		sel.data.slave.type = slave_type;
		sel.data.slave.id = slave_id;
	}
	if ((err = snd_timer_select(handle, &sel)) < 0) {
		fprintf(stderr, "timer select %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	if (!sel.slave) {
		if ((err = snd_timer_info(handle, &info)) < 0) {
			fprintf(stderr, "timer info %i (%s)\n", err, snd_strerror(err));
			exit(0);
		}
		printf("Timer %i info:\n", sel.data.number);
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
	read_loop(handle, 25, sel.slave ? 10000 : 1);
	show_status(handle);
	snd_timer_close(handle);
	return 0;
}
