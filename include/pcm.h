/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

/* sndrv aliasing */
typedef enum sndrv_pcm_class snd_pcm_class_t;
typedef enum sndrv_pcm_subclass snd_pcm_subclass_t;
typedef enum sndrv_pcm_stream snd_pcm_stream_t;
typedef enum sndrv_pcm_access snd_pcm_access_t;
typedef enum sndrv_pcm_format snd_pcm_format_t;
typedef enum sndrv_pcm_subformat snd_pcm_subformat_t;
typedef enum sndrv_pcm_state snd_pcm_state_t;
typedef enum sndrv_pcm_hw_param snd_pcm_hw_param_t;
typedef enum sndrv_pcm_start snd_pcm_start_t;
typedef enum sndrv_pcm_xrun snd_pcm_xrun_t;
typedef enum sndrv_pcm_tstamp snd_pcm_tstamp_t;

typedef sndrv_pcm_uframes_t snd_pcm_uframes_t;
typedef sndrv_pcm_sframes_t snd_pcm_sframes_t;
typedef struct timeval snd_timestamp_t;

typedef struct _snd_pcm_info snd_pcm_info_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
typedef struct _snd_pcm_sw_params snd_pcm_sw_params_t;
typedef struct _snd_pcm_status snd_pcm_status_t;
#define SND_PCM_CLASS_GENERIC SNDRV_PCM_CLASS_GENERIC
#define SND_PCM_CLASS_MULTI SNDRV_PCM_CLASS_MULTI
#define SND_PCM_CLASS_MODEM SNDRV_PCM_CLASS_MODEM
#define SND_PCM_CLASS_DIGITIZER SNDRV_PCM_CLASS_DIGITIZER
#define SND_PCM_SUBCLASS_GENERIC_MIX SNDRV_PCM_SUBCLASS_GENERIC_MIX
#define SND_PCM_SUBCLASS_MULTI_MIX SNDRV_PCM_SUBCLASS_MULTI_MIX
#define SND_PCM_STREAM_PLAYBACK SNDRV_PCM_STREAM_PLAYBACK
#define SND_PCM_STREAM_CAPTURE SNDRV_PCM_STREAM_CAPTURE
#define SND_PCM_STREAM_LAST SNDRV_PCM_STREAM_LAST
#define SND_PCM_ACCESS_MMAP_INTERLEAVED SNDRV_PCM_ACCESS_MMAP_INTERLEAVED
#define SND_PCM_ACCESS_MMAP_NONINTERLEAVED SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED
#define SND_PCM_ACCESS_MMAP_COMPLEX SNDRV_PCM_ACCESS_MMAP_COMPLEX
#define SND_PCM_ACCESS_RW_INTERLEAVED SNDRV_PCM_ACCESS_RW_INTERLEAVED
#define SND_PCM_ACCESS_RW_NONINTERLEAVED SNDRV_PCM_ACCESS_RW_NONINTERLEAVED
#define SND_PCM_ACCESS_LAST SNDRV_PCM_ACCESS_LAST
#define SND_PCM_FORMAT_S8 SNDRV_PCM_FORMAT_S8
#define SND_PCM_FORMAT_U8 SNDRV_PCM_FORMAT_U8
#define SND_PCM_FORMAT_S16_LE SNDRV_PCM_FORMAT_S16_LE
#define SND_PCM_FORMAT_S16_BE SNDRV_PCM_FORMAT_S16_BE
#define SND_PCM_FORMAT_U16_LE SNDRV_PCM_FORMAT_U16_LE
#define SND_PCM_FORMAT_U16_BE SNDRV_PCM_FORMAT_U16_BE
#define SND_PCM_FORMAT_S24_LE SNDRV_PCM_FORMAT_S24_LE
#define SND_PCM_FORMAT_S24_BE SNDRV_PCM_FORMAT_S24_BE
#define SND_PCM_FORMAT_U24_LE SNDRV_PCM_FORMAT_U24_LE
#define SND_PCM_FORMAT_U24_BE SNDRV_PCM_FORMAT_U24_BE
#define SND_PCM_FORMAT_S32_LE SNDRV_PCM_FORMAT_S32_LE
#define SND_PCM_FORMAT_S32_BE SNDRV_PCM_FORMAT_S32_BE
#define SND_PCM_FORMAT_U32_LE SNDRV_PCM_FORMAT_U32_LE
#define SND_PCM_FORMAT_U32_BE SNDRV_PCM_FORMAT_U32_BE
#define SND_PCM_FORMAT_FLOAT_LE SNDRV_PCM_FORMAT_FLOAT_LE
#define SND_PCM_FORMAT_FLOAT_BE SNDRV_PCM_FORMAT_FLOAT_BE
#define SND_PCM_FORMAT_FLOAT64_LE SNDRV_PCM_FORMAT_FLOAT64_LE
#define SND_PCM_FORMAT_FLOAT64_BE SNDRV_PCM_FORMAT_FLOAT64_BE
#define SND_PCM_FORMAT_IEC958_SUBFRAME_LE SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE
#define SND_PCM_FORMAT_IEC958_SUBFRAME_BE SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE
#define SND_PCM_FORMAT_MU_LAW SNDRV_PCM_FORMAT_MU_LAW
#define SND_PCM_FORMAT_A_LAW SNDRV_PCM_FORMAT_A_LAW
#define SND_PCM_FORMAT_IMA_ADPCM SNDRV_PCM_FORMAT_IMA_ADPCM
#define SND_PCM_FORMAT_MPEG SNDRV_PCM_FORMAT_MPEG
#define SND_PCM_FORMAT_GSM SNDRV_PCM_FORMAT_GSM
#define SND_PCM_FORMAT_SPECIAL SNDRV_PCM_FORMAT_SPECIAL
#define SND_PCM_FORMAT_LAST SNDRV_PCM_FORMAT_LAST
#define SND_PCM_FORMAT_S16 SNDRV_PCM_FORMAT_S16
#define SND_PCM_FORMAT_U16 SNDRV_PCM_FORMAT_U16
#define SND_PCM_FORMAT_S24 SNDRV_PCM_FORMAT_S24
#define SND_PCM_FORMAT_U24 SNDRV_PCM_FORMAT_U24
#define SND_PCM_FORMAT_S32 SNDRV_PCM_FORMAT_S32
#define SND_PCM_FORMAT_U32 SNDRV_PCM_FORMAT_U32
#define SND_PCM_FORMAT_FLOAT SNDRV_PCM_FORMAT_FLOAT
#define SND_PCM_FORMAT_FLOAT64 SNDRV_PCM_FORMAT_FLOAT64
#define SND_PCM_FORMAT_IEC958_SUBFRAME SNDRV_PCM_FORMAT_IEC958_SUBFRAME
#define SND_PCM_FORMAT_S16 SNDRV_PCM_FORMAT_S16
#define SND_PCM_FORMAT_U16 SNDRV_PCM_FORMAT_U16
#define SND_PCM_FORMAT_S24 SNDRV_PCM_FORMAT_S24
#define SND_PCM_FORMAT_U24 SNDRV_PCM_FORMAT_U24
#define SND_PCM_FORMAT_S32 SNDRV_PCM_FORMAT_S32
#define SND_PCM_FORMAT_U32 SNDRV_PCM_FORMAT_U32
#define SND_PCM_FORMAT_FLOAT SNDRV_PCM_FORMAT_FLOAT
#define SND_PCM_FORMAT_FLOAT64 SNDRV_PCM_FORMAT_FLOAT64
#define SND_PCM_FORMAT_IEC958_SUBFRAME SNDRV_PCM_FORMAT_IEC958_SUBFRAME
#define SND_PCM_SUBFORMAT_STD SNDRV_PCM_SUBFORMAT_STD
#define SND_PCM_SUBFORMAT_LAST SNDRV_PCM_SUBFORMAT_LAST
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
#define SND_PCM_STATE_OPEN SNDRV_PCM_STATE_OPEN
#define SND_PCM_STATE_SETUP SNDRV_PCM_STATE_SETUP
#define SND_PCM_STATE_PREPARED SNDRV_PCM_STATE_PREPARED
#define SND_PCM_STATE_RUNNING SNDRV_PCM_STATE_RUNNING
#define SND_PCM_STATE_XRUN SNDRV_PCM_STATE_XRUN
#define SND_PCM_STATE_DRAINING SNDRV_PCM_STATE_DRAINING
#define SND_PCM_STATE_PAUSED SNDRV_PCM_STATE_PAUSED
#define SND_PCM_STATE_LAST SNDRV_PCM_STATE_LAST
#define SND_PCM_MMAP_OFFSET_DATA SNDRV_PCM_MMAP_OFFSET_DATA
#define SND_PCM_MMAP_OFFSET_STATUS SNDRV_PCM_MMAP_OFFSET_STATUS
#define SND_PCM_MMAP_OFFSET_CONTROL SNDRV_PCM_MMAP_OFFSET_CONTROL
#define SND_PCM_HW_PARAM_ACCESS SNDRV_PCM_HW_PARAM_ACCESS
#define SND_PCM_HW_PARAM_FIRST_MASK SNDRV_PCM_HW_PARAM_FIRST_MASK
#define SND_PCM_HW_PARAM_FORMAT SNDRV_PCM_HW_PARAM_FORMAT
#define SND_PCM_HW_PARAM_SUBFORMAT SNDRV_PCM_HW_PARAM_SUBFORMAT
#define SND_PCM_HW_PARAM_LAST_MASK SNDRV_PCM_HW_PARAM_LAST_MASK
#define SND_PCM_HW_PARAM_SAMPLE_BITS SNDRV_PCM_HW_PARAM_SAMPLE_BITS
#define SND_PCM_HW_PARAM_FIRST_INTERVAL SNDRV_PCM_HW_PARAM_FIRST_INTERVAL
#define SND_PCM_HW_PARAM_FRAME_BITS SNDRV_PCM_HW_PARAM_FRAME_BITS
#define SND_PCM_HW_PARAM_CHANNELS SNDRV_PCM_HW_PARAM_CHANNELS
#define SND_PCM_HW_PARAM_RATE SNDRV_PCM_HW_PARAM_RATE
#define SND_PCM_HW_PARAM_PERIOD_TIME SNDRV_PCM_HW_PARAM_PERIOD_TIME
#define SND_PCM_HW_PARAM_PERIOD_SIZE SNDRV_PCM_HW_PARAM_PERIOD_SIZE
#define SND_PCM_HW_PARAM_PERIOD_BYTES SNDRV_PCM_HW_PARAM_PERIOD_BYTES
#define SND_PCM_HW_PARAM_PERIODS SNDRV_PCM_HW_PARAM_PERIODS
#define SND_PCM_HW_PARAM_BUFFER_TIME SNDRV_PCM_HW_PARAM_BUFFER_TIME
#define SND_PCM_HW_PARAM_BUFFER_SIZE SNDRV_PCM_HW_PARAM_BUFFER_SIZE
#define SND_PCM_HW_PARAM_BUFFER_BYTES SNDRV_PCM_HW_PARAM_BUFFER_BYTES
#define SND_PCM_HW_PARAM_TICK_TIME SNDRV_PCM_HW_PARAM_TICK_TIME
#define SND_PCM_HW_PARAM_LAST_INTERVAL SNDRV_PCM_HW_PARAM_LAST_INTERVAL
#define SND_PCM_HW_PARAM_LAST SNDRV_PCM_HW_PARAM_LAST
#define SND_PCM_HW_PARAMS_RUNTIME SNDRV_PCM_HW_PARAMS_RUNTIME
#define SND_PCM_HW_PARAM_LAST_MASK SNDRV_PCM_HW_PARAM_LAST_MASK
#define SND_PCM_HW_PARAM_FIRST_MASK SNDRV_PCM_HW_PARAM_FIRST_MASK
#define SND_PCM_HW_PARAM_LAST_INTERVAL SNDRV_PCM_HW_PARAM_LAST_INTERVAL
#define SND_PCM_HW_PARAM_FIRST_INTERVAL SNDRV_PCM_HW_PARAM_FIRST_INTERVAL
#define SND_PCM_START_DATA SNDRV_PCM_START_DATA
#define SND_PCM_START_EXPLICIT SNDRV_PCM_START_EXPLICIT
#define SND_PCM_START_LAST SNDRV_PCM_START_LAST
#define SND_PCM_XRUN_NONE SNDRV_PCM_XRUN_NONE
#define SND_PCM_XRUN_STOP SNDRV_PCM_XRUN_STOP
#define SND_PCM_XRUN_LAST SNDRV_PCM_XRUN_LAST
#define SND_PCM_TSTAMP_NONE SNDRV_PCM_TSTAMP_NONE
#define SND_PCM_TSTAMP_MMAP SNDRV_PCM_TSTAMP_MMAP
#define SND_PCM_TSTAMP_LAST SNDRV_PCM_TSTAMP_LAST
#define SND_PCM_STATE_XXXX SNDRV_PCM_STATE_XXXX

