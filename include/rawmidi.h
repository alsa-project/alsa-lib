/****************************************************************************
 *                                                                          *
 *                              rawmidi.h                                   *
 *                          RawMIDI interface                               *
 *                                                                          *
 ****************************************************************************/

#define SND_RAWMIDI_OPEN_OUTPUT	(1<<SND_RAWMIDI_STREAM_OUTPUT)
#define SND_RAWMIDI_OPEN_INPUT	(1<<SND_RAWMIDI_STREAM_INPUT)
#define SND_RAWMIDI_OPEN_DUPLEX	(SND_RAWMIDI_OPEN_OUTPUT|SND_RAWMIDI_OPEN_INPUT)

#define SND_RAWMIDI_APPEND	1
#define SND_RAWMIDI_NONBLOCK	2

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _snd_rawmidi snd_rawmidi_t;

typedef enum _snd_rawmidi_type {
	SND_RAWMIDI_TYPE_HW,
	SND_RAWMIDI_TYPE_SHM,
	SND_RAWMIDI_TYPE_INET,
} snd_rawmidi_type_t;

int snd_rawmidi_open(snd_rawmidi_t **handle, char *name, int streams, int mode);
int snd_rawmidi_close(snd_rawmidi_t *handle);
int snd_rawmidi_card(snd_rawmidi_t *handle);
int snd_rawmidi_poll_descriptor(snd_rawmidi_t *handle);
int snd_rawmidi_nonblock(snd_rawmidi_t *handle, int nonblock);
int snd_rawmidi_info(snd_rawmidi_t *handle, snd_rawmidi_info_t * info);
int snd_rawmidi_params(snd_rawmidi_t *handle, snd_rawmidi_params_t * params);
int snd_rawmidi_status(snd_rawmidi_t *handle, snd_rawmidi_status_t * status);
int snd_rawmidi_output_drop(snd_rawmidi_t *handle);
int snd_rawmidi_output_drain(snd_rawmidi_t *handle);
int snd_rawmidi_input_drain(snd_rawmidi_t *handle);
int snd_rawmidi_drain(snd_rawmidi_t *handle, int channel);
int snd_rawmidi_drop(snd_rawmidi_t *handle, int channel);
ssize_t snd_rawmidi_write(snd_rawmidi_t *handle, const void *buffer, size_t size);
ssize_t snd_rawmidi_read(snd_rawmidi_t *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

