#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/asoundlib.h"

#define CARD	0
#define DEVICE	0
#define SUBDEV	0
#define MODE	SND_PCM_LB_STREAM_MODE_PACKET

static void show_format1(const char *prefix, snd_pcm_format_t *format)
{
	printf("%sinterleave = %i, rate = %i, voices = %i, format = %i\n",
			prefix,
			format->interleave ? 1 : 0,
			format->rate,
			format->voices,
			format->sfmt);
}

static void show_format(snd_pcm_loopback_t *handle)
{
	snd_pcm_format_t format;
	int err;

	err = snd_pcm_loopback_format(handle, &format);
	if (err < 0) {
		fprintf(stderr, "format failed: %s\n", snd_strerror(err));
		exit(0);
	}
	show_format1("Format: ", &format);
}

static void data(void *private_data, char *buf, size_t count)
{
	printf("DATA> count = %li\n", (long)count);
}

static void format_change(void *private_data, snd_pcm_format_t *format)
{
	show_format1("Format change> ", format);
}

static void position_change(void *private_data, unsigned int pos)
{
	printf("Position change> %u\n", pos);
}

int main(int argc, char *argv[])
{
	int err;
	ssize_t res;
	snd_pcm_loopback_t *handle;
	snd_pcm_loopback_callbacks_t callbacks;

	err = snd_pcm_loopback_open(&handle, CARD, DEVICE, SUBDEV, SND_PCM_LB_OPEN_PLAYBACK);
	if (err < 0) {
		fprintf(stderr, "open error: %s\n", snd_strerror(err));
		exit(0);
	}
	err = snd_pcm_loopback_stream_mode(handle, MODE);
	if (err < 0) {
		fprintf(stderr, "stream mode setup failed: %s\n", snd_strerror(err));
		exit(0);
	}
	show_format(handle);
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.data = data;
	callbacks.format_change = format_change;	
	callbacks.position_change = position_change;
	while ((res = snd_pcm_loopback_read(handle, &callbacks)) >= 0) {
		if (res > 0)
			printf("Read ok.. - %i\n", res);
	}
	snd_pcm_loopback_close(handle);
	return 0;
}