#define SND_PCM_NONBLOCK		0x0001
#define SND_PCM_ASYNC			0x0002

typedef struct _snd_mask snd_mask_t;
typedef struct _snd_pcm snd_pcm_t;

typedef enum _snd_pcm_type {
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
	SND_PCM_TYPE_MIX,
	SND_PCM_TYPE_DROUTE,
	SND_PCM_TYPE_LBSERVER,
} snd_pcm_type_t;

typedef struct _snd_pcm_channel_area {
	void *addr;			/* base address of channel samples */
	unsigned int first;		/* offset to first sample in bits */
	unsigned int step;		/* samples distance in bits */
} snd_pcm_channel_area_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_pcm_open(snd_pcm_t **pcm, char *name, 
		 int stream, int mode);

/* Obsolete functions */
#define snd_pcm_write snd_pcm_writei
#define snd_pcm_read snd_pcm_readi
snd_pcm_sframes_t snd_pcm_writev(snd_pcm_t *pcm, const struct iovec *vector, int count);
snd_pcm_sframes_t snd_pcm_readv(snd_pcm_t *pcm, const struct iovec *vector, int count);


snd_pcm_type_t snd_pcm_type(snd_pcm_t *pcm);
int snd_pcm_close(snd_pcm_t *pcm);
int snd_pcm_poll_descriptor(snd_pcm_t *pcm);
int snd_pcm_nonblock(snd_pcm_t *pcm, int nonblock);
int snd_pcm_async(snd_pcm_t *pcm, int sig, pid_t pid);
int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t *info);
int snd_pcm_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
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
int snd_pcm_state(snd_pcm_t *pcm);
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
int snd_pcm_set_avail_min(snd_pcm_t *pcm, snd_pcm_uframes_t size);


