/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

typedef struct _snd_pcm_info snd_pcm_info_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
typedef struct _snd_pcm_sw_params snd_pcm_sw_params_t;
typedef struct _snd_pcm_status snd_pcm_status_t;
typedef struct _snd_pcm_access_mask snd_pcm_access_mask_t;
typedef struct _snd_pcm_format_mask snd_pcm_format_mask_t;
typedef struct _snd_pcm_subformat_mask snd_pcm_subformat_mask_t;

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_pcm_class *snd_pcm_class_t;
typedef struct __snd_pcm_subclass *snd_pcm_subclass_t;
typedef struct __snd_pcm_stream *snd_pcm_stream_t;
typedef struct __snd_pcm_access *snd_pcm_access_t;
typedef struct __snd_pcm_format *snd_pcm_format_t;
typedef struct __snd_pcm_subformat *snd_pcm_subformat_t;
typedef struct __snd_pcm_state *snd_pcm_state_t;
typedef struct __snd_pcm_start *snd_pcm_start_t;
typedef struct __snd_pcm_xrun *snd_pcm_xrun_t;
typedef struct __snd_pcm_tstamp *snd_pcm_tstamp_t;
#else
typedef enum sndrv_pcm_class snd_pcm_class_t;
typedef enum sndrv_pcm_subclass snd_pcm_subclass_t;
typedef enum sndrv_pcm_stream snd_pcm_stream_t;
typedef enum sndrv_pcm_access snd_pcm_access_t;
typedef enum sndrv_pcm_format snd_pcm_format_t;
typedef enum sndrv_pcm_subformat snd_pcm_subformat_t;
typedef enum sndrv_pcm_state snd_pcm_state_t;
typedef enum sndrv_pcm_start snd_pcm_start_t;
typedef enum sndrv_pcm_xrun snd_pcm_xrun_t;
typedef enum sndrv_pcm_tstamp snd_pcm_tstamp_t;
#endif

#define SND_PCM_CLASS_GENERIC ((snd_pcm_class_t) SNDRV_PCM_CLASS_GENERIC)
#define SND_PCM_CLASS_MULTI ((snd_pcm_class_t) SNDRV_PCM_CLASS_MULTI)
#define SND_PCM_CLASS_MODEM ((snd_pcm_class_t) SNDRV_PCM_CLASS_MODEM)
#define SND_PCM_CLASS_DIGITIZER ((snd_pcm_class_t) SNDRV_PCM_CLASS_DIGITIZER)
#define SND_PCM_CLASS_LAST ((snd_pcm_class_t) SNDRV_PCM_CLASS_LAST)

#define SND_PCM_SUBCLASS_GENERIC_MIX ((snd_pcm_subclass_t) SNDRV_PCM_SUBCLASS_GENERIC_MIX)
#define SND_PCM_SUBCLASS_MULTI_MIX ((snd_pcm_subclass_t) SNDRV_PCM_SUBCLASS_MULTI_MIX)
#define SND_PCM_SUBCLASS_LAST ((snd_pcm_subclass_t) SNDRV_PCM_SUBCLASS_LAST)

#define SND_PCM_STREAM_PLAYBACK ((snd_pcm_stream_t) SNDRV_PCM_STREAM_PLAYBACK)
#define SND_PCM_STREAM_CAPTURE ((snd_pcm_stream_t) SNDRV_PCM_STREAM_CAPTURE)
#define SND_PCM_STREAM_LAST ((snd_pcm_stream_t) SNDRV_PCM_STREAM_LAST)

#define SND_PCM_ACCESS_MMAP_INTERLEAVED ((snd_pcm_access_t) SNDRV_PCM_ACCESS_MMAP_INTERLEAVED)
#define SND_PCM_ACCESS_MMAP_NONINTERLEAVED ((snd_pcm_access_t) SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED)
#define SND_PCM_ACCESS_MMAP_COMPLEX ((snd_pcm_access_t) SNDRV_PCM_ACCESS_MMAP_COMPLEX)
#define SND_PCM_ACCESS_RW_INTERLEAVED ((snd_pcm_access_t) SNDRV_PCM_ACCESS_RW_INTERLEAVED)
#define SND_PCM_ACCESS_RW_NONINTERLEAVED ((snd_pcm_access_t) SNDRV_PCM_ACCESS_RW_NONINTERLEAVED)
#define SND_PCM_ACCESS_LAST ((snd_pcm_access_t) SNDRV_PCM_ACCESS_LAST)

