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
				  snd_mixer_selem_value_t *value);
typedef int (mixer_simple_write_t)(snd_mixer_elem_t *elem,
				   const snd_mixer_selem_value_t *control);
typedef int (mixer_simple_info_t)(snd_mixer_elem_t *elem,
				  snd_mixer_selem_info_t *info);

typedef enum _selem_ctl_type {
  CTL_SINGLE,
  CTL_GLOBAL_SWITCH,
  CTL_GLOBAL_VOLUME,
  CTL_GLOBAL_ROUTE,
  CTL_PLAYBACK_SWITCH,
  CTL_PLAYBACK_VOLUME,
  CTL_PLAYBACK_ROUTE,
  CTL_CAPTURE_SWITCH,
  CTL_CAPTURE_VOLUME,
  CTL_CAPTURE_ROUTE,
  CTL_CAPTURE_SOURCE,
  CTL_LAST = CTL_CAPTURE_SOURCE,
} selem_ctl_type_t;

typedef struct _selem_ctl {
	snd_hctl_elem_t *elem;
	snd_ctl_elem_type_t type;
	unsigned int values;
	long min, max;
} selem_ctl_t;

typedef struct _selem {
	snd_mixer_selem_id_t id;
	selem_ctl_t ctls[CTL_LAST + 1];
	unsigned int capture_item;
	unsigned int caps;
	long min;
	long max;
	unsigned int channels;
	/* -- */
	mixer_simple_read_t *read;
	mixer_simple_write_t *write;
	mixer_simple_info_t *info;
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

static const char *get_short_name(const char *lname)
{
	struct mixer_name_table *p;
	for (p = name_table; p->longname; p++) {
		if (!strcmp(lname, p->longname))
			return p->shortname;
	}
	return lname;
}

#if 0
static const char *get_long_name(const char *sname)
{
	struct mixer_name_table *p;
	for (p = name_table; p->longname; p++) {
		if (!strcmp(sname, p->shortname))
			return p->longname;
	}
	return sname;
}

static const char *simple_elems[] = {
	"Master Mono",
	"Master Digital",
	"Master",
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
	"Mono Output",
	"Mono",
	"Playback",
	"Capture Boost",
	"Capture",
	NULL
};
#endif

static int selem_info(snd_mixer_elem_t *elem,
		      snd_mixer_selem_info_t *info)
{
	selem_t *simple;

	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	simple = elem->private_data;

	info->caps = simple->caps;
	info->channels |= (1 << simple->channels) - 1;
	info->capture_group = 0;
	info->min = simple->min;
	info->max = simple->max;
	return 0;
}

static int elem_read_volume(selem_t *simple, selem_ctl_type_t type,
			    snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->channels; idx++)
		control->volume[idx] = ctl.value.integer.value[simple->ctls[type].values == 1 ? 0 : idx];
	return 0;
}

static int elem_read_mute_switch(selem_t *simple, selem_ctl_type_t type,
				 snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->channels; idx++)
		if (ctl.value.integer.value[simple->ctls[type].values == 1 ? 0 : idx] == 0)
			control->mute |= 1 << idx;
	return 0;
}

static int elem_read_mute_route(selem_t *simple, selem_ctl_type_t type,
				snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->channels; idx++)
		if (ctl.value.integer.value[(idx * simple->ctls[type].values) + idx] == 0)
			control->mute |= 1 << idx;
	return 0;
}

static int elem_read_capture_switch(selem_t *simple, selem_ctl_type_t type,
				    snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->channels; idx++)
		if (ctl.value.integer.value[simple->ctls[type].values == 1 ? 0 : idx])
			control->capture |= 1 << idx;
	return 0;
}

static int elem_read_capture_route(selem_t *simple, selem_ctl_type_t type,
				   snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->channels; idx++)
		if (ctl.value.integer.value[(idx * simple->ctls[type].values) + idx])
			control->capture |= 1 << idx;
	return 0;
}

