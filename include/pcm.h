/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/

#define SND_PCM_OPEN_PLAYBACK		0x0001
#define SND_PCM_OPEN_CAPTURE		0x0002
#define SND_PCM_OPEN_DUPLEX		0x0003
#define SND_PCM_NONBLOCK_PLAYBACK	0x1000
#define SND_PCM_NONBLOCK_CAPTURE	0x2000
#define SND_PCM_NONBLOCK		0x3000

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int bitset_t;

static inline size_t bitset_size(int nbits)
{
	return (nbits + sizeof(bitset_t) * 8 - 1) / (sizeof(bitset_t) * 8);
}

static inline bitset_t *bitset_alloc(int nbits)
{
	return (bitset_t*) calloc(bitset_size(nbits), sizeof(bitset_t));
}
	
static inline void bitset_set(bitset_t *bitmap, unsigned int pos)
{
	int bits = sizeof(*bitmap) * 8;
	bitmap[pos / bits] |= 1 << (pos % bits);
}

static inline void bitset_reset(bitset_t *bitmap, unsigned int pos)
{
	int bits = sizeof(*bitmap) * 8;
	bitmap[pos / bits] &= ~(1 << (pos % bits));
}

static inline int bitset_get(bitset_t *bitmap, unsigned int pos)
{
	int bits = sizeof(*bitmap) * 8;
	return !!(bitmap[pos / bits] & (1 << (pos % bits)));
}

static inline void bitset_copy(bitset_t *dst, bitset_t *src, unsigned int nbits)
{
	memcpy(dst, src, bitset_size(nbits) * sizeof(bitset_t));
}

static inline void bitset_and(bitset_t *dst, bitset_t *bs, unsigned int nbits)
{
	bitset_t *end = dst + bitset_size(nbits);
	while (dst < end)
		*dst++ &= *bs++;
}

static inline void bitset_or(bitset_t *dst, bitset_t *bs, unsigned int nbits)
{
	bitset_t *end = dst + bitset_size(nbits);
	while (dst < end)
		*dst++ |= *bs++;
}

static inline void bitset_zero(bitset_t *dst, unsigned int nbits)
{
	bitset_t *end = dst + bitset_size(nbits);
	while (dst < end)
		*dst++ = 0;
}

static inline void bitset_one(bitset_t *dst, unsigned int nbits)
{
	bitset_t *end = dst + bitset_size(nbits);
	while (dst < end)
		*dst++ = ~(bitset_t)0;
}

typedef struct snd_pcm snd_pcm_t;
typedef struct snd_pcm_loopback snd_pcm_loopback_t;

typedef enum { SND_PCM_TYPE_HW, SND_PCM_TYPE_PLUG, SND_PCM_TYPE_MULTI } snd_pcm_type_t;

int snd_pcm_open(snd_pcm_t **handle, int card, int device, int mode);
int snd_pcm_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int mode);

