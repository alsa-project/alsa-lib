/**
 * \file include/pcm_types.h
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@perex.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 1998-2001
 *
 * Application interface library for the ALSA driver.
 * See the \ref pcm page for more details.
 */
/*
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#if !defined(__ASOUNDLIB_H) && !defined(ALSA_LIBRARY_BUILD)
/* don't use ALSA_LIBRARY_BUILD define in sources outside alsa-lib */
#warning "use #include <alsa/asoundlib.h>, <alsa/pcm.h> should not be used directly"
#include <alsa/asoundlib.h>
#endif

#ifndef __ALSA_PCM_TYPES_H
#define __ALSA_PCM_TYPES_H /**< header include loop protection */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup PCM PCM Interface
 *  See the \ref pcm page for more details.
 *  \{
 */

/** dlsym version for interface entry callback */
#define SND_PCM_DLSYM_VERSION		_dlsym_pcm_001

/** PCM generic info container */
typedef struct _snd_pcm_info snd_pcm_info_t;

/** PCM hardware configuration space container
 *
 *  snd_pcm_hw_params_t is an opaque structure which contains a set of possible
 *  PCM hardware configurations. For example, a given instance might include a
 *  range of buffer sizes, a range of period sizes, and a set of several sample
 *  formats. Some subset of all possible combinations these sets may be valid,
 *  but not necessarily any combination will be valid.
 *
 *  When a parameter is set or restricted using a snd_pcm_hw_params_set*
 *  function, all of the other ranges will be updated to exclude as many
 *  impossible configurations as possible. Attempting to set a parameter
 *  outside of its acceptable range will result in the function failing
 *  and an error code being returned.
 */
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

	SND_PCM_CLASS_GENERIC = 0,
	/** multichannel device */
	SND_PCM_CLASS_MULTI,
	/** software modem device */
	SND_PCM_CLASS_MODEM,
	/** digitizer device */
	SND_PCM_CLASS_DIGITIZER,
	SND_PCM_CLASS_LAST = SND_PCM_CLASS_DIGITIZER
} snd_pcm_class_t;

/** PCM subclass */
typedef enum _snd_pcm_subclass {
	/** subdevices are mixed together */
	SND_PCM_SUBCLASS_GENERIC_MIX = 0,
	/** multichannel subdevices are mixed together */
	SND_PCM_SUBCLASS_MULTI_MIX,
	SND_PCM_SUBCLASS_LAST = SND_PCM_SUBCLASS_MULTI_MIX
} snd_pcm_subclass_t;

/** PCM stream (direction) */
typedef enum _snd_pcm_stream {
	/** Playback stream */
	SND_PCM_STREAM_PLAYBACK = 0,
	/** Capture stream */
	SND_PCM_STREAM_CAPTURE,
	SND_PCM_STREAM_LAST = SND_PCM_STREAM_CAPTURE
} snd_pcm_stream_t;

/** PCM access type */
typedef enum _snd_pcm_access {
	/** mmap access with simple interleaved channels */
	SND_PCM_ACCESS_MMAP_INTERLEAVED = 0,
	/** mmap access with simple non interleaved channels */
	SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
	/** mmap access with complex placement */
	SND_PCM_ACCESS_MMAP_COMPLEX,
	/** snd_pcm_readi/snd_pcm_writei access */
	SND_PCM_ACCESS_RW_INTERLEAVED,
	/** snd_pcm_readn/snd_pcm_writen access */
	SND_PCM_ACCESS_RW_NONINTERLEAVED,
	SND_PCM_ACCESS_LAST = SND_PCM_ACCESS_RW_NONINTERLEAVED
} snd_pcm_access_t;

