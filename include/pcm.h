/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

#define SND_PCM_NONBLOCK		0x0001

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int bitset_t;

static inline size_t bitset_size(size_t nbits)
{
	return (nbits + sizeof(bitset_t) * 8 - 1) / (sizeof(bitset_t) * 8);
}

static inline bitset_t *bitset_alloc(size_t nbits)
{
	return (bitset_t*) calloc(bitset_size(nbits), sizeof(bitset_t));
}
	
static inline void bitset_set(bitset_t *bitmap, unsigned int pos)
{
	size_t bits = sizeof(*bitmap) * 8;
	bitmap[pos / bits] |= 1U << (pos % bits);
}

static inline void bitset_reset(bitset_t *bitmap, unsigned int pos)
{
	size_t bits = sizeof(*bitmap) * 8;
	bitmap[pos / bits] &= ~(1U << (pos % bits));
}

static inline int bitset_get(bitset_t *bitmap, unsigned int pos)
{
	size_t bits = sizeof(*bitmap) * 8;
	return !!(bitmap[pos / bits] & (1U << (pos % bits)));
}

static inline void bitset_copy(bitset_t *dst, bitset_t *src, size_t nbits)
{
	memcpy(dst, src, bitset_size(nbits) * sizeof(bitset_t));
}

static inline void bitset_and(bitset_t *dst, bitset_t *bs, size_t nbits)
{
	bitset_t *end = dst + bitset_size(nbits);
	while (dst < end)
		*dst++ &= *bs++;
}

static inline void bitset_or(bitset_t *dst, bitset_t *bs, size_t nbits)
{
	bitset_t *end = dst + bitset_size(nbits);
	while (dst < end)
		*dst++ |= *bs++;
}

static inline void bitset_zero(bitset_t *dst, size_t nbits)
{
	bitset_t *end = dst + bitset_size(nbits);
	while (dst < end)
		*dst++ = 0;
}

static inline void bitset_one(bitset_t *dst, size_t nbits)
{
	bitset_t *end = dst + bitset_size(nbits);
	while (dst < end)
		*dst++ = ~(bitset_t)0;
}

static inline size_t hweight32(bitset_t v)
{
        v = (v & 0x55555555) + ((v >> 1) & 0x55555555);
        v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
        v = (v & 0x0F0F0F0F) + ((v >> 4) & 0x0F0F0F0F);
        v = (v & 0x00FF00FF) + ((v >> 8) & 0x00FF00FF);
        return (v & 0x0000FFFF) + ((v >> 16) & 0x0000FFFF);
}

/* Count bits set */
static inline size_t bitset_count(bitset_t *bitset, size_t nbits)
{
	bitset_t *end = bitset + bitset_size(nbits) - 1;
	size_t bits = sizeof(*bitset) * 8;
	size_t count = 0;
	while (bitset < end)
		count += hweight32(*bitset++);
	count += hweight32(*bitset & ((1U << (nbits % bits)) - 1));
	return count;
}

typedef struct snd_pcm snd_pcm_t;
typedef struct snd_pcm_loopback snd_pcm_loopback_t;

typedef enum { SND_PCM_TYPE_HW, SND_PCM_TYPE_PLUG, SND_PCM_TYPE_MULTI } snd_pcm_type_t;

#if 0
typedef struct {
	snd_pcm_t *handle;
	snd_timestamp_t tstamp;
	int result;
	union {
		char reserved[256];
	} arg;
} snd_pcm_synchro_request_t;

typedef enum { SND_PCM_SYNCHRO_GO } snd_pcm_synchro_cmd_t;

#define snd_pcm_synchro_mode_t snd_pcm_sync_mode_t
#define SND_PCM_SYNCHRO_MODE_NORMAL SND_PCM_SYNC_MODE_NORMAL
#define SND_PCM_SYNCHRO_MODE_HARDWARE SND_PCM_SYNC_MODE_HARDWARE
#define SND_PCM_SYNCHRO_MODE_RELAXED SND_PCM_SYNC_MODE_RELAXED
int snd_pcm_synchro(snd_pcm_synchro_cmd_t cmd, 
		    unsigned int reqs_count, snd_pcm_synchro_request_t *reqs,
		    snd_pcm_synchro_mode_t mode);
#endif


int snd_pcm_open(snd_pcm_t **handle, char *name, 
		 int stream, int mode);

int snd_pcm_hw_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int stream, int mode);
int snd_pcm_hw_open(snd_pcm_t **handle, int card, int device, int stream, int mode);

