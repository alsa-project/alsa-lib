/**
 * \file control/control.c
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2000
 *
 * CTL interface is designed to access primitive controls.
 */
/*
 *  Control Interface - main file
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

#ifndef DOC_HIDDEN
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/poll.h>
#include "control_local.h"
#endif

/**
 * \brief get identifier of CTL handle
 * \param ctl CTL handle
 * \return ascii identifier of CTL handle
 *
 * Returns the ASCII identifier of given CTL handle. It's the same
 * identifier specified in snd_ctl_open().
 */
const char *snd_ctl_name(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->name;
}

/**
 * \brief get type of CTL handle
 * \param ctl CTL handle
 * \return type of CTL handle
 *
 * Returns the type #snd_ctl_type_t of given CTL handle.
 */
snd_ctl_type_t snd_ctl_type(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->type;
}

/**
 * \brief close CTL handle
 * \param ctl CTL handle
 * \return 0 on success otherwise a negative error code
 *
 * Closes the specified CTL handle and frees all associated
 * resources.
 */
int snd_ctl_close(snd_ctl_t *ctl)
{
	int res;
	res = ctl->ops->close(ctl);
	if (ctl->name)
		free(ctl->name);
	free(ctl);
	return res;
}

/**
 * \brief set nonblock mode
 * \param ctl CTL handle
 * \param nonblock 0 = block, 1 = nonblock mode
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_nonblock(snd_ctl_t *ctl, int nonblock)
{
	int err;
	assert(ctl);
	err = ctl->ops->nonblock(ctl, nonblock);
	if (err < 0)
		return err;
	ctl->nonblock = nonblock;
	return 0;
}

/**
 * \brief set async mode
 * \param ctl CTL handle
 * \param sig Signal to raise: < 0 disable, 0 default (SIGIO)
 * \param pid Process ID to signal: 0 current
 * \return 0 on success otherwise a negative error code
 *
 * A signal is raised when a change happens.
 */
int snd_ctl_async(snd_ctl_t *ctl, int sig, pid_t pid)
{
	int err;
	assert(ctl);
	err = ctl->ops->async(ctl, sig, pid);
	if (err < 0)
		return err;
	if (sig)
		ctl->async_sig = sig;
	else
		ctl->async_sig = SIGIO;
	if (pid)
		ctl->async_pid = pid;
	else
		ctl->async_pid = getpid();
	return 0;
}

/**
 * \brief get count of poll descriptors for CTL handle
 * \param ctl CTL handle
 * \return count of poll descriptors
 */
int snd_ctl_poll_descriptors_count(snd_ctl_t *ctl)
{
	assert(ctl);
	return 1;
}

/**
 * \brief get poll descriptors
 * \param ctl CTL handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */
int snd_ctl_poll_descriptors(snd_ctl_t *ctl, struct pollfd *pfds, unsigned int space)
{
	assert(ctl);
	if (space > 0) {
		pfds->fd = ctl->ops->poll_descriptor(ctl);
		pfds->events = POLLIN;
		return 1;
	}
	return 0;
}