/** PCM sample format */
typedef enum _snd_pcm_format {
	/** Unknown */
	SND_PCM_FORMAT_UNKNOWN = -1,
	/** Signed 8 bit */
	SND_PCM_FORMAT_S8 = 0,
	/** Unsigned 8 bit */
	SND_PCM_FORMAT_U8,
	/** Signed 16 bit Little Endian */
	SND_PCM_FORMAT_S16_LE,
	/** Signed 16 bit Big Endian */
	SND_PCM_FORMAT_S16_BE,
	/** Unsigned 16 bit Little Endian */
	SND_PCM_FORMAT_U16_LE,
	/** Unsigned 16 bit Big Endian */
	SND_PCM_FORMAT_U16_BE,
	/** Signed 24 bit Little Endian using low three bytes in 32-bit word */
	SND_PCM_FORMAT_S24_LE,
	/** Signed 24 bit Big Endian using low three bytes in 32-bit word */
	SND_PCM_FORMAT_S24_BE,
	/** Unsigned 24 bit Little Endian using low three bytes in 32-bit word */
	SND_PCM_FORMAT_U24_LE,
	/** Unsigned 24 bit Big Endian using low three bytes in 32-bit word */
	SND_PCM_FORMAT_U24_BE,
	/** Signed 32 bit Little Endian */
	SND_PCM_FORMAT_S32_LE,
	/** Signed 32 bit Big Endian */
	SND_PCM_FORMAT_S32_BE,
	/** Unsigned 32 bit Little Endian */
	SND_PCM_FORMAT_U32_LE,
	/** Unsigned 32 bit Big Endian */
	SND_PCM_FORMAT_U32_BE,
	/** Float 32 bit Little Endian, Range -1.0 to 1.0 */
	SND_PCM_FORMAT_FLOAT_LE,
	/** Float 32 bit Big Endian, Range -1.0 to 1.0 */
	SND_PCM_FORMAT_FLOAT_BE,
	/** Float 64 bit Little Endian, Range -1.0 to 1.0 */
	SND_PCM_FORMAT_FLOAT64_LE,
	/** Float 64 bit Big Endian, Range -1.0 to 1.0 */
	SND_PCM_FORMAT_FLOAT64_BE,
	/** IEC-958 Little Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
	/** IEC-958 Big Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
	/** Mu-Law */
	SND_PCM_FORMAT_MU_LAW,
	/** A-Law */
	SND_PCM_FORMAT_A_LAW,
	/** Ima-ADPCM */
	SND_PCM_FORMAT_IMA_ADPCM,
	/** MPEG */
	SND_PCM_FORMAT_MPEG,
	/** GSM */
	SND_PCM_FORMAT_GSM,
	/** Signed 20bit Little Endian in 4bytes format, LSB justified */
	SND_PCM_FORMAT_S20_LE,
	/** Signed 20bit Big Endian in 4bytes format, LSB justified */
	SND_PCM_FORMAT_S20_BE,
	/** Unsigned 20bit Little Endian in 4bytes format, LSB justified */
	SND_PCM_FORMAT_U20_LE,
	/** Unsigned 20bit Big Endian in 4bytes format, LSB justified */
	SND_PCM_FORMAT_U20_BE,
	/** Special */
	SND_PCM_FORMAT_SPECIAL = 31,
	/** Signed 24bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_S24_3LE = 32,
	/** Signed 24bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_S24_3BE,
	/** Unsigned 24bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_U24_3LE,
	/** Unsigned 24bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_U24_3BE,
	/** Signed 20bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_S20_3LE,
	/** Signed 20bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_S20_3BE,
	/** Unsigned 20bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_U20_3LE,
	/** Unsigned 20bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_U20_3BE,
	/** Signed 18bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_S18_3LE,
	/** Signed 18bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_S18_3BE,
	/** Unsigned 18bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_U18_3LE,
	/** Unsigned 18bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_U18_3BE,
	/* G.723 (ADPCM) 24 kbit/s, 8 samples in 3 bytes */
	SND_PCM_FORMAT_G723_24,
	/* G.723 (ADPCM) 24 kbit/s, 1 sample in 1 byte */
	SND_PCM_FORMAT_G723_24_1B,
	/* G.723 (ADPCM) 40 kbit/s, 8 samples in 3 bytes */
	SND_PCM_FORMAT_G723_40,
	/* G.723 (ADPCM) 40 kbit/s, 1 sample in 1 byte */
	SND_PCM_FORMAT_G723_40_1B,
	/* Direct Stream Digital (DSD) in 1-byte samples (x8) */
	SND_PCM_FORMAT_DSD_U8,
	/* Direct Stream Digital (DSD) in 2-byte samples (x16) */
	SND_PCM_FORMAT_DSD_U16_LE,
	/* Direct Stream Digital (DSD) in 4-byte samples (x32) */
	SND_PCM_FORMAT_DSD_U32_LE,
	/* Direct Stream Digital (DSD) in 2-byte samples (x16) */
	SND_PCM_FORMAT_DSD_U16_BE,
	/* Direct Stream Digital (DSD) in 4-byte samples (x32) */
	SND_PCM_FORMAT_DSD_U32_BE,
	SND_PCM_FORMAT_LAST = SND_PCM_FORMAT_DSD_U32_BE,

#if __BYTE_ORDER == __LITTLE_ENDIAN
	/** Signed 16 bit CPU endian */
	SND_PCM_FORMAT_S16 = SND_PCM_FORMAT_S16_LE,
	/** Unsigned 16 bit CPU endian */
	SND_PCM_FORMAT_U16 = SND_PCM_FORMAT_U16_LE,
	/** Signed 24 bit CPU endian */
	SND_PCM_FORMAT_S24 = SND_PCM_FORMAT_S24_LE,
	/** Unsigned 24 bit CPU endian */
	SND_PCM_FORMAT_U24 = SND_PCM_FORMAT_U24_LE,
	/** Signed 32 bit CPU endian */
	SND_PCM_FORMAT_S32 = SND_PCM_FORMAT_S32_LE,
	/** Unsigned 32 bit CPU endian */
	SND_PCM_FORMAT_U32 = SND_PCM_FORMAT_U32_LE,
	/** Float 32 bit CPU endian */
	SND_PCM_FORMAT_FLOAT = SND_PCM_FORMAT_FLOAT_LE,
	/** Float 64 bit CPU endian */
	SND_PCM_FORMAT_FLOAT64 = SND_PCM_FORMAT_FLOAT64_LE,
	/** IEC-958 CPU Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME = SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
	/** Signed 20bit in 4bytes format, LSB justified, CPU Endian */
	SND_PCM_FORMAT_S20 = SND_PCM_FORMAT_S20_LE,
	/** Unsigned 20bit in 4bytes format, LSB justified, CPU Endian */
	SND_PCM_FORMAT_U20 = SND_PCM_FORMAT_U20_LE,
#elif __BYTE_ORDER == __BIG_ENDIAN
	/** Signed 16 bit CPU endian */
	SND_PCM_FORMAT_S16 = SND_PCM_FORMAT_S16_BE,
	/** Unsigned 16 bit CPU endian */
	SND_PCM_FORMAT_U16 = SND_PCM_FORMAT_U16_BE,
	/** Signed 24 bit CPU endian */
	SND_PCM_FORMAT_S24 = SND_PCM_FORMAT_S24_BE,
	/** Unsigned 24 bit CPU endian */
	SND_PCM_FORMAT_U24 = SND_PCM_FORMAT_U24_BE,
	/** Signed 32 bit CPU endian */
	SND_PCM_FORMAT_S32 = SND_PCM_FORMAT_S32_BE,
	/** Unsigned 32 bit CPU endian */
	SND_PCM_FORMAT_U32 = SND_PCM_FORMAT_U32_BE,
	/** Float 32 bit CPU endian */
	SND_PCM_FORMAT_FLOAT = SND_PCM_FORMAT_FLOAT_BE,
	/** Float 64 bit CPU endian */
	SND_PCM_FORMAT_FLOAT64 = SND_PCM_FORMAT_FLOAT64_BE,
	/** IEC-958 CPU Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME = SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
	/** Signed 20bit in 4bytes format, LSB justified, CPU Endian */
	SND_PCM_FORMAT_S20 = SND_PCM_FORMAT_S20_BE,
	/** Unsigned 20bit in 4bytes format, LSB justified, CPU Endian */
	SND_PCM_FORMAT_U20 = SND_PCM_FORMAT_U20_BE,
#else
#error "Unknown endian"
#endif
} snd_pcm_format_t;

