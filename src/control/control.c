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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include "asoundlib.h"
#include "control_local.h"

snd_ctl_type_t snd_ctl_type(snd_ctl_t *ctl)
{
	return ctl->type;
}

int snd_ctl_close(snd_ctl_t *ctl)
{
	int res;
	assert(ctl);
	res = ctl->ops->close(ctl);
	free(ctl);
	return res;
}

int snd_ctl_poll_descriptor(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->ops->poll_descriptor(ctl);
}

int snd_ctl_hw_info(snd_ctl_t *ctl, snd_ctl_hw_info_t *info)
{
	assert(ctl && info);
	return ctl->ops->hw_info(ctl, info);
}

int snd_ctl_clist(snd_ctl_t *ctl, snd_control_list_t *list)
{
	assert(ctl && list);
	return ctl->ops->clist(ctl, list);
}

int snd_ctl_cinfo(snd_ctl_t *ctl, snd_control_info_t *info)
{
	assert(ctl && info && (info->id.name[0] || info->id.numid));
	return ctl->ops->cinfo(ctl, info);
}

int snd_ctl_cread(snd_ctl_t *ctl, snd_control_t *control)
{
	assert(ctl && control && (control->id.name[0] || control->id.numid));
	return ctl->ops->cread(ctl, control);
}

int snd_ctl_cwrite(snd_ctl_t *ctl, snd_control_t *control)
{
	assert(ctl && control && (control->id.name[0] || control->id.numid));
	return ctl->ops->cwrite(ctl, control);
}

int snd_ctl_hwdep_info(snd_ctl_t *ctl, snd_hwdep_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->hwdep_info(ctl, info);
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

int snd_ctl_rawmidi_info(snd_ctl_t *ctl, snd_rawmidi_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->rawmidi_info(ctl, info);
}

int snd_ctl_read1(snd_ctl_t *ctl, snd_ctl_event_t *event)
{
	assert(ctl && event);
	return ctl->ops->read(ctl, event);
}

int snd_ctl_read(snd_ctl_t *ctl, snd_ctl_callbacks_t * callbacks)
{
	int result, count;
	snd_ctl_event_t r;

	assert(ctl);
	count = 0;
	while ((result = snd_ctl_read1(ctl, &r)) > 0) {
		if (result != sizeof(r))
			return -EIO;
		if (!callbacks)
			continue;
		switch (r.type) {
		case SND_CTL_EVENT_REBUILD:
			if (callbacks->rebuild)
				callbacks->rebuild(ctl, callbacks->private_data);
			break;
		case SND_CTL_EVENT_VALUE:
			if (callbacks->value)
				callbacks->value(ctl, callbacks->private_data, &r.data.id);
			break;
		case SND_CTL_EVENT_CHANGE:
			if (callbacks->change)
				callbacks->change(ctl, callbacks->private_data, &r.data.id);
			break;
		case SND_CTL_EVENT_ADD:
			if (callbacks->add)
				callbacks->add(ctl, callbacks->private_data, &r.data.id);
			break;
		case SND_CTL_EVENT_REMOVE:
			if (callbacks->remove)
				callbacks->remove(ctl, callbacks->private_data, &r.data.id);
			break;
		}
		count++;
	}
	return result >= 0 ? count : -errno;
}

static int _snd_ctl_open_hw(snd_ctl_t **handlep, snd_config_t *conf)
{
	snd_config_iterator_t i;
	long card = -1;
	char *str;
	int err;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "card") == 0) {
			err = snd_config_integer_get(n, &card);
			if (err < 0) {
				err = snd_config_string_get(n, &str);
				if (err < 0)
					return -EINVAL;
				card = snd_card_get_index(str);
				if (card < 0)
					return card;
			}
			continue;
		}
		return -EINVAL;
	}
	if (card < 0)
		return -EINVAL;
	return snd_ctl_hw_open(handlep, card);
}
				
static int _snd_ctl_open_client(snd_ctl_t **handlep, snd_config_t *conf)
{
	snd_config_iterator_t i;
	char *socket = NULL;
	char *name = NULL;
	char *host = NULL;
	long port = -1;
	int err;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "socket") == 0) {
			err = snd_config_string_get(n, &socket);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "host") == 0) {
			err = snd_config_string_get(n, &host);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "port") == 0) {
			err = snd_config_integer_get(n, &port);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "name") == 0) {
			err = snd_config_string_get(n, &name);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		return -EINVAL;
	}
	if (!name)
		return -EINVAL;
	if (socket) {
		if (port >= 0 || host)
			return -EINVAL;
		return snd_ctl_client_open(handlep, socket, -1, SND_TRANSPORT_TYPE_SHM, name);
	} else  {
		if (port < 0 || !name)
			return -EINVAL;
		return snd_ctl_client_open(handlep, host, port, SND_TRANSPORT_TYPE_TCP, name);
	}
}
				
int snd_ctl_open(snd_ctl_t **handlep, char *name)
{
	char *str;
	int err;
	snd_config_t *ctl_conf, *conf;
	assert(handlep && name);
	err = snd_config_update();
	if (err < 0)
		return err;
	err = snd_config_searchv(snd_config, &ctl_conf, "ctl", name, 0);
	if (err < 0) {
		int idx = snd_card_get_index(name);
		if (idx < 0)
			return idx;
		return snd_ctl_hw_open(handlep, idx);
	}
	if (snd_config_type(ctl_conf) != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;
	err = snd_config_search(ctl_conf, "type", &conf);
	if (err < 0)
		return err;
	err = snd_config_string_get(conf, &str);
	if (err < 0)
		return err;
	if (strcmp(str, "hw") == 0)
		return _snd_ctl_open_hw(handlep, ctl_conf);
	else if (strcmp(str, "client") == 0)
		return _snd_ctl_open_client(handlep, ctl_conf);
	else
		return -EINVAL;
}