/**
 * \brief Ask to be informed about events (poll, #snd_ctl_async, #snd_ctl_read)
 * \param ctl CTL handle
 * \param subscribe 0 = unsubscribe, 1 = subscribe
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_subscribe_events(snd_ctl_t *ctl, int subscribe)
{
	assert(ctl);
	return ctl->ops->subscribe_events(ctl, subscribe);
}


/**
 * \brief Get card related information
 * \param ctl CTL handle
 * \param info Card info pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_card_info(snd_ctl_t *ctl, snd_ctl_card_info_t *info)
{
	assert(ctl && info);
	return ctl->ops->card_info(ctl, info);
}

/**
 * \brief Get a list of element identificators
 * \param ctl CTL handle
 * \param list CTL element identificators list pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_list(snd_ctl_t *ctl, snd_ctl_elem_list_t *list)
{
	assert(ctl && list);
	assert(list->space == 0 || list->pids);
	return ctl->ops->element_list(ctl, list);
}

/**
 * \brief Get CTL element information
 * \param ctl CTL handle
 * \param info CTL element id/information pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_info(snd_ctl_t *ctl, snd_ctl_elem_info_t *info)
{
	assert(ctl && info && (info->id.name[0] || info->id.numid));
	return ctl->ops->element_info(ctl, info);
}

/**
 * \brief Get CTL element value
 * \param ctl CTL handle
 * \param control CTL element id/value pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_read(snd_ctl_t *ctl, snd_ctl_elem_value_t *control)
{
	assert(ctl && control && (control->id.name[0] || control->id.numid));
	return ctl->ops->element_read(ctl, control);
}

/**
 * \brief Set CTL element value
 * \param ctl CTL handle
 * \param control CTL element id/value pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_write(snd_ctl_t *ctl, snd_ctl_elem_value_t *control)
{
	assert(ctl && control && (control->id.name[0] || control->id.numid));
	return ctl->ops->element_write(ctl, control);
}

/**
 * \brief Get next hardware dependent device number
 * \param ctl CTL handle
 * \param device current device on entry and next device on return
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_hwdep_next_device(snd_ctl_t *ctl, int *device)
{
	assert(ctl && device);
	return ctl->ops->hwdep_next_device(ctl, device);
}

/**
 * \brief Get info about a hardware dependent device
 * \param ctl CTL handle
 * \param info Hardware dependent device id/info pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_hwdep_info(snd_ctl_t *ctl, snd_hwdep_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->hwdep_info(ctl, info);
}

/**
 * \brief Get next PCM device number
 * \param ctl CTL handle
 * \param device current device on entry and next device on return
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_pcm_next_device(snd_ctl_t *ctl, int * device)
{
	assert(ctl && device);
	return ctl->ops->pcm_next_device(ctl, device);
}

/**
 * \brief Get info about a PCM device
 * \param ctl CTL handle
 * \param info PCM device id/info pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_pcm_info(snd_ctl_t *ctl, snd_pcm_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->pcm_info(ctl, info);
}

/**
 * \brief Set preferred PCM subdevice number of successive PCM open
 * \param ctl CTL handle
 * \param subdev Preferred PCM subdevice number
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_pcm_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	assert(ctl);
	return ctl->ops->pcm_prefer_subdevice(ctl, subdev);
}

/**
 * \brief Get next RawMidi device number
 * \param ctl CTL handle
 * \param device current device on entry and next device on return
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_rawmidi_next_device(snd_ctl_t *ctl, int * device)
{
	assert(ctl && device);
	return ctl->ops->rawmidi_next_device(ctl, device);
}

/**
 * \brief Get info about a RawMidi device
 * \param ctl CTL handle
 * \param info RawMidi device id/info pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_rawmidi_info(snd_ctl_t *ctl, snd_rawmidi_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->rawmidi_info(ctl, info);
}

/**
 * \brief Set preferred RawMidi subdevice number of successive RawMidi open
 * \param ctl CTL handle
 * \param subdev Preferred RawMidi subdevice number
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_rawmidi_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	assert(ctl);
	return ctl->ops->rawmidi_prefer_subdevice(ctl, subdev);
}


/**
 * \brief Read an event
 * \param ctl CTL handle
 * \param event Event pointer
 * \return number of events read otherwise a negative error code on failure
 */
int snd_ctl_read(snd_ctl_t *ctl, snd_ctl_event_t *event)
{
	assert(ctl && event);
	return ctl->ops->read(ctl, event);
}

/**
 * \brief Wait for a CTL to become ready (i.e. at least one event pending)
 * \param ctl CTL handle
 * \param timeout maximum time in milliseconds to wait
 * \return 0 otherwise a negative error code on failure
 */
int snd_ctl_wait(snd_ctl_t *ctl, int timeout)
{
	struct pollfd pfd;
	int err;
	err = snd_ctl_poll_descriptors(ctl, &pfd, 1);
	assert(err == 1);
	err = poll(&pfd, 1, timeout);
	if (err < 0)
		return -errno;
	return 0;
}

