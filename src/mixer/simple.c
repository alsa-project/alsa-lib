/*
 *  Mixer Interface - simple controls
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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
#include "mixer_local.h"

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

static snd_hcontrol_t *test_mixer_id(snd_mixer_t *handle, const char *name, int index)
{
	snd_control_id_t id;
	snd_hcontrol_t *hcontrol;
	
	memset(&id, 0, sizeof(id));
	id.iface = SND_CONTROL_IFACE_MIXER;
	strcpy(id.name, name);
	id.index = index;
	hcontrol = snd_ctl_hfind(handle->ctl_handle, &id);
	// fprintf(stderr, "Looking for control: '%s', %i (0x%lx)\n", name, index, (long)hcontrol);
	return hcontrol;
}

static int get_mixer_info(snd_mixer_t *handle, const char *name, int index, snd_control_info_t *info)
{
	memset(info, 0, sizeof(*info));
	info->id.iface = SND_CONTROL_IFACE_MIXER;
	strcpy(info->id.name, name);
	info->id.index = index;
	return snd_ctl_cinfo(handle->ctl_handle, info);
}

static int get_mixer_read(snd_mixer_t *handle, const char *name, int index, snd_control_t *control)
{
	memset(control, 0, sizeof(*control));
	control->id.iface = SND_CONTROL_IFACE_MIXER;
	strcpy(control->id.name, name);
	control->id.index = index;
	return snd_ctl_cread(handle->ctl_handle, control);
}

static int put_mixer_write(snd_mixer_t *handle, const char *name, int index, snd_control_t *control)
{
	control->id.numid = 0;
	control->id.iface = SND_CONTROL_IFACE_MIXER;
	strcpy(control->id.name, name);
	control->id.device = control->id.subdevice = 0;
	control->id.index = index;
	control->indirect = 0;
	memset(&control->reserved, 0, sizeof(control->reserved));
	return snd_ctl_cwrite(handle->ctl_handle, control);
}

static mixer_simple_t *simple_new(mixer_simple_t *scontrol)
{
	mixer_simple_t *s;
	
	if (scontrol == NULL)
		return NULL;
	s = (mixer_simple_t *) calloc(1, sizeof(*s));
	if (s == NULL)
		return NULL;
	*s = *scontrol;
	return s;
}

static void hcontrol_event_change(snd_ctl_t *ctl_handle ATTRIBUTE_UNUSED, snd_hcontrol_t *hcontrol ATTRIBUTE_UNUSED)
{
	/* ignore at this moment */
}

static void hcontrol_event_value(snd_ctl_t *ctl_handle ATTRIBUTE_UNUSED, snd_hcontrol_t *hcontrol)
{
	snd_mixer_t *handle = (snd_mixer_t *)hcontrol->private_data;
	mixer_simple_t *s;
	struct list_head *list;
	list_for_each(list, &handle->simples) {
		s = list_entry(list, mixer_simple_t, list);
		if (snd_ctl_hbag_find(&s->hcontrols, &hcontrol->id))
			s->change++;
	}
}

static void hcontrol_event_remove(snd_ctl_t *ctl_handle ATTRIBUTE_UNUSED, snd_hcontrol_t *hcontrol ATTRIBUTE_UNUSED)
{
	/* ignore at this moment */
}

static void hcontrol_add(snd_mixer_t *handle, void **bag, snd_hcontrol_t *hcontrol)
{
	snd_ctl_hbag_add(bag, hcontrol);
	hcontrol->event_change = hcontrol_event_change;
	hcontrol->event_value = hcontrol_event_value;
	hcontrol->event_remove = hcontrol_event_remove;
	hcontrol->private_data = handle;
}

static int simple_add(snd_mixer_t *handle, mixer_simple_t *scontrol)
{
	if (handle == NULL || scontrol == NULL)
		return -EINVAL;
	list_add_tail(&scontrol->list, &handle->simples);
	handle->simple_count++;
	return 0;
}

