/*
 *  Rawmidi - Automatically generated functions
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
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
  
#include "rawmidi_local.h"

size_t snd_rawmidi_params_sizeof()
{
	return sizeof(snd_rawmidi_params_t);
}

int snd_rawmidi_params_malloc(snd_rawmidi_params_t **ptr)
{
	assert(ptr);
	*ptr = malloc(sizeof(snd_rawmidi_params_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_rawmidi_params_free(snd_rawmidi_params_t *obj)
{
	free(obj);
}

void snd_rawmidi_params_copy(snd_rawmidi_params_t *dst, const snd_rawmidi_params_t *src)
{
	assert(dst && src);
	*dst = *src;
}

int snd_rawmidi_params_set_buffer_size(snd_rawmidi_t *rmidi ATTRIBUTE_UNUSED, snd_rawmidi_params_t *params, size_t val)
{
	assert(rmidi && params);
	assert(val > params->avail_min);
	params->buffer_size = val;
	return 0;
}

size_t snd_rawmidi_params_get_buffer_size(const snd_rawmidi_params_t *params)
{
	assert(params);
	return params->buffer_size;
}


int snd_rawmidi_params_set_avail_min(snd_rawmidi_t *rmidi ATTRIBUTE_UNUSED, snd_rawmidi_params_t *params, size_t val)
{
	assert(rmidi && params);
	assert(val < params->buffer_size);
	params->avail_min = val;
	return 0;
}

size_t snd_rawmidi_params_get_avail_min(const snd_rawmidi_params_t *params)
{
	assert(params);
	return params->avail_min;
}


int snd_rawmidi_params_set_no_active_sensing(snd_rawmidi_t *rmidi ATTRIBUTE_UNUSED, snd_rawmidi_params_t *params, int val)
{
	assert(rmidi && params);
	params->no_active_sensing = val;
	return 0;
}

int snd_rawmidi_params_get_no_active_sensing(const snd_rawmidi_params_t *params)
{
	assert(params);
	return params->no_active_sensing;
}


size_t snd_rawmidi_info_sizeof()
{
	return sizeof(snd_rawmidi_info_t);
}

int snd_rawmidi_info_malloc(snd_rawmidi_info_t **ptr)
{
	assert(ptr);
	*ptr = malloc(sizeof(snd_rawmidi_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_rawmidi_info_free(snd_rawmidi_info_t *obj)
{
	free(obj);
}

void snd_rawmidi_info_copy(snd_rawmidi_info_t *dst, const snd_rawmidi_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

unsigned int snd_rawmidi_info_get_device(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->device;
}

unsigned int snd_rawmidi_info_get_subdevice(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->subdevice;
}

snd_rawmidi_stream_t snd_rawmidi_info_get_stream(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->stream);
}

int snd_rawmidi_info_get_card(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->card;
}

unsigned int snd_rawmidi_info_get_flags(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->flags;
}

const char * snd_rawmidi_info_get_id(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->id;
}

const char * snd_rawmidi_info_get_name(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->name;
}

const char * snd_rawmidi_info_get_subdevice_name(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->subname;
}

unsigned int snd_rawmidi_info_get_subdevices_count(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->subdevices_count;
}

unsigned int snd_rawmidi_info_get_subdevices_avail(const snd_rawmidi_info_t *obj)
{
	assert(obj);
	return obj->subdevices_avail;
}

void snd_rawmidi_info_set_device(snd_rawmidi_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->device = val;
}

void snd_rawmidi_info_set_subdevice(snd_rawmidi_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->subdevice = val;
}

void snd_rawmidi_info_set_stream(snd_rawmidi_info_t *obj, snd_rawmidi_stream_t val)
{
	assert(obj);
	obj->stream = snd_enum_to_int(val);
}

size_t snd_rawmidi_status_sizeof()
{
	return sizeof(snd_rawmidi_status_t);
}

int snd_rawmidi_status_malloc(snd_rawmidi_status_t **ptr)
{
	assert(ptr);
	*ptr = malloc(sizeof(snd_rawmidi_status_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_rawmidi_status_free(snd_rawmidi_status_t *obj)
{
	free(obj);
}

void snd_rawmidi_status_copy(snd_rawmidi_status_t *dst, const snd_rawmidi_status_t *src)
{
	assert(dst && src);
	*dst = *src;
}

void snd_rawmidi_status_get_tstamp(const snd_rawmidi_status_t *obj, snd_timestamp_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->tstamp;
}

size_t snd_rawmidi_status_get_avail(const snd_rawmidi_status_t *obj)
{
	assert(obj);
	return obj->avail;
}

size_t snd_rawmidi_status_get_avail_max(const snd_rawmidi_status_t *obj)
{
	assert(obj);
	return obj->xruns;
}