/**
 * \brief Opens a CTL
 * \param ctlp Returned CTL handle
 * \param name ASCII identifier of the CTL handle
 * \param mode Open mode (see #SND_CTL_NONBLOCK, #SND_CTL_ASYNC)
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_open(snd_ctl_t **ctlp, const char *name, int mode)
{
	const char *str;
	char buf[256];
	int err;
	snd_config_t *ctl_conf, *conf, *type_conf;
	snd_config_iterator_t i, next;
	const char *lib = NULL, *open = NULL;
	int (*open_func)(snd_ctl_t **ctlp, const char *name, snd_config_t *conf, int mode);
	void *h;
	const char *name1;
	assert(ctlp && name);
	err = snd_config_update();
	if (err < 0)
		return err;

	err = snd_config_search_alias(snd_config, "ctl", name, &ctl_conf);
	name1 = name;
	if (err < 0 || snd_config_get_string(ctl_conf, &name1) >= 0) {
		int card;
		char socket[256], sname[256];
		err = sscanf(name1, "hw:%d", &card);
		if (err == 1)
			return snd_ctl_hw_open(ctlp, name, card, mode);
		err = sscanf(name1, "shm:%256[^,],%256[^,]", socket, sname);
		if (err == 2)
			return snd_ctl_shm_open(ctlp, name, socket, sname, mode);
		SNDERR("Unknown ctl %s", name1);
		return -ENOENT;
	}
	if (snd_config_get_type(ctl_conf) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("Invalid type for %s", snd_config_get_id(ctl_conf));
		return -EINVAL;
	}
	err = snd_config_search(ctl_conf, "type", &conf);
	if (err < 0)
		return err;
	err = snd_config_get_string(conf, &str);
	if (err < 0)
		return err;
	err = snd_config_search_alias(snd_config, "ctl_type", str, &type_conf);
	if (err >= 0) {
		if (snd_config_get_type(type_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for ctl type %s definition", str);
			return -EINVAL;
		}
		snd_config_for_each(i, next, type_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0)
					return -EINVAL;
				continue;
			}
			if (strcmp(id, "open") == 0) {
				err = snd_config_get_string(n, &open);
				if (err < 0)
					return -EINVAL;
				continue;
			}
			SNDERR("Unknown field %s", id);
			return -EINVAL;
		}
	}
	if (!open) {
		open = buf;
		snprintf(buf, sizeof(buf), "_snd_ctl_%s_open", str);
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
	return open_func(ctlp, name, ctl_conf, mode);
}

/**
 * \brief Set CTL element #SND_CTL_ELEM_TYPE_BYTES value
 * \param ctl CTL handle
 * \param data Bytes value
 * \param size Size in bytes
 */
void snd_ctl_elem_set_bytes(snd_ctl_elem_value_t *obj, void *data, size_t size)
{
	assert(obj);
	if (size >= sizeof(obj->value.bytes.data)) {
		assert(0);
		return;
	}
	memcpy(obj->value.bytes.data, data, size);
}

#ifndef DOC_HIDDEN
#define TYPE(v) [SND_CTL_ELEM_TYPE_##v] = #v
#define IFACE(v) [SND_CTL_ELEM_IFACE_##v] = #v
#define EVENT(v) [SND_CTL_EVENT_##v] = #v

static const char *snd_ctl_elem_type_names[] = {
	TYPE(NONE),
	TYPE(BOOLEAN),
	TYPE(INTEGER),
	TYPE(ENUMERATED),
	TYPE(BYTES),
	TYPE(IEC958),
};

static const char *snd_ctl_elem_iface_names[] = {
	IFACE(CARD),
	IFACE(HWDEP),
	IFACE(MIXER),
	IFACE(PCM),
	IFACE(RAWMIDI),
	IFACE(TIMER),
	IFACE(SEQUENCER),
};