/** PCM sample subformat */
typedef enum _snd_pcm_subformat {
	/** Unknown */
	SND_PCM_SUBFORMAT_UNKNOWN = -1,
	/** Standard */
	SND_PCM_SUBFORMAT_STD = 0,
	/** Maximum bits based on PCM format */
	SND_PCM_SUBFORMAT_MSBITS_MAX = 1,
	/** 20 most significant bits */
	SND_PCM_SUBFORMAT_MSBITS_20 = 2,
	/** 24 most significant bits */
	SND_PCM_SUBFORMAT_MSBITS_24 = 3,
	SND_PCM_SUBFORMAT_LAST = SND_PCM_SUBFORMAT_MSBITS_24
} snd_pcm_subformat_t;

/** PCM state */
typedef enum _snd_pcm_state {
	/** Open */
	SND_PCM_STATE_OPEN = 0,
	/** Setup installed */
	SND_PCM_STATE_SETUP,
	/** Ready to start */
	SND_PCM_STATE_PREPARED,
	/** Running */
	SND_PCM_STATE_RUNNING,
	/** Stopped: underrun (playback) or overrun (capture) detected */
	SND_PCM_STATE_XRUN,
	/** Draining: running (playback) or stopped (capture) */
	SND_PCM_STATE_DRAINING,
	/** Paused */
	SND_PCM_STATE_PAUSED,
	/** Hardware is suspended */
	SND_PCM_STATE_SUSPENDED,
	/** Hardware is disconnected */
	SND_PCM_STATE_DISCONNECTED,
	SND_PCM_STATE_LAST = SND_PCM_STATE_DISCONNECTED,
	/** Private - used internally in the library - do not use*/
	SND_PCM_STATE_PRIVATE1 = 1024
} snd_pcm_state_t;