snd_pcm_type_t snd_pcm_type(snd_pcm_t *handle);
int snd_pcm_close(snd_pcm_t *handle);
int snd_pcm_stream_close(snd_pcm_t *handle, int stream);
int snd_pcm_file_descriptor(snd_pcm_t *handle, int stream);
int snd_pcm_stream_nonblock(snd_pcm_t *handle, int stream, int nonblock);
int snd_pcm_info(snd_pcm_t *handle, snd_pcm_info_t *info);
int snd_pcm_stream_info(snd_pcm_t *handle, snd_pcm_stream_info_t *info);
int snd_pcm_stream_params(snd_pcm_t *handle, snd_pcm_stream_params_t *params);
int snd_pcm_stream_setup(snd_pcm_t *handle, snd_pcm_stream_setup_t *setup);
int snd_pcm_channel_setup(snd_pcm_t *handle, int stream, snd_pcm_channel_setup_t *setup);
int snd_pcm_stream_status(snd_pcm_t *handle, snd_pcm_stream_status_t *status);
int snd_pcm_playback_prepare(snd_pcm_t *handle);
int snd_pcm_capture_prepare(snd_pcm_t *handle);
int snd_pcm_stream_prepare(snd_pcm_t *handle, int stream);
int snd_pcm_playback_go(snd_pcm_t *handle);
int snd_pcm_capture_go(snd_pcm_t *handle);
int snd_pcm_stream_go(snd_pcm_t *handle, int stream);
int snd_pcm_sync_go(snd_pcm_t *handle, snd_pcm_sync_t *sync);
int snd_pcm_playback_drain(snd_pcm_t *handle);
int snd_pcm_stream_drain(snd_pcm_t *handle, int stream);
int snd_pcm_playback_flush(snd_pcm_t *handle);
int snd_pcm_capture_flush(snd_pcm_t *handle);
int snd_pcm_stream_flush(snd_pcm_t *handle, int stream);
int snd_pcm_playback_pause(snd_pcm_t *handle, int enable);
int snd_pcm_stream_pause(snd_pcm_t *handle, int stream, int enable);
ssize_t snd_pcm_transfer_size(snd_pcm_t *handle, int stream);
int snd_pcm_stream_state(snd_pcm_t *handle, int stream);
ssize_t snd_pcm_stream_byte_io(snd_pcm_t *handle, int stream, int update);
ssize_t snd_pcm_stream_seek(snd_pcm_t *pcm, int stream, off_t offset);
ssize_t snd_pcm_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_read(snd_pcm_t *handle, void *buffer, size_t size);
ssize_t snd_pcm_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long  count);
ssize_t snd_pcm_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count);
const char *snd_pcm_get_format_name(int format);
const char *snd_pcm_get_format_description(int format);
int snd_pcm_get_format_value(const char* name);
int snd_pcm_dump_setup(snd_pcm_t *pcm, int stream, FILE *fp);

int snd_pcm_mmap(snd_pcm_t *handle, int stream, snd_pcm_mmap_control_t **control, void **buffer);
int snd_pcm_munmap(snd_pcm_t *handle, int stream);
int snd_pcm_mmap_control(snd_pcm_t *handle, int stream, snd_pcm_mmap_control_t **control);
int snd_pcm_mmap_data(snd_pcm_t *handle, int stream, void **buffer);
int snd_pcm_munmap_control(snd_pcm_t *handle, int stream);
int snd_pcm_munmap_data(snd_pcm_t *handle, int stream);
int snd_pcm_channels_mask(snd_pcm_t *pcm, int stream, bitset_t *client_vmask);
int snd_pcm_mmap_ready(snd_pcm_t *pcm, int stream);
ssize_t snd_pcm_mmap_write(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_mmap_read(snd_pcm_t *handle, void *buffer, size_t size);
ssize_t snd_pcm_mmap_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long  count);
ssize_t snd_pcm_mmap_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count);
int snd_pcm_mmap_frames_used(snd_pcm_t *pcm, int stream, ssize_t *frames);
int snd_pcm_mmap_frames_free(snd_pcm_t *pcm, int stream, ssize_t *frames);
ssize_t snd_pcm_mmap_frames_xfer(snd_pcm_t *pcm, int stream, size_t frames);
ssize_t snd_pcm_mmap_frames_offset(snd_pcm_t *pcm, int stream);
int snd_pcm_mmap_commit_frames(snd_pcm_t *pcm, int stream, int frames);
ssize_t snd_pcm_mmap_write_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *channels, size_t frames);
ssize_t snd_pcm_mmap_write_frames(snd_pcm_t *pcm, const void *buffer, size_t frames);
ssize_t snd_pcm_mmap_read_areas(snd_pcm_t *pcm, snd_pcm_channel_area_t *channels, size_t frames);
ssize_t snd_pcm_mmap_read_frames(snd_pcm_t *pcm, const void *buffer, size_t frames);
int snd_pcm_mmap_get_areas(snd_pcm_t *pcm, int stream, snd_pcm_channel_area_t *areas);

ssize_t snd_pcm_bytes_per_second(snd_pcm_t *pcm, int stream);

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
ssize_t snd_pcm_format_bytes_per_second(snd_pcm_format_t *format);
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
#define snd_pcm_plugin_handle_t snd_pcm_t

typedef enum {
	INIT = 0,
	PREPARE = 1,
	DRAIN = 2,
	FLUSH = 3,
	PAUSE = 4,
} snd_pcm_plugin_action_t;

