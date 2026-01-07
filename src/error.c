/**
 * \file error.c
 * \brief Error code handling routines
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 1998-2001
 *
 * Error code handling routines.
 */
/*
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@perex.cz>
 *
 *  snd_strerror routine needs to be recoded for the locale support
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

#include "local.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static void snd_lib_error_default(const char *file, int line, const char *function, int errcode, const char *fmt, ...);

/**
 * Array of error codes in US ASCII.
 */
static const char *snd_error_codes[] =
{
	"Sound protocol is not compatible"
};

/**
 * \brief Returns the message for an error code.
 * \param errnum The error code number, which must be a system error code
 *               or an ALSA error code.
 * \return The ASCII description of the given numeric error code.
 */
const char *snd_strerror(int errnum)
{
	if (errnum < 0)
		errnum = -errnum;
	if (errnum < SND_ERROR_BEGIN)
		return (const char *) strerror(errnum);
	errnum -= SND_ERROR_BEGIN;
	if ((unsigned int) errnum >= sizeof(snd_error_codes) / sizeof(const char *))
		 return "Unknown error";
	return snd_error_codes[errnum];
}

#ifndef DOC_HIDDEN
#ifdef HAVE___THREAD
#define TLS_PFX		__thread
#else
#define TLS_PFX		/* NOP */
#endif
#endif

static TLS_PFX snd_lib_log_handler_t local_log = NULL;
static TLS_PFX snd_local_error_handler_t local_error = NULL;

/**
 * \brief Install local log handler
 * \param func The local log handler function
 * \retval Previous local log handler function
 */
snd_lib_log_handler_t snd_lib_log_set_local(snd_lib_log_handler_t func)
{
	snd_lib_log_handler_t old = local_log;
	local_log = func;
	return old;
}

/**
 * Array of log priority level names.
 */
static const char *snd_log_prio_names[SND_LOG_LAST + 1] = {
	[0] = NULL,
	[SND_LOG_ERROR] = "error",
	[SND_LOG_WARN] = "warning",
	[SND_LOG_INFO] = "info",
	[SND_LOG_DEBUG] = "debug",
	[SND_LOG_TRACE] = "trace",
};

/**
 * Array of interface names.
 */
static const char *snd_ilog_interface_names[SND_ILOG_LAST + 1] = {
	[0] = NULL,
	[SND_ILOG_CORE] = "core",
	[SND_ILOG_CONFIG] = "config",
	[SND_ILOG_CONTROL] = "control",
	[SND_ILOG_HWDEP] = "hwdep",
	[SND_ILOG_TIMER] = "timer",
	[SND_ILOG_RAWMIDI] = "rawmidi",
	[SND_ILOG_PCM] = "pcm",
	[SND_ILOG_MIXER] = "mixer",
	[SND_ILOG_SEQUENCER] = "sequencer",
	[SND_ILOG_UCM] = "ucm",
	[SND_ILOG_TOPOLOGY] = "topology",
	[SND_ILOG_ASERVER] = "aserver",
};

/**
 * \brief Function to convert log priority level to text.
 * \param prio Priority value (SND_LOG_*).
 * \return The textual representation of the priority level, or NULL if invalid.
 */
const char *snd_lib_log_priority(int prio)
{
	if (prio >= 0 && prio <= SND_LOG_TRACE)
		return snd_log_prio_names[prio];
	return NULL;
}

/**
 * \brief Function to convert interface code to text.
 * \param interface Interface (SND_ILOG_*).
 * \return The textual representation of the interface code, or NULL if invalid.
 */
const char *snd_lib_log_interface(int interface)
{
	if (interface >= 0 && interface <= SND_ILOG_TOPOLOGY)
		return snd_ilog_interface_names[interface];
	return NULL;
}

/**
 * \brief Structure to hold parsed debug configuration.
 */
static struct {
	const char *configstr;
	int global_level;
	int interface_levels[SND_ILOG_LAST + 1];
	int parsed;
} debug_config;

