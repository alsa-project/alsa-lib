/*
 *  Simple event decoder
 */

static char *event_names[256] = {
	/* 0   */	"System",
	/* 1   */	"Result",
	/* 2   */	"Reserved 2",
	/* 3   */	"Reserved 3",
	/* 4   */	"Reserved 4",
	/* 5   */	"Note",
	/* 6   */	"Note On",
	/* 7   */	"Note Off",
	/* 8   */	"Key Pressure",
	/* 9   */	"Reserved 9",
	/* 10   */	"Controller",
	/* 11   */	"Program Change",
	/* 12   */	"Channel Pressure",
	/* 13   */	"Pitchbend",
	/* 14   */	"Control14",
	/* 15   */	"Nonregparam",
	/* 16   */	"Regparam",
	/* 17   */	"Reserved 17",
	/* 18   */	"Reserved 18",
	/* 19   */	"Reserved 19",
	/* 20   */	"Song Position",
	/* 21   */	"Song Select",
	/* 22   */	"Qframe",
	/* 23   */	"SMF Time Signature",
	/* 24   */	"SMF Key Signature",
	/* 25   */	"Reserved 25",
	/* 26   */	"Reserved 26",
	/* 27   */	"Reserved 27",
	/* 28   */	"Reserved 28",
	/* 29   */	"Reserved 29",
	/* 30   */	"Start",
	/* 31   */	"Continue",
	/* 32   */	"Stop",
	/* 33   */	"Set Position Tick",
	/* 34   */	"Set Position Time",
	/* 35   */	"Tempo",
	/* 36   */	"Clock",
	/* 37   */	"Tick",
	/* 38   */	"Reserved 38",
	/* 39   */	"Reserved 39",
	/* 40   */	"Tune Request",
	/* 41   */	"Reset",
	/* 42   */	"Active Sensing",
	/* 43   */	"Reserved 43",
	/* 44   */	"Reserved 44",
	/* 45   */	"Reserved 45",
	/* 46   */	"Reserved 46",
	/* 47   */	"Reserved 47",
	/* 48   */	"Reserved 48",
	/* 49   */	"Reserved 49",
	/* 50   */	"Echo",
	/* 51   */	"OSS",
	/* 52   */	"Reserved 52",
	/* 53   */	"Reserved 53",
	/* 54   */	"Reserved 54",
	/* 55   */	"Reserved 55",
	/* 56   */	"Reserved 56",
	/* 57   */	"Reserved 57",
	/* 58   */	"Reserved 58",
	/* 59   */	"Reserved 59",
	/* 60   */	"Client Start",
	/* 61   */	"Client Exit",
	/* 62   */	"Client Change",
	/* 63   */	"Port Start",
	/* 64   */	"Port Exit",
	/* 65   */	"Port Change",
	/* 66   */	"Port Subscribed",
	/* 67   */	"Port Used",
	/* 68   */	"Port Unsubscribed",
	/* 69   */	"Port Unused",
	/* 70   */	"Sample",
	/* 71   */	"Sample Cluster",
	/* 72   */	"Sample Start",
	/* 73   */	"Sample Stop",
	/* 74   */	"Sample Freq",
	/* 75   */	"Sample Volume",
	/* 76   */	"Sample Loop",
	/* 77   */	"Sample Position",
	/* 78   */	"Sample Private1",
	/* 79   */	"Reserved 79",
	/* 80   */	"Reserved 80",
	/* 81   */	"Reserved 81",
	/* 82   */	"Reserved 82",
	/* 83   */	"Reserved 83",
	/* 84   */	"Reserved 84",
	/* 85   */	"Reserved 85",
	/* 86   */	"Reserved 86",
	/* 87   */	"Reserved 87",
	/* 88   */	"Reserved 88",
	/* 89   */	"Reserved 89",
	/* 90   */	"User 0",
	/* 91   */	"User 1",
	/* 92   */	"User 2",
	/* 93   */	"User 3",
	/* 94   */	"User 4",
	/* 95   */	"User 5",
	/* 96   */	"User 6",
	/* 97   */	"User 7",
	/* 98   */	"User 8",
	/* 99   */	"User 9",
	/* 100  */	"Instr Begin",
	/* 101  */	"Instr End",
	/* 102  */	"Instr Info",
	/* 103  */	"Instr Info Result",
	/* 104  */	"Instr Finfo",
	/* 105  */	"Instr Finfo Result",
	/* 106  */	"Instr Reset",
	/* 107  */	"Instr Status",
	/* 108  */	"Instr Status Result",
	/* 109  */	"Instr Put",
	/* 110  */	"Instr Get",
	/* 111  */	"Instr Get Result",
	/* 112  */	"Instr Free",
	/* 113  */	"Instr List",
	/* 114  */	"Instr List Result",
	/* 115  */	"Instr Cluster",
	/* 116  */	"Instr Cluster Get",
	/* 117  */	"Instr Cluster Result",
	/* 118  */	"Instr Change",
	/* 119  */	"Reserved 119",
	/* 120  */	"Reserved 120",
	/* 121  */	"Reserved 121",
	/* 122  */	"Reserved 122",
	/* 123  */	"Reserved 123",
	/* 124  */	"Reserved 124",
	/* 125  */	"Reserved 125",
	/* 126  */	"Reserved 126",
	/* 127  */	"Reserved 127",
	/* 128  */	"Reserved 128",
	/* 129  */	"Reserved 129",
	/* 130  */	"Sysex",
	/* 131  */	"Bounce",
	/* 132  */	"Reserved 132",
	/* 133  */	"Reserved 133",
	/* 134  */	"Reserved 134",
	/* 135  */	"User Var0",
	/* 136  */	"User Var1",
	/* 137  */	"User Var2",
	/* 138  */	"User Var3",
	/* 139  */	"User Var4",
	/* 140  */	"IPC Shm",
	/* 141  */	"Reserved 141",
	/* 142  */	"Reserved 142",
	/* 143  */	"Reserved 143",
	/* 144  */	"Reserved 144",
	/* 145  */	"User IPC0",
	/* 146  */	"User IPC1",
	/* 147  */	"User IPC2",
	/* 148  */	"User IPC3",
	/* 149  */	"User IPC4",
	/* 150  */	"Reserved 150",
	/* 151  */	"Reserved 151",
	/* 152  */	"Reserved 152",
	/* 153  */	"Reserved 153",
	/* 154  */	"Reserved 154",
	/* 155  */	"Reserved 155",
	/* 156  */	"Reserved 156",
	/* 157  */	"Reserved 157",
	/* 158  */	"Reserved 158",
	/* 159  */	"Reserved 159",
	/* 160  */	"Reserved 160",
	/* 161  */	"Reserved 161",
	/* 162  */	"Reserved 162",
	/* 163  */	"Reserved 163",
	/* 164  */	"Reserved 164",
	/* 165  */	"Reserved 165",
	/* 166  */	"Reserved 166",
	/* 167  */	"Reserved 167",
	/* 168  */	"Reserved 168",
	/* 169  */	"Reserved 169",
	/* 170  */	"Reserved 170",
	/* 171  */	"Reserved 171",
	/* 172  */	"Reserved 172",
	/* 173  */	"Reserved 173",
	/* 174  */	"Reserved 174",
	/* 175  */	"Reserved 175",
	/* 176  */	"Reserved 176",
	/* 177  */	"Reserved 177",
	/* 178  */	"Reserved 178",
	/* 179  */	"Reserved 179",
	/* 180  */	"Reserved 180",
	/* 181  */	"Reserved 181",
	/* 182  */	"Reserved 182",
	/* 183  */	"Reserved 183",
	/* 184  */	"Reserved 184",
	/* 185  */	"Reserved 185",
	/* 186  */	"Reserved 186",
	/* 187  */	"Reserved 187",
	/* 188  */	"Reserved 188",
	/* 189  */	"Reserved 189",
	/* 190  */	"Reserved 190",
	/* 191  */	"Reserved 191",
	/* 192  */	"Reserved 192",
	/* 193  */	"Reserved 193",
	/* 194  */	"Reserved 194",
	/* 195  */	"Reserved 195",
	/* 196  */	"Reserved 196",
	/* 197  */	"Reserved 197",
	/* 198  */	"Reserved 198",
	/* 199  */	"Reserved 199",
	/* 200  */	"Reserved 200",
	/* 201  */	"Reserved 201",
	/* 202  */	"Reserved 202",
	/* 203  */	"Reserved 203",
	/* 204  */	"Reserved 204",
	/* 205  */	"Reserved 205",
	/* 206  */	"Reserved 206",
	/* 207  */	"Reserved 207",
	/* 208  */	"Reserved 208",
	/* 209  */	"Reserved 209",
	/* 210  */	"Reserved 210",
	/* 211  */	"Reserved 211",
	/* 212  */	"Reserved 212",
	/* 213  */	"Reserved 213",
	/* 214  */	"Reserved 214",
	/* 215  */	"Reserved 215",
	/* 216  */	"Reserved 216",
	/* 217  */	"Reserved 217",
	/* 218  */	"Reserved 218",
	/* 219  */	"Reserved 219",
	/* 220  */	"Reserved 220",
	/* 221  */	"Reserved 221",
	/* 222  */	"Reserved 222",
	/* 223  */	"Reserved 223",
	/* 224  */	"Reserved 224",
	/* 225  */	"Reserved 225",
	/* 226  */	"Reserved 226",
	/* 227  */	"Reserved 227",
	/* 228  */	"Reserved 228",
	/* 229  */	"Reserved 229",
	/* 230  */	"Reserved 230",
	/* 231  */	"Reserved 231",
	/* 232  */	"Reserved 232",
	/* 233  */	"Reserved 233",
	/* 234  */	"Reserved 234",
	/* 235  */	"Reserved 235",
	/* 236  */	"Reserved 236",
	/* 237  */	"Reserved 237",
	/* 238  */	"Reserved 238",
	/* 239  */	"Reserved 239",
	/* 240  */	"Reserved 240",
	/* 241  */	"Reserved 241",
	/* 242  */	"Reserved 242",
	/* 243  */	"Reserved 243",
	/* 244  */	"Reserved 244",
	/* 245  */	"Reserved 245",
	/* 246  */	"Reserved 246",
	/* 247  */	"Reserved 247",
	/* 248  */	"Reserved 248",
	/* 249  */	"Reserved 249",
	/* 250  */	"Reserved 250",
	/* 251  */	"Reserved 251",
	/* 252  */	"Reserved 252",
	/* 253  */	"Reserved 253",
	/* 254  */	"Reserved 254",
	/* 255  */	"None"
};

