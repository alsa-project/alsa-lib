/*
 *  Mixer Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@suse.cz>
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
#include <sys/ioctl.h>
#include "asoundlib.h"

#define __USE_GNU
#include <search.h>

#define SND_FILE_MIXER		"/dev/snd/mixerC%iD%i"
#define SND_MIXER_VERSION_MAX	SND_PROTOCOL_VERSION(2, 0, 0)

struct snd_mixer {
	int card;
	int device;
	int fd;
} ;

int snd_mixer_open(snd_mixer_t **handle, int card, int device)
{
	int fd, ver;
	char filename[32];
	snd_mixer_t *mixer;

	*handle = NULL;

	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	sprintf(filename, SND_FILE_MIXER, card, device);
	if ((fd = open(filename, O_RDWR)) < 0) {
		snd_card_load(card);	
		if ((fd = open(filename, O_RDWR)) < 0) 
			return -errno;
	}
	if (ioctl(fd, SND_MIXER_IOCTL_PVERSION, &ver) < 0) {
		close(fd);
		return -errno;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(ver, SND_MIXER_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	mixer = (snd_mixer_t *) calloc(1, sizeof(snd_mixer_t));
	if (mixer == NULL) {
		close(fd);
		return -ENOMEM;
	}
	mixer->card = card;
	mixer->device = device;
	mixer->fd = fd;
	*handle = mixer;
	return 0;
}

int snd_mixer_close(snd_mixer_t *handle)
{
	snd_mixer_t *mixer;
	int res;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	res = close(mixer->fd) < 0 ? -errno : 0;
	free(mixer);
	return res;
}

int snd_mixer_file_descriptor(snd_mixer_t *handle)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	return mixer->fd;
}

int snd_mixer_info(snd_mixer_t *handle, snd_mixer_info_t * info)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !info)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_mixer_elements(snd_mixer_t *handle, snd_mixer_elements_t * elements)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !elements)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ELEMENTS, elements) < 0)
		return -errno;
	return 0;
}

int snd_mixer_routes(snd_mixer_t *handle, snd_mixer_routes_t * routes)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !routes)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ROUTES, routes) < 0)
		return -errno;
	return 0;
}

int snd_mixer_groups(snd_mixer_t *handle, snd_mixer_groups_t * groups)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !groups)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_GROUPS, groups) < 0)
		return -errno;
	return 0;
}

int snd_mixer_group_read(snd_mixer_t *handle, snd_mixer_group_t * group)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !group)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_GROUP_READ, group) < 0)
		return -errno;
	return 0;
}

int snd_mixer_group_write(snd_mixer_t *handle, snd_mixer_group_t * group)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !group)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_GROUP_WRITE, group) < 0)
		return -errno;
	return 0;
}

int snd_mixer_element_info(snd_mixer_t *handle, snd_mixer_element_info_t * info)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !info)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ELEMENT_INFO, info) < 0)
		return -errno;
	return 0;
}

int snd_mixer_element_read(snd_mixer_t *handle, snd_mixer_element_t * element)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !element)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ELEMENT_READ, element) < 0)
		return -errno;
	return 0;
}

int snd_mixer_element_write(snd_mixer_t *handle, snd_mixer_element_t * element)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !element)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_ELEMENT_WRITE, element) < 0)
		return -errno;
	return 0;
}

int snd_mixer_get_filter(snd_mixer_t *handle, snd_mixer_filter_t * filter)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !filter)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_GET_FILTER, filter) < 0)
		return -errno;
	return 0;
}

int snd_mixer_put_filter(snd_mixer_t *handle, snd_mixer_filter_t * filter)
{
	snd_mixer_t *mixer;

	mixer = handle;
	if (!mixer || !filter)
		return -EINVAL;
	if (ioctl(mixer->fd, SND_MIXER_IOCTL_PUT_FILTER, filter) < 0)
		return -errno;
	return 0;
}

int snd_mixer_read(snd_mixer_t *handle, snd_mixer_callbacks_t * callbacks)
{
	snd_mixer_t *mixer;
	int result, count;
	snd_mixer_read_t r;

	mixer = handle;
	if (!mixer)
		return -EINVAL;
	count = 0;
	while ((result = read(mixer->fd, &r, sizeof(r))) > 0) {
		if (result != sizeof(r))
			return -EIO;
		if (!callbacks)
			continue;
		switch (r.cmd) {
		case SND_MIXER_READ_REBUILD:
			if (callbacks->rebuild)
				callbacks->rebuild(callbacks->private_data);
			break;
		case SND_MIXER_READ_ELEMENT_VALUE:
		case SND_MIXER_READ_ELEMENT_CHANGE:
		case SND_MIXER_READ_ELEMENT_ROUTE:
		case SND_MIXER_READ_ELEMENT_ADD:
		case SND_MIXER_READ_ELEMENT_REMOVE:
			if (callbacks->element)
				callbacks->element(callbacks->private_data, r.cmd, &r.data.eid);
			break;
		case SND_MIXER_READ_GROUP_CHANGE:
		case SND_MIXER_READ_GROUP_ADD:
		case SND_MIXER_READ_GROUP_REMOVE:
			if (callbacks->group)
				callbacks->group(callbacks->private_data, r.cmd, &r.data.gid);
			break;
		}
		count++;
	}
	return result >= 0 ? count : -errno;
}

void snd_mixer_set_bit(unsigned int *bitmap, int bit, int val)
{
	if (val) {
		bitmap[bit >> 5] |= 1 << (bit & 31);
	} else {
		bitmap[bit >> 5] &= ~(1 << (bit & 31));
	}
}

int snd_mixer_get_bit(unsigned int *bitmap, int bit)
{
	return (bitmap[bit >> 5] & (1 << (bit & 31))) ? 1 : 0;
}

const char *snd_mixer_channel_name(int channel)
{
	static char *array[6] = {
		"Front-Left",
		"Front-Right",
		"Front-Center",
		"Rear-Left",
		"Rear-Right",
		"Woofer"
	};

	if (channel < 0 || channel > 5)
		return "?";
	return array[channel];
}

typedef int (*snd_mixer_compare_gid_func_t)(const snd_mixer_gid_t *a, const snd_mixer_gid_t *b, void* private_data);

void snd_mixer_sort_gid_ptr(snd_mixer_gid_t **list, int count,
			    void* private_data,
			    snd_mixer_compare_gid_func_t compare)
{
	int _compare(const void* a, const void* b) {
		snd_mixer_gid_t * const *_a = a;
		snd_mixer_gid_t * const *_b = b;
		return compare(*_a, *_b, private_data);
	}
	qsort(list, count, sizeof(snd_mixer_gid_t *), _compare);
}	

void snd_mixer_sort_gid(snd_mixer_gid_t *list, int count,
			void* private_data,
			snd_mixer_compare_gid_func_t compare)
{
	snd_mixer_gid_t *list1 = malloc(sizeof(snd_mixer_gid_t) * count);
	snd_mixer_gid_t **ptrs = malloc(sizeof(snd_mixer_gid_t *) * count);
	int k;
	memcpy(list1, list, count * sizeof(snd_mixer_gid_t));
	for (k = 0; k < count; ++k)
		ptrs[k] = list1 + k;
	snd_mixer_sort_gid_ptr(ptrs, count, private_data, compare);
	for (k = 0; k < count; ++k)
		memcpy(list + k, ptrs[k], sizeof(snd_mixer_gid_t));
	free(list1);
	free(ptrs);
}

/* Compare using name and index */
int snd_mixer_compare_gid_name_index(const snd_mixer_gid_t *a,
				     const snd_mixer_gid_t *b,
				     void *ignored UNUSED)
{
	int r = strcmp(a->name, b->name);
	if (r != 0)
		return r;
	return a->index - b->index;
}

