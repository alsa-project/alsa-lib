#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/asoundlib.h>
#include <signal.h>

static void usage(void)
{
	fprintf(stderr, "usage: rawmidi [options]\n");
	fprintf(stderr, "  options:\n");
	fprintf(stderr, "    -v: verbose mode\n");
	fprintf(stderr, "    -i [ card-id device-id | node ] : test input device\n");
	fprintf(stderr, "    -o [ card-id device-id | node ] : test output device\n");
	fprintf(stderr, "    -t: test midi thru\n");
	fprintf(stderr, "  example:\n");
	fprintf(stderr, "    rawmidi -i 0 0 -o /dev/midi1\n");
	fprintf(stderr, "    tests input for card 0, device 0, using snd_rawmidi API\n");
	fprintf(stderr, "    and /dev/midi1 using file desciptors\n");
}

int stop=0;

void sighandler(int dum)
{
	stop=1;
}

int main(int argc,char** argv)
{
	int i;
	int err;
	int thru=0;
	int verbose = 0;
	int card_in = -1,device_in = -1;
	int card_out = -1,device_out = -1;
	char* node_in = 0;
	char* node_out = 0;
	
	int fd_in = -1,fd_out = -1;
	snd_rawmidi_t *handle_in = 0,*handle_out = 0;
	
	if (argc==1) {
		usage();
		exit(0);
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
				case 't':
					thru = 1;
					break;
				case 'i':
					if (isdigit(argv[i+1][0])) {
						card_in = atoi(argv[i+1]);
						if (isdigit(argv[i+2][0])) {
							device_in = atoi(argv[i+2]);
						}else{
							fprintf(stderr,"Error: -i with card_id, but missing device id\n");
							exit(-1);
						}
						i+=2;
					}else{
						node_in = argv[i+1];
						i++;
					}
					break;
				case 'o':
					if (isdigit(argv[i+1][0])) {
						card_out = atoi(argv[i+1]);
						if (isdigit(argv[i+2][0])) {
							device_out = atoi(argv[i+2]);
						}else{
							fprintf(stderr,"Error: -i with card_id, but missing device id\n");
							exit(-1);
						}
						i+=2;
					}else{
						node_out = argv[i+1];
						i++;
					}
					break;
			}			
		}
	}
	
	if (verbose) {
		fprintf(stderr,"Using: \n");
		fprintf(stderr,"Input: ");
		if (card_in!=-1) {
			fprintf(stderr,"card %d, device %d\n",card_in,device_in);
		}else if (node_in){
			fprintf(stderr,"%s\n",node_in);		
		}else{
			fprintf(stderr,"NONE\n");
		}
		fprintf(stderr,"Output: ");
		if (card_out!=-1) {
			fprintf(stderr,"card %d, device %d\n",card_out,device_out);
		}else if (node_out){
			fprintf(stderr,"%s\n",node_out);		
		}else{
			fprintf(stderr,"NONE\n");
		}
	}
	
	if (card_in!=-1) {
		err = snd_rawmidi_open(&handle_in,card_in,device_in,O_RDONLY);		
		if (err) {
			fprintf(stderr,"snd_rawmidi_open %d %d failed: %d\n",card_in,device_in,err);
		}
	}
	if (node_in) {
		fd_in = open(node_in,O_RDONLY);		
		if (err) {
			fprintf(stderr,"open %s for input failed\n",node_in);
		}	
	}

	signal(SIGINT,sighandler);

	if (card_out!=-1) {
		err = snd_rawmidi_open(&handle_out,card_out,device_out,O_WRONLY);		
		if (err) {
			fprintf(stderr,"snd_rawmidi_open %d %d failed: %d\n",card_out,device_out,err);
		}
	}
	if (node_out) {
		fd_out = open(node_out,O_WRONLY);		
		if (err) {
			fprintf(stderr,"open %s for output failed\n",node_out);
		}	
	}

	if (!thru) {
		if (handle_in || fd_in!=-1) {
			if (verbose) {
				fprintf(stderr,"Read midi in\n");
			}
		}

		if (handle_in) {
			unsigned char ch;
			while (!stop) {
				snd_rawmidi_read(handle_in,&ch,1);
				if (verbose) {
					fprintf(stderr,"read %02x\n",ch);
				}
			}
		}
		if (fd_in!=-1) {
			unsigned char ch;
			while (!stop) {
				read(fd_in,&ch,1);
				if (verbose) {
					fprintf(stderr,"read %02x\n",ch);
				}
			}	
		}

		if (handle_out || fd_out!=-1) {
			if (verbose) {
				fprintf(stderr,"Writing note on / note off\n");
			}
		}

		if (handle_out) {
			unsigned char ch;
			ch=0x90; snd_rawmidi_write(handle_out,&ch,1);
			ch=60;   snd_rawmidi_write(handle_out,&ch,1);
			ch=100;  snd_rawmidi_write(handle_out,&ch,1);
			snd_rawmidi_output_flush(handle_in); 
			sleep(1);
			ch=0x90; snd_rawmidi_write(handle_out,&ch,1);
			ch=60;   snd_rawmidi_write(handle_out,&ch,1);
			ch=0;    snd_rawmidi_write(handle_out,&ch,1);
			snd_rawmidi_output_flush(handle_out); 
		}
		if (fd_out!=-1) {
			unsigned char ch;
			ch=0x90; write(fd_out,&ch,1);
			ch=60;   write(fd_out,&ch,1);
			ch=100;  write(fd_out,&ch,1);
			sleep(1);
			ch=0x90; write(fd_out,&ch,1);
			ch=60;   write(fd_out,&ch,1);
			ch=0;    write(fd_out,&ch,1);
		}
	} else {
		if ((handle_in || fd_in!=-1) && (handle_out || fd_out!=-1)) {
			if (verbose) {
				fprintf(stderr,"Testing midi thru in\n");
			}
			while (!stop) {
				unsigned char ch;
			
				if (handle_in) {
					snd_rawmidi_read(handle_in,&ch,1);
				}
				if (fd_in!=-1) {
					read(fd_in,&ch,1);
				}	
				if (verbose) {
					fprintf(stderr,"thru: %02x\n",ch);
				}

				if (handle_out) {
					snd_rawmidi_write(handle_out,&ch,1);
					snd_rawmidi_output_flush(handle_out); 
				}
				if (fd_out!=-1) {
					write(fd_out,&ch,1);
				}
			}
		}else{
				fprintf(stderr,"Testing midi thru needs both input and output\n");		
				exit(-1);
		}
	}

	if (verbose) {
		fprintf(stderr,"Closing\n");
	}
	
	if (handle_in) {
		snd_rawmidi_output_flush(handle_in); 
		snd_rawmidi_close(handle_in);	
	}
	if (handle_out) {
		snd_rawmidi_output_flush(handle_in); 
		snd_rawmidi_close(handle_in);	
	}
	if (fd_in!=-1) {
		close(fd_in);
	}
	if (fd_out!=-1) {
		close(fd_out);
	}

	return 0;
}