static int simple_remove(snd_mixer_t *handle, mixer_simple_t *scontrol)
{
	if (handle == NULL || scontrol == NULL)
		return -EINVAL;
	list_del(&scontrol->list);
	handle->simple_count--;
	snd_ctl_hbag_destroy(&scontrol->hcontrols, NULL);
	free(scontrol);
	return 0;
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

static int input_get_volume(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, const char *postfix, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s%s%s", get_full_name(simple->sid.name), direction, postfix);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		control->volume.values[idx] = ctl.value.integer.value[voices == 1 ? 0 : idx];
	return 0;
}

static int input_get_mute_switch(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, const char *postfix, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s%s%s", get_full_name(simple->sid.name), direction, postfix);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[voices == 1 ? 0 : idx] == 0)
			control->mute |= 1 << idx;
	return 0;
}

static int input_get_mute_route(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sRoute", get_full_name(simple->sid.name), direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[(idx * voices) + idx] == 0)
			control->mute |= 1 << idx;
	return 0;
}

static int input_get_capture_switch(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sSwitch", get_full_name(simple->sid.name), direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[voices == 1 ? 0 : idx])
			control->capture |= 1 << idx;
	return 0;
}

static int input_get_capture_route(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sRoute", get_full_name(simple->sid.name), direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[(idx * voices) + idx])
			control->capture |= 1 << idx;
	return 0;
}

static int input_get(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control)
{
	int idx, err;

	if (simple == NULL)
		return -EINVAL;

	control->caps = simple->caps;
	control->channels = 0;
	control->mute = 0;
	control->capture = 0;
	control->capture_group = 0;
	control->min = simple->min;
	control->max = simple->max;
	for (idx = 0; idx < 32; idx++)
		control->volume.values[idx] = 0;

	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		control->channels |= 1 << idx;
	if (simple->caps & SND_MIXER_SCTCAP_VOLUME) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_VOLUME) {
			input_get_volume(handle, simple, control, " Playback", " Volume", simple->pvolume_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_VOLUME) {
			input_get_volume(handle, simple, control, "", " Volume", simple->gvolume_values);
		} else if (simple->present & MIXER_PRESENT_SINGLE_VOLUME) {
			input_get_volume(handle, simple, control, "", "", simple->global_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_MUTE) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_SWITCH) {
			input_get_mute_switch(handle, simple, control, " Playback", " Switch", simple->pswitch_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_SWITCH) {
			input_get_mute_switch(handle, simple, control, "", " Switch", simple->gswitch_values);
		} else if (simple->present & MIXER_PRESENT_SINGLE_SWITCH) {
			input_get_mute_switch(handle, simple, control, "", "", simple->global_values);
		} else if (simple->present & MIXER_PRESENT_PLAYBACK_ROUTE) {
			input_get_mute_route(handle, simple, control, "Playback ", simple->proute_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_ROUTE) {
			input_get_mute_route(handle, simple, control, "", simple->groute_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_CAPTURE) {
		if (simple->present & MIXER_PRESENT_CAPTURE_SWITCH) {
			input_get_capture_switch(handle, simple, control, "Capture ", simple->cswitch_values);
		} else if (simple->present & MIXER_PRESENT_CAPTURE_ROUTE) {
			input_get_capture_route(handle, simple, control, "Capture ", simple->croute_values);
		} else if (simple->present & MIXER_PRESENT_CAPTURE_SOURCE) {
			snd_control_t ctl;
			if ((err = get_mixer_read(handle, "Capture Source", 0, &ctl)) < 0)
				return err;
			for (idx = 0; idx < simple->voices && idx < 32; idx++)
				if (ctl.value.enumerated.item[simple->ccapture_values == 1 ? 0 : idx] == simple->capture_item)
					control->capture |= 1 << idx;
		}
	}
	return 0;
}

static int input_put_volume(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, const char *postfix, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s%s%s", get_full_name(simple->sid.name), direction, postfix);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++) {
		ctl.value.integer.value[idx] = control->volume.values[idx];
		// fprintf(stderr, "ctl.id.name = '%s', volume = %i [%i]\n", ctl.id.name, ctl.value.integer.value[idx], idx);
	}
	if ((err = put_mixer_write(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	return 0;
}

static int input_put_mute_switch(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, const char *postfix, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s%s%s", get_full_name(simple->sid.name), direction, postfix);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[idx] = (control->mute & (1 << idx)) ? 0 : 1;
	if ((err = put_mixer_write(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	return 0;
}

static int input_put_mute_route(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sRoute", get_full_name(simple->sid.name), direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices * voices; idx++)
		ctl.value.integer.value[idx] = 0;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[(idx * voices) + idx] = (control->mute & (1 << idx)) ? 0 : 1;
	if ((err = put_mixer_write(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	return 0;
}

static int input_put_capture_switch(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sSwitch", get_full_name(simple->sid.name), direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[idx] = (control->capture & (1 << idx)) ? 1 : 0;
	if ((err = put_mixer_write(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	return 0;
}

static int input_put_capture_route(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sRoute", get_full_name(simple->sid.name), direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices * voices; idx++)
		ctl.value.integer.value[idx] = 0;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[(idx * voices) + idx] = (control->capture & (1 << idx)) ? 1 : 0;
	if ((err = put_mixer_write(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	return 0;
}

static int input_put(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control)
{
	int err, idx;

	if (simple == NULL)
		return -EINVAL;

	if (simple->caps & SND_MIXER_SCTCAP_VOLUME) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_VOLUME) {
			input_put_volume(handle, simple, control, " Playback", " Volume", simple->pvolume_values);
			if (simple->present & MIXER_PRESENT_CAPTURE_VOLUME)
				input_put_volume(handle, simple, control, " Capture", " Volume", simple->cvolume_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_VOLUME) {
			input_put_volume(handle, simple, control, "", " Volume", simple->gvolume_values);
		} else if (simple->present & MIXER_PRESENT_SINGLE_VOLUME) {
			input_put_volume(handle, simple, control, "", "", simple->global_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_MUTE) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_SWITCH)
			input_put_mute_switch(handle, simple, control, " Playback", " Switch", simple->pswitch_values);
		if (simple->present & MIXER_PRESENT_GLOBAL_SWITCH)
			input_put_mute_switch(handle, simple, control, "", " Switch", simple->gswitch_values);
		if (simple->present & MIXER_PRESENT_SINGLE_SWITCH)
			input_put_mute_switch(handle, simple, control, "", "", simple->global_values);
		if (simple->present & MIXER_PRESENT_PLAYBACK_ROUTE)
			input_put_mute_route(handle, simple, control, "Playback ", simple->proute_values);
		if (simple->present & MIXER_PRESENT_GLOBAL_ROUTE)
			input_put_mute_route(handle, simple, control, "", simple->groute_values);
	}
	if (simple->caps & SND_MIXER_SCTCAP_CAPTURE) {
		// fprintf(stderr, "capture: present = 0x%x\n", simple->present);
		if (simple->present & MIXER_PRESENT_CAPTURE_SWITCH) {
			input_put_capture_switch(handle, simple, control, "Capture ", simple->cswitch_values);
		} else if (simple->present & MIXER_PRESENT_CAPTURE_ROUTE) {
			input_put_capture_route(handle, simple, control, "Capture ", simple->croute_values);
		} else if (simple->present & MIXER_PRESENT_CAPTURE_SOURCE) {
			snd_control_t ctl;
			if ((err = get_mixer_read(handle, "Capture Source", 0, &ctl)) < 0)
				return err;
			// fprintf(stderr, "put capture source : %i [0x%x]\n", simple->capture_item, control->capture);
			for (idx = 0; idx < simple->voices && idx < 32; idx++) {
				if (control->capture & (1 << idx))
					ctl.value.enumerated.item[idx] = simple->capture_item;
			}
			if ((err = put_mixer_write(handle, "Capture Source", 0, &ctl)) < 0)
				return err;
		}
	}
	return 0;
}

static mixer_simple_t *build_input_scontrol(snd_mixer_t *handle, const char *sname, int index)
{
	mixer_simple_t s, *p;

	memset(&s, 0, sizeof(s));
	strcpy(s.sid.name, sname);
	s.sid.index = index;
	s.get = input_get;
	s.put = input_put;
	if (simple_add(handle, p = simple_new(&s)) < 0) {
		free(p);
		return NULL;
	}
	return p;
}

static int build_input(snd_mixer_t *handle, const char *sname)
{
	char str[128];
	unsigned int present, caps, capture_item, voices;
	int index = -1, err;
	snd_control_info_t global_info;
	snd_control_info_t gswitch_info, pswitch_info, cswitch_info;
	snd_control_info_t gvolume_info, pvolume_info, cvolume_info;
	snd_control_info_t groute_info, proute_info, croute_info;
	snd_control_info_t csource_info;
	long min, max;
	void *bag;
	mixer_simple_t *simple;
	snd_hcontrol_t *hcontrol;
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
		index++;
		voices = 0;
		present = caps = capture_item = 0;
		min = max = 0;
		bag = NULL;
		if ((hcontrol = test_mixer_id(handle, sname, index)) != NULL) {
			if ((err = get_mixer_info(handle, sname, index, &global_info)) < 0)
				return err;
			if (global_info.type == SND_CONTROL_TYPE_BOOLEAN || global_info.type == SND_CONTROL_TYPE_INTEGER) {
				if (voices < global_info.values_count)
					voices = global_info.values_count;
				caps |= global_info.type == SND_CONTROL_TYPE_BOOLEAN ? SND_MIXER_SCTCAP_MUTE : SND_MIXER_SCTCAP_VOLUME;
				present |= global_info.type == SND_CONTROL_TYPE_BOOLEAN ? MIXER_PRESENT_SINGLE_SWITCH : MIXER_PRESENT_SINGLE_VOLUME;
				if (global_info.type == SND_CONTROL_TYPE_INTEGER) {
					if (min > global_info.value.integer.min)
						min = global_info.value.integer.min;
					if (max < global_info.value.integer.max)
						max = global_info.value.integer.max;
				}
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Switch", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &gswitch_info)) < 0)
				return err;
			if (gswitch_info.type == SND_CONTROL_TYPE_BOOLEAN) {
				if (voices < gswitch_info.values_count)
					voices = gswitch_info.values_count;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_GLOBAL_SWITCH;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Route", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &groute_info)) < 0)
				return err;
			if (groute_info.type == SND_CONTROL_TYPE_BOOLEAN && groute_info.values_count == 4) {
				if (voices < 2)
					voices = 2;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_GLOBAL_ROUTE;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Volume", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &gvolume_info)) < 0)
				return err;
			if (gvolume_info.type == SND_CONTROL_TYPE_INTEGER) {
				if (voices < gvolume_info.values_count)
					voices = gvolume_info.values_count;
				if (min > gvolume_info.value.integer.min)
					min = gvolume_info.value.integer.min;
				if (max < gvolume_info.value.integer.max)
					max = gvolume_info.value.integer.max;
				caps |= SND_MIXER_SCTCAP_VOLUME;
				present |= MIXER_PRESENT_GLOBAL_VOLUME;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Playback Switch", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &pswitch_info)) < 0)
				return err;
			if (pswitch_info.type == SND_CONTROL_TYPE_BOOLEAN) {
				if (voices < pswitch_info.values_count)
					voices = pswitch_info.values_count;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_PLAYBACK_SWITCH;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Playback Route", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &proute_info)) < 0)
				return err;
			if (proute_info.type == SND_CONTROL_TYPE_BOOLEAN && proute_info.values_count == 4) {
				if (voices < 2)
					voices = 2;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_PLAYBACK_ROUTE;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Capture Switch", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &cswitch_info)) < 0)
				return err;
			if (cswitch_info.type == SND_CONTROL_TYPE_BOOLEAN) {
				if (voices < cswitch_info.values_count)
					voices = cswitch_info.values_count;
				caps |= SND_MIXER_SCTCAP_CAPTURE;
				present |= MIXER_PRESENT_CAPTURE_SWITCH;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Capture Route", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &croute_info)) < 0)
				return err;
			if (croute_info.type == SND_CONTROL_TYPE_BOOLEAN && croute_info.values_count == 4) {
				if (voices < 2)
					voices = 2;
				caps |= SND_MIXER_SCTCAP_CAPTURE;
				present |= MIXER_PRESENT_CAPTURE_ROUTE;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Playback Volume", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &pvolume_info)) < 0)
				return err;
			if (pvolume_info.type == SND_CONTROL_TYPE_INTEGER) {
				if (voices < pvolume_info.values_count)
					voices = pvolume_info.values_count;
				if (min > pvolume_info.value.integer.min)
					min = pvolume_info.value.integer.min;
				if (max < pvolume_info.value.integer.max)
					max = pvolume_info.value.integer.max;
				caps |= SND_MIXER_SCTCAP_VOLUME;
				present |= MIXER_PRESENT_PLAYBACK_VOLUME;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		sprintf(str, "%s Capture Volume", sname);
		if ((hcontrol = test_mixer_id(handle, str, index)) != NULL) {
			if ((err = get_mixer_info(handle, str, index, &cvolume_info)) < 0)
				return err;
			if (cvolume_info.type == SND_CONTROL_TYPE_INTEGER) {
				if (voices < cvolume_info.values_count)
					voices = cvolume_info.values_count;
				if (min > pvolume_info.value.integer.min)
					min = pvolume_info.value.integer.min;
				if (max < pvolume_info.value.integer.max)
					max = pvolume_info.value.integer.max;
				caps |= SND_MIXER_SCTCAP_VOLUME;
				present |= MIXER_PRESENT_CAPTURE_VOLUME;
				hcontrol_add(handle, &bag, hcontrol);
			}
		}
		if (index == 0 && (hcontrol = test_mixer_id(handle, "Capture Source", 0)) != NULL) {
			if ((err = get_mixer_info(handle, "Capture Source", 0, &csource_info)) < 0)
				return err;
			strcpy(str, sname);
			if (!strcmp(str, "Master"))	/* special case */
				strcpy(str, "Mix");
			else if (!strcmp(str, "Master Mono")) /* special case */
				strcpy(str, "Mono Mix");
			if (csource_info.type == SND_CONTROL_TYPE_ENUMERATED) {
				capture_item = 0;
				if (!strcmp(csource_info.value.enumerated.name, str)) {
					if (voices < csource_info.values_count)
						voices = csource_info.values_count;
					caps |= SND_MIXER_SCTCAP_CAPTURE;
					present |= MIXER_PRESENT_CAPTURE_SOURCE;
					hcontrol_add(handle, &bag, hcontrol);
				} else for (capture_item = 1; capture_item < csource_info.value.enumerated.items; capture_item++) {
					csource_info.value.enumerated.item = capture_item;
					if ((err = snd_ctl_cinfo(handle->ctl_handle, &csource_info)) < 0)
						return err;
					if (!strcmp(csource_info.value.enumerated.name, str)) {
						if (voices < csource_info.values_count)
							voices = csource_info.values_count;
						caps |= SND_MIXER_SCTCAP_CAPTURE;
						present |= MIXER_PRESENT_CAPTURE_SOURCE;
						hcontrol_add(handle, &bag, hcontrol);
						break;
					}
				}
			}
		}
		if (voices > 1) {
			if (present & (MIXER_PRESENT_SINGLE_SWITCH|MIXER_PRESENT_GLOBAL_SWITCH|MIXER_PRESENT_GLOBAL_ROUTE|MIXER_PRESENT_PLAYBACK_SWITCH|MIXER_PRESENT_GLOBAL_SWITCH))
				caps |= SND_MIXER_SCTCAP_JOINTLY_MUTE;
			if (present & (MIXER_PRESENT_CAPTURE_SWITCH|MIXER_PRESENT_CAPTURE_ROUTE|MIXER_PRESENT_CAPTURE_SOURCE))
				caps |= SND_MIXER_SCTCAP_JOINTLY_CAPTURE;
			if (present & (MIXER_PRESENT_SINGLE_VOLUME|MIXER_PRESENT_GLOBAL_VOLUME|MIXER_PRESENT_PLAYBACK_VOLUME|MIXER_PRESENT_CAPTURE_VOLUME))
				caps |= SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			if (present & MIXER_PRESENT_SINGLE_SWITCH) {
				if (global_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_MUTE;
			}
			if (present & MIXER_PRESENT_GLOBAL_SWITCH) {
				if (gswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_MUTE;
			}
			if (present & MIXER_PRESENT_PLAYBACK_SWITCH) {
				if (pswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_MUTE;
			}
			if (present & (MIXER_PRESENT_GLOBAL_ROUTE | MIXER_PRESENT_PLAYBACK_ROUTE))
				caps &= ~SND_MIXER_SCTCAP_JOINTLY_MUTE;
			if (present & MIXER_PRESENT_CAPTURE_SWITCH) {
				if (cswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_CAPTURE;
			}
			if (present & MIXER_PRESENT_CAPTURE_ROUTE)
				caps &= ~SND_MIXER_SCTCAP_JOINTLY_CAPTURE;
			if (present & MIXER_PRESENT_SINGLE_VOLUME) {
				if (global_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			}
			if (present & MIXER_PRESENT_GLOBAL_VOLUME) {
				if (gvolume_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			}
			if (present & MIXER_PRESENT_PLAYBACK_VOLUME) {
				if (pvolume_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			}
			if (present & MIXER_PRESENT_CAPTURE_VOLUME) {
				if (cvolume_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			}
		}
		if (present == 0)
			break;
		sname1 = get_short_name(sname);
		simple = build_input_scontrol(handle, sname1, index);
		if (simple == NULL) {
			snd_ctl_hbag_destroy(&bag, NULL);
			return -ENOMEM;
		}
		simple->present = present;
		simple->global_values = global_info.values_count;
		simple->gswitch_values = gswitch_info.values_count;
		simple->pswitch_values = pswitch_info.values_count;
		simple->cswitch_values = cswitch_info.values_count;
		simple->gvolume_values = gvolume_info.values_count;
		simple->pvolume_values = pvolume_info.values_count;
		simple->cvolume_values = cvolume_info.values_count;
		simple->groute_values = 2;
		simple->proute_values = 2;
		simple->croute_values = 2;
		simple->ccapture_values = csource_info.values_count;
		simple->capture_item = capture_item;
		simple->caps = caps;
		simple->voices = voices;
		simple->min = min;
		simple->max = max;
		simple->hcontrols = bag;
		// fprintf(stderr, "sname = '%s', index = %i, present = 0x%x, voices = %i\n", sname, index, present, voices);
	};
	return 0;
}

int snd_mixer_simple_build(snd_mixer_t *handle)
{
	static char *inputs[] = {
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
	char **input = inputs;
	int err;

	if ((err = snd_ctl_hbuild(handle->ctl_handle, snd_ctl_hsort)) < 0)
		return err;
	while (*input) {
		if ((err = build_input(handle, *input)) < 0) {
			snd_mixer_simple_destroy(handle);
			return err;
		}
		input++;
	}
	handle->simple_valid = 1;
	return 0;
}

int snd_mixer_simple_destroy(snd_mixer_t *handle)
{
	while (!list_empty(&handle->simples))
		simple_remove(handle, list_entry(handle->simples.next, mixer_simple_t, list));
	handle->simple_valid = 0;
	snd_ctl_hfree(handle->ctl_handle);
	return 0;
}
