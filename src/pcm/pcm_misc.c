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
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "pcm_local.h"

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
	if (val >= 0)
		val ^= 1;
	return val;
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