/** PCM start mode */
typedef enum _snd_pcm_start {
	/** Automatic start on data read/write */
	SND_PCM_START_DATA = 0,
	/** Explicit start */
	SND_PCM_START_EXPLICIT,
	SND_PCM_START_LAST = SND_PCM_START_EXPLICIT
} snd_pcm_start_t;

/** PCM xrun mode */
typedef enum _snd_pcm_xrun {
	/** Xrun detection disabled */
	SND_PCM_XRUN_NONE = 0,
	/** Stop on xrun detection */
	SND_PCM_XRUN_STOP,
	SND_PCM_XRUN_LAST = SND_PCM_XRUN_STOP
} snd_pcm_xrun_t;

/** PCM timestamp mode */
typedef enum _snd_pcm_tstamp {
	/** No timestamp */
	SND_PCM_TSTAMP_NONE = 0,
	/** Update timestamp at every hardware position update */
	SND_PCM_TSTAMP_ENABLE,
	/** Equivalent with #SND_PCM_TSTAMP_ENABLE,
	 * just for compatibility with older versions
	 */
	SND_PCM_TSTAMP_MMAP = SND_PCM_TSTAMP_ENABLE,
	SND_PCM_TSTAMP_LAST = SND_PCM_TSTAMP_ENABLE
} snd_pcm_tstamp_t;

/** PCM timestamp type */
typedef enum _snd_pcm_tstamp_type {
	SND_PCM_TSTAMP_TYPE_GETTIMEOFDAY = 0,	/**< gettimeofday equivalent */
	SND_PCM_TSTAMP_TYPE_MONOTONIC,	/**< posix_clock_monotonic equivalent */
	SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW,	/**< monotonic_raw (no NTP) */
	SND_PCM_TSTAMP_TYPE_LAST = SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW,
} snd_pcm_tstamp_type_t;

