#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include "../include/asoundlib.h"
#include <sys/time.h>

#if 0
#define USE_FRAGMENT_MODE	/* latency is twice more than for frame mode!!! */
#endif

//#define USED_RATE	48000
#define USED_RATE       22050
#define LATENCY_MIN	32
#define LATENCY_MAX	2048		/* in frames */
#define LOOP_LIMIT	(30UL * USED_RATE)	/* 30 seconds */

#if 0
static char *xitoa(int aaa)
{
	static char str[12];

	sprintf(str, "%i", aaa);
	return str;
}
#endif

/*
 *  This small demo program can be used for measuring latency between
 *  capture and playback. This latency is measured from driver (diff when
 *  playback and capture was started). Scheduler is set to SCHED_RR.
 *
 *  Used format is 44100Hz, Signed Little Endian 16-bit, Stereo.
 */

int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle, int *bufsize)
{
	int err;
	snd_pcm_params_t params;
	snd_pcm_setup_t psetup, csetup;

	bzero(&params, sizeof(params));
#ifdef USE_FRAGMENT_MODE
	params.mode = SND_PCM_MODE_FRAGMENT;
#else
	params.mode = SND_PCM_MODE_FRAME;
#endif
	params.format.interleave = 1;
	params.format.format = SND_PCM_SFMT_S16_LE;
	params.format.channels = 2;
	params.format.rate = USED_RATE;
	params.start_mode = SND_PCM_START_GO;
	params.xrun_mode = SND_PCM_XRUN_DRAIN;
	params.time = 1;
	*bufsize += 4;

      __again:
	if (*bufsize > LATENCY_MAX)
		return -1;
	params.frag_size = *bufsize / 2;
	params.frames_min = 1;
	params.buffer_size = *bufsize;

	if ((err = snd_pcm_params(phandle, &params)) < 0) {
		printf("Playback params error: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_params(chandle, &params)) < 0) {
		printf("Capture params error: %s\n", snd_strerror(err));
		exit(0);
	}
	bzero(&psetup, sizeof(psetup));
	if ((err = snd_pcm_setup(phandle, &psetup)) < 0) {
		printf("Playback setup error: %s\n", snd_strerror(err));
		exit(0);
	}
	bzero(&csetup, sizeof(csetup));
	if ((err = snd_pcm_setup(chandle, &csetup)) < 0) {
		printf("Capture setup error: %s\n", snd_strerror(err));
		exit(0);
	}
	if (psetup.buffer_size > *bufsize) {
		*bufsize = psetup.buffer_size;
		goto __again;
	}

	if ((err = snd_pcm_prepare(phandle)) < 0) {
		printf("Playback prepare error: %s\n", snd_strerror(err));
		exit(0);
	}

	snd_pcm_dump(phandle, stdout);
	snd_pcm_dump(chandle, stdout);
	printf("Trying latency %i...\n", *bufsize);
	fflush(stdout);
	return 0;
}

void showstat(snd_pcm_t *handle, snd_pcm_status_t *rstatus, size_t frames)
{
	int err;
	snd_pcm_status_t status;

	bzero(&status, sizeof(status));
	if ((err = snd_pcm_status(handle, &status)) < 0) {
		printf("Stream status error: %s\n", snd_strerror(err));
		exit(0);
	}
	printf("  state = %i\n", status.state);
	printf("  frames = %i\n", frames);
	printf("  frame_io = %li\n", (long)status.frame_io);
	printf("  frame_data = %li\n", (long)status.frame_data);
	printf("  frames_avail = %li\n", (long)status.frames_avail);
	if (rstatus)
		*rstatus = status;
}

void setscheduler(void)
{
	struct sched_param sched_param;

	if (sched_getparam(0, &sched_param) < 0) {
		printf("Scheduler getparam failed...\n");
		return;
	}
	sched_param.sched_priority = sched_get_priority_max(SCHED_RR);
	if (!sched_setscheduler(0, SCHED_RR, &sched_param)) {
		printf("Scheduler set to Round Robin with priority %i...\n", sched_param.sched_priority);
		fflush(stdout);
		return;
	}
	printf("!!!Scheduler set to Round Robin with priority %i FAILED!!!\n", sched_param.sched_priority);
}

long timediff(struct timeval t1, struct timeval t2)
{
	signed long l;

	t1.tv_sec -= t2.tv_sec;
	l = (signed long) t1.tv_usec - (signed long) t2.tv_usec;
	if (l < 0) {
		t1.tv_sec--;
		l = -l;
		l %= 1000000;
	}
	return (t1.tv_sec * 1000000) + l;
}

long readbuf(snd_pcm_t *handle, char *buf, long len, size_t *frames)
{
	long r;
	
	do {
		r = snd_pcm_read(handle, buf, len);
	} while (r == -EAGAIN);
	if (r > 0)
		*frames += r;
	// printf("read = %li\n", r);
	// showstat(handle, NULL);
	return r;
}

long writebuf(snd_pcm_t *handle, char *buf, long len, size_t *frames)
{
	long r;

	while (len > 0) {
		r = snd_pcm_write(handle, buf, len);
		if (r == -EAGAIN)
			continue;
		// printf("write = %li\n", r);
		if (r < 0)
			return r;
		// showstat(handle, NULL);
		buf += r * 4;
		len -= r;
		*frames += r;
	}
	return 0;
}

int main(void)
{
	snd_pcm_t *phandle, *chandle;
	char buffer[LATENCY_MAX*4];
	int pcard = 0, pdevice = 0;
	int ccard = 0, cdevice = 0;
	int err, latency = LATENCY_MIN - 4;
	int size, ok;
	snd_pcm_status_t pstatus, cstatus;
	ssize_t r;
	size_t frames_in, frames_out;

	setscheduler();
	if ((err = snd_pcm_plug_open(&phandle, pcard, pdevice, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		return 0;
	}
	if ((err = snd_pcm_plug_open(&chandle, ccard, cdevice, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) {
		printf("Record open error: %s\n", snd_strerror(err));
		return 0;
	}
	if ((err = snd_pcm_link(phandle, chandle)) < 0) {
		printf("Streams link error: %s\n", snd_strerror(err));
		exit(0);
	}
	  
#ifdef USE_FRAGMENT_MODE
	printf("Using fragment mode...\n");
#else
	printf("Using frame mode...\n");
#endif
	printf("Loop limit is %li frames\n", LOOP_LIMIT);
	while (1) {
		frames_in = frames_out = 0;
		if (setparams(phandle, chandle, &latency) < 0)
			break;
		if (snd_pcm_format_set_silence(SND_PCM_SFMT_S16_LE, buffer, latency*2) < 0) {
			fprintf(stderr, "silence error\n");
			break;
		}
		if (writebuf(phandle, buffer, latency, &frames_out) < 0) {
			fprintf(stderr, "write error\n");
			break;
		}

		if ((err = snd_pcm_go(phandle)) < 0) {
			printf("Go error: %s\n", snd_strerror(err));
			exit(0);
		}
		ok = 1;
		size = 0;
		while (ok && frames_in < LOOP_LIMIT) {
			if ((r = readbuf(chandle, buffer, latency, &frames_in)) < 0)
				ok = 0;
			else if (writebuf(phandle, buffer, r, &frames_out) < 0)
				ok = 0;
		}
		printf("Playback:\n");
		showstat(phandle, &pstatus, frames_out);
		printf("Capture:\n");
		showstat(chandle, &cstatus, frames_in);
		if (pstatus.trigger_time.tv_sec == cstatus.trigger_time.tv_sec &&
		    pstatus.trigger_time.tv_usec == cstatus.trigger_time.tv_usec)
			printf("Hardware sync\n");
		snd_pcm_flush(phandle);
		if (ok) {
#if 0
			printf("Playback time = %li.%i, Record time = %li.%i, diff = %li\n",
			       pstatus.trigger_time.tv_sec,
			       (int)pstatus.trigger_time.tv_usec,
			       cstatus.trigger_time.tv_sec,
			       (int)cstatus.trigger_time.tv_usec,
			       timediff(pstatus.trigger_time, cstatus.trigger_time));
#endif
			break;
		}
	}
	snd_pcm_close(phandle);
	snd_pcm_close(chandle);
	return 0;
}
