/****************************************************************************
 *                                                                          *
 *               Sequencer event <-> MIDI byte stream coder		    *
 *                                                                          *
 ****************************************************************************/

typedef struct snd_midi_event snd_midi_event_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_midi_event_new(size_t bufsize, snd_midi_event_t **rdev);
int snd_midi_event_resize_buffer(snd_midi_event_t *dev, size_t bufsize);
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