/**
 * \brief Parse the LIBASOUND_DEBUG environment variable.
 *
 * Format: [<level>][,<interface1>:<level1>][,<interface2>:<level2>,...]
 *
 * Examples:
 *   "debug"                 - Set global level to debug
 *   "3"                     - Set global level to 3 (info)
 *   "info,pcm:debug"        - Set global to info, pcm to debug
 *   "error,mixer:5,pcm:4"   - Set global to error, mixer to 5 (trace), pcm to 4 (debug)
 */
static void parse_libasound_debug(const char *configstr)
{
	const char *env;
	char *str, *token, *saveptr;
	int i;

	if (debug_config.parsed && debug_config.configstr == configstr)
		return;

	debug_config.parsed = 1;
	debug_config.global_level = 0;
	debug_config.configstr = configstr;
	for (i = 0; i <= SND_ILOG_LAST; i++)
		debug_config.interface_levels[i] = 0;

	if (configstr == NULL) {
		env = getenv("LIBASOUND_DEBUG");
		if (!env || !*env)
			return;
	} else {
		env = configstr;
	}

	str = strdup(env);
	if (!str)
		return;

	token = strtok_r(str, ",", &saveptr);
	while (token) {
		char *colon = strchr(token, ':');
		if (colon) {
			/* interface:level format */
			*colon = '\0';
			const char *interface_name = token;
			const char *level_str = colon + 1;
			int interface_num = -1;
			int level = -1;

			/* Try to find interface by name */
			for (i = 1; i <= SND_ILOG_LAST; i++) {
				if (snd_ilog_interface_names[i] &&
				    strcmp(snd_ilog_interface_names[i], interface_name) == 0) {
					interface_num = i;
					break;
				}
			}

			/* If not found by name, try direct number */
			if (interface_num < 0) {
				char *endptr;
				long val = strtol(interface_name, &endptr, 10);
				if (*endptr == '\0' && val >= 0 && val <= SND_ILOG_LAST)
					interface_num = val;
			}

			/* Parse level */
			for (i = 1; i <= SND_LOG_LAST; i++) {
				if (snd_log_prio_names[i] &&
				    strcmp(snd_log_prio_names[i], level_str) == 0) {
					level = i;
					break;
				}
			}

			/* If not found by name, try direct number */
			if (level < 0) {
				char *endptr;
				long val = strtol(level_str, &endptr, 10);
				if (*endptr == '\0' && val >= 0 && val <= SND_LOG_LAST)
					level = val;
			}

			/* Store the interface-specific level */
			if (interface_num > 0 && level > 0)
				debug_config.interface_levels[interface_num] = level;
		} else {
			/* Global level only */
			int level = -1;

			/* Try to find level by name */
			for (i = 1; i <= SND_LOG_LAST; i++) {
				if (snd_log_prio_names[i] &&
				    strcmp(snd_log_prio_names[i], token) == 0) {
					level = i;
					break;
				}
			}

			/* If not found by name, try direct number */
			if (level < 0) {
				char *endptr;
				long val = strtol(token, &endptr, 10);
				if (*endptr == '\0' && val >= 0 && val <= SND_LOG_LAST)
					level = val;
			}

			if (level > 0)
				debug_config.global_level = level;
		}

		token = strtok_r(NULL, ",", &saveptr);
	}

	free(str);
}

/**
 * \brief Check if a log message should be shown based on LIBASOUND_DEBUG.
 * \param prio Priority value (SND_LOG_*).
 * \param interface Interface (SND_ILOG_*).
 * \param configstr Configuration string (usually LIBASOUND_DEBUG environment variable)
 * \return 1 if the message should be shown, 0 otherwise.
 */
