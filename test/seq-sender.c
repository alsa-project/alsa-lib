
#ifdef USE_PCM
/*
 *  PCM timer layer
 */

int pcard = 0;
int pdevice = 0;
int pfragment_size = 4096;

void set_format(snd_pcm_t *phandle)
{
	int err;
	snd_pcm_format_t format;

	bzero(&format, sizeof(format));
	format.format = SND_PCM_SFMT_S16_LE;
	format.channels = 2;
	format.rate = 44100;
	if ((err = snd_pcm_playback_format(phandle, &format)) < 0) {
		fprintf(stderr, "Playback format error: %s\n", snd_strerror(err));
		exit(0);
	}
}

void set_fragment(snd_pcm_t *phandle)
{
	int err;
	snd_pcm_playback_params_t pparams;

	bzero(&pparams, sizeof(pparams));
	pparams.fragment_size = pfragment_size;
	pparams.fragments_max = -1;	/* maximum */
	pparams.fragments_room = 1;
	if ((err = snd_pcm_playback_params(phandle, &pparams)) < 0) {
		fprintf(stderr, "Fragment setup error: %s\n", snd_strerror(err));
		exit(0);
	}
}

void show_playback_status(snd_pcm_t *phandle)
{
	int err;
	snd_pcm_playback_status_t pstatus;

	if ((err = snd_pcm_playback_status(phandle, &pstatus)) < 0) {
		fprintf(stderr, "Playback status error: %s\n", snd_strerror(err));
		exit(0);
	}
	printf("Playback status\n");
	printf("    Real rate      : %u\n", pstatus.rate);
	printf("    Fragments      : %i\n", pstatus.fragments);
	printf("    Fragment size  : %i\n", pstatus.fragment_size);
}
#endif
/*
 *  Simple event sender
 */

void event_sender_start_timer(snd_seq_t *handle, int client, int queue, snd_pcm_t *phandle)
{
	int err;
	
#ifdef USE_PCM
	if (phandle) {
		snd_pcm_playback_info_t pinfo;
		snd_seq_queue_timer_t qtimer;

		if ((err = snd_pcm_playback_info(phandle, &pinfo)) < 0) {
			fprintf(stderr, "Playback info error: %s\n", snd_strerror(err));
			exit(0);
		}
		bzero(&qtimer, sizeof(qtimer));
		qtimer.type = SND_SEQ_TIMER_MASTER;
		/* note: last bit from subdevices specifies playback */
		/* or capture direction for the timer specification */
		qtimer.number = SND_TIMER_PCM(pcard, pdevice, pinfo.subdevice << 1);
		if ((err = snd_seq_set_queue_timer(handle, queue, &qtimer)) < 0) {
			fprintf(stderr, "Sequencer PCM timer setup failed: %s\n", snd_strerror(err));
			exit(0);
		}
	}	
#endif
	if ((err = snd_seq_start_queue(handle, queue, NULL))<0)
		fprintf(stderr, "Timer event output error: %s\n", snd_strerror(err));
	/* ugly, but working */
	while (snd_seq_flush_output(handle)>0)
		sleep(1);
}

void event_sender_filter(snd_seq_t *handle)
{
	int err;
	snd_seq_client_info_t info;

	if ((err = snd_seq_get_client_info(handle, &info)) < 0) {
		fprintf(stderr, "Unable to get client info: %s\n", snd_strerror(err));
		return;
	}
	info.filter = SND_SEQ_FILTER_USE_EVENT;
	memset(&info.event_filter, 0, sizeof(info.event_filter));
	snd_seq_set_bit(SND_SEQ_EVENT_ECHO, info.event_filter);
	if ((err = snd_seq_set_client_info(handle, &info)) < 0) {
		fprintf(stderr, "Unable to set client info: %s\n", snd_strerror(err));
		return;
	}
}

void send_event(snd_seq_t *handle, int queue, int client, int port,
                snd_seq_port_subscribe_t *sub, int *time)
{
	int err;
	snd_seq_event_t ev;
	
	bzero(&ev, sizeof(ev));
	ev.queue = queue;
	ev.source.client = ev.dest.client = client;
	ev.source.port = ev.dest.port = port;
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	ev.time.real.tv_sec = *time; (*time)++;
	ev.type = SND_SEQ_EVENT_ECHO;
	if ((err = snd_seq_event_output(handle, &ev))<0)
		fprintf(stderr, "Event output error: %s\n", snd_strerror(err));
	ev.dest.client = sub->dest.client;
	ev.dest.port = sub->dest.port;
	ev.type = SND_SEQ_EVENT_NOTE;
	ev.data.note.channel = 0;
	ev.data.note.note = 64 + (queue*2);
	ev.data.note.velocity = 127;
	ev.data.note.off_velocity = 127;
	ev.data.note.duration = 500;	/* 0.5sec */
	if ((err = snd_seq_event_output(handle, &ev))<0)
		fprintf(stderr, "Event output error: %s\n", snd_strerror(err));
	if ((err = snd_seq_flush_output(handle))<0)
		fprintf(stderr, "Event flush error: %s\n", snd_strerror(err));
}