/** PCM audio timestamp type */
typedef enum _snd_pcm_audio_tstamp_type {
	/**
	 * first definition for backwards compatibility only,
	 * maps to wallclock/link time for HDAudio playback and DEFAULT/DMA time for everything else
	 */
	SND_PCM_AUDIO_TSTAMP_TYPE_COMPAT = 0,
	SND_PCM_AUDIO_TSTAMP_TYPE_DEFAULT = 1,           /**< DMA time, reported as per hw_ptr */
	SND_PCM_AUDIO_TSTAMP_TYPE_LINK = 2,	           /**< link time reported by sample or wallclock counter, reset on startup */
	SND_PCM_AUDIO_TSTAMP_TYPE_LINK_ABSOLUTE = 3,	   /**< link time reported by sample or wallclock counter, not reset on startup */
	SND_PCM_AUDIO_TSTAMP_TYPE_LINK_ESTIMATED = 4,    /**< link time estimated indirectly */
	SND_PCM_AUDIO_TSTAMP_TYPE_LINK_SYNCHRONIZED = 5, /**< link time synchronized with system time */
	SND_PCM_AUDIO_TSTAMP_TYPE_LAST = SND_PCM_AUDIO_TSTAMP_TYPE_LINK_SYNCHRONIZED
} snd_pcm_audio_tstamp_type_t;

/** PCM audio timestamp config */
typedef struct _snd_pcm_audio_tstamp_config {
	/* 5 of max 16 bits used */
	unsigned int type_requested:4; /**< requested audio tstamp type */
	unsigned int report_delay:1; /**< add total delay to A/D or D/A */
} snd_pcm_audio_tstamp_config_t;

/** PCM audio timestamp report */
typedef struct _snd_pcm_audio_tstamp_report {
	/* 6 of max 16 bits used for bit-fields */

	unsigned int valid:1; /**< for backwards compatibility */
	unsigned int actual_type:4; /**< actual type if hardware could not support requested timestamp */

	unsigned int accuracy_report:1; /**< 0 if accuracy unknown, 1 if accuracy field is valid */
	unsigned int accuracy; /**< up to 4.29s in ns units, will be packed in separate field  */
} snd_pcm_audio_tstamp_report_t;

/** Unsigned frames quantity */
typedef unsigned long snd_pcm_uframes_t;
/** Signed frames quantity */
typedef long snd_pcm_sframes_t;

/** Non blocking mode (flag for open mode) \hideinitializer */
#define SND_PCM_NONBLOCK		0x00000001
/** Async notification (flag for open mode) \hideinitializer */
#define SND_PCM_ASYNC			0x00000002
/** Return EINTR instead blocking (wait operation) */
#define SND_PCM_EINTR			0x00000080
/** In an abort state (internal, not allowed for open) */
#define SND_PCM_ABORT			0x00008000
/** Disable automatic (but not forced!) rate resamplinig */
#define SND_PCM_NO_AUTO_RESAMPLE	0x00010000
/** Disable automatic (but not forced!) channel conversion */
#define SND_PCM_NO_AUTO_CHANNELS	0x00020000
/** Disable automatic (but not forced!) format conversion */
#define SND_PCM_NO_AUTO_FORMAT		0x00040000
/** Disable soft volume control */
#define SND_PCM_NO_SOFTVOL		0x00080000

/** PCM handle */
typedef struct _snd_pcm snd_pcm_t;

