#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include "../include/mixer_ordinary.h"
#include <sys/time.h>
#include <math.h>

static void help(void)
{
	printf(
"Usage: omixer [OPTION]...\n"
"-h,--help      help\n"
"-P,--pname     playback device\n"
"-C,--cname     capture device\n"
"\n");
}

int main(int argc, char *argv[])
{
	struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},
		{"pname", 1, NULL, 'P'},
		{"cname", 1, NULL, 'C'},
		{NULL, 0, NULL, 0},
	};
	int err, morehelp;
	char *pname = "default", *cname = "default";
	sndo_mixer_t *handle;

	morehelp = 0;
	while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "hP:C:", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			morehelp++;
			break;
		case 'P':
			pname = strdup(optarg);
			break;
		case 'C':
			cname = strdup(optarg);
			break;
		}
	}

	if (morehelp) {
		help();
		return 0;
	}

	err = sndo_mixer_open(&handle, pname, cname, NULL);
	if (err < 0) {
		fprintf(stderr, "mixer open error: %s\n", snd_strerror(err));
		return EXIT_FAILURE;
	}
	sndo_mixer_close(handle);
	return EXIT_SUCCESS;
}
