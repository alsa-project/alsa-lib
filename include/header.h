/*
 *  Application interface library for the ALSA driver
 *  Copyright (c) by Jaroslav Kysela <perex@suse.cz>
 *                   Abramo Bagnara <abramo@alsa-project.org>
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

#ifndef __ASOUNDLIB_H
#define __ASOUNDLIB_H

#include <linux/asound.h>
#include <linux/asoundef.h>
#include <linux/asequencer.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/uio.h>

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif

#ifdef SNDRV_LITTLE_ENDIAN
#define SND_LITTLE_ENDIAN SNDRV_LITTLE_ENDIAN
#endif

#ifdef SNDRV_BIG_ENDIAN
#define SND_BIG_ENDIAN SNDRV_BIG_ENDIAN
#endif


enum _snd_set_mode {
	SND_CHANGE,
	SND_TRY,
	SND_TEST,
};

//#define SND_ENUM_TYPECHECK

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_set_mode *snd_set_mode_t;
#define snd_enum_to_int(v) ((unsigned int)(unsigned long)(v))
#define snd_int_to_enum(v) ((void *)(unsigned long)(v))
#define snd_enum_incr(v) (++(unsigned long)(v))
#else
typedef enum _snd_set_mode snd_set_mode_t;
#define snd_enum_to_int(v) (v)
#define snd_int_to_enum(v) (v)
#define snd_enum_incr(v) (++(v))
#endif

#define SND_CHANGE ((snd_set_mode_t) SND_CHANGE)
#define SND_TRY ((snd_set_mode_t) SND_TRY)
#define SND_TEST ((snd_set_mode_t) SND_TEST)
