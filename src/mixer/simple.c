/*
 *  Mixer Interface - simple controls
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "mixer_local.h"

typedef struct _mixer_simple mixer_simple_t;

typedef int (mixer_simple_read_t)(snd_mixer_elem_t *elem,
				  snd_mixer_selem_t *value);
typedef int (mixer_simple_write_t)(snd_mixer_elem_t *elem,
				   const snd_mixer_selem_t *control);

#define MIXER_PRESENT_SINGLE_SWITCH	(1<<0)
#define MIXER_PRESENT_SINGLE_VOLUME	(1<<1)
#define MIXER_PRESENT_GLOBAL_SWITCH	(1<<2)
#define MIXER_PRESENT_GLOBAL_VOLUME	(1<<3)
#define MIXER_PRESENT_GLOBAL_ROUTE	(1<<4)
#define MIXER_PRESENT_PLAYBACK_SWITCH	(1<<5)
#define MIXER_PRESENT_PLAYBACK_VOLUME	(1<<6)
#define MIXER_PRESENT_PLAYBACK_ROUTE	(1<<7)
#define MIXER_PRESENT_CAPTURE_SWITCH	(1<<8)
#define MIXER_PRESENT_CAPTURE_VOLUME	(1<<9)
#define MIXER_PRESENT_CAPTURE_ROUTE	(1<<10)
#define MIXER_PRESENT_CAPTURE_SOURCE	(1<<11)

typedef struct _selem {
	snd_mixer_selem_id_t id;
	unsigned int present;		/* present controls */
	unsigned int global_values;
	unsigned int gswitch_values;
	unsigned int pswitch_values;
	unsigned int cswitch_values;
	unsigned int gvolume_values;
	unsigned int pvolume_values;
	unsigned int cvolume_values;
	unsigned int groute_values;
	unsigned int proute_values;
	unsigned int croute_values;
	unsigned int ccapture_values;
	unsigned int capture_item;
	unsigned int caps;
	long min;
	long max;
	int voices;
	/* -- */
	mixer_simple_read_t *read;
	mixer_simple_write_t *write;
	snd_hctl_bag_t elems;		/* bag of associated elems */
	unsigned long private_value;
} selem_t;

static struct mixer_name_table {
	const char *longname;
	const char *shortname;
} name_table[] = {
	{"Tone Control - Bass", "Bass"},
	{"Tone Control - Treble", "Treble"},
	{"Synth Tone Control - Bass", "Synth Bass"},
	{"Synth Tone Control - Treble", "Synth Treble"},
	{0, 0},
};

const char *snd_mixer_channel_name(snd_mixer_channel_id_t channel)
{
	static char *array[snd_enum_to_int(SND_MIXER_CHN_LAST) + 1] = {
		[SND_MIXER_CHN_FRONT_LEFT] = "Front Left",
		[SND_MIXER_CHN_FRONT_RIGHT] = "Front Right",
		[SND_MIXER_CHN_FRONT_CENTER] = "Front Center",
		[SND_MIXER_CHN_REAR_LEFT] = "Rear Left",
		[SND_MIXER_CHN_REAR_RIGHT] = "Rear Right",
		[SND_MIXER_CHN_WOOFER] = "Woofer"
	};
	char *p;
	assert(channel <= SND_MIXER_CHN_LAST);
	p = array[snd_enum_to_int(channel)];
	if (!p)
		return "?";
	return p;
}

static snd_hctl_elem_t *test_mixer_id(snd_mixer_t *mixer, const char *name, int index)
{
	snd_ctl_elem_id_t id;
	snd_hctl_elem_t *helem;
	
	memset(&id, 0, sizeof(id));
	id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strcpy(id.name, name);
	id.index = index;
	helem = snd_hctl_find_elem(mixer->ctl, &id);
	// fprintf(stderr, "Looking for control: '%s', %i (0x%lx)\n", name, index, (long)helem);
	return helem;
}

static int get_mixer_info(snd_mixer_t *mixer, const char *name, int index, snd_ctl_elem_info_t *info)
{
	memset(info, 0, sizeof(*info));
	info->id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strcpy(info->id.name, name);
	info->id.index = index;
	return snd_ctl_elem_info(mixer->ctl, info);
}