int snd_lib_log_filter(int prio, int interface, const char *configstr)
{
	unsigned int level;

	parse_libasound_debug(configstr);

	if (interface > 0 && interface <= SND_ILOG_LAST && debug_config.interface_levels[interface] > 0) {
		level = debug_config.interface_levels[interface];
	} else {
		level = debug_config.global_level;
	}

	if (level == 0)
		level = SND_LOG_ERROR;

	/* Show message if its priority is less than or equal to the configured level */
	return prio <= (int)level;
}

/**
 * \brief The default log handler function.
 * \param prio Priority value (SND_LOG_*).
 * \param interface Interface (SND_ILOG_*).
 * \param file The filename where the error was hit.
 * \param line The line number.
 * \param function The function name.
 * \param errcode The error code.
 * \param fmt The message (including the format characters).
 * \param ... Optional arguments.
 *
 * If a local error function has been installed for the current thread by
 * \ref snd_lib_log_set_local, it is called. Otherwise, prints the error
 * message including location to \c stderr.
 */
static void snd_lib_vlog_default(int prio, int interface, const char *file, int line, const char *function, int errcode, const char *fmt, va_list arg)
{
	const char *text1, *text2;

	if (local_log) {
		local_log(prio, interface, file, line, function, errcode, fmt, arg);
		return;
	}
	if (local_error && prio == SND_LOG_ERROR) {
		local_error(file, line, function, errcode, fmt, arg);
		return;
	}
	if (snd_lib_error != snd_lib_error_default) {
		if (prio == SND_LOG_ERROR)
			snd_lib_error(file, line, function, errcode, fmt, arg);
		/* ignore other priorities - restore old behaviour */
		return;
	}

	if (!snd_lib_log_filter(prio, interface, NULL))
		return;

	fprintf(stderr, "ALSA lib %s:%i:(%s) ", file, line, function);

	text1 = snd_lib_log_priority(prio);
	text2 = snd_lib_log_interface(interface);
	if (text1 || text2)
		fprintf(stderr, "[%s.%s] ", text1 ? text1 : "", text2 ? text2 : "");

	vfprintf(stderr, fmt, arg);
	if (errcode)
		fprintf(stderr, ": %s", snd_strerror(errcode));
	putc('\n', stderr);
}

/**
 * \brief Root log handler function.
 * \param prio Priority value (SND_LOG_*).
 * \param interface Interface (SND_ILOG_*).
 * \param file The filename where the error was hit.
 * \param line The line number.
 * \param function The function name.
 * \param errcode The error code.
 * \param fmt The message (including the format characters).
 * \param ... Optional arguments.
 */
void snd_lib_log(int prio, int interface, const char *file, int line, const char *function, int errcode, const char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	snd_lib_vlog(prio, interface, file, line, function, errcode, fmt, arg);
	va_end(arg);
}

/**
 * \brief The check point function.
 * \param interface Interface (SND_ILOG_*).
 * \param file The filename where the error was hit.
 * \param line The line number.
 * \param function The function name.
 * \param errcode The error code.
 * \param fmt The message (including the format characters).
 * \param ... Optional arguments.
 *
 * The error message is passed with error priority level to snd_lib_vlog handler.
 */
void snd_lib_check(int interface, const char *file, int line, const char *function, int errcode, const char *fmt, ...)
{
	const char *verbose;
	va_list arg;

	va_start(arg, fmt);
	verbose = getenv("LIBASOUND_DEBUG");
	if (! verbose || ! *verbose)
		goto finish;
	snd_lib_vlog(SND_LOG_ERROR, interface, file, line, function, errcode, fmt, arg);
#ifdef ALSA_DEBUG_ASSERT
	verbose = getenv("LIBASOUND_DEBUG_ASSERT");
	if (verbose && *verbose)
		assert(0);
#endif
finish:
	va_end(arg);
}

/**
 * \ingroup Error
 * Pointer to the error handler function.
 * For internal use only.
 */
snd_lib_log_handler_t snd_lib_vlog = snd_lib_vlog_default;

