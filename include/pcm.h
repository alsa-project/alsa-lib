/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

/**
 *  \defgroup PCM PCM Interface
 *  The PCM Interface.
 *  \{
 */

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
	SND_PCM_START_DATA,
	/** Explicit start */
	SND_PCM_START_EXPLICIT,
	SND_PCM_START_LAST,
} snd_pcm_start_t;

/** PCM xrun mode */
typedef enum _snd_pcm_xrun {
	/** Xrun detection disabled */
	SND_PCM_XRUN_NONE,
	/** Stop on xrun detection */
	SND_PCM_XRUN_STOP,
	SND_PCM_XRUN_LAST,
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

/** device accepts mmaped access */
#define SND_PCM_INFO_MMAP SNDRV_PCM_INFO_MMAP
/** device accepts  mmaped access with sample resolution */
#define SND_PCM_INFO_MMAP_VALID SNDRV_PCM_INFO_MMAP_VALID
/** device is doing double buffering */
#define SND_PCM_INFO_DOUBLE SNDRV_PCM_INFO_DOUBLE
/** device transfers samples in batch */
#define SND_PCM_INFO_BATCH SNDRV_PCM_INFO_BATCH
/** device accepts interleaved samples */
#define SND_PCM_INFO_INTERLEAVED SNDRV_PCM_INFO_INTERLEAVED
/** device accepts non-interleaved samples */
#define SND_PCM_INFO_NONINTERLEAVED SNDRV_PCM_INFO_NONINTERLEAVED
/** device accepts complex sample organization */
#define SND_PCM_INFO_COMPLEX SNDRV_PCM_INFO_COMPLEX
/** device is capable block transfers */
#define SND_PCM_INFO_BLOCK_TRANSFER SNDRV_PCM_INFO_BLOCK_TRANSFER
/** device can detect DAC/ADC overrange */
#define SND_PCM_INFO_OVERRANGE SNDRV_PCM_INFO_OVERRANGE
/** device is capable to pause */
#define SND_PCM_INFO_PAUSE SNDRV_PCM_INFO_PAUSE
/** device can do only half duplex */
#define SND_PCM_INFO_HALF_DUPLEX SNDRV_PCM_INFO_HALF_DUPLEX
/** device can do only joint duplex (same parameters) */
#define SND_PCM_INFO_JOINT_DUPLEX SNDRV_PCM_INFO_JOINT_DUPLEX
/** device can do a kind of synchronized start */
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
	/** Hooked PCM */
	SND_PCM_TYPE_HOOKS,
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
	/** Surround plugin */
	SND_PCM_TYPE_SURROUND,
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

int snd_pcm_close(snd_pcm_t *pcm);
int snd_pcm_poll_descriptors_count(snd_pcm_t *pcm);
int snd_pcm_poll_descriptors(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int space);
int snd_pcm_nonblock(snd_pcm_t *pcm, int nonblock);
int snd_async_add_pcm_handler(snd_async_handler_t **handler, snd_pcm_t *pcm, 
			      snd_async_callback_t callback, void *private_data);
snd_pcm_t *snd_async_handler_get_pcm(snd_async_handler_t *handler);
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
snd_pcm_stream_t snd_pcm_stream(snd_pcm_t *pcm);

/* HW params */
int snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);

