/**
 * \file <alsa/error.h>
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

#ifndef __ALSA_ERROR_H
#define __ALSA_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Error Error handling
 *  Error handling
 *  \{
 */

#define SND_ERROR_BEGIN				500000			/**< begin boundary of sound error codes */
#define SND_ERROR_INCOMPATIBLE_VERSION		(SND_ERROR_BEGIN+0)	/**< protocol is not compatible */

const char *snd_strerror(int errnum);

/**
 * \brief Error handler
 * \param file File name
 * \param line Line number
 * \param function Function name
 * \param err errno value (or 0 if not relevant)
 * \param fmt printf(3) format
 * \param ... printf(3) arguments
 */
typedef void (snd_lib_error_handler_t)(const char *file, int line, const char *function, int err, const char *fmt, ...) /* __attribute__ ((format (printf, 5, 6))) */;
extern snd_lib_error_handler_t *snd_lib_error;
extern int snd_lib_error_set_handler(snd_lib_error_handler_t *handler);

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 95)
#define SNDERR(...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, 0, __VA_ARGS__) /**< show sound error */
#define SYSERR(...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, errno, __VA_ARGS__) /**< show system error */
#else
#define SNDERR(args...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, 0, ##args) /**< show sound error */
#define SYSERR(args...) snd_lib_error(__FILE__, __LINE__, __FUNCTION__, errno, ##args) /**< show system error */
#endif

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_ERROR_H */