void event_sender(snd_seq_t *handle, int argc, char *argv[])
{
	snd_seq_event_t *ev;
	snd_seq_port_info_t port;
	snd_seq_port_subscribe_t sub;
	fd_set out, in;
	int client, queue, max, err, v1, v2, time = 0, first, pcm_flag = 0;
	char *ptr;
	snd_pcm_t *phandle = NULL;
	char *pbuf = NULL;

	if (argc < 1) {
		fprintf(stderr, "Invalid destonation...\n");
		return;
	}

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
	event_sender_filter(handle);
	if ((err = snd_seq_block_mode(handle, 0))<0)
		fprintf(stderr, "Cannot set nonblock mode: %s\n", snd_strerror(err));
	bzero(&port, sizeof(port));
	port.capability = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_READ;
	strcpy(port.name, "Output");
	if ((err = snd_seq_create_port(handle, &port)) < 0) {
		fprintf(stderr, "Cannot create output port: %s\n", snd_strerror(err));
		return;
	}

	bzero(&sub, sizeof(sub));
	sub.sender.client = client;
	sub.sender.port = port.port;
	sub.exclusive = 0;

	for (max = 0; max < argc; max++) {
		ptr = argv[max];
		if (!ptr)
			continue;
		if (!strcmp(ptr, "pcm")) {
			pcm_flag = 1;
			continue;
		}
		if (sscanf(ptr, "%i.%i", &v1, &v2) != 2) {
			fprintf(stderr, "Wrong argument '%s'...\n", argv[max]);
			return;
		}
		sub.dest.client = v1;
		sub.dest.port = v2;
		if ((err = snd_seq_subscribe_port(handle, &sub))<0) {
			fprintf(stderr, "Cannot subscribe port %i from client %i: %s\n", v2, v1, snd_strerror(err));
			return;
		}
	}

	printf("Destonation client = %i, port = %i\n", sub.dest.client, sub.dest.port);

#ifdef USE_PCM
	if (pcm_flag) {
		if ((err = snd_pcm_open(&phandle, pcard, pdevice, SND_PCM_OPEN_PLAYBACK)) < 0) {
			fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
			exit(0);
		}
		set_format(phandle);
		set_fragment(phandle);	
		show_playback_status(phandle);
		pbuf = calloc(1, pfragment_size);
		if (pbuf == NULL) {
			fprintf(stderr, "No enough memory...\n");
			exit(0);
		}
	}
#endif
	event_sender_start_timer(handle, client, queue, phandle);
	
	first = 1;
	while (1) {
		FD_ZERO(&out);
		FD_ZERO(&in);
		max = snd_seq_file_descriptor(handle);
		FD_SET(snd_seq_file_descriptor(handle), &out);
		FD_SET(snd_seq_file_descriptor(handle), &in);
#ifdef USE_PCM
		if (phandle) {
			if (snd_pcm_file_descriptor(phandle) > max)
				max = snd_pcm_file_descriptor(phandle);
			FD_SET(snd_pcm_file_descriptor(phandle), &out);
		}
#endif
		if (select(max + 1, &in, &out, NULL, NULL) < 0)
			break;
#ifdef USE_PCM
		if (phandle && FD_ISSET(snd_pcm_file_descriptor(phandle), &out)) {
			if (snd_pcm_write(phandle, pbuf, pfragment_size) != pfragment_size) {
				fprintf(stderr, "Playback write error!!\n");
				exit(0);
			}
		}
#endif
		if (FD_ISSET(snd_seq_file_descriptor(handle), &out)) {
			if (first) {
				send_event(handle, queue, client, port.port, &sub, &time);
				first = 0;
			}
		}
		if (FD_ISSET(snd_seq_file_descriptor(handle), &in)) {
			do {
				if ((err = snd_seq_event_input(handle, &ev))<0)
					break;
				if (!ev)
					continue;
				if (ev->type == SND_SEQ_EVENT_ECHO)
					send_event(handle, queue, client, port.port, &sub, &time);
				decode_event(ev);
				snd_seq_free_event(ev);
			} while (err > 0);
		}
	}
}
