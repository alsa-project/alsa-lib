/**
 * \file rawmidi/rawmidi.c
 * \author Jaroslav Kysela <perex@suse.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2000-2001
 *
 * RawMidi Interface is designed to write or read raw (unchanged) MIDI
 * data over the MIDI line. MIDI stands Musical Instrument Digital Interface
 * and more information about this standard can be found at
 * http://www.midi.org.
 *
 * RawMidi devices are opened exclusively for a selected direction.
 * While more than one process may not open a given MIDI device in the same
 * direction simultaneously, separate processes may open a single MIDI device
 * in different directions (i.e. process one opens a MIDI device in write
 * direction and process two opens the same device in read direction). MIDI
 * devices return EBUSY error to applications when other applications have
 * already opened the requested direction (nonblock behaviour) or wait
 * until the device is not available (block behaviour).
 */
/*
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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include "rawmidi_local.h"

/**
 * \brief setup the default parameters
 * \param rawmidi RawMidi handle
 * \param params pointer to a snd_rawmidi_params_t structure
 * \return zero on success otherwise a negative error code
 */
static int snd_rawmidi_params_default(snd_rawmidi_t *rawmidi, snd_rawmidi_params_t *params)
{
	assert(rawmidi);
	assert(params);
	params->buffer_size = page_size();
	params->avail_min = 1;
	params->no_active_sensing = 0;
	return 0;
}

/**
 * \brief Opens a new connection to the RawMidi interface.
 * \param inputp Returned input handle (NULL if not wanted)
 * \param outputp Returned output handle (NULL if not wanted)
 * \param name ASCII identifier of the RawMidi handle
 * \param mode Open mode
 * \return a negative error code on failure or zero on success
 *
 * Opens a new connection to the RawMidi interface specified with
 * an ASCII identifier and mode.
 */
int snd_rawmidi_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp,
		     const char *name, int mode)
{
	const char *str;
	char buf[256];
	int err;
	snd_config_t *rawmidi_conf, *conf, *type_conf;
	snd_config_iterator_t i, next;
	snd_rawmidi_params_t params;
	const char *lib = NULL, *open = NULL;
	int (*open_func)(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp,
			 const char *name, snd_config_t *conf, int mode);
	void *h;
	const char *name1;
	assert((inputp || outputp) && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	err = snd_config_search_alias(snd_config, "rawmidi", name, &rawmidi_conf);
	name1 = name;
	if (err < 0 || snd_config_get_string(rawmidi_conf, &name1) >= 0) {
		int card, dev, subdev;
		err = sscanf(name1, "hw:%d,%d,%d", &card, &dev, &subdev);
		if (err == 3)
			return snd_rawmidi_hw_open(inputp, outputp, name, card, dev, subdev, mode);
		err = sscanf(name1, "hw:%d,%d", &card, &dev);
		if (err == 2)
			return snd_rawmidi_hw_open(inputp, outputp, name, card, dev, -1, mode);
		SNDERR("Unknown RAWMIDI %s", name1);
		return -ENOENT;
	}
	if (snd_config_get_type(rawmidi_conf) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("Invalid type for RAWMIDI %s definition", name);
		return -EINVAL;
	}
	err = snd_config_search(rawmidi_conf, "type", &conf);
	if (err < 0) {
		SNDERR("type is not defined");
		return err;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", snd_config_get_id(conf));
		return err;
	}
	err = snd_config_search_alias(snd_config, "rawmidi_type", str, &type_conf);
	if (err >= 0) {
		snd_config_for_each(i, next, type_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					return -EINVAL;
				}
				continue;
			}
			if (strcmp(id, "open") == 0) {
				err = snd_config_get_string(n, &open);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					return -EINVAL;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
			return -EINVAL;
		}
	}
	if (!open) {
		open = buf;
		snprintf(buf, sizeof(buf), "_snd_rawmidi_%s_open", str);
	}
	if (!lib)
		lib = "libasound.so";
	h = dlopen(lib, RTLD_NOW);
	if (!h) {
		SNDERR("Cannot open shared library %s", lib);
		return -ENOENT;
	}
	open_func = dlsym(h, open);
	if (!open_func) {
		SNDERR("symbol %s is not defined inside %s", open, lib);
		dlclose(h);
		return -ENXIO;
	}
	err = open_func(inputp, outputp, name, rawmidi_conf, mode);
	if (err < 0)
		return err;
	if (inputp) {
		snd_rawmidi_params_default(*inputp, &params);
		err = snd_rawmidi_params(*inputp, &params);
		assert(err >= 0);
	}
	if (outputp) {
		snd_rawmidi_params_default(*outputp, &params);
		err = snd_rawmidi_params(*outputp, &params);
		assert(err >= 0);
	}
	return 0;
}