static int get_mixer_read(snd_mixer_t *mixer, const char *name, int index, snd_ctl_elem_t *control)
{
	memset(control, 0, sizeof(*control));
	control->id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strcpy(control->id.name, name);
	control->id.index = index;
	return snd_ctl_elem_read(mixer->ctl, control);
}

static int put_mixer_write(snd_mixer_t *mixer, const char *name, int index, snd_ctl_elem_t *control)
{
	control->id.numid = 0;
	control->id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strcpy(control->id.name, name);
	control->id.device = control->id.subdevice = 0;
	control->id.index = index;
	control->indirect = 0;
	memset(&control->reserved, 0, sizeof(control->reserved));
	return snd_ctl_elem_write(mixer->ctl, control);
}

static int hctl_elem_event(snd_hctl_elem_t *helem,
			      snd_ctl_event_type_t type)
{
	switch (type) {
	case SND_CTL_EVENT_CHANGE:
	case SND_CTL_EVENT_REMOVE:
		/* ignore at this moment */
		break;
	case SND_CTL_EVENT_VALUE:
	{
		snd_hctl_bag_t *bag = snd_hctl_elem_get_callback_private(helem);
		snd_mixer_elem_t *e = bag->private;
		if (e->callback) {
			int res = e->callback(e, type);
			if (res < 0)
				return res;
		}
		break;
	}
	default:
		assert(0);
		break;
	}
	return 0;
}

static void hctl_elem_add(selem_t *s, snd_hctl_elem_t *helem)
{
	int err;
	err = snd_hctl_bag_add(&s->elems, helem);
	assert(err >= 0);
	snd_hctl_elem_set_callback(helem, hctl_elem_event);
	snd_hctl_elem_set_callback_private(helem, &s->elems);
}

static const char *get_full_name(const char *sname)
{
	struct mixer_name_table *p;
	for (p = name_table; p->longname; p++) {
		if (!strcmp(sname, p->shortname))
			return p->longname;
	}
	return sname;
}

static const char *get_short_name(const char *lname)
{
	struct mixer_name_table *p;
	for (p = name_table; p->longname; p++) {
		if (!strcmp(lname, p->longname))
			return p->shortname;
	}
	return lname;
}

static int elem_read_volume(snd_mixer_t *mixer, selem_t *simple, snd_mixer_selem_t *control, const char *direction, const char *postfix, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s%s%s", get_full_name(simple->id.name), direction, postfix);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		control->volume[idx] = ctl.value.integer.value[voices == 1 ? 0 : idx];
	return 0;
}

static int elem_read_mute_switch(snd_mixer_t *mixer, selem_t *simple, snd_mixer_selem_t *control, const char *direction, const char *postfix, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s%s%s", get_full_name(simple->id.name), direction, postfix);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[voices == 1 ? 0 : idx] == 0)
			control->mute |= 1 << idx;
	return 0;
}

static int elem_read_mute_route(snd_mixer_t *mixer, selem_t *simple, snd_mixer_selem_t *control, const char *direction, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sRoute", get_full_name(simple->id.name), direction);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[(idx * voices) + idx] == 0)
			control->mute |= 1 << idx;
	return 0;
}

static int elem_read_capture_switch(snd_mixer_t *mixer, selem_t *simple, snd_mixer_selem_t *control, const char *direction, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sSwitch", get_full_name(simple->id.name), direction);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[voices == 1 ? 0 : idx])
			control->capture |= 1 << idx;
	return 0;
}

static int elem_read_capture_route(snd_mixer_t *mixer, selem_t *simple, snd_mixer_selem_t *control, const char *direction, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sRoute", get_full_name(simple->id.name), direction);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[(idx * voices) + idx])
			control->capture |= 1 << idx;
	return 0;
}

