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
#include "asoundlib.h"
#include "mixer_local.h"

static int test_mixer_id(snd_mixer_t *handle, const char *name, int index)
{
	snd_control_id_t id;
	snd_hcontrol_t *hcontrol;
	
	memset(&id, 0, sizeof(id));
	id.iface = SND_CONTROL_IFACE_MIXER;
	strcpy(id.name, name);
	id.index = index;
	printf("look\n");
	hcontrol = snd_ctl_cfind(handle->ctl_handle, &id);
	fprintf(stderr, "Looking for control: '%s', %i (0x%lx)\n", name, index, (long)hcontrol);
	return hcontrol != NULL;
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

static int simple_add(snd_mixer_t *handle, mixer_simple_t *scontrol)
{
	if (handle == NULL || scontrol == NULL)
		return -EINVAL;
	if (handle->simple_last != NULL) {
		handle->simple_last->next = scontrol;
		scontrol->prev = handle->simple_last;
		scontrol->next = NULL;
		handle->simple_last = scontrol;
	} else {
		handle->simple_first = handle->simple_last = scontrol;
		scontrol->prev = scontrol->next = NULL;
	}
	handle->simple_count++;
	return 0;
}

static int simple_remove(snd_mixer_t *handle, mixer_simple_t *scontrol)
{
	if (handle == NULL || scontrol == NULL)
		return -EINVAL;
	if (handle->simple_first == scontrol)
		handle->simple_first = scontrol->next;
	if (handle->simple_last == scontrol)
		handle->simple_last = scontrol->prev;
	if (scontrol->prev)
		scontrol->prev->next = scontrol->next;
	if (scontrol->next)
		scontrol->next->prev = scontrol->prev;
	handle->simple_count--;
	return 0;
}

static int input_get_volume(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sVolume", simple->sid.name, direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		control->volume.values[idx] = ctl.value.integer.value[voices == 1 ? 0 : idx];
	return 0;
}

static int input_get_mute_switch(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sSwitch", simple->sid.name, direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[voices == 1 ? 0 : idx] == 0)
			control->mute |= 1 << idx;
	return 0;
}

static int input_get_capture_switch(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sSwitch", simple->sid.name, direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->voices && idx < 32; idx++)
		if (ctl.value.integer.value[voices == 1 ? 0 : idx])
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
			input_get_volume(handle, simple, control, "Playback ", simple->pvolume_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_VOLUME) {
			input_get_volume(handle, simple, control, "", simple->gvolume_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_MUTE) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_SWITCH) {
			input_get_mute_switch(handle, simple, control, "Playback ", simple->pswitch_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_VOLUME) {
			input_get_mute_switch(handle, simple, control, "", simple->pvolume_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_CAPTURE) {
		if (simple->present & MIXER_PRESENT_CAPTURE_SWITCH) {
			input_get_capture_switch(handle, simple, control, "Capture ", simple->cswitch_values);
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

static int input_put_volume(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sVolume", simple->sid.name, direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++) {
		ctl.value.integer.value[idx] = control->volume.values[idx];
		// fprintf(stderr, "ctl.id.name = '%s', volume = %i\n", ctl.id.name, ctl.value.integer.value[idx]);
	}
	if ((err = put_mixer_write(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	return 0;
}

static int input_put_mute_switch(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sSwitch", simple->sid.name, direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++) {
		ctl.value.integer.value[idx] = (control->mute & (1 << idx)) ? 0 : 1;
		// fprintf(stderr, "ctl.id.name = '%s', switch = %i\n", ctl.id.name, ctl.value.integer.value[idx]);
	}
	err = put_mixer_write(handle, str, simple->sid.index, &ctl);
	if ((err = put_mixer_write(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	return 0;
}

static int input_put_capture_switch(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control, const char *direction, int voices)
{
	char str[128];
	snd_control_t ctl;
	int idx, err;
	
	sprintf(str, "%s %sSwitch", simple->sid.name, direction);
	if ((err = get_mixer_read(handle, str, simple->sid.index, &ctl)) < 0)
		return err;
	for (idx = 0; idx < voices && idx < 32; idx++)
		ctl.value.integer.value[idx] = (control->capture & (1 << idx)) ? 1 : 0;
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
			input_put_volume(handle, simple, control, "Playback ", simple->pvolume_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_VOLUME) {
			input_put_volume(handle, simple, control, "", simple->gvolume_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_MUTE) {
		if (simple->present & MIXER_PRESENT_PLAYBACK_SWITCH) {
			input_put_mute_switch(handle, simple, control, "Playback ", simple->pswitch_values);
		} else if (simple->present & MIXER_PRESENT_GLOBAL_VOLUME) {
			input_put_mute_switch(handle, simple, control, "", simple->pvolume_values);
		}
	}
	if (simple->caps & SND_MIXER_SCTCAP_CAPTURE) {
		// fprintf(stderr, "capture: present = 0x%x\n", simple->present);
		if (simple->present & MIXER_PRESENT_CAPTURE_SWITCH) {
			input_put_capture_switch(handle, simple, control, "Capture ", simple->cswitch_values);
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
	snd_control_info_t gswitch_info, pswitch_info, cswitch_info;
	snd_control_info_t gvolume_info, pvolume_info, cvolume_info;
	snd_control_info_t csource_info;
	long min, max;
	mixer_simple_t *simple;

	memset(&gswitch_info, 0, sizeof(gswitch_info));
	memset(&pswitch_info, 0, sizeof(pswitch_info));
	memset(&cswitch_info, 0, sizeof(cswitch_info));
	memset(&gvolume_info, 0, sizeof(gvolume_info));
	memset(&pvolume_info, 0, sizeof(pvolume_info));
	memset(&cvolume_info, 0, sizeof(cvolume_info));
	printf("b (1)\n");
	do {
		index++;
		voices = 0;
		present = caps = capture_item = 0;
		min = max = 0;
		sprintf(str, "%s Switch", sname);
		printf("b (2)\n");
		if (test_mixer_id(handle, str, index)) {
			printf("b (3)\n");
			if ((err = get_mixer_info(handle, str, index, &gswitch_info)) < 0)
				return err;
			printf("b (4)\n");
			if (gswitch_info.type == SND_CONTROL_TYPE_BOOLEAN) {
				if (voices < gswitch_info.values_count)
					voices = gswitch_info.values_count;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_GLOBAL_SWITCH;
			}
		}
		printf("b (3)\n");
		sprintf(str, "%s Volume", sname);
		if (test_mixer_id(handle, str, index)) {
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
			}
		}
		sprintf(str, "%s Playback Switch", sname);
		if (test_mixer_id(handle, str, index)) {
			if ((err = get_mixer_info(handle, str, index, &pswitch_info)) < 0)
				return err;
			if (pswitch_info.type == SND_CONTROL_TYPE_BOOLEAN) {
				if (voices < pswitch_info.values_count)
					voices = pswitch_info.values_count;
				caps |= SND_MIXER_SCTCAP_MUTE;
				present |= MIXER_PRESENT_PLAYBACK_SWITCH;
			}
		}
		sprintf(str, "%s Capture Switch", sname);
		if (test_mixer_id(handle, str, index)) {
			if ((err = get_mixer_info(handle, str, index, &cswitch_info)) < 0)
				return err;
			if (cswitch_info.type == SND_CONTROL_TYPE_BOOLEAN) {
				if (voices < cswitch_info.values_count)
					voices = cswitch_info.values_count;
				caps |= SND_MIXER_SCTCAP_CAPTURE;
				present |= MIXER_PRESENT_CAPTURE_SWITCH;
			}
		}
		sprintf(str, "%s Playback Volume", sname);
		if (test_mixer_id(handle, str, index)) {
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
			}
		}
		sprintf(str, "%s Capture Volume", sname);
		if (test_mixer_id(handle, str, index)) {
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
			}
		}
		if (index == 0 && test_mixer_id(handle, "Capture Source", 0)) {
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
				} else for (capture_item = 1; capture_item < csource_info.value.enumerated.items; capture_item++) {
					csource_info.value.enumerated.item = capture_item;
					if ((err = snd_ctl_cinfo(handle->ctl_handle, &csource_info)) < 0)
						return err;
					if (!strcmp(csource_info.value.enumerated.name, str)) {
						if (voices < csource_info.values_count)
							voices = csource_info.values_count;
						caps |= SND_MIXER_SCTCAP_CAPTURE;
						present |= MIXER_PRESENT_CAPTURE_SOURCE;
						break;
					}
				}
			}
		}
		if (voices > 1) {
			caps |= SND_MIXER_SCTCAP_JOINTLY_MUTE | SND_MIXER_SCTCAP_JOINTLY_CAPTURE | SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			if (present & MIXER_PRESENT_GLOBAL_SWITCH) {
				if (gswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_MUTE;
			}
			if (present & MIXER_PRESENT_PLAYBACK_SWITCH) {
				if (pswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_MUTE;
			}
			if (present & MIXER_PRESENT_CAPTURE_SWITCH) {
				if (cswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_CAPTURE;
			}
			if (present & MIXER_PRESENT_GLOBAL_VOLUME) {
				if (gswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			}
			if (present & MIXER_PRESENT_PLAYBACK_VOLUME) {
				if (pswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			}
			if (present & MIXER_PRESENT_CAPTURE_VOLUME) {
				if (cswitch_info.values_count > 1)
					caps &= ~SND_MIXER_SCTCAP_JOINTLY_VOLUME;
			}
		}
		printf("b (4)\n");
		simple = build_input_scontrol(handle, sname, index);
		if (simple == NULL)
			return -ENOMEM;
		simple->present = present;
		simple->gswitch_values = gswitch_info.values_count;
		simple->pswitch_values = pswitch_info.values_count;
		simple->cswitch_values = cswitch_info.values_count;
		simple->gvolume_values = gvolume_info.values_count;
		simple->pvolume_values = pvolume_info.values_count;
		simple->cvolume_values = cvolume_info.values_count;
		simple->ccapture_values = csource_info.values_count;
		simple->capture_item = capture_item;
		simple->caps = caps;
		simple->voices = voices;
		simple->min = min;
		simple->max = max;
		fprintf(stderr, "sname = '%s', index = %i, present = 0x%x, voices = %i\n", sname, index, present, voices);
	} while (present != 0);
	return 0;
}

int snd_mixer_simple_build(snd_mixer_t *handle)
{
	static char *inputs[] = {
		"Master",
		"Master Mono",
		"Master Digital",
		"PCM",
		"Synth",
		"Wave",
		"Music",
		"Line",
		"CD",
		"Mic",
		"Video",
		"Phone",
		"PC Speaker",
		"Aux",
		NULL
	};
	char **input = inputs;
	int err;

	printf("simple build - start\n");
	if ((err = snd_ctl_cbuild(handle->ctl_handle, snd_ctl_csort)) < 0)
		return err;
	while (*input) {
		printf("simple build - input '%s'\n", *input);
		if ((err = build_input(handle, *input)) < 0) {
			snd_mixer_simple_destroy(handle);
			return err;
		}
		input++;
	}
	handle->simple_valid = 1;
	printf("simple build - end\n");
	return 0;
}

int snd_mixer_simple_destroy(snd_mixer_t *handle)
{
	while (handle->simple_first)
		simple_remove(handle, handle->simple_first);
	handle->simple_valid = 0;
	snd_ctl_cfree(handle->ctl_handle);
	return 0;
}
