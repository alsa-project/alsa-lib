/*
 *  Simple event sender
 */

void event_sender_start_timer(void *handle, int client, int queue)
{
	int err;
	snd_seq_event_t ev;
	
	bzero(&ev, sizeof(ev));
	ev.source.queue = queue;
	ev.source.client = client;
	ev.source.port = 0;
	ev.dest.queue = queue;
	ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
	ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_REL;
	ev.type = SND_SEQ_EVENT_START;
	if ((err = snd_seq_event_output(handle, &ev))<0)
		fprintf(stderr, "Timer event output error: %s\n", snd_strerror(err));
	while (snd_seq_flush_output(handle)>0)
		sleep(1);
}

void event_sender_filter(void *handle)
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

void send_event(void *handle, int queue, int client, int port,
                snd_seq_port_subscribe_t *sub, int *time)
{
	int err;
	snd_seq_event_t ev;
	
	bzero(&ev, sizeof(ev));
	ev.source.queue = ev.dest.queue = queue;
	ev.source.client = ev.dest.client = client;
	ev.source.port = ev.dest.port = port;
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	ev.time.real.tv_sec = *time; (*time)++;
	ev.type = SND_SEQ_EVENT_ECHO;
	if ((err = snd_seq_event_output(handle, &ev))<0)
		fprintf(stderr, "Event output error: %s\n", snd_strerror(err));
	ev.dest.client = sub->dest.client;
	ev.dest.port = sub->dest.port;
	ev.dest.channel = 0;
	ev.type = SND_SEQ_EVENT_NOTE;
	ev.data.note.note = 64 + (queue*2);
	ev.data.note.velocity = 127;
	ev.data.note.duration = 500;	/* 0.5sec */
	if ((err = snd_seq_event_output(handle, &ev))<0)
		fprintf(stderr, "Event output error: %s\n", snd_strerror(err));
	if ((err = snd_seq_flush_output(handle))<0)
		fprintf(stderr, "Event flush error: %s\n", snd_strerror(err));
}

void event_sender(void *handle, int argc, char *argv[])
{
	snd_seq_event_t *ev;
	snd_seq_port_info_t port;
	snd_seq_port_subscribe_t sub;
	fd_set out, in;
	int client, queue, max, err, v1, v2, time = 0, first;
	char *ptr;

	if (argc != 1) {
		fprintf(stderr, "Invalid destonation...\n");
		return;
	}

	if ((client = snd_seq_client_id(handle))<0) {
		fprintf(stderr, "Cannot determine client number: %s\n", snd_strerror(client));
		return;
	}
	printf("Client ID = %i\n", client);
	if ((queue = snd_seq_alloc_queue(handle, NULL))<0) {
		fprintf(stderr, "Cannot allocate queue: %s\n", snd_strerror(queue));
		return;
	}
	printf("Queue ID = %i\n", queue);
	event_sender_filter(handle);
	if ((err = snd_seq_block_mode(handle, 0))<0)
		fprintf(stderr, "Cannot set nonblock mode: %s\n", snd_strerror(err));
	event_sender_start_timer(handle, client, queue);
	bzero(&port, sizeof(port));
	strcpy(port.name, "Output");
	if ((err = snd_seq_create_port(handle, &port)) < 0) {
		fprintf(stderr, "Cannot create output port: %s\n", snd_strerror(err));
		return;
	}

	bzero(&sub, sizeof(sub));
	sub.sender.queue = queue;
	sub.sender.client = client;
	sub.sender.port = port.port;
	sub.dest.queue = queue;
	sub.exclusive = 0;

	for (max = 0; max < argc; max++) {
		ptr = argv[max];
		if (!ptr)
			continue;
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
	
	first = 1;
	while (1) {
		FD_ZERO(&out);
		FD_ZERO(&in);
		FD_SET(max = snd_seq_file_descriptor(handle), &out);
		FD_SET(max = snd_seq_file_descriptor(handle), &in);
		if (select(max + 1, &in, &out, NULL, NULL) < 0)
			break;
		if (FD_ISSET(max, &out)) {
			if (first) {
				send_event(handle, queue, client, port.port, &sub, &time);
				first = 0;
			}
		}
		if (FD_ISSET(max, &in)) {
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