static int elem_read(snd_mixer_elem_t *elem,
		     snd_mixer_selem_t *control)
{
	snd_mixer_t *mixer;
	selem_t *simple;
	int idx, err;

	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	simple = elem->private;
	mixer = elem->mixer;

	control->caps = simple->caps;
	control->channels = 0;
	control->mute = 0;
	control->capture = 0;
	control->capture_group = 0;
	control->min = simple->min;
	control->max = simple->max;
	for (idx = 0; idx < 32; idx++)
		control->volume[idx] = 0;

	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		control->channels |= 1 << idx;
	if (simple->caps & SND_MIXER_SCTCAP_VOLUME) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_VOLUME) {
			elem_read_volume(mixer, simple, control, " Playback", " Volume", simple->pvolume_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_VOLUME) {
			elem_read_volume(mixer, simple, control, "", " Volume", simple->gvolume_values);
		} else if (simple->present & MIXER_PRESENT_SINGLE_VOLUME) {
			elem_read_volume(mixer, simple, control, "", "", simple->global_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_MUTE) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_SWITCH) {
			elem_read_mute_switch(mixer, simple, control, " Playback", " Switch", simple->pswitch_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_SWITCH) {
			elem_read_mute_switch(mixer, simple, control, "", " Switch", simple->gswitch_values);
		} else if (simple->present & MIXER_PRESENT_SINGLE_SWITCH) {
			elem_read_mute_switch(mixer, simple, control, "", "", simple->global_values);
		} else if (simple->present & MIXER_PRESENT_PLAYBACK_ROUTE) {
			elem_read_mute_route(mixer, simple, control, "Playback ", simple->proute_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_ROUTE) {
			elem_read_mute_route(mixer, simple, control, "", simple->groute_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_CAPTURE) {
		if (simple->present & MIXER_PRESENT_CAPTURE_SWITCH) {
			elem_read_capture_switch(mixer, simple, control, "Capture ", simple->cswitch_values);
		} else if (simple->present & MIXER_PRESENT_CAPTURE_ROUTE) {
			elem_read_capture_route(mixer, simple, control, "Capture ", simple->croute_values);
		} else if (simple->present & MIXER_PRESENT_CAPTURE_SOURCE) {
			snd_ctl_elem_t ctl;
			if ((err = get_mixer_read(mixer, "Capture Source", 0, &ctl)) < 0)
				return err;
			for (idx = 0; idx < simple->voices && idx < 32; idx++)
				if (ctl.value.enumerated.item[simple->ccapture_values == 1 ? 0 : idx] == simple->capture_item)
					control->capture |= 1 << idx;
		}
	}
	return 0;
}

static int elem_write_volume(snd_mixer_t *mixer, selem_t *simple, const snd_mixer_selem_t *control, const char *direction, const char *postfix, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s%s%s", get_full_name(simple->id.name), direction, postfix);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++) {
		ctl.value.integer.value[idx] = control->volume[idx];
		// fprintf(stderr, "ctl.id.name = '%s', volume = %i [%i]\n", ctl.id.name, ctl.value.integer.value[idx], idx);
	}
	if ((err = put_mixer_write(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_mute_switch(snd_mixer_t *mixer, selem_t *simple, const snd_mixer_selem_t *control, const char *direction, const char *postfix, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s%s%s", get_full_name(simple->id.name), direction, postfix);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[idx] = (control->mute & (1 << idx)) ? 0 : 1;
	if ((err = put_mixer_write(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_mute_route(snd_mixer_t *mixer, selem_t *simple, const snd_mixer_selem_t *control, const char *direction, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sRoute", get_full_name(simple->id.name), direction);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices * voices; idx++)
		ctl.value.integer.value[idx] = 0;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[(idx * voices) + idx] = (control->mute & (1 << idx)) ? 0 : 1;
	if ((err = put_mixer_write(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_capture_switch(snd_mixer_t *mixer, selem_t *simple, const snd_mixer_selem_t *control, const char *direction, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sSwitch", get_full_name(simple->id.name), direction);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[idx] = (control->capture & (1 << idx)) ? 1 : 0;
	if ((err = put_mixer_write(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_capture_route(snd_mixer_t *mixer, selem_t *simple, const snd_mixer_selem_t *control, const char *direction, int voices)
{
	char str[128];
	snd_ctl_elem_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sRoute", get_full_name(simple->id.name), direction);
	if ((err = get_mixer_read(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices * voices; idx++)
		ctl.value.integer.value[idx] = 0;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[(idx * voices) + idx] = (control->capture & (1 << idx)) ? 1 : 0;
	if ((err = put_mixer_write(mixer, str, simple->id.index, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write(snd_mixer_elem_t *elem,
		      const snd_mixer_selem_t *control)
{
	snd_mixer_t *mixer;
	selem_t *simple;
	int idx, err;

	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	simple = elem->private;
	mixer = elem->mixer;

	if (simple->caps & SND_MIXER_SCTCAP_VOLUME) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_VOLUME) {
			elem_write_volume(mixer, simple, control, " Playback", " Volume", simple->pvolume_values);
			if (simple->present & MIXER_PRESENT_CAPTURE_VOLUME)
				elem_write_volume(mixer, simple, control, " Capture", " Volume", simple->cvolume_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_VOLUME) {
			elem_write_volume(mixer, simple, control, "", " Volume", simple->gvolume_values);
		} else if (simple->present & MIXER_PRESENT_SINGLE_VOLUME) {
			elem_write_volume(mixer, simple, control, "", "", simple->global_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_MUTE) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_SWITCH)
			elem_write_mute_switch(mixer, simple, control, " Playback", " Switch", simple->pswitch_values);
		if (simple->present & MIXER_PRESENT_GLOBAL_SWITCH)
			elem_write_mute_switch(mixer, simple, control, "", " Switch", simple->gswitch_values);
		if (simple->present & MIXER_PRESENT_SINGLE_SWITCH)
			elem_write_mute_switch(mixer, simple, control, "", "", simple->global_values);
		if (simple->present & MIXER_PRESENT_PLAYBACK_ROUTE)
			elem_write_mute_route(mixer, simple, control, "Playback ", simple->proute_values);
		if (simple->present & MIXER_PRESENT_GLOBAL_ROUTE)
			elem_write_mute_route(mixer, simple, control, "", simple->groute_values);
	}
	if (simple->caps & SND_MIXER_SCTCAP_CAPTURE) {
		// fprintf(stderr, "capture: present = 0x%x\n", simple->present);
		if (simple->present & MIXER_PRESENT_CAPTURE_SWITCH) {
			elem_write_capture_switch(mixer, simple, control, "Capture ", simple->cswitch_values);
		} else if (simple->present & MIXER_PRESENT_CAPTURE_ROUTE) {
			elem_write_capture_route(mixer, simple, control, "Capture ", simple->croute_values);
		} else if (simple->present & MIXER_PRESENT_CAPTURE_SOURCE) {
			snd_ctl_elem_t ctl;
			if ((err = get_mixer_read(mixer, "Capture Source", 0, &ctl)) < 0)
				return err;
			// fprintf(stderr, "put capture source : %i [0x%x]\n", simple->capture_item, control->capture);
			for (idx = 0; idx < simple->voices && idx < 32; idx++) {
				if (control->capture & (1 << idx))
					ctl.value.enumerated.item[idx] = simple->capture_item;
			}
			if ((err = put_mixer_write(mixer, "Capture Source", 0, &ctl)) < 0)
				return err;
		}
	}
	return 0;
}

static void selem_free(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private;
	snd_hctl_bag_destroy(&s->elems);
	free(s);
}

static int build_elem_scontrol(snd_mixer_t *mixer, selem_t *s,
			       const char *sname, int index)
{
	snd_mixer_elem_t *elem;
	strcpy(s->id.name, sname);
	s->id.index = index;
	s->read = elem_read;
	s->write = elem_write;
	elem = calloc(1, sizeof(*elem));
	if (!elem)
		return -ENOMEM;
	s->elems.private = elem;
	elem->type = SND_MIXER_ELEM_SIMPLE;
	elem->private = s;
	elem->private_free = selem_free;
	snd_mixer_add_elem(mixer, elem);
	return 0;
}

static int build_elem(snd_mixer_t *mixer, const char *sname)
{
	char str[128];
	unsigned int present, caps, capture_item, voices;
	int index = -1, err;
	snd_ctl_elem_info_t global_info;
	snd_ctl_elem_info_t gswitch_info, pswitch_info, cswitch_info;
	snd_ctl_elem_info_t gvolume_info, pvolume_info, cvolume_info;
	snd_ctl_elem_info_t groute_info, proute_info, croute_info;
	snd_ctl_elem_info_t csource_info;
	long min, max;
	selem_t *simple;
	snd_hctl_elem_t *helem;
	const char *sname1;

	memset(&gswitch_info, 0, sizeof(gswitch_info));
	memset(&pswitch_info, 0, sizeof(pswitch_info));
	memset(&cswitch_info, 0, sizeof(cswitch_info));
	memset(&gvolume_info, 0, sizeof(gvolume_info));
	memset(&pvolume_info, 0, sizeof(pvolume_info));
	memset(&cvolume_info, 0, sizeof(cvolume_info));
	memset(&groute_info, 0, sizeof(groute_info));
	memset(&proute_info, 0, sizeof(proute_info));
	memset(&croute_info, 0, sizeof(croute_info));
	while (1) {
		simple = calloc(1, sizeof(*simple));
		index++;
		voices = 0;
		present = caps = capture_item = 0;
		min = max = 0;
		if ((helem = test_mixer_id(mixer, sname, index)) != NULL) {
			if ((err = get_mixer_info(mixer, sname, index, &global_info)) < 0)
				return err;
			if (global_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN || 
			    global_info.type == SNDRV_CTL_ELEM_TYPE_INTEGER) {
				if (voices < global_info.count)
					voices = global_info.count;
				caps |= global_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN ? SND_MIXER_SCTCAP_MUTE : SND_MIXER_SCTCAP_VOLUME;
				present |= global_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN ? MIXER_PRESENT_SINGLE_SWITCH : MIXER_PRESENT_SINGLE_VOLUME;
				if (global_info.type == SNDRV_CTL_ELEM_TYPE_INTEGER) {
					if (min > global_info.value.integer.min)
						min = global_info.value.integer.min;
					if (max < global_info.value.integer.max)
						max = global_info.value.integer.max;
				}
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Switch", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &gswitch_info)) < 0)
				return err;
			if (gswitch_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN) {
				if (voices < gswitch_info.count)
					voices = gswitch_info.count;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_GLOBAL_SWITCH;
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Route", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &groute_info)) < 0)
				return err;
			if (groute_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN && groute_info.count == 4) {
				if (voices < 2)
					voices = 2;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_GLOBAL_ROUTE;
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Volume", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &gvolume_info)) < 0)
				return err;
			if (gvolume_info.type == SNDRV_CTL_ELEM_TYPE_INTEGER) {
				if (voices < gvolume_info.count)
					voices = gvolume_info.count;
				if (min > gvolume_info.value.integer.min)
					min = gvolume_info.value.integer.min;
				if (max < gvolume_info.value.integer.max)
					max = gvolume_info.value.integer.max;
				caps |= SND_MIXER_SCTCAP_VOLUME;
				present |= MIXER_PRESENT_GLOBAL_VOLUME;
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Playback Switch", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &pswitch_info)) < 0)
				return err;
			if (pswitch_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN) {
				if (voices < pswitch_info.count)
					voices = pswitch_info.count;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_PLAYBACK_SWITCH;
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Playback Route", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &proute_info)) < 0)
				return err;
			if (proute_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN && proute_info.count == 4) {
				if (voices < 2)
					voices = 2;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_PLAYBACK_ROUTE;
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Capture Switch", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &cswitch_info)) < 0)
				return err;
			if (cswitch_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN) {
				if (voices < cswitch_info.count)
					voices = cswitch_info.count;
				caps |= SND_MIXER_SCTCAP_CAPTURE;
				present |= MIXER_PRESENT_CAPTURE_SWITCH;
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Capture Route", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &croute_info)) < 0)
				return err;
			if (croute_info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN && croute_info.count == 4) {
				if (voices < 2)
					voices = 2;
				caps |= SND_MIXER_SCTCAP_CAPTURE;
				present |= MIXER_PRESENT_CAPTURE_ROUTE;
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Playback Volume", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &pvolume_info)) < 0)
				return err;
			if (pvolume_info.type == SNDRV_CTL_ELEM_TYPE_INTEGER) {
				if (voices < pvolume_info.count)
					voices = pvolume_info.count;
				if (min > pvolume_info.value.integer.min)
					min = pvolume_info.value.integer.min;
				if (max < pvolume_info.value.integer.max)
					max = pvolume_info.value.integer.max;
				caps |= SND_MIXER_SCTCAP_VOLUME;
				present |= MIXER_PRESENT_PLAYBACK_VOLUME;
				hctl_elem_add(simple, helem);
			}
		}
		sprintf(str, "%s Capture Volume", sname);
		if ((helem = test_mixer_id(mixer, str, index)) != NULL) {
			if ((err = get_mixer_info(mixer, str, index, &cvolume_info)) < 0)
				return err;
			if (cvolume_info.type == SNDRV_CTL_ELEM_TYPE_INTEGER) {
				if (voices < cvolume_info.count)
					voices = cvolume_info.count;
				if (min > pvolume_info.value.integer.min)
					min = pvolume_info.value.integer.min;
				if (max < pvolume_info.value.integer.max)
					max = pvolume_info.value.integer.max;
				caps |= SND_MIXER_SCTCAP_VOLUME;
				present |= MIXER_PRESENT_CAPTURE_VOLUME;
				hctl_elem_add(simple, helem);
			}
		}
		if (index == 0 && (helem = test_mixer_id(mixer, "Capture Source", 0)) != NULL) {
			if ((err = get_mixer_info(mixer, "Capture Source", 0, &csource_info)) < 0)
				return err;
			strcpy(str, sname);
			if (!strcmp(str, "Master"))	/* special case */
				strcpy(str, "Mix");
			else if (!strcmp(str, "Master Mono")) /* special case */
				strcpy(str, "Mono Mix");
			if (csource_info.type == SNDRV_CTL_ELEM_TYPE_ENUMERATED) {
				capture_item = 0;
				if (!strcmp(csource_info.value.enumerated.name, str)) {
					if (voices < csource_info.count)
						voices = csource_info.count;
					caps |= SND_MIXER_SCTCAP_CAPTURE;
					present |= MIXER_PRESENT_CAPTURE_SOURCE;
					hctl_elem_add(simple, helem);
				} else for (capture_item = 1; capture_item < csource_info.value.enumerated.items; capture_item++) {
					csource_info.value.enumerated.item = capture_item;
					if ((err = snd_ctl_elem_info(mixer->ctl, &csource_info)) < 0)
						return err;
					if (!strcmp(csource_info.value.enumerated.name, str)) {
						if (voices < csource_info.count)
							voices = csource_info.count;
						caps |= SND_MIXER_SCTCAP_CAPTURE;
						present |= MIXER_PRESENT_CAPTURE_SOURCE;
						hctl_elem_add(simple, helem);
						break;
					}
				}
			}
		}
		if (voices > 1) {
			if (present & (MIXER_PRESENT_SINGLE_SWITCH|MIXER_PRESENT_GLOBAL_SWITCH|MIXER_PRESENT_GLOBAL_ROUTE|MIXER_PRESENT_PLAYBACK_SWITCH|MIXER_PRESENT_GLOBAL_SWITCH))
				caps |= SND_MIXER_SCTCAP_JOIN_MUTE;
			if (present & (MIXER_PRESENT_CAPTURE_SWITCH|MIXER_PRESENT_CAPTURE_ROUTE|MIXER_PRESENT_CAPTURE_SOURCE))
				caps |= SND_MIXER_SCTCAP_JOIN_CAPTURE;
			if (present & (MIXER_PRESENT_SINGLE_VOLUME|MIXER_PRESENT_GLOBAL_VOLUME|MIXER_PRESENT_PLAYBACK_VOLUME|MIXER_PRESENT_CAPTURE_VOLUME))
				caps |= SND_MIXER_SCTCAP_JOIN_VOLUME;
			if (present & MIXER_PRESENT_SINGLE_SWITCH) {
				if (global_info.count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOIN_MUTE;
			}
			if (present & MIXER_PRESENT_GLOBAL_SWITCH) {
				if (gswitch_info.count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOIN_MUTE;
			}
			if (present & MIXER_PRESENT_PLAYBACK_SWITCH) {
				if (pswitch_info.count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOIN_MUTE;
			}
			if (present & (MIXER_PRESENT_GLOBAL_ROUTE | MIXER_PRESENT_PLAYBACK_ROUTE))
				caps &= ~SND_MIXER_SCTCAP_JOIN_MUTE;
			if (present & MIXER_PRESENT_CAPTURE_SWITCH) {
				if (cswitch_info.count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOIN_CAPTURE;
			}
			if (present & MIXER_PRESENT_CAPTURE_ROUTE)
				caps &= ~SND_MIXER_SCTCAP_JOIN_CAPTURE;
			if (present & MIXER_PRESENT_SINGLE_VOLUME) {
				if (global_info.count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOIN_VOLUME;
			}
			if (present & MIXER_PRESENT_GLOBAL_VOLUME) {
				if (gvolume_info.count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOIN_VOLUME;
			}
			if (present & MIXER_PRESENT_PLAYBACK_VOLUME) {
				if (pvolume_info.count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOIN_VOLUME;
			}
			if (present & MIXER_PRESENT_CAPTURE_VOLUME) {
				if (cvolume_info.count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOIN_VOLUME;
			}
		}
		if (present == 0) {
			free(simple);
			break;
		}
		sname1 = get_short_name(sname);
		simple->present = present;
		simple->global_values = global_info.count;
		simple->gswitch_values = gswitch_info.count;
		simple->pswitch_values = pswitch_info.count;
		simple->cswitch_values = cswitch_info.count;
		simple->gvolume_values = gvolume_info.count;
		simple->pvolume_values = pvolume_info.count;
		simple->cvolume_values = cvolume_info.count;
		simple->groute_values = 2;
		simple->proute_values = 2;
		simple->croute_values = 2;
		simple->ccapture_values = csource_info.count;
		simple->capture_item = capture_item;
		simple->caps = caps;
		simple->voices = voices;
		simple->min = min;
		simple->max = max;
		if (build_elem_scontrol(mixer, simple, sname1, index) < 0) {
			snd_hctl_bag_destroy(&simple->elems);
			free(simple);
			return -ENOMEM;
		}
		// fprintf(stderr, "sname = '%s', index = %i, present = 0x%x, voices = %i\n", sname, index, present, voices);
	};
	return 0;
}

int mixer_simple_ctl_callback(snd_ctl_t *ctl,
			      snd_ctl_event_type_t event,
			      snd_hctl_elem_t *elem ATTRIBUTE_UNUSED)
{
	snd_mixer_t *mixer = snd_hctl_get_callback_private(ctl);
	int err;
	switch (event) {
	case SND_CTL_EVENT_REBUILD:
		snd_mixer_free(mixer);
		err = snd_mixer_simple_build(mixer);
		if (err < 0)
			return err;
		return mixer->callback(mixer, event, NULL);
	case SND_CTL_EVENT_ADD:
		/* TODO */
		return 0;
	default:
		assert(0);
	}
}

int snd_mixer_simple_build(snd_mixer_t *mixer)
{
	static char *elems[] = {
		"Master",
		"Master Mono",
		"Master Digital",
		"Tone Control - Bass",
		"Tone Control - Treble",
		"Synth Tone Control - Bass",
		"Synth Tone Control - Treble",
		"PCM",
		"Surround",
		"Synth",
		"FM",
		"Wave",
		"Music",
		"DSP",
		"Line",
		"CD",
		"Mic",
		"Video",
		"Phone",
		"PC Speaker",
		"Aux",
		"Mono",
		"Mono Output",
		"Playback",
		"Capture",
		"Capture Boost",
		NULL
	};
	snd_ctl_t *ctl = mixer->ctl;
	char **elem = elems;
	int err;

	if ((err = snd_hctl_build(ctl)) < 0)
		return err;
	while (*elem) {
		if ((err = build_elem(mixer, *elem)) < 0)
			return err;
		elem++;
	}
	snd_hctl_set_callback(ctl, mixer_simple_ctl_callback);
	snd_hctl_set_callback_private(ctl, mixer);
	return 0;
}

snd_mixer_elem_t *snd_mixer_find_selem(snd_mixer_t *mixer,
						   const snd_mixer_selem_id_t *id)
{
	struct list_head *list;
	list_for_each(list, &mixer->elems) {
		snd_mixer_elem_t *e;
		selem_t *s;
		e = list_entry(list, snd_mixer_elem_t, list);
		if (e->type != SND_MIXER_ELEM_SIMPLE)
			continue;
		s = e->private;
		if (!strcmp(s->id.name, id->name) && s->id.index == id->index)
			return e;
	}
	return NULL;
}

void snd_mixer_selem_get_id(snd_mixer_elem_t *element,
			    snd_mixer_selem_id_t *id)
{
	selem_t *s;
	assert(element && id);
	assert(element->type == SND_MIXER_ELEM_SIMPLE);
	s = element->private;
	*id = s->id;
}

int snd_mixer_selem_read(snd_mixer_elem_t *element,
			 snd_mixer_selem_t *value)
{
	selem_t *s;
	assert(element && value);
	assert(element->type == SND_MIXER_ELEM_SIMPLE);
	s = element->private;
	if (s->read == NULL)
		return -EIO;
	return s->read(element, value);
}

int snd_mixer_selem_write(snd_mixer_elem_t *element,
			  const snd_mixer_selem_t *value)
{
	selem_t *s;
	assert(element && value);
	assert(element->type == SND_MIXER_ELEM_SIMPLE);
	s = element->private;
	if (s->write == NULL)
		return -EIO;
	return s->write(element, value);
}

int snd_mixer_selem_is_mono(const snd_mixer_selem_t *obj)
{
	assert(obj);
	return obj->channels == 1 << SND_MIXER_CHN_MONO;
}

long snd_mixer_selem_get_volume(const snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel)
{
	assert(obj);
	return obj->volume[channel];
}

void snd_mixer_selem_set_volume(snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel, long value)
{
	assert(obj);
	obj->volume[channel] = value;
}

int snd_mixer_selem_has_channel(const snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel)
{
	assert(obj);
	assert(channel <= SND_MIXER_CHN_LAST);
	return !!(obj->channels & (1 << channel));
}

int snd_mixer_selem_get_mute(const snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel)
{
	assert(obj);
	assert(channel <= SND_MIXER_CHN_LAST);
	return !!(obj->mute & (1 << channel));
}

void snd_mixer_selem_set_mute(snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel, int mute)
{
	assert(obj);
	assert(channel <= SND_MIXER_CHN_LAST);
	if (mute)
		obj->mute |= (1 << channel);
	else
		obj->mute &= ~(1 << channel);
}

int snd_mixer_selem_get_capture(const snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel)
{
	assert(obj);
	assert(channel <= SND_MIXER_CHN_LAST);
	return !!(obj->capture & (1 << channel));
}

void snd_mixer_selem_set_capture(snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel, int capture)
{
	assert(obj);
	assert(channel <= SND_MIXER_CHN_LAST);
	if (capture)
		obj->capture |= (1 << channel);
	else
		obj->capture &= ~(1 << channel);
}

void snd_mixer_selem_set_mute_all(snd_mixer_selem_t *obj, int mute)
{
	assert(obj);
	if (mute)
		obj->mute = ~0U;
	else
		obj->mute = 0;
}

void snd_mixer_selem_set_capture_all(snd_mixer_selem_t *obj, int capture)
{
	assert(obj);
	if (capture)
		obj->capture = ~0U;
	else
		obj->capture = 0;
}

void snd_mixer_selem_set_volume_all(snd_mixer_selem_t *obj, long value)
{
	unsigned int c;
	assert(obj);
	for (c = 0; c < sizeof(obj->volume) / sizeof(obj->volume[0]); c++)
		obj->volume[c] = value;
}