#define SND_PCM_FORMAT_UNKNOWN ((snd_pcm_format_t) -1)
#define SND_PCM_FORMAT_S8 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S8)
#define SND_PCM_FORMAT_U8 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U8)
#define SND_PCM_FORMAT_S16_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S16_LE)
#define SND_PCM_FORMAT_S16_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S16_BE)
#define SND_PCM_FORMAT_U16_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U16_LE)
#define SND_PCM_FORMAT_U16_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U16_BE)
#define SND_PCM_FORMAT_S24_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S24_LE)
#define SND_PCM_FORMAT_S24_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S24_BE)
#define SND_PCM_FORMAT_U24_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U24_LE)
#define SND_PCM_FORMAT_U24_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U24_BE)
#define SND_PCM_FORMAT_S32_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S32_LE)
#define SND_PCM_FORMAT_S32_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S32_BE)
#define SND_PCM_FORMAT_U32_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U32_LE)
#define SND_PCM_FORMAT_U32_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U32_BE)
#define SND_PCM_FORMAT_FLOAT_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_FLOAT_LE)
#define SND_PCM_FORMAT_FLOAT_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_FLOAT_BE)
#define SND_PCM_FORMAT_FLOAT64_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_FLOAT64_LE)
#define SND_PCM_FORMAT_FLOAT64_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_FLOAT64_BE)
#define SND_PCM_FORMAT_IEC958_SUBFRAME_LE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE)
#define SND_PCM_FORMAT_IEC958_SUBFRAME_BE ((snd_pcm_format_t) SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE)
#define SND_PCM_FORMAT_MU_LAW ((snd_pcm_format_t) SNDRV_PCM_FORMAT_MU_LAW)
#define SND_PCM_FORMAT_A_LAW ((snd_pcm_format_t) SNDRV_PCM_FORMAT_A_LAW)
#define SND_PCM_FORMAT_IMA_ADPCM ((snd_pcm_format_t) SNDRV_PCM_FORMAT_IMA_ADPCM)
#define SND_PCM_FORMAT_MPEG ((snd_pcm_format_t) SNDRV_PCM_FORMAT_MPEG)
#define SND_PCM_FORMAT_GSM ((snd_pcm_format_t) SNDRV_PCM_FORMAT_GSM)
#define SND_PCM_FORMAT_SPECIAL ((snd_pcm_format_t) SNDRV_PCM_FORMAT_SPECIAL)
#define SND_PCM_FORMAT_LAST ((snd_pcm_format_t) SNDRV_PCM_FORMAT_LAST)
#define SND_PCM_FORMAT_S16 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S16)
#define SND_PCM_FORMAT_U16 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U16)
#define SND_PCM_FORMAT_S24 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S24)
#define SND_PCM_FORMAT_U24 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U24)
#define SND_PCM_FORMAT_S32 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S32)
#define SND_PCM_FORMAT_U32 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U32)
#define SND_PCM_FORMAT_FLOAT ((snd_pcm_format_t) SNDRV_PCM_FORMAT_FLOAT)
#define SND_PCM_FORMAT_FLOAT64 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_FLOAT64)
#define SND_PCM_FORMAT_IEC958_SUBFRAME ((snd_pcm_format_t) SNDRV_PCM_FORMAT_IEC958_SUBFRAME)
#define SND_PCM_FORMAT_S16 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S16)
#define SND_PCM_FORMAT_U16 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U16)
#define SND_PCM_FORMAT_S24 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S24)
#define SND_PCM_FORMAT_U24 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U24)
#define SND_PCM_FORMAT_S32 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_S32)
#define SND_PCM_FORMAT_U32 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_U32)
#define SND_PCM_FORMAT_FLOAT ((snd_pcm_format_t) SNDRV_PCM_FORMAT_FLOAT)
#define SND_PCM_FORMAT_FLOAT64 ((snd_pcm_format_t) SNDRV_PCM_FORMAT_FLOAT64)
#define SND_PCM_FORMAT_IEC958_SUBFRAME ((snd_pcm_format_t) SNDRV_PCM_FORMAT_IEC958_SUBFRAME)

