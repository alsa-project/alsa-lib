/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

#define SND_PCM_NONBLOCK		0x0001
#define SND_PCM_ASYNC			0x0002

#ifdef __cplusplus
extern "C" {
#endif

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

int snd_pcm_open(snd_pcm_t **pcm, char *name, 
		 int stream, int mode);

/* Obsolete functions */
#define snd_pcm_write snd_pcm_writei
#define snd_pcm_read snd_pcm_readi
ssize_t snd_pcm_writev(snd_pcm_t *pcm, const struct iovec *vector, int count);
ssize_t snd_pcm_readv(snd_pcm_t *pcm, const struct iovec *vector, int count);


snd_pcm_type_t snd_pcm_type(snd_pcm_t *pcm);
int snd_pcm_close(snd_pcm_t *pcm);
int snd_pcm_poll_descriptor(snd_pcm_t *pcm);
int snd_pcm_nonblock(snd_pcm_t *pcm, int nonblock);
int snd_pcm_async(snd_pcm_t *pcm, int sig, pid_t pid);
int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t *info);
int snd_pcm_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int snd_pcm_status(snd_pcm_t *pcm, snd_pcm_status_t *status);
int snd_pcm_prepare(snd_pcm_t *pcm);
int snd_pcm_reset(snd_pcm_t *pcm);
int snd_pcm_start(snd_pcm_t *pcm);
int snd_pcm_drop(snd_pcm_t *pcm);
int snd_pcm_drain(snd_pcm_t *pcm);
int snd_pcm_pause(snd_pcm_t *pcm, int enable);
int snd_pcm_state(snd_pcm_t *pcm);
int snd_pcm_delay(snd_pcm_t *pcm, ssize_t *delayp);
ssize_t snd_pcm_rewind(snd_pcm_t *pcm, size_t frames);
ssize_t snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, size_t size);
ssize_t snd_pcm_readi(snd_pcm_t *pcm, void *buffer, size_t size);
ssize_t snd_pcm_writen(snd_pcm_t *pcm, void **bufs, size_t size);
ssize_t snd_pcm_readn(snd_pcm_t *pcm, void **bufs, size_t size);

int snd_pcm_dump_hw_setup(snd_pcm_t *pcm, FILE *fp);
int snd_pcm_dump_sw_setup(snd_pcm_t *pcm, FILE *fp);
int snd_pcm_dump_setup(snd_pcm_t *pcm, FILE *fp);
int snd_pcm_dump_hw_params(snd_pcm_hw_params_t *params, FILE *fp);
int snd_pcm_dump_hw_params_fail(snd_pcm_hw_params_t *params, FILE *fp);
int snd_pcm_dump_sw_params_fail(snd_pcm_sw_params_t *params, FILE *fp);
int snd_pcm_dump(snd_pcm_t *pcm, FILE *fp);
int snd_pcm_dump_status(snd_pcm_status_t *status, FILE *fp);
int snd_pcm_link(snd_pcm_t *pcm1, snd_pcm_t *pcm2);
int snd_pcm_unlink(snd_pcm_t *pcm);

int snd_pcm_wait(snd_pcm_t *pcm, int timeout);
ssize_t snd_pcm_avail_update(snd_pcm_t *pcm);
int snd_pcm_set_avail_min(snd_pcm_t *pcm, size_t size);

typedef struct _mask mask_t;
size_t mask_sizeof();
void mask_none(mask_t *mask);
void mask_all(mask_t *mask);
void mask_load(mask_t *mask, unsigned int msk);
int mask_empty(const mask_t *mask);
void mask_set(mask_t *mask, unsigned int val);
void mask_reset(mask_t *mask, unsigned int val);
void mask_copy(mask_t *mask, const mask_t *v);
int mask_test(const mask_t *mask, unsigned int val);
void mask_intersect(mask_t *mask, const mask_t *v);
int mask_eq(const mask_t *a, const mask_t *b);
int mask_single(const mask_t *mask);

int snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			   unsigned int par, unsigned int val);
int snd_pcm_hw_params_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  unsigned int par, unsigned int val);
int snd_pcm_hw_params_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  unsigned int par, unsigned int val);
int snd_pcm_hw_params_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			     unsigned int par, unsigned int min, unsigned int max);
int snd_pcm_hw_params_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			    unsigned int par);
int snd_pcm_hw_params_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			   unsigned int par);
int snd_pcm_hw_params_set(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  unsigned int par, unsigned int val);
int snd_pcm_hw_params_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			   unsigned int par, const mask_t *mask);
int snd_pcm_hw_params_info_rate(const snd_pcm_hw_params_t *params,
				unsigned int *rate_num,
				unsigned int *rate_den);
