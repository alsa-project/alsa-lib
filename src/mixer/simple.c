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
	snd_control_t c;
	int err;
	
	memset(&c, 0, sizeof(c));
	c.id.iface = SND_CONTROL_IFACE_MIXER;
	strcpy(c.id.name, name);
	c.id.index = index;
	err = snd_ctl_cread(handle->ctl_handle, &c);
	fprintf(stderr, "Looking for control: '%s', %i (%i)\n", name, index, err);
	switch (err) {
	case 0:
	case -EBUSY:
		return 1;
	default:
		return 0;
	}
}

static int get_mixer_info(snd_mixer_t *handle, const char *name, int index, snd_control_info_t *info)
{
	memset(info, 0, sizeof(*info));
	info->id.iface = SND_CONTROL_IFACE_MIXER;
	strcpy(info->id.name, name);
	info->id.index = index;
	return snd_ctl_cinfo(handle->ctl_handle, info);
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

static int input_get(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control)
{
	char str[128];
	int mute_is_valid = 0;
	snd_control_info_t mute_info;
	int volume_is_valid = 0;
	snd_control_info_t volume_info;
	int capture_is_valid = 0, capture1_is_valid = 0;
	unsigned int capture1_item = 0;
	snd_control_info_t capture_info;
	unsigned int voices = 0, idx;
	snd_control_t ctl;
	int err;

	if (simple == NULL)
		return -EINVAL;

	control->caps = 0;
	control->channels = 0;
	control->mute = 0;
	control->capture = 0;
	control->capture_group = 0;
	control->min = 0;
	control->max = 0;
	for (idx = 0; idx < 32; idx++)
		control->volume.values[idx] = 0;

	sprintf(str, "%s Mute", control->sid.name);
	if (test_mixer_id(handle, str, control->sid.index)) {
		if ((err = get_mixer_info(handle, str, control->sid.index, &mute_info)) < 0)
			return err;
		if (mute_info.type == SND_CONTROL_TYPE_BOOLEAN) {
			if (voices < mute_info.values_count)
				voices = mute_info.values_count;
			mute_is_valid++;
			control->caps |= SND_MIXER_SCTCAP_MUTE;
		}
	}
	sprintf(str, "%s Volume", control->sid.name);
	if (test_mixer_id(handle, str, control->sid.index)) {
		if ((err = get_mixer_info(handle, str, control->sid.index, &volume_info)) < 0)
			return err;
		if (volume_info.type == SND_CONTROL_TYPE_INTEGER) {
			if (voices < volume_info.values_count)
				voices = volume_info.values_count;
			volume_is_valid++;
			control->caps |= SND_MIXER_SCTCAP_VOLUME;
			control->min = volume_info.value.integer.min;
			control->max = volume_info.value.integer.max;
		}
	}
	sprintf(str, "%s Capture", control->sid.name);
	if (test_mixer_id(handle, str, control->sid.index)) {
		if ((err = get_mixer_info(handle, str, control->sid.index, &capture_info)) < 0)
			return err;
		if (capture_info.type == SND_CONTROL_TYPE_BOOLEAN) {
			if (voices < capture_info.values_count)
				voices = capture_info.values_count;
			capture_is_valid++;
			control->caps |= SND_MIXER_SCTCAP_CAPTURE;
		}
	} else if (test_mixer_id(handle, "Capture Source", 0)) {
		if ((err = get_mixer_info(handle, "Capture Source", 0, &capture_info)) < 0)
			return err;
		strcpy(str, control->sid.name);
		if (!strcmp(str, "Master"))	/* special case */
			strcpy(str, "Mix");
		else if (!strcmp(str, "Master Mono")) /* special case */
			strcpy(str, "Mono Mix");
		if (capture_info.type == SND_CONTROL_TYPE_ENUMERATED) {
			if (!strcmp(capture_info.value.enumerated.name, str)) {
				if (voices < capture_info.values_count)
					voices = capture_info.values_count;
				capture1_is_valid++;
				control->caps |= SND_MIXER_SCTCAP_CAPTURE | SND_MIXER_SCTCAP_EXCL_CAPTURE;
				control->capture_group = 1;
			}
			for (capture1_item = 1; capture1_item < capture_info.value.enumerated.items; capture1_item++) {
				capture_info.value.enumerated.item = capture1_item;
				if ((err = snd_ctl_cinfo(handle->ctl_handle, &capture_info)) < 0)
					return err;
				if (!strcmp(capture_info.value.enumerated.name, str)) {
					if (voices < capture_info.values_count)
						voices = capture_info.values_count;
					capture1_is_valid++;
					control->caps |= SND_MIXER_SCTCAP_CAPTURE | SND_MIXER_SCTCAP_EXCL_CAPTURE;
					control->capture_group = 1;
					break;
				}
			}
		}
	}

	for (idx = 0; idx < voices && idx < 32; idx++)
		control->channels |= 1 << idx;
	if (voices > 1) {
		if (volume_is_valid && volume_info.values_count == 1)
			control->caps |= SND_MIXER_SCTCAP_JOINTLY_VOLUME;
		if (mute_is_valid && mute_info.values_count == 1)
			control->caps |= SND_MIXER_SCTCAP_JOINTLY_MUTE;
		if ((capture_is_valid || capture1_is_valid) && capture_info.values_count == 1)
			control->caps |= SND_MIXER_SCTCAP_JOINTLY_CAPTURE;
	}
	if (volume_is_valid) {
		memset(&ctl, 0, sizeof(ctl));
		ctl.id = volume_info.id;
		if ((err = snd_ctl_cread(handle->ctl_handle, &ctl)) < 0)
			return err;
		for (idx = 0; idx < voices && idx < 32; idx++)
			control->volume.values[idx] = ctl.value.integer.value[volume_info.values_count == 1 ? 0 : idx];
	}
	if (mute_is_valid) {
		memset(&ctl, 0, sizeof(ctl));
		ctl.id = mute_info.id;
		if ((err = snd_ctl_cread(handle->ctl_handle, &ctl)) < 0)
			return err;
		for (idx = 0; idx < voices && idx < 32; idx++)
			if (ctl.value.integer.value[mute_info.values_count == 1 ? 0 : idx])
				control->mute |= 1 << idx;
	}
	if (capture_is_valid) {
		memset(&ctl, 0, sizeof(ctl));
		ctl.id = capture_info.id;
		if ((err = snd_ctl_cread(handle->ctl_handle, &ctl)) < 0)
			return err;
		for (idx = 0; idx < voices && idx < 32; idx++)
			if (ctl.value.integer.value[capture_info.values_count == 1 ? 0 : idx])
				control->capture |= 1 << idx;
	} else if (capture1_is_valid) {
		memset(&ctl, 0, sizeof(ctl));
		ctl.id = capture_info.id;
		if ((err = snd_ctl_cread(handle->ctl_handle, &ctl)) < 0)
			return err;
		for (idx = 0; idx < voices && idx < 32; idx++)
			if (ctl.value.enumerated.item[capture_info.values_count == 1 ? 0 : idx] == capture1_item)
				control->capture |= 1 << idx;
	}

	return 0;
}

static int input_put(snd_mixer_t *handle, mixer_simple_t *simple, snd_mixer_simple_control_t *control)
{
	char str[128];
	int mute_is_valid = 0;
	snd_control_info_t mute_info;
	int volume_is_valid = 0;
	snd_control_info_t volume_info;
	int capture_is_valid = 0, capture1_is_valid = 0;
	unsigned int capture1_item = 0;
	snd_control_info_t capture_info;
	unsigned int voices = 0, idx;
	snd_control_t ctl_mute, ctl_volume, ctl_capture;
	int err;

	if (simple == NULL)
		return -EINVAL;

	sprintf(str, "%s Mute", control->sid.name);
	if (test_mixer_id(handle, str, control->sid.index)) {
		if ((err = get_mixer_info(handle, str, control->sid.index, &mute_info)) < 0)
			return err;
		if (mute_info.type == SND_CONTROL_TYPE_BOOLEAN) {
			if (voices < mute_info.values_count)
				voices = mute_info.values_count;
			mute_is_valid++;
		}
	}
	sprintf(str, "%s Volume", control->sid.name);
	if (test_mixer_id(handle, str, control->sid.index)) {
		if ((err = get_mixer_info(handle, str, control->sid.index, &volume_info)) < 0)
			return err;
		if (volume_info.type == SND_CONTROL_TYPE_INTEGER) {
			if (voices < volume_info.values_count)
				voices = volume_info.values_count;
			volume_is_valid++;
		}
	}
	sprintf(str, "%s Capture", control->sid.name);
	if (test_mixer_id(handle, str, control->sid.index)) {
		if ((err = get_mixer_info(handle, str, control->sid.index, &capture_info)) < 0)
			return err;
		if (capture_info.type == SND_CONTROL_TYPE_BOOLEAN) {
			if (voices < capture_info.values_count)
				voices = capture_info.values_count;
			capture_is_valid++;
		}
	} else if (test_mixer_id(handle, "Capture Source", 0)) {
		if ((err = get_mixer_info(handle, "Capture Source", 0, &capture_info)) < 0)
			return err;
		strcpy(str, control->sid.name);
		if (!strcmp(str, "Master"))	/* special case */
			strcpy(str, "Mix");
		else if (!strcmp(str, "Master Mono")) /* special case */
			strcpy(str, "Mono Mix");
		if (capture_info.type == SND_CONTROL_TYPE_ENUMERATED) {
			if (!strcmp(capture_info.value.enumerated.name, str)) {
				if (voices < capture_info.values_count)
					voices = capture_info.values_count;
				capture1_is_valid++;
			}
			for (capture1_item = 1; capture1_item < capture_info.value.enumerated.items; capture1_item++) {
				capture_info.value.enumerated.item = capture1_item;
				if ((err = snd_ctl_cinfo(handle->ctl_handle, &capture_info)) < 0)
					return err;
				if (!strcmp(capture_info.value.enumerated.name, str)) {
					if (voices < capture_info.values_count)
						voices = capture_info.values_count;
					capture1_is_valid++;
					break;
				}
			}
		}
	}

	memset(&ctl_mute, 0, sizeof(ctl_mute));
	memset(&ctl_volume, 0, sizeof(ctl_volume));
	memset(&ctl_capture, 0, sizeof(ctl_capture));
	if (mute_is_valid) {
		ctl_mute.id = mute_info.id;
		if ((err = snd_ctl_cread(handle->ctl_handle, &ctl_mute)) < 0)
			return err;
	}
	if (volume_is_valid) {
		ctl_volume.id = volume_info.id;
		if ((err = snd_ctl_cread(handle->ctl_handle, &ctl_volume)) < 0)
			return err;
	}
	if (capture_is_valid || capture1_is_valid) {
		ctl_capture.id = capture_info.id;
		if ((err = snd_ctl_cread(handle->ctl_handle, &ctl_capture)) < 0)
			return err;
	}
	for (idx = 0; idx < voices && idx < 32; idx++) {
		if (control->channels & (1 << idx)) {
			if (volume_is_valid) {
				if (control->volume.values[idx] < volume_info.value.integer.min ||
				    control->volume.values[idx] > volume_info.value.integer.max)
					return -EINVAL;
				ctl_volume.value.integer.value[idx] = control->volume.values[idx];
			}
			if (mute_is_valid) {
				ctl_mute.value.integer.value[idx] = (control->mute & (1 << idx)) ? 1 : 0;
			}
			if (capture_is_valid) {
				ctl_capture.value.integer.value[idx] = (control->capture & (1 << idx)) ? 1 : 0;
			} else if (capture1_is_valid && (control->capture & (1 << idx))) {
				ctl_capture.value.enumerated.item[idx] = capture1_item;
			}
		}
	}
	if (volume_is_valid) {
		if ((err = snd_ctl_cwrite(handle->ctl_handle, &ctl_volume)) < 0)
			return err;
	}
	if (mute_is_valid) {
		if ((err = snd_ctl_cwrite(handle->ctl_handle, &ctl_mute)) < 0)
			return err;
	}
	if (capture_is_valid || capture1_is_valid) {
		if ((err = snd_ctl_cwrite(handle->ctl_handle, &ctl_capture)) < 0)
			return err;
	}
	return 0;
}

static int build_input_scontrol(snd_mixer_t *handle, const char *sname, int index)
{
	mixer_simple_t s;

	memset(&s, 0, sizeof(s));
	strcpy(s.id.name, sname);
	s.id.index = index;
	s.get = input_get;
	s.put = input_put;
	return simple_add(handle, simple_new(&s));
}

static int build_input(snd_mixer_t *handle, const char *sname)
{
	char str[128];

	fprintf(stderr, "build_input: '%s'\n", sname);
	sprintf(str, "%s Mute", sname);
	if (test_mixer_id(handle, str, 0)) {
		fprintf(stderr, "id ok (mute): %s\n", str);
		return build_input_scontrol(handle, sname, 0);
	}
	sprintf(str, "%s Volume", sname);
	if (test_mixer_id(handle, str, 0))
		return build_input_scontrol(handle, sname, 0);
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
	while (handle->simple_first)
		simple_remove(handle, handle->simple_first);
	handle->simple_valid = 0;
	return 0;
}
