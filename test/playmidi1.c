/*
 *   MIDI file player for ALSA sequencer 
 *   (type 0 only!, the library that is used doesn't support merging of tracks)
 *
 *   Copyright (c) 1998 by Frank van de Pol <F.K.W.van.de.Pol@inter.nl.net>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* define this if you want to send real-time time stamps instead of midi ticks to the ALSA sequencer */
/*#define USE_REALTIME */


#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <string.h>

#include "midifile.h"		/* SMF library header */
#include "midifile.c"		/* SMF library code */

#include "sys/asoundlib.h"

//#define DEST_QUEUE_NUMBER 0
#define DEST_QUEUE_NUMBER 7
#define DEST_CLIENT_NUMBER 64
//#define DEST_CLIENT_NUMBER 72
//#define DEST_CLIENT_NUMBER 128
//#define DEST_CLIENT_NUMBER 255
//#define DEST_CLIENT_NUMBER SND_SEQ_ADDRESS_BROADCAST

FILE *F;
void* seq_handle = NULL;
int ppq = 96;

double local_secs = 0;
int local_ticks = 0;
int local_tempo = 500000;

static int dest_queue = DEST_QUEUE_NUMBER;
static int dest_client = DEST_CLIENT_NUMBER;
static int dest_port = 0;
//static int dest_port = 1;
static int source_channel = 0;
static int source_port = 0;


extern void alsa_start_timer(void);
extern void alsa_stop_timer(void);


static inline double tick2time_dbl(int tick)
{
	return local_secs + ((double) (tick - local_ticks) * (double) local_tempo * 1.0E-9 / (double) ppq);
}

#ifdef USE_REALTIME
static void tick2time(snd_seq_real_time_t * tm, int tick)
{
	double secs = tick2time_dbl(tick);

	//double secs = ((double) tick * (double) local_tempo * 1.0E-6 / (double) ppq);

	tm->tv_sec = secs;
	tm->tv_nsec = (secs - tm->tv_sec) * 1.0E9;

	//printf("secs = %lf  = %d.%09d\n", secs, tm->tv_sec, tm->tv_nsec);
}

#endif

/* sleep until sequencer has reached specified timestamp, to guard that we play too much events ahead */
void sleep_seq(int tick)
{
#if 0
  snd_seq_queue_info_t queue_info;
  const int COUNT_MAX = 500;
  const int COUNT_MIN = 50;
  static int count = 0;
  count++;
  if (count >= COUNT_MAX)
    {
      while (snd_seq_flush_output(seq_handle) > COUNT_MIN)
	sched_yield ();
      count = 0;
    }
#endif
}


/* write event to ALSA sequencer */
void write_ev_im(snd_seq_event_t * ev)
{
	int written;

	ev->flags &= ~SND_SEQ_EVENT_LENGTH_MASK;
	ev->flags |= SND_SEQ_EVENT_LENGTH_FIXED;
	

	written = -ENOMEM;
	while (written<0) {
	  written = snd_seq_event_output (seq_handle, ev);
		if (written<0) {
		  printf("written = %i (%s)\n", written, snd_strerror(written));
		  sleep(1);
		}
	}
}

/* write event to ALSA sequencer */
void write_ev(snd_seq_event_t * ev)
{
  sleep_seq(ev->time.tick-ppq);
  write_ev_im(ev);
}

/* write variable length event to ALSA sequencer */
void write_ev_var(snd_seq_event_t * ev, int len, void *ptr)
{
	int written;

	sleep_seq(ev->time.tick+ppq);

	ev->flags &= ~SND_SEQ_EVENT_LENGTH_MASK;
	ev->flags |= SND_SEQ_EVENT_LENGTH_VARIABLE;
	ev->data.ext.len = len;
	ev->data.ext.ptr = ptr;

	written = -ENOMEM;
	while (written<0) {
	  written = snd_seq_event_output (seq_handle, ev);
	        if (written<0) {
	          printf("written = %i (%s)\n", written, snd_strerror(written));
	          sleep(1);
		}
	}
}


int mygetc(void)
{
	return (getc(F));
}