/* Mask */
size_t snd_mask_sizeof();
#define snd_mask_alloca(maskp) ({(*maskp) = alloca(snd_mask_sizeof()); 0;})
int snd_mask_malloc(snd_mask_t **maskp);
void snd_mask_free(snd_mask_t *mask);
void snd_mask_none(snd_mask_t *mask);
void snd_mask_any(snd_mask_t *mask);
void snd_mask_set(snd_mask_t *mask, unsigned int val);
void snd_mask_reset(snd_mask_t *mask, unsigned int val);
void snd_mask_copy(snd_mask_t *dst, const snd_mask_t *src);

/* HW params */
size_t snd_pcm_hw_params_sizeof();
#define snd_pcm_hw_params_alloca(paramsp) ({(*paramsp) = alloca(snd_pcm_hw_params_sizeof()); 0;})
int snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **paramsp);
int snd_pcm_hw_params_free(snd_pcm_hw_params_t *params);
void snd_pcm_hw_params_copy(snd_pcm_hw_params_t *dst, const snd_pcm_hw_params_t *src);

int snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_param_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			 snd_pcm_hw_param_t var);
int snd_pcm_hw_param_test(const snd_pcm_hw_params_t *params,
			  snd_pcm_hw_param_t var, unsigned int val);
int snd_pcm_hw_param_setinteger(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
				snd_pcm_hw_param_t var);
