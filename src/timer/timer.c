/*
 * \file timer/timer.c
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 1998-2001
 *
 * Timer Interface is designed to access timers.
 */
/*
 *  Timer Interface - main file
 *  Copyright (c) 1998-2001 by Jaroslav Kysela <perex@suse.cz>
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
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include "timer_local.h"

static int snd_timer_open_conf(snd_timer_t **timer,
			       const char *name, snd_config_t *timer_root,
			       snd_config_t *timer_conf, int mode)
{
	const char *str;
	char buf[256];
	int err;
	snd_config_t *conf, *type_conf;
	snd_config_iterator_t i, next;
	const char *lib = NULL, *open_name = NULL;
	int (*open_func)(snd_timer_t **, const char *, snd_config_t *, snd_config_t *, int) = NULL;
	void *h;
	if (snd_config_get_type(timer_conf) != SND_CONFIG_TYPE_COMPOUND) {
		if (name)
			SNDERR("Invalid type for TIMER %s definition", name);
		else
			SNDERR("Invalid type for TIMER definition");
		return -EINVAL;
	}
	err = snd_config_search(timer_conf, "type", &conf);
	if (err < 0) {
		SNDERR("type is not defined");
		return err;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", snd_config_get_id(conf));
		return err;
	}
	err = snd_config_search_definition(timer_root, "timer_type", str, &type_conf);
	if (err >= 0) {
		if (snd_config_get_type(type_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for TIMER type %s definition", str);
			goto _err;
		}
		snd_config_for_each(i, next, type_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			if (strcmp(id, "open") == 0) {
				err = snd_config_get_string(n, &open_name);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
			err = -EINVAL;
			goto _err;
		}
	}
	if (!open_name) {
		open_name = buf;
		snprintf(buf, sizeof(buf), "_snd_timer_%s_open", str);
	}
	if (!lib)
		lib = ALSA_LIB;
	h = dlopen(lib, RTLD_NOW);
	open_func = h ? dlsym(h, open_name) : NULL;
	if (!h) {
		SNDERR("Cannot open shared library %s", lib);
		err = -ENOENT;
	} else if (!open_func) {
		SNDERR("symbol %s is not defined inside %s", open_name, lib);
		dlclose(h);
		err = -ENXIO;
	}
       _err:
	if (type_conf)
		snd_config_delete(type_conf);
	if (err >= 0)
		err = open_func(timer, name, timer_root, timer_conf, mode);
	if (err < 0)
		return err;
	return 0;
}

static int snd_timer_open_noupdate(snd_timer_t **timer, snd_config_t *root, const char *name, int mode)
{
	int err;
	snd_config_t *timer_conf;
	err = snd_config_search_definition(root, "timer", name, &timer_conf);
	if (err < 0) {
		SNDERR("Unknown timer %s", name);
		return err;
	}
	err = snd_timer_open_conf(timer, name, root, timer_conf, mode);
	snd_config_delete(timer_conf);
	return err;
}

/**
 * \brief Opens a new connection to the timer interface.
 * \param timer Returned handle (NULL if not wanted)
 * \param name ASCII identifier of the timer handle
 * \param mode Open mode
 * \return 0 on success otherwise a negative error code
 *
 * Opens a new connection to the timer interface specified with
 * an ASCII identifier and mode.
 */
