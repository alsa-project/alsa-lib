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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __ALSA_GLOBAL_H_
#define __ALSA_GLOBAL_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Global Global defines and functions
 *  Global defines and functions.
 *  \{
 */

#ifndef ATTRIBUTE_UNUSED
/** do not print warning (gcc) when function parameter is not used */
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif

#ifdef PIC /* dynamic build */

/** helper macro for SND_DLSYM_BUILD_VERSION \hideinitializer */
#define __SND_DLSYM_VERSION(name, version) _ ## name ## version
/** build version for versioned dynamic symbol \hideinitializer */
#define SND_DLSYM_BUILD_VERSION(name, version) char __SND_DLSYM_VERSION(name, version);

#else /* static build */

struct snd_dlsym_link {
	struct snd_dlsym_link *next;
	const char *dlsym_name;
	const void *dlsym_ptr;
};

extern struct snd_dlsym_link *snd_dlsym_start;

/** helper macro for SND_DLSYM_BUILD_VERSION \hideinitializer */
#define __SND_DLSYM_VERSION(prefix, name, version) _ ## prefix ## name ## version
/** build version for versioned dynamic symbol \hideinitializer */
#define SND_DLSYM_BUILD_VERSION(name, version) \
  static struct snd_dlsym_link __SND_DLSYM_VERSION(snd_dlsym_, name, version); \
  void __SND_DLSYM_VERSION(snd_dlsym_constructor_, name, version) (void) __attribute__ ((constructor)); \
  void __SND_DLSYM_VERSION(snd_dlsym_constructor_, name, version) (void) { \
    __SND_DLSYM_VERSION(snd_dlsym_, name, version).next = snd_dlsym_start; \
    __SND_DLSYM_VERSION(snd_dlsym_, name, version).dlsym_name = # name; \
    __SND_DLSYM_VERSION(snd_dlsym_, name, version).dlsym_ptr = (void *)&name; \
    snd_dlsym_start = &__SND_DLSYM_VERSION(snd_dlsym_, name, version); \
  }

#endif

/** get version of dynamic symbol as string */
#define SND_DLSYM_VERSION(version) __STRING(version)

void *snd_dlopen(const char *file, int mode);
void *snd_dlsym(void *handle, const char *name, const char *version);
int snd_dlclose(void *handle);


/** Async notification client handler */
typedef struct _snd_async_handler snd_async_handler_t;

/** Async notification callback */
typedef void (*snd_async_callback_t)(snd_async_handler_t *handler);

int snd_async_add_handler(snd_async_handler_t **handler, int fd, 
			  snd_async_callback_t callback, void *private_data);
int snd_async_del_handler(snd_async_handler_t *handler);
int snd_async_handler_get_fd(snd_async_handler_t *handler);
int snd_async_handler_get_signo(snd_async_handler_t *handler);
void *snd_async_handler_get_callback_private(snd_async_handler_t *handler);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_GLOBAL_H */