/* Compare using a table mapping name to weight */
int snd_mixer_compare_gid_table(const snd_mixer_gid_t *a,
				const snd_mixer_gid_t *b,
				void* private_data)
{
	struct hsearch_data *htab = private_data;
	ENTRY ea, eb;
	ENTRY *ra, *rb;
	int aw = 0, bw = 0;
	int r;
	ea.key = (char *) a->name;
	if (hsearch_r(ea, FIND, &ra, htab))
		aw = *(int *)ra->data;
	eb.key = (char *) b->name;
	if (hsearch_r(eb, FIND, &rb, htab))
		bw = *(int *)rb->data;
	r = aw - bw;
	if (r != 0)
		return r;
	r = strcmp(a->name, b->name);
	if (r != 0)
		return r;
	return a->index - b->index;
}


void snd_mixer_sort_gid_name_index(snd_mixer_gid_t *list, int count)
{
	snd_mixer_sort_gid(list, count, NULL, snd_mixer_compare_gid_name_index);
}

void snd_mixer_sort_gid_table(snd_mixer_gid_t *list, int count, snd_mixer_weight_entry_t *table)
{
	struct hsearch_data htab;
	int k;
	htab.table = NULL;
	for (k = 0; table[k].name; ++k);
	hcreate_r(k*2, &htab);
	for (k = 0; table[k].name; ++k) {
		ENTRY e;
		ENTRY *r;
		e.key = table[k].name;
		e.data = (char *) &table[k].weight;
		hsearch_r(e, ENTER, &r, &htab);
	}
	snd_mixer_sort_gid(list, count, &htab, snd_mixer_compare_gid_table);
	hdestroy_r(&htab);
}

