/*
 *  This small demo generates simple sinus wave on output of speakers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include "../include/asoundlib.h"
#include <sys/time.h>
#include <math.h>

char *device = "plughw:0,0";			 /* playback device */
snd_pcm_format_t format = SND_PCM_FORMAT_S16;	 /* sample format */
int rate = 44100;				 /* stream rate */
int channels = 1;				 /* count of channels */
int buffer_time = 500000;			 /* ring buffer length in us */
int period_time = 100000;			 /* period time in us */
double freq = 440;				 /* sinus wave frequency in Hz */

snd_pcm_sframes_t buffer_size;
snd_pcm_sframes_t period_size;
snd_output_t *output = NULL;

static void generate_sine(signed short *samples, int count, double *_phase)
{
	double phase = *_phase;
	double max_phase = 1.0 / freq;
	double step = 1.0 / (double)rate;
	double res;
	int chn, ires;
	
	while (count-- > 0) {
		res = sin((phase * 2 * M_PI) / max_phase - M_PI) * 32767;
		ires = res;
		for (chn = 0; chn < channels; chn++)
			*samples++ = ires;
		// printf("phase: %.8f, max_phase: %.8f, res: %.8f, smp = %i\n", phase, max_phase, res, *(samples-1));
		phase += step;
		if (phase >= max_phase)
			phase -= max_phase;
	}
	*_phase = phase;
}

