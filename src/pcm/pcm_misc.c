/*
 *  PCM Interface - misc routines
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@perex.cz>
 *
 *
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
  
#include "pcm_local.h"
#include "bswap.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


/**
 * \brief Return sign info for a PCM sample linear format
 * \param format Format
 * \return 0 unsigned, 1 signed, a negative error code if format is not linear
 */
int snd_pcm_format_signed(snd_pcm_format_t format)
{
	switch (format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_S20_LE:
	case SNDRV_PCM_FORMAT_S20_BE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S18_3BE:
		return 1;
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
	case SNDRV_PCM_FORMAT_U20_LE:
	case SNDRV_PCM_FORMAT_U20_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
	case SNDRV_PCM_FORMAT_U20_3LE:
	case SNDRV_PCM_FORMAT_U20_3BE:
	case SNDRV_PCM_FORMAT_U18_3LE:
	case SNDRV_PCM_FORMAT_U18_3BE:
	case SNDRV_PCM_FORMAT_DSD_U8:
	case SNDRV_PCM_FORMAT_DSD_U16_LE:
	case SNDRV_PCM_FORMAT_DSD_U32_LE:
	case SNDRV_PCM_FORMAT_DSD_U16_BE:
	case SNDRV_PCM_FORMAT_DSD_U32_BE:
		return 0;
	default:
		return -EINVAL;
	}
}

/**
 * \brief Return sign info for a PCM sample linear format
 * \param format Format
 * \return 0 signed, 1 unsigned, a negative error code if format is not linear
 */
int snd_pcm_format_unsigned(snd_pcm_format_t format)
{
	int val;

	val = snd_pcm_format_signed(format);
	if (val < 0)
		return val;
	return !val;
}

/**
 * \brief Return linear info for a PCM sample format
 * \param format Format
 * \return 0 non linear, 1 linear
 */
int snd_pcm_format_linear(snd_pcm_format_t format)
{
	return snd_pcm_format_signed(format) >= 0;
}

/**
 * \brief Return float info for a PCM sample format
 * \param format Format
 * \return 0 non float, 1 float
 */
int snd_pcm_format_float(snd_pcm_format_t format)
{
	switch (format) {
	case SNDRV_PCM_FORMAT_FLOAT_LE:
	case SNDRV_PCM_FORMAT_FLOAT_BE:
	case SNDRV_PCM_FORMAT_FLOAT64_LE:
	case SNDRV_PCM_FORMAT_FLOAT64_BE:
		return 1;
	default:
		return 0;
	}
}

/**
 * \brief Return endian info for a PCM sample format
 * \param format Format
 * \return 0 big endian, 1 little endian, a negative error code if endian independent
 */
int snd_pcm_format_little_endian(snd_pcm_format_t format)
{
	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S20_LE:
	case SNDRV_PCM_FORMAT_U20_LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_FLOAT_LE:
	case SNDRV_PCM_FORMAT_FLOAT64_LE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U20_3LE:
	case SNDRV_PCM_FORMAT_U18_3LE:
	case SNDRV_PCM_FORMAT_DSD_U16_LE:
	case SNDRV_PCM_FORMAT_DSD_U32_LE:
		return 1;
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_BE:
	case SNDRV_PCM_FORMAT_S20_BE:
	case SNDRV_PCM_FORMAT_U20_BE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_BE:
	case SNDRV_PCM_FORMAT_FLOAT_BE:
	case SNDRV_PCM_FORMAT_FLOAT64_BE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_S18_3BE:
	case SNDRV_PCM_FORMAT_U24_3BE:
	case SNDRV_PCM_FORMAT_U20_3BE:
	case SNDRV_PCM_FORMAT_U18_3BE:
	case SNDRV_PCM_FORMAT_DSD_U16_BE:
	case SNDRV_PCM_FORMAT_DSD_U32_BE:
		return 0;
	default:
		return -EINVAL;
	}
}

/**
 * \brief Return endian info for a PCM sample format
 * \param format Format
 * \return 0 little endian, 1 big endian, a negative error code if endian independent
 */
int snd_pcm_format_big_endian(snd_pcm_format_t format)
{
	int val;

	val = snd_pcm_format_little_endian(format);
	if (val < 0)
		return val;
	return !val;
}

/**
 * \brief Return endian info for a PCM sample format
 * \param format Format
 * \return 0 swapped, 1 CPU endian, a negative error code if endian independent
 */
