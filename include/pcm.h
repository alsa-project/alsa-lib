/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

#define SND_PCM_OPEN_PLAYBACK		0x0001
#define SND_PCM_OPEN_CAPTURE		0x0002
#define SND_PCM_OPEN_DUPLEX		0x0003
#define SND_PCM_OPEN_STREAM		0x1000
#define SND_PCM_OPEN_STREAM_PLAYBACK	0x1001
#define SND_PCM_OPEN_STREAM_CAPTURE	0x1002
#define SND_PCM_OPEN_STREAM_DUPLEX	0x1003

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_pcm snd_pcm_t;
typedef struct snd_pcm_loopback snd_pcm_loopback_t;

int snd_pcm_open(snd_pcm_t **handle, int card, int device, int mode);
int snd_pcm_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int mode);
int snd_pcm_close(snd_pcm_t *handle);
int snd_pcm_file_descriptor(snd_pcm_t *handle);
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
int snd_pcm_drain_playback(snd_pcm_t *handle);
int snd_pcm_flush_playback(snd_pcm_t *handle);
int snd_pcm_flush_capture(snd_pcm_t *handle);
int snd_pcm_flush_channel(snd_pcm_t *handle, int channel);
int snd_pcm_playback_pause(snd_pcm_t *handle, int enable);
ssize_t snd_pcm_transfer_size(snd_pcm_t *handle, int channel);
ssize_t snd_pcm_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_read(snd_pcm_t *handle, void *buffer, size_t size);
int snd_pcm_mmap(snd_pcm_t *handle, int channel, snd_pcm_mmap_control_t **control, void **buffer);
int snd_pcm_munmap(snd_pcm_t *handle, int channel);

/* misc */

int snd_pcm_format_signed(int format);
int snd_pcm_format_unsigned(int format);
int snd_pcm_format_little_endian(int format);
int snd_pcm_format_big_endian(int format);
int snd_pcm_format_width(int format);		/* in bits */
ssize_t snd_pcm_format_size(int format, size_t samples);

#ifdef __cplusplus
}
#endif

/*
 *  Plug-In interface (ala C++)
 */

typedef struct snd_stru_pcm_plugin snd_pcm_plugin_t;

typedef enum {
	INIT = 0,
	PREPARE = 1,
	DRAIN = 2,
	FLUSH = 3
} snd_pcm_plugin_action_t;

#define snd_pcm_plugin_extra_data(plugin) (((char *)plugin) + sizeof(*plugin))

struct snd_stru_pcm_plugin {
	char *name;				   /* plug-in name */
	int (*transfer_src_ptr)(snd_pcm_plugin_t *plugin, char **src_ptr, size_t *src_size);
	ssize_t (*transfer)(snd_pcm_plugin_t *plugin,
			    char *src_ptr, size_t src_size,
			    char *dst_ptr, size_t dst_size);
	ssize_t (*src_size)(snd_pcm_plugin_t *plugin, size_t dst_size);
	ssize_t (*dst_size)(snd_pcm_plugin_t *plugin, size_t src_size);
	int (*action)(snd_pcm_plugin_t *plugin, snd_pcm_plugin_action_t action);
	snd_pcm_plugin_t *prev;
	snd_pcm_plugin_t *next;
	void *private_data;
	void (*private_free)(snd_pcm_plugin_t *plugin, void *private_data);
};

snd_pcm_plugin_t *snd_pcm_plugin_build(const char *name, int extra);
int snd_pcm_plugin_free(snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_clear(snd_pcm_t *handle, int channel);
int snd_pcm_plugin_insert(snd_pcm_t *handle, int channel, snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_append(snd_pcm_t *handle, int channel, snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_remove_to(snd_pcm_t *handle, int channel, snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_remove_first(snd_pcm_t *handle, int channel);
snd_pcm_plugin_t *snd_pcm_plugin_first(snd_pcm_t *handle, int channel);
snd_pcm_plugin_t *snd_pcm_plugin_last(snd_pcm_t *handle, int channel);
ssize_t snd_pcm_plugin_transfer_size(snd_pcm_t *handle, int channel, size_t drv_size);
ssize_t snd_pcm_plugin_hardware_size(snd_pcm_t *handle, int channel, size_t trf_size);
int snd_pcm_plugin_info(snd_pcm_t *handle, snd_pcm_channel_info_t *info);
int snd_pcm_plugin_params(snd_pcm_t *handle, snd_pcm_channel_params_t *params);
int snd_pcm_plugin_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup);
int snd_pcm_plugin_status(snd_pcm_t *handle, snd_pcm_channel_status_t *status);
int snd_pcm_plugin_prepare(snd_pcm_t *handle, int channel);
int snd_pcm_plugin_drain_playback(snd_pcm_t *handle);
int snd_pcm_plugin_flush(snd_pcm_t *handle, int channel);
int snd_pcm_plugin_pointer(snd_pcm_t *pcm, int channel, void **ptr, size_t *size);
ssize_t snd_pcm_plugin_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_plugin_read(snd_pcm_t *handle, void *bufer, size_t size);

/*
 *  Plug-In constructors
 */

/* basic I/O */
int snd_pcm_plugin_build_stream(snd_pcm_t *handle, int channel, snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_block(snd_pcm_t *handle, int channel, snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_mmap(snd_pcm_t *handle, int channel, snd_pcm_plugin_t **r_plugin);
/* conversion plugins */
int snd_pcm_plugin_build_interleave(int src_interleave, int dst_interleave, int format, snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_linear(int src_format, int dst_format, snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_mulaw(int src_format, int dst_format, snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_alaw(int src_format, int dst_format, snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_rate(int src_format, int src_rate, int src_voices,
			      int dst_format, int dst_rate, int dst_voices,
			      snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_voices(int src_format, int src_voices,
				int dst_format, int dst_voices,
				snd_pcm_plugin_t **r_plugin);

/*
 *  Loopback interface
 */

#define SND_PCM_LB_OPEN_PLAYBACK	0
#define SND_PCM_LB_OPEN_CAPTURE		1

#ifdef __cplusplus
extern "C" {
#endif

int snd_pcm_loopback_open(snd_pcm_loopback_t **handle, int card, int device, int subchn, int mode);
int snd_pcm_loopback_close(snd_pcm_loopback_t *handle);
int snd_pcm_loopback_file_descriptor(snd_pcm_loopback_t *handle);
int snd_pcm_loopback_block_mode(snd_pcm_loopback_t *handle, int enable);
int snd_pcm_loopback_stream_mode(snd_pcm_loopback_t *handle, int mode);
int snd_pcm_loopback_format(snd_pcm_loopback_t *handle, snd_pcm_format_t * format);
ssize_t snd_pcm_loopback_read(snd_pcm_loopback_t *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

