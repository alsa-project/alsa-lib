#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/asoundlib.h>
#include <string.h>
#include <signal.h>

static void usage(void)
{
	fprintf(stderr, "Usage: midiloop [options]\n");
	fprintf(stderr, "  options:\n");
	fprintf(stderr, "    -v: verbose mode\n");
	fprintf(stderr, "    -i [ card-id device-id ] : test input device\n");
	fprintf(stderr, "    -o [ card-id device-id ] : test output device\n");
}

int stop = 0;

void sighandler(int dum)
{
	stop=1;
}

long long timediff(struct timeval t1, struct timeval t2)
{
	signed long l;

	t1.tv_sec -= t2.tv_sec;
	l = (signed long) t1.tv_usec - (signed long) t2.tv_usec;
	if (l < 0) {
		t1.tv_sec--;
		l = -l;
		l %= 1000000;
	}
	return ((long long)t1.tv_sec * (long long)1000000) + (long long)l;
}

int writepattern(snd_rawmidi_t *handle_out, unsigned char *obuf)
{
	int patsize, i;

	patsize = 0;
	for (i = 0; i < 15; i++) {
		obuf[patsize++] = 0x90 + i;
		obuf[patsize++] = 0x40;
		obuf[patsize++] = 0x3f;
		obuf[patsize++] = 0xb0 + i;
		obuf[patsize++] = 0x2e;
		obuf[patsize++] = 0x7a;
		obuf[patsize++] = 0x80 + i;
		obuf[patsize++] = 0x23;
		obuf[patsize++] = 0x24;
		obuf[patsize++] = 0x25;
		obuf[patsize++] = 0x26;
	}
	i = snd_rawmidi_write(handle_out, obuf, patsize);
	if (i != patsize) {
		printf("Written only %i bytes from %i bytes\n", i, patsize);
		exit(EXIT_FAILURE);
	}
	return patsize;
}

int main(int argc, char** argv)
{
	int i, j, k, opos, ipos, patsize;
	int err;
	int verbose = 0;
	int card_in = -1, device_in = 0;
	int card_out = -1, device_out = 0;	
	snd_rawmidi_t *handle_in = NULL, *handle_out = NULL;
	unsigned char ibuf[512], obuf[512];
	struct timeval start, end;
	long long diff;
	snd_rawmidi_status_t istat, ostat;
	
	if (argc == 1) {
		usage();
		exit(EXIT_SUCCESS);
	}
	
	for (i = 1 ; i<argc ; i++) {
		if (argv[i][0]=='-') {
			switch (argv[i][1]) {
				case 'h':
					usage();
					break;
				case 'v':
					verbose = 1;
					break;
				case 'i':
					card_in = atoi(argv[i+1]);
					if (isdigit(argv[i+2][0])) {
						device_in = atoi(argv[i+2]);
					} else {
						fprintf(stderr,"Error: -i with card_id, but missing device id\n");
						exit(-1);
					}
					i+=2;
					break;
				case 'o':
					card_out = atoi(argv[i+1]);
					if (isdigit(argv[i+2][0])) {
						device_out = atoi(argv[i+2]);
					}else{
						fprintf(stderr,"Error: -i with card_id, but missing device id\n");
						exit(EXIT_FAILURE);
					}
					break;
			}			
		}
	}

	if (card_in == -1 && card_out == -1) {
		fprintf(stderr, "specify at least one device\n");
		exit(EXIT_FAILURE);
	}

	if (card_in == -1)
		card_in = card_out;
	if (card_out == -1)
		card_out = card_in;
	
	if (verbose) {
		fprintf(stderr, "Using: \n");
		fprintf(stderr, "  Input: ");
		fprintf(stderr, "card %d, device %d\n", card_in, device_in);
		fprintf(stderr, "  Output: ");
		fprintf(stderr, "card %d, device %d\n", card_out, device_out);
	}
	
	err = snd_rawmidi_open(&handle_in, card_in, device_in, SND_RAWMIDI_OPEN_INPUT | SND_RAWMIDI_OPEN_NONBLOCK);
	if (err) {
		fprintf(stderr,"snd_rawmidi_open %d %d failed: %d\n",card_in,device_in,err);
		exit(EXIT_FAILURE);
	}

	err = snd_rawmidi_open(&handle_out, card_out, device_out, SND_RAWMIDI_OPEN_OUTPUT);
	if (err) {
		fprintf(stderr,"snd_rawmidi_open %d %d failed: %d\n",card_out,device_out,err);
		exit(EXIT_FAILURE);
	}

	signal(SIGINT, sighandler);

	i = snd_rawmidi_read(handle_in, ibuf, sizeof(ibuf));
	if (i > 0) {
		printf("Read ahead: %i\n", i);
		for (j = 0; j < i; j++)
			printf("%02x:", ibuf[j]);
		printf("\n");
		exit(EXIT_FAILURE);
	}

	patsize = writepattern(handle_out, obuf);
	gettimeofday(&start, NULL);
	patsize = writepattern(handle_out, obuf);

	k = ipos = opos = err = 0;
	while (!stop) {
		i = snd_rawmidi_read(handle_in, ibuf, sizeof(ibuf));
		for (j = 0; j < i; j++, ipos++)
			if (obuf[k] != ibuf[j]) {
				printf("ipos = %i, i[0x%x] != o[0x%x]\n", ipos, ibuf[j], obuf[k]);
				if (opos > 0)
					stop = 1;
			} else {
				printf("match success: ipos = %i, opos = %i [%i:0x%x]\n", ipos, opos, k, obuf[k]);
				k++; opos++;
				if (k >= patsize) {
					patsize = writepattern(handle_out, obuf);
					k = 0;
				}
			}
	}

	gettimeofday(&end, NULL);

	printf("End...\n");

	bzero(&istat, sizeof(istat));
	bzero(&ostat, sizeof(ostat));
	istat.stream = SND_RAWMIDI_STREAM_INPUT;
	ostat.stream = SND_RAWMIDI_STREAM_OUTPUT;
	err = snd_rawmidi_stream_status(handle_in, &istat);
	if (err < 0)
		fprintf(stderr, "input stream status error: %d\n", err);
	err = snd_rawmidi_stream_status(handle_out, &ostat);
	if (err < 0)
		fprintf(stderr, "output stream status error: %d\n", err);
	printf("input.status.queue = %i\n", istat.queue);
	printf("input.status.overrun = %i\n", istat.overrun);
	printf("output.status.queue = %i\n", ostat.queue);
	printf("output.status.overrun = %i\n", ostat.overrun);

	diff = timediff(end, start);
	printf("Time diff: %Liusec (%Li bytes/sec)\n", diff, ((long long)opos * 1000000) / diff);

	if (verbose) {
		fprintf(stderr,"Closing\n");
	}
	
	snd_rawmidi_input_flush(handle_in); 
	snd_rawmidi_close(handle_in);	
	snd_rawmidi_output_flush(handle_out); 
	snd_rawmidi_close(handle_out);	

	return 0;
}