static const char *snd_ctl_event_type_names[] = {
	EVENT(ELEM),
};
#endif

/**
 * \brief get name of a CTL element type
 * \param type CTL element type
 * \return ascii name of CTL element type
 */
const char *snd_ctl_elem_type_name(snd_ctl_elem_type_t type)
{
	assert(type <= SND_CTL_ELEM_TYPE_LAST);
	return snd_ctl_elem_type_names[snd_enum_to_int(type)];
}

/**
 * \brief get name of a CTL element related interface
 * \param iface CTL element related interface
 * \return ascii name of CTL element related interface
 */
const char *snd_ctl_elem_iface_name(snd_ctl_elem_iface_t iface)
{
	assert(iface <= SND_CTL_ELEM_IFACE_LAST);
	return snd_ctl_elem_iface_names[snd_enum_to_int(iface)];
}

/**
 * \brief get name of a CTL event type
 * \param type CTL event type
 * \return ascii name of CTL event type
 */
const char *snd_ctl_event_type_name(snd_ctl_event_type_t type)
{
	assert(type <= SND_CTL_EVENT_LAST);
	return snd_ctl_event_type_names[snd_enum_to_int(type)];
}

/**
 * \brief allocate space for CTL element identificators list
 * \param obj CTL element identificators list
 * \param entries Entries to allocate
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_list_alloc_space(snd_ctl_elem_list_t *obj, unsigned int entries)
{
	if (obj->pids)
		free(obj->pids);
	obj->pids = calloc(entries, sizeof(*obj->pids));
	if (!obj->pids) {
		obj->space = 0;
		return -ENOMEM;
	}
	obj->space = entries;
	return 0;
}  

/**
 * \brief free previously allocated space for CTL element identificators list
 * \param obj CTL element identificators list
 */
void snd_ctl_elem_list_free_space(snd_ctl_elem_list_t *obj)
{
	free(obj->pids);
	obj->pids = NULL;
}

/**
 * \brief Get event mask for an element related event
 * \param obj CTL event
 * \return event mask for element related event
 */
unsigned int snd_ctl_event_elem_get_mask(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.mask;
}

/**
 * \brief Get element numeric identificator for an element related event
 * \param obj CTL event
 * \return element numeric identificator for element related event
 */
unsigned int snd_ctl_event_elem_get_numid(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.numid;
}

/**
 * \brief Get CTL element identificator for an element related event
 * \param obj CTL event
 * \param ptr Pointer to returned CTL element identificator
 */
void snd_ctl_event_elem_get_id(const snd_ctl_event_t *obj, snd_ctl_elem_id_t *ptr)
{
	assert(obj && ptr);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	*ptr = obj->data.elem.id;
}

/**
 * \brief Get interface part of CTL element identificator for an element related event
 * \param obj CTL event
 * \return interface part of element identificator
 */
snd_ctl_elem_iface_t snd_ctl_event_elem_get_interface(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return snd_int_to_enum(obj->data.elem.id.iface);
}

/**
 * \brief Get device part of CTL element identificator for an element related event
 * \param obj CTL event
 * \return device part of element identificator
 */
unsigned int snd_ctl_event_elem_get_device(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.device;
}

/**
 * \brief Get subdevice part of CTL element identificator for an element related event
 * \param obj CTL event
 * \return subdevice part of element identificator
 */
unsigned int snd_ctl_event_elem_get_subdevice(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.subdevice;
}

/**
 * \brief Get name part of CTL element identificator for an element related event
 * \param obj CTL event
 * \return name part of element identificator
 */
const char *snd_ctl_event_elem_get_name(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.name;
}

/**
 * \brief Get index part of CTL element identificator for an element related event
 * \param obj CTL event
 * \return index part of element identificator
 */
unsigned int snd_ctl_event_elem_get_index(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.index;
}

#ifndef DOC_HIDDEN
int _snd_ctl_poll_descriptor(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->ops->poll_descriptor(ctl);
}
#endif