int snd_pcm_hw_params_info_msbits(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_info_flags(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_info_fifo_size(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_info_dig_groups(const snd_pcm_hw_params_t *params);
int snd_pcm_hw_params_value(const snd_pcm_hw_params_t *params,
			    unsigned int var);
const mask_t *snd_pcm_hw_params_value_mask(const snd_pcm_hw_params_t *params,
					   unsigned int var);
const interval_t *snd_pcm_hw_params_value_interval(const snd_pcm_hw_params_t *params,
						   unsigned int var);
unsigned int snd_pcm_hw_params_value_min(const snd_pcm_hw_params_t *params,
					 unsigned int var);
unsigned int snd_pcm_hw_params_value_max(const snd_pcm_hw_params_t *params,
					 unsigned int var);
int snd_pcm_hw_params_test(const snd_pcm_hw_params_t *params,
			   unsigned int var, unsigned int val);
int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);

typedef struct _snd_pcm_strategy snd_pcm_strategy_t;

/* choices need to be sorted on ascending badness */
typedef struct _snd_pcm_strategy_simple_choices_list {
	unsigned int value;
	unsigned int badness;
} snd_pcm_strategy_simple_choices_list_t;

int snd_pcm_hw_info_strategy(snd_pcm_t *pcm, snd_pcm_hw_params_t *info,
			     const snd_pcm_strategy_t *strategy);

int snd_pcm_strategy_free(snd_pcm_strategy_t *strategy);
int snd_pcm_strategy_simple(snd_pcm_strategy_t **strategyp,
			    unsigned int badness_min,
			    unsigned int badness_max);
int snd_pcm_strategy_simple_near(snd_pcm_strategy_t *strategy, int order,
				 unsigned int param,
				 unsigned int best,
				 unsigned int mul);
int snd_pcm_strategy_simple_choices(snd_pcm_strategy_t *strategy, int order,
				    unsigned int param,
				    unsigned int count,
				    snd_pcm_strategy_simple_choices_list_t *choices);
int snd_pcm_hw_params_try_explain_failure(snd_pcm_t *pcm,
					snd_pcm_hw_params_t *fail,
					snd_pcm_hw_params_t *success,
					unsigned int depth,
					FILE *fp);

/* mmap */
const snd_pcm_channel_area_t *snd_pcm_mmap_areas(snd_pcm_t *pcm);
const snd_pcm_channel_area_t *snd_pcm_mmap_running_areas(snd_pcm_t *pcm);
const snd_pcm_channel_area_t *snd_pcm_mmap_stopped_areas(snd_pcm_t *pcm);
ssize_t snd_pcm_mmap_forward(snd_pcm_t *pcm, size_t size);
size_t snd_pcm_mmap_offset(snd_pcm_t *pcm);
size_t snd_pcm_mmap_xfer(snd_pcm_t *pcm, size_t size);
ssize_t snd_pcm_mmap_writei(snd_pcm_t *pcm, const void *buffer, size_t size);
ssize_t snd_pcm_mmap_readi(snd_pcm_t *pcm, void *buffer, size_t size);
ssize_t snd_pcm_mmap_writen(snd_pcm_t *pcm, void **bufs, size_t size);
ssize_t snd_pcm_mmap_readn(snd_pcm_t *pcm, void **bufs, size_t size);

const char *snd_pcm_access_name(unsigned int access);
const char *snd_pcm_format_name(unsigned int format);
const char *snd_pcm_subformat_name(unsigned int subformat);
const char *snd_pcm_hw_param_name(unsigned int params);
const char *snd_pcm_sw_param_name(unsigned int params);

const char *snd_pcm_format_description(unsigned int format);
int snd_pcm_format_value(const char* name);

int snd_pcm_area_silence(const snd_pcm_channel_area_t *dst_channel, size_t dst_offset,
			 size_t samples, int format);
int snd_pcm_areas_silence(const snd_pcm_channel_area_t *dst_channels, size_t dst_offset,
			  size_t vcount, size_t frames, int format);
int snd_pcm_area_copy(const snd_pcm_channel_area_t *src_channel, size_t src_offset,
		      const snd_pcm_channel_area_t *dst_channel, size_t dst_offset,
		      size_t samples, int format);
int snd_pcm_areas_copy(const snd_pcm_channel_area_t *src_channels, size_t src_offset,
		       const snd_pcm_channel_area_t *dst_channels, size_t dst_offset,
		       size_t channels, size_t frames, int format);

ssize_t snd_pcm_bytes_to_frames(snd_pcm_t *pcm, ssize_t bytes);
ssize_t snd_pcm_frames_to_bytes(snd_pcm_t *pcm, ssize_t frames);
ssize_t snd_pcm_bytes_to_samples(snd_pcm_t *pcm, ssize_t bytes);
ssize_t snd_pcm_samples_to_bytes(snd_pcm_t *pcm, ssize_t samples);


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
ssize_t snd_pcm_format_set_silence(int format, void *buf, size_t count);

#ifdef __cplusplus
}
#endif

