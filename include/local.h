/*
 *  ALSA lib - local header file
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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

#ifndef __LOCAL_H
#define __LOCAL_H

#define _snd_interval sndrv_interval
#define _snd_pcm_info sndrv_pcm_info
#define _snd_pcm_hw_params sndrv_pcm_hw_params
#define _snd_pcm_sw_params sndrv_pcm_sw_params
#define _snd_pcm_status sndrv_pcm_status

#define _snd_ctl_card_info sndrv_ctl_card_info
#define _snd_ctl_elem_id sndrv_ctl_elem_id
#define _snd_ctl_elem_list sndrv_ctl_elem_list
#define _snd_ctl_elem_info sndrv_ctl_elem_info
#define _snd_ctl_elem_value sndrv_ctl_elem_value
#define _snd_ctl_event sndrv_ctl_event

#define _snd_rawmidi_info sndrv_rawmidi_info
#define _snd_rawmidi_params sndrv_rawmidi_params
#define _snd_rawmidi_status sndrv_rawmidi_status

#define _snd_hwdep_info sndrv_hwdep_info

#include "asoundlib.h"
#include <errno.h>

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define ERR(...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, 0, __VA_ARGS__)
#define SYSERR(...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, errno, __VA_ARGS__)
#else
#define ERR(args...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, 0, ##args)
#define SYSERR(args...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, errno, ##args)
#endif

enum _snd_set_mode {
	SND_CHANGE,
	SND_TRY,
	SND_TEST,
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_set_mode *snd_set_mode_t;
#else
typedef enum _snd_set_mode snd_set_mode_t;
#endif

#define SND_CHANGE ((snd_set_mode_t) SND_CHANGE)
#define SND_TRY ((snd_set_mode_t) SND_TRY)
#define SND_TEST ((snd_set_mode_t) SND_TEST)

size_t page_align(size_t size);
size_t page_size(void);

#endif