void mytext(int type, int leng, char *msg)
{
	char *p;
	char *ep = msg + leng;

	for (p = msg; p < ep; p++)
		putchar(isprint(*p) ? *p : '?');
	putchar('\n');
}

void do_header(int format, int ntracks, int division)
{
	printf("smf format %d, %d tracks, %d ppq\n", format, ntracks, division);
	ppq = division;

	if ((format != 0) || (ntracks != 1)) {
		printf("This player does not support merging of tracks.\n");
		alsa_stop_timer();
		exit(1);
	}
	/* set ppq */
	{
		snd_seq_queue_info_t queue_info;

		queue_info.queue = 1;	/* queue we're using */
		queue_info.ppq = ppq;
		//queue_info.tempo = -1;	/* don't change */
		queue_info.tempo = 500000;	/* don't change */
		if (snd_seq_set_queue_info (seq_handle, dest_queue, &queue_info) < 0) {
			perror("ioctl");
			exit(1);
		}
		printf("ALSA Timer updated, PPQ = %d\n", queue_info.ppq);
	}

	/* start playing... */
	alsa_start_timer();
}

void do_tempo(int us)
{
	double bpm;
	snd_seq_event_t ev;

	bpm = 60.0E6 / (double) us;

	printf("tempo = %d us/beat\n", us);
	printf("tempo = %.2f bpm\n", bpm);

	/* store new tempo and timestamp of tempo change */
	local_secs = tick2time_dbl(Mf_currtime);
	local_ticks = Mf_currtime;
	local_tempo = us;


	/* and send tempo change event to the sequencer.... */
	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	//ev.dest.client = dest_client;	/* broadcast */
	ev.dest.client = 255;	/* broadcast */
	ev.dest.port = 0;
	ev.dest.channel = 0;	/* don't care */

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_TEMPO;
	ev.data.control.value = us;

	write_ev_im(&ev);

}

void do_noteon(int chan, int pitch, int vol)
{
	snd_seq_event_t ev;

	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	ev.dest.client = dest_client;
	ev.dest.port = dest_port;
	ev.dest.channel = chan;

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_NOTEON;
	ev.data.note.note = pitch;
	ev.data.note.velocity = vol;

	write_ev(&ev);

}


void do_noteoff(int chan, int pitch, int vol)
{
	snd_seq_event_t ev;

	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	ev.dest.client = dest_client;
	ev.dest.port = dest_port;
	ev.dest.channel = chan;

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_NOTEOFF;
	ev.data.note.note = pitch;
	ev.data.note.velocity = vol;

	write_ev(&ev);
}


void do_program(int chan, int program)
{
	snd_seq_event_t ev;

	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	ev.dest.client = dest_client;
	ev.dest.port = dest_port;
	ev.dest.channel = chan;

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_PGMCHANGE;
	ev.data.control.value = program;

	write_ev_im(&ev);
}


void do_parameter(int chan, int control, int value)
{
	snd_seq_event_t ev;

	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	ev.dest.client = dest_client;
	ev.dest.port = dest_port;
	ev.dest.channel = chan;

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_CONTROLLER;
	ev.data.control.param = control;
	ev.data.control.value = value;

	write_ev(&ev);
}


void do_pitchbend(int chan, int lsb, int msb)
{				/* !@#$% lsb & msb are in wrong order in docs */
	snd_seq_event_t ev;

	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	ev.dest.client = dest_client;
	ev.dest.port = dest_port;
	ev.dest.channel = chan;

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_PITCHBEND;
	ev.data.control.value = (lsb + (msb << 7)) - 8192;

	write_ev(&ev);
}

void do_pressure(int chan, int pitch, int pressure)
{
	snd_seq_event_t ev;

	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	ev.dest.client = dest_client;
	ev.dest.port = dest_port;
	ev.dest.channel = chan;

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_KEYPRESS;
	ev.data.control.param = pitch;
	ev.data.control.value = pressure;

	write_ev(&ev);
}

void do_chanpressure(int chan, int pressure)
{
	snd_seq_event_t ev;

	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	ev.dest.client = dest_client;
	ev.dest.port = dest_port;
	ev.dest.channel = chan;

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_CHANPRESS;
	ev.data.control.value = pressure;

	write_ev(&ev);
}

