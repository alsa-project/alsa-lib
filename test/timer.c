#include <stdio.h>
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

void main(void)
{
	int err;
	void *handle;
	snd_timer_general_info_t ginfo;
	snd_timer_select_t sel;
	snd_timer_info_t info;
	snd_timer_params_t params;

	if ((err = snd_timer_open(&handle))<0) {
		fprintf(stderr, "timer open %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	if (snd_timer_general_info(handle, &ginfo)<0) {
		fprintf(stderr, "timer general info %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	printf("Global timers = %i\n", ginfo.count);
	bzero(&sel, sizeof(sel));
	sel.number = SND_TIMER_SYSTEM;
	sel.slave = 0;
	if ((err = snd_timer_select(handle, &sel)) < 0) {
		fprintf(stderr, "timer select %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	if ((err = snd_timer_info(handle, &info)) < 0) {
		fprintf(stderr, "timer info %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	printf("Timer %i info:\n", sel.number);
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
	show_status(handle);
	if ((err = snd_timer_start(handle)) < 0) {
		fprintf(stderr, "timer start %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	usleep(500000);
	show_status(handle);
	snd_timer_close(handle);
}