int snd_pcm_hw_param_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			   snd_pcm_hw_param_t var, int *dir);
int snd_pcm_hw_param_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  snd_pcm_hw_param_t var, int *dir);
int snd_pcm_hw_param_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  snd_pcm_hw_param_t var, unsigned int val,
			  int *dir);
int snd_pcm_hw_param_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			 snd_pcm_hw_param_t var,
			 unsigned int val, int *dir);
int snd_pcm_hw_param_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			 snd_pcm_hw_param_t var, unsigned int val, int *dir);
int snd_pcm_hw_param_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			    snd_pcm_hw_param_t var,
			    unsigned int *min, int *mindir,
			    unsigned int *max, int *maxdir);
int snd_pcm_hw_param_set(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			 snd_pcm_hw_param_t var, unsigned int val, int dir);
int snd_pcm_hw_param_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  snd_pcm_hw_param_t var, const snd_mask_t *mask);
int snd_pcm_hw_param_min_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			     snd_pcm_hw_param_t var, 
			     unsigned int val, int *dir);
int snd_pcm_hw_param_max_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			     snd_pcm_hw_param_t var,
			     unsigned int val, int *dir);
int snd_pcm_hw_param_minmax_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
				snd_pcm_hw_param_t var,
				unsigned int *min, int *mindir,
				unsigned int *max, int *maxdir);
