/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

/** PCM generic info container */
typedef struct _snd_pcm_info snd_pcm_info_t;
/** PCM hardware configuration space container */
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
/** PCM software configuration container */
typedef struct _snd_pcm_sw_params snd_pcm_sw_params_t;
/** PCM status container */
typedef struct _snd_pcm_status snd_pcm_status_t;
/** PCM access types mask */
typedef struct _snd_pcm_access_mask snd_pcm_access_mask_t;
/** PCM formats mask */
typedef struct _snd_pcm_format_mask snd_pcm_format_mask_t;
/** PCM subformats mask */
typedef struct _snd_pcm_subformat_mask snd_pcm_subformat_mask_t;

/** PCM class */
typedef enum _snd_pcm_class {
	/** standard device */
	SND_PCM_CLASS_GENERIC = SNDRV_PCM_CLASS_GENERIC,
	/** multichannel device */
	SND_PCM_CLASS_MULTI = SNDRV_PCM_CLASS_MULTI,
	/** software modem device */
	SND_PCM_CLASS_MODEM = SNDRV_PCM_CLASS_MODEM,
	/** digitizer device */
	SND_PCM_CLASS_DIGITIZER = SNDRV_PCM_CLASS_DIGITIZER,
	SND_PCM_CLASS_LAST = SNDRV_PCM_CLASS_LAST,
} snd_pcm_class_t;

/** PCM subclass */
typedef enum _snd_pcm_subclass {
	/** subdevices are mixed together */
	SND_PCM_SUBCLASS_GENERIC_MIX = SNDRV_PCM_SUBCLASS_GENERIC_MIX,
	/** multichannel subdevices are mixed together */
	SND_PCM_SUBCLASS_MULTI_MIX = SNDRV_PCM_SUBCLASS_MULTI_MIX,
	SND_PCM_SUBCLASS_LAST = SNDRV_PCM_SUBCLASS_LAST,
} snd_pcm_subclass_t;

/** PCM stream (direction) */
typedef enum _snd_pcm_stream {
	/** Playback stream */
	SND_PCM_STREAM_PLAYBACK = SNDRV_PCM_STREAM_PLAYBACK,
	/** Capture stream */
	SND_PCM_STREAM_CAPTURE = SNDRV_PCM_STREAM_CAPTURE,
	SND_PCM_STREAM_LAST = SNDRV_PCM_STREAM_LAST,
} snd_pcm_stream_t;

/** PCM access type */
typedef enum _snd_pcm_access {
	/** mmap access with simple interleaved channels */
	SND_PCM_ACCESS_MMAP_INTERLEAVED = SNDRV_PCM_ACCESS_MMAP_INTERLEAVED,
	/** mmap access with simple non interleaved channels */
	SND_PCM_ACCESS_MMAP_NONINTERLEAVED = SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED,
	/** mmap access with complex placement */
	SND_PCM_ACCESS_MMAP_COMPLEX = SNDRV_PCM_ACCESS_MMAP_COMPLEX,
	/** snd_pcm_readi/snd_pcm_writei access */
	SND_PCM_ACCESS_RW_INTERLEAVED = SNDRV_PCM_ACCESS_RW_INTERLEAVED,
	/** snd_pcm_readn/snd_pcm_writen access */
	SND_PCM_ACCESS_RW_NONINTERLEAVED = SNDRV_PCM_ACCESS_RW_NONINTERLEAVED,
	SND_PCM_ACCESS_LAST = SNDRV_PCM_ACCESS_LAST,
} snd_pcm_access_t;

