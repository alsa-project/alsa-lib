/****************************************************************************
 *                                                                          *
 *                              rawmidi.h                                   *
 *                          RawMIDI interface                               *
 *                                                                          *
 ****************************************************************************/

typedef struct _snd_rawmidi_info snd_rawmidi_info_t;
typedef struct _snd_rawmidi_params snd_rawmidi_params_t;
typedef struct _snd_rawmidi_status snd_rawmidi_status_t;

/* sndrv aliasing */
#ifdef SND_ENUM_TYPECHECK
typedef struct _snd_rawmidi_stream *snd_rawmidi_stream_t;
#else
typedef enum sndrv_rawmidi_stream snd_rawmidi_stream_t;
#endif

#define SND_RAWMIDI_STREAM_OUTPUT ((snd_rawmidi_stream_t) SNDRV_RAWMIDI_STREAM_OUTPUT)
#define SND_RAWMIDI_STREAM_INPUT ((snd_rawmidi_stream_t) SNDRV_RAWMIDI_STREAM_INPUT)
#define SND_RAWMIDI_INFO_OUTPUT SNDRV_RAWMIDI_INFO_OUTPUT
#define SND_RAWMIDI_INFO_INPUT SNDRV_RAWMIDI_INFO_INPUT
#define SND_RAWMIDI_INFO_DUPLEX SNDRV_RAWMIDI_INFO_DUPLEX
#define SND_RAWMIDI_OPEN_OUTPUT	(1<<SNDRV_RAWMIDI_STREAM_OUTPUT)
#define SND_RAWMIDI_OPEN_INPUT	(1<<SNDRV_RAWMIDI_STREAM_INPUT)
#define SND_RAWMIDI_OPEN_DUPLEX	(SND_RAWMIDI_OPEN_OUTPUT|SND_RAWMIDI_OPEN_INPUT)

#define SND_RAWMIDI_APPEND	1
#define SND_RAWMIDI_NONBLOCK	2

typedef struct _snd_rawmidi snd_rawmidi_t;

typedef enum _snd_rawmidi_type {
	SND_RAWMIDI_TYPE_HW,
	SND_RAWMIDI_TYPE_SHM,
	SND_RAWMIDI_TYPE_INET,
} snd_rawmidi_type_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_rawmidi_open(snd_rawmidi_t **rmidi, char *name, int streams, int mode);
int snd_rawmidi_close(snd_rawmidi_t *rmidi);
int snd_rawmidi_poll_descriptor(snd_rawmidi_t *rmidi, snd_rawmidi_stream_t stream);
int snd_rawmidi_nonblock(snd_rawmidi_t *rmidi, snd_rawmidi_stream_t stream, int nonblock);
int snd_rawmidi_info(snd_rawmidi_t *rmidi, snd_rawmidi_stream_t stream, snd_rawmidi_info_t * info);
int snd_rawmidi_params(snd_rawmidi_t *rmidi, snd_rawmidi_stream_t stream, snd_rawmidi_params_t * params);
int snd_rawmidi_status(snd_rawmidi_t *rmidi, snd_rawmidi_stream_t stream, snd_rawmidi_status_t * status);
int snd_rawmidi_drain(snd_rawmidi_t *rmidi, snd_rawmidi_stream_t stream);
int snd_rawmidi_drop(snd_rawmidi_t *rmidi, snd_rawmidi_stream_t stream);
ssize_t snd_rawmidi_write(snd_rawmidi_t *rmidi, const void *buffer, size_t size);
ssize_t snd_rawmidi_read(snd_rawmidi_t *rmidi, void *buffer, size_t size);

int snd_rawmidi_params_current(snd_rawmidi_t *rmidi, snd_rawmidi_stream_t stream, snd_rawmidi_params_t *params);

#ifdef __cplusplus
}
#endif