int snd_pcm_format_cpu_endian(snd_pcm_format_t format)
{
#ifdef SNDRV_LITTLE_ENDIAN
	return snd_pcm_format_little_endian(format);
#else
	return snd_pcm_format_big_endian(format);
#endif
}

/**
 * \brief Return the bit-width of the format
 * \param format Sample format
 * \return the bit-width of the format, or a negative error code if not applicable
 */
int snd_pcm_format_width(snd_pcm_format_t format)
{
	switch (format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_DSD_U8:
		return 8;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
	case SNDRV_PCM_FORMAT_DSD_U16_LE:
	case SNDRV_PCM_FORMAT_DSD_U16_BE:
		return 16;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S18_3BE:
	case SNDRV_PCM_FORMAT_U18_3LE:
	case SNDRV_PCM_FORMAT_U18_3BE:
		return 18;
	case SNDRV_PCM_FORMAT_S20_LE:
	case SNDRV_PCM_FORMAT_S20_BE:
	case SNDRV_PCM_FORMAT_U20_LE:
	case SNDRV_PCM_FORMAT_U20_BE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_U20_3LE:
	case SNDRV_PCM_FORMAT_U20_3BE:
		return 20;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
		return 24;
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
	case SNDRV_PCM_FORMAT_FLOAT_LE:
	case SNDRV_PCM_FORMAT_FLOAT_BE:
	case SNDRV_PCM_FORMAT_DSD_U32_LE:
	case SNDRV_PCM_FORMAT_DSD_U32_BE:
		return 32;
	case SNDRV_PCM_FORMAT_FLOAT64_LE:
	case SNDRV_PCM_FORMAT_FLOAT64_BE:
		return 64;
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE:
		return 32;
	case SNDRV_PCM_FORMAT_MU_LAW:
	case SNDRV_PCM_FORMAT_A_LAW:
		return 8;
	case SNDRV_PCM_FORMAT_IMA_ADPCM:
		return 4;
	default:
		return -EINVAL;
	}
}

/**
 * \brief Return the physical bit-width of the format (bits needed to store a PCM sample)
 * \param format Sample format
 * \return the physical bit-width of the format, or a negative error code if not applicable
 */
int snd_pcm_format_physical_width(snd_pcm_format_t format)
{
	switch (format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_DSD_U8:
		return 8;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
	case SNDRV_PCM_FORMAT_DSD_U16_LE:
	case SNDRV_PCM_FORMAT_DSD_U16_BE:
		return 16;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S18_3BE:
	case SNDRV_PCM_FORMAT_U18_3LE:
	case SNDRV_PCM_FORMAT_U18_3BE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_U20_3LE:
	case SNDRV_PCM_FORMAT_U20_3BE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
		return 24;
	case SNDRV_PCM_FORMAT_S20_LE:
	case SNDRV_PCM_FORMAT_S20_BE:
	case SNDRV_PCM_FORMAT_U20_LE:
	case SNDRV_PCM_FORMAT_U20_BE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
	case SNDRV_PCM_FORMAT_FLOAT_LE:
	case SNDRV_PCM_FORMAT_FLOAT_BE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE:
	case SNDRV_PCM_FORMAT_DSD_U32_LE:
	case SNDRV_PCM_FORMAT_DSD_U32_BE:
		return 32;
	case SNDRV_PCM_FORMAT_FLOAT64_LE:
	case SNDRV_PCM_FORMAT_FLOAT64_BE:
		return 64;
	case SNDRV_PCM_FORMAT_MU_LAW:
	case SNDRV_PCM_FORMAT_A_LAW:
		return 8;
	case SNDRV_PCM_FORMAT_IMA_ADPCM:
		return 4;
	default:
		return -EINVAL;
	}
}

/**
 * \brief Return bytes needed to store a quantity of PCM sample
 * \param format Sample format
 * \param samples Samples count
 * \return bytes needed, a negative error code if not integer or unknown
 */
