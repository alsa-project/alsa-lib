/****************************************************************************
 *                                                                          *
 *                      Sequencer Middle Level                              *
 *                                                                          *
 ****************************************************************************/

/**
 *  \defgroup SeqMiddle Sequencer Middle Level Interface
 *  Sequencer Middle Level Interface
 *  \ingroup Sequencer
 *  \{
 */

#ifdef __cplusplus
extern "C" {
#endif

/* initialize event record */
void snd_seq_ev_clear(snd_seq_event_t *ev);

/* set destination - following three macros are exclusive */
  /* explicit destination */
void snd_seq_ev_set_dest(snd_seq_event_t *ev, int client, int port);
  /* to subscribers */
void snd_seq_ev_set_subs(snd_seq_event_t *ev);
  /* broadcast to all clients/ports */
void snd_seq_ev_set_broadcast(snd_seq_event_t *ev);

/* set source port */
void snd_seq_ev_set_source(snd_seq_event_t *ev, int port);

/* set scheduling - following three macros are exclusive */
  /* direct event passing without enqueued */
void snd_seq_ev_set_direct(snd_seq_event_t *ev);
  /* scheduled on tick-queue */
void snd_seq_ev_schedule_tick(snd_seq_event_t *ev, int q, int relative,
			      snd_seq_tick_time_t tick);
  /* scheduled on real-time-queue */
void snd_seq_ev_schedule_real(snd_seq_event_t *ev, int q, int relative,
			      snd_seq_real_time_t *_time);

/* set event priority (optional) */
void snd_seq_ev_set_priority(snd_seq_event_t *ev, int high_prior);

/* set event data type - following three macros are exclusive */
  /* fixed size event */
void snd_seq_ev_set_fixed(snd_seq_event_t *ev);
  /* variable size event */
void snd_seq_ev_set_variable(snd_seq_event_t *ev, int len, void *ptr);
  /* variable size event - user memory space */
void snd_seq_ev_set_varusr(snd_seq_event_t *ev, int len, void *ptr);

/* set queue control event data */
/* destination is overwritten to Timer port (0:0) */
int snd_seq_ev_set_queue_start(snd_seq_event_t *ev, int q);
int snd_seq_ev_set_queue_stop(snd_seq_event_t *ev, int q);
int snd_seq_ev_set_queue_continue(snd_seq_event_t *ev, int q);
int snd_seq_ev_set_queue_tempo(snd_seq_event_t *ev, int q, int tempo);
int snd_seq_ev_set_queue_control(snd_seq_event_t *ev, int type, int q, int value);
int snd_seq_ev_set_queue_pos_real(snd_seq_event_t *ev, int q, snd_seq_real_time_t *rtime);
int snd_seq_ev_set_queue_pos_tick(snd_seq_event_t *ev, int q, snd_seq_tick_time_t tick);

/*
 * use/unuse a queue
 */
int snd_seq_use_queue(snd_seq_t *seq, int q, int use);

/* set and send a queue control event:
 * to send at scheduled time, set the schedule in ev.
 * if ev is NULL, event is sent immediately (to output queue).
 * Note: to send actually to driver, you need to call snd_seq_flush_event()
 *       apropriately.
 */
int snd_seq_control_queue(snd_seq_t *seq, int q, int type, int value, snd_seq_event_t *ev);
int snd_seq_start_queue(snd_seq_t *seq, int q, snd_seq_event_t *ev);
int snd_seq_stop_queue(snd_seq_t *seq, int q, snd_seq_event_t *ev);
int snd_seq_continue_queue(snd_seq_t *seq, int q, snd_seq_event_t *ev);
int snd_seq_change_queue_tempo(snd_seq_t *seq, int q, int tempo, snd_seq_event_t *ev);
int snd_seq_setpos_queue(snd_seq_t *seq, int q, snd_seq_timestamp_t *rtime, snd_seq_event_t *ev);

/* create a port - simple version - return the port number */
int snd_seq_create_simple_port(snd_seq_t *seq, const char *name,
			       unsigned int caps, unsigned int type);
/* delete the port */
int snd_seq_delete_simple_port(snd_seq_t *seq, int port);

/* simple subscription between this port and another port
   (w/o exclusive & time conversion)
   */
int snd_seq_connect_from(snd_seq_t *seq, int my_port, int src_client, int src_port);
int snd_seq_connect_to(snd_seq_t *seq, int my_port, int dest_client, int dest_port);
int snd_seq_disconnect_from(snd_seq_t *seq, int my_port, int src_client, int src_port);
int snd_seq_disconnect_to(snd_seq_t *seq, int my_port, int dest_client, int dest_port);

/*
 * set client information
 */
int snd_seq_set_client_name(snd_seq_t *seq, const char *name);
int snd_seq_set_client_group(snd_seq_t *seq, const char *name);
int snd_seq_set_client_filter(snd_seq_t *seq, unsigned int filter);
int snd_seq_set_client_event_filter(snd_seq_t *seq, int event_type);
int snd_seq_set_client_pool_output(snd_seq_t *seq, int size);
int snd_seq_set_client_pool_output_room(snd_seq_t *seq, int size);
int snd_seq_set_client_pool_input(snd_seq_t *seq, int size);

/*
 * reset client input/output pool
 */
int snd_seq_reset_pool_output(snd_seq_t *seq);
int snd_seq_reset_pool_input(snd_seq_t *seq);

/*
 * equivalent macros
 */
#define snd_seq_ev_clear(ev)	memset(ev, 0, sizeof(snd_seq_event_t))
#define snd_seq_ev_set_dest(ev,c,p) \
	((ev)->dest.client = (c), (ev)->dest.port = (p))
#define snd_seq_ev_set_subs(ev) \
	((ev)->dest.client = SND_SEQ_ADDRESS_SUBSCRIBERS,\
	 (ev)->dest.port = SND_SEQ_ADDRESS_UNKNOWN)
#define snd_seq_ev_set_broadcast(ev) \
	((ev)->dest.client = SND_SEQ_ADDRESS_BROADCAST,\
	 (ev)->dest.port = SND_SEQ_ADDRESS_BROADCAST)
#define snd_seq_ev_set_source(ev,p) ((ev)->source.port = (p))

/*
 * queue controls
 */
#define snd_seq_ev_set_queue_control(ev,t,q,val) \
	((ev)->type = (t),\
	 snd_seq_ev_set_dest(ev, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_TIMER),\
	 (ev)->data.queue.queue = (q),\
	 (ev)->data.queue.param.value = (val))
#define snd_seq_ev_set_queue_start(ev,q) \
	snd_seq_ev_set_queue_control(ev,SND_SEQ_EVENT_START,q,0)
#define snd_seq_ev_set_queue_stop(ev,q) \
	snd_seq_ev_set_queue_control(ev,SND_SEQ_EVENT_STOP,q,0)
#define snd_seq_ev_set_queue_continue(ev,q) \
	snd_seq_ev_set_queue_control(ev,SND_SEQ_EVENT_CONTINUE,q,0)
#define snd_seq_ev_set_queue_tempo(ev,q,val) \
	snd_seq_ev_set_queue_control(ev,SND_SEQ_EVENT_TEMPO,q,val)
#define snd_seq_ev_set_queue_pos_real(ev,q,rtime) \
	((ev)->type = SND_SEQ_EVENT_SETPOS_TIME,\
	 snd_seq_ev_set_dest(ev, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_TIMER),\
	 (ev)->data.queue.queue = (q),\
	 (ev)->data.queue.param.time.time = *(rtime))
#define snd_seq_ev_set_queue_pos_tick(ev,q,ttime) \
	((ev)->type = SND_SEQ_EVENT_SETPOS_TICK,\
	 snd_seq_ev_set_dest(ev, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_TIMER),\
	 (ev)->data.queue.queue = (q),\
	 (ev)->data.queue.param.time.tick = (ttime))

#define snd_seq_start_queue(seq,q,ev) \
	snd_seq_control_queue(seq, q, SND_SEQ_EVENT_START, 0, ev)
#define snd_seq_stop_queue(seq,q,ev) \
	snd_seq_control_queue(seq, q, SND_SEQ_EVENT_STOP, 0, ev)
#define snd_seq_continue_queue(seq,q,ev) \
	snd_seq_control_queue(seq, q, SND_SEQ_EVENT_CONTINUE, 0, ev)
#define snd_seq_change_queue_tempo(seq,q,tempo,ev) \
	snd_seq_control_queue(seq, q, SND_SEQ_EVENT_TEMPO, tempo, ev)

/*
 * macros to set standard event data
 */
#define snd_seq_ev_set_note(ev,ch,key,vel,dur) \
	((ev)->type = SND_SEQ_EVENT_NOTE,\
	 snd_seq_ev_set_fixed(ev),\
	 (ev)->data.note.channel = (ch),\
	 (ev)->data.note.note = (key),\
	 (ev)->data.note.velocity = (vel),\
	 (ev)->data.note.dulation = (dur))
#define snd_seq_ev_set_noteon(ev,ch,key,vel) \
	((ev)->type = SND_SEQ_EVENT_NOTEON,\
	 snd_seq_ev_set_fixed(ev),\
	 (ev)->data.note.channel = (ch),\
	 (ev)->data.note.note = (key),\
	 (ev)->data.note.velocity = (vel))
#define snd_seq_ev_set_noteoff(ev,ch,key,vel) \
	((ev)->type = SND_SEQ_EVENT_NOTEOFF,\
	 snd_seq_ev_set_fixed(ev),\
	 (ev)->data.note.channel = (ch),\
	 (ev)->data.note.note = (key),\
	 (ev)->data.note.velocity = (vel))
#define snd_seq_ev_set_keypress(ev,ch,key,vel) \
	((ev)->type = SND_SEQ_EVENT_KEYPRESS,\
	 snd_seq_ev_set_fixed(ev),\
	 (ev)->data.note.channel = (ch),\
	 (ev)->data.note.note = (key),\
	 (ev)->data.note.velocity = (vel))
#define snd_seq_ev_set_controller(ev,ch,cc,val) \
	((ev)->type = SND_SEQ_EVENT_CONTROLLER,\
	 snd_seq_ev_set_fixed(ev),\
	 (ev)->data.control.channel = (ch),\
	 (ev)->data.control.param = (cc),\
	 (ev)->data.control.value = (val))
#define snd_seq_ev_set_pgmchange(ev,ch,val) \
	((ev)->type = SND_SEQ_EVENT_PGMCHANGE,\
	 snd_seq_ev_set_fixed(ev),\
	 (ev)->data.control.channel = (ch),\
	 (ev)->data.control.value = (val))
#define snd_seq_ev_set_pitchbend(ev,ch,val) \
	((ev)->type = SND_SEQ_EVENT_PITCHBEND,\
	 snd_seq_ev_set_fixed(ev),\
	 (ev)->data.control.channel = (ch),\
	 (ev)->data.control.value = (val))
#define snd_seq_ev_set_chanpress(ev,ch,val) \
	((ev)->type = SND_SEQ_EVENT_CHANPRESS,\
	 snd_seq_ev_set_fixed(ev),\
	 (ev)->data.control.channel = (ch),\
	 (ev)->data.control.value = (val))
#define snd_seq_ev_set_sysex(ev,datalen,dataptr) \
	((ev)->type = SND_SEQ_EVENT_SYSEX,\
	 snd_seq_ev_set_variable(ev, datalen, dataptr))

/* etc. etc... */


#ifdef __cplusplus
}
#endif

/** \} */