int snd_timer_open(snd_timer_t **timer, const char *name, int mode)
{
	int err;
	assert(timer && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	return snd_timer_open_noupdate(timer, snd_config, name, mode);
}

/**
 * \brief close timer handle
 * \param timer timer handle
 * \return 0 on success otherwise a negative error code
 *
 * Closes the specified timer handle and frees all associated
 * resources.
 */
int snd_timer_close(snd_timer_t *timer)
{
	int err;
  	assert(timer);
	if ((err = timer->ops->close(timer)) < 0)
		return err;
	if (timer->name)
		free(timer->name);
	free(timer);
	return 0;
}

/**
 * \brief get identifier of timer handle
 * \param timer a timer handle
 * \return ascii identifier of timer handle
 *
 * Returns the ASCII identifier of given timer handle. It's the same
 * identifier specified in snd_timer_open().
 */
const char *snd_timer_name(snd_timer_t *timer)
{
	assert(timer);
	return timer->name;
}

/**
 * \brief get type of timer handle
 * \param timer a timer handle
 * \return type of timer handle
 *
 * Returns the type #snd_timer_type_t of given timer handle.
 */
snd_timer_type_t snd_timer_type(snd_timer_t *timer)
{
	assert(timer);
	return timer->type;
}

/**
 * \brief get count of poll descriptors for timer handle
 * \param timer timer handle
 * \return count of poll descriptors
 */
int snd_timer_poll_descriptors_count(snd_timer_t *timer)
{
	assert(timer);
	return 1;
}

/**
 * \brief get poll descriptors
 * \param timer timer handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */
int snd_timer_poll_descriptors(snd_timer_t *timer, struct pollfd *pfds, unsigned int space)
{
	assert(timer);
	if (space >= 1) {
		pfds->fd = timer->poll_fd;
		switch (timer->mode & O_ACCMODE) {
		case O_WRONLY:
			pfds->events = POLLOUT;
			break;
		case O_RDONLY:
			pfds->events = POLLIN;
			break;
		case O_RDWR:
			pfds->events = POLLOUT|POLLIN;
			break;
		default:
			return -EIO;
		}
		return 1;
	}
	return 0;
}

/**
 * \brief set nonblock mode
 * \param timer timer handle
 * \param nonblock 0 = block, 1 = nonblock mode
 * \return 0 on success otherwise a negative error code
 */
int snd_timer_nonblock(snd_timer_t *timer, int nonblock)
{
	int err;
	assert(timer);
	if ((err = timer->ops->nonblock(timer, nonblock)) < 0)
		return err;
	if (nonblock)
		timer->mode |= SND_TIMER_OPEN_NONBLOCK;
	else
		timer->mode &= ~SND_TIMER_OPEN_NONBLOCK;
	return 0;
}

/**
 * \brief get information about timer handle
 * \param timer timer handle
 * \param info pointer to a snd_timer_info_t structure to be filled
 * \return 0 on success otherwise a negative error code
 */
int snd_timer_info(snd_timer_t *timer, snd_timer_info_t * info)
{
	assert(timer);
	assert(info);
	return timer->ops->info(timer, info);
}

/**
 * \brief set parameters for timer handle
 * \param timer timer handle
 * \param params pointer to a #snd_timer_params_t structure
 * \return 0 on success otherwise a negative error code
 */
int snd_timer_params(snd_timer_t *timer, snd_timer_params_t * params)
{
	assert(timer);
	assert(params);
	return timer->ops->params(timer, params);
}

/**
 * \brief get status from timer handle
 * \param timer timer handle
 * \param status pointer to a #snd_timer_status_t structure to be filled
 * \return 0 on success otherwise a negative error code
 */
int snd_timer_status(snd_timer_t *timer, snd_timer_status_t * status)
{
	assert(timer);
	assert(status);
	return timer->ops->status(timer, status);
}

/**
 * \brief start the timer
 * \param timer timer handle
 * \return 0 on success otherwise a negative error code
 */
int snd_timer_start(snd_timer_t *timer)
{
	assert(timer);
	return timer->ops->rt_start(timer);
}

/**
 * \brief stop the timer
 * \param timer timer handle
 * \return 0 on success otherwise a negative error code
 */
int snd_timer_stop(snd_timer_t *timer)
{
	assert(timer);
	return timer->ops->rt_stop(timer);
}

/**
 * \brief continue the timer
 * \param timer timer handle
 * \return 0 on success otherwise a negative error code
 */
int snd_timer_continue(snd_timer_t *timer)
{
	assert(timer);
	return timer->ops->rt_continue(timer);
}

/**
 * \brief read bytes using timer handle
 * \param timer timer handle
 * \param buffer buffer to store the input bytes
 * \param size input buffer size in bytes
 */
ssize_t snd_timer_read(snd_timer_t *timer, void *buffer, size_t size)
{
	assert(timer);
	assert(((timer->mode & O_ACCMODE) == O_RDONLY) || ((timer->mode & O_ACCMODE) == O_RDWR));
	assert(buffer || size == 0);
	return timer->ops->read(timer, buffer, size);
}
