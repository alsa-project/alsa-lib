#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include "../include/asoundlib.h"

#define LATENCY_LIMIT	8192		/* in bytes */
#define LOOP_LIMIT	(30 * 176400)	/* 30 seconds */

static char *xitoa(int aaa)
{
	static char str[12];

	sprintf(str, "%i", aaa);
	return str;
}

static int syncro(snd_pcm_t *phandle, snd_pcm_t *chandle)
{
	snd_pcm_channel_info_t pinfo, cinfo;
	int err;
	
	bzero(&pinfo, sizeof(pinfo));
	bzero(&cinfo, sizeof(cinfo));
	pinfo.channel = SND_PCM_CHANNEL_PLAYBACK;
	cinfo.channel = SND_PCM_CHANNEL_CAPTURE;
	pinfo.mode = cinfo.mode = SND_PCM_MODE_STREAM;
	if ((err = snd_pcm_channel_info(phandle, &pinfo)) < 0) {
		printf("Playback info error: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_channel_info(chandle, &cinfo)) < 0) {
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
	snd_pcm_channel_params_t params;
	snd_pcm_channel_setup_t psetup, csetup;

	bzero(&params, sizeof(params));
	params.channel = SND_PCM_CHANNEL_PLAYBACK;
	params.mode = SND_PCM_MODE_STREAM;
	params.format.interleave = 1;
	params.format.format = SND_PCM_SFMT_S16_LE;
	params.format.voices = 2;
	params.format.rate = 44100;
	params.start_mode = SND_PCM_START_GO;
	params.stop_mode = SND_PCM_STOP_STOP;
	params.time = 1;
	*queue += 16;
	params.buf.stream.fill = SND_PCM_FILL_SILENCE;
	params.buf.stream.max_fill = 1024;
	if (sync)
		syncro_id(&params.sync);
      __again:
	if (*queue > LATENCY_LIMIT)
		return -1;
      	again = 0;
      	params.channel = SND_PCM_CHANNEL_PLAYBACK;
	params.buf.stream.queue_size = *queue;
	if ((err = snd_pcm_plugin_params(phandle, &params)) < 0) {
		printf("Playback params error: %s\n", snd_strerror(err));
		exit(0);
	}
	params.channel = SND_PCM_CHANNEL_CAPTURE;
	if ((err = snd_pcm_plugin_params(chandle, &params)) < 0) {
		printf("Capture params error: %s\n", snd_strerror(err));
		exit(0);
	}
	bzero(&psetup, sizeof(psetup));
	psetup.mode = SND_PCM_MODE_STREAM;
	psetup.channel = SND_PCM_CHANNEL_PLAYBACK;
	if ((err = snd_pcm_plugin_setup(phandle, &psetup)) < 0) {
		printf("Playback setup error: %s\n", snd_strerror(err));
		exit(0);
	}
	bzero(&csetup, sizeof(csetup));
	csetup.mode = SND_PCM_MODE_STREAM;
	csetup.channel = SND_PCM_CHANNEL_CAPTURE;
	if ((err = snd_pcm_plugin_setup(chandle, &csetup)) < 0) {
		printf("Capture setup error: %s\n", snd_strerror(err));
		exit(0);
	}
	if (psetup.buf.stream.queue_size > *queue) {
		*queue = psetup.buf.stream.queue_size;
		again++;
	}
	if (csetup.buf.stream.queue_size > *queue) {
		*queue = csetup.buf.stream.queue_size;
		again++;
	}
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
	printf("Trying latency %i...\n", *queue);
	fflush(stdout);
	return 0;
}

void showstat(snd_pcm_t *handle, int channel, snd_pcm_channel_status_t *rstatus)
{
	int err;
	snd_pcm_channel_status_t status;
	char *str;

	str = channel == SND_PCM_CHANNEL_CAPTURE ? "Capture" : "Playback";
	bzero(&status, sizeof(status));
	status.channel = channel;
	status.mode = SND_PCM_MODE_STREAM;
	if ((err = snd_pcm_plugin_status(handle, &status)) < 0) {
		printf("Channel %s status error: %s\n", str, snd_strerror(err));
		exit(0);
	}
	printf("%s:\n", str);
	printf("  status = %i\n", status.status);
	printf("  position = %u\n", status.scount);
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

long readbuf(snd_pcm_t *handle, char *buf, long len)
{
	long r;
	
	r = snd_pcm_plugin_read(handle, buf, len);
	// printf("read = %li\n", r);
	// showstat(handle, SND_PCM_CHANNEL_CAPTURE, NULL);
	return r;
}

long writebuf(snd_pcm_t *handle, char *buf, long len)
{
	long r;

	while (len > 0) {
		r = snd_pcm_plugin_write(handle, buf, len);
		if (r == -EAGAIN)
			continue;
		// printf("write = %li\n", r);
		if (r < 0)
			return r;
		// showstat(handle, SND_PCM_CHANNEL_PLAYBACK, NULL);
		buf += r;
		len -= r;
	}
	return 0;
}

int main(void)
{
	snd_pcm_t *phandle, *chandle;
	char buffer[4096 * 2];	/* max two fragments by 4096 bytes */
	int pcard = 0, pdevice = 0;
	int ccard = 0, cdevice = 0;
	int err, latency = 16;
	int size, ok;
	int sync;
	snd_pcm_sync_t ssync;
	snd_pcm_channel_status_t pstatus, cstatus;
	long r;

	setscheduler();
	if ((err = snd_pcm_open(&phandle, pcard, pdevice, SND_PCM_OPEN_STREAM_PLAYBACK)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		return 0;
	}
	if ((err = snd_pcm_open(&chandle, ccard, cdevice, SND_PCM_OPEN_STREAM_CAPTURE)) < 0) {
		printf("Record open error: %s\n", snd_strerror(err));
		return 0;
	}
	sync = syncro(phandle, chandle);
	if (sync)
		printf("Using hardware synchronization mode\n");
	while (1) {
		if (setparams(phandle, chandle, sync, &latency) < 0)
			break;
		memset(buffer, 0, latency);
		if (writebuf(phandle, buffer, latency) < 0)
			break;
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
		while (ok && size < LOOP_LIMIT) {
			if ((r = readbuf(chandle, buffer, latency)) < 0)
				ok = 0;
			if (writebuf(phandle, buffer, r) < 0)
				ok = 0;
			size += r;
		}
		showstat(phandle, SND_PCM_CHANNEL_PLAYBACK, &pstatus);
		showstat(chandle, SND_PCM_CHANNEL_CAPTURE, &cstatus);
		snd_pcm_flush_capture(chandle);
		snd_pcm_flush_playback(phandle);
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
