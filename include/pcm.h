/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

#define SND_PCM_OPEN_PLAYBACK	(O_WRONLY)
#define SND_PCM_OPEN_CAPTURE	(O_RDONLY)
#define SND_PCM_OPEN_DUPLEX	(O_RDWR)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_pcm snd_pcm_t;
typedef struct snd_pcm_loopback snd_pcm_loopback_t;

int snd_pcm_open(snd_pcm_t **handle, int card, int device, int mode);
int snd_pcm_close(snd_pcm_t *handle);
int snd_pcm_file_descriptor(snd_pcm_t *handle);
int snd_pcm_block_mode(snd_pcm_t *handle, int enable);
int snd_pcm_info(snd_pcm_t *handle, snd_pcm_info_t * info);
int snd_pcm_playback_info(snd_pcm_t *handle, snd_pcm_playback_info_t * info);
int snd_pcm_capture_info(snd_pcm_t *handle, snd_pcm_capture_info_t * info);
int snd_pcm_playback_format(snd_pcm_t *handle, snd_pcm_format_t * format);
int snd_pcm_capture_format(snd_pcm_t *handle, snd_pcm_format_t * format);
int snd_pcm_playback_params(snd_pcm_t *handle, snd_pcm_playback_params_t * params);
int snd_pcm_capture_params(snd_pcm_t *handle, snd_pcm_capture_params_t * params);
int snd_pcm_playback_status(snd_pcm_t *handle, snd_pcm_playback_status_t * status);
int snd_pcm_capture_status(snd_pcm_t *handle, snd_pcm_capture_status_t * status);
int snd_pcm_drain_playback(snd_pcm_t *handle);
int snd_pcm_flush_playback(snd_pcm_t *handle);
int snd_pcm_flush_capture(snd_pcm_t *handle);
int snd_pcm_playback_pause(snd_pcm_t *handle, int enable);
int snd_pcm_playback_time(snd_pcm_t *handle, int enable);
int snd_pcm_capture_time(snd_pcm_t *handle, int enable);
ssize_t snd_pcm_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_read(snd_pcm_t *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#define SND_PCM_LB_OPEN_PLAYBACK	0
#define SND_PCM_LB_OPEN_CAPTURE		1

#ifdef __cplusplus
extern "C" {
#endif

int snd_pcm_loopback_open(snd_pcm_loopback_t **handle, int card, int device, int mode);
int snd_pcm_loopback_close(snd_pcm_loopback_t *handle);
int snd_pcm_loopback_file_descriptor(snd_pcm_loopback_t *handle);
int snd_pcm_loopback_block_mode(snd_pcm_loopback_t *handle, int enable);
int snd_pcm_loopback_stream_mode(snd_pcm_loopback_t *handle, int mode);
int snd_pcm_loopback_format(snd_pcm_loopback_t *handle, snd_pcm_format_t * format);
ssize_t snd_pcm_loopback_read(snd_pcm_loopback_t *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

