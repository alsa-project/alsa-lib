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
		     const char *name, int mode);
int snd_rawmidi_close(snd_rawmidi_t *rmidi);
int snd_rawmidi_poll_descriptors_count(snd_rawmidi_t *rmidi);
int snd_rawmidi_poll_descriptors(snd_rawmidi_t *rmidi, struct pollfd *pfds, unsigned int space);
int snd_rawmidi_nonblock(snd_rawmidi_t *rmidi, int nonblock);
size_t snd_rawmidi_info_sizeof();
#define snd_rawmidi_info_alloca(ptr) do { assert(ptr); *ptr = (snd_rawmidi_info_t *) alloca(snd_rawmidi_info_sizeof()); memset(*ptr, 0, snd_rawmidi_info_sizeof()); } while (0)
int snd_rawmidi_info_malloc(snd_rawmidi_info_t **ptr);
void snd_rawmidi_info_free(snd_rawmidi_info_t *obj);
void snd_rawmidi_info_copy(snd_rawmidi_info_t *dst, const snd_rawmidi_info_t *src);
unsigned int snd_rawmidi_info_get_device(const snd_rawmidi_info_t *obj);
unsigned int snd_rawmidi_info_get_subdevice(const snd_rawmidi_info_t *obj);
snd_rawmidi_stream_t snd_rawmidi_info_get_stream(const snd_rawmidi_info_t *obj);
int snd_rawmidi_info_get_card(const snd_rawmidi_info_t *obj);
unsigned int snd_rawmidi_info_get_flags(const snd_rawmidi_info_t *obj);
const char *snd_rawmidi_info_get_id(const snd_rawmidi_info_t *obj);
const char *snd_rawmidi_info_get_name(const snd_rawmidi_info_t *obj);
const char *snd_rawmidi_info_get_subdevice_name(const snd_rawmidi_info_t *obj);
unsigned int snd_rawmidi_info_get_subdevices_count(const snd_rawmidi_info_t *obj);
unsigned int snd_rawmidi_info_get_subdevices_avail(const snd_rawmidi_info_t *obj);
void snd_rawmidi_info_set_device(snd_rawmidi_info_t *obj, unsigned int val);
void snd_rawmidi_info_set_subdevice(snd_rawmidi_info_t *obj, unsigned int val);
void snd_rawmidi_info_set_stream(snd_rawmidi_info_t *obj, snd_rawmidi_stream_t val);
int snd_rawmidi_info(snd_rawmidi_t *rmidi, snd_rawmidi_info_t * info);
size_t snd_rawmidi_params_sizeof();
#define snd_rawmidi_params_alloca(ptr) do { assert(ptr); *ptr = (snd_rawmidi_params_t *) alloca(snd_rawmidi_params_sizeof()); memset(*ptr, 0, snd_rawmidi_params_sizeof()); } while (0)
int snd_rawmidi_params_malloc(snd_rawmidi_params_t **ptr);
void snd_rawmidi_params_free(snd_rawmidi_params_t *obj);
void snd_rawmidi_params_copy(snd_rawmidi_params_t *dst, const snd_rawmidi_params_t *src);
int snd_rawmidi_params_set_buffer_size(snd_rawmidi_t *rmidi, snd_rawmidi_params_t *params, size_t val);
size_t snd_rawmidi_params_get_buffer_size(const snd_rawmidi_params_t *params);
int snd_rawmidi_params_set_avail_min(snd_rawmidi_t *rmidi, snd_rawmidi_params_t *params, size_t val);
size_t snd_rawmidi_params_get_avail_min(const snd_rawmidi_params_t *params);
int snd_rawmidi_params_set_no_active_sensing(snd_rawmidi_t *rmidi, snd_rawmidi_params_t *params, int val);
int snd_rawmidi_params_get_no_active_sensing(const snd_rawmidi_params_t *params);
int snd_rawmidi_params(snd_rawmidi_t *rmidi, snd_rawmidi_params_t * params);
int snd_rawmidi_params_current(snd_rawmidi_t *rmidi, snd_rawmidi_params_t *params);
size_t snd_rawmidi_status_sizeof();
#define snd_rawmidi_status_alloca(ptr) do { assert(ptr); *ptr = (snd_rawmidi_status_t *) alloca(snd_rawmidi_status_sizeof()); memset(*ptr, 0, snd_rawmidi_status_sizeof()); } while (0)
int snd_rawmidi_status_malloc(snd_rawmidi_status_t **ptr);
void snd_rawmidi_status_free(snd_rawmidi_status_t *obj);
void snd_rawmidi_status_copy(snd_rawmidi_status_t *dst, const snd_rawmidi_status_t *src);
void snd_rawmidi_status_get_tstamp(const snd_rawmidi_status_t *obj, snd_timestamp_t *ptr);
size_t snd_rawmidi_status_get_avail(const snd_rawmidi_status_t *obj);
size_t snd_rawmidi_status_get_avail_max(const snd_rawmidi_status_t *obj);
int snd_rawmidi_status(snd_rawmidi_t *rmidi, snd_rawmidi_status_t * status);
int snd_rawmidi_drain(snd_rawmidi_t *rmidi);
int snd_rawmidi_drop(snd_rawmidi_t *rmidi);
ssize_t snd_rawmidi_write(snd_rawmidi_t *rmidi, const void *buffer, size_t size);
ssize_t snd_rawmidi_read(snd_rawmidi_t *rmidi, void *buffer, size_t size);
const char *snd_rawmidi_name(snd_rawmidi_t *rmidi);
snd_rawmidi_type_t snd_rawmidi_type(snd_rawmidi_t *rmidi);

#ifdef __cplusplus
}
#endif

