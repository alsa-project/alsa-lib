/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

#define SND_PCM_OPEN_PLAYBACK		0x0001
#define SND_PCM_OPEN_CAPTURE		0x0002
#define SND_PCM_OPEN_DUPLEX		0x0003
#define SND_PCM_OPEN_NONBLOCK		0x1000

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_pcm snd_pcm_t;
typedef struct snd_pcm_loopback snd_pcm_loopback_t;

int snd_pcm_open(snd_pcm_t **handle, int card, int device, int mode);
int snd_pcm_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int mode);
int snd_pcm_close(snd_pcm_t *handle);
int snd_pcm_file_descriptor(snd_pcm_t *handle, int channel);
int snd_pcm_nonblock_mode(snd_pcm_t *handle, int nonblock);
int snd_pcm_info(snd_pcm_t *handle, snd_pcm_info_t * info);
int snd_pcm_channel_info(snd_pcm_t *handle, snd_pcm_channel_info_t * info);
int snd_pcm_channel_params(snd_pcm_t *handle, snd_pcm_channel_params_t * params);
int snd_pcm_channel_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t * setup);
int snd_pcm_channel_status(snd_pcm_t *handle, snd_pcm_channel_status_t * status);
int snd_pcm_playback_prepare(snd_pcm_t *handle);
int snd_pcm_capture_prepare(snd_pcm_t *handle);
int snd_pcm_channel_prepare(snd_pcm_t *handle, int channel);
int snd_pcm_playback_go(snd_pcm_t *handle);
int snd_pcm_capture_go(snd_pcm_t *handle);
int snd_pcm_channel_go(snd_pcm_t *handle, int channel);
int snd_pcm_sync_go(snd_pcm_t *handle, snd_pcm_sync_t *sync);
int snd_pcm_playback_drain(snd_pcm_t *handle);
int snd_pcm_playback_flush(snd_pcm_t *handle);
int snd_pcm_capture_flush(snd_pcm_t *handle);
int snd_pcm_channel_flush(snd_pcm_t *handle, int channel);
int snd_pcm_playback_pause(snd_pcm_t *handle, int enable);
ssize_t snd_pcm_transfer_size(snd_pcm_t *handle, int channel);
ssize_t snd_pcm_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_read(snd_pcm_t *handle, void *buffer, size_t size);
ssize_t snd_pcm_writev(snd_pcm_t *pcm, const struct iovec *vector, int count);
ssize_t snd_pcm_readv(snd_pcm_t *pcm, const struct iovec *vector, int count);
int snd_pcm_mmap(snd_pcm_t *handle, int channel, snd_pcm_mmap_control_t **control, void **buffer);
int snd_pcm_munmap(snd_pcm_t *handle, int channel);

/* misc */

int snd_pcm_format_signed(int format);
int snd_pcm_format_unsigned(int format);
int snd_pcm_format_linear(int format);
int snd_pcm_format_little_endian(int format);
int snd_pcm_format_big_endian(int format);
int snd_pcm_format_width(int format);			/* in bits */
int snd_pcm_format_physical_width(int format);		/* in bits */
int snd_pcm_build_linear_format(int width, int unsignd, int big_endian);
ssize_t snd_pcm_format_size(int format, size_t samples);
unsigned char snd_pcm_format_silence(int format);
const char *snd_pcm_get_format_name(int format);

#ifdef __cplusplus
}
#endif

/*
 *  PCM Plug-In interface
 */

typedef struct snd_stru_pcm_plugin snd_pcm_plugin_t;
#define snd_pcm_plugin_handle_t snd_pcm_t

typedef enum {
	INIT = 0,
	PREPARE = 1,
	DRAIN = 2,
	FLUSH = 3,
} snd_pcm_plugin_action_t;

typedef struct snd_stru_pcm_plugin_voice {
	void *aptr;			/* pointer to the allocated area */
	void *addr;			/* address to voice samples */
	unsigned int offset;		/* offset to first voice in bits */
	unsigned int next;		/* offset to next voice in bits */
} snd_pcm_plugin_voice_t;

