/**
 * \file include/error.h
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@perex.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 1998-2001
 *
 * Application interface library for the ALSA driver
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
#warning "use #include <alsa/asoundlib.h>, <alsa/error.h> should not be used directly"
#include <alsa/asoundlib.h>
#endif

#ifndef __ALSA_ERROR_H
#define __ALSA_ERROR_H /**< header include loop protection */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Error Error handling
 *  Error handling macros and functions.
 *  \{
 */

#define SND_ERROR_BEGIN				500000			/**< Lower boundary of sound error codes. */
#define SND_ERROR_INCOMPATIBLE_VERSION		(SND_ERROR_BEGIN+0)	/**< Kernel/library protocols are not compatible. */

const char *snd_strerror(int errnum);

#define SND_LOG_ERROR		1	/**< error priority level */
#define SND_LOG_WARN		2	/**< warning priority level */
#define SND_LOG_INFO		3	/**< info priority level */
#define SND_LOG_DEBUG		4	/**< debug priority level */
#define SND_LOG_TRACE		5	/**< trace priority level */
#define SND_LOG_LAST		SND_LOG_TRACE /**< last known value for priority level */

#define SND_ILOG_CORE           1	/**< core library code */
#define SND_ILOG_CONFIG		2	/**< configuration parsing and operations */
#define SND_ILOG_CONTROL	3	/**< control API */
#define SND_ILOG_HWDEP		4	/**< hwdep API */
#define SND_ILOG_TIMER		5	/**< timer API */
#define SND_ILOG_RAWMIDI	6	/**< RawMidi API */
#define SND_ILOG_PCM		7	/**< PCM API */
#define SND_ILOG_MIXER		8	/**< mixer API */
#define SND_ILOG_SEQUENCER	9	/**< sequencer API */
#define SND_ILOG_UCM		10	/**< UCM API */
#define SND_ILOG_TOPOLOGY	11	/**< topology API */
#define SND_ILOG_ASERVER	12	/**< aserver */
#define SND_ILOG_LAST		SND_ILOG_ASERVER /**< last known value for interface */

/**
 * \brief Log handler callback.
 * \param prio Priority (SND_LOG_* defines).
 * \param interface Interface (SND_ILOG_* defines).
 * \param file Source file name.
 * \param line Line number.
 * \param function Function name.
 * \param errcode Value of \c errno, or 0 if not relevant.
 * \param fmt \c printf(3) format.
 * \param ... \c printf(3) arguments.
 *
 * A function of this type is called by the ALSA library when an error occurs.
 * This function usually shows the message on the screen, and/or logs it.
 */
typedef void (*snd_lib_log_handler_t)(int prio, int interface, const char *file, int line, const char *function, int errcode, const char *fmt, va_list arg);
extern snd_lib_log_handler_t snd_lib_vlog;
int snd_lib_log_filter(int prio, int interface, const char *configstr);
void snd_lib_log(int prio, int interface, const char *file, int line, const char *function, int errcode, const char *fmt, ...) /* __attribute__ ((format (printf, 7, 8))) */;
void snd_lib_check(int interface, const char *file, int line, const char *function, int errcode, const char *fmt, ...);
snd_lib_log_handler_t snd_lib_log_set_handler(snd_lib_log_handler_t handler);
snd_lib_log_handler_t snd_lib_log_set_local(snd_lib_log_handler_t handler);
const char *snd_lib_log_priority(int prio);
const char *snd_lib_log_interface(int interface);

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 95)
#define snd_error(interface, ...) snd_lib_log(SND_LOG_ERROR, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, __VA_ARGS__) /**< Shows an error log message. */
#define snd_errornum(interface, ...) snd_lib_log(SND_LOG_ERROR, SND_ILOG_##interface, __FILE__, __LINE__, __func__, errno, __VA_ARGS__) /**< Shows a system error log message (related to \c errno). */
#define snd_warn(interface, ...) snd_lib_log(SND_LOG_WARN, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, __VA_ARGS__) /**< Shows an error log message. */
#define snd_info(interface, ...) snd_lib_log(SND_LOG_INFO, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, __VA_ARGS__) /**< Shows an informational log message. */
#define snd_debug(interface, ...) snd_lib_log(SND_LOG_DEBUG, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, __VA_ARGS__) /**< Shows an debug log message. */
#define snd_trace(interface, ...) snd_lib_log(SND_LOG_TRACE, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, __VA_ARGS__) /**< Shows an trace log message. */
#define snd_check(interface, ...) snd_lib_check(SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, __VA_ARGS__) /**< Shows an check log message. */
#define snd_checknum(interface, ...) snd_lib_check(SND_ILOG_##interface, __FILE__, __LINE__, __func__, errno, __VA_ARGS__) /**< Shows an check log message (related to \c errno). */
#else
#define snd_error(interface, args...) snd_lib_log(SND_LOG_ERROR, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, ##args) /**< Shows an error log message. */
#define snd_errornum(interface, args...) snd_lib_log(SND_LOG_ERROR, SND_ILOG_##interface, __FILE__, __LINE__, __func__, errno, ##args) /**< Shows a system error log message (related to \c errno). */
#define snd_warn(interface, args...) snd_lib_log(SND_LOG_WARN, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, ##args) /**< Shows an error log message. */
#define snd_info(interface, args...) snd_lib_log(SND_LOG_INFO, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, ##args) /**< Shows an error log message. */
#define snd_debug(interface, args...) snd_lib_log(SND_LOG_DEBUG, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, ##args) /**< Shows an error log message. */
#define snd_trace(interface, args...) snd_lib_log(SND_LOG_TRACE, SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, ##args) /**< Shows an trace log message. */
#define snd_check(interface, args...) snd_lib_check(SND_ILOG_##interface, __FILE__, __LINE__, __func__, 0, ##args) /**< Shows an check log message. */
#define snd_checknum(interface, args...) snd_lib_check(SND_ILOG_##interface, __FILE__, __LINE__, __func__, errno, ##args) /**< Shows an check log message (related to \c errno). */
#endif