snd_pcm_type_t snd_pcm_type(snd_pcm_t *handle);
int snd_pcm_close(snd_pcm_t *handle);
int snd_pcm_file_descriptor(snd_pcm_t *handle);
int snd_pcm_nonblock(snd_pcm_t *handle, int nonblock);
int snd_pcm_info(snd_pcm_t *handle, snd_pcm_info_t *info);
int snd_pcm_params_info(snd_pcm_t *handle, snd_pcm_params_info_t *info);
int snd_pcm_params(snd_pcm_t *handle, snd_pcm_params_t *params);
int snd_pcm_setup(snd_pcm_t *handle, snd_pcm_setup_t *setup);
int snd_pcm_channel_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup);
int snd_pcm_status(snd_pcm_t *handle, snd_pcm_status_t *status);
int snd_pcm_prepare(snd_pcm_t *handle);
int snd_pcm_go(snd_pcm_t *handle);
int snd_pcm_drain(snd_pcm_t *handle);
int snd_pcm_flush(snd_pcm_t *handle);
int snd_pcm_pause(snd_pcm_t *handle, int enable);
int snd_pcm_state(snd_pcm_t *handle);
ssize_t snd_pcm_frame_io(snd_pcm_t *handle, int update);
ssize_t snd_pcm_frame_data(snd_pcm_t *handle, off_t offset);
ssize_t snd_pcm_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_read(snd_pcm_t *handle, void *buffer, size_t size);
ssize_t snd_pcm_writev(snd_pcm_t *handle, const struct iovec *vector, unsigned long  count);
ssize_t snd_pcm_readv(snd_pcm_t *handle, const struct iovec *vector, unsigned long count);
int snd_pcm_dump_setup(snd_pcm_t *handle, FILE *fp);
int snd_pcm_dump(snd_pcm_t *handle, FILE *fp);
int snd_pcm_link(snd_pcm_t *handle1, snd_pcm_t *handle2);
int snd_pcm_unlink(snd_pcm_t *handle);

int snd_pcm_channels_mask(snd_pcm_t *handle, bitset_t *client_vmask);

/* mmap */
int snd_pcm_mmap(snd_pcm_t *handle, snd_pcm_mmap_status_t **status, snd_pcm_mmap_control_t **control, void **buffer);
int snd_pcm_munmap(snd_pcm_t *handle);
int snd_pcm_mmap_state(snd_pcm_t *handle);
ssize_t snd_pcm_mmap_frame_io(snd_pcm_t *handle);
ssize_t snd_pcm_mmap_frame_data(snd_pcm_t *handle, off_t offset);
int snd_pcm_mmap_status(snd_pcm_t *handle, snd_pcm_mmap_status_t **status);
int snd_pcm_mmap_control(snd_pcm_t *handle, snd_pcm_mmap_control_t **control);
int snd_pcm_mmap_data(snd_pcm_t *handle, void **buffer);
int snd_pcm_munmap_status(snd_pcm_t *handle);
int snd_pcm_munmap_control(snd_pcm_t *handle);
int snd_pcm_munmap_data(snd_pcm_t *handle);
int snd_pcm_mmap_ready(snd_pcm_t *handle);
ssize_t snd_pcm_mmap_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_mmap_read(snd_pcm_t *handle, void *buffer, size_t size);
ssize_t snd_pcm_mmap_writev(snd_pcm_t *handle, const struct iovec *vector, unsigned long  count);
ssize_t snd_pcm_mmap_readv(snd_pcm_t *handle, const struct iovec *vector, unsigned long count);
int snd_pcm_mmap_frames_avail(snd_pcm_t *handle, ssize_t *frames);
ssize_t snd_pcm_mmap_frames_xfer(snd_pcm_t *handle, size_t frames);
ssize_t snd_pcm_mmap_frames_offset(snd_pcm_t *handle);
ssize_t snd_pcm_mmap_write_areas(snd_pcm_t *handle, snd_pcm_channel_area_t *channels, size_t frames);
ssize_t snd_pcm_mmap_write_frames(snd_pcm_t *handle, const void *buffer, size_t frames);
ssize_t snd_pcm_mmap_read_areas(snd_pcm_t *handle, snd_pcm_channel_area_t *channels, size_t frames);
ssize_t snd_pcm_mmap_read_frames(snd_pcm_t *handle, const void *buffer, size_t frames);
int snd_pcm_mmap_get_areas(snd_pcm_t *handle, snd_pcm_channel_area_t *areas);


const char *snd_pcm_format_name(int format);
const char *snd_pcm_format_description(int format);
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
		       size_t vcount, size_t frames, int format);

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

/*
 *  PCM Plug-In interface
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_stru_pcm_plugin snd_pcm_plugin_t;
#define snd_pcm_plug_t struct snd_pcm_plug

typedef enum {
	INIT = 0,
	PREPARE = 1,
	DRAIN = 2,
	FLUSH = 3,
	PAUSE = 4,
} snd_pcm_plugin_action_t;

typedef struct snd_stru_pcm_plugin_channel {
	snd_pcm_channel_area_t area;
	unsigned int enabled:1;		/* channel need to be processed */
	unsigned int wanted:1;		/* channel is wanted */
} snd_pcm_plugin_channel_t;