typedef struct snd_stru_pcm_plugin_channel {
	void *aptr;			/* pointer to the allocated area */
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
	snd_pcm_plugin_t *prev;
	snd_pcm_plugin_t *next;
	snd_pcm_plugin_handle_t *handle;
	void *private_data;
	void (*private_free)(snd_pcm_plugin_t *plugin, void *private_data);
	snd_pcm_plugin_channel_t *src_channels;
	snd_pcm_plugin_channel_t *dst_channels;
	bitset_t *src_vmask;
	bitset_t *dst_vmask;
	char extra_data[0];
};

int snd_pcm_plug_connect(snd_pcm_t **handle, snd_pcm_t *slave, int mode, int close_slave);
int snd_pcm_plug_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int mode);
int snd_pcm_plug_open(snd_pcm_t **handle, int card, int device, int mode);

int snd_pcm_plugin_free(snd_pcm_plugin_t *plugin);
int snd_pcm_plug_clear(snd_pcm_t *handle, int stream);
int snd_pcm_plugin_insert(snd_pcm_plugin_t *plugin);
int snd_pcm_plugin_append(snd_pcm_plugin_t *plugin);
#if 0
int snd_pcm_plugin_remove_to(snd_pcm_plugin_t *plugin);
int snd_pcm_plug_remove_first(snd_pcm_t *handle, int stream);
#endif
snd_pcm_plugin_t *snd_pcm_plug_first(snd_pcm_t *handle, int stream);
snd_pcm_plugin_t *snd_pcm_plug_last(snd_pcm_t *handle, int stream);
int snd_pcm_plug_direct(snd_pcm_t *pcm, int stream);
ssize_t snd_pcm_plug_client_frames(snd_pcm_t *handle, int stream, size_t drv_frames);
ssize_t snd_pcm_plug_slave_frames(snd_pcm_t *handle, int stream, size_t clt_frames);
ssize_t snd_pcm_plug_client_size(snd_pcm_t *handle, int stream, size_t drv_size);
ssize_t snd_pcm_plug_slave_size(snd_pcm_t *handle, int stream, size_t clt_size);

/*
 *  Plug-In helpers
 */

ssize_t snd_pcm_plugin_src_frames_to_size(snd_pcm_plugin_t *plugin, size_t frames);
ssize_t snd_pcm_plugin_dst_frames_to_size(snd_pcm_plugin_t *plugin, size_t frames);
ssize_t snd_pcm_plugin_src_size_to_frames(snd_pcm_plugin_t *plugin, size_t size);
ssize_t snd_pcm_plugin_dst_size_to_frames(snd_pcm_plugin_t *plugin, size_t size);

/*
 *  Plug-In constructors
 */

int snd_pcm_plugin_build(snd_pcm_plugin_handle_t *handle,
			 int stream,
			 const char *name,
			 snd_pcm_format_t *src_format,
			 snd_pcm_format_t *dst_format,
			 int extra,
			 snd_pcm_plugin_t **ret);
/* basic I/O */
int snd_pcm_plugin_build_io(snd_pcm_plugin_handle_t *handle,
			    int stream,
			    snd_pcm_t *slave,
			    snd_pcm_format_t *format,
			    snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_mmap(snd_pcm_plugin_handle_t *handle,
			      int stream,
			      snd_pcm_t *slave,
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
int snd_pcm_plugin_build_interleave(snd_pcm_plugin_handle_t *handle,
				    int stream,
				    snd_pcm_format_t *src_format,
				    snd_pcm_format_t *dst_format,
				    snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_linear(snd_pcm_plugin_handle_t *handle,
				int stream,
				snd_pcm_format_t *src_format,
				snd_pcm_format_t *dst_format,
				snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_mulaw(snd_pcm_plugin_handle_t *handle,
			       int stream,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_alaw(snd_pcm_plugin_handle_t *handle,
			      int stream,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_adpcm(snd_pcm_plugin_handle_t *handle,
			       int stream,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_rate(snd_pcm_plugin_handle_t *handle,
			      int stream,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_route(snd_pcm_plugin_handle_t *handle,
			       int stream,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       route_ttable_entry_t *ttable,
			       snd_pcm_plugin_t **r_plugin);
int snd_pcm_plugin_build_copy(snd_pcm_plugin_handle_t *handle,
			      int stream,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin);

#ifdef __cplusplus
}
#endif