/** PCM sample format */
typedef enum _snd_pcm_format {
	/** Unknown */
	SND_PCM_FORMAT_UNKNOWN = -1,
	/** Signed 8 bit */
	SND_PCM_FORMAT_S8 = SNDRV_PCM_FORMAT_S8,
	/** Unsigned 8 bit */
	SND_PCM_FORMAT_U8 = SNDRV_PCM_FORMAT_U8,
	/** Signed 16 bit Little Endian */
	SND_PCM_FORMAT_S16_LE = SNDRV_PCM_FORMAT_S16_LE,
	/** Signed 16 bit Big Endian */
	SND_PCM_FORMAT_S16_BE = SNDRV_PCM_FORMAT_S16_BE,
	/** Unsigned 16 bit Little Endian */
	SND_PCM_FORMAT_U16_LE = SNDRV_PCM_FORMAT_U16_LE,
	/** Unsigned 16 bit Big Endian */
	SND_PCM_FORMAT_U16_BE = SNDRV_PCM_FORMAT_U16_BE,
	/** Signed 24 bit Little Endian */
	SND_PCM_FORMAT_S24_LE = SNDRV_PCM_FORMAT_S24_LE,
	/** Signed 24 bit Big Endian */
	SND_PCM_FORMAT_S24_BE = SNDRV_PCM_FORMAT_S24_BE,
	/** Unsigned 24 bit Little Endian */
	SND_PCM_FORMAT_U24_LE = SNDRV_PCM_FORMAT_U24_LE,
	/** Unsigned 24 bit Big Endian */
	SND_PCM_FORMAT_U24_BE = SNDRV_PCM_FORMAT_U24_BE,
	/** Signed 32 bit Little Endian */
	SND_PCM_FORMAT_S32_LE = SNDRV_PCM_FORMAT_S32_LE,
	/** Signed 32 bit Big Endian */
	SND_PCM_FORMAT_S32_BE = SNDRV_PCM_FORMAT_S32_BE,
	/** Unsigned 32 bit Little Endian */
	SND_PCM_FORMAT_U32_LE = SNDRV_PCM_FORMAT_U32_LE,
	/** Unsigned 32 bit Big Endian */
	SND_PCM_FORMAT_U32_BE = SNDRV_PCM_FORMAT_U32_BE,
	/** Float 32 bit Little Endian */
	SND_PCM_FORMAT_FLOAT_LE = SNDRV_PCM_FORMAT_FLOAT_LE,
	/** Float 32 bit Big Endian */
	SND_PCM_FORMAT_FLOAT_BE = SNDRV_PCM_FORMAT_FLOAT_BE,
	/** Float 64 bit Little Endian */
	SND_PCM_FORMAT_FLOAT64_LE = SNDRV_PCM_FORMAT_FLOAT64_LE,
	/** Float 64 bit Big Endian */
	SND_PCM_FORMAT_FLOAT64_BE = SNDRV_PCM_FORMAT_FLOAT64_BE,
	/** IEC-958 Little Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME_LE = SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE,
	/** IEC-958 Big Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME_BE = SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE,
	/** Mu-Law */
	SND_PCM_FORMAT_MU_LAW = SNDRV_PCM_FORMAT_MU_LAW,
	/** A-Law */
	SND_PCM_FORMAT_A_LAW = SNDRV_PCM_FORMAT_A_LAW,
	/** Ima-ADPCM */
	SND_PCM_FORMAT_IMA_ADPCM = SNDRV_PCM_FORMAT_IMA_ADPCM,
	/** MPEG */
	SND_PCM_FORMAT_MPEG = SNDRV_PCM_FORMAT_MPEG,
	/** GSM */
	SND_PCM_FORMAT_GSM = SNDRV_PCM_FORMAT_GSM,
	/** Special */
	SND_PCM_FORMAT_SPECIAL = SNDRV_PCM_FORMAT_SPECIAL,
	SND_PCM_FORMAT_LAST = SNDRV_PCM_FORMAT_LAST,
	/** Signed 16 bit CPU endian */
	SND_PCM_FORMAT_S16 = SNDRV_PCM_FORMAT_S16,
	/** Unsigned 16 bit CPU endian */
	SND_PCM_FORMAT_U16 = SNDRV_PCM_FORMAT_U16,
	/** Signed 24 bit CPU endian */
	SND_PCM_FORMAT_S24 = SNDRV_PCM_FORMAT_S24,
	/** Unsigned 24 bit CPU endian */
	SND_PCM_FORMAT_U24 = SNDRV_PCM_FORMAT_U24,
	/** Signed 32 bit CPU endian */
	SND_PCM_FORMAT_S32 = SNDRV_PCM_FORMAT_S32,
	/** Unsigned 32 bit CPU endian */
	SND_PCM_FORMAT_U32 = SNDRV_PCM_FORMAT_U32,
	/** Float 32 bit CPU endian */
	SND_PCM_FORMAT_FLOAT = SNDRV_PCM_FORMAT_FLOAT,
	/** Float 64 bit CPU endian */
	SND_PCM_FORMAT_FLOAT64 = SNDRV_PCM_FORMAT_FLOAT64,
	/** IEC-958 CPU Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME = SNDRV_PCM_FORMAT_IEC958_SUBFRAME,
} snd_pcm_format_t;

/** PCM sample subformat */
typedef enum _snd_pcm_subformat {
	/** Standard */
	SND_PCM_SUBFORMAT_STD = SNDRV_PCM_SUBFORMAT_STD,
	SND_PCM_SUBFORMAT_LAST = SNDRV_PCM_SUBFORMAT_LAST,
} snd_pcm_subformat_t;

/** PCM state */
typedef enum _snd_pcm_state {
	/** Open */
	SND_PCM_STATE_OPEN = SNDRV_PCM_STATE_OPEN,
	/** Setup installed */ 
	SND_PCM_STATE_SETUP = SNDRV_PCM_STATE_SETUP,
	/** Ready to start */
	SND_PCM_STATE_PREPARED = SNDRV_PCM_STATE_PREPARED,
	/** Running */
	SND_PCM_STATE_RUNNING = SNDRV_PCM_STATE_RUNNING,
	/** Stopped: underrun (playback) or overrun (capture) detected */
	SND_PCM_STATE_XRUN = SNDRV_PCM_STATE_XRUN,
	/** Draining: running (playback) or stopped (capture) */
	SND_PCM_STATE_DRAINING = SNDRV_PCM_STATE_DRAINING,
	/** Paused */
	SND_PCM_STATE_PAUSED = SNDRV_PCM_STATE_PAUSED,
	SND_PCM_STATE_LAST = SNDRV_PCM_STATE_LAST,
} snd_pcm_state_t;

/** PCM start mode */
typedef enum _snd_pcm_start {
	/** Automatic start on data read/write */
	SND_PCM_START_DATA = SNDRV_PCM_START_DATA,
	/** Explicit start */
	SND_PCM_START_EXPLICIT = SNDRV_PCM_START_EXPLICIT,
	SND_PCM_START_LAST = SNDRV_PCM_START_LAST,
} snd_pcm_start_t;

/** PCM xrun mode */
typedef enum _snd_pcm_xrun {
	/** Xrun detection disabled */
	SND_PCM_XRUN_NONE = SNDRV_PCM_XRUN_NONE,
	/** Stop on xrun detection */
	SND_PCM_XRUN_STOP = SNDRV_PCM_XRUN_STOP,
	SND_PCM_XRUN_LAST = SNDRV_PCM_XRUN_LAST,
} snd_pcm_xrun_t;

/** PCM timestamp mode */
typedef enum _snd_pcm_tstamp {
	/** No timestamp */
	SND_PCM_TSTAMP_NONE = SNDRV_PCM_TSTAMP_NONE,
	/** Update mmap'ed timestamp */
	SND_PCM_TSTAMP_MMAP = SNDRV_PCM_TSTAMP_MMAP,
	SND_PCM_TSTAMP_LAST = SNDRV_PCM_TSTAMP_LAST,
} snd_pcm_tstamp_t;

/** Unsigned frames quantity */
typedef sndrv_pcm_uframes_t snd_pcm_uframes_t;
/** Signed frames quantity */
typedef sndrv_pcm_sframes_t snd_pcm_sframes_t;
/** Timestamp */
typedef struct timeval snd_timestamp_t;

#define SND_PCM_INFO_MMAP SNDRV_PCM_INFO_MMAP
#define SND_PCM_INFO_MMAP_VALID SNDRV_PCM_INFO_MMAP_VALID
#define SND_PCM_INFO_DOUBLE SNDRV_PCM_INFO_DOUBLE
#define SND_PCM_INFO_BATCH SNDRV_PCM_INFO_BATCH
#define SND_PCM_INFO_INTERLEAVED SNDRV_PCM_INFO_INTERLEAVED
#define SND_PCM_INFO_NONINTERLEAVED SNDRV_PCM_INFO_NONINTERLEAVED
#define SND_PCM_INFO_COMPLEX SNDRV_PCM_INFO_COMPLEX
#define SND_PCM_INFO_BLOCK_TRANSFER SNDRV_PCM_INFO_BLOCK_TRANSFER
#define SND_PCM_INFO_OVERRANGE SNDRV_PCM_INFO_OVERRANGE
#define SND_PCM_INFO_PAUSE SNDRV_PCM_INFO_PAUSE
#define SND_PCM_INFO_HALF_DUPLEX SNDRV_PCM_INFO_HALF_DUPLEX
#define SND_PCM_INFO_JOINT_DUPLEX SNDRV_PCM_INFO_JOINT_DUPLEX
#define SND_PCM_INFO_SYNC_START SNDRV_PCM_INFO_SYNC_START

/** Non blocking mode \hideinitializer */
#define SND_PCM_NONBLOCK		0x0001
/** Async notification \hideinitializer */
#define SND_PCM_ASYNC			0x0002

/** PCM handle */
typedef struct _snd_pcm snd_pcm_t;

/** PCM type */
enum _snd_pcm_type {
	/** Kernel level PCM */
	SND_PCM_TYPE_HW,
	/** One ore more linked PCM with exclusive access to selected
	    channels */
	SND_PCM_TYPE_MULTI,
	/** File writing plugin */
	SND_PCM_TYPE_FILE,
	/** Null endpoint PCM */
	SND_PCM_TYPE_NULL,
	/** Shared memory client PCM */
	SND_PCM_TYPE_SHM,
	/** INET client PCM (not yet implemented) */
	SND_PCM_TYPE_INET,
	/** Copying plugin */
	SND_PCM_TYPE_COPY,
	/** Linear format conversion PCM */
	SND_PCM_TYPE_LINEAR,
	/** A-Law format conversion PCM */
	SND_PCM_TYPE_ALAW,
	/** Mu-Law format conversion PCM */
	SND_PCM_TYPE_MULAW,
	/** IMA-ADPCM format conversion PCM */
	SND_PCM_TYPE_ADPCM,
	/** Rate conversion PCM */
	SND_PCM_TYPE_RATE,
	/** Attenuated static route PCM */
	SND_PCM_TYPE_ROUTE,
	/** Format adjusted PCM */
	SND_PCM_TYPE_PLUG,
	/** Sharing PCM */
	SND_PCM_TYPE_SHARE,
	/** Meter plugin */
	SND_PCM_TYPE_METER,
	/** Mixing PCM */
	SND_PCM_TYPE_MIX,
	/** Attenuated dynamic route PCM (not yet implemented) */
	SND_PCM_TYPE_DROUTE,
	/** Loopback server plugin (not yet implemented) */
	SND_PCM_TYPE_LBSERVER,
};

/** PCM type */
typedef enum _snd_pcm_type snd_pcm_type_t;

/** PCM area specification */
typedef struct _snd_pcm_channel_area {
	/** base address of channel samples */
	void *addr;
	/** offset to first sample in bits */
	unsigned int first;
	/** samples distance in bits */
	unsigned int step;
} snd_pcm_channel_area_t;

/** #SND_PCM_TYPE_METER scope handle */
typedef struct _snd_pcm_scope snd_pcm_scope_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_pcm_open(snd_pcm_t **pcm, const char *name, 
		 snd_pcm_stream_t stream, int mode);

snd_pcm_type_t snd_pcm_type(snd_pcm_t *pcm);
int snd_pcm_close(snd_pcm_t *pcm);
int snd_pcm_poll_descriptors_count(snd_pcm_t *pcm);
int snd_pcm_poll_descriptors(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int space);
int snd_pcm_nonblock(snd_pcm_t *pcm, int nonblock);
int snd_pcm_async(snd_pcm_t *pcm, int sig, pid_t pid);
int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t *info);
int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_free(snd_pcm_t *pcm);
int snd_pcm_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int snd_pcm_prepare(snd_pcm_t *pcm);
int snd_pcm_reset(snd_pcm_t *pcm);
int snd_pcm_status(snd_pcm_t *pcm, snd_pcm_status_t *status);
int snd_pcm_start(snd_pcm_t *pcm);
int snd_pcm_drop(snd_pcm_t *pcm);
int snd_pcm_drain(snd_pcm_t *pcm);
int snd_pcm_pause(snd_pcm_t *pcm, int enable);
snd_pcm_state_t snd_pcm_state(snd_pcm_t *pcm);
int snd_pcm_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp);
snd_pcm_sframes_t snd_pcm_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);

