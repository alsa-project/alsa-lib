/****************************************************************************
 *                                                                          *
 *                              rawmidi.h                                   *
 *                          RawMIDI interface                               *
 *                                                                          *
 ****************************************************************************/

typedef struct _snd_rawmidi_info snd_rawmidi_info_t;
typedef struct _snd_rawmidi_params snd_rawmidi_params_t;
typedef struct _snd_rawmidi_status snd_rawmidi_status_t;

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_rawmidi_stream *snd_rawmidi_stream_t;
#else
typedef enum sndrv_rawmidi_stream snd_rawmidi_stream_t;
#endif

#define SND_RAWMIDI_STREAM_OUTPUT ((snd_rawmidi_stream_t) SNDRV_RAWMIDI_STREAM_OUTPUT)
#define SND_RAWMIDI_STREAM_INPUT ((snd_rawmidi_stream_t) SNDRV_RAWMIDI_STREAM_INPUT)
#define SND_RAWMIDI_STREAM_LAST ((snd_rawmidi_stream_t) SNDRV_RAWMIDI_STREAM_LAST)

#define SND_RAWMIDI_APPEND	1
#define SND_RAWMIDI_NONBLOCK	2

typedef struct _snd_rawmidi snd_rawmidi_t;

enum _snd_rawmidi_type {
	SND_RAWMIDI_TYPE_HW,
	SND_RAWMIDI_TYPE_SHM,
	SND_RAWMIDI_TYPE_INET,
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_rawmidi_type *snd_rawmidi_type_t;
#else
typedef enum _snd_rawmidi_type snd_rawmidi_type_t;
#endif

#define	SND_RAWMIDI_TYPE_HW ((snd_rawmidi_type_t) SND_RAWMIDI_TYPE_HW)
#define	SND_RAWMIDI_TYPE_SHM ((snd_rawmidi_type_t) SND_RAWMIDI_TYPE_SHM)
#define	SND_RAWMIDI_TYPE_INET ((snd_rawmidi_type_t) SND_RAWMIDI_TYPE_INET)

#ifdef __cplusplus
extern "C" {
#endif

int snd_rawmidi_open(snd_rawmidi_t **in_rmidi, snd_rawmidi_t **out_rmidi,
		     char *name, int mode);
int snd_rawmidi_close(snd_rawmidi_t *rmidi);
int snd_rawmidi_poll_descriptor(snd_rawmidi_t *rmidi);
int snd_rawmidi_nonblock(snd_rawmidi_t *rmidi, int nonblock);
int snd_rawmidi_info(snd_rawmidi_t *rmidi, snd_rawmidi_info_t * info);
int snd_rawmidi_params(snd_rawmidi_t *rmidi, snd_rawmidi_params_t * params);
int snd_rawmidi_status(snd_rawmidi_t *rmidi, snd_rawmidi_status_t * status);
int snd_rawmidi_drain(snd_rawmidi_t *rmidi);
int snd_rawmidi_drop(snd_rawmidi_t *rmidi);
ssize_t snd_rawmidi_write(snd_rawmidi_t *rmidi, const void *buffer, size_t size);
ssize_t snd_rawmidi_read(snd_rawmidi_t *rmidi, void *buffer, size_t size);

int snd_rawmidi_params_current(snd_rawmidi_t *rmidi, snd_rawmidi_params_t *params);

#ifdef __cplusplus
}
#endif

