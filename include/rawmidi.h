/****************************************************************************
 *                                                                          *
 *                              rawmidi.h                                   *
 *                          RawMIDI interface                               *
 *                                                                          *
 ****************************************************************************/

#define SND_RAWMIDI_OPEN_OUTPUT		(O_WRONLY)
#define SND_RAWMIDI_OPEN_INPUT		(O_RDONLY)
#define SND_RAWMIDI_OPEN_DUPLEX		(O_RDWR)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_rawmidi snd_rawmidi_t;

int snd_rawmidi_open(snd_rawmidi_t **handle, int card, int device, int mode);
int snd_rawmidi_close(snd_rawmidi_t *handle);
int snd_rawmidi_file_descriptor(snd_rawmidi_t *handle);
int snd_rawmidi_block_mode(snd_rawmidi_t *handle, int enable);
int snd_rawmidi_info(snd_rawmidi_t *handle, snd_rawmidi_info_t * info);
int snd_rawmidi_output_params(snd_rawmidi_t *handle, snd_rawmidi_output_params_t * params);
int snd_rawmidi_input_params(snd_rawmidi_t *handle, snd_rawmidi_input_params_t * params);
int snd_rawmidi_output_status(snd_rawmidi_t *handle, snd_rawmidi_output_status_t * status);
int snd_rawmidi_input_status(snd_rawmidi_t *handle, snd_rawmidi_input_status_t * status);
int snd_rawmidi_drain_output(snd_rawmidi_t *handle);
int snd_rawmidi_flush_output(snd_rawmidi_t *handle);
int snd_rawmidi_flush_input(snd_rawmidi_t *handle);
ssize_t snd_rawmidi_write(snd_rawmidi_t *handle, const void *buffer, size_t size);
ssize_t snd_rawmidi_read(snd_rawmidi_t *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

