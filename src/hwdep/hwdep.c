/*
 * \file hwdep/hwdep.c
 * \brief HwDep Interface (hardware dependent)
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2000-2001
 *
 * HwDep (hardware dependent) Interface is designed for individual hardware
 * access. This interface does not cover any API specification.
 */
/*
 *  Hardware dependent Interface - main file
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include "hwdep_local.h"

static int snd_hwdep_open_conf(snd_hwdep_t **hwdep,
			       const char *name, snd_config_t *hwdep_root,
			       snd_config_t *hwdep_conf, int mode)
{
	const char *str;
	char buf[256];
	int err;
	snd_config_t *conf, *type_conf = NULL;
	snd_config_iterator_t i, next;
	const char *id;
	const char *lib = NULL, *open_name = NULL;
	int (*open_func)(snd_hwdep_t **, const char *, snd_config_t *, snd_config_t *, int) = NULL;
#ifndef PIC
	extern void *snd_hwdep_open_symbols(void);
#endif
	void *h;
	if (snd_config_get_type(hwdep_conf) != SND_CONFIG_TYPE_COMPOUND) {
		if (name)
			SNDERR("Invalid type for HWDEP %s definition", name);
		else
			SNDERR("Invalid type for HWDEP definition");
		return -EINVAL;
	}
	err = snd_config_search(hwdep_conf, "type", &conf);
	if (err < 0) {
		SNDERR("type is not defined");
		return err;
	}
	err = snd_config_get_id(conf, &id);
	if (err < 0) {
		SNDERR("unable to get id");
		return err;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", id);
		return err;
	}
	err = snd_config_search_definition(hwdep_root, "hwdep_type", str, &type_conf);
	if (err >= 0) {
		if (snd_config_get_type(type_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for HWDEP type %s definition", str);
			goto _err;
		}
		snd_config_for_each(i, next, type_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			if (snd_config_get_id(n, &id) < 0)
				continue;
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
		snprintf(buf, sizeof(buf), "_snd_hwdep_%s_open", str);
	}
#ifndef PIC
	snd_hwdep_open_symbols();
#endif
	h = snd_dlopen(lib, RTLD_NOW);
	if (h)
		open_func = snd_dlsym(h, open_name, SND_DLSYM_VERSION(SND_HWDEP_DLSYM_VERSION));
	err = 0;
	if (!h) {
		SNDERR("Cannot open shared library %s", lib);
		err = -ENOENT;
	} else if (!open_func) {
		SNDERR("symbol %s is not defined inside %s", open_name, lib);
		snd_dlclose(h);
		err = -ENXIO;
	}
       _err:
	if (type_conf)
		snd_config_delete(type_conf);
	return err >= 0 ? open_func(hwdep, name, hwdep_root, hwdep_conf, mode) : err;
}

static int snd_hwdep_open_noupdate(snd_hwdep_t **hwdep, snd_config_t *root, const char *name, int mode)
{
	int err;
	snd_config_t *hwdep_conf;
	err = snd_config_search_definition(root, "hwdep", name, &hwdep_conf);
	if (err < 0) {
		SNDERR("Unknown HwDep %s", name);
		return err;
	}
	err = snd_hwdep_open_conf(hwdep, name, root, hwdep_conf, mode);
	snd_config_delete(hwdep_conf);
	return err;
}

/**
 * \brief Opens a new connection to the HwDep interface.
 * \param hwdep Returned handle (NULL if not wanted)
 * \param name ASCII identifier of the HwDep handle
 * \param mode Open mode
 * \return 0 on success otherwise a negative error code
 *
 * Opens a new connection to the HwDep interface specified with
 * an ASCII identifier and mode.
 */