struct snd_stru_pcm_plugin {
	char *name;			/* plug-in name */
	snd_pcm_format_t src_format;	/* source format */
	snd_pcm_format_t dst_format;	/* destination format */
	int src_width;			/* sample width in bits */
	int dst_width;			/* sample width in bits */
	ssize_t (*src_samples)(snd_pcm_plugin_t *plugin, size_t dst_samples);
	ssize_t (*dst_samples)(snd_pcm_plugin_t *plugin, size_t src_samples);
	int (*src_voices)(snd_pcm_plugin_t *plugin,
			  snd_pcm_plugin_voice_t **voices,
			  size_t samples,
			  void *(*plugin_alloc)(snd_pcm_plugin_handle_t *handle, size_t size));
	int (*dst_voices)(snd_pcm_plugin_t *plugin,
			  snd_pcm_plugin_voice_t **voices,
			  size_t samples,
			  void *(*plugin_alloc)(snd_pcm_plugin_handle_t *handle, size_t size));
	ssize_t (*transfer)(snd_pcm_plugin_t *plugin,
			    const snd_pcm_plugin_voice_t *src_voices,
			    const snd_pcm_plugin_voice_t *dst_voices,
			    size_t samples);
	int (*action)(snd_pcm_plugin_t *plugin,
		      snd_pcm_plugin_action_t action,
		      unsigned long data);
	int (*parameter_set)(snd_pcm_plugin_t *plugin,
			     const char *name,
			     unsigned long value);
	int (*parameter_get)(snd_pcm_plugin_t *plugin,
			     const char *name,
			     unsigned long *value);
	snd_pcm_plugin_t *prev;
	snd_pcm_plugin_t *next;
	snd_pcm_plugin_handle_t *handle;
	void *private_data;
	void (*private_free)(snd_pcm_plugin_t *plugin, void *private_data);
	snd_pcm_plugin_voice_t *voices;
	void *extra_data;
};

snd_pcm_plugin_t *snd_pcm_plugin_build(snd_pcm_plugin_handle_t *handle,
				       const char *name,
				       snd_pcm_format_t *src_format,
				       snd_pcm_format_t *dst_format,
				       int extra);