typedef int (*snd_mixer_compare_eid_func_t)(const snd_mixer_eid_t *a, const snd_mixer_eid_t *b, void* private_data);

void snd_mixer_sort_eid_ptr(snd_mixer_eid_t **list, int count,
			    void* private_data,
			    snd_mixer_compare_eid_func_t compare)
{
	int _compare(const void* a, const void* b) {
		snd_mixer_eid_t * const *_a = a;
		snd_mixer_eid_t * const *_b = b;
		return compare(*_a, *_b, private_data);
	}
	qsort(list, count, sizeof(snd_mixer_eid_t *), _compare);
}	

void snd_mixer_sort_eid(snd_mixer_eid_t *list, int count,
			void* private_data,
			snd_mixer_compare_eid_func_t compare)
{
	snd_mixer_eid_t *list1 = malloc(sizeof(snd_mixer_eid_t) * count);
	snd_mixer_eid_t **ptrs = malloc(sizeof(snd_mixer_eid_t *) * count);
	int k;
	memcpy(list1, list, count * sizeof(snd_mixer_eid_t));
	for (k = 0; k < count; ++k)
		ptrs[k] = list1 + k;
	snd_mixer_sort_eid_ptr(ptrs, count, private_data, compare);
	for (k = 0; k < count; ++k)
		memcpy(list + k, ptrs[k], sizeof(snd_mixer_eid_t));
	free(list1);
	free(ptrs);
}

/* Compare using name and index */
int snd_mixer_compare_eid_name_index(const snd_mixer_eid_t *a,
				     const snd_mixer_eid_t *b,
				     void *ignored UNUSED)
{
	int r = strcmp(a->name, b->name);
	if (r != 0)
		return r;
	return a->index - b->index;
}

/* Compare using a table mapping name to weight */
int snd_mixer_compare_eid_table(const snd_mixer_eid_t *a,
				const snd_mixer_eid_t *b,
				void* private_data)
{
	struct hsearch_data *htab = private_data;
	ENTRY ea, eb;
	ENTRY *ra, *rb;
	int aw = 0, bw = 0;
	int r;
	ea.key = (char *) a->name;
	if (hsearch_r(ea, FIND, &ra, htab))
		aw = *(int *)ra->data;
	eb.key = (char *) b->name;
	if (hsearch_r(eb, FIND, &rb, htab))
		bw = *(int *)rb->data;
	r = aw - bw;
	if (r != 0)
		return r;
	r = strcmp(a->name, b->name);
	if (r != 0)
		return r;
	return a->index - b->index;
}