static int set_hwparams(snd_pcm_t *handle,
			snd_pcm_hw_params_t *params)
{
	int err, dir;

	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		printf("Access type not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		printf("Sample format not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, channels);
	if (err < 0) {
		printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
		return err;
	}
	/* set the stream rate */
	err = snd_pcm_hw_params_set_rate_near(handle, params, rate, 0);
	if (err < 0) {
		printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
		return err;
	}
	if (err != rate) {
		printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
		return -EINVAL;
	}
	/* set buffer time */
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params, buffer_time, &dir);
	if (err < 0) {
		printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
		return err;
	}
	buffer_size = snd_pcm_hw_params_get_buffer_size(params);
	/* set period time */
	err = snd_pcm_hw_params_set_period_time_near(handle, params, period_time, &dir);
	if (err < 0) {
		printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
		return err;
	}
	period_size = snd_pcm_hw_params_get_period_size(params, &dir);
	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
	int err;

	/* get current swparams */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* start transfer when the buffer is full */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, buffer_size);
	if (err < 0) {
		printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* allow transfer when at least period_size samples can be processed */
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
	if (err < 0) {
		printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* align all transfers to 1 samples */
	err = snd_pcm_sw_params_set_xfer_align(handle, swparams, 1);
	if (err < 0) {
		printf("Unable to set transfer align for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* write the parameters to device */
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

/*
 *   Underrun and suspend recovery
 */
 
static int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (err = -EPIPE) {	/* underrun */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
		return 0;
	} else if (err = -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);	/* wait until suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
		}
		return 0;
	}
	return err;
}

/*
 *   Transfer method - write only
 */

static int write_loop(snd_pcm_t *handle, signed short *samples)
{
	int ufds_count;
	struct pollfd *ufds;
	double phase = 0;
	signed short *ptr;
	int err, count, cptr;

	while (1) {
		generate_sine(ptr = samples, cptr = period_size, &phase);
		while (cptr > 0) {
			err = snd_pcm_writei(handle, ptr, cptr);
			if (err == -EAGAIN)
				continue;
			if (err < 0) {
				if (xrun_recovery(handle, err) < 0) {
					printf("Write error: %s\n", snd_strerror(err));
					exit(EXIT_FAILURE);
				}
				break;	/* skip one period */
			}
			ptr += err * channels;
			cptr -= err;
		}
	}
}
 
/*
 *   Transfer method - write and wait for room in buffer using poll
 */

static int wait_for_poll(struct pollfd *ufds, int count)
{
	int i;
	unsigned int events;

	while (1) {
		poll(ufds, count, -1);
		for (i = 0; i < count; i++) {
			events = ufds[i].revents;
			if (events & POLLERR) {
				printf("Poll - POLLERR detected\n");
				return -EIO;
			}
			if (events & POLLOUT)
				return 0;
		}
	}
}

static int write_and_poll_loop(snd_pcm_t *handle, signed short *samples)
{
	int ufds_count;
	struct pollfd *ufds;
	double phase = 0;
	signed short *ptr;
	int err, count, cptr;

	count = snd_pcm_poll_descriptors_count (handle);
	if (count <= 0) {
		printf("Invalid poll descriptors count\n");
		return count;
	}

	ufds = malloc(sizeof(struct pollfd) * count);
	if (ufds == NULL) {
		printf("No enough memory\n");
		return err;;
	}
	if ((err = snd_pcm_poll_descriptors(handle, ufds, count)) < 0) {
		printf("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
		return err;
	}

	while (1) {
		err = wait_for_poll(ufds, count);
		if (err < 0) {
			printf("Wait for poll failed\n");
			return err;
		}

		generate_sine(ptr = samples, cptr = period_size, &phase);
		while (cptr > 0) {
			err = snd_pcm_writei(handle, ptr, cptr);
			if (err < 0) {
				if (xrun_recovery(handle, err) < 0) {
					printf("Write error: %s\n", snd_strerror(err));
					exit(EXIT_FAILURE);
				}
				break;	/* skip one period */
			}
			ptr += err * channels;
			cptr -= err;
			if (cptr == 0)
				break;
			/* it is possible, that initial buffer cannot store */
			/* all data from last period, so wait awhile */
			err = wait_for_poll(ufds, count);
			if (err < 0) {
				printf("Wait for poll failed\n");
				return err;
			}
		}
	}
}

/*
 *
 */

struct transfer_method {
	const char *name;
	int (*transfer_loop)(snd_pcm_t *handle, signed short *samples);
};

static struct transfer_method transfer_methods[] = {
	{ "write", write_loop },
	{ "write_and_poll", write_and_poll_loop },
	{ NULL, NULL }
};

static void help(void)
{
	int k;
	printf("\
Usage: latency [OPTION]... [FILE]...
-h,--help       help
-D,--device     playback device
-r,--rate	stream rate in Hz
-c,--channels	count of channels in stream
-f,--frequency  sine wave frequency in Hz
-b,--buffer     ring buffer size in samples
-p,--period     period size in us
-m,--method     tranfer method

");
        printf("Recognized sample formats are:");
        for (k = 0; k < SND_PCM_FORMAT_LAST; ++(unsigned long) k) {
                const char *s = snd_pcm_format_name(k);
                if (s)
                        printf(" %s", s);
        }
        printf("\n");
        printf("Recognized tranfer methods are:");
        for (k = 0; transfer_methods[k].name; k++)
        	printf(" %s", transfer_methods[k].name);
	printf("\n");
}

int main(int argc, char *argv[])
{
	struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},
		{"device", 1, NULL, 'D'},
		{"rate", 1, NULL, 'r'},
		{"channels", 1, NULL, 'c'},
		{"frequency", 1, NULL, 'f'},
		{"buffer", 1, NULL, 'b'},
		{"period", 1, NULL, 'p'},
		{"method", 1, NULL, 'm'},
		{NULL, 0, NULL, 0},
	};
	snd_pcm_t *handle;
	char *buffer;
	int err, morehelp;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	double phase;
	int method = 0;
	signed short *samples;

	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_sw_params_alloca(&swparams);

	morehelp = 0;
	while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "hD:r:c:f:b:p:m:", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			morehelp++;
			break;
		case 'D':
			device = strdup(optarg);
			break;
		case 'r':
			rate = atoi(optarg);
			rate = rate < 4000 ? 4000 : rate;
			rate = rate > 196000 ? 196000 : rate;
			break;
		case 'c':
			channels = atoi(optarg);
			channels = channels < 1 ? 1 : channels;
			channels = channels > 1024 ? 1024 : channels;
			break;
		case 'f':
			freq = atoi(optarg);
			freq = freq < 50 ? 50 : freq;
			freq = freq > 5000 ? 5000 : freq;
			break;
		case 'b':
			buffer_size = atoi(optarg);
			buffer_size = buffer_size < 64 ? 64 : buffer_size;
			buffer_size = buffer_size > 64*1024 ? 64*1024 : buffer_size;
			break;
		case 'p':
			period_time = atoi(optarg);
			period_time = period_time < 1000 ? 1000 : period_time;
			period_time = period_time > 1000000 ? 1000000 : period_time;
			break;
		case 'm':
			for (method = 0; transfer_methods[method].name; method++)
				if (!strcasecmp(transfer_methods[method].name, optarg))
					break;
			if (transfer_methods[method].name == NULL)
				method = 0;
			break;
		}
	}

	if (morehelp) {
		help();
		return 0;
	}

	err = snd_output_stdio_attach(&output, stdout, 0);
	if (err < 0) {
		printf("Output failed: %s\n", snd_strerror(err));
		return 0;
	}

	printf("Playback device is %s\n", device);
	printf("Stream parameters are %iHz, %s, %i channels\n", rate, snd_pcm_format_name(format), channels);
	printf("Sine wave rate is %.4fHz\n", freq);
	printf("Using transfer method: %s\n", transfer_methods[method].name);

	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		return 0;
	}
	
	if ((err = set_hwparams(handle, hwparams)) < 0) {
		printf("Setting of hwparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = set_swparams(handle, swparams)) < 0) {
		printf("Setting of swparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	samples = malloc((period_size * channels * snd_pcm_format_width(format)) / 8);
	if (samples == NULL) {
		printf("No enough memory\n");
		exit(EXIT_FAILURE);
	}

	err = transfer_methods[method].transfer_loop(handle, samples);
	if (err < 0)
		printf("Transfer failed: %s\n", snd_strerror(err));

	snd_pcm_close(handle);
	return 0;
}