#define SND_PCM_SUBFORMAT_STD ((snd_pcm_subformat_t) SNDRV_PCM_SUBFORMAT_STD)
#define SND_PCM_SUBFORMAT_LAST ((snd_pcm_subformat_t) SNDRV_PCM_SUBFORMAT_LAST)

#define SND_PCM_STATE_OPEN ((snd_pcm_state_t) SNDRV_PCM_STATE_OPEN)
#define SND_PCM_STATE_SETUP ((snd_pcm_state_t) SNDRV_PCM_STATE_SETUP)
#define SND_PCM_STATE_PREPARED ((snd_pcm_state_t) SNDRV_PCM_STATE_PREPARED)
#define SND_PCM_STATE_RUNNING ((snd_pcm_state_t) SNDRV_PCM_STATE_RUNNING)
#define SND_PCM_STATE_XRUN ((snd_pcm_state_t) SNDRV_PCM_STATE_XRUN)
#define SND_PCM_STATE_DRAINING ((snd_pcm_state_t) SNDRV_PCM_STATE_DRAINING)
#define SND_PCM_STATE_PAUSED ((snd_pcm_state_t) SNDRV_PCM_STATE_PAUSED)
#define SND_PCM_STATE_LAST ((snd_pcm_state_t) SNDRV_PCM_STATE_LAST)

#define SND_PCM_START_DATA ((snd_pcm_start_t) SNDRV_PCM_START_DATA)
#define SND_PCM_START_EXPLICIT ((snd_pcm_start_t) SNDRV_PCM_START_EXPLICIT)
#define SND_PCM_START_LAST ((snd_pcm_start_t) SNDRV_PCM_START_LAST)

#define SND_PCM_XRUN_NONE ((snd_pcm_xrun_t) SNDRV_PCM_XRUN_NONE)
#define SND_PCM_XRUN_STOP ((snd_pcm_xrun_t) SNDRV_PCM_XRUN_STOP)
#define SND_PCM_XRUN_LAST ((snd_pcm_xrun_t) SNDRV_PCM_XRUN_LAST)

#define SND_PCM_TSTAMP_NONE ((snd_pcm_tstamp_t) SNDRV_PCM_TSTAMP_NONE)
#define SND_PCM_TSTAMP_MMAP ((snd_pcm_tstamp_t) SNDRV_PCM_TSTAMP_MMAP)
#define SND_PCM_TSTAMP_LAST ((snd_pcm_tstamp_t) SNDRV_PCM_TSTAMP_LAST)

typedef sndrv_pcm_uframes_t snd_pcm_uframes_t;
typedef sndrv_pcm_sframes_t snd_pcm_sframes_t;
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
#define SND_PCM_MMAP_OFFSET_DATA SNDRV_PCM_MMAP_OFFSET_DATA
#define SND_PCM_MMAP_OFFSET_STATUS SNDRV_PCM_MMAP_OFFSET_STATUS
#define SND_PCM_MMAP_OFFSET_CONTROL SNDRV_PCM_MMAP_OFFSET_CONTROL

#define SND_PCM_NONBLOCK		0x0001
#define SND_PCM_ASYNC			0x0002

typedef struct _snd_pcm snd_pcm_t;