void snd_mixer_sort_eid_name_index(snd_mixer_eid_t *list, int count)
{
	snd_mixer_sort_eid(list, count, NULL, snd_mixer_compare_eid_name_index);
}

void snd_mixer_sort_eid_table(snd_mixer_eid_t *list, int count, snd_mixer_weight_entry_t *table            )
{
	struct hsearch_data htab;
	int k;
	htab.table = NULL;
	for (k = 0; table[k].name; ++k);
	hcreate_r(k*2, &htab);
	for (k = 0; table[k].name; ++k) {
		ENTRY e;
		ENTRY *r;
		e.key = table[k].name;
		e.data = (char *) &table[k].weight;
		hsearch_r(e, ENTER, &r, &htab);
	}
	snd_mixer_sort_eid(list, count, &htab, snd_mixer_compare_eid_table);
	hdestroy_r(&htab);
}


static snd_mixer_weight_entry_t _snd_mixer_default_weights[] = {
	{ SND_MIXER_OUT_MASTER,		-1360 },
	{ SND_MIXER_OUT_MASTER_DIGITAL,	-1350 },
	{ SND_MIXER_OUT_MASTER_MONO,	-1340 },
	{ SND_MIXER_OUT_HEADPHONE,	-1330 },
	{ SND_MIXER_OUT_PHONE,		-1320 },
	{ SND_MIXER_GRP_EFFECT_3D,	-1310 },
	{ SND_MIXER_GRP_BASS,		-1300 },
	{ SND_MIXER_GRP_TREBLE,		-1290 },
	{ SND_MIXER_GRP_EQUALIZER,	-1280 },
	{ SND_MIXER_GRP_FADER,		-1270 },
	{ SND_MIXER_OUT_CENTER,		-1260 },
	{ SND_MIXER_IN_CENTER,		-1250 },
	{ SND_MIXER_OUT_WOOFER,		-1240 },
	{ SND_MIXER_IN_WOOFER,		-1230 },
	{ SND_MIXER_OUT_SURROUND,	-1220 },
	{ SND_MIXER_IN_SURROUND,	-1210 },
	{ SND_MIXER_IN_SYNTHESIZER,	-1200 },
	{ SND_MIXER_IN_FM,		-1190 },
	{ SND_MIXER_GRP_EFFECT,		-1180 },
	{ SND_MIXER_OUT_DSP,		-1170 },
	{ SND_MIXER_IN_DSP,		-1160 },
	{ SND_MIXER_IN_PCM,		-1150 },
	{ SND_MIXER_IN_DAC,		-1140 },
	{ SND_MIXER_IN_LINE,		-1130 },
	{ SND_MIXER_IN_MIC,		-1120 },
	{ SND_MIXER_IN_CD,		-1110 },
	{ SND_MIXER_IN_VIDEO,		-1100 },
	{ SND_MIXER_IN_RADIO,		-1090 },
	{ SND_MIXER_IN_PHONE,		-1080 },
	{ SND_MIXER_GRP_MIC_GAIN,	-1070 },
	{ SND_MIXER_GRP_OGAIN,		-1060 },
	{ SND_MIXER_GRP_IGAIN,		-1050 },
	{ SND_MIXER_GRP_ANALOG_LOOPBACK,-1040 },
	{ SND_MIXER_GRP_DIGITAL_LOOPBACK,-1030 },
	{ SND_MIXER_IN_SPEAKER,		-1020 },
	{ SND_MIXER_IN_MONO,		-1010 },
	{ SND_MIXER_IN_AUX,		-1000 },
	{ NULL, 0 }
};

snd_mixer_weight_entry_t *snd_mixer_default_weights = _snd_mixer_default_weights;