ssize_t snd_pcm_format_size(snd_pcm_format_t format, size_t samples)
{
	switch (format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_DSD_U8:
		return samples;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
	case SNDRV_PCM_FORMAT_DSD_U16_LE:
	case SNDRV_PCM_FORMAT_DSD_U16_BE:
		return samples * 2;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S18_3BE:
	case SNDRV_PCM_FORMAT_U18_3LE:
	case SNDRV_PCM_FORMAT_U18_3BE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_U20_3LE:
	case SNDRV_PCM_FORMAT_U20_3BE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
		return samples * 3;
	case SNDRV_PCM_FORMAT_S20_LE:
	case SNDRV_PCM_FORMAT_S20_BE:
	case SNDRV_PCM_FORMAT_U20_LE:
	case SNDRV_PCM_FORMAT_U20_BE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
	case SNDRV_PCM_FORMAT_FLOAT_LE:
	case SNDRV_PCM_FORMAT_FLOAT_BE:
	case SNDRV_PCM_FORMAT_DSD_U32_LE:
	case SNDRV_PCM_FORMAT_DSD_U32_BE:
		return samples * 4;
	case SNDRV_PCM_FORMAT_FLOAT64_LE:
	case SNDRV_PCM_FORMAT_FLOAT64_BE:
		return samples * 8;
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE:
		return samples * 4;
	case SNDRV_PCM_FORMAT_MU_LAW:
	case SNDRV_PCM_FORMAT_A_LAW:
		return samples;
	case SNDRV_PCM_FORMAT_IMA_ADPCM:
		if (samples & 1)
			return -EINVAL;
		return samples / 2;
	default:
		assert(0);
		return -EINVAL;
	}
}

/**
 * \brief Return 64 bit expressing silence for a PCM sample format
 * \param format Sample format
 * \return silence 64 bit word
 */
uint64_t snd_pcm_format_silence_64(snd_pcm_format_t format)
{
	switch (format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_S20_LE:
	case SNDRV_PCM_FORMAT_S20_BE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S18_3BE:
		return 0;
	case SNDRV_PCM_FORMAT_U8:
		return 0x8080808080808080ULL;
	case SNDRV_PCM_FORMAT_DSD_U8:
	case SNDRV_PCM_FORMAT_DSD_U16_LE:
	case SNDRV_PCM_FORMAT_DSD_U32_LE:
	case SNDRV_PCM_FORMAT_DSD_U16_BE:
	case SNDRV_PCM_FORMAT_DSD_U32_BE:
		return 0x6969696969696969ULL;
#ifdef SNDRV_LITTLE_ENDIAN
	case SNDRV_PCM_FORMAT_U16_LE:
		return 0x8000800080008000ULL;
	case SNDRV_PCM_FORMAT_U20_LE:
		return 0x0008000000080000ULL;
	case SNDRV_PCM_FORMAT_U24_LE:
		return 0x0080000000800000ULL;
	case SNDRV_PCM_FORMAT_U32_LE:
		return 0x8000000080000000ULL;
	case SNDRV_PCM_FORMAT_U16_BE:
		return 0x0080008000800080ULL;
	case SNDRV_PCM_FORMAT_U20_BE:
		return 0x0000080000000800ULL;
	case SNDRV_PCM_FORMAT_U24_BE:
		return 0x0000800000008000ULL;
	case SNDRV_PCM_FORMAT_U32_BE:
		return 0x0000008000000080ULL;
	case SNDRV_PCM_FORMAT_U24_3LE:
		return 0x0000800000800000ULL;
	case SNDRV_PCM_FORMAT_U24_3BE:
		return 0x0080000080000080ULL;
	case SNDRV_PCM_FORMAT_U20_3LE:
		return 0x0000080000080000ULL;
	case SNDRV_PCM_FORMAT_U20_3BE:
		return 0x0008000008000008ULL;
	case SNDRV_PCM_FORMAT_U18_3LE:
		return 0x0000020000020000ULL;
	case SNDRV_PCM_FORMAT_U18_3BE:
		return 0x0002000002000002ULL;
#else
	case SNDRV_PCM_FORMAT_U16_LE:
		return 0x0080008000800080ULL;
	case SNDRV_PCM_FORMAT_U20_LE:
		return 0x0000080000000800ULL;
	case SNDRV_PCM_FORMAT_U24_LE:
		return 0x0000800000008000ULL;
	case SNDRV_PCM_FORMAT_U32_LE:
		return 0x0000008000000080ULL;
	case SNDRV_PCM_FORMAT_U16_BE:
		return 0x8000800080008000ULL;
	case SNDRV_PCM_FORMAT_U20_BE:
		return 0x0008000000080000ULL;
	case SNDRV_PCM_FORMAT_U24_BE:
		return 0x0080000000800000ULL;
	case SNDRV_PCM_FORMAT_U32_BE:
		return 0x8000000080000000ULL;
	case SNDRV_PCM_FORMAT_U24_3LE:
		return 0x0080000080000080ULL;
	case SNDRV_PCM_FORMAT_U24_3BE:
		return 0x0000800000800000ULL;
	case SNDRV_PCM_FORMAT_U20_3LE:
		return 0x0008000008000008ULL;
	case SNDRV_PCM_FORMAT_U20_3BE:
		return 0x0000080000080000ULL;
	case SNDRV_PCM_FORMAT_U18_3LE:
		return 0x0002000002000002ULL;
	case SNDRV_PCM_FORMAT_U18_3BE:
		return 0x0000020000020000ULL;
#endif
	case SNDRV_PCM_FORMAT_FLOAT_LE:
	{
		union {
			float f[2];
			uint64_t i;
		} u;
		u.f[0] = u.f[1] = 0.0;
#ifdef SNDRV_LITTLE_ENDIAN
		return u.i;
#else
		return bswap_64(u.i);
#endif
	}
	case SNDRV_PCM_FORMAT_FLOAT64_LE:
	{
		union {
			double f;
			uint64_t i;
		} u;
		u.f = 0.0;
#ifdef SNDRV_LITTLE_ENDIAN
		return u.i;
#else
		return bswap_64(u.i);
#endif
	}
	case SNDRV_PCM_FORMAT_FLOAT_BE:		
	{
		union {
			float f[2];
			uint64_t i;
		} u;
		u.f[0] = u.f[1] = 0.0;
#ifdef SNDRV_LITTLE_ENDIAN
		return bswap_64(u.i);
#else
		return u.i;
#endif
	}
	case SNDRV_PCM_FORMAT_FLOAT64_BE:
	{
		union {
			double f;
			uint64_t i;
		} u;
		u.f = 0.0;
#ifdef SNDRV_LITTLE_ENDIAN
		return bswap_64(u.i);
#else
		return u.i;
#endif
	}
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE:
		return 0;	
	case SNDRV_PCM_FORMAT_MU_LAW:
		return 0x7f7f7f7f7f7f7f7fULL;
	case SNDRV_PCM_FORMAT_A_LAW:
		return 0x5555555555555555ULL;
	case SNDRV_PCM_FORMAT_IMA_ADPCM:	/* special case */
	case SNDRV_PCM_FORMAT_MPEG:
	case SNDRV_PCM_FORMAT_GSM:
	case SNDRV_PCM_FORMAT_SPECIAL:
		return 0;
	default:
		assert(0);
		return 0;
	}
	return 0;
}