static int selem_read(snd_mixer_elem_t *elem,
		      snd_mixer_selem_value_t *control)
{
	selem_t *simple;
	unsigned int idx;
	int err;

	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	simple = elem->private_data;

	control->mute = 0;
	control->capture = 0;
	for (idx = 0; idx < 32; idx++)
		control->volume[idx] = 0;

	if (simple->caps & CAP_VOLUME) {
		if (simple->ctls[CTL_PLAYBACK_VOLUME].elem) {
			elem_read_volume(simple, CTL_PLAYBACK_VOLUME, control);
		} else if (simple->ctls[CTL_GLOBAL_VOLUME].elem) {
			elem_read_volume(simple, CTL_GLOBAL_VOLUME, control);
		} else if (simple->ctls[CTL_SINGLE].elem &&
			   simple->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_INTEGER) {
			elem_read_volume(simple, CTL_SINGLE, control);
		}
	}
	if (simple->caps & CAP_MUTE) {
		if (simple->ctls[CTL_PLAYBACK_SWITCH].elem) {
			elem_read_mute_switch(simple, CTL_PLAYBACK_SWITCH, control);
		} else if (simple->ctls[CTL_GLOBAL_SWITCH].elem) {
			elem_read_mute_switch(simple, CTL_GLOBAL_SWITCH, control);
		} else if (simple->ctls[CTL_SINGLE].elem &&
			   simple->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_BOOLEAN) {
			elem_read_mute_switch(simple, CTL_SINGLE, control);
		} else if (simple->ctls[CTL_PLAYBACK_ROUTE].elem) {
			elem_read_mute_route(simple, CTL_PLAYBACK_ROUTE, control);
		} else if (simple->ctls[CTL_GLOBAL_ROUTE].elem) {
			elem_read_mute_route(simple, CTL_GLOBAL_ROUTE, control);
		}
	}
	if (simple->caps & CAP_CAPTURE) {
		if (simple->ctls[CTL_CAPTURE_SWITCH].elem) {
			elem_read_capture_switch(simple, CTL_CAPTURE_SWITCH, control);
		} else if (simple->ctls[CTL_CAPTURE_ROUTE].elem) {
			elem_read_capture_route(simple, CTL_CAPTURE_ROUTE, control);
		} else if (simple->ctls[CTL_CAPTURE_SOURCE].elem) {
			snd_ctl_elem_value_t ctl;
			memset(&ctl, 0, sizeof(ctl));
			err = snd_hctl_elem_read(simple->ctls[CTL_CAPTURE_SOURCE].elem, &ctl);
			assert(err >= 0);
			for (idx = 0; idx < simple->channels; idx++)
				if (snd_ctl_elem_value_get_enumerated(&ctl, simple->ctls[CTL_CAPTURE_SOURCE].values == 1 ? 0 : idx) == simple->capture_item)
					control->capture |= 1 << idx;
		}
	}
	return 0;
}

