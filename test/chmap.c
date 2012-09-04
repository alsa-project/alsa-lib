/*
 * channel mapping API test program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include "../include/asoundlib.h"

static void usage(void)
{
	printf("usage: chmap [options] query\n"
	       "       chmap [options] get\n"
	       "       chmap [options] set CH0 CH1 CH2...\n"
	       "options:\n"
	       "  -D device     Specify PCM device to handle\n"
	       "  -f format     PCM format\n"
	       "  -c channels   Channels\n"
	       "  -r rate       Sample rate\n");
}

static const char * const chname[] = {
	"Unknown",
	"FL", "FR", "RL", "RR", "FC", "LFE", "SL", "SR", "RC",
	"FLC", "FRC", "RLC", "RRC", "FLW", "FRW", "FLH",
	"FCH", "FCH", "FRH", "TC",
	"N/A",
};

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof(x[0]))

static void print_channels(const snd_pcm_chmap_t *map)
{
	unsigned int i;

	printf("  ");
	for (i = 0; i < map->channels; i++) {
		unsigned int c = map->pos[i];
		unsigned int p = c & SND_CHMAP_POSITION_MASK;
		if (c & SND_CHMAP_DRIVER_SPEC)
			printf(" %d", p);
		else if (p >= ARRAY_SIZE(chname))
			printf(" Ch%d", p);
		else
			printf(" %s", chname[p]);
		if (c & SND_CHMAP_PHASE_INVERSE)
			printf("[INV]");
	}
	printf("\n");
}

static int to_channel(const char *name)
{
	unsigned int i;

	if (isdigit(*name))
		return atoi(name);
	for (i = 0; i < ARRAY_SIZE(chname); i++)
		if (!strcmp(chname[i], name))
			return i;
	return SND_CHMAP_UNKNOWN;
}

static const char *chmap_type(int type)
{
	switch (type) {
	case SND_CHMAP_NONE:
		return "None";
	case SND_CHMAP_FIXED:
		return "Fixed";
	case SND_CHMAP_VAR:
		return "Variable";
	case SND_CHMAP_PAIRED:
		return "Paired";
	default:
		return "Unknown";
	}
}

static int query_chmaps(snd_pcm_t *pcm)
{
	snd_pcm_chmap_query_t **maps = snd_pcm_query_chmaps(pcm);
	snd_pcm_chmap_query_t **p, *v;

	if (!maps) {
		printf("Cannot query maps\n");
		return 1;
	}
	for (p = maps; (v = *p) != NULL; p++) {
		printf("Type = %s, Channels = %d\n", chmap_type(v->type), v->map.channels);
		print_channels(&v->map);
	}
	snd_pcm_free_chmaps(maps);
	return 0;
}

static int setup_pcm(snd_pcm_t *pcm, int format, int channels, int rate)
{
	snd_pcm_hw_params_t *params;

	snd_pcm_hw_params_alloca(&params);
	if (snd_pcm_hw_params_any(pcm, params) < 0) {
		printf("Cannot init hw_params\n");
		return -1;
	}
	if (format != SND_PCM_FORMAT_UNKNOWN) {
		if (snd_pcm_hw_params_set_format(pcm, params, format) < 0) {
			printf("Cannot set format %s\n",
			       snd_pcm_format_name(format));
			return -1;
		}
	}
	if (channels > 0) {
		if (snd_pcm_hw_params_set_channels(pcm, params, channels) < 0) {
			printf("Cannot set channels %d\n", channels);
			return -1;
		}
	}
	if (rate > 0) {
		if (snd_pcm_hw_params_set_rate_near(pcm, params, (unsigned int *)&rate, 0) < 0) {
			printf("Cannot set rate %d\n", rate);
			return -1;
		}
	}
	if (snd_pcm_hw_params(pcm, params) < 0) {
		printf("Cannot set hw_params\n");
		return -1;
	}
	return 0;
}

static int get_chmap(snd_pcm_t *pcm, int format, int channels, int rate)
{
	snd_pcm_chmap_t *map;

	if (setup_pcm(pcm, format, channels, rate))
		return 1;
	map = snd_pcm_get_chmap(pcm);
	if (!map) {
		printf("Cannot get chmap\n");
		return 1;
	}
	printf("Channels = %d\n", map->channels);
	print_channels(map);
	free(map);
	return 0;
}

static int set_chmap(snd_pcm_t *pcm, int format, int channels, int rate,
		     int nargs, char **arg)
{
	int i;
	snd_pcm_chmap_t *map;

	if (channels && channels != nargs) {
		printf("Inconsistent channels %d vs %d\n", channels, nargs);
		return 1;
	}
	if (!channels) {
		if (!nargs) {
			printf("No channels are given\n");
			return 1;
		}
		channels = nargs;
	}
	if (setup_pcm(pcm, format, channels, rate))
		return 1;
	map = malloc(sizeof(int) * channels + 1);
	if (!map) {
		printf("cannot malloc\n");
		return 1;
	}
	map->channels = channels;
	for (i = 0; i < channels; i++)
		map->pos[i] = to_channel(arg[i]);
	if (snd_pcm_set_chmap(pcm, map) < 0) {
		printf("Cannot set chmap\n");
		return 1;
	}
	free(map);

	map = snd_pcm_get_chmap(pcm);
	if (!map) {
		printf("Cannot get chmap\n");
		return 1;
	}
	printf("Get channels = %d\n", map->channels);
	print_channels(map);
	free(map);
	return 0;
}

int main(int argc, char **argv)
{
	char *device = NULL;
	int stream = SND_PCM_STREAM_PLAYBACK;
	int format = SND_PCM_FORMAT_UNKNOWN;
	int channels = 0;
	int rate = 0;
	snd_pcm_t *pcm;
	int c;

	while ((c = getopt(argc, argv, "D:s:f:c:r:")) != -1) {
		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			if (*optarg == 'c' || *optarg == 'C')
				stream = SND_PCM_STREAM_CAPTURE;
			else
				stream = SND_PCM_STREAM_PLAYBACK;
			break;
		case 'f':
			format = snd_pcm_format_value(optarg);
			break;
		case 'c':
			channels = atoi(optarg);
			break;
		case 'r':
			rate = atoi(optarg);
			break;
		default:
			usage();
			return 1;
		}
	}

	if (argc <= optind) {
		usage();
		return 1;
	}

	if (!device) {
		printf("No device is specified\n");
		return 1;
	}

	if (snd_pcm_open(&pcm, device, stream, SND_PCM_NONBLOCK) < 0) {
		printf("Cannot open PCM stream %s for %s\n", device,
		       snd_pcm_stream_name(stream));
		return 1;
	}

	switch (*argv[optind]) {
	case 'q':
		return query_chmaps(pcm);
	case 'g':
		return get_chmap(pcm, format, channels, rate);
	case 's':
		return set_chmap(pcm, format, channels, rate,
				 argc - optind - 1, argv + optind + 1);
	}
	usage();
	return 1;
}