int snd_hwdep_open(snd_hwdep_t **hwdep, const char *name, int mode)
{
	int err;
	assert(hwdep && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	return snd_hwdep_open_noupdate(hwdep, snd_config, name, mode);
}

/**
 * \brief Opens a new connection to the HwDep interface using local configuration
 * \param hwdep Returned handle (NULL if not wanted)
 * \param name ASCII identifier of the HwDep handle
 * \param mode Open mode
 * \return 0 on success otherwise a negative error code
 *
 * Opens a new connection to the HwDep interface specified with
 * an ASCII identifier and mode.
 */
int snd_hwdep_open_lconf(snd_hwdep_t **hwdep, const char *name,
			 int mode, snd_config_t *lconf)
{
	assert(hwdep && name && lconf);
	return snd_hwdep_open_noupdate(hwdep, lconf, name, mode);
}

/**
 * \brief close HwDep handle
 * \param hwdep HwDep handle
 * \return 0 on success otherwise a negative error code
 *
 * Closes the specified HwDep handle and frees all associated
 * resources.
 */
int snd_hwdep_close(snd_hwdep_t *hwdep)
{
	int err;
  	assert(hwdep);
	if ((err = hwdep->ops->close(hwdep)) < 0)
		return err;
	if (hwdep->name)
		free(hwdep->name);
	free(hwdep);
	return 0;
}

/**
 * \brief get identifier of HwDep handle
 * \param hwdep a Hwdep handle
 * \return ascii identifier of HwDep handle
 *
 * Returns the ASCII identifier of given HwDep handle. It's the same
 * identifier specified in snd_hwdep_open().
 */
const char *snd_hwdep_name(snd_hwdep_t *hwdep)
{
	assert(hwdep);
	return hwdep->name;
}

/**
 * \brief get type of HwDep handle
 * \param hwdep a HwDep handle
 * \return type of HwDep handle
 *
 * Returns the type #snd_hwdep_type_t of given HwDep handle.
 */
snd_hwdep_type_t snd_hwdep_type(snd_hwdep_t *hwdep)
{
	assert(hwdep);
	return hwdep->type;
}

/**
 * \brief get count of poll descriptors for HwDep handle
 * \param hwdep HwDep handle
 * \return count of poll descriptors
 */
int snd_hwdep_poll_descriptors_count(snd_hwdep_t *hwdep)
{
	assert(hwdep);
	return 1;
}

/**
 * \brief get poll descriptors
 * \param hwdep HwDep handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */
int snd_hwdep_poll_descriptors(snd_hwdep_t *hwdep, struct pollfd *pfds, unsigned int space)
{
	assert(hwdep);
	if (space >= 1) {
		pfds->fd = hwdep->poll_fd;
		switch (hwdep->mode & O_ACCMODE) {
		case O_WRONLY:
			pfds->events = POLLOUT|POLLERR;
			break;
		case O_RDONLY:
			pfds->events = POLLIN|POLLERR;
			break;
		case O_RDWR:
			pfds->events = POLLOUT|POLLIN|POLLERR;
			break;
		default:
			return -EIO;
		}
		return 1;
	}
	return 0;
}

/**
 * \brief get returned events from poll descriptors
 * \param hwdep HwDep  handle
 * \param pfds array of poll descriptors
 * \param nfds count of poll descriptors
 * \param revents returned events
 * \return zero if success, otherwise a negative error code
 */
int snd_hwdep_poll_descriptors_revents(snd_hwdep_t *hwdep, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
        assert(hwdep && pfds && revents);
        if (nfds == 1) {
                *revents = pfds->revents;
                return 0;
        }
        return -EINVAL;
}                                                                       
                                                                       
/**
 * \brief set nonblock mode
 * \param hwdep HwDep handle
 * \param nonblock 0 = block, 1 = nonblock mode
 * \return 0 on success otherwise a negative error code
 */
int snd_hwdep_nonblock(snd_hwdep_t *hwdep, int nonblock)
{
	int err;
	assert(hwdep);
	if ((err = hwdep->ops->nonblock(hwdep, nonblock)) < 0)
		return err;
	if (nonblock)
		hwdep->mode |= SND_HWDEP_OPEN_NONBLOCK;
	else
		hwdep->mode &= ~SND_HWDEP_OPEN_NONBLOCK;
	return 0;
}

/**
 * \brief get size of the snd_hwdep_info_t structure in bytes
 * \return size of the snd_hwdep_info_t structure in bytes
 */
size_t snd_hwdep_info_sizeof()
{
	return sizeof(snd_hwdep_info_t);
}

/**
 * \brief allocate a new snd_hwdep_info_t structure
 * \param ptr returned pointer
 * \return 0 on success otherwise a negative error code if fails
 *
 * Allocates a new snd_hwdep_info_t structure using the standard
 * malloc C library function.
 */
int snd_hwdep_info_malloc(snd_hwdep_info_t **info)
{
	assert(info);
	*info = calloc(1, sizeof(snd_hwdep_info_t));
	if (!*info)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees the snd_hwdep_info_t structure
 * \param info pointer to the snd_hwdep_info_t structure to free
 *
 * Frees the given snd_hwdep_info_t structure using the standard
 * free C library function.
 */
void snd_hwdep_info_free(snd_hwdep_info_t *info)
{
	assert(info);
	free(info);
}

/**
 * \brief copy one snd_hwdep_info_t structure to another
 * \param dst destination snd_hwdep_info_t structure
 * \param src source snd_hwdep_info_t structure
 */
void snd_hwdep_info_copy(snd_hwdep_info_t *dst, const snd_hwdep_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief get hwdep card number
 * \param info pointer to a snd_hwdep_info_t structure
 * \return hwdep card number
 */
int snd_hwdep_info_get_card(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return obj->card;
}

/**
 * \brief get hwdep device number
 * \param info pointer to a snd_hwdep_info_t structure
 * \return hwdep device number
 */
unsigned int snd_hwdep_info_get_device(const snd_hwdep_info_t *info)
{
	assert(info);
	return info->device;
}

/**
 * \brief get hwdep driver identifier
 * \param info pointer to a snd_hwdep_info_t structure
 * \return hwdep driver identifier
 */
const char *snd_hwdep_info_get_id(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return obj->id;
}

/**
 * \brief get hwdep driver name
 * \param info pointer to a snd_hwdep_info_t structure
 * \return hwdep driver name
 */
const char *snd_hwdep_info_get_name(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return obj->name;
}

/**
 * \brief get hwdep protocol interface
 * \param info pointer to a snd_hwdep_info_t structure
 * \return hwdep protocol interface
 */
snd_hwdep_iface_t snd_hwdep_info_get_iface(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return obj->iface;
}

/**
 * \brief set hwdep device number
 * \param info pointer to a snd_hwdep_info_t structure
 * \param val hwdep device
 */
void snd_hwdep_info_set_device(snd_hwdep_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->device = val;
}

/**
 * \brief get information about HwDep handle
 * \param hwdep HwDep handle
 * \param info pointer to a snd_hwdep_info_t structure to be filled
 * \return 0 on success otherwise a negative error code
 */
int snd_hwdep_info(snd_hwdep_t *hwdep, snd_hwdep_info_t * info)
{
	assert(hwdep);
	assert(info);
	return hwdep->ops->info(hwdep, info);
}

/**
 * \brief do hardware dependent ioctl
 * \param hwdep HwDep handle
 * \param request ioctl command
 * \param arg ioctl argument
 * \return 0 on success otherwise a negative error code
 */
int snd_hwdep_ioctl(snd_hwdep_t *hwdep, unsigned int request, void * arg)
{
	assert(hwdep);
	return hwdep->ops->ioctl(hwdep, request, arg);
}

/**
 * \brief write bytes using HwDep handle
 * \param hwdep HwDep handle
 * \param buffer buffer containing bytes to write
 * \param size output buffer size in bytes
 */
ssize_t snd_hwdep_write(snd_hwdep_t *hwdep, const void *buffer, size_t size)
{
	assert(hwdep);
	assert(((hwdep->mode & O_ACCMODE) == O_WRONLY) || ((hwdep->mode & O_ACCMODE) == O_RDWR));
	assert(buffer || size == 0);
	return hwdep->ops->write(hwdep, buffer, size);
}

/**
 * \brief read bytes using HwDep handle
 * \param hwdep HwDep handle
 * \param buffer buffer to store the input bytes
 * \param size input buffer size in bytes
 */
ssize_t snd_hwdep_read(snd_hwdep_t *hwdep, void *buffer, size_t size)
{
	assert(hwdep);
	assert(((hwdep->mode & O_ACCMODE) == O_RDONLY) || ((hwdep->mode & O_ACCMODE) == O_RDWR));
	assert(buffer || size == 0);
	return hwdep->ops->read(hwdep, buffer, size);
}