/**
 * \brief Error handler callback.
 * \param file Source file name.
 * \param line Line number.
 * \param function Function name.
 * \param errcode Value of \c errno, or 0 if not relevant.
 * \param fmt \c printf(3) format.
 * \param ... \c printf(3) arguments.
 * \deprecated Since 1.2.15
 *
 * A function of this type is called by the ALSA library when an error occurs.
 * This function usually shows the message on the screen, and/or logs it.
 */
typedef void (*snd_lib_error_handler_t)(const char *file, int line, const char *function, int errcode, const char *fmt, ...) /* __attribute__ ((format (printf, 5, 6))) */;
extern snd_lib_error_handler_t snd_lib_error;
int snd_lib_error_set_handler(snd_lib_error_handler_t handler);

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 95)
#define SNDERR(...) snd_lib_log(SND_LOG_ERROR, 0, __FILE__, __LINE__, __func__, 0, __VA_ARGS__) /**< Shows a sound error message. */
#define SYSERR(...) snd_lib_log(SND_LOG_ERROR, 0, __FILE__, __LINE__, __func__, errno, __VA_ARGS__) /**< Shows a system error message (related to \c errno). */
#else
#define SNDERR(args...) snd_lib_log(SND_LOG_ERROR, 0, __FILE__, __LINE__, __func__, 0, ##args) /**< Shows a sound error message. */
#define SYSERR(args...) snd_lib_log(SND_LOG_ERROR, 0, __FILE__, __LINE__, __func__, errno, ##args) /**< Shows a system error message (related to \c errno). */
#endif

/** Local error handler function type */
typedef void (*snd_local_error_handler_t)(const char *file, int line,
					  const char *func, int errcode,
					  const char *fmt, va_list arg);
snd_local_error_handler_t snd_lib_error_set_local(snd_local_error_handler_t func);

/** \} */

#ifdef __cplusplus
}
#endif



#endif /* __ALSA_ERROR_H */