/**
 * \brief close RawMidi handle
 * \param rawmidi RawMidi handle
 * \return zero on success otherwise a negative error code
 *
 * Closes the specified RawMidi handle and frees all associated
 * resources.
 */
int snd_rawmidi_close(snd_rawmidi_t *rawmidi)
{
	int err;
  	assert(rawmidi);
	if ((err = rawmidi->ops->close(rawmidi)) < 0)
		return err;
	if (rawmidi->name)
		free(rawmidi->name);
	free(rawmidi);
	return 0;
}

/**
 * \brief get identifier of RawMidi handle
 * \param rawmidi a RawMidi handle
 * \return ascii identifier of RawMidi handle
 *
 * Returns the ASCII identifier of given RawMidi handle. It's the same
 * identifier as for snd_rawmidi_open().
 */
const char *snd_rawmidi_name(snd_rawmidi_t *rawmidi)
{
	assert(rawmidi);
	return rawmidi->name;
}

/**
 * \brief get type of RawMidi handle
 * \param rawmidi a RawMidi handle
 * \return type of RawMidi handle
 *
 * Returns the type #snd_rawmidi_type_t of given RawMidi handle.
 */
snd_rawmidi_type_t snd_rawmidi_type(snd_rawmidi_t *rawmidi)
{
	assert(rawmidi);
	return rawmidi->type;
}

/**
 * \brief get stream (direction) of RawMidi handle
 * \param rawmidi a RawMidi handle
 * \return stream of RawMidi handle
 *
 * Returns the stream #snd_rawmidi_stream_t of given RawMidi handle.
 */
snd_rawmidi_stream_t snd_rawmidi_stream(snd_rawmidi_t *rawmidi)
{
	assert(rawmidi);
	return rawmidi->stream;
}

/**
 * \brief get count of poll descriptors for RawMidi handle
 * \param rawmidi RawMidi handle
 * \return count of poll descriptors
 */
int snd_rawmidi_poll_descriptors_count(snd_rawmidi_t *rawmidi)
{
	assert(rawmidi);
	return 1;
}

/**
 * \brief get poll descriptors
 * \param rawmidi RawMidi handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */
int snd_rawmidi_poll_descriptors(snd_rawmidi_t *rawmidi, struct pollfd *pfds, unsigned int space)
{
	assert(rawmidi);
	if (space >= 1) {
		pfds->fd = rawmidi->poll_fd;
		pfds->events = rawmidi->stream == SND_RAWMIDI_STREAM_OUTPUT ? POLLOUT : POLLIN;
		return 1;
	}
	return 0;
}

/**
 * \brief set nonblock mode
 * \param rawmidi RawMidi handle
 * \param nonblock 0 = block, 1 = nonblock mode
 * \return zero on success otherwise a negative error code
 */
int snd_rawmidi_nonblock(snd_rawmidi_t *rawmidi, int nonblock)
{
	int err;
	assert(rawmidi);
	assert(!(rawmidi->mode & SND_RAWMIDI_APPEND));
	if ((err = rawmidi->ops->nonblock(rawmidi, nonblock)) < 0)
		return err;
	if (nonblock)
		rawmidi->mode |= SND_RAWMIDI_NONBLOCK;
	else
		rawmidi->mode &= ~SND_RAWMIDI_NONBLOCK;
	return 0;
}

/**
 * \brief get size of the snd_rawmidi_info_t structure in bytes
 * \return size of the snd_rawmidi_info_t structure in bytes
 */
size_t snd_rawmidi_info_sizeof()
{
	return sizeof(snd_rawmidi_info_t);
}

/**
 * \brief allocate a new snd_rawmidi_info_t structure
 * \param ptr returned pointer
 * \return zero on success or a negative error code if fails
 *
 * Allocates a new snd_rawmidi_params_t structure using the standard
 * malloc C library function.
 */