/** PCM type */
enum _snd_pcm_type {
	/** Kernel level PCM */
	SND_PCM_TYPE_HW = 0,
	/** Hooked PCM */
	SND_PCM_TYPE_HOOKS,
	/** One or more linked PCM with exclusive access to selected
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
	/** Linear Integer <-> Linear Float format conversion PCM */
	SND_PCM_TYPE_LINEAR_FLOAT,
	/** LADSPA integration plugin */
	SND_PCM_TYPE_LADSPA,
	/** Direct Mixing plugin */
	SND_PCM_TYPE_DMIX,
	/** Jack Audio Connection Kit plugin */
	SND_PCM_TYPE_JACK,
	/** Direct Snooping plugin */
	SND_PCM_TYPE_DSNOOP,
	/** Direct Sharing plugin */
	SND_PCM_TYPE_DSHARE,
	/** IEC958 subframe plugin */
	SND_PCM_TYPE_IEC958,
	/** Soft volume plugin */
	SND_PCM_TYPE_SOFTVOL,
	/** External I/O plugin */
	SND_PCM_TYPE_IOPLUG,
	/** External filter plugin */
	SND_PCM_TYPE_EXTPLUG,
	/** Mmap-emulation plugin */
	SND_PCM_TYPE_MMAP_EMUL,
	SND_PCM_TYPE_LAST = SND_PCM_TYPE_MMAP_EMUL
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

/** PCM synchronization ID */
typedef union _snd_pcm_sync_id {
	/** 8-bit ID */
	unsigned char id[16];
	/** 16-bit ID */
	unsigned short id16[8];
	/** 32-bit ID */
	unsigned int id32[4];
} snd_pcm_sync_id_t;

/** synchronization ID size (see snd_pcm_hw_params_get_sync) */
#define SND_PCM_HW_PARAMS_SYNC_SIZE	16

/** Infinite wait for snd_pcm_wait() */
#define SND_PCM_WAIT_INFINITE		(-1)
/** Wait for next i/o in snd_pcm_wait() */
#define SND_PCM_WAIT_IO			(-10001)
/** Wait for drain in snd_pcm_wait() */
#define SND_PCM_WAIT_DRAIN		(-10002)

/** #SND_PCM_TYPE_METER scope handle */
typedef struct _snd_pcm_scope snd_pcm_scope_t;


/** channel mapping API version number */
#define SND_CHMAP_API_VERSION	((1 << 16) | (0 << 8) | 1)

/** channel map list type */
enum snd_pcm_chmap_type {
	SND_CHMAP_TYPE_NONE = 0,/**< unspecified channel position */
	SND_CHMAP_TYPE_FIXED,	/**< fixed channel position */
	SND_CHMAP_TYPE_VAR,	/**< freely swappable channel position */
	SND_CHMAP_TYPE_PAIRED,	/**< pair-wise swappable channel position */
	SND_CHMAP_TYPE_LAST = SND_CHMAP_TYPE_PAIRED, /**< last entry */
};

/** channel positions */
enum snd_pcm_chmap_position {
	SND_CHMAP_UNKNOWN = 0,	/**< unspecified */
	SND_CHMAP_NA,		/**< N/A, silent */
	SND_CHMAP_MONO,		/**< mono stream */
	SND_CHMAP_FL,		/**< front left */
	SND_CHMAP_FR,		/**< front right */
	SND_CHMAP_RL,		/**< rear left */
	SND_CHMAP_RR,		/**< rear right */
	SND_CHMAP_FC,		/**< front center */
	SND_CHMAP_LFE,		/**< LFE */
	SND_CHMAP_SL,		/**< side left */
	SND_CHMAP_SR,		/**< side right */
	SND_CHMAP_RC,		/**< rear center */
	SND_CHMAP_FLC,		/**< front left center */
	SND_CHMAP_FRC,		/**< front right center */
	SND_CHMAP_RLC,		/**< rear left center */
	SND_CHMAP_RRC,		/**< rear right center */
	SND_CHMAP_FLW,		/**< front left wide */
	SND_CHMAP_FRW,		/**< front right wide */
	SND_CHMAP_FLH,		/**< front left high */
	SND_CHMAP_FCH,		/**< front center high */
	SND_CHMAP_FRH,		/**< front right high */
	SND_CHMAP_TC,		/**< top center */
	SND_CHMAP_TFL,		/**< top front left */
	SND_CHMAP_TFR,		/**< top front right */
	SND_CHMAP_TFC,		/**< top front center */
	SND_CHMAP_TRL,		/**< top rear left */
	SND_CHMAP_TRR,		/**< top rear right */
	SND_CHMAP_TRC,		/**< top rear center */
	SND_CHMAP_TFLC,		/**< top front left center */
	SND_CHMAP_TFRC,		/**< top front right center */
	SND_CHMAP_TSL,		/**< top side left */
	SND_CHMAP_TSR,		/**< top side right */
	SND_CHMAP_LLFE,		/**< left LFE */
	SND_CHMAP_RLFE,		/**< right LFE */
	SND_CHMAP_BC,		/**< bottom center */
	SND_CHMAP_BLC,		/**< bottom left center */
	SND_CHMAP_BRC,		/**< bottom right center */
	SND_CHMAP_LAST = SND_CHMAP_BRC,
};

/** bitmask for channel position */
#define SND_CHMAP_POSITION_MASK		0xffff

/** bit flag indicating the channel is phase inverted */
#define SND_CHMAP_PHASE_INVERSE		(0x01 << 16)
/** bit flag indicating the non-standard channel value */
#define SND_CHMAP_DRIVER_SPEC		(0x02 << 16)

/** the channel map header */
typedef struct snd_pcm_chmap {
	unsigned int channels;	/**< number of channels */
	unsigned int pos[0];	/**< channel position array */
} snd_pcm_chmap_t;

/** the header of array items returned from snd_pcm_query_chmaps() */
typedef struct snd_pcm_chmap_query {
	enum snd_pcm_chmap_type type;	/**< channel map type */
	snd_pcm_chmap_t map;		/**< available channel map */
} snd_pcm_chmap_query_t;


/** \} */

/**
 * \defgroup PCM_Info Stream Information
 * \ingroup PCM
 * See the \ref pcm page for more details.
 * \{
 */

size_t snd_pcm_info_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_pcm_info_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_pcm_info_alloca(ptr) __snd_alloca(ptr, snd_pcm_info)

size_t snd_pcm_hw_params_sizeof(void);

/** \} */

/**
 * \defgroup PCM_Hook Hook Extension
 * \ingroup PCM
 * See the \ref pcm page for more details.
 * \{
 */

/** type of pcm hook */
typedef enum _snd_pcm_hook_type {
	SND_PCM_HOOK_TYPE_HW_PARAMS = 0,
	SND_PCM_HOOK_TYPE_HW_FREE,
	SND_PCM_HOOK_TYPE_CLOSE,
	SND_PCM_HOOK_TYPE_LAST = SND_PCM_HOOK_TYPE_CLOSE
} snd_pcm_hook_type_t;

/** PCM hook container */
typedef struct _snd_pcm_hook snd_pcm_hook_t;
/** PCM hook callback function */
typedef int (*snd_pcm_hook_func_t)(snd_pcm_hook_t *hook);


/** \} */

/**
 * \defgroup PCM_Scope Scope Plugin Extension
 * \ingroup PCM
 * See the \ref pcm page for more details.
 * \{
 */

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


/** \} */

/**
 * \defgroup PCM_Simple Simple setup functions
 * \ingroup PCM
 * See the \ref pcm page for more details.
 * \{
 */

/** Simple PCM latency type */
typedef enum _snd_spcm_latency {
	/** standard latency - for standard playback or capture
	    (estimated latency in one direction 350ms) */
	SND_SPCM_LATENCY_STANDARD = 0,
	/** medium latency - software phones etc.
	    (estimated latency in one direction maximally 25ms */
	SND_SPCM_LATENCY_MEDIUM,
	/** realtime latency - realtime applications (effect processors etc.)
	    (estimated latency in one direction 5ms and better) */
	SND_SPCM_LATENCY_REALTIME
} snd_spcm_latency_t;

/** Simple PCM xrun type */
typedef enum _snd_spcm_xrun_type {
	/** driver / library will ignore all xruns, the stream runs forever */
	SND_SPCM_XRUN_IGNORE = 0,
	/** driver / library stops the stream when an xrun occurs */
	SND_SPCM_XRUN_STOP
} snd_spcm_xrun_type_t;

/** Simple PCM duplex type */
typedef enum _snd_spcm_duplex_type {
	/** liberal duplex - the buffer and period sizes might not match */
	SND_SPCM_DUPLEX_LIBERAL = 0,
	/** pedantic duplex - the buffer and period sizes MUST match */
	SND_SPCM_DUPLEX_PEDANTIC
} snd_spcm_duplex_type_t;


/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_PCM_TYPES_H */
