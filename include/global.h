/**
 * \file <alsa/global.h>
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@suse.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 1998-2001
 *
 * Application interface library for the ALSA driver
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

#ifndef __ALSA_GLOBAL_H_
#define __ALSA_GLOBAL_H_

/**
 *  \defgroup Global Global defines
 *  Global defines
 *  \{
 */

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__)) /**< don't print warning when attribute is not used */
#endif

/** helper macro for SND_DLSYM_BUILD_VERSION */
#define __SND_DLSYM_VERSION(name, version) _ ## name ## version
/** build version for versioned dynamic symbol */
#define SND_DLSYM_BUILD_VERSION(name, version) char __SND_DLSYM_VERSION(name, version)
/** get version of dynamic symbol as string */
#define SND_DLSYM_VERSION(version) __STRING(version)

int snd_dlsym_verify(void *handle, const char *name, const char *version);

/** Async notification client handler */
typedef struct _snd_async_handler snd_async_handler_t;

/** Async notification callback */
typedef void (*snd_async_callback_t)(snd_async_handler_t *handler);

int snd_async_add_handler(snd_async_handler_t **handler, int fd, 
			  snd_async_callback_t callback, void *private_data);
int snd_async_del_handler(snd_async_handler_t *handler);
int snd_async_handler_get_fd(snd_async_handler_t *handler);
void *snd_async_handler_get_callback_private(snd_async_handler_t *handler);

/** \} */

#endif /* __ALSA_GLOBAL_H */

