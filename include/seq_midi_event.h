/****************************************************************************
 *                                                                          *
 *               Sequencer event <-> MIDI byte stream coder		    *
 *                                                                          *
 ****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_midi_event snd_midi_event_t;

int snd_midi_event_new(int bufsize, snd_midi_event_t **rdev);
int snd_midi_event_resize_buffer(snd_midi_event_t *dev, int bufsize);
void snd_midi_event_free(snd_midi_event_t *dev);
void snd_midi_event_init(snd_midi_event_t *dev);
void snd_midi_event_reset_encode(snd_midi_event_t *dev);
void snd_midi_event_reset_decode(snd_midi_event_t *dev);
/* encode from byte stream - return number of written bytes if success */
long snd_midi_event_encode(snd_midi_event_t *dev, unsigned char *buf, long count, snd_seq_event_t *ev);
int snd_midi_event_encode_byte(snd_midi_event_t *dev, int c, snd_seq_event_t *ev);
/* decode from event to bytes - return number of written bytes if success */
long snd_midi_event_decode(snd_midi_event_t *dev, unsigned char *buf, long count, snd_seq_event_t *ev);

#ifdef __cplusplus
}
#endif