int decode_event(snd_seq_event_t * ev)
{
	char *space = "         ";

	printf("EVENT>>> Type = %d, flags = 0x%x", ev->type, ev->flags);
	switch (ev->flags & SND_SEQ_TIME_STAMP_MASK) {
		case SND_SEQ_TIME_STAMP_TICK:
			printf(", time = %d ticks",
			       ev->time.tick);
			break;
		case SND_SEQ_TIME_STAMP_REAL:
			printf(", time = %d.%09d",
			       (int)ev->time.time.tv_sec,
			       (int)ev->time.time.tv_nsec);
			break;
	}
	printf("\n%sSource = %d.%d, dest = %d.%d, queue = %d\n",
	       space,
	       ev->source.client,
	       ev->source.port,
	       ev->dest.client,
	       ev->dest.port,
	       ev->queue);

	printf("%sEvent = %s", space, event_names[ev->type]);
	/* decode actual event data... */
	switch (ev->type) {
		case SND_SEQ_EVENT_NOTE:
			printf("; ch=%d, note=%d, velocity=%d, off_velocity=%d, duration=%d\n",
			       ev->data.note.channel,
			       ev->data.note.note,
			       ev->data.note.velocity,
			       ev->data.note.off_velocity,
			       ev->data.note.duration);
			break;

		case SND_SEQ_EVENT_NOTEON:
		case SND_SEQ_EVENT_NOTEOFF:
		case SND_SEQ_EVENT_KEYPRESS:
			printf("; ch=%d, note=%d, velocity=%d\n",
			       ev->data.note.channel,
			       ev->data.note.note,
			       ev->data.note.velocity);
			break;
		
		case SND_SEQ_EVENT_CONTROLLER:
			printf("; ch=%d, param=%i, value=%i\n",
			       ev->data.control.channel,
			       ev->data.control.param,
			       ev->data.control.value);
			break;

		case SND_SEQ_EVENT_PGMCHANGE:
			printf("; ch=%d, program=%i\n",
			       ev->data.control.channel,
			       ev->data.control.value);
			break;
			
		case SND_SEQ_EVENT_CHANPRESS:
		case SND_SEQ_EVENT_PITCHBEND:
			printf("; ch=%d, value=%i\n",
			       ev->data.control.channel,
			       ev->data.control.value);
			break;
			
		case SND_SEQ_EVENT_SYSEX:
			{
				unsigned char *sysex = (unsigned char *) ev + sizeof(snd_seq_event_t);
				int c;

				printf("; len=%d [", ev->data.ext.len);

				for (c = 0; c < ev->data.ext.len; c++) {
					printf("%02x%s", sysex[c], c < ev->data.ext.len - 1 ? ":" : "");
				}
				printf("]\n");
			}
			break;
			
		case SND_SEQ_EVENT_QFRAME:
			printf("; frame=0x%02x\n", ev->data.control.value);
			break;
			
		case SND_SEQ_EVENT_CLOCK:
		case SND_SEQ_EVENT_START:
		case SND_SEQ_EVENT_CONTINUE:
		case SND_SEQ_EVENT_STOP:
			printf("; queue = %i\n", ev->data.queue.queue);
			break;

		case SND_SEQ_EVENT_SENSING:
			printf("\n");
			break;

		case SND_SEQ_EVENT_ECHO:
			{
				int i;
				
				printf("; ");
				for (i = 0; i < 8; i++) {
					printf("%02i%s", ev->data.raw8.d[i], i < 7 ? ":" : "\n");
				}
		 	}
		 	break;
			
		case SND_SEQ_EVENT_CLIENT_START:
		case SND_SEQ_EVENT_CLIENT_EXIT:
		case SND_SEQ_EVENT_CLIENT_CHANGE:
			printf("; client=%i\n", ev->data.addr.client);
			break;

		case SND_SEQ_EVENT_PORT_START:
		case SND_SEQ_EVENT_PORT_EXIT:
		case SND_SEQ_EVENT_PORT_CHANGE:
		case SND_SEQ_EVENT_PORT_SUBSCRIBED:
		case SND_SEQ_EVENT_PORT_USED:
		case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
		case SND_SEQ_EVENT_PORT_UNUSED:
			printf("; client=%i, port = %i\n", ev->data.addr.client, ev->data.addr.port);
			break;

		default:
			printf("; not implemented\n");
	}


	switch (ev->flags & SND_SEQ_EVENT_LENGTH_MASK) {
		case SND_SEQ_EVENT_LENGTH_FIXED:
			return sizeof(snd_seq_event_t);

		case SND_SEQ_EVENT_LENGTH_VARIABLE:
			return sizeof(snd_seq_event_t) + ev->data.ext.len;
	}

	return 0;
}

