#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include "../include/asoundlib.h"

static char *xitoa(int aaa)
{
	static char str[12];

	sprintf(str, "%i", aaa);
	return str;
}

/*
 *  This small demo program can be used for measuring latency between
 *  capture and playback. This latency is measured from driver (diff when
 *  playback and capture was started). Scheduler is set to SCHED_RR.
 *
 *  Used format is 44100Hz, Signed Little Endian 16-bit, Stereo.
 *
 *  Program begins with 128-byte fragment (about 726us) and step is 32-byte
 *  until playback without underruns is reached. This program starts playback
 *  after two fragments are captureed (teoretical latency is 1452us by 128-byte
 *  fragment).
 */

void setformat(void *phandle, void *rhandle)
{
	int err;
	snd_pcm_format_t format;

	bzero(&format, sizeof(format));
	format.format = SND_PCM_SFMT_S16_LE;
	format.channels = 2;
	format.rate = 44100;
	if ((err = snd_pcm_playback_format(phandle, &format)) < 0) {
		printf("Playback format error: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_capture_format(rhandle, &format)) < 0) {
		printf("Record format error: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_playback_time(phandle, 1)) < 0) {
		printf("Playback time error: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_capture_time(rhandle, 1)) < 0) {
		printf("Record time error: %s\n", snd_strerror(err));
		exit(0);
	}
}

int setparams(void *phandle, void *rhandle, int *fragmentsize)
{
	int err, step = 4;
	snd_pcm_playback_info_t pinfo;
	snd_pcm_capture_info_t rinfo;
	snd_pcm_playback_params_t pparams;
	snd_pcm_capture_params_t rparams;

	if ((err = snd_pcm_playback_info(phandle, &pinfo)) < 0) {
		printf("Playback info error: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_capture_info(rhandle, &rinfo)) < 0) {
		printf("Record info error: %s\n", snd_strerror(err));
		exit(0);
	}
	if (step < pinfo.min_fragment_size)
		step = pinfo.min_fragment_size;
	if (step < rinfo.min_fragment_size)
		step = rinfo.min_fragment_size;
	while (step < 4096 &&
	       (step & pinfo.fragment_align) != 0 &&
	       (step & rinfo.fragment_align) != 0)
		step++;
	if (*fragmentsize) {
		*fragmentsize += step;
	} else {
		if (step < 128)
			*fragmentsize = 128;
		else
			*fragmentsize = step;
	}
	while (*fragmentsize < 4096) {
		bzero(&pparams, sizeof(pparams));
		pparams.fragment_size = *fragmentsize;
		pparams.fragments_max = -1;	/* maximum */
		pparams.fragments_room = 1;
		if ((err = snd_pcm_playback_params(phandle, &pparams)) < 0) {
			*fragmentsize += step;
			continue;
		}
		bzero(&rparams, sizeof(rparams));
		rparams.fragment_size = *fragmentsize;
		rparams.fragments_min = 1;	/* wakeup if at least one fragment is ready */
		if ((err = snd_pcm_capture_params(rhandle, &rparams)) < 0) {
			*fragmentsize += step;
			continue;
		}
		break;
	}
	if (*fragmentsize < 4096) {
		printf("Trying fragment size %i...\n", *fragmentsize);
		fflush(stdout);
		return 0;
	}
	return -1;
}

int playbackunderrun(void *phandle)
{
	int err;
	snd_pcm_playback_status_t pstatus;

	if ((err = snd_pcm_playback_status(phandle, &pstatus)) < 0) {
		printf("Playback status error: %s\n", snd_strerror(err));
		exit(0);
	}
	return pstatus.underrun;
}

void capturefragment(void *rhandle, char *buffer, int index, int fragmentsize)
{
	int err;

	buffer += index * fragmentsize;
	if ((err = snd_pcm_read(rhandle, buffer, fragmentsize)) != fragmentsize) {
		printf("Read error: %s\n", err < 0 ? snd_strerror(err) : xitoa(err));
		exit(0);
	}
}

void playfragment(void *phandle, char *buffer, int index, int fragmentsize)
{
	int err;

	buffer += index * fragmentsize;
	if ((err = snd_pcm_write(phandle, buffer, fragmentsize)) != fragmentsize) {
		printf("Write error: %s\n", err < 0 ? snd_strerror(err) : xitoa(err));
		exit(0);
	}
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

int main(void)
{
	snd_pcm_t *phandle, *rhandle;
	char buffer[4096 * 2];	/* max two fragments by 4096 bytes */
	int pcard = 0, pdevice = 0;
	int rcard = 0, rdevice = 0;
	int err, fragmentsize = 0;
	int ridx, pidx, size, ok;
	snd_pcm_playback_status_t pstatus;
	snd_pcm_capture_status_t rstatus;

	setscheduler();
	if ((err = snd_pcm_open(&phandle, pcard, pdevice, SND_PCM_OPEN_PLAYBACK)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		return 0;
	}
	if ((err = snd_pcm_open(&rhandle, rcard, rdevice, SND_PCM_OPEN_CAPTURE)) < 0) {
		printf("Record open error: %s\n", snd_strerror(err));
		return 0;
	}
	setformat(phandle, rhandle);
	while (1) {
		if (setparams(phandle, rhandle, &fragmentsize) < 0)
			break;
		ok = 1;
		ridx = pidx = size = 0;
		capturefragment(rhandle, buffer, ridx++, fragmentsize);
		capturefragment(rhandle, buffer, ridx++, fragmentsize);
		if ((err = snd_pcm_capture_status(rhandle, &rstatus)) < 0) {
			printf("Record status error: %s\n", snd_strerror(err));
			exit(0);
		}
		ridx %= 2;
		playfragment(phandle, buffer, 0, 2 * fragmentsize);
		size += 2 * fragmentsize;
		pidx += 2;
		pidx %= 2;
		while (ok && size < 3 * 176400) {	/* 30 seconds */
			capturefragment(rhandle, buffer, ridx++, fragmentsize);
			ridx %= 2;
			playfragment(phandle, buffer, pidx++, fragmentsize);
			size += fragmentsize;
			pidx %= 2;
			if (playbackunderrun(phandle) > 0)
				ok = 0;
		}
		if ((err = snd_pcm_playback_status(phandle, &pstatus)) < 0) {
			printf("Playback status error: %s\n", snd_strerror(err));
			exit(0);
		}
		snd_pcm_flush_capture(rhandle);
		snd_pcm_flush_playback(phandle);
		if (ok && !playbackunderrun(phandle) > 0) {
			printf("Playback OK!!!\n");
			printf("Playback time = %li.%i, Record time = %li.%i, diff = %li\n",
			       pstatus.stime.tv_sec,
			       (int)pstatus.stime.tv_usec,
			       rstatus.stime.tv_sec,
			       (int)rstatus.stime.tv_usec,
			       timediff(pstatus.stime, rstatus.stime));
			break;
		}
	}
	snd_pcm_close(phandle);
	snd_pcm_close(rhandle);
	return 0;
}