int snd_pcm_plugin_free(snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_clear(snd_pcm_t *handle, int channel);
int snd_pcm_plugin_insert(snd_pcm_t *handle, int channel, snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_append(snd_pcm_t *handle, int channel, snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_remove_to(snd_pcm_t *handle, int channel, snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_remove_first(snd_pcm_t *handle, int channel);
snd_pcm_plugin_t *snd_pcm_plugin_first(snd_pcm_t *handle, int channel);
snd_pcm_plugin_t *snd_pcm_plugin_last(snd_pcm_t *handle, int channel);
ssize_t snd_pcm_plugin_client_samples(snd_pcm_t *handle, int channel, size_t drv_samples);
ssize_t snd_pcm_plugin_hardware_samples(snd_pcm_t *handle, int channel, size_t clt_samples);
ssize_t snd_pcm_plugin_client_size(snd_pcm_t *handle, int channel, size_t drv_size);
ssize_t snd_pcm_plugin_hardware_size(snd_pcm_t *handle, int channel, size_t clt_size);
int snd_pcm_plugin_info(snd_pcm_t *handle, snd_pcm_channel_info_t *info);
int snd_pcm_plugin_params(snd_pcm_t *handle, snd_pcm_channel_params_t *params);
int snd_pcm_plugin_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup);
int snd_pcm_plugin_status(snd_pcm_t *handle, snd_pcm_channel_status_t *status);
int snd_pcm_plugin_prepare(snd_pcm_t *handle, int channel);
int snd_pcm_plugin_playback_drain(snd_pcm_t *handle);
int snd_pcm_plugin_flush(snd_pcm_t *handle, int channel);
ssize_t snd_pcm_plugin_transfer_size(snd_pcm_t *handle, int channel);
int snd_pcm_plugin_pointer(snd_pcm_t *pcm, int channel, void **ptr, size_t *size);
ssize_t snd_pcm_plugin_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_plugin_read(snd_pcm_t *handle, void *bufer, size_t size);
int snd_pcm_plugin_pointerv(snd_pcm_t *pcm, int channel, struct iovec **vector, int *count);
ssize_t snd_pcm_plugin_writev(snd_pcm_t *pcm, const struct iovec *vector, int count);
ssize_t snd_pcm_plugin_readv(snd_pcm_t *pcm, const struct iovec *vector, int count);
ssize_t snd_pcm_plugin_write_continue(snd_pcm_t *pcm);

/*
 *  Plug-In helpers
 */

ssize_t snd_pcm_plugin_src_samples_to_size(snd_pcm_plugin_t *plugin, size_t samples);
ssize_t snd_pcm_plugin_dst_samples_to_size(snd_pcm_plugin_t *plugin, size_t samples);
ssize_t snd_pcm_plugin_src_size_to_samples(snd_pcm_plugin_t *plugin, size_t size);
ssize_t snd_pcm_plugin_dst_size_to_samples(snd_pcm_plugin_t *plugin, size_t size);
int snd_pcm_plugin_src_voices(snd_pcm_plugin_t *plugin,
			      snd_pcm_plugin_voice_t **voices,
			      size_t samples);
int snd_pcm_plugin_dst_voices(snd_pcm_plugin_t *plugin,
			      snd_pcm_plugin_voice_t **voices,
			      size_t samples);

/*
 *  Plug-In constructors
 */

/* basic I/O */
int snd_pcm_plugin_build_stream(snd_pcm_plugin_handle_t *handle, int channel,
				snd_pcm_format_t *format,
				snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_block(snd_pcm_plugin_handle_t *handle, int channel,
			       snd_pcm_format_t *format,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_mmap(snd_pcm_plugin_handle_t *handle, int channel,
			      snd_pcm_format_t *format,
			      snd_pcm_plugin_t **r_plugin);
/* conversion plugins */
int snd_pcm_plugin_build_interleave(snd_pcm_plugin_handle_t *handle,
				    snd_pcm_format_t *src_format,
				    snd_pcm_format_t *dst_format,
				    snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_linear(snd_pcm_plugin_handle_t *handle,
				snd_pcm_format_t *src_format,
				snd_pcm_format_t *dst_format,
				snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_mulaw(snd_pcm_plugin_handle_t *handle,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_alaw(snd_pcm_plugin_handle_t *handle,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_adpcm(snd_pcm_plugin_handle_t *handle,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_rate(snd_pcm_plugin_handle_t *handle,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_route(snd_pcm_plugin_handle_t *handle,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       int *ttable,
			       snd_pcm_plugin_t **r_plugin);

/*
 *  Loopback interface
 */

#define SND_PCM_LB_OPEN_PLAYBACK	0
#define SND_PCM_LB_OPEN_CAPTURE		1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_pcm_loopback_callbacks {
	void *private_data;		/* should be used with an application */
	size_t max_buffer_size;		/* zero = default (64kB) */
	void (*data) (void *private_data, char *buffer, size_t count);
	void (*position_change) (void *private_data, unsigned int pos);
	void (*format_change) (void *private_data, snd_pcm_format_t *format);
	void (*silence) (void *private_data, size_t count);
	void *reserved[31];		/* reserved for the future use - must be NULL!!! */
} snd_pcm_loopback_callbacks_t;

int snd_pcm_loopback_open(snd_pcm_loopback_t **handle, int card, int device, int subdev, int mode);
int snd_pcm_loopback_close(snd_pcm_loopback_t *handle);
int snd_pcm_loopback_file_descriptor(snd_pcm_loopback_t *handle);
int snd_pcm_loopback_block_mode(snd_pcm_loopback_t *handle, int enable);
int snd_pcm_loopback_stream_mode(snd_pcm_loopback_t *handle, int mode);
int snd_pcm_loopback_format(snd_pcm_loopback_t *handle, snd_pcm_format_t * format);
int snd_pcm_loopback_status(snd_pcm_loopback_t *handle, snd_pcm_loopback_status_t * status);
ssize_t snd_pcm_loopback_read(snd_pcm_loopback_t *handle, snd_pcm_loopback_callbacks_t * callbacks);

#ifdef __cplusplus
}
#endif

