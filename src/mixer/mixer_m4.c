/*
 *  Mixer - Automatically generated functions
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
  
#include "mixer_local.h"

size_t snd_mixer_selem_id_sizeof()
{
	return sizeof(snd_mixer_selem_id_t);
}

int snd_mixer_selem_id_malloc(snd_mixer_selem_id_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_mixer_selem_id_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_mixer_selem_id_free(snd_mixer_selem_id_t *obj)
{
	free(obj);
}

void snd_mixer_selem_id_copy(snd_mixer_selem_id_t *dst, const snd_mixer_selem_id_t *src)
{
	assert(dst && src);
	*dst = *src;
}

const char *snd_mixer_selem_id_get_name(const snd_mixer_selem_id_t *obj)
{
	assert(obj);
	return obj->name;
}

unsigned int snd_mixer_selem_id_get_index(const snd_mixer_selem_id_t *obj)
{
	assert(obj);
	return obj->index;
}

void snd_mixer_selem_id_set_name(snd_mixer_selem_id_t *obj, const char *val)
{
	assert(obj);
	strncpy(obj->name, val, sizeof(obj->name));
}

void snd_mixer_selem_id_set_index(snd_mixer_selem_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->index = val;
}

void snd_mixer_set_callback(snd_mixer_t *obj, snd_mixer_callback_t val)
{
	assert(obj);
	obj->callback = val;
}

void * snd_mixer_get_callback_private(const snd_mixer_t *obj)
{
	assert(obj);
	return obj->callback_private;
}

void snd_mixer_set_callback_private(snd_mixer_t *obj, void * val)
{
	assert(obj);
	obj->callback_private = val;
}

unsigned int snd_mixer_get_count(const snd_mixer_t *obj)
{
	assert(obj);
	return obj->count;
}

void snd_mixer_elem_set_callback(snd_mixer_elem_t *obj, snd_mixer_elem_callback_t val)
{
	assert(obj);
	obj->callback = val;
}

void * snd_mixer_elem_get_callback_private(const snd_mixer_elem_t *obj)
{
	assert(obj);
	return obj->callback_private;
}

void snd_mixer_elem_set_callback_private(snd_mixer_elem_t *obj, void * val)
{
	assert(obj);
	obj->callback_private = val;
}

snd_mixer_elem_type_t snd_mixer_elem_get_type(const snd_mixer_elem_t *obj)
{
	assert(obj);
	return obj->type;
}

size_t snd_mixer_selem_info_sizeof()
{
	return sizeof(snd_mixer_selem_info_t);
}

int snd_mixer_selem_info_malloc(snd_mixer_selem_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_mixer_selem_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_mixer_selem_info_free(snd_mixer_selem_info_t *obj)
{
	free(obj);
}

void snd_mixer_selem_info_copy(snd_mixer_selem_info_t *dst, const snd_mixer_selem_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

long snd_mixer_selem_info_get_min(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return obj->min;
}

long snd_mixer_selem_info_get_max(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return obj->max;
}

int snd_mixer_selem_info_get_capture_group(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return obj->capture_group;
}

int snd_mixer_selem_info_has_volume(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return !!(obj->caps & CAP_VOLUME);
}

int snd_mixer_selem_info_has_joined_volume(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return !!(obj->caps & CAP_JOIN_VOLUME);
}

int snd_mixer_selem_info_has_mute(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return !!(obj->caps & CAP_MUTE);
}

int snd_mixer_selem_info_has_joined_mute(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return !!(obj->caps & CAP_JOIN_MUTE);
}

int snd_mixer_selem_info_has_capture(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return !!(obj->caps & CAP_CAPTURE);
}

int snd_mixer_selem_info_has_joined_capture(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return !!(obj->caps & CAP_JOIN_CAPTURE);
}

int snd_mixer_selem_info_has_exclusive_capture(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return !!(obj->caps & CAP_EXCL_CAPTURE);
}

size_t snd_mixer_selem_value_sizeof()
{
	return sizeof(snd_mixer_selem_value_t);
}

int snd_mixer_selem_value_malloc(snd_mixer_selem_value_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_mixer_selem_value_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_mixer_selem_value_free(snd_mixer_selem_value_t *obj)
{
	free(obj);
}

void snd_mixer_selem_value_copy(snd_mixer_selem_value_t *dst, const snd_mixer_selem_value_t *src)
{
	assert(dst && src);
	*dst = *src;
}

