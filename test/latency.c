#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include "../include/asoundlib.h"

#if 0
#define USE_FRAGMENT_MODE	/* latency is twice more than for frame mode!!! */
#endif

#define USED_RATE	48000
//#define LATENCY_MIN	8192
#define LATENCY_MIN	32
#define LATENCY_MAX	8192		/* in bytes */
//#define LOOP_LIMIT	(8192UL * 2)
#define LOOP_LIMIT	(30 * 176400UL)	/* 30 seconds */

#if 0
static char *xitoa(int aaa)
{
	static char str[12];

	sprintf(str, "%i", aaa);
	return str;
}
#endif

static int syncro(snd_pcm_t *phandle, snd_pcm_t *chandle)
{
	snd_pcm_stream_info_t pinfo, cinfo;
	int err;
	
	bzero(&pinfo, sizeof(pinfo));
	bzero(&cinfo, sizeof(cinfo));
	pinfo.stream = SND_PCM_STREAM_PLAYBACK;
	cinfo.stream = SND_PCM_STREAM_CAPTURE;
	if ((err = snd_pcm_stream_info(phandle, &pinfo)) < 0) {
		printf("Playback info error: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_stream_info(chandle, &cinfo)) < 0) {
		printf("Capture info error: %s\n", snd_strerror(err));
		exit(0);
	}
	if (pinfo.sync.id32[0] == 0 && pinfo.sync.id32[1] == 0 &&
	    pinfo.sync.id32[2] == 0 && pinfo.sync.id32[3] == 0)
		return 0;
	if (memcmp(&pinfo.sync, &cinfo.sync, sizeof(pinfo.sync)))
		return 0;
	return 1;
}

static void syncro_id(snd_pcm_sync_t *sync)
{
	sync->id32[0] = 0;	/* application */
	sync->id32[1] = getpid();
	sync->id32[2] = 0x89abcdef;
	sync->id32[3] = 0xaaaaaaaa;
}

/*
 *  This small demo program can be used for measuring latency between
 *  capture and playback. This latency is measured from driver (diff when
 *  playback and capture was started). Scheduler is set to SCHED_RR.
 *
 *  Used format is 44100Hz, Signed Little Endian 16-bit, Stereo.
 */

int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle, int sync, int *queue)
{
	int err, again;
	snd_pcm_stream_params_t params;
	snd_pcm_stream_setup_t psetup, csetup;

	bzero(&params, sizeof(params));
	params.stream = SND_PCM_STREAM_PLAYBACK;
#ifdef USE_BLOCK_MODE
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
	*queue += 16;
#if 0
	params.buf.stream.fill = SND_PCM_FILL_SILENCE;
	params.buf.stream.max_fill = 1024;
#else
	params.buf.stream.fill = SND_PCM_FILL_NONE;
#endif
	if (sync)
		syncro_id(&params.sync);
      __again:
	if (*queue > LATENCY_MAX)
		return -1;
      	again = 0;
      	params.stream = SND_PCM_STREAM_PLAYBACK;
	params.frag_size = *queue;
#ifdef USE_BLOCK_MODE
	params.buffer_size = *queue * 2;
	params.buf.block.frags_min = 1;
#else
	params.buffer_size = *queue;
	params.buf.stream.bytes_min = 2;
#endif
	if ((err = snd_pcm_stream_params(phandle, &params)) < 0) {
		printf("Playback params error: %s\n", snd_strerror(err));
		exit(0);
	}
	params.stream = SND_PCM_STREAM_CAPTURE;
	if ((err = snd_pcm_stream_params(chandle, &params)) < 0) {
		printf("Capture params error: %s\n", snd_strerror(err));
		exit(0);
	}
	bzero(&psetup, sizeof(psetup));
	psetup.stream = SND_PCM_STREAM_PLAYBACK;
	if ((err = snd_pcm_stream_setup(phandle, &psetup)) < 0) {
		printf("Playback setup error: %s\n", snd_strerror(err));
		exit(0);
	}
	bzero(&csetup, sizeof(csetup));
	csetup.stream = SND_PCM_STREAM_CAPTURE;
	if ((err = snd_pcm_stream_setup(chandle, &csetup)) < 0) {
		printf("Capture setup error: %s\n", snd_strerror(err));
		exit(0);
	}
#ifdef USE_BLOCK_MODE
	if (psetup.buffer_size / 2 > *queue) {
		*queue = psetup.buffer_size / 2;
		again++;
	}
#else
	if (psetup.buffer_size > *queue) {
		*queue = psetup.buffer_size;
		again++;
	}
#endif
	if (again)
		goto __again;
	if ((err = snd_pcm_playback_prepare(phandle)) < 0) {
		printf("Playback prepare error: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_capture_prepare(chandle)) < 0) {
		printf("Capture prepare error: %s\n", snd_strerror(err));
		exit(0);
	}	
	printf("Trying latency %i (playback rate = %iHz, capture rate = %iHz)...\n",
#ifdef USE_BLOCK_MODE
		*queue * 2, 
#else
		*queue, 
#endif
		psetup.format.rate, csetup.format.rate);
	printf("Fragment boundary = %li/%li, Position boundary = %li/%li\n", (long)psetup.frag_boundary, (long)csetup.frag_boundary, (long)psetup.pos_boundary, (long)psetup.pos_boundary);
	printf("Frags = %li/%li, Buffer size = %li/%li\n", (long)psetup.frags, (long)csetup.frags, (long)psetup.buffer_size, (long)csetup.buffer_size);
	fflush(stdout);
	return 0;
}

void showstat(snd_pcm_t *handle, int stream, snd_pcm_stream_status_t *rstatus, size_t bytes)
{
	int err;
	snd_pcm_stream_status_t status;
	char *str;

	str = stream == SND_PCM_STREAM_CAPTURE ? "Capture" : "Playback";
	bzero(&status, sizeof(status));
	status.stream = stream;
	if ((err = snd_pcm_stream_status(handle, &status)) < 0) {
		printf("Stream %s status error: %s\n", str, snd_strerror(err));
		exit(0);
	}
	printf("%s:\n", str);
	printf("  status = %i\n", status.status);
	printf("  bytes = %i\n", bytes);
	printf("  frag_io = %li\n", (long)status.frag_io);
	printf("  frag_data = %li\n", (long)status.frag_data);
	printf("  frag_used = %li\n", (long)status.frags_used);
	printf("  frag_free = %li\n", (long)status.frags_free);
	printf("  pos_io = %li\n", (long)status.pos_io);
	printf("  pos_data = %li\n", (long)status.pos_data);
	printf("  bytes_used = %li\n", (long)status.bytes_used);
	printf("  bytes_free = %li\n", (long)status.bytes_free);
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

long readbuf(snd_pcm_t *handle, char *buf, long len, size_t *bytes)
{
	long r;
	
	do {
		r = snd_pcm_read(handle, buf, len);
	} while (r == -EAGAIN);
	if (r > 0)
		*bytes += r;
	// printf("read = %li\n", r);
	// showstat(handle, SND_PCM_STREAM_CAPTURE, NULL);
	return r;
}

long writebuf(snd_pcm_t *handle, char *buf, long len, size_t *bytes)
{
	long r;

	while (len > 0) {
		r = snd_pcm_write(handle, buf, len);
#ifndef USE_BLOCK_MODE
		if (r == -EAGAIN)
			continue;
#endif
		// printf("write = %li\n", r);
		if (r < 0)
			return r;
		// showstat(handle, SND_PCM_STREAM_PLAYBACK, NULL);
		buf += r;
		len -= r;
		*bytes += r;
	}
	return 0;
}

int main(void)
{
	snd_pcm_t *phandle, *chandle;
	char buffer[LATENCY_MAX];	/* max two fragments by 4096 bytes */
	int pcard = 0, pdevice = 0;
	int ccard = 0, cdevice = 0;
	int err, latency = LATENCY_MIN - 16;
	int size, ok;
	int sync;
	snd_pcm_sync_t ssync;
	snd_pcm_stream_status_t pstatus, cstatus;
	ssize_t r;
	size_t bytes_in, bytes_out;

	setscheduler();
	if ((err = snd_pcm_plug_open(&phandle, pcard, pdevice, SND_PCM_OPEN_PLAYBACK|SND_PCM_NONBLOCK_PLAYBACK)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		return 0;
	}
	if ((err = snd_pcm_plug_open(&chandle, ccard, cdevice, SND_PCM_OPEN_CAPTURE|SND_PCM_NONBLOCK_CAPTURE)) < 0) {
		printf("Record open error: %s\n", snd_strerror(err));
		return 0;
	}
#ifdef USE_BLOCK_MODE
	printf("Using fragment mode...\n");
#else
	printf("Using frame mode...\n");
#endif
	sync = syncro(phandle, chandle);
	if (sync)
		printf("Using hardware synchronization mode\n");
	printf("Loop limit is %li bytes\n", LOOP_LIMIT);
	while (1) {
		bytes_in = bytes_out = 0;
		if (setparams(phandle, chandle, sync, &latency) < 0)
			break;
		if (snd_pcm_format_set_silence(SND_PCM_SFMT_S16_LE, buffer, latency) < 0) {
			fprintf(stderr, "silence error\n");
			break;
		}
		if (writebuf(phandle, buffer, latency, &bytes_out) < 0) {
			fprintf(stderr, "write error\n");
			break;
		}
#ifdef USE_BLOCK_MODE	/* at least two fragments MUST BE filled !!! */
		if (writebuf(phandle, buffer, latency, &bytes_out) < 0)
			break;
#endif
		if (sync) {
			syncro_id(&ssync);
			if ((err = snd_pcm_sync_go(phandle, &ssync)) < 0) {
				printf("Sync go error: %s\n", snd_strerror(err));
				exit(0);
			}
		} else {
			if ((err = snd_pcm_capture_go(chandle)) < 0) {
				printf("Capture go error: %s\n", snd_strerror(err));
				exit(0);
			}
			if ((err = snd_pcm_playback_go(phandle)) < 0) {
				printf("Playback go error: %s\n", snd_strerror(err));
				exit(0);
			}
		}
		ok = 1;
		size = 0;
		while (ok && bytes_in < LOOP_LIMIT) {
			if ((r = readbuf(chandle, buffer, latency, &bytes_in)) < 0)
				ok = 0;
			if (r > 0 && writebuf(phandle, buffer, r, &bytes_out) < 0)
				ok = 0;
		}
		showstat(phandle, SND_PCM_STREAM_PLAYBACK, &pstatus, bytes_out);
		showstat(chandle, SND_PCM_STREAM_CAPTURE, &cstatus, bytes_in);
		snd_pcm_capture_flush(chandle);
		snd_pcm_playback_flush(phandle);
		if (ok) {
			printf("Playback time = %li.%i, Record time = %li.%i, diff = %li\n",
			       pstatus.stime.tv_sec,
			       (int)pstatus.stime.tv_usec,
			       cstatus.stime.tv_sec,
			       (int)cstatus.stime.tv_usec,
			       timediff(pstatus.stime, cstatus.stime));
			break;
		}
	}
	snd_pcm_close(phandle);
	snd_pcm_close(chandle);
	return 0;
}