enum _snd_pcm_type {
	SND_PCM_TYPE_HW,
	SND_PCM_TYPE_MULTI,
	SND_PCM_TYPE_FILE,
	SND_PCM_TYPE_NULL,
	SND_PCM_TYPE_SHM,
	SND_PCM_TYPE_INET,
	SND_PCM_TYPE_COPY,
	SND_PCM_TYPE_LINEAR,
	SND_PCM_TYPE_ALAW,
	SND_PCM_TYPE_MULAW,
	SND_PCM_TYPE_ADPCM,
	SND_PCM_TYPE_RATE,
	SND_PCM_TYPE_ROUTE,
	SND_PCM_TYPE_PLUG,
	SND_PCM_TYPE_SHARE,
	SND_PCM_TYPE_METER,
	SND_PCM_TYPE_MIX,
	SND_PCM_TYPE_DROUTE,
	SND_PCM_TYPE_LBSERVER,
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_pcm_type *snd_pcm_type_t;
#else
typedef enum _snd_pcm_type snd_pcm_type_t;
#endif

#define	SND_PCM_TYPE_HW ((snd_pcm_type_t) SND_PCM_TYPE_HW)
#define	SND_PCM_TYPE_MULTI ((snd_pcm_type_t) SND_PCM_TYPE_MULTI)
#define	SND_PCM_TYPE_FILE ((snd_pcm_type_t) SND_PCM_TYPE_FILE)
#define	SND_PCM_TYPE_NULL ((snd_pcm_type_t) SND_PCM_TYPE_NULL)
#define	SND_PCM_TYPE_SHM ((snd_pcm_type_t) SND_PCM_TYPE_SHM)
#define	SND_PCM_TYPE_INET ((snd_pcm_type_t) SND_PCM_TYPE_INET)
#define	SND_PCM_TYPE_COPY ((snd_pcm_type_t) SND_PCM_TYPE_COPY)
#define	SND_PCM_TYPE_LINEAR ((snd_pcm_type_t) SND_PCM_TYPE_LINEAR)
#define	SND_PCM_TYPE_ALAW ((snd_pcm_type_t) SND_PCM_TYPE_ALAW)
#define	SND_PCM_TYPE_MULAW ((snd_pcm_type_t) SND_PCM_TYPE_MULAW)
#define	SND_PCM_TYPE_ADPCM ((snd_pcm_type_t) SND_PCM_TYPE_ADPCM)
#define	SND_PCM_TYPE_RATE ((snd_pcm_type_t) SND_PCM_TYPE_RATE)
#define	SND_PCM_TYPE_ROUTE ((snd_pcm_type_t) SND_PCM_TYPE_ROUTE)
#define	SND_PCM_TYPE_PLUG ((snd_pcm_type_t) SND_PCM_TYPE_PLUG)
#define	SND_PCM_TYPE_SHARE ((snd_pcm_type_t) SND_PCM_TYPE_SHARE)
#define	SND_PCM_TYPE_METER ((snd_pcm_type_t) SND_PCM_TYPE_METER)
#define	SND_PCM_TYPE_MIX ((snd_pcm_type_t) SND_PCM_TYPE_MIX)
#define	SND_PCM_TYPE_DROUTE ((snd_pcm_type_t) SND_PCM_TYPE_DROUTE)
#define	SND_PCM_TYPE_LBSERVER ((snd_pcm_type_t) SND_PCM_TYPE_LBSERVER)

typedef struct _snd_pcm_channel_area {
	void *addr;			/* base address of channel samples */
	unsigned int first;		/* offset to first sample in bits */
	unsigned int step;		/* samples distance in bits */
} snd_pcm_channel_area_t;

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

int snd_pcm_hw_params_try_explain_failure(snd_pcm_t *pcm,
					  snd_pcm_hw_params_t *fail,
					  snd_pcm_hw_params_t *success,
					  unsigned int depth,
					  snd_output_t *out);

int snd_pcm_hw_params_get_rate_numden(const snd_pcm_hw_params_t *params,
				      unsigned int *rate_num,
				      unsigned int *rate_den);
int snd_pcm_hw_params_get_sbits(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_get_fifo_size(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_dump(snd_pcm_hw_params_t *params, snd_output_t *out);

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

