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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/poll.h>
#include "control_local.h"

const char *snd_ctl_name(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->name;
}

snd_ctl_type_t snd_ctl_type(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->type;
}

int snd_ctl_close(snd_ctl_t *ctl)
{
	int res;
	res = ctl->ops->close(ctl);
	if (ctl->name)
		free(ctl->name);
	free(ctl);
	return res;
}

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

int snd_ctl_async(snd_ctl_t *ctl, int sig, pid_t pid)
{
	assert(ctl);
	return ctl->ops->async(ctl, sig, pid);
}

int snd_ctl_poll_descriptor(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->ops->poll_descriptor(ctl);
}

int snd_ctl_card_info(snd_ctl_t *ctl, snd_ctl_card_info_t *info)
{
	assert(ctl && info);
	return ctl->ops->hw_info(ctl, info);
}

int snd_ctl_elem_list(snd_ctl_t *ctl, snd_ctl_elem_list_t *list)
{
	assert(ctl && list);
	assert(list->space == 0 || list->pids);
	return ctl->ops->element_list(ctl, list);
}

int snd_ctl_elem_info(snd_ctl_t *ctl, snd_ctl_elem_info_t *info)
{
	assert(ctl && info && (info->id.name[0] || info->id.numid));
	return ctl->ops->element_info(ctl, info);
}

int snd_ctl_elem_read(snd_ctl_t *ctl, snd_ctl_elem_value_t *control)
{
	assert(ctl && control && (control->id.name[0] || control->id.numid));
	return ctl->ops->element_read(ctl, control);
}

int snd_ctl_elem_write(snd_ctl_t *ctl, snd_ctl_elem_value_t *control)
{
	assert(ctl && control && (control->id.name[0] || control->id.numid));
	return ctl->ops->element_write(ctl, control);
}

int snd_ctl_hwdep_next_device(snd_ctl_t *ctl, int *device)
{
	assert(ctl && device);
	return ctl->ops->hwdep_next_device(ctl, device);
}

int snd_ctl_hwdep_info(snd_ctl_t *ctl, snd_hwdep_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->hwdep_info(ctl, info);
}

int snd_ctl_pcm_next_device(snd_ctl_t *ctl, int * device)
{
	assert(ctl && device);
	return ctl->ops->pcm_next_device(ctl, device);
}

int snd_ctl_pcm_info(snd_ctl_t *ctl, snd_pcm_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->pcm_info(ctl, info);
}

int snd_ctl_pcm_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	assert(ctl);
	return ctl->ops->pcm_prefer_subdevice(ctl, subdev);
}

int snd_ctl_rawmidi_next_device(snd_ctl_t *ctl, int * device)
{
	assert(ctl && device);
	return ctl->ops->rawmidi_next_device(ctl, device);
}

int snd_ctl_rawmidi_info(snd_ctl_t *ctl, snd_rawmidi_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->rawmidi_info(ctl, info);
}

int snd_ctl_rawmidi_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	assert(ctl);
	return ctl->ops->rawmidi_prefer_subdevice(ctl, subdev);
}

int snd_ctl_read(snd_ctl_t *ctl, snd_ctl_event_t *event)
{
	assert(ctl && event);
	return ctl->ops->read(ctl, event);
}

int snd_ctl_wait(snd_ctl_t *ctl, int timeout)
{
	struct pollfd pfd;
	int err;
	pfd.fd = snd_ctl_poll_descriptor(ctl);
	pfd.events = POLLIN;
	err = poll(&pfd, 1, timeout);
	if (err < 0)
		return -errno;
	return 0;
}

int snd_ctl_open(snd_ctl_t **ctlp, const char *name)
{
	const char *str;
	int err;
	snd_config_t *ctl_conf, *conf, *type_conf;
	snd_config_iterator_t i, next;
	const char *lib = NULL, *open = NULL;
	int (*open_func)(snd_ctl_t **ctlp, const char *name, snd_config_t *conf);
	void *h;
	assert(ctlp && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	err = snd_config_searchv(snd_config, &ctl_conf, "ctl", name, 0);
	if (err < 0) {
		int card;
		char socket[256], sname[256];
		err = sscanf(name, "hw:%d", &card);
		if (err == 1)
			return snd_ctl_hw_open(ctlp, name, card);
		err = sscanf(name, "shm:%256[^,],%256[^,]", socket, sname);
		if (err == 2)
			return snd_ctl_shm_open(ctlp, name, socket, sname);
		ERR("Unknown control %s", name);
		return -ENOENT;
	}
	if (snd_config_get_type(ctl_conf) != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;
	err = snd_config_search(ctl_conf, "type", &conf);
	if (err < 0)
		return err;
	err = snd_config_get_string(conf, &str);
	if (err < 0)
		return err;
	err = snd_config_searchv(snd_config, &type_conf, "ctltype", str, 0);
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
			return -EINVAL;
		}
	}
	if (!open)
		return -EINVAL;
	if (!lib)
		lib = "libasound.so";
	h = dlopen(lib, RTLD_NOW);
	if (!h)
		return -ENOENT;
	open_func = dlsym(h, open);
	dlclose(h);
	if (!open_func)
		return -ENXIO;
	return open_func(ctlp, name, ctl_conf);
}

void snd_ctl_elem_set_bytes(snd_ctl_elem_value_t *obj, void *data, size_t size)
{
	assert(obj);
	assert(size <= sizeof(obj->value.bytes.data));
	memcpy(obj->value.bytes.data, data, size);
}

#define TYPE(v) [SND_CTL_ELEM_TYPE_##v] = #v
#define IFACE(v) [SND_CTL_ELEM_IFACE_##v] = #v
#define EVENT(v) [SND_CTL_EVENT_##v] = #v

const char *snd_ctl_elem_type_names[] = {
	TYPE(NONE),
	TYPE(BOOLEAN),
	TYPE(INTEGER),
	TYPE(ENUMERATED),
	TYPE(BYTES),
	TYPE(IEC958),
};

const char *snd_ctl_elem_iface_names[] = {
	IFACE(CARD),
	IFACE(HWDEP),
	IFACE(MIXER),
	IFACE(PCM),
	IFACE(RAWMIDI),
	IFACE(TIMER),
	IFACE(SEQUENCER),
};

const char *snd_ctl_event_type_names[] = {
	EVENT(REBUILD),
	EVENT(VALUE),
	EVENT(INFO),
	EVENT(ADD),
	EVENT(REMOVE),
};

const char *snd_ctl_elem_type_name(snd_ctl_elem_type_t type)
{
	assert(type <= SND_CTL_ELEM_TYPE_LAST);
	return snd_ctl_elem_type_names[snd_enum_to_int(type)];
}

const char *snd_ctl_elem_iface_name(snd_ctl_elem_iface_t iface)
{
	assert(iface <= SND_CTL_ELEM_IFACE_LAST);
	return snd_ctl_elem_iface_names[snd_enum_to_int(iface)];
}

const char *snd_ctl_event_type_name(snd_ctl_event_type_t type)
{
	assert(type <= SND_CTL_EVENT_LAST);
	return snd_ctl_event_type_names[snd_enum_to_int(type)];
}

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

void snd_ctl_elem_list_free_space(snd_ctl_elem_list_t *obj)
{
	free(obj->pids);
	obj->pids = NULL;
}