int snd_rawmidi_info_malloc(snd_rawmidi_info_t **info)
{
	assert(info);
	*info = calloc(1, sizeof(snd_rawmidi_info_t));
	if (!*info)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees the snd_rawmidi_info_t structure
 * \param info pointer to the snd_rawmidi_info_t structure to free
 *
 * Frees the given snd_rawmidi_params_t structure using the standard
 * free C library function.
 */
void snd_rawmidi_info_free(snd_rawmidi_info_t *info)
{
	assert(info);
	free(info);
}

/**
 * \brief copy one snd_rawmidi_info_t structure to another
 * \param dst destination snd_rawmidi_info_t structure
 * \param src source snd_rawmidi_info_t structure
 */
void snd_rawmidi_info_copy(snd_rawmidi_info_t *dst, const snd_rawmidi_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief get rawmidi device number
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi device number
 */
unsigned int snd_rawmidi_info_get_device(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->device;
}

/**
 * \brief get rawmidi subdevice number
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi subdevice number
 */
unsigned int snd_rawmidi_info_get_subdevice(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->subdevice;
}

/**
 * \brief get rawmidi stream identification
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi stream identification
 */
snd_rawmidi_stream_t snd_rawmidi_info_get_stream(const snd_rawmidi_info_t *info)
{
	assert(info);
	return snd_int_to_enum(info->stream);
}

/**
 * \brief get rawmidi card number
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi card number
 */
int snd_rawmidi_info_get_card(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->card;
}

/**
 * \brief get rawmidi flags
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi flags
 */
unsigned int snd_rawmidi_info_get_flags(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->flags;
}

/**
 * \brief get rawmidi hardware driver identifier
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi hardware driver identifier
 */
const char *snd_rawmidi_info_get_id(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->id;
}

/**
 * \brief get rawmidi hardware driver name
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi hardware driver name
 */
const char *snd_rawmidi_info_get_name(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->name;
}

/**
 * \brief get rawmidi subdevice name
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi subdevice name
 */
const char *snd_rawmidi_info_get_subdevice_name(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->subname;
}

/**
 * \brief get rawmidi count of subdevices
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi count of subdevices
 */
unsigned int snd_rawmidi_info_get_subdevices_count(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->subdevices_count;
}

/**
 * \brief get rawmidi available count of subdevices
 * \param info pointer to a snd_rawmidi_info_t structure
 * \return rawmidi available count of subdevices
 */
unsigned int snd_rawmidi_info_get_subdevices_avail(const snd_rawmidi_info_t *info)
{
	assert(info);
	return info->subdevices_avail;
}

/**
 * \brief set rawmidi device number
 * \param info pointer to a snd_rawmidi_info_t structure
 * \param val device number
 */
void snd_rawmidi_info_set_device(snd_rawmidi_info_t *info, unsigned int val)
{
	assert(info);
	info->device = val;
}

/**
 * \brief set rawmidi subdevice number
 * \param info pointer to a snd_rawmidi_info_t structure
 * \param val subdevice number
 */
void snd_rawmidi_info_set_subdevice(snd_rawmidi_info_t *info, unsigned int val)
{
	assert(info);
	info->subdevice = val;
}

/**
 * \brief set rawmidi stream identifier
 * \param info pointer to a snd_rawmidi_info_t structure
 * \param val rawmidi stream identifier
 */
void snd_rawmidi_info_set_stream(snd_rawmidi_info_t *info, snd_rawmidi_stream_t val)
{
	assert(info);
	info->stream = snd_enum_to_int(val);
}

/**
 * \brief get information about RawMidi handle
 * \param rawmidi RawMidi handle
 * \param info pointer to a snd_rawmidi_info_t structure to be filled
 * \return zero on success otherwise a negative error code
 */
int snd_rawmidi_info(snd_rawmidi_t *rawmidi, snd_rawmidi_info_t * info)
{
	assert(rawmidi);
	assert(info);
	return rawmidi->ops->info(rawmidi, info);
}

/**
 * \brief get size of the snd_rawmidi_params_t structure in bytes
 * \return size of the snd_rawmidi_params_t structure in bytes
 */
size_t snd_rawmidi_params_sizeof()
{
	return sizeof(snd_rawmidi_params_t);
}

/**
 * \brief allocate the snd_rawmidi_params_t structure
 * \param ptr returned pointer
 * \return zero on success or a negative error code if fails
 *
 * Allocates a new snd_rawmidi_params_t structure using the standard
 * malloc C library function.
 */
int snd_rawmidi_params_malloc(snd_rawmidi_params_t **params)
{
	assert(params);
	*params = calloc(1, sizeof(snd_rawmidi_params_t));
	if (!*params)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees the snd_rawmidi_params_t structure
 * \param params pointer to the #snd_rawmidi_params_t structure to free
 *
 * Frees the given snd_rawmidi_params_t structure using the standard
 * free C library function.
 */
void snd_rawmidi_params_free(snd_rawmidi_params_t *params)
{
	assert(params);
	free(params);
}

/**
 * \brief copy one snd_rawmidi_params_t structure to another
 * \param dst destination snd_rawmidi_params_t structure
 * \param src source snd_rawmidi_params_t structure
 */
void snd_rawmidi_params_copy(snd_rawmidi_params_t *dst, const snd_rawmidi_params_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief set rawmidi I/O ring buffer size
 * \param rawmidi RawMidi handle
 * \param params pointer to a snd_rawmidi_params_t structure
 * \param val size in bytes
 * \return zero on success otherwise a negative error code
 */
int snd_rawmidi_params_set_buffer_size(snd_rawmidi_t *rawmidi ATTRIBUTE_UNUSED, snd_rawmidi_params_t *params, size_t val)
{
	assert(rawmidi && params);
	assert(val > params->avail_min);
	params->buffer_size = val;
	return 0;
}

/**
 * \brief get rawmidi I/O ring buffer size
 * \param params pointer to a snd_rawmidi_params_t structure
 * \return size of rawmidi I/O ring buffer in bytes
 */
size_t snd_rawmidi_params_get_buffer_size(const snd_rawmidi_params_t *params)
{
	assert(params);
	return params->buffer_size;
}

/**
 * \brief set minimum available bytes in rawmidi I/O ring buffer for wakeup
 * \param rawmidi RawMidi handle
 * \param params pointer to a snd_rawmidi_params_t structure
 * \param val desired value
 */
int snd_rawmidi_params_set_avail_min(snd_rawmidi_t *rawmidi ATTRIBUTE_UNUSED, snd_rawmidi_params_t *params, size_t val)
{
	assert(rawmidi && params);
	assert(val < params->buffer_size);
	params->avail_min = val;
	return 0;
}

/**
 * \brief get minimum available bytes in rawmidi I/O ring buffer for wakeup
 * \param params pointer to snd_rawmidi_params_t structure
 * \return minimum available bytes
 */
size_t snd_rawmidi_params_get_avail_min(const snd_rawmidi_params_t *params)
{
	assert(params);
	return params->avail_min;
}

/**
 * \brief set no-active-sensing action on snd_rawmidi_close()
 * \param rawmidi RawMidi handle
 * \param params pointer to snd_rawmidi_params_t structure
 * \param val value: 0 = enable to send the active sensing message, 1 = disable
 * \return zero on success otherwise a negative error code
 */
int snd_rawmidi_params_set_no_active_sensing(snd_rawmidi_t *rawmidi ATTRIBUTE_UNUSED, snd_rawmidi_params_t *params, int val)
{
	assert(rawmidi && params);
	params->no_active_sensing = val;
	return 0;
}

/**
 * \brief get no-active-sensing action status
 * \param params pointer to snd_rawmidi_params_t structure
 * \return the current status (0 = enable, 1 = disable the active sensing message)
 */
int snd_rawmidi_params_get_no_active_sensing(const snd_rawmidi_params_t *params)
{
	assert(params);
	return params->no_active_sensing;
}

/**
 * \brief get parameters about rawmidi stream
 * \param rawmidi RawMidi handle
 * \param params pointer to a snd_rawmidi_params_t structure to be filled
 * \return zero on success otherwise a negative error code
 */
int snd_rawmidi_params(snd_rawmidi_t *rawmidi, snd_rawmidi_params_t * params)
{
	int err;
	assert(rawmidi);
	assert(params);
	err = rawmidi->ops->params(rawmidi, params);
	if (err < 0)
		return err;
	rawmidi->buffer_size = params->buffer_size;
	rawmidi->avail_min = params->avail_min;
	rawmidi->no_active_sensing = params->no_active_sensing;
	return 0;
}

/**
 * \brief get current parameters about rawmidi stream
 * \param rawmidi RawMidi handle
 * \param params pointer to a snd_rawmidi_params_t structure to be filled
 * \return zero on success otherwise a negative error code
 */
int snd_rawmidi_params_current(snd_rawmidi_t *rawmidi, snd_rawmidi_params_t *params)
{
	assert(rawmidi);
	assert(params);
	params->buffer_size = rawmidi->buffer_size;
	params->avail_min = rawmidi->avail_min;
	params->no_active_sensing = rawmidi->no_active_sensing;
	return 0;
}

/**
 * \brief get size of the snd_rawmidi_status_t structure in bytes
 * \return size of the snd_rawmidi_status_t structure in bytes
 */
size_t snd_rawmidi_status_sizeof()
{
	return sizeof(snd_rawmidi_status_t);
}

/**
 * \brief allocate the snd_rawmidi_status_t structure
 * \param ptr returned pointer
 * \return zero on success or a negative error code if fails
 *
 * Allocates a new snd_rawmidi_status_t structure using the standard
 * malloc C library function.
 */
int snd_rawmidi_status_malloc(snd_rawmidi_status_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_rawmidi_status_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees the snd_rawmidi_status_t structure
 * \param status pointer to the snd_rawmidi_status_t structure to free
 *
 * Frees the given snd_rawmidi_status_t structure using the standard
 * free C library function.
 */
void snd_rawmidi_status_free(snd_rawmidi_status_t *status)
{
	assert(status);
	free(status);
}

/**
 * \brief copy one snd_rawmidi_status_t structure to another
 * \param dst destination snd_rawmidi_status_t structure
 * \param src source snd_rawmidi_status_t structure
 */
void snd_rawmidi_status_copy(snd_rawmidi_status_t *dst, const snd_rawmidi_status_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief get the start timestamp
 * \param status pointer to a snd_rawmidi_status_t structure
 * \param tstamp returned timestamp value
 */
void snd_rawmidi_status_get_tstamp(const snd_rawmidi_status_t *status, snd_timestamp_t *tstamp)
{
	assert(status && tstamp);
	*tstamp = status->tstamp;
}

/**
 * \brief get current available bytes in the rawmidi I/O ring buffer
 * \param status pointer to a snd_rawmidi_status_t structure
 * \return current available bytes in the rawmidi I/O ring buffer
 */
size_t snd_rawmidi_status_get_avail(const snd_rawmidi_status_t *status)
{
	assert(status);
	return status->avail;
}

/**
 * \brief get count of xruns
 * \param status pointer to a snd_rawmidi_status_t structure
 * \return count of xruns
 */
size_t snd_rawmidi_status_get_xruns(const snd_rawmidi_status_t *status)
{
	assert(status);
	return status->xruns;
}

/**
 * \brief get status of rawmidi stream
 * \param rawmidi RawMidi handle
 * \param status pointer to a snd_rawmidi_status_t structure to be filled
 * \return zero on success otherwise a negative error code
 */
int snd_rawmidi_status(snd_rawmidi_t *rawmidi, snd_rawmidi_status_t * status)
{
	assert(rawmidi);
	assert(status);
	return rawmidi->ops->status(rawmidi, status);
}

/**
 * \brief drop all bytes in the rawmidi I/O ring buffer immediately
 * \param rawmidi RawMidi handle
 * \return zero on success otherwise a negative error code
 */
int snd_rawmidi_drop(snd_rawmidi_t *rawmidi)
{
	assert(rawmidi);
	return rawmidi->ops->drop(rawmidi);
}

/**
 * \brief drain all bytes in the rawmidi I/O ring buffer
 * \param rawmidi RawMidi handle
 * \return zero on success otherwise a negative error code
 *
 * Waits until all MIDI bytes are not drained (sent) to the
 * hardware device.
 */
int snd_rawmidi_drain(snd_rawmidi_t *rawmidi)
{
	assert(rawmidi);
	return rawmidi->ops->drain(rawmidi);
}

/**
 * \brief write MIDI bytes to MIDI stream
 * \param rawmidi RawMidi handle
 * \param buffer buffer containing MIDI bytes
 * \param size output buffer size in bytes
 */
ssize_t snd_rawmidi_write(snd_rawmidi_t *rawmidi, const void *buffer, size_t size)
{
	assert(rawmidi);
	assert(rawmidi->stream == SND_RAWMIDI_STREAM_OUTPUT);
	assert(buffer || size == 0);
	return rawmidi->ops->write(rawmidi, buffer, size);
}

/**
 * \brief read MIDI bytes from MIDI stream
 * \param rawmidi RawMidi handle
 * \param buffer buffer to store the input MIDI bytes
 * \param size input buffer size in bytes
 */
ssize_t snd_rawmidi_read(snd_rawmidi_t *rawmidi, void *buffer, size_t size)
{
	assert(rawmidi);
	assert(rawmidi->stream == SND_RAWMIDI_STREAM_INPUT);
	assert(buffer || size == 0);
	return rawmidi->ops->read(rawmidi, buffer, size);
}