/**
 * \brief Return 32 bit expressing silence for a PCM sample format
 * \param format Sample format
 * \return silence 32 bit word
 */
uint32_t snd_pcm_format_silence_32(snd_pcm_format_t format)
{
	assert(snd_pcm_format_physical_width(format) <= 32);
	return (uint32_t)snd_pcm_format_silence_64(format);
}

/**
 * \brief Return 16 bit expressing silence for a PCM sample format
 * \param format Sample format
 * \return silence 16 bit word
 */
uint16_t snd_pcm_format_silence_16(snd_pcm_format_t format)
{
	assert(snd_pcm_format_physical_width(format) <= 16);
	return (uint16_t)snd_pcm_format_silence_64(format);
}

/**
 * \brief Return 8 bit expressing silence for a PCM sample format
 * \param format Sample format
 * \return silence 8 bit word
 */
uint8_t snd_pcm_format_silence(snd_pcm_format_t format)
{
	assert(snd_pcm_format_physical_width(format) <= 8);
	return (uint8_t)snd_pcm_format_silence_64(format);
}

/**
 * \brief Silence a PCM samples buffer
 * \param format Sample format
 * \param data Buffer
 * \param samples Samples count
 * \return 0 if successful or a negative error code
 */
int snd_pcm_format_set_silence(snd_pcm_format_t format, void *data, unsigned int samples)
{
	if (samples == 0)
		return 0;
	switch (snd_pcm_format_physical_width(format)) {
	case 4: {
		uint8_t silence = snd_pcm_format_silence_64(format);
		unsigned int samples1;
		if (samples % 2 != 0)
			return -EINVAL;
		samples1 = samples / 2;
		memset(data, silence, samples1);
		break;
	}
	case 8: {
		uint8_t silence = snd_pcm_format_silence_64(format);
		memset(data, silence, samples);
		break;
	}
	case 16: {
		uint16_t silence = snd_pcm_format_silence_64(format);
		uint16_t *pdata = (uint16_t *)data;
		if (! silence)
			memset(data, 0, samples * 2);
		else {
			while (samples-- > 0)
				*pdata++ = silence;
		}
		break;
	}
	case 24: {
		uint32_t silence = snd_pcm_format_silence_64(format);
		uint8_t *pdata = (uint8_t *)data;
		if (! silence)
			memset(data, 0, samples * 3);
		else {
			while (samples-- > 0) {
#ifdef SNDRV_LITTLE_ENDIAN
				*pdata++ = silence >> 0;
				*pdata++ = silence >> 8;
				*pdata++ = silence >> 16;
#else
				*pdata++ = silence >> 16;
				*pdata++ = silence >> 8;
				*pdata++ = silence >> 0;
#endif
			}
		}
		break;
	}
	case 32: {
		uint32_t silence = snd_pcm_format_silence_64(format);
		uint32_t *pdata = (uint32_t *)data;
		if (! silence)
			memset(data, 0, samples * 4);
		else {
			while (samples-- > 0)
				*pdata++ = silence;
		}
		break;
	}
	case 64: {
		uint64_t silence = snd_pcm_format_silence_64(format);
		uint64_t *pdata = (uint64_t *)data;
		if (! silence)
			memset(data, 0, samples * 8);
		else {
			while (samples-- > 0)
				*pdata++ = silence;
		}
		break;
	}
	default:
		assert(0);
		return -EINVAL;
	}
	return 0;
}