/**
 * \brief Sets the log handler.
 * \param handler The pointer to the new log handler function.
 * \retval Previous log handler function
 *
 * This function sets a new log handler, or (if \c handler is \c NULL)
 * the default one which prints the error messages to \c stderr.
 */
snd_lib_log_handler_t snd_lib_log_set_handler(snd_lib_log_handler_t handler)
{
	snd_lib_log_handler_t old = snd_lib_vlog;
	snd_lib_vlog = handler == NULL ? snd_lib_vlog_default : handler;
	return old;
}


/**
 * \brief Install local error handler
 * \param func The local error handler function
 * \retval Previous local error handler function
 * \deprecated Since 1.2.15
 */
snd_local_error_handler_t snd_lib_error_set_local(snd_local_error_handler_t func)
{
	snd_local_error_handler_t old = local_error;
	local_error = func;
	return old;
}
#ifndef DOC_HIDDEN
link_warning(snd_lib_error_set_local, "Warning: snd_lib_error_set_local is deprecated, use snd_lib_log_set_local");
#endif

/**
 * \brief The default error handler function.
 * \param file The filename where the error was hit.
 * \param line The line number.
 * \param function The function name.
 * \param errcode The error code.
 * \param fmt The message (including the format characters).
 * \param ... Optional arguments.
 * \deprecated Since 1.2.15
 *
 * Use snd_lib_vlog handler to print error message for anonymous interface.
 */
static void snd_lib_error_default(const char *file, int line, const char *function, int errcode, const char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	snd_lib_vlog(SND_LOG_ERROR, 0, file, line, function, errcode, fmt, arg);
	va_end(arg);
}

/**
 * \ingroup Error
 * \deprecated Since 1.2.15
 * Pointer to the error handler function.
 * For internal use only.
 */
snd_lib_error_handler_t snd_lib_error = snd_lib_error_default;
#ifndef DOC_HIDDEN
link_warning(snd_lib_error, "Warning: snd_lib_error is deprecated, use snd_log interface");
#endif

/**
 * \brief Sets the error handler.
 * \param handler The pointer to the new error handler function.
 * \deprecated Since 1.2.15
 *
 * This function sets a new error handler, or (if \c handler is \c NULL)
 * the default one which prints the error messages to \c stderr.
 */
int snd_lib_error_set_handler(snd_lib_error_handler_t handler)
{
	snd_lib_error = handler == NULL ? snd_lib_error_default : handler;
	return 0;
}

/**
 * \brief Returns the ALSA sound library version in ASCII format
 * \return The ASCII description of the used ALSA sound library.
 */
const char *snd_asoundlib_version(void)
{
	return SND_LIB_VERSION_STR;
}

/**
 * \brief Copy a C-string into a sized buffer
 * \param dst Where to copy the string to
 * \param src Where to copy the string from
 * \param size Size of destination buffer
 * \retval The source string length
 *
 * The result is always a valid NUL-terminated string that fits
 * in the buffer (unless, of course, the buffer size is zero).
 * It does not pad out the result like strncpy() does.
 */
size_t snd_strlcpy(char *dst, const char *src, size_t size)
{
	size_t ret = strlen(src);
	if (size) {
		size_t len = ret >= size ? size - 1 : ret;
		memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return ret;
}

/**
 * \brief Append a C-string into a sized buffer
 * \param dst Where to append the string to
 * \param src Where to copy the string from
 * \param size Size of destination buffer
 * \retval The total string length (no trimming)
 *
 * The result is always a valid NUL-terminated string that fits
 * in the buffer (unless, of course, the buffer size is zero).
 * It does not pad out the result.
 */
size_t snd_strlcat(char *dst, const char *src, size_t size)
{
	size_t dst_len = strlen(dst);
	size_t len = strlen(src);
	size_t ret = dst_len + len;
	if (dst_len < size) {
		dst += dst_len;
		size -= dst_len;
		if (len >= size)
			len = size - 1;
		memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return ret;
}
