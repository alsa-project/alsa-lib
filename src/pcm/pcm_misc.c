/*
 *  PCM Interface - misc routines
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
  
#ifdef __KERNEL__
#include "../include/driver.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include "pcm_local.h"
#endif

int snd_pcm_format_signed(int format)
{
	switch (format) {
	case SND_PCM_SFMT_S8:
	case SND_PCM_SFMT_S16_LE:
	case SND_PCM_SFMT_S16_BE:
	case SND_PCM_SFMT_S24_LE:
	case SND_PCM_SFMT_S24_BE:
	case SND_PCM_SFMT_S32_LE:
	case SND_PCM_SFMT_S32_BE:
		return 1;
	case SND_PCM_SFMT_U8:
	case SND_PCM_SFMT_U16_LE:
	case SND_PCM_SFMT_U16_BE:
	case SND_PCM_SFMT_U24_LE:
	case SND_PCM_SFMT_U24_BE:
	case SND_PCM_SFMT_U32_LE:
	case SND_PCM_SFMT_U32_BE:
		return 0;
	default:
		return -EINVAL;
	}
}

int snd_pcm_format_unsigned(int format)
{
	int val;

	val = snd_pcm_format_signed(format);
	if (val >= 0)
		val ^= 1;
	return val;
}

int snd_pcm_format_linear(int format)
{
	return snd_pcm_format_signed(format) >= 0;
}

int snd_pcm_format_little_endian(int format)
{
	switch (format) {
	case SND_PCM_SFMT_S16_LE:
	case SND_PCM_SFMT_U16_LE:
	case SND_PCM_SFMT_S24_LE:
	case SND_PCM_SFMT_U24_LE:
	case SND_PCM_SFMT_S32_LE:
	case SND_PCM_SFMT_U32_LE:
	case SND_PCM_SFMT_FLOAT_LE:
	case SND_PCM_SFMT_FLOAT64_LE:
	case SND_PCM_SFMT_IEC958_SUBFRAME_LE:
		return 1;
	case SND_PCM_SFMT_S16_BE:
	case SND_PCM_SFMT_U16_BE:
	case SND_PCM_SFMT_S24_BE:
	case SND_PCM_SFMT_U24_BE:
	case SND_PCM_SFMT_S32_BE:
	case SND_PCM_SFMT_U32_BE:
	case SND_PCM_SFMT_FLOAT_BE:
	case SND_PCM_SFMT_FLOAT64_BE:
	case SND_PCM_SFMT_IEC958_SUBFRAME_BE:
		return 0;
	default:
		return -EINVAL;
	}
}

int snd_pcm_format_big_endian(int format)
{
	int val;

	val = snd_pcm_format_little_endian(format);
	if (val < 0)
		return val;
	return !val;
}

int snd_pcm_format_width(int format)
{
	switch (format) {
	case SND_PCM_SFMT_S8:
	case SND_PCM_SFMT_U8:
		return 8;
	case SND_PCM_SFMT_S16_LE:
	case SND_PCM_SFMT_S16_BE:
	case SND_PCM_SFMT_U16_LE:
	case SND_PCM_SFMT_U16_BE:
		return 16;
	case SND_PCM_SFMT_S24_LE:
	case SND_PCM_SFMT_S24_BE:
	case SND_PCM_SFMT_U24_LE:
	case SND_PCM_SFMT_U24_BE:
		return 24;
	case SND_PCM_SFMT_S32_LE:
	case SND_PCM_SFMT_S32_BE:
	case SND_PCM_SFMT_U32_LE:
	case SND_PCM_SFMT_U32_BE:
	case SND_PCM_SFMT_FLOAT_LE:
	case SND_PCM_SFMT_FLOAT_BE:
		return 32;
	case SND_PCM_SFMT_FLOAT64_LE:
	case SND_PCM_SFMT_FLOAT64_BE:
		return 64;
	case SND_PCM_SFMT_IEC958_SUBFRAME_LE:
	case SND_PCM_SFMT_IEC958_SUBFRAME_BE:
		return 24;
	case SND_PCM_SFMT_MU_LAW:
	case SND_PCM_SFMT_A_LAW:
		return 8;
	case SND_PCM_SFMT_IMA_ADPCM:
		return 4;
	default:
		return -EINVAL;
	}
}

ssize_t snd_pcm_format_size(int format, size_t samples)
{
	if (samples < 0)
		return -EINVAL;
	if (samples == 0)
		return samples;
	switch (format) {
	case SND_PCM_SFMT_S8:
	case SND_PCM_SFMT_U8:
		return samples;
	case SND_PCM_SFMT_S16_LE:
	case SND_PCM_SFMT_S16_BE:
	case SND_PCM_SFMT_U16_LE:
	case SND_PCM_SFMT_U16_BE:
		return samples * 2;
	case SND_PCM_SFMT_S24_LE:
	case SND_PCM_SFMT_S24_BE:
	case SND_PCM_SFMT_U24_LE:
	case SND_PCM_SFMT_U24_BE:
	case SND_PCM_SFMT_S32_LE:
	case SND_PCM_SFMT_S32_BE:
	case SND_PCM_SFMT_U32_LE:
	case SND_PCM_SFMT_U32_BE:
	case SND_PCM_SFMT_FLOAT_LE:
	case SND_PCM_SFMT_FLOAT_BE:
		return samples * 4;
	case SND_PCM_SFMT_FLOAT64_LE:
	case SND_PCM_SFMT_FLOAT64_BE:
		return samples * 8;
	case SND_PCM_SFMT_IEC958_SUBFRAME_LE:
	case SND_PCM_SFMT_IEC958_SUBFRAME_BE:
		return samples * 4;
	case SND_PCM_SFMT_MU_LAW:
	case SND_PCM_SFMT_A_LAW:
		return samples;
	case SND_PCM_SFMT_IMA_ADPCM:
		if (samples & 1)
			return -EINVAL;
		return samples / 2;
	default:
		return -EINVAL;
	}
}

const char *snd_pcm_get_format_name(int format)
{
	static char *formats[] = {
		"Signed 8-bit",
		"Unsigned 8-bit",
		"Signed 16-bit Little Endian",
		"Signed 16-bit Big Endian",
		"Unsigned 16-bit Little Endian",
		"Unsigned 16-bit Big Endian",
		"Signed 24-bit Little Endian",
		"Signed 24-bit Big Endian",
		"Unsigned 24-bit Little Endian",
		"Unsigned 24-bit Big Endian",
		"Signed 32-bit Little Endian",
		"Signed 32-bit Big Endian",
		"Unsigned 32-bit Little Endian",
		"Unsigned 32-bit Big Endian",
		"Float Little Endian",
		"Float Big Endian",
		"Float64 Little Endian",
		"Float64 Big Endian",
		"IEC-958 Little Endian",
		"IEC-958 Big Endian",
		"Mu-Law",
		"A-Law",
		"Ima-ADPCM",
		"MPEG",
		"GSM"
	};
	if (format == SND_PCM_SFMT_SPECIAL)
		return "Special";
	if (format < 0 || format > SND_PCM_SFMT_GSM)
		return "Unknown";
	return formats[format];
}

unsigned char snd_pcm_format_silence(int format)
{
	switch (format) {
	case SND_PCM_SFMT_IMA_ADPCM:	/* special case */
	case SND_PCM_SFMT_MPEG:
	case SND_PCM_SFMT_GSM:
	case SND_PCM_SFMT_MU_LAW:
	case SND_PCM_SFMT_A_LAW:
		return 0;
	case SND_PCM_SFMT_U8:
	case SND_PCM_SFMT_U16_LE:
	case SND_PCM_SFMT_U16_BE:
	case SND_PCM_SFMT_U24_LE:
	case SND_PCM_SFMT_U24_BE:
	case SND_PCM_SFMT_U32_LE:
	case SND_PCM_SFMT_U32_BE:
		return 0x80;
	case SND_PCM_SFMT_S8:
	case SND_PCM_SFMT_S16_LE:
	case SND_PCM_SFMT_S16_BE:
	case SND_PCM_SFMT_S24_LE:
	case SND_PCM_SFMT_S24_BE:
	case SND_PCM_SFMT_S32_LE:
	case SND_PCM_SFMT_S32_BE:
		return 0;
	case SND_PCM_SFMT_FLOAT:
	case SND_PCM_SFMT_FLOAT64:
	case SND_PCM_SFMT_IEC958_SUBFRAME_LE:
	case SND_PCM_SFMT_IEC958_SUBFRAME_BE:
		return 0;	
	}
	return 0;
}

static int linear_formats[4*2*2] = {
	SND_PCM_SFMT_S8,
	SND_PCM_SFMT_U8,
	SND_PCM_SFMT_S8,
	SND_PCM_SFMT_U8,
	SND_PCM_SFMT_S16_LE,
	SND_PCM_SFMT_S16_BE,
	SND_PCM_SFMT_U16_LE,
	SND_PCM_SFMT_U16_BE,
	SND_PCM_SFMT_S24_LE,
	SND_PCM_SFMT_S24_BE,
	SND_PCM_SFMT_U24_LE,
	SND_PCM_SFMT_U24_BE,
	SND_PCM_SFMT_S32_LE,
	SND_PCM_SFMT_S32_BE,
	SND_PCM_SFMT_U32_LE,
	SND_PCM_SFMT_U32_BE
};

int snd_pcm_build_linear_format(int width, int unsignd, int big_endian)
{
	switch (width) {
	case 8:
		width = 0;
		break;
	case 16:
		width = 1;
		break;
	case 24:
		width = 2;
		break;
	case 32:
		width = 3;
		break;
	default:
		return -1;
	}
	return ((int(*)[2][2])linear_formats)[width][!!unsignd][!!big_endian];
}