static const int linear_formats[5][2][2] = {
	{ { SNDRV_PCM_FORMAT_S8, SNDRV_PCM_FORMAT_S8 },
	  { SNDRV_PCM_FORMAT_U8, SNDRV_PCM_FORMAT_U8 } },
	{ { SNDRV_PCM_FORMAT_S16_LE, SNDRV_PCM_FORMAT_S16_BE },
	  { SNDRV_PCM_FORMAT_U16_LE, SNDRV_PCM_FORMAT_U16_BE } },
	{ { SNDRV_PCM_FORMAT_S20_LE, SNDRV_PCM_FORMAT_S20_BE },
	  { SNDRV_PCM_FORMAT_U20_LE, SNDRV_PCM_FORMAT_U20_BE } },
	{ { SNDRV_PCM_FORMAT_S24_LE, SNDRV_PCM_FORMAT_S24_BE },
	  { SNDRV_PCM_FORMAT_U24_LE, SNDRV_PCM_FORMAT_U24_BE } },
	{ { SNDRV_PCM_FORMAT_S32_LE, SNDRV_PCM_FORMAT_S32_BE },
	  { SNDRV_PCM_FORMAT_U32_LE, SNDRV_PCM_FORMAT_U32_BE } }
};

static const int linear24_formats[3][2][2] = {
	{ { SNDRV_PCM_FORMAT_S24_3LE, SNDRV_PCM_FORMAT_S24_3BE },
	  { SNDRV_PCM_FORMAT_U24_3LE, SNDRV_PCM_FORMAT_U24_3BE } },
	{ { SNDRV_PCM_FORMAT_S20_3LE, SNDRV_PCM_FORMAT_S20_3BE },
	  { SNDRV_PCM_FORMAT_U20_3LE, SNDRV_PCM_FORMAT_U20_3BE } },
	{ { SNDRV_PCM_FORMAT_S18_3LE, SNDRV_PCM_FORMAT_S18_3BE },
	  { SNDRV_PCM_FORMAT_U18_3LE, SNDRV_PCM_FORMAT_U18_3BE } },
};

/**
 * \brief Compose a PCM sample linear format
 * \param width Nominal bits per sample
 * \param pwidth Physical bit width of the format
 * \param unsignd Sign: 0 signed, 1 unsigned
 * \param big_endian Endian: 0 little endian, 1 big endian
 * \return The matching format type, or #SND_PCM_FORMAT_UNKNOWN if no match
 */