static int elem_write_volume(selem_t *simple, selem_ctl_type_t type,
			     const snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->ctls[type].values && idx < 32; idx++) {
		ctl.value.integer.value[idx] = control->volume[idx];
		// fprintf(stderr, "ctl.id.name = '%s', volume = %i [%i]\n", ctl.id.name, ctl.value.integer.value[idx], idx);
	}
	if ((err = snd_hctl_elem_write(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_mute_switch(selem_t *simple, selem_ctl_type_t type,
				  const snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->ctls[type].values && idx < 32; idx++)
		ctl.value.integer.value[idx] = (control->mute & (1 << idx)) ? 0 : 1;
	if ((err = snd_hctl_elem_write(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_mute_route(selem_t *simple, selem_ctl_type_t type,
				 const snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->ctls[type].values * simple->ctls[type].values; idx++)
		ctl.value.integer.value[idx] = 0;
	for (idx = 0; idx < simple->ctls[type].values && idx < 32; idx++)
		ctl.value.integer.value[(idx * simple->ctls[type].values) + idx] = (control->mute & (1 << idx)) ? 0 : 1;
	if ((err = snd_hctl_elem_write(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_capture_switch(selem_t *simple, selem_ctl_type_t type,
				     const snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->ctls[type].values && idx < 32; idx++)
		ctl.value.integer.value[idx] = (control->capture & (1 << idx)) ? 1 : 0;
	if ((err = snd_hctl_elem_write(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_capture_route(selem_t *simple, selem_ctl_type_t type,
				    const snd_mixer_selem_value_t *control)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < simple->ctls[type].values * simple->ctls[type].values; idx++)
		ctl.value.integer.value[idx] = 0;
	for (idx = 0; idx < simple->ctls[type].values && idx < 32; idx++)
		ctl.value.integer.value[(idx * simple->ctls[type].values) + idx] = (control->capture & (1 << idx)) ? 1 : 0;
	if ((err = snd_hctl_elem_write(simple->ctls[type].elem, &ctl)) < 0)
		return err;
	return 0;
}

static int selem_write(snd_mixer_elem_t *elem,
		      const snd_mixer_selem_value_t *control)
{
	selem_t *simple;
	unsigned int idx;
	int err;

	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	simple = elem->private_data;

	if (simple->caps & CAP_VOLUME) {
		if (simple->ctls[CTL_PLAYBACK_VOLUME].elem) {
			elem_write_volume(simple, CTL_PLAYBACK_VOLUME, control);
			if (simple->ctls[CTL_CAPTURE_VOLUME].elem)
				elem_write_volume(simple, CTL_CAPTURE_VOLUME, control);
		} else if (simple->ctls[CTL_GLOBAL_VOLUME].elem) {
			elem_write_volume(simple, CTL_GLOBAL_VOLUME, control);
		} else if (simple->ctls[CTL_SINGLE].elem &&
			   simple->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_INTEGER) {
			elem_write_volume(simple, CTL_SINGLE, control);
		}
	}
	if (simple->caps & CAP_MUTE) {
		if (simple->ctls[CTL_PLAYBACK_SWITCH].elem)
			elem_write_mute_switch(simple, CTL_PLAYBACK_SWITCH, control);
		if (simple->ctls[CTL_GLOBAL_SWITCH].elem)
			elem_write_mute_switch(simple, CTL_GLOBAL_SWITCH, control);
		if (simple->ctls[CTL_SINGLE].elem &&
		    simple->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_BOOLEAN)
			elem_write_mute_switch(simple, CTL_SINGLE, control);
		if (simple->ctls[CTL_PLAYBACK_ROUTE].elem)
			elem_write_mute_route(simple, CTL_PLAYBACK_ROUTE, control);
		if (simple->ctls[CTL_GLOBAL_ROUTE].elem)
			elem_write_mute_route(simple, CTL_GLOBAL_ROUTE, control);
	}
	if (simple->caps & CAP_CAPTURE) {
		if (simple->ctls[CTL_CAPTURE_SWITCH].elem) {
			elem_write_capture_switch(simple, CTL_CAPTURE_SWITCH, control);
		} else if (simple->ctls[CTL_CAPTURE_ROUTE].elem) {
			elem_write_capture_route(simple, CTL_CAPTURE_ROUTE, control);
		} else if (simple->ctls[CTL_CAPTURE_SOURCE].elem) {
			snd_ctl_elem_value_t ctl;
			memset(&ctl, 0, sizeof(ctl));
			if ((err = snd_hctl_elem_read(simple->ctls[CTL_CAPTURE_SOURCE].elem, &ctl)) < 0)
				return err;
			for (idx = 0; idx < simple->channels; idx++) {
				if (control->capture & (1 << idx))
					snd_ctl_elem_value_set_enumerated(&ctl, idx, simple->capture_item);
			}
			if ((err = snd_hctl_elem_write(simple->ctls[CTL_CAPTURE_SOURCE].elem, &ctl)) < 0)
				return err;
		}
	}
	return 0;
}

static void selem_free(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	free(s);
}

static int simple_update(snd_mixer_elem_t *melem)
{
	selem_t *simple;
	unsigned int caps, channels;
	long min, max;
	selem_ctl_t *ctl;

	channels = 0;
	caps = 0;
	min = max = 0;
	assert(melem->type == SND_MIXER_ELEM_SIMPLE);
	simple = melem->private_data;
	ctl = &simple->ctls[CTL_SINGLE];
	if (ctl->elem) {
		if (channels < ctl->values)
			channels = ctl->values;
		caps |= ctl->type == SNDRV_CTL_ELEM_TYPE_BOOLEAN ? CAP_MUTE : CAP_VOLUME;
		if (ctl->type == SNDRV_CTL_ELEM_TYPE_INTEGER) {
			if (min > ctl->min)
				min = ctl->min;
			if (max < ctl->max)
				max = ctl->max;
		}
	}
	ctl = &simple->ctls[CTL_GLOBAL_SWITCH];
	if (ctl->elem) {
		if (channels < ctl->values)
			channels = ctl->values;
		caps |= CAP_MUTE;
	}
	ctl = &simple->ctls[CTL_GLOBAL_ROUTE];
	if (ctl->elem) {
		if (channels < 2)
			channels = 2;
		caps |= CAP_MUTE;
	}
	ctl = &simple->ctls[CTL_GLOBAL_VOLUME];
	if (ctl->elem) {
		if (channels < ctl->values)
			channels = ctl->values;
		if (min > ctl->min)
			min = ctl->min;
		if (max < ctl->max)
			max = ctl->max;
		caps |= CAP_VOLUME;
	}
	ctl = &simple->ctls[CTL_PLAYBACK_SWITCH];
	if (ctl->elem) {
		if (channels < ctl->values)
			channels = ctl->values;
		caps |= CAP_MUTE;
	}
	ctl = &simple->ctls[CTL_PLAYBACK_ROUTE];
	if (ctl->elem) {
		if (channels < 2)
			channels = 2;
		caps |= CAP_MUTE;
	}
	ctl = &simple->ctls[CTL_CAPTURE_SWITCH];
	if (ctl->elem) {
		if (channels < ctl->values)
			channels = ctl->values;
		caps |= CAP_CAPTURE;
	}
	ctl = &simple->ctls[CTL_CAPTURE_ROUTE];
	if (ctl->elem) {
		if (channels < 2)
			channels = 2;
		caps |= CAP_CAPTURE;
	}
	ctl = &simple->ctls[CTL_PLAYBACK_VOLUME];
	if (ctl->elem) {
		if (channels < ctl->values)
			channels = ctl->values;
		if (min > ctl->min)
			min = ctl->min;
		if (max < ctl->max)
			max = ctl->max;
		caps |= CAP_VOLUME;
	}
	ctl = &simple->ctls[CTL_CAPTURE_VOLUME];
	if (ctl->elem) {
		if (channels < ctl->values)
			channels = ctl->values;
		if (min > ctl->min)
			min = ctl->min;
		if (max < ctl->max)
			max = ctl->max;
		caps |= CAP_VOLUME;
	}
	ctl = &simple->ctls[CTL_CAPTURE_SOURCE];
	if (ctl->elem) {
		if (channels < ctl->values)
			channels = ctl->values;
		caps |= CAP_CAPTURE;
	}
	if (channels > 32)
		channels = 32;
	if (channels > 1) {
		if ((simple->ctls[CTL_SINGLE].elem &&
		     simple->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_BOOLEAN) ||
		    simple->ctls[CTL_GLOBAL_SWITCH].elem ||
		    simple->ctls[CTL_GLOBAL_ROUTE].elem ||
		    simple->ctls[CTL_PLAYBACK_SWITCH].elem ||
		    simple->ctls[CTL_PLAYBACK_ROUTE].elem)
			caps |= CAP_JOIN_MUTE;
		if (simple->ctls[CTL_CAPTURE_SWITCH].elem ||
		    simple->ctls[CTL_CAPTURE_ROUTE].elem ||
		    simple->ctls[CTL_CAPTURE_SOURCE].elem)
			caps |= CAP_JOIN_CAPTURE;
		if ((simple->ctls[CTL_SINGLE].elem &&
		     simple->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_BOOLEAN) ||
		    simple->ctls[CTL_GLOBAL_VOLUME].elem ||
		    simple->ctls[CTL_PLAYBACK_VOLUME].elem ||
		    simple->ctls[CTL_CAPTURE_VOLUME].elem)
			caps |= CAP_JOIN_VOLUME;
		if (simple->ctls[CTL_SINGLE].elem &&
		    simple->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_BOOLEAN &&
		    simple->ctls[CTL_SINGLE].values > 1)
			caps &= ~CAP_JOIN_MUTE;
		if (simple->ctls[CTL_GLOBAL_SWITCH].elem &&
		    simple->ctls[CTL_GLOBAL_SWITCH].values > 1)
			caps &= ~CAP_JOIN_MUTE;
		if (simple->ctls[CTL_PLAYBACK_SWITCH].elem &&
		    simple->ctls[CTL_PLAYBACK_SWITCH].values > 1)
			caps &= ~CAP_JOIN_MUTE;
		if (simple->ctls[CTL_GLOBAL_ROUTE].elem ||
		    simple->ctls[CTL_PLAYBACK_ROUTE].elem)
			caps &= ~CAP_JOIN_MUTE;
		if (simple->ctls[CTL_CAPTURE_SWITCH].elem &&
		    simple->ctls[CTL_CAPTURE_SWITCH].values > 1)
			caps &= ~CAP_JOIN_CAPTURE;
		if (simple->ctls[CTL_CAPTURE_ROUTE].elem)
			caps &= ~CAP_JOIN_CAPTURE;
		if (simple->ctls[CTL_SINGLE].elem &&
		    simple->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_INTEGER &&
		    simple->ctls[CTL_SINGLE].values > 1)
			caps &= ~CAP_JOIN_VOLUME;
		if (simple->ctls[CTL_GLOBAL_VOLUME].elem &&
		    simple->ctls[CTL_GLOBAL_VOLUME].values > 1)
				caps &= ~CAP_JOIN_VOLUME;
		if (simple->ctls[CTL_PLAYBACK_VOLUME].elem &&
		    simple->ctls[CTL_PLAYBACK_VOLUME].values > 1)
			caps &= ~CAP_JOIN_VOLUME;
		if (simple->ctls[CTL_CAPTURE_VOLUME].elem &&
		    simple->ctls[CTL_CAPTURE_VOLUME].values > 1)
			caps &= ~CAP_JOIN_VOLUME;
	}
	simple->caps = caps;
	simple->channels = channels;
	simple->min = min;
	simple->max = max;
	return 0;
}	   

static struct suf {
	const char *suffix;
	selem_ctl_type_t type;
} suffixes[] = {
	{ " Playback Switch", CTL_PLAYBACK_SWITCH},
	{" Playback Route", CTL_PLAYBACK_ROUTE},
	{" Playback Volume", CTL_PLAYBACK_VOLUME},
	{" Capture Switch", CTL_CAPTURE_SWITCH},
	{" Capture Route", CTL_CAPTURE_ROUTE},
	{" Capture Volume", CTL_CAPTURE_VOLUME},
	{" Switch", CTL_GLOBAL_SWITCH},
	{" Route", CTL_GLOBAL_ROUTE},
	{" Volume", CTL_GLOBAL_VOLUME},
	{NULL, 0}
};

/* Return base length or 0 on failure */
static int base_len(const char *name, selem_ctl_type_t *type)
{
	struct suf *p;
	size_t nlen = strlen(name);
	p = suffixes;
	while (p->suffix) {
		size_t slen = strlen(p->suffix);
		size_t l;
		if (nlen > slen) {
			l = nlen - slen;
			if (strncmp(name + l, p->suffix, slen) == 0) {
				*type = p->type;
				return l;
			}
		}
		p++;
	}
	return 0;
}

int simple_add1(snd_mixer_class_t *class, const char *name,
		snd_hctl_elem_t *helem, selem_ctl_type_t type,
		int value)
{
	snd_mixer_elem_t *melem;
	snd_mixer_selem_id_t id;
	int new = 0;
	int err;
	snd_ctl_elem_info_t info;
	selem_t *simple;
	const char *name1;
	memset(&info, 0, sizeof(info));
	err = snd_hctl_elem_info(helem, &info);
	assert(err >= 0);
	switch (type) {
	case CTL_SINGLE:
		if (info.type != SND_CTL_ELEM_TYPE_BOOLEAN &&
		    info.type != SND_CTL_ELEM_TYPE_INTEGER)
			return 0;
		break;
	case CTL_GLOBAL_ROUTE:
	case CTL_PLAYBACK_ROUTE:
	case CTL_CAPTURE_ROUTE:
		if (info.count != 4 ||
		    info.type != SND_CTL_ELEM_TYPE_BOOLEAN)
			return 0;
		break;
	case CTL_GLOBAL_SWITCH:
	case CTL_PLAYBACK_SWITCH:
	case CTL_CAPTURE_SWITCH:
		if (info.type != SND_CTL_ELEM_TYPE_BOOLEAN)
			return 0;
		break;
	case CTL_GLOBAL_VOLUME:
	case CTL_PLAYBACK_VOLUME:
	case CTL_CAPTURE_VOLUME:
		if (info.type != SND_CTL_ELEM_TYPE_INTEGER)
			return 0;
		break;
	case CTL_CAPTURE_SOURCE:
		if (info.type != SND_CTL_ELEM_TYPE_ENUMERATED)
			return 0;
		break;
	default:
		assert(0);
		break;
	}
	name1 = get_short_name(name);
	strncpy(id.name, name1, sizeof(id.name));
	id.index = snd_hctl_elem_get_index(helem);
	melem = snd_mixer_find_selem(class->mixer, &id);
	if (!melem) {
		simple = calloc(1, sizeof(*simple));
		if (!simple)
			return -ENOMEM;
		melem = calloc(1, sizeof(*melem));
		if (!melem) {
			free(simple);
			return -ENOMEM;
		}
		simple->id = id;
		simple->read = selem_read;
		simple->write = selem_write;
		simple->info = selem_info;
		melem->type = SND_MIXER_ELEM_SIMPLE;
		melem->private_data = simple;
		melem->private_free = selem_free;
		INIT_LIST_HEAD(&melem->helems);
		new = 1;
	} else {
		simple = melem->private_data;
	}
	assert(!simple->ctls[type].elem);
	simple->ctls[type].elem = helem;
	simple->ctls[type].type = info.type;
	simple->ctls[type].values = info.count;
	simple->ctls[type].min = info.value.integer.min;
	simple->ctls[type].max = info.value.integer.max;
	switch (type) {
	case CTL_CAPTURE_SOURCE:
		simple->capture_item = value;
		break;
	default:
		break;
	}
	err = snd_mixer_elem_attach(melem, helem);
	if (err < 0)
		return err;
	err = simple_update(melem);
	assert(err >= 0);
	if (new)
		return snd_mixer_elem_add(melem, class);
	return snd_mixer_elem_change(melem);
}

int simple_event_add(snd_mixer_class_t *class, snd_hctl_elem_t *helem)
{
	const char *name = snd_hctl_elem_get_name(helem);
	size_t len;
	selem_ctl_type_t type;
	if (snd_hctl_elem_get_interface(helem) != SND_CTL_ELEM_IFACE_MIXER)
		return 0;
	if (strcmp(name, "Capture Source") == 0) {
		snd_ctl_elem_info_t *info;
		unsigned int k, items;
		int err;
		snd_ctl_elem_info_alloca(&info);
		err = snd_hctl_elem_info(helem, info);
		assert(err >= 0);
		if (snd_ctl_elem_info_get_type(info) != SND_CTL_ELEM_TYPE_ENUMERATED)
			return 0;
		items = snd_ctl_elem_info_get_items(info);
		for (k = 0; k < items; ++k) {
			const char *n;
			snd_ctl_elem_info_set_item(info, k);
			err = snd_hctl_elem_info(helem, info);
			assert(err >= 0);
			n = snd_ctl_elem_info_get_item_name(info);
#if 0
			/* FIXME: es18xx has both Mix and Master */
			if (strcmp(n, "Mix") == 0)
				n = "Master";
			else if (strcmp(n, "Mono Mix") == 0)
				n = "Master Mono";
#endif
			err = simple_add1(class, n, helem, CTL_CAPTURE_SOURCE, k);
			if (err < 0)
				return err;
		}
		return 0;
	}
	len = base_len(name, &type);
	if (len == 0) {
		return simple_add1(class, name, helem, CTL_SINGLE, 0);
	} else {
		char ename[128];
		if (len >= sizeof(ename))
			len = sizeof(ename) - 1;
		memcpy(ename, name, len);
		ename[len] = 0;
		return simple_add1(class, ename, helem, type, 0);
	}
}

int simple_event_remove(snd_hctl_elem_t *helem,
			snd_mixer_elem_t *melem)
{
	selem_t *simple = melem->private_data;
	int err;
	int k;
	for (k = 0; k <= CTL_LAST; k++) {
		if (simple->ctls[k].elem == helem)
			break;
	}
	assert(k <= CTL_LAST);
	simple->ctls[k].elem = NULL;
	err = snd_mixer_elem_detach(melem, helem);
	assert(err >= 0);
	if (snd_mixer_elem_empty(melem))
		return snd_mixer_elem_remove(melem);
	err = simple_update(melem);
	return snd_mixer_elem_change(melem);
}

int simple_event_info(snd_mixer_elem_t *melem)
{
	int err = simple_update(melem);
	assert(err >= 0);
	return snd_mixer_elem_change(melem);
}

int simple_event(snd_mixer_class_t *class, snd_ctl_event_type_t event,
		 snd_hctl_elem_t *helem, snd_mixer_elem_t *melem)
{
	switch (event) {
	case SND_CTL_EVENT_ADD:
		return simple_event_add(class, helem);
	case SND_CTL_EVENT_INFO:
		return simple_event_info(melem);
	case SND_CTL_EVENT_VALUE:
		return snd_mixer_elem_throw_event(melem, event);
	case SND_CTL_EVENT_REMOVE:
		return simple_event_remove(helem, melem);
	default:
		assert(0);
		break;
	}
	return 0;
}

int snd_mixer_selem_register(snd_mixer_t *mixer, snd_mixer_class_t **classp)
{
	snd_mixer_class_t *class = calloc(1, sizeof(*class));
	int err;
	if (!class)
		return -ENOMEM;
	class->event = simple_event;
	err = snd_mixer_class_register(class, mixer);
	if (err < 0) {
		free(class);
		return err;
	}
	if (classp)
		*classp = class;
	return 0;
}
	
snd_mixer_elem_t *snd_mixer_find_selem(snd_mixer_t *mixer,
				       const snd_mixer_selem_id_t *id)
{
	struct list_head *list, *next;
	list_for_each(list, next, &mixer->elems) {
		snd_mixer_elem_t *e;
		selem_t *s;
		e = list_entry(list, snd_mixer_elem_t, list);
		if (e->type != SND_MIXER_ELEM_SIMPLE)
			continue;
		s = e->private_data;
		if (!strcmp(s->id.name, id->name) && s->id.index == id->index)
			return e;
	}
	return NULL;
}

void snd_mixer_selem_get_id(snd_mixer_elem_t *elem,
			    snd_mixer_selem_id_t *id)
{
	selem_t *s;
	assert(elem && id);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	*id = s->id;
}

int snd_mixer_selem_info(snd_mixer_elem_t *elem,
			 snd_mixer_selem_info_t *info)
{
	selem_t *s;
	assert(elem && info);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->info);
	return s->info(elem, info);
}

int snd_mixer_selem_read(snd_mixer_elem_t *elem,
			 snd_mixer_selem_value_t *value)
{
	selem_t *s;
	assert(elem && value);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->read);
	return s->read(elem, value);
}

int snd_mixer_selem_write(snd_mixer_elem_t *elem,
			  const snd_mixer_selem_value_t *value)
{
	selem_t *s;
	assert(elem && value);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->write);
	return s->write(elem, value);
}

int snd_mixer_selem_info_is_mono(const snd_mixer_selem_info_t *obj)
{
	assert(obj);
	return obj->channels == 1 << SND_MIXER_SCHN_MONO;
}

int snd_mixer_selem_info_has_channel(const snd_mixer_selem_info_t *obj, snd_mixer_selem_channel_id_t channel)
{
	assert(obj);
	assert(channel <= SND_MIXER_SCHN_LAST);
	return !!(obj->channels & (1 << channel));
}

long snd_mixer_selem_value_get_volume(const snd_mixer_selem_value_t *obj, snd_mixer_selem_channel_id_t channel)
{
	assert(obj);
	return obj->volume[channel];
}

void snd_mixer_selem_value_set_volume(snd_mixer_selem_value_t *obj, snd_mixer_selem_channel_id_t channel, long value)
{
	assert(obj);
	obj->volume[channel] = value;
}

int snd_mixer_selem_value_get_mute(const snd_mixer_selem_value_t *obj, snd_mixer_selem_channel_id_t channel)
{
	assert(obj);
	assert(channel <= SND_MIXER_SCHN_LAST);
	return !!(obj->mute & (1 << channel));
}

void snd_mixer_selem_value_set_mute(snd_mixer_selem_value_t *obj, snd_mixer_selem_channel_id_t channel, int mute)
{
	assert(obj);
	assert(channel <= SND_MIXER_SCHN_LAST);
	if (mute)
		obj->mute |= (1 << channel);
	else
		obj->mute &= ~(1 << channel);
}

int snd_mixer_selem_value_get_capture(const snd_mixer_selem_value_t *obj, snd_mixer_selem_channel_id_t channel)
{
	assert(obj);
	assert(channel <= SND_MIXER_SCHN_LAST);
	return !!(obj->capture & (1 << channel));
}

void snd_mixer_selem_value_set_capture(snd_mixer_selem_value_t *obj, snd_mixer_selem_channel_id_t channel, int capture)
{
	assert(obj);
	assert(channel <= SND_MIXER_SCHN_LAST);
	if (capture)
		obj->capture |= (1 << channel);
	else
		obj->capture &= ~(1 << channel);
}

void snd_mixer_selem_value_set_mute_all(snd_mixer_selem_value_t *obj, int mute)
{
	assert(obj);
	if (mute)
		obj->mute = ~0U;
	else
		obj->mute = 0;
}

void snd_mixer_selem_value_set_capture_all(snd_mixer_selem_value_t *obj, int capture)
{
	assert(obj);
	if (capture)
		obj->capture = ~0U;
	else
		obj->capture = 0;
}

void snd_mixer_selem_value_set_volume_all(snd_mixer_selem_value_t *obj, long value)
{
	unsigned int c;
	assert(obj);
	for (c = 0; c < sizeof(obj->volume) / sizeof(obj->volume[0]); c++)
		obj->volume[c] = value;
}

const char *snd_mixer_selem_channel_name(snd_mixer_selem_channel_id_t channel)
{
	static char *array[snd_enum_to_int(SND_MIXER_SCHN_LAST) + 1] = {
		[SND_MIXER_SCHN_FRONT_LEFT] = "Front Left",
		[SND_MIXER_SCHN_FRONT_RIGHT] = "Front Right",
		[SND_MIXER_SCHN_FRONT_CENTER] = "Front Center",
		[SND_MIXER_SCHN_REAR_LEFT] = "Rear Left",
		[SND_MIXER_SCHN_REAR_RIGHT] = "Rear Right",
		[SND_MIXER_SCHN_WOOFER] = "Woofer"
	};
	char *p;
	assert(channel <= SND_MIXER_SCHN_LAST);
	p = array[snd_enum_to_int(channel)];
	if (!p)
		return "?";
	return p;
}

