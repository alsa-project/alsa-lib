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

typedef enum {
	SND_PCM_TYPE_HW,
	SND_PCM_TYPE_MULTI,
	SND_PCM_TYPE_FILE,
	SND_PCM_TYPE_NULL,
	SND_PCM_TYPE_CLIENT,
	SND_PCM_TYPE_LINEAR,
	SND_PCM_TYPE_ALAW,
	SND_PCM_TYPE_MULAW,
	SND_PCM_TYPE_ADPCM,
	SND_PCM_TYPE_RATE,
	SND_PCM_TYPE_ROUTE,
	SND_PCM_TYPE_COPY,
	SND_PCM_TYPE_PLUG,
	SND_PCM_TYPE_DROUTE,
	SND_PCM_TYPE_LBSERVER,
} snd_pcm_type_t;


int snd_pcm_open(snd_pcm_t **handle, char *name, 
		 int stream, int mode);

/* Obsolete functions */
int snd_pcm_hw_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int stream, int mode);
int snd_pcm_hw_open_device(snd_pcm_t **handle, int card, int device, int stream, int mode);
int snd_pcm_plug_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int stream, int mode);
int snd_pcm_plug_open_device(snd_pcm_t **handle, int card, int device, int stream, int mode);
#define snd_pcm_write snd_pcm_writei
#define snd_pcm_read snd_pcm_readi
ssize_t snd_pcm_writev(snd_pcm_t *handle, const struct iovec *vector, int count);
ssize_t snd_pcm_readv(snd_pcm_t *handle, const struct iovec *vector, int count);


snd_pcm_type_t snd_pcm_type(snd_pcm_t *handle);
int snd_pcm_close(snd_pcm_t *handle);
int snd_pcm_poll_descriptor(snd_pcm_t *handle);
int snd_pcm_nonblock(snd_pcm_t *handle, int nonblock);
int snd_pcm_info(snd_pcm_t *handle, snd_pcm_info_t *info);
int snd_pcm_params_info(snd_pcm_t *handle, snd_pcm_params_info_t *info);
int snd_pcm_params(snd_pcm_t *handle, snd_pcm_params_t *params);
int snd_pcm_setup(snd_pcm_t *handle, snd_pcm_setup_t *setup);
int snd_pcm_channel_info(snd_pcm_t *handle, snd_pcm_channel_info_t *info);
int snd_pcm_channel_params(snd_pcm_t *handle, snd_pcm_channel_params_t *params);
int snd_pcm_channel_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup);
int snd_pcm_status(snd_pcm_t *handle, snd_pcm_status_t *status);
int snd_pcm_prepare(snd_pcm_t *handle);
int snd_pcm_start(snd_pcm_t *handle);
int snd_pcm_stop(snd_pcm_t *handle);
int snd_pcm_flush(snd_pcm_t *handle);
int snd_pcm_pause(snd_pcm_t *handle, int enable);
int snd_pcm_state(snd_pcm_t *handle);
int snd_pcm_delay(snd_pcm_t *handle, ssize_t *delayp);
ssize_t snd_pcm_rewind(snd_pcm_t *handle, size_t frames);
ssize_t snd_pcm_writei(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_readi(snd_pcm_t *handle, void *buffer, size_t size);
ssize_t snd_pcm_writen(snd_pcm_t *handle, void **bufs, size_t size);
ssize_t snd_pcm_readn(snd_pcm_t *handle, void **bufs, size_t size);
int snd_pcm_dump_setup(snd_pcm_t *handle, FILE *fp);
int snd_pcm_dump(snd_pcm_t *handle, FILE *fp);
int snd_pcm_dump_status(snd_pcm_status_t *status, FILE *fp);
int snd_pcm_link(snd_pcm_t *handle1, snd_pcm_t *handle2);
int snd_pcm_unlink(snd_pcm_t *handle);

int snd_pcm_channels_mask(snd_pcm_t *handle, bitset_t *cmask);
int snd_pcm_wait(snd_pcm_t *pcm, int timeout);
ssize_t snd_pcm_avail_update(snd_pcm_t *pcm);


/* mmap */
int snd_pcm_mmap(snd_pcm_t *handle, void **buffer);
int snd_pcm_munmap(snd_pcm_t *handle);
int snd_pcm_mmap_get_areas(snd_pcm_t *handle, snd_pcm_channel_area_t *areas);
ssize_t snd_pcm_mmap_forward(snd_pcm_t *pcm, size_t size);
size_t snd_pcm_mmap_offset(snd_pcm_t *pcm);
size_t snd_pcm_mmap_xfer(snd_pcm_t *pcm, size_t size);
ssize_t snd_pcm_mmap_writei(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t snd_pcm_mmap_readi(snd_pcm_t *handle, void *buffer, size_t size);
ssize_t snd_pcm_mmap_writen(snd_pcm_t *handle, void **bufs, size_t size);
ssize_t snd_pcm_mmap_readn(snd_pcm_t *handle, void **bufs, size_t size);

const char *snd_pcm_format_name(int format);
const char *snd_pcm_format_description(int format);
int snd_pcm_format_value(const char* name);

int snd_pcm_area_silence(snd_pcm_channel_area_t *dst_channel, size_t dst_offset,
			 size_t samples, int format);
int snd_pcm_areas_silence(snd_pcm_channel_area_t *dst_channels, size_t dst_offset,
			  size_t vcount, size_t frames, int format);
int snd_pcm_area_copy(snd_pcm_channel_area_t *src_channel, size_t src_offset,
		      snd_pcm_channel_area_t *dst_channel, size_t dst_offset,
		      size_t samples, int format);
int snd_pcm_areas_copy(snd_pcm_channel_area_t *src_channels, size_t src_offset,
		       snd_pcm_channel_area_t *dst_channels, size_t dst_offset,
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

