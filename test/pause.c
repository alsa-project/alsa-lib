#include <stdio.h>
#include <malloc.h>
#include "../include/asoundlib.h"

#define AU_FILE "/home/ftp/pub/audio/AlwaysOnTheRun.au"

static void show_playback_status(void *handle)
{
	snd_pcm_playback_status_t pstatus;

	if (snd_pcm_playback_status(handle, &pstatus) < 0) {
		perror("playback status");
		return;
	}
	printf("PCM playback status:\n");
	printf("  rate = %i\n", pstatus.rate);
	printf("  fragments = %i\n", pstatus.fragments);
	printf("  fragment_size = %i\n", pstatus.fragment_size);
	printf("  count = %i\n", pstatus.count);
	printf("  queue = %i\n", pstatus.queue);
	printf("  underrun = %i\n", pstatus.underrun);
	printf("  time = %i.%i\n", (int) pstatus.time.tv_sec, (int) pstatus.time.tv_usec);
	printf("  stime = %i.%i\n", (int) pstatus.stime.tv_sec, (int) pstatus.stime.tv_usec);
	printf("  scount = %i\n", pstatus.scount);
}

int main(void)
{
	int card = 0, device = 0, err, fd, count, count1, size, idx;
	snd_pcm_t *handle;
	snd_pcm_format_t format;
	snd_pcm_playback_status_t status;
	char *buffer, *buffer1;

	buffer = (char *) malloc(512 * 1024);
	if (!buffer)
		return 0;
	if ((err = snd_pcm_open(&handle, card, device, SND_PCM_OPEN_PLAYBACK)) < 0) {
		fprintf(stderr, "open failed: %s\n", snd_strerror(err));
		return 0;
	}
	format.sfmt = SND_PCM_FORMAT_MU_LAW;
	format.rate = 8000;
	format.channels = 1;
	if ((err = snd_pcm_playback_format(handle, &format)) < 0) {
		fprintf(stderr, "format setup failed: %s\n", snd_strerror(err));
		snd_pcm_close(handle);
		return 0;
	}
	if ((err = snd_pcm_playback_status(handle, &status)) < 0) {
		fprintf(stderr, "status failed: %s\n", snd_strerror(err));
		snd_pcm_close(handle);
		return 0;
	}
	fd = open(AU_FILE, O_RDONLY);
	if (fd < 0) {
		perror("open file");
		snd_pcm_close(handle);
		return 0;
	}
	idx = 0;
	count = read(fd, buffer, 512 * 1024);
	if (count <= 0) {
		perror("read from file");
		snd_pcm_close(handle);
		return 0;
	}
	close(fd);
	if (!memcmp(buffer, ".snd", 4)) {
		idx = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | (buffer[7]);
		if (idx > 128)
			idx = 128;
		if (idx > count)
			idx = count;
	}
	buffer1 = buffer + idx;
	count -= idx;
	if (count < 256 * 1024) {
		perror("small count < 256k");
		snd_pcm_close(handle);
		return 0;
	}
	count1 = status.fragment_size * 12;
	show_playback_status(handle);
	size = snd_pcm_writei(handle, buffer1, count1);
	sleep(2);
	show_playback_status(handle);
	printf("Pause.. Bytes written %i from %i...\n", size, count1);
	count -= count1;
	buffer1 += count1;
	snd_pcm_playback_pause(handle, 1);
	show_playback_status(handle);
	sleep(5);
	printf("Pause end..\n");
	snd_pcm_playback_pause(handle, 0);
	show_playback_status(handle);
	size = snd_pcm_writei(handle, buffer1, count);
	printf("Pause end.. Bytes written %i from %i...\n", size, count);
	snd_pcm_close(handle);
	free(buffer);
	return 0;
}