snd_pcm_format_t snd_pcm_build_linear_format(int width, int pwidth, int unsignd, int big_endian)
{
	if (pwidth == 24) {
		switch (width) {
		case 24:
			width = 0;
			break;
		case 20:
			width = 1;
			break;
		case 18:
			width = 2;
			break;
		default:
			return SND_PCM_FORMAT_UNKNOWN;
		}
		return linear24_formats[width][!!unsignd][!!big_endian];
	} else {
		switch (width) {
		case 8:
			width = 0;
			break;
		case 16:
			width = 1;
			break;
		case 20:
			width = 2;
			break;
		case 24:
			width = 3;
			break;
		case 32:
			width = 4;
			break;
		default:
			return SND_PCM_FORMAT_UNKNOWN;
		}
		return linear_formats[width][!!unsignd][!!big_endian];
	}
}

/**
 * \brief Parse control element id from the config
 * \param conf the config tree to parse
 * \param ctl_id the pointer to store the resultant control element id
 * \param cardp the pointer to store the card index
 * \param cchannelsp the pointer to store the number of channels (optional)
 * \param hwctlp the pointer to store the h/w control flag (optional)
 * \return 0 if successful, or a negative error code
 *
 * \deprecated	Since 1.2.5
 * This function parses the given config tree to retrieve the control element id
 * and the card index.  It's used by softvol.  External PCM plugins can use this
 * function for creating or assigining their controls.
 *
 * cchannelsp and hwctlp arguments are optional.  Set NULL if not necessary.
 */
int snd_pcm_parse_control_id(snd_config_t *conf, snd_ctl_elem_id_t *ctl_id, int *cardp,
			     int *cchannelsp, int *hwctlp)
{
	snd_config_iterator_t i, next;
	int iface = SND_CTL_ELEM_IFACE_MIXER;
	const char *name = NULL;
	long index = 0;
	long device = -1;
	long subdevice = -1;
	int err;

	assert(ctl_id && cardp);

	*cardp = -1;
	if (cchannelsp)
		*cchannelsp = 2;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "card") == 0) {
			err = snd_config_get_card(n);
			if (err < 0)
				goto _err;
			*cardp = err;
			continue;
		}
		if (strcmp(id, "iface") == 0 || strcmp(id, "interface") == 0) {
			const char *ptr;
			if ((err = snd_config_get_string(n, &ptr)) < 0) {
				SNDERR("field %s is not a string", id);
				goto _err;
			}
			if ((err = snd_config_get_ctl_iface_ascii(ptr)) < 0) {
				SNDERR("Invalid value for '%s'", id);
				goto _err;
			}
			iface = err;
			continue;
		}
		if (strcmp(id, "name") == 0) {
			if ((err = snd_config_get_string(n, &name)) < 0) {
				SNDERR("field %s is not a string", id);
				goto _err;
			}
			continue;
		}
		if (strcmp(id, "index") == 0) {
			if ((err = snd_config_get_integer(n, &index)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			continue;
		}
		if (strcmp(id, "device") == 0) {
			if ((err = snd_config_get_integer(n, &device)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			continue;
		}
		if (strcmp(id, "subdevice") == 0) {
			if ((err = snd_config_get_integer(n, &subdevice)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			continue;
		}
		if (cchannelsp && strcmp(id, "count") == 0) {
			long v;
			if ((err = snd_config_get_integer(n, &v)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			if (v < 1 || v > 2) {
				SNDERR("Invalid count %ld", v);
				goto _err;
			}
			*cchannelsp = v;
			continue;
		}
		if (hwctlp && strcmp(id, "hwctl") == 0) {
			if ((err = snd_config_get_bool(n)) < 0) {
				SNDERR("The field %s must be a boolean type", id);
				return err;
			}
			*hwctlp = err;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (name == NULL) {
		SNDERR("Missing control name");
		err = -EINVAL;
		goto _err;
	}
	if (device < 0)
		device = 0;
	if (subdevice < 0)
		subdevice = 0;

	snd_ctl_elem_id_set_interface(ctl_id, iface);
	snd_ctl_elem_id_set_name(ctl_id, name);
	snd_ctl_elem_id_set_index(ctl_id, index);
	snd_ctl_elem_id_set_device(ctl_id, device);
	snd_ctl_elem_id_set_subdevice(ctl_id, subdevice);

	return 0;

 _err:
	return err;
}
