/*
 *  Control - Automatically generated functions
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
  
#include "control_local.h"

size_t snd_ctl_element_id_sizeof()
{
	return sizeof(snd_ctl_element_id_t);
}

int snd_ctl_element_id_malloc(snd_ctl_element_id_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_element_id_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_ctl_element_id_free(snd_ctl_element_id_t *obj)
{
	free(obj);
}

void snd_ctl_element_id_copy(snd_ctl_element_id_t *dst, const snd_ctl_element_id_t *src)
{
	assert(dst && src);
	*dst = *src;
}

unsigned int snd_ctl_element_id_get_numid(const snd_ctl_element_id_t *obj)
{
	assert(obj);
	return obj->numid;
}

snd_ctl_element_iface_t snd_ctl_element_id_get_interface(const snd_ctl_element_id_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->iface);
}

unsigned int snd_ctl_element_id_get_device(const snd_ctl_element_id_t *obj)
{
	assert(obj);
	return obj->device;
}

unsigned int snd_ctl_element_id_get_subdevice(const snd_ctl_element_id_t *obj)
{
	assert(obj);
	return obj->subdevice;
}

const char *snd_ctl_element_id_get_name(const snd_ctl_element_id_t *obj)
{
	assert(obj);
	return obj->name;
}

unsigned int snd_ctl_element_id_get_index(const snd_ctl_element_id_t *obj)
{
	assert(obj);
	return obj->index;
}

void snd_ctl_element_id_set_numid(snd_ctl_element_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->numid = val;
}

void snd_ctl_element_id_set_interface(snd_ctl_element_id_t *obj, snd_ctl_element_iface_t val)
{
	assert(obj);
	obj->iface = snd_enum_to_int(val);
}

void snd_ctl_element_id_set_device(snd_ctl_element_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->device = val;
}

void snd_ctl_element_id_set_subdevice(snd_ctl_element_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->subdevice = val;
}

void snd_ctl_element_id_set_name(snd_ctl_element_id_t *obj, const char *val)
{
	assert(obj);
	strncpy(obj->name, val, sizeof(obj->name));
}

void snd_ctl_element_id_set_index(snd_ctl_element_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->index = val;
}

size_t snd_ctl_card_info_sizeof()
{
	return sizeof(snd_ctl_card_info_t);
}

int snd_ctl_card_info_malloc(snd_ctl_card_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_card_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_ctl_card_info_free(snd_ctl_card_info_t *obj)
{
	free(obj);
}

void snd_ctl_card_info_copy(snd_ctl_card_info_t *dst, const snd_ctl_card_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

int snd_ctl_card_info_get_card(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return obj->card;
}

snd_card_type_t snd_ctl_card_info_get_type(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->type);
}

const char *snd_ctl_card_info_get_id(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return obj->id;
}

const char *snd_ctl_card_info_get_abbreviation(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return obj->abbreviation;
}

const char *snd_ctl_card_info_get_name(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return obj->name;
}

const char *snd_ctl_card_info_get_longname(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return obj->longname;
}

const char *snd_ctl_card_info_get_mixerid(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return obj->mixerid;
}

const char *snd_ctl_card_info_get_mixername(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return obj->mixername;
}

size_t snd_ctl_event_sizeof()
{
	return sizeof(snd_ctl_event_t);
}

int snd_ctl_event_malloc(snd_ctl_event_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_event_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_ctl_event_free(snd_ctl_event_t *obj)
{
	free(obj);
}

void snd_ctl_event_copy(snd_ctl_event_t *dst, const snd_ctl_event_t *src)
{
	assert(dst && src);
	*dst = *src;
}

snd_ctl_event_type_t snd_ctl_event_get_type(const snd_ctl_event_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->type);
}

unsigned int snd_ctl_event_get_numid(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type != SNDRV_CTL_EVENT_REBUILD);
	return obj->data.id.numid;
}

void snd_ctl_event_get_id(const snd_ctl_event_t *obj, snd_ctl_element_id_t *ptr)
{
	assert(obj && ptr);
	assert(obj->type != SNDRV_CTL_EVENT_REBUILD);
	*ptr = obj->data.id;
}

snd_ctl_element_iface_t snd_ctl_event_get_interface(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type != SNDRV_CTL_EVENT_REBUILD);
	return snd_int_to_enum(obj->data.id.iface);
}

unsigned int snd_ctl_event_get_device(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type != SNDRV_CTL_EVENT_REBUILD);
	return obj->data.id.device;
}

unsigned int snd_ctl_event_get_subdevice(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type != SNDRV_CTL_EVENT_REBUILD);
	return obj->data.id.subdevice;
}

const char *snd_ctl_event_get_name(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type != SNDRV_CTL_EVENT_REBUILD);
	return obj->data.id.name;
}

unsigned int snd_ctl_event_get_index(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type != SNDRV_CTL_EVENT_REBUILD);
	return obj->data.id.index;
}

size_t snd_ctl_element_list_sizeof()
{
	return sizeof(snd_ctl_element_list_t);
}

int snd_ctl_element_list_malloc(snd_ctl_element_list_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_element_list_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_ctl_element_list_free(snd_ctl_element_list_t *obj)
{
	free(obj);
}

void snd_ctl_element_list_copy(snd_ctl_element_list_t *dst, const snd_ctl_element_list_t *src)
{
	assert(dst && src);
	*dst = *src;
}

void snd_ctl_element_list_set_offset(snd_ctl_element_list_t *obj, unsigned int val)
{
	assert(obj);
	obj->offset = val;
}

unsigned int snd_ctl_element_list_get_used(const snd_ctl_element_list_t *obj)
{
	assert(obj);
	return obj->used;
}

unsigned int snd_ctl_element_list_get_count(const snd_ctl_element_list_t *obj)
{
	assert(obj);
	return obj->count;
}

void snd_ctl_element_list_get_id(const snd_ctl_element_list_t *obj, unsigned int idx, snd_ctl_element_id_t *ptr)
{
	assert(obj && ptr);
	assert(idx < obj->used);
	*ptr = obj->pids[idx];
}

unsigned int snd_ctl_element_list_get_numid(const snd_ctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].numid;
}

snd_ctl_element_iface_t snd_ctl_element_list_get_interface(const snd_ctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return snd_int_to_enum(obj->pids[idx].iface);
}

unsigned int snd_ctl_element_list_get_device(const snd_ctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].device;
}

unsigned int snd_ctl_element_list_get_subdevice(const snd_ctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].subdevice;
}

const char *snd_ctl_element_list_get_name(const snd_ctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].name;
}

unsigned int snd_ctl_element_list_get_index(const snd_ctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].index;
}

size_t snd_ctl_element_info_sizeof()
{
	return sizeof(snd_ctl_element_info_t);
}

int snd_ctl_element_info_malloc(snd_ctl_element_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_element_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_ctl_element_info_free(snd_ctl_element_info_t *obj)
{
	free(obj);
}

void snd_ctl_element_info_copy(snd_ctl_element_info_t *dst, const snd_ctl_element_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

snd_ctl_element_type_t snd_ctl_element_info_get_type(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->type);
}

int snd_ctl_element_info_is_readable(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEMENT_ACCESS_READ);
}

int snd_ctl_element_info_is_writable(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEMENT_ACCESS_WRITE);
}

int snd_ctl_element_info_is_volatile(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEMENT_ACCESS_VOLATILE);
}

int snd_ctl_element_info_is_inactive(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEMENT_ACCESS_INACTIVE);
}

int snd_ctl_element_info_is_locked(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEMENT_ACCESS_LOCK);
}

int snd_ctl_element_info_is_indirect(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEMENT_ACCESS_INDIRECT);
}

unsigned int snd_ctl_element_info_get_count(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return obj->count;
}

long snd_ctl_element_info_get_min(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	assert(obj->type == SNDRV_CTL_ELEMENT_TYPE_INTEGER);
	return obj->value.integer.min;
}

long snd_ctl_element_info_get_max(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	assert(obj->type == SNDRV_CTL_ELEMENT_TYPE_INTEGER);
	return obj->value.integer.max;
}

long snd_ctl_element_info_get_step(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	assert(obj->type == SNDRV_CTL_ELEMENT_TYPE_INTEGER);
	return obj->value.integer.step;
}

unsigned int snd_ctl_element_info_get_items(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	assert(obj->type == SNDRV_CTL_ELEMENT_TYPE_ENUMERATED);
	return obj->value.enumerated.items;
}

void snd_ctl_element_info_set_item(snd_ctl_element_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->value.enumerated.item = val;
}

const char *snd_ctl_element_info_get_item_name(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	assert(obj->type == SNDRV_CTL_ELEMENT_TYPE_ENUMERATED);
	return obj->value.enumerated.name;
}

void snd_ctl_element_info_get_id(const snd_ctl_element_info_t *obj, snd_ctl_element_id_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->id;
}

unsigned int snd_ctl_element_info_get_numid(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return obj->id.numid;
}

snd_ctl_element_iface_t snd_ctl_element_info_get_interface(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->id.iface);
}

unsigned int snd_ctl_element_info_get_device(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return obj->id.device;
}

unsigned int snd_ctl_element_info_get_subdevice(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return obj->id.subdevice;
}

const char *snd_ctl_element_info_get_name(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return obj->id.name;
}

unsigned int snd_ctl_element_info_get_index(const snd_ctl_element_info_t *obj)
{
	assert(obj);
	return obj->id.index;
}

void snd_ctl_element_info_set_id(snd_ctl_element_info_t *obj, const snd_ctl_element_id_t *ptr)
{
	assert(obj && ptr);
	obj->id = *ptr;
}

void snd_ctl_element_info_set_numid(snd_ctl_element_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.numid = val;
}

void snd_ctl_element_info_set_interface(snd_ctl_element_info_t *obj, snd_ctl_element_iface_t val)
{
	assert(obj);
	obj->id.iface = snd_enum_to_int(val);
}

void snd_ctl_element_info_set_device(snd_ctl_element_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.device = val;
}

void snd_ctl_element_info_set_subdevice(snd_ctl_element_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.subdevice = val;
}

void snd_ctl_element_info_set_name(snd_ctl_element_info_t *obj, const char *val)
{
	assert(obj);
	strncpy(obj->id.name, val, sizeof(obj->id.name));
}

void snd_ctl_element_info_set_index(snd_ctl_element_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.index = val;
}

size_t snd_ctl_element_sizeof()
{
	return sizeof(snd_ctl_element_t);
}

int snd_ctl_element_malloc(snd_ctl_element_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_element_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_ctl_element_free(snd_ctl_element_t *obj)
{
	free(obj);
}

void snd_ctl_element_copy(snd_ctl_element_t *dst, const snd_ctl_element_t *src)
{
	assert(dst && src);
	*dst = *src;
}

void snd_ctl_element_get_id(const snd_ctl_element_t *obj, snd_ctl_element_id_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->id;
}

unsigned int snd_ctl_element_get_numid(const snd_ctl_element_t *obj)
{
	assert(obj);
	return obj->id.numid;
}

snd_ctl_element_iface_t snd_ctl_element_get_interface(const snd_ctl_element_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->id.iface);
}

unsigned int snd_ctl_element_get_device(const snd_ctl_element_t *obj)
{
	assert(obj);
	return obj->id.device;
}

unsigned int snd_ctl_element_get_subdevice(const snd_ctl_element_t *obj)
{
	assert(obj);
	return obj->id.subdevice;
}

const char *snd_ctl_element_get_name(const snd_ctl_element_t *obj)
{
	assert(obj);
	return obj->id.name;
}

unsigned int snd_ctl_element_get_index(const snd_ctl_element_t *obj)
{
	assert(obj);
	return obj->id.index;
}

void snd_ctl_element_set_id(snd_ctl_element_t *obj, const snd_ctl_element_id_t *ptr)
{
	assert(obj && ptr);
	obj->id = *ptr;
}

void snd_ctl_element_set_numid(snd_ctl_element_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.numid = val;
}

void snd_ctl_element_set_interface(snd_ctl_element_t *obj, snd_ctl_element_iface_t val)
{
	assert(obj);
	obj->id.iface = snd_enum_to_int(val);
}

void snd_ctl_element_set_device(snd_ctl_element_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.device = val;
}

void snd_ctl_element_set_subdevice(snd_ctl_element_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.subdevice = val;
}

void snd_ctl_element_set_name(snd_ctl_element_t *obj, const char *val)
{
	assert(obj);
	strncpy(obj->id.name, val, sizeof(obj->id.name));
}

void snd_ctl_element_set_index(snd_ctl_element_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.index = val;
}

long snd_ctl_element_get_boolean(const snd_ctl_element_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < sizeof(obj->value.integer.value) / sizeof(obj->value.integer.value[0]));
	return obj->value.integer.value[idx];
}

long snd_ctl_element_get_integer(const snd_ctl_element_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < sizeof(obj->value.integer.value) / sizeof(obj->value.integer.value[0]));
	return obj->value.integer.value[idx];
}

unsigned int snd_ctl_element_get_enumerated(const snd_ctl_element_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < sizeof(obj->value.enumerated.item) / sizeof(obj->value.enumerated.item[0]));
	return obj->value.enumerated.item[idx];
}

unsigned char snd_ctl_element_get_byte(const snd_ctl_element_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < sizeof(obj->value.bytes.data));
	return obj->value.bytes.data[idx];
}

void snd_ctl_element_set_boolean(snd_ctl_element_t *obj, unsigned int idx, long val)
{
	assert(obj);
	obj->value.integer.value[idx] = val;
}

void snd_ctl_element_set_integer(snd_ctl_element_t *obj, unsigned int idx, long val)
{
	assert(obj);
	obj->value.integer.value[idx] = val;
}

void snd_ctl_element_set_enumerated(snd_ctl_element_t *obj, unsigned int idx, unsigned int val)
{
	assert(obj);
	obj->value.enumerated.item[idx] = val;
}

void snd_ctl_element_set_byte(snd_ctl_element_t *obj, unsigned int idx, unsigned char val)
{
	assert(obj);
	obj->value.bytes.data[idx] = val;
}

const void * snd_ctl_element_get_bytes(const snd_ctl_element_t *obj)
{
	assert(obj);
	return obj->value.bytes.data;
}

void snd_ctl_element_get_iec958(const snd_ctl_element_t *obj, snd_aes_iec958_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->value.iec958;
}

void snd_ctl_element_set_iec958(snd_ctl_element_t *obj, const snd_aes_iec958_t *ptr)
{
	assert(obj && ptr);
	obj->value.iec958 = *ptr;
}

size_t snd_hctl_element_list_sizeof()
{
	return sizeof(snd_hctl_element_list_t);
}

int snd_hctl_element_list_malloc(snd_hctl_element_list_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_hctl_element_list_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_hctl_element_list_free(snd_hctl_element_list_t *obj)
{
	free(obj);
}

void snd_hctl_element_list_copy(snd_hctl_element_list_t *dst, const snd_hctl_element_list_t *src)
{
	assert(dst && src);
	*dst = *src;
}

void snd_hctl_element_list_set_offset(snd_hctl_element_list_t *obj, unsigned int val)
{
	assert(obj);
	obj->offset = val;
}

unsigned int snd_hctl_element_list_get_used(const snd_hctl_element_list_t *obj)
{
	assert(obj);
	return obj->used;
}

unsigned int snd_hctl_element_list_get_count(const snd_hctl_element_list_t *obj)
{
	assert(obj);
	return obj->count;
}

void snd_hctl_element_list_get_id(const snd_hctl_element_list_t *obj, unsigned int idx, snd_ctl_element_id_t *ptr)
{
	assert(obj && ptr);
	assert(idx < obj->used);
	*ptr = obj->pids[idx];
}

unsigned int snd_hctl_element_list_get_numid(const snd_hctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].numid;
}

snd_ctl_element_iface_t snd_hctl_element_list_get_interface(const snd_hctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return snd_int_to_enum(obj->pids[idx].iface);
}

unsigned int snd_hctl_element_list_get_device(const snd_hctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].device;
}

unsigned int snd_hctl_element_list_get_subdevice(const snd_hctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].subdevice;
}

const char *snd_hctl_element_list_get_name(const snd_hctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].name;
}

unsigned int snd_hctl_element_list_get_index(const snd_hctl_element_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].index;
}

size_t snd_hctl_element_sizeof()
{
	return sizeof(snd_hctl_element_t);
}

int snd_hctl_element_malloc(snd_hctl_element_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_hctl_element_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_hctl_element_free(snd_hctl_element_t *obj)
{
	free(obj);
}

void snd_hctl_element_copy(snd_hctl_element_t *dst, const snd_hctl_element_t *src)
{
	assert(dst && src);
	*dst = *src;
}

void snd_hctl_element_get_id(const snd_hctl_element_t *obj, snd_ctl_element_id_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->id;
}

unsigned int snd_hctl_element_get_numid(const snd_hctl_element_t *obj)
{
	assert(obj);
	return obj->id.numid;
}

snd_ctl_element_iface_t snd_hctl_element_get_interface(const snd_hctl_element_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->id.iface);
}

unsigned int snd_hctl_element_get_device(const snd_hctl_element_t *obj)
{
	assert(obj);
	return obj->id.device;
}

unsigned int snd_hctl_element_get_subdevice(const snd_hctl_element_t *obj)
{
	assert(obj);
	return obj->id.subdevice;
}

const char *snd_hctl_element_get_name(const snd_hctl_element_t *obj)
{
	assert(obj);
	return obj->id.name;
}

unsigned int snd_hctl_element_get_index(const snd_hctl_element_t *obj)
{
	assert(obj);
	return obj->id.index;
}

void snd_hctl_element_set_callback_change(snd_hctl_element_t *obj, snd_hctl_element_callback_t val)
{
	assert(obj);
	obj->callback_change = val;
}

void snd_hctl_element_set_callback_value(snd_hctl_element_t *obj, snd_hctl_element_callback_t val)
{
	assert(obj);
	obj->callback_value = val;
}

void snd_hctl_element_set_callback_remove(snd_hctl_element_t *obj, snd_hctl_element_callback_t val)
{
	assert(obj);
	obj->callback_remove = val;
}

void * snd_hctl_element_get_private_data(const snd_hctl_element_t *obj)
{
	assert(obj);
	return obj->private_data;
}

void snd_hctl_element_set_private_data(snd_hctl_element_t *obj, void * val)
{
	assert(obj);
	obj->private_data = val;
}

void snd_hctl_element_set_private_free(snd_hctl_element_t *obj, snd_hctl_element_private_free_t val)
{
	assert(obj);
	obj->private_free = val;
}