void event_decoder_start_timer(snd_seq_t *handle, int queue, int client, int port)
{
	int err;

	if ((err = snd_seq_start_queue(handle, queue, NULL))<0)
		fprintf(stderr, "Timer event output error: %s\n", snd_strerror(err));
	while (snd_seq_drain_output(handle)>0)
		sleep(1);
}

void event_decoder(snd_seq_t *handle, int argc, char *argv[])
{
	snd_seq_event_t *ev;
	snd_seq_port_info_t port;
	snd_seq_port_subscribe_t sub;
	fd_set in;
	int client, queue, max, err, v1, v2;
	char *ptr;

	if ((client = snd_seq_client_id(handle))<0) {
		fprintf(stderr, "Cannot determine client number: %s\n", snd_strerror(client));
		return;
	}
	printf("Client ID = %i\n", client);
	if ((queue = snd_seq_alloc_queue(handle))<0) {
		fprintf(stderr, "Cannot allocate queue: %s\n", snd_strerror(queue));
		return;
	}
	printf("Queue ID = %i\n", queue);
	if ((err = snd_seq_block_mode(handle, 0))<0)
		fprintf(stderr, "Cannot set nonblock mode: %s\n", snd_strerror(err));
	bzero(&port, sizeof(port));
	strcpy(port.name, "Input");
	port.capability = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_READ; 
	port.capability |= SND_SEQ_PORT_CAP_SUBS_WRITE; 
	if ((err = snd_seq_create_port(handle, &port)) < 0) {
		fprintf(stderr, "Cannot create input port: %s\n", snd_strerror(err));
		return;
	}
	event_decoder_start_timer(handle, queue, client, port.port);

	bzero(&sub, sizeof(sub));
	sub.sender.client = SND_SEQ_CLIENT_SYSTEM;
	sub.sender.port = SND_SEQ_PORT_SYSTEM_ANNOUNCE;
	sub.dest.client = client;
	sub.dest.port = port.port;
	sub.queue = queue;
	sub.exclusive = 0;
	sub.convert_time = 1;
	sub.realtime = 1;
	if ((err = snd_seq_subscribe_port(handle, &sub))<0) {
		fprintf(stderr, "Cannot subscribe announce port: %s\n", snd_strerror(err));
		return;
	}
	sub.sender.port = SND_SEQ_PORT_SYSTEM_TIMER;
	if ((err = snd_seq_subscribe_port(handle, &sub))<0) {
		fprintf(stderr, "Cannot subscribe timer port: %s\n", snd_strerror(err));
		return;
	}

	for (max = 0; max < argc; max++) {
		ptr = argv[max];
		if (!ptr)
			continue;
		sub.realtime = 0;
		if (tolower(*ptr) == 'r') {
			sub.realtime = 1;
			ptr++;
		}
		if (sscanf(ptr, "%i.%i", &v1, &v2) != 2) {
			fprintf(stderr, "Wrong argument '%s'...\n", argv[max]);
			return;
		}
		sub.sender.client = v1;
		sub.sender.port = v2;
		if ((err = snd_seq_subscribe_port(handle, &sub))<0) {
			fprintf(stderr, "Cannot subscribe port %i from client %i: %s\n", v2, v1, snd_strerror(err));
			return;
		}
	}
	
	while (1) {
		FD_ZERO(&in);
		FD_SET(max = snd_seq_poll_descriptor(handle), &in);
		if (select(max + 1, &in, NULL, NULL, NULL) < 0)
			break;
		do {
			if ((err = snd_seq_event_input(handle, &ev))<0)
				break;
			if (!ev)
				continue;
			decode_event(ev);
			snd_seq_free_event(ev);
		} while (err > 0);
	}
}