struct snd_stru_pcm_plugin {
	char *name;			/* plug-in name */
	int stream;
	snd_pcm_format_t src_format;	/* source format */
	snd_pcm_format_t dst_format;	/* destination format */
	int src_width;			/* sample width in bits */
	int dst_width;			/* sample width in bits */
	ssize_t (*src_frames)(snd_pcm_plugin_t *plugin, size_t dst_frames);
	ssize_t (*dst_frames)(snd_pcm_plugin_t *plugin, size_t src_frames);
	ssize_t (*client_channels)(snd_pcm_plugin_t *plugin,
				 size_t frames,
				 snd_pcm_plugin_channel_t **channels);
	int (*src_channels_mask)(snd_pcm_plugin_t *plugin,
			       bitset_t *dst_vmask,
			       bitset_t **src_vmask);
	int (*dst_channels_mask)(snd_pcm_plugin_t *plugin,
			       bitset_t *src_vmask,
			       bitset_t **dst_vmask);
	ssize_t (*transfer)(snd_pcm_plugin_t *plugin,
			    const snd_pcm_plugin_channel_t *src_channels,
			    snd_pcm_plugin_channel_t *dst_channels,
			    size_t frames);
	int (*action)(snd_pcm_plugin_t *plugin,
		      snd_pcm_plugin_action_t action,
		      unsigned long data);
	int (*parameter_set)(snd_pcm_plugin_t *plugin,
			     const char *name,
			     unsigned long value);
	int (*parameter_get)(snd_pcm_plugin_t *plugin,
			     const char *name,
			     unsigned long *value);
	void (*dump)(snd_pcm_plugin_t *plugin, FILE *fp);
	snd_pcm_plugin_t *prev;
	snd_pcm_plugin_t *next;
	snd_pcm_plug_t *plug;
	void *private_data;
	void (*private_free)(snd_pcm_plugin_t *plugin);
	char *buf;
	size_t buf_frames;
	snd_pcm_plugin_channel_t *buf_channels;
	bitset_t *src_vmask;
	bitset_t *dst_vmask;
	char extra_data[0];
};

int snd_pcm_plug_create(snd_pcm_t **handle, snd_pcm_t *slave, int close_slave);
int snd_pcm_plug_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int stream, int mode);
int snd_pcm_plug_open(snd_pcm_t **handle, int card, int device, int stream, int mode);

int snd_pcm_plugin_free(snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_insert(snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_append(snd_pcm_plugin_t *plugin);
void snd_pcm_plugin_dump(snd_pcm_plugin_t *plugin, FILE *fp);
int snd_pcm_plug_alloc(snd_pcm_plug_t *plug, size_t frames);
int snd_pcm_plug_clear(snd_pcm_plug_t *plug);
snd_pcm_plugin_t *snd_pcm_plug_first(snd_pcm_plug_t *plug);
snd_pcm_plugin_t *snd_pcm_plug_last(snd_pcm_plug_t *plug);
int snd_pcm_plug_direct(snd_pcm_plug_t *plug);
ssize_t snd_pcm_plug_client_size(snd_pcm_plug_t *plug, size_t drv_frames);
ssize_t snd_pcm_plug_slave_size(snd_pcm_plug_t *plug, size_t clt_frames);

/*
 *  Plug-In constructors
 */

int snd_pcm_plugin_build(snd_pcm_plug_t *plug,
			 const char *name,
			 snd_pcm_format_t *src_format,
			 snd_pcm_format_t *dst_format,
			 size_t extra,
			 snd_pcm_plugin_t **ret);
/* basic I/O */
int snd_pcm_plugin_build_io(snd_pcm_plug_t *plug,
			    snd_pcm_format_t *format,
			    snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_mmap(snd_pcm_plug_t *plug,
			      snd_pcm_format_t *format,
			      snd_pcm_plugin_t **r_plugin);

#define ROUTE_PLUGIN_USE_FLOAT 1
#if ROUTE_PLUGIN_USE_FLOAT
#define FULL 1.0 
#define HALF 0.5
typedef float route_ttable_entry_t;
#else
#define FULL ROUTE_PLUGIN_RESOLUTION
#define HALF ROUTE_PLUGIN_RESOLUTION / 2
typedef int route_ttable_entry_t;
#endif

/* conversion plugins */
int snd_pcm_plugin_build_interleave(snd_pcm_plug_t *plug,
				    snd_pcm_format_t *src_format,
				    snd_pcm_format_t *dst_format,
				    snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_linear(snd_pcm_plug_t *plug,
				snd_pcm_format_t *src_format,
				snd_pcm_format_t *dst_format,
				snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_mulaw(snd_pcm_plug_t *plug,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_alaw(snd_pcm_plug_t *plug,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_adpcm(snd_pcm_plug_t *plug,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_rate(snd_pcm_plug_t *plug,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_route(snd_pcm_plug_t *plug,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       route_ttable_entry_t *ttable,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_copy(snd_pcm_plug_t *plug,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin);

int snd_pcm_multi_create(snd_pcm_t **handlep, size_t slaves_count,
			 snd_pcm_t **slaves_handle, size_t *slaves_channels_count,
			 size_t binds_count,  unsigned int *binds_client_channel,
			 unsigned int *binds_slave, unsigned int *binds_slave_channel,
			 int close_slaves);

#ifdef __cplusplus
}
#endif

