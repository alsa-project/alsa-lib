#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/asoundlib.h"

int main(int argc, char *argv[])
{
	int err;
	void *handle;

	err = snd_pcm_loopback_open(&handle, 0, 0, SND_PCM_LB_OPEN_PLAYBACK);
	if (err < 0) {
		fprintf(stderr, "open error: %s\n", snd_strerror(err));
		exit(0);
	}
	snd_pcm_loopback_close(handle);
	return 0;
}
