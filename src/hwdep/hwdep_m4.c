/*
 *  Hwdep - Automatically generated functions
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
  
#include "local.h"

size_t snd_hwdep_info_sizeof()
{
	return sizeof(snd_hwdep_info_t);
}

int snd_hwdep_info_malloc(snd_hwdep_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_hwdep_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_hwdep_info_free(snd_hwdep_info_t *obj)
{
	free(obj);
}

void snd_hwdep_info_copy(snd_hwdep_info_t *dst, const snd_hwdep_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

unsigned int snd_hwdep_info_get_device(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return obj->device;
}

int snd_hwdep_info_get_card(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return obj->card;
}

const char *snd_hwdep_info_get_id(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return obj->id;
}

const char *snd_hwdep_info_get_name(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return obj->name;
}

snd_hwdep_type_t snd_hwdep_info_get_type(const snd_hwdep_info_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->type);
}

void snd_hwdep_info_set_device(snd_hwdep_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->device = val;
}

