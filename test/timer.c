#include <stdio.h>
#include <string.h>
#include "../include/asoundlib.h"

void main(void)
{
	int err;
	void *handle;
	snd_timer_general_info_t info;

	if ((err = snd_timer_open(&handle))<0) {
		fprintf(stderr, "timer open %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	if (snd_timer_general_info(handle, &info)<0) {
		fprintf(stderr, "timer general info %i (%s)\n", err, snd_strerror(err));
		exit(0);
	}
	printf("global timers = %i\n", info.count);
	snd_timer_close(handle);
}