int snd_pcm_hw_params_is_mmap_sample_resolution(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_is_double(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_is_batch(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_is_block_transfer(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_is_overrange(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_is_pause(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_is_half_duplex(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_is_joint_duplex(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_is_sync_start(const snd_pcm_hw_params_t *params);
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
int snd_pcm_mmap_begin(snd_pcm_t *pcm,
		       const snd_pcm_channel_area_t **areas,
		       snd_pcm_uframes_t *offset,
		       snd_pcm_uframes_t *frames);
int snd_pcm_mmap_commit(snd_pcm_t *pcm, snd_pcm_uframes_t offset,
			snd_pcm_uframes_t frames);

const char *snd_pcm_stream_name(snd_pcm_stream_t stream);
const char *snd_pcm_access_name(snd_pcm_access_t _access);
const char *snd_pcm_format_name(snd_pcm_format_t format);
const char *snd_pcm_format_description(snd_pcm_format_t format);
const char *snd_pcm_subformat_name(snd_pcm_subformat_t subformat);
const char *snd_pcm_subformat_description(snd_pcm_subformat_t subformat);
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
	/** \brief Enable and prepare it using current params
	 * \param scope scope handle
	 */
	int (*enable)(snd_pcm_scope_t *scope);
	/** \brief Disable
	 * \param scope scope handle
	 */
	void (*disable)(snd_pcm_scope_t *scope);
	/** \brief PCM has been started
	 * \param scope scope handle
	 */
	void (*start)(snd_pcm_scope_t *scope);
	/** \brief PCM has been stopped
	 * \param scope scope handle
	 */
	void (*stop)(snd_pcm_scope_t *scope);
	/** \brief New frames are present
	 * \param scope scope handle
	 */
	void (*update)(snd_pcm_scope_t *scope);
	/** \brief Reset status
	 * \param scope scope handle
	 */
	void (*reset)(snd_pcm_scope_t *scope);
	/** \brief PCM is closing
	 * \param scope scope handle
	 */
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

size_t snd_pcm_access_mask_sizeof(void);
/** \hideinitializer
 * \brief allocate an empty #snd_pcm_access_mask_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_pcm_access_mask_alloca(ptr) do { assert(ptr); *ptr = (snd_pcm_access_mask_t *) alloca(snd_pcm_access_mask_sizeof()); memset(*ptr, 0, snd_pcm_access_mask_sizeof()); } while (0)
int snd_pcm_access_mask_malloc(snd_pcm_access_mask_t **ptr);
void snd_pcm_access_mask_free(snd_pcm_access_mask_t *obj);
void snd_pcm_access_mask_copy(snd_pcm_access_mask_t *dst, const snd_pcm_access_mask_t *src);
void snd_pcm_access_mask_none(snd_pcm_access_mask_t *mask);
void snd_pcm_access_mask_any(snd_pcm_access_mask_t *mask);
int snd_pcm_access_mask_test(const snd_pcm_access_mask_t *mask, snd_pcm_access_t val);
void snd_pcm_access_mask_set(snd_pcm_access_mask_t *mask, snd_pcm_access_t val);
void snd_pcm_access_mask_reset(snd_pcm_access_mask_t *mask, snd_pcm_access_t val);

size_t snd_pcm_format_mask_sizeof(void);
/** \hideinitializer
 * \brief allocate an empty #snd_pcm_format_mask_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_pcm_format_mask_alloca(ptr) do { assert(ptr); *ptr = (snd_pcm_format_mask_t *) alloca(snd_pcm_format_mask_sizeof()); memset(*ptr, 0, snd_pcm_format_mask_sizeof()); } while (0)
int snd_pcm_format_mask_malloc(snd_pcm_format_mask_t **ptr);
void snd_pcm_format_mask_free(snd_pcm_format_mask_t *obj);
void snd_pcm_format_mask_copy(snd_pcm_format_mask_t *dst, const snd_pcm_format_mask_t *src);
void snd_pcm_format_mask_none(snd_pcm_format_mask_t *mask);
void snd_pcm_format_mask_any(snd_pcm_format_mask_t *mask);
int snd_pcm_format_mask_test(const snd_pcm_format_mask_t *mask, snd_pcm_format_t val);
void snd_pcm_format_mask_set(snd_pcm_format_mask_t *mask, snd_pcm_format_t val);
void snd_pcm_format_mask_reset(snd_pcm_format_mask_t *mask, snd_pcm_format_t val);

size_t snd_pcm_subformat_mask_sizeof(void);
/** \hideinitializer
 * \brief allocate an empty #snd_pcm_subformat_mask_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_pcm_subformat_mask_alloca(ptr) do { assert(ptr); *ptr = (snd_pcm_subformat_mask_t *) alloca(snd_pcm_subformat_mask_sizeof()); memset(*ptr, 0, snd_pcm_subformat_mask_sizeof()); } while (0)
int snd_pcm_subformat_mask_malloc(snd_pcm_subformat_mask_t **ptr);
void snd_pcm_subformat_mask_free(snd_pcm_subformat_mask_t *obj);
void snd_pcm_subformat_mask_copy(snd_pcm_subformat_mask_t *dst, const snd_pcm_subformat_mask_t *src);
void snd_pcm_subformat_mask_none(snd_pcm_subformat_mask_t *mask);
void snd_pcm_subformat_mask_any(snd_pcm_subformat_mask_t *mask);
int snd_pcm_subformat_mask_test(const snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val);
void snd_pcm_subformat_mask_set(snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val);
void snd_pcm_subformat_mask_reset(snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val);

size_t snd_pcm_hw_params_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_pcm_hw_params_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_pcm_hw_params_alloca(ptr) do { assert(ptr); *ptr = (snd_pcm_hw_params_t *) alloca(snd_pcm_hw_params_sizeof()); memset(*ptr, 0, snd_pcm_hw_params_sizeof()); } while (0)
int snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **ptr);
void snd_pcm_hw_params_free(snd_pcm_hw_params_t *obj);
void snd_pcm_hw_params_copy(snd_pcm_hw_params_t *dst, const snd_pcm_hw_params_t *src);
int snd_pcm_hw_params_get_access(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_test_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t val);
int snd_pcm_hw_params_set_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t val);
snd_pcm_access_t snd_pcm_hw_params_set_access_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
snd_pcm_access_t snd_pcm_hw_params_set_access_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_set_access_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_mask_t *mask);
void snd_pcm_hw_params_get_access_mask(snd_pcm_hw_params_t *params, snd_pcm_access_mask_t *mask);
int snd_pcm_hw_params_get_format(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_test_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val);
int snd_pcm_hw_params_set_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val);
snd_pcm_format_t snd_pcm_hw_params_set_format_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
snd_pcm_format_t snd_pcm_hw_params_set_format_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_set_format_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_mask_t *mask);
void snd_pcm_hw_params_get_format_mask(snd_pcm_hw_params_t *params, snd_pcm_format_mask_t *mask);
int snd_pcm_hw_params_test_subformat(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t val);
int snd_pcm_hw_params_get_subformat(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_set_subformat(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t val);
snd_pcm_subformat_t snd_pcm_hw_params_set_subformat_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
snd_pcm_subformat_t snd_pcm_hw_params_set_subformat_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_set_subformat_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_mask_t *mask);
void snd_pcm_hw_params_get_subformat_mask(snd_pcm_hw_params_t *params, snd_pcm_subformat_mask_t *mask);
int snd_pcm_hw_params_get_channels(const snd_pcm_hw_params_t *params);
unsigned int snd_pcm_hw_params_get_channels_min(const snd_pcm_hw_params_t *params);
unsigned int snd_pcm_hw_params_get_channels_max(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_test_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
int snd_pcm_hw_params_set_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
int snd_pcm_hw_params_set_channels_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val);
int snd_pcm_hw_params_set_channels_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val);
int snd_pcm_hw_params_set_channels_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, unsigned int *max);
unsigned int snd_pcm_hw_params_set_channels_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
unsigned int snd_pcm_hw_params_set_channels_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
unsigned int snd_pcm_hw_params_set_channels_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_get_rate(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_rate_min(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_rate_max(const snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_test_rate(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_rate(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_rate_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_rate_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_rate_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir);
unsigned int snd_pcm_hw_params_set_rate_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir);
unsigned int snd_pcm_hw_params_set_rate_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_set_rate_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_get_period_time(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_period_time_min(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_period_time_max(const snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_test_period_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_period_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_period_time_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_period_time_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_period_time_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir);
unsigned int snd_pcm_hw_params_set_period_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir);
unsigned int snd_pcm_hw_params_set_period_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_set_period_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
snd_pcm_sframes_t snd_pcm_hw_params_get_period_size(const snd_pcm_hw_params_t *params, int *dir);
snd_pcm_uframes_t snd_pcm_hw_params_get_period_size_min(const snd_pcm_hw_params_t *params, int *dir);
snd_pcm_uframes_t snd_pcm_hw_params_get_period_size_max(const snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_test_period_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int dir);
int snd_pcm_hw_params_set_period_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int dir);
int snd_pcm_hw_params_set_period_size_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir);
int snd_pcm_hw_params_set_period_size_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir);
int snd_pcm_hw_params_set_period_size_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *min, int *mindir, snd_pcm_uframes_t *max, int *maxdir);
snd_pcm_uframes_t snd_pcm_hw_params_set_period_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int *dir);
snd_pcm_uframes_t snd_pcm_hw_params_set_period_size_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
snd_pcm_uframes_t snd_pcm_hw_params_set_period_size_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_set_period_size_integer(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_get_periods(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_periods_min(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_periods_max(const snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_test_periods(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_periods(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_periods_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_periods_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_periods_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir);
unsigned int snd_pcm_hw_params_set_periods_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir);
unsigned int snd_pcm_hw_params_set_periods_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_set_periods_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_set_periods_integer(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_get_buffer_time(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_buffer_time_min(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_buffer_time_max(const snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_test_buffer_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_buffer_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_buffer_time_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_buffer_time_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_buffer_time_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir);
unsigned int snd_pcm_hw_params_set_buffer_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir);
unsigned int snd_pcm_hw_params_set_buffer_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_set_buffer_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
snd_pcm_sframes_t snd_pcm_hw_params_get_buffer_size(const snd_pcm_hw_params_t *params);
snd_pcm_uframes_t snd_pcm_hw_params_get_buffer_size_min(const snd_pcm_hw_params_t *params);
snd_pcm_uframes_t snd_pcm_hw_params_get_buffer_size_max(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_test_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val);
int snd_pcm_hw_params_set_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val);
int snd_pcm_hw_params_set_buffer_size_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val);
int snd_pcm_hw_params_set_buffer_size_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val);
int snd_pcm_hw_params_set_buffer_size_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *min, snd_pcm_uframes_t *max);
snd_pcm_uframes_t snd_pcm_hw_params_set_buffer_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val);
snd_pcm_uframes_t snd_pcm_hw_params_set_buffer_size_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
snd_pcm_uframes_t snd_pcm_hw_params_set_buffer_size_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_get_tick_time(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_tick_time_min(const snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_get_tick_time_max(const snd_pcm_hw_params_t *params, int *dir);
int snd_pcm_hw_params_test_tick_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_tick_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
int snd_pcm_hw_params_set_tick_time_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_tick_time_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int snd_pcm_hw_params_set_tick_time_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir);
unsigned int snd_pcm_hw_params_set_tick_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir);
unsigned int snd_pcm_hw_params_set_tick_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);
unsigned int snd_pcm_hw_params_set_tick_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir);

size_t snd_pcm_sw_params_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_pcm_sw_params_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_pcm_sw_params_alloca(ptr) do { assert(ptr); *ptr = (snd_pcm_sw_params_t *) alloca(snd_pcm_sw_params_sizeof()); memset(*ptr, 0, snd_pcm_sw_params_sizeof()); } while (0)
int snd_pcm_sw_params_malloc(snd_pcm_sw_params_t **ptr);
void snd_pcm_sw_params_free(snd_pcm_sw_params_t *obj);
void snd_pcm_sw_params_copy(snd_pcm_sw_params_t *dst, const snd_pcm_sw_params_t *src);
int snd_pcm_sw_params_set_start_mode(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_start_t val);
snd_pcm_start_t snd_pcm_sw_params_get_start_mode(const snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_set_xrun_mode(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_xrun_t val);
snd_pcm_xrun_t snd_pcm_sw_params_get_xrun_mode(const snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_set_tstamp_mode(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_tstamp_t val);
snd_pcm_tstamp_t snd_pcm_sw_params_get_tstamp_mode(const snd_pcm_sw_params_t *params);
#if 0
int snd_pcm_sw_params_set_period_step(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, unsigned int val);
unsigned int snd_pcm_sw_params_get_period_step(const snd_pcm_sw_params_t *params);
#endif
int snd_pcm_sw_params_set_sleep_min(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, unsigned int val);
unsigned int snd_pcm_sw_params_get_sleep_min(const snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_set_avail_min(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
snd_pcm_uframes_t snd_pcm_sw_params_get_avail_min(const snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_set_xfer_align(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
snd_pcm_uframes_t snd_pcm_sw_params_get_xfer_align(const snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_set_start_threshold(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
snd_pcm_uframes_t snd_pcm_sw_params_get_start_threshold(const snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_set_stop_threshold(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
snd_pcm_uframes_t snd_pcm_sw_params_get_stop_threshold(const snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_set_silence_threshold(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
snd_pcm_uframes_t snd_pcm_sw_params_get_silence_threshold(const snd_pcm_sw_params_t *params);
int snd_pcm_sw_params_set_silence_size(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
snd_pcm_uframes_t snd_pcm_sw_params_get_silence_size(const snd_pcm_sw_params_t *params);

size_t snd_pcm_status_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_pcm_status_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_pcm_status_alloca(ptr) do { assert(ptr); *ptr = (snd_pcm_status_t *) alloca(snd_pcm_status_sizeof()); memset(*ptr, 0, snd_pcm_status_sizeof()); } while (0)
int snd_pcm_status_malloc(snd_pcm_status_t **ptr);
void snd_pcm_status_free(snd_pcm_status_t *obj);
void snd_pcm_status_copy(snd_pcm_status_t *dst, const snd_pcm_status_t *src);
snd_pcm_state_t snd_pcm_status_get_state(const snd_pcm_status_t *obj);
void snd_pcm_status_get_trigger_tstamp(const snd_pcm_status_t *obj, snd_timestamp_t *ptr);
void snd_pcm_status_get_tstamp(const snd_pcm_status_t *obj, snd_timestamp_t *ptr);
snd_pcm_sframes_t snd_pcm_status_get_delay(const snd_pcm_status_t *obj);
snd_pcm_uframes_t snd_pcm_status_get_avail(const snd_pcm_status_t *obj);
snd_pcm_uframes_t snd_pcm_status_get_avail_max(const snd_pcm_status_t *obj);

size_t snd_pcm_info_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_pcm_info_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_pcm_info_alloca(ptr) do { assert(ptr); *ptr = (snd_pcm_info_t *) alloca(snd_pcm_info_sizeof()); memset(*ptr, 0, snd_pcm_info_sizeof()); } while (0)
int snd_pcm_info_malloc(snd_pcm_info_t **ptr);
void snd_pcm_info_free(snd_pcm_info_t *obj);
void snd_pcm_info_copy(snd_pcm_info_t *dst, const snd_pcm_info_t *src);
unsigned int snd_pcm_info_get_device(const snd_pcm_info_t *obj);
unsigned int snd_pcm_info_get_subdevice(const snd_pcm_info_t *obj);
snd_pcm_stream_t snd_pcm_info_get_stream(const snd_pcm_info_t *obj);
int snd_pcm_info_get_card(const snd_pcm_info_t *obj);
const char *snd_pcm_info_get_id(const snd_pcm_info_t *obj);
const char *snd_pcm_info_get_name(const snd_pcm_info_t *obj);
const char *snd_pcm_info_get_subdevice_name(const snd_pcm_info_t *obj);
snd_pcm_class_t snd_pcm_info_get_class(const snd_pcm_info_t *obj);
snd_pcm_subclass_t snd_pcm_info_get_subclass(const snd_pcm_info_t *obj);
unsigned int snd_pcm_info_get_subdevices_count(const snd_pcm_info_t *obj);
unsigned int snd_pcm_info_get_subdevices_avail(const snd_pcm_info_t *obj);
void snd_pcm_info_set_device(snd_pcm_info_t *obj, unsigned int val);
void snd_pcm_info_set_subdevice(snd_pcm_info_t *obj, unsigned int val);
void snd_pcm_info_set_stream(snd_pcm_info_t *obj, snd_pcm_stream_t val);

/** type of pcm hook */
typedef enum _snd_pcm_hook_type {
	SND_PCM_HOOK_TYPE_HW_PARAMS,
	SND_PCM_HOOK_TYPE_HW_FREE,
	SND_PCM_HOOK_TYPE_CLOSE,
	SND_PCM_HOOK_TYPE_LAST = SND_PCM_HOOK_TYPE_CLOSE,
} snd_pcm_hook_type_t;

/** PCM hook container */
typedef struct _snd_pcm_hook snd_pcm_hook_t;
/** PCM hook callback function */
typedef int (*snd_pcm_hook_func_t)(snd_pcm_hook_t *hook);
snd_pcm_t *snd_pcm_hook_get_pcm(snd_pcm_hook_t *hook);
void *snd_pcm_hook_get_private(snd_pcm_hook_t *hook);
void snd_pcm_hook_set_private(snd_pcm_hook_t *hook, void *private_data);
int snd_pcm_hook_add(snd_pcm_hook_t **hookp, snd_pcm_t *pcm,
		     snd_pcm_hook_type_t type,
		     snd_pcm_hook_func_t func, void *private_data);
int snd_pcm_hook_remove(snd_pcm_hook_t *hook);

#ifdef __cplusplus
}
#endif

/** \} */