void do_sysex(int len, char *msg)
{
	snd_seq_event_t ev;

#if 0
	int c;

	printf("Sysex, len=%d\n", len);
	for (c = 0; c < len; c++) {
		printf("    %3d : %02x\n", c, (unsigned char) msg[c]);
	}
#endif


	ev.source.port = dest_port;
	ev.source.channel = source_channel;

	ev.dest.queue = dest_queue;
	ev.dest.client = dest_client;
	ev.dest.port = dest_port;
	ev.dest.channel = 0;	/* don't care */

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_SYSEX;

	write_ev_var(&ev, len, msg);
}

/**/
void alsa_sync ()
{
  //dump buffer
  int left = snd_seq_flush_output (seq_handle);
  printf ("alsa_sync syncing\n");
  while (left > 0)
    {
      sched_yield ();
      left = snd_seq_flush_output (seq_handle);
      if (left < 0)
	{
	  printf ("alsa_sync error!:%s\n", snd_strerror (left));
	  return;
	}
    }

  
  for (;;)
    {
      snd_seq_queue_info_t queue_info;
      int tmp = snd_seq_get_queue_info (seq_handle, dest_queue, &queue_info);
      if (tmp < 0)
        {
          printf ("AlsaClient::sync snd_seq_get_queue_info:%s\n",
		  snd_strerror (tmp));
          break;
	}
      
      if (Mf_currtime < queue_info.tick)
	break;
      sched_yield ();
    }
}


/* start timer */
void alsa_start_timer(void)
{
	snd_seq_event_t ev;

	ev.source.port = SND_SEQ_PORT_SYSTEM_TIMER;
	ev.source.channel = 0;

	ev.dest.queue = dest_queue;
	//ev.dest.client = 0;	/* system */
	ev.dest.client = SND_SEQ_CLIENT_SYSTEM;	/* system */
	//ev.dest.client = dest_client;	/* broadcast */
	ev.dest.client = 255;	/* broadcast */
	ev.dest.port = 0;	/* timer */
	ev.dest.channel = 0;	/* don't care */

	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_REL;
	//ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_REL;
	ev.time.real.tv_sec = 0;
	ev.time.real.tv_nsec = 0;

	ev.type = SND_SEQ_EVENT_START;

	write_ev_im(&ev);
	usleep(0.1E6);
}

/* stop timer */
void alsa_stop_timer(void)
{

	snd_seq_event_t ev;

	ev.source.port = 0;
	ev.source.channel = 0;

	ev.dest.queue = dest_queue;
	ev.dest.client = 0;	/* system */
	ev.dest.client = dest_client;	/* broadcast */
	ev.dest.port = 0;	/* timer */
	ev.dest.channel = 0;	/* don't care */

#ifdef USE_REALTIME
	ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
	tick2time(&ev.time.real, Mf_currtime);
#else
	ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_ABS;
	ev.time.tick = Mf_currtime;
#endif

	ev.type = SND_SEQ_EVENT_STOP;

	write_ev_im(&ev);
}