int snd_pcm_dump_hw_setup(snd_pcm_t *pcm, snd_output_t *out);
int snd_pcm_dump_sw_setup(snd_pcm_t *pcm, snd_output_t *out);
int snd_pcm_dump_setup(snd_pcm_t *pcm, snd_output_t *out);
int snd_pcm_dump(snd_pcm_t *pcm, snd_output_t *out);
int snd_pcm_link(snd_pcm_t *pcm1, snd_pcm_t *pcm2);
int snd_pcm_unlink(snd_pcm_t *pcm);

int snd_pcm_wait(snd_pcm_t *pcm, int timeout);
snd_pcm_sframes_t snd_pcm_avail_update(snd_pcm_t *pcm);
const char *snd_pcm_name(snd_pcm_t *pcm);
snd_pcm_type_t snd_pcm_type(snd_pcm_t *pcm);

/* HW params */
int snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);

int snd_pcm_hw_params_get_rate_numden(const snd_pcm_hw_params_t *params,
				      unsigned int *rate_num,
				      unsigned int *rate_den);
int snd_pcm_hw_params_get_sbits(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_get_fifo_size(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_dump(snd_pcm_hw_params_t *params, snd_output_t *out);

#if 0
typedef struct _snd_pcm_hw_strategy snd_pcm_hw_strategy_t;

/* choices need to be sorted on ascending badness */
typedef struct _snd_pcm_hw_strategy_simple_choices_list {
	unsigned int value;
	unsigned int badness;
} snd_pcm_hw_strategy_simple_choices_list_t;

int snd_pcm_hw_params_strategy(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			       const snd_pcm_hw_strategy_t *strategy,
			       unsigned int badness_min,
			       unsigned int badness_max);

void snd_pcm_hw_strategy_free(snd_pcm_hw_strategy_t *strategy);
int snd_pcm_hw_strategy_simple(snd_pcm_hw_strategy_t **strategyp,
			       unsigned int badness_min,
			       unsigned int badness_max);
int snd_pcm_hw_params_try_explain_failure(snd_pcm_t *pcm,
					  snd_pcm_hw_params_t *fail,
					  snd_pcm_hw_params_t *success,
					  unsigned int depth,
					  snd_output_t *out);

#endif

int snd_pcm_sw_params_current(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_dump(snd_pcm_sw_params_t *params, snd_output_t *out);

int snd_pcm_status_dump(snd_pcm_status_t *status, snd_output_t *out);

/* mmap */
const snd_pcm_channel_area_t *snd_pcm_mmap_areas(snd_pcm_t *pcm);
const snd_pcm_channel_area_t *snd_pcm_mmap_running_areas(snd_pcm_t *pcm);
const snd_pcm_channel_area_t *snd_pcm_mmap_stopped_areas(snd_pcm_t *pcm);
snd_pcm_sframes_t snd_pcm_mmap_forward(snd_pcm_t *pcm, snd_pcm_uframes_t size);
snd_pcm_uframes_t snd_pcm_mmap_offset(snd_pcm_t *pcm);
snd_pcm_uframes_t snd_pcm_mmap_xfer(snd_pcm_t *pcm, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_mmap_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_mmap_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_mmap_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_mmap_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);

const char *snd_pcm_stream_name(snd_pcm_stream_t stream);
const char *snd_pcm_access_name(snd_pcm_access_t access);
const char *snd_pcm_format_name(snd_pcm_format_t format);
const char *snd_pcm_subformat_name(snd_pcm_subformat_t subformat);
const char *snd_pcm_format_description(snd_pcm_format_t format);
snd_pcm_format_t snd_pcm_format_value(const char* name);
const char *snd_pcm_start_mode_name(snd_pcm_start_t mode);
const char *snd_pcm_xrun_mode_name(snd_pcm_xrun_t mode);
const char *snd_pcm_tstamp_mode_name(snd_pcm_tstamp_t mode);
const char *snd_pcm_state_name(snd_pcm_state_t state);

int snd_pcm_area_silence(const snd_pcm_channel_area_t *dst_channel, snd_pcm_uframes_t dst_offset,
			 unsigned int samples, snd_pcm_format_t format);
int snd_pcm_areas_silence(const snd_pcm_channel_area_t *dst_channels, snd_pcm_uframes_t dst_offset,
			  unsigned int channels, snd_pcm_uframes_t frames, snd_pcm_format_t format);
int snd_pcm_area_copy(const snd_pcm_channel_area_t *dst_channel, snd_pcm_uframes_t dst_offset,
		      const snd_pcm_channel_area_t *src_channel, snd_pcm_uframes_t src_offset,
		      unsigned int samples, snd_pcm_format_t format);
int snd_pcm_areas_copy(const snd_pcm_channel_area_t *dst_channels, snd_pcm_uframes_t dst_offset,
		       const snd_pcm_channel_area_t *src_channels, snd_pcm_uframes_t src_offset,
		       unsigned int channels, snd_pcm_uframes_t frames, snd_pcm_format_t format);

snd_pcm_sframes_t snd_pcm_bytes_to_frames(snd_pcm_t *pcm, ssize_t bytes);
ssize_t snd_pcm_frames_to_bytes(snd_pcm_t *pcm, snd_pcm_sframes_t frames);
int snd_pcm_bytes_to_samples(snd_pcm_t *pcm, ssize_t bytes);
ssize_t snd_pcm_samples_to_bytes(snd_pcm_t *pcm, int samples);

/** #SND_PCM_TYPE_METER scope functions */
typedef struct _snd_pcm_scope_ops {
	/** Enable and prepare it using current params */
	int (*enable)(snd_pcm_scope_t *scope);
	/** Disable */
	void (*disable)(snd_pcm_scope_t *scope);
	/** PCM has been started */
	void (*start)(snd_pcm_scope_t *scope);
	/** PCM has been stopped */
	void (*stop)(snd_pcm_scope_t *scope);
	/** New frames are present */
	void (*update)(snd_pcm_scope_t *scope);
	/** Reset status */
	void (*reset)(snd_pcm_scope_t *scope);
	/** PCM is closing */
	void (*close)(snd_pcm_scope_t *scope);
} snd_pcm_scope_ops_t;

snd_pcm_uframes_t snd_pcm_meter_get_bufsize(snd_pcm_t *pcm);
unsigned int snd_pcm_meter_get_channels(snd_pcm_t *pcm);
unsigned int snd_pcm_meter_get_rate(snd_pcm_t *pcm);
snd_pcm_uframes_t snd_pcm_meter_get_now(snd_pcm_t *pcm);
snd_pcm_uframes_t snd_pcm_meter_get_boundary(snd_pcm_t *pcm);
int snd_pcm_meter_add_scope(snd_pcm_t *pcm, snd_pcm_scope_t *scope);
snd_pcm_scope_t *snd_pcm_meter_search_scope(snd_pcm_t *pcm, const char *name);
int snd_pcm_scope_malloc(snd_pcm_scope_t **ptr);
void snd_pcm_scope_set_ops(snd_pcm_scope_t *scope, snd_pcm_scope_ops_t *val);
void snd_pcm_scope_set_name(snd_pcm_scope_t *scope, const char *val);
const char *snd_pcm_scope_get_name(snd_pcm_scope_t *scope);
void *snd_pcm_scope_get_callback_private(snd_pcm_scope_t *scope);
void snd_pcm_scope_set_callback_private(snd_pcm_scope_t *scope, void *val);
int snd_pcm_scope_s16_open(snd_pcm_t *pcm, const char *name,
			   snd_pcm_scope_t **scopep);
int16_t *snd_pcm_scope_s16_get_channel_buffer(snd_pcm_scope_t *scope,
					      unsigned int channel);

/* misc */

int snd_pcm_format_signed(snd_pcm_format_t format);
int snd_pcm_format_unsigned(snd_pcm_format_t format);
int snd_pcm_format_linear(snd_pcm_format_t format);
int snd_pcm_format_little_endian(snd_pcm_format_t format);
int snd_pcm_format_big_endian(snd_pcm_format_t format);
int snd_pcm_format_cpu_endian(snd_pcm_format_t format);
int snd_pcm_format_width(snd_pcm_format_t format);			/* in bits */
int snd_pcm_format_physical_width(snd_pcm_format_t format);		/* in bits */
snd_pcm_format_t snd_pcm_build_linear_format(int width, int unsignd, int big_endian);
ssize_t snd_pcm_format_size(snd_pcm_format_t format, size_t samples);
u_int8_t snd_pcm_format_silence(snd_pcm_format_t format);
u_int16_t snd_pcm_format_silence_16(snd_pcm_format_t format);
u_int32_t snd_pcm_format_silence_32(snd_pcm_format_t format);
u_int64_t snd_pcm_format_silence_64(snd_pcm_format_t format);
int snd_pcm_format_set_silence(snd_pcm_format_t format, void *buf, unsigned int samples);

#ifdef __cplusplus
}
#endif