int snd_pcm_hw_param_set_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			     snd_pcm_hw_param_t var, unsigned int val, int dir);
int snd_pcm_hw_param_mask_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			      snd_pcm_hw_param_t var, const snd_mask_t *mask);
int snd_pcm_hw_param_value(const snd_pcm_hw_params_t *params,
			   snd_pcm_hw_param_t var, int *dir);
unsigned int snd_pcm_hw_param_value_min(const snd_pcm_hw_params_t *params,
					snd_pcm_hw_param_t var, int *dir);
unsigned int snd_pcm_hw_param_value_max(const snd_pcm_hw_params_t *params,
					snd_pcm_hw_param_t var, int *dir);
int snd_pcm_hw_params_try_explain_failure(snd_pcm_t *pcm,
					  snd_pcm_hw_params_t *fail,
					  snd_pcm_hw_params_t *success,
					  unsigned int depth,
					  snd_output_t *out);

int snd_pcm_hw_params_info_rate(const snd_pcm_hw_params_t *params,
				unsigned int *rate_num,
				unsigned int *rate_den);
int snd_pcm_hw_params_info_msbits(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_info_flags(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_info_fifo_size(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_info_dig_groups(const snd_pcm_hw_params_t *params);
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

int snd_pcm_hw_strategy_free(snd_pcm_hw_strategy_t *strategy);
int snd_pcm_hw_strategy_simple(snd_pcm_hw_strategy_t **strategyp,
			       unsigned int badness_min,
			       unsigned int badness_max);
int snd_pcm_hw_strategy_simple_near(snd_pcm_hw_strategy_t *strategy, int order,
				    snd_pcm_hw_param_t var,
				    unsigned int best,
				    unsigned int mul);
int snd_pcm_hw_strategy_simple_choices(snd_pcm_hw_strategy_t *strategy, int order,
				       snd_pcm_hw_param_t var,
				       unsigned int count,
				       snd_pcm_hw_strategy_simple_choices_list_t *choices);

/* SW params */
typedef enum _snd_pcm_sw_param {
	SND_PCM_SW_PARAM_START_MODE,
	SND_PCM_SW_PARAM_XRUN_MODE,
	SND_PCM_SW_PARAM_TSTAMP_MODE,
	SND_PCM_SW_PARAM_PERIOD_STEP,
	SND_PCM_SW_PARAM_SLEEP_MIN,
	SND_PCM_SW_PARAM_AVAIL_MIN,
	SND_PCM_SW_PARAM_XFER_ALIGN,
	SND_PCM_SW_PARAM_SILENCE_THRESHOLD,
	SND_PCM_SW_PARAM_SILENCE_SIZE,
	SND_PCM_SW_PARAM_LAST = SND_PCM_SW_PARAM_SILENCE_SIZE,
} snd_pcm_sw_param_t;

size_t snd_pcm_sw_params_sizeof();
#define snd_pcm_sw_params_alloca(paramsp) ({(*paramsp) = alloca(snd_pcm_sw_params_sizeof()); 0;})
int snd_pcm_sw_params_malloc(snd_pcm_sw_params_t **paramsp);
int snd_pcm_sw_params_free(snd_pcm_sw_params_t *params);
void snd_pcm_sw_params_copy(snd_pcm_sw_params_t *dst, const snd_pcm_sw_params_t *src);

int snd_pcm_sw_params_current(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int snd_pcm_sw_param_set(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_sw_param_t var, unsigned int val);
int snd_pcm_sw_param_near(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_sw_param_t var, unsigned int val);
int snd_pcm_sw_param_value(snd_pcm_sw_params_t *params, snd_pcm_sw_param_t var);
int snd_pcm_sw_params_dump(snd_pcm_sw_params_t *params, snd_output_t *out);

/* Info */
size_t snd_pcm_info_sizeof();
#define snd_pcm_info_alloca(infop) ({(*infop) = alloca(snd_pcm_info_sizeof()); 0;})
int snd_pcm_info_malloc(snd_pcm_info_t **infop);
int snd_pcm_info_free(snd_pcm_info_t *info);
void snd_pcm_info_copy(snd_pcm_info_t *dst, const snd_pcm_info_t *src);
void snd_pcm_info_set_device(snd_pcm_info_t *info, unsigned int device);
void snd_pcm_info_set_subdevice(snd_pcm_info_t *info, unsigned int subdevice);
void snd_pcm_info_set_stream(snd_pcm_info_t *info, snd_pcm_stream_t stream);
int snd_pcm_info_card(snd_pcm_info_t *info);
unsigned int snd_pcm_info_device(snd_pcm_info_t *info);
unsigned int snd_pcm_info_subdevice(snd_pcm_info_t *info);
snd_pcm_stream_t snd_pcm_info_stream(snd_pcm_info_t *info);
const char *snd_pcm_info_device_id(snd_pcm_info_t *info);
const char *snd_pcm_info_device_name(snd_pcm_info_t *info);
const char *snd_pcm_info_subdevice_name(snd_pcm_info_t *info);
snd_pcm_class_t snd_pcm_info_device_class(snd_pcm_info_t *info);
snd_pcm_subclass_t snd_pcm_info_device_subclass(snd_pcm_info_t *info);
unsigned int snd_pcm_info_subdevices_count(snd_pcm_info_t *info);
unsigned int snd_pcm_info_subdevices_avail(snd_pcm_info_t *info);

/* Status */
size_t snd_pcm_status_sizeof();
#define snd_pcm_status_alloca(statusp) ({(*statusp) = alloca(snd_pcm_status_sizeof()); 0;})
int snd_pcm_status_malloc(snd_pcm_status_t **statusp);
int snd_pcm_status_free(snd_pcm_status_t *status);
void snd_pcm_status_copy(snd_pcm_status_t *dst, const snd_pcm_status_t *src);

snd_pcm_state_t snd_pcm_status_state(snd_pcm_status_t *status);
int snd_pcm_status_delay(snd_pcm_status_t *status);
int snd_pcm_status_avail(snd_pcm_status_t *status);
int snd_pcm_status_avail_max(snd_pcm_status_t *status);
void snd_pcm_status_tstamp(snd_pcm_status_t *status,
			   snd_timestamp_t *tstamp);
void snd_pcm_status_trigger_tstamp(snd_pcm_status_t *status,
				   snd_timestamp_t *tstamp);
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
const char *snd_pcm_hw_param_name(snd_pcm_hw_param_t var);
const char *snd_pcm_sw_param_name(snd_pcm_sw_param_t var);
const char *snd_pcm_access_name(snd_pcm_access_t access);
const char *snd_pcm_format_name(snd_pcm_format_t format);
const char *snd_pcm_subformat_name(snd_pcm_subformat_t subformat);
const char *snd_pcm_format_description(snd_pcm_format_t format);
int snd_pcm_format_value(const char* name);
const char *snd_pcm_start_mode_name(snd_pcm_start_t mode);
const char *snd_pcm_xrun_mode_name(snd_pcm_xrun_t mode);
const char *snd_pcm_tstamp_mode_name(snd_pcm_tstamp_t mode);
const char *snd_pcm_state_name(snd_pcm_state_t state);

int snd_pcm_area_silence(const snd_pcm_channel_area_t *dst_channel, snd_pcm_uframes_t dst_offset,
			 unsigned int samples, int format);
int snd_pcm_areas_silence(const snd_pcm_channel_area_t *dst_channels, snd_pcm_uframes_t dst_offset,
			  unsigned int channels, snd_pcm_uframes_t frames, int format);
int snd_pcm_area_copy(const snd_pcm_channel_area_t *dst_channel, snd_pcm_uframes_t dst_offset,
		      const snd_pcm_channel_area_t *src_channel, snd_pcm_uframes_t src_offset,
		      unsigned int samples, int format);
int snd_pcm_areas_copy(const snd_pcm_channel_area_t *dst_channels, snd_pcm_uframes_t dst_offset,
		       const snd_pcm_channel_area_t *src_channels, snd_pcm_uframes_t src_offset,
		       unsigned int channels, snd_pcm_uframes_t frames, int format);

snd_pcm_sframes_t snd_pcm_bytes_to_frames(snd_pcm_t *pcm, ssize_t bytes);
ssize_t snd_pcm_frames_to_bytes(snd_pcm_t *pcm, snd_pcm_sframes_t frames);
int snd_pcm_bytes_to_samples(snd_pcm_t *pcm, ssize_t bytes);
ssize_t snd_pcm_samples_to_bytes(snd_pcm_t *pcm, int samples);


/* misc */

int snd_pcm_format_signed(int format);
int snd_pcm_format_unsigned(int format);
int snd_pcm_format_linear(int format);
int snd_pcm_format_little_endian(int format);
int snd_pcm_format_big_endian(int format);
int snd_pcm_format_cpu_endian(int format);
int snd_pcm_format_width(int format);			/* in bits */
int snd_pcm_format_physical_width(int format);		/* in bits */
int snd_pcm_build_linear_format(int width, int unsignd, int big_endian);
ssize_t snd_pcm_format_size(int format, size_t samples);
u_int8_t snd_pcm_format_silence(int format);
u_int16_t snd_pcm_format_silence_16(int format);
u_int32_t snd_pcm_format_silence_32(int format);
u_int64_t snd_pcm_format_silence_64(int format);
int snd_pcm_format_set_silence(int format, void *buf, unsigned int samples);

#ifdef __cplusplus
}
#endif