int main(int argc, char *argv[])
{
	snd_seq_client_info_t inf;
	snd_seq_port_info_t src_port_info;
	snd_seq_queue_client_t queue_info;
	snd_seq_port_subscribe_t subscribe;
	int tmp;

#ifdef USE_REALTIME
	printf("ALSA MIDI Player, feeding events to real-time queue\n");
#else
	printf("ALSA MIDI Player, feeding events to song queue\n");
#endif

	/* open sequencer device */
	//tmp = snd_seq_open (&seq_handle, SND_SEQ_OPEN_OUT);
	tmp = snd_seq_open (&seq_handle, SND_SEQ_OPEN);
	if (tmp < 0) {
		perror("open /dev/snd/seq");
		exit(1);
	}
	
	tmp = snd_seq_block_mode (seq_handle, 0);
	if (tmp < 0)
	  {
	    perror ("block_mode");
	    exit (1);
	  }
	
		
	/* set name */
	memset(&inf, 0, sizeof(snd_seq_client_info_t));
	strcpy(inf.name, "MIDI file player");
	if (snd_seq_set_client_info (seq_handle, &inf) < 0) {
		perror("ioctl");
		exit(1);
	}

	//create port
	memset (&src_port_info, 0, sizeof (snd_seq_port_info_t));
	src_port_info.capability = SND_SEQ_PORT_CAP_OUT | SND_SEQ_PORT_CAP_SUBSCRIPTION;
	src_port_info.type = SND_SEQ_PORT_TYPE_MIDI_GENERIC;
	src_port_info.midi_channels = 16;
	src_port_info.synth_voices = 0;
	src_port_info.use = 0;
	src_port_info.kernel = NULL;
	tmp = snd_seq_create_port (seq_handle, &src_port_info);
	if (tmp < 0)
	  {
	    perror ("creat port");
	    exit (1);
	  }
	
	//setup queue
	queue_info.used = 1;
	//queue_info.low = 100;//???
	//queue_info.low = SND_SEQ_MAX_EVENTS-1;//???
	//queue_info.low = 500-1;//???
	//queue_info.low = 1;//???
	queue_info.low = 0;
	//queue_info.low = 0x7ffffff;//???
	//queue_info.high = 0x7ffffff;//???
	queue_info.high = 500-100;//???
	//queue_info.high = 50;//???
	//queue_info.high = 0;//???
	//queue_info.high = 1;//???
	tmp = snd_seq_set_queue_client (seq_handle, dest_queue,
					&queue_info);
	if (tmp < 0)
	  {
	    perror ("queue_client");
	    exit (1);
	  }
#if (DEST_CLIENT_NUMBER == 64) || (DEST_CLIENT_NUMBER == 72)
	//setup subscriber
	printf ("debug subscribe src_port_info.client=%d\n",
		src_port_info.client);
	subscribe.sender.client = snd_seq_client_id (seq_handle);
	subscribe.sender.queue = dest_queue;
	subscribe.sender.port = src_port_info.port;
	subscribe.dest.client = dest_client;
	subscribe.dest.port = dest_port;
	subscribe.dest.queue = dest_queue;
	subscribe.realtime = 1;
	subscribe.exclusive = 0;
	tmp = snd_seq_subscribe_port (seq_handle, &subscribe);
	if (tmp < 0)
	  {
	    perror ("subscribe");
	    exit (1);
	  }
#endif
	
	if (argc > 1)
		F = fopen(argv[1], "r");
	else
		F = stdin;

	Mf_header = do_header;
	Mf_tempo = do_tempo;
	Mf_getc = mygetc;
	Mf_text = mytext;

	Mf_noteon = do_noteon;
	Mf_noteoff = do_noteoff;
	Mf_program = do_program;
	Mf_parameter = do_parameter;
	Mf_pitchbend = do_pitchbend;
	Mf_pressure = do_pressure;
	Mf_chanpressure = do_chanpressure;
	Mf_sysex = do_sysex;


	/* stop timer in case it was left running by a previous client */
	{
		snd_seq_event_t ev;

		ev.source.port = 0;
		ev.source.channel = 0;

		ev.dest.queue = dest_queue;
		ev.dest.client = 0;	/* system */
		//ev.dest.client = dest_client;	/* broadcast */
		ev.dest.client = 255;	/* broadcast */
		ev.dest.port = 0;	/* timer */
		ev.dest.channel = 0;	/* don't care */

		ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_REL;
		//ev.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_TIME_MODE_REL;
		ev.time.real.tv_sec = 0;
		ev.time.real.tv_nsec = 0;
		
		//ev.type = SND_SEQ_EVENT_STOP;
		ev.type = SND_SEQ_EVENT_START;

		write_ev(&ev);
	}
	alsa_start_timer ();
	
	/* go.. go.. go.. */
	mfread();

	alsa_sync ();
	alsa_stop_timer();

	snd_seq_close (seq_handle);

	printf("Stopping at %f s,  tick %f\n",
	       tick2time_dbl(Mf_currtime + 1), (double) (Mf_currtime + 1));


	exit(0);
}
