/*
 * \file mixer/simple.c
 * \brief Mixer Simple Element Class Interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2001
 *
 * Mixer simple element class interface.
 */
/*
 *  Mixer Interface - simple controls
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>
#include "mixer_local.h"

#ifndef DOC_HIDDEN

#define CAP_GVOLUME		(1<<1)
#define CAP_GSWITCH		(1<<2)
#define CAP_PVOLUME		(1<<3)
#define CAP_PVOLUME_JOIN	(1<<4)
#define CAP_PSWITCH		(1<<5)
#define CAP_PSWITCH_JOIN	(1<<6)
#define CAP_CVOLUME		(1<<7)
#define CAP_CVOLUME_JOIN	(1<<8)
#define CAP_CSWITCH		(1<<9)
#define CAP_CSWITCH_JOIN	(1<<10)
#define CAP_CSWITCH_EXCL	(1<<11)

typedef struct _mixer_simple mixer_simple_t;

#define PLAY 0
#define CAPT 1

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
	unsigned int access;
	unsigned int values;
	long min, max;
} selem_ctl_t;

typedef struct _selem {
	snd_mixer_selem_id_t id;
	selem_ctl_t ctls[CTL_LAST + 1];
	unsigned int capture_item;
	unsigned int capture_group;
	unsigned int caps;
	struct {
		unsigned int range: 1;	/* Forced range */
		long min, max;
		unsigned int channels;
		long vol[32];
		unsigned int sw;
	} str[2];
} selem_t;

static struct mixer_name_table {
	const char *longname;
	const char *shortname;
} name_table[] = {
	{"Tone Control - Switch", "Tone"},
	{"Tone Control - Bass", "Bass"},
	{"Tone Control - Treble", "Treble"},
	{"Synth Tone Control - Switch", "Synth Tone"},
	{"Synth Tone Control - Bass", "Synth Bass"},
	{"Synth Tone Control - Treble", "Synth Treble"},
	{0, 0},
};

#endif

static const char *get_short_name(const char *lname)
{
	struct mixer_name_table *p;
	for (p = name_table; p->longname; p++) {
		if (!strcmp(lname, p->longname))
			return p->shortname;
	}
	return lname;
}

static int compare_mixer_priority_lookup(const char **name, const char * const *names, int coef)
{
	int res;

	for (res = 0; *names; names++, res += coef) {
		if (!strncmp(*name, *names, strlen(*names))) {
			*name += strlen(*names);
			if (**name == ' ')
				(*name)++;
			return res+1;
		}
	}
	return MIXER_COMPARE_WEIGHT_NOT_FOUND;
}

static int get_compare_weight(const char *name, unsigned int idx)
{
	static const char *names[] = {
		"Master",
		"Headphone",
		"Tone",
		"Bass",
		"Treble",
		"3D Control",
		"PCM",
		"Surround",
		"Center",
		"LFE",
		"Synth",
		"FM",
		"Wave",
		"Music",
		"DSP",
		"Line",
		"CD",
		"Mic",
		"Video",
		"Zoom Video",
		"Phone",
		"I2S",
		"IEC958",
		"PC Speaker",
		"Aux",
		"Mono",
		"Playback",
		"Capture",
		"Mix",
		NULL
	};
	static const char *names1[] = {
		"-",
		NULL,
	};
	static const char *names2[] = {
		"Mono",
		"Digital",
		"Switch",
		"Depth",
		"Wide",
		"Space",
		"Level",
		"Center",
		"Output",
		"Boost",
		"Tone",
		"Bass",
		"Treble",
		NULL,
	};
	const char *name1;
	int res, res1;

	if ((res = compare_mixer_priority_lookup((const char **)&name, names, 1000)) == MIXER_COMPARE_WEIGHT_NOT_FOUND)
		return MIXER_COMPARE_WEIGHT_NOT_FOUND;
	if (*name == '\0')
		goto __res;
	for (name1 = name; *name1 != '\0'; name1++);
	for (name1--; name1 != name && *name1 != ' '; name1--);
	while (name1 != name && *name1 == ' ')
		name1--;
	if (name1 != name) {
		for (; name1 != name && *name1 != ' '; name1--);
		name = name1;
		if ((res1 = compare_mixer_priority_lookup((const char **)&name, names1, 200)) == MIXER_COMPARE_WEIGHT_NOT_FOUND)
			return res;
		res += res1;
	} else {
		name = name1;
	}
	if ((res1 = compare_mixer_priority_lookup((const char **)&name, names2, 20)) == MIXER_COMPARE_WEIGHT_NOT_FOUND)
		return res;
      __res:
	return MIXER_COMPARE_WEIGHT_SIMPLE_BASE + res + idx;
}

static long to_user(selem_t *s, int dir, selem_ctl_t *c, long value)
{
	int64_t n = (int64_t) (value - c->min) * (s->str[dir].max - s->str[dir].min);
	return s->str[dir].min + (n + (c->max - c->min) / 2) / (c->max - c->min);
}

static long from_user(selem_t *s, int dir, selem_ctl_t *c, long value)
{
	int64_t n = (int64_t) (value - s->str[dir].min) * (c->max - c->min);
	return c->min + (n + (s->str[dir].max - s->str[dir].min) / 2) / (s->str[dir].max - s->str[dir].min);
}

static int elem_read_volume(selem_t *s, int dir, selem_ctl_type_t type)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	selem_ctl_t *c = &s->ctls[type];
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(c->elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < s->str[dir].channels; idx++) {
		unsigned int idx1 = idx;
		if (idx >= c->values)
			idx1 = 0;
		s->str[dir].vol[idx] = to_user(s, dir, c, ctl.value.integer.value[idx1]);
	}
	return 0;
}

static int elem_read_switch(selem_t *s, int dir, selem_ctl_type_t type)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	selem_ctl_t *c = &s->ctls[type];
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(c->elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < s->str[dir].channels; idx++) {
		unsigned int idx1 = idx;
		if (idx >= c->values)
			idx1 = 0;
		if (!ctl.value.integer.value[idx1])
			s->str[dir].sw &= ~(1 << idx);
	}
	return 0;
}

static int elem_read_route(selem_t *s, int dir, selem_ctl_type_t type)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	selem_ctl_t *c = &s->ctls[type];
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(c->elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < s->str[dir].channels; idx++) {
		unsigned int idx1 = idx;
		if (idx >= c->values)
			idx1 = 0;
		if (!ctl.value.integer.value[idx1 * c->values + idx1])
			s->str[dir].sw &= ~(1 << idx);
	}
	return 0;
}

static int selem_read(snd_mixer_elem_t *elem)
{
	selem_t *s;
	unsigned int idx;
	int err = 0;
	long pvol[32], cvol[32];
	unsigned int psw, csw;

	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;

	memcpy(pvol, s->str[PLAY].vol, sizeof(pvol));
	memset(&s->str[PLAY].vol, 0, sizeof(s->str[PLAY].vol));
	psw = s->str[PLAY].sw;
	s->str[PLAY].sw = ~0U;
	memcpy(cvol, s->str[CAPT].vol, sizeof(cvol));
	memset(&s->str[CAPT].vol, 0, sizeof(s->str[CAPT].vol));
	csw = s->str[CAPT].sw;
	s->str[CAPT].sw = ~0U;

	if (s->ctls[CTL_PLAYBACK_VOLUME].elem)
		err = elem_read_volume(s, PLAY, CTL_PLAYBACK_VOLUME);
	else if (s->ctls[CTL_GLOBAL_VOLUME].elem)
		err = elem_read_volume(s, PLAY, CTL_GLOBAL_VOLUME);
	else if (s->ctls[CTL_SINGLE].elem &&
		 s->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_INTEGER)
		err = elem_read_volume(s, PLAY, CTL_SINGLE);
	if (err < 0)
		return err;

	if ((s->caps & (CAP_GSWITCH|CAP_PSWITCH)) == 0) {
		s->str[PLAY].sw = 0;
		goto __skip_pswitch;
	}
	if (s->ctls[CTL_PLAYBACK_SWITCH].elem) {
		err = elem_read_switch(s, PLAY, CTL_PLAYBACK_SWITCH);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_GLOBAL_SWITCH].elem) {
		err = elem_read_switch(s, PLAY, CTL_GLOBAL_SWITCH);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_SINGLE].elem &&
	    s->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_BOOLEAN) {
		err = elem_read_switch(s, PLAY, CTL_SINGLE);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_PLAYBACK_ROUTE].elem) {
		err = elem_read_route(s, PLAY, CTL_PLAYBACK_ROUTE);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_GLOBAL_ROUTE].elem) {
		err = elem_read_route(s, PLAY, CTL_GLOBAL_ROUTE);
		if (err < 0)
			return err;
	}
      __skip_pswitch:

	if (s->ctls[CTL_CAPTURE_VOLUME].elem)
		err = elem_read_volume(s, CAPT, CTL_CAPTURE_VOLUME);
	else if (s->ctls[CTL_GLOBAL_VOLUME].elem)
		err = elem_read_volume(s, CAPT, CTL_GLOBAL_VOLUME);
	else if (s->ctls[CTL_SINGLE].elem &&
		 s->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_INTEGER)
		err = elem_read_volume(s, CAPT, CTL_SINGLE);
	if (err < 0)
		return err;

	if ((s->caps & (CAP_GSWITCH|CAP_CSWITCH)) == 0) {
		s->str[CAPT].sw = 0;
		goto __skip_cswitch;
	}
	if (s->ctls[CTL_CAPTURE_SWITCH].elem) {
		err = elem_read_switch(s, CAPT, CTL_CAPTURE_SWITCH);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_GLOBAL_SWITCH].elem) {
		err = elem_read_switch(s, CAPT, CTL_GLOBAL_SWITCH);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_SINGLE].elem &&
	    s->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_BOOLEAN) {
		err = elem_read_switch(s, CAPT, CTL_SINGLE);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_CAPTURE_ROUTE].elem) {
		err = elem_read_route(s, CAPT, CTL_CAPTURE_ROUTE);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_GLOBAL_ROUTE].elem) {
		err = elem_read_route(s, CAPT, CTL_GLOBAL_ROUTE);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_CAPTURE_SOURCE].elem) {
		snd_ctl_elem_value_t ctl;
		selem_ctl_t *c = &s->ctls[CTL_CAPTURE_SOURCE];
		memset(&ctl, 0, sizeof(ctl));
		err = snd_hctl_elem_read(c->elem, &ctl);
		if (err < 0)
			return err;
		for (idx = 0; idx < s->str[CAPT].channels; idx++) {
			unsigned int idx1 = idx;
			if (idx >= c->values)
				idx1 = 0;
			if (snd_ctl_elem_value_get_enumerated(&ctl, idx1) != s->capture_item)
				s->str[CAPT].sw &= ~(1 << idx);
		}
	}
      __skip_cswitch:

	if (memcmp(pvol, s->str[PLAY].vol, sizeof(pvol)) ||
	    psw != s->str[PLAY].sw ||
	    memcmp(cvol, s->str[CAPT].vol, sizeof(cvol)) ||
	    csw != s->str[CAPT].sw)
		return 1;
	return 0;
}

static int elem_write_volume(selem_t *s, int dir, selem_ctl_type_t type)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	selem_ctl_t *c = &s->ctls[type];
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(c->elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < c->values; idx++)
		ctl.value.integer.value[idx] = from_user(s, dir, c, s->str[dir].vol[idx]);
	if ((err = snd_hctl_elem_write(c->elem, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_switch(selem_t *s, int dir, selem_ctl_type_t type)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	selem_ctl_t *c = &s->ctls[type];
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(c->elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < c->values; idx++)
		ctl.value.integer.value[idx] = !!(s->str[dir].sw & (1 << idx));
	if ((err = snd_hctl_elem_write(c->elem, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_switch_constant(selem_t *s, selem_ctl_type_t type, int val)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	selem_ctl_t *c = &s->ctls[type];
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(c->elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < c->values; idx++)
		ctl.value.integer.value[idx] = !!val;
	if ((err = snd_hctl_elem_write(c->elem, &ctl)) < 0)
		return err;
	return 0;
}

static int elem_write_route(selem_t *s, int dir, selem_ctl_type_t type)
{
	snd_ctl_elem_value_t ctl;
	unsigned int idx;
	int err;
	selem_ctl_t *c = &s->ctls[type];
	memset(&ctl, 0, sizeof(ctl));
	if ((err = snd_hctl_elem_read(c->elem, &ctl)) < 0)
		return err;
	for (idx = 0; idx < c->values * c->values; idx++)
		ctl.value.integer.value[idx] = 0;
	for (idx = 0; idx < c->values; idx++)
		ctl.value.integer.value[idx * c->values + idx] = !!(s->str[dir].sw & (1 << idx));
	if ((err = snd_hctl_elem_write(c->elem, &ctl)) < 0)
		return err;
	return 0;
}

static int selem_write(snd_mixer_elem_t *elem)
{
	selem_t *s;
	unsigned int idx;
	int err;

	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;

	if (s->ctls[CTL_SINGLE].elem) {
		if (s->ctls[CTL_SINGLE].type == SND_CTL_ELEM_TYPE_INTEGER)
			err = elem_write_volume(s, PLAY, CTL_SINGLE);
		else
			err = elem_write_switch(s, PLAY, CTL_SINGLE);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_GLOBAL_VOLUME].elem) {
		err = elem_write_volume(s, PLAY, CTL_GLOBAL_VOLUME);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_GLOBAL_SWITCH].elem) {
		if (s->ctls[CTL_PLAYBACK_SWITCH].elem && s->ctls[CTL_CAPTURE_SWITCH].elem)
			err = elem_write_switch_constant(s, CTL_GLOBAL_SWITCH, 1);
		else
			err = elem_write_switch(s, PLAY, CTL_GLOBAL_SWITCH);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_PLAYBACK_VOLUME].elem) {
		err = elem_write_volume(s, PLAY, CTL_PLAYBACK_VOLUME);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_PLAYBACK_SWITCH].elem) {
		err = elem_write_switch(s, PLAY, CTL_PLAYBACK_SWITCH);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_PLAYBACK_ROUTE].elem) {
		err = elem_write_route(s, PLAY, CTL_PLAYBACK_ROUTE);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_CAPTURE_VOLUME].elem) {
		err = elem_write_volume(s, CAPT, CTL_CAPTURE_VOLUME);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_CAPTURE_SWITCH].elem) {
		err = elem_write_switch(s, CAPT, CTL_CAPTURE_SWITCH);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_CAPTURE_ROUTE].elem) {
		err = elem_write_route(s, CAPT, CTL_CAPTURE_ROUTE);
		if (err < 0)
			return err;
	}
	if (s->ctls[CTL_CAPTURE_SOURCE].elem) {
		snd_ctl_elem_value_t ctl;
		selem_ctl_t *c = &s->ctls[CTL_CAPTURE_SOURCE];
		memset(&ctl, 0, sizeof(ctl));
		if ((err = snd_hctl_elem_read(c->elem, &ctl)) < 0)
			return err;
		for (idx = 0; idx < c->values; idx++) {
			if (s->str[CAPT].sw & (1 << idx))
				snd_ctl_elem_value_set_enumerated(&ctl, idx, s->capture_item);
		}
		if ((err = snd_hctl_elem_write(c->elem, &ctl)) < 0)
			return err;
		/* update the element, don't remove */
		return selem_read(elem);
	}
	return 0;
}

static void selem_free(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	elem->private_data = NULL;
	free(s);
}

static int simple_update(snd_mixer_elem_t *melem)
{
	selem_t *simple;
	unsigned int caps, pchannels, cchannels;
	long pmin, pmax, cmin, cmax;
	selem_ctl_t *ctl;

	caps = 0;
	pchannels = 0;
	pmin = pmax = 0;
	cchannels = 0;
	cmin = cmax = 0;
	assert(melem->type == SND_MIXER_ELEM_SIMPLE);
	simple = melem->private_data;
	ctl = &simple->ctls[CTL_SINGLE];
	if (ctl->elem) {
		pchannels = cchannels = ctl->values;
		if (ctl->type == SNDRV_CTL_ELEM_TYPE_INTEGER) {
			caps |= CAP_GVOLUME;
			pmin = cmin = ctl->min;
			pmax = cmax = ctl->max;
		} else
			caps |= CAP_GSWITCH;
	}
	ctl = &simple->ctls[CTL_GLOBAL_SWITCH];
	if (ctl->elem) {
		if (pchannels < ctl->values)
			pchannels = ctl->values;
		if (cchannels < ctl->values)
			cchannels = ctl->values;
		caps |= CAP_GSWITCH;
	}
	ctl = &simple->ctls[CTL_GLOBAL_ROUTE];
	if (ctl->elem) {
		if (pchannels < ctl->values)
			pchannels = ctl->values;
		if (cchannels < ctl->values)
			cchannels = ctl->values;
		caps |= CAP_GSWITCH;
	}
	ctl = &simple->ctls[CTL_GLOBAL_VOLUME];
	if (ctl->elem) {
		if (pchannels < ctl->values)
			pchannels = ctl->values;
		if (pmin > ctl->min)
			pmin = ctl->min;
		if (pmax < ctl->max)
			pmax = ctl->max;
		if (cchannels < ctl->values)
			cchannels = ctl->values;
		if (cmin > ctl->min)
			cmin = ctl->min;
		if (cmax < ctl->max)
			cmax = ctl->max;
		caps |= CAP_GVOLUME;
	}
	ctl = &simple->ctls[CTL_PLAYBACK_SWITCH];
	if (ctl->elem) {
		if (pchannels < ctl->values)
			pchannels = ctl->values;
		caps |= CAP_PSWITCH;
		caps &= ~CAP_GSWITCH;
	}
	ctl = &simple->ctls[CTL_PLAYBACK_ROUTE];
	if (ctl->elem) {
		if (pchannels < ctl->values)
			pchannels = ctl->values;
		caps |= CAP_PSWITCH;
		caps &= ~CAP_GSWITCH;
	}
	ctl = &simple->ctls[CTL_CAPTURE_SWITCH];
	if (ctl->elem) {
		if (cchannels < ctl->values)
			cchannels = ctl->values;
		caps |= CAP_CSWITCH;
		caps &= ~CAP_GSWITCH;
	}
	ctl = &simple->ctls[CTL_CAPTURE_ROUTE];
	if (ctl->elem) {
		if (cchannels < ctl->values)
			cchannels = ctl->values;
		caps |= CAP_CSWITCH;
		caps &= ~CAP_GSWITCH;
	}
	ctl = &simple->ctls[CTL_PLAYBACK_VOLUME];
	if (ctl->elem) {
		if (pchannels < ctl->values)
			pchannels = ctl->values;
		if (pmin > ctl->min)
			pmin = ctl->min;
		if (pmax < ctl->max)
			pmax = ctl->max;
		caps |= CAP_PVOLUME;
		caps &= ~CAP_GVOLUME;
	}
	ctl = &simple->ctls[CTL_CAPTURE_VOLUME];
	if (ctl->elem) {
		if (cchannels < ctl->values)
			cchannels = ctl->values;
		if (cmin > ctl->min)
			cmin = ctl->min;
		if (cmax < ctl->max)
			cmax = ctl->max;
		caps |= CAP_CVOLUME;
		caps &= ~CAP_GVOLUME;
	}
	ctl = &simple->ctls[CTL_CAPTURE_SOURCE];
	if (ctl->elem) {
		if (cchannels < ctl->values)
			cchannels = ctl->values;
		caps |= CAP_CSWITCH | CAP_CSWITCH_EXCL;
		caps &= ~CAP_GSWITCH;
	}
	if (pchannels > 32)
		pchannels = 32;
	if (cchannels > 32)
		cchannels = 32;
	if (caps & (CAP_GSWITCH|CAP_PSWITCH))
		caps |= CAP_PSWITCH_JOIN;
	if (caps & (CAP_GVOLUME|CAP_PVOLUME))
		caps |= CAP_PVOLUME_JOIN;
	if (caps & (CAP_GSWITCH|CAP_CSWITCH))
		caps |= CAP_CSWITCH_JOIN;
	if (caps & (CAP_GVOLUME|CAP_CVOLUME))
		caps |= CAP_PVOLUME_JOIN;
	if (pchannels > 1 || cchannels > 1) {
		if (simple->ctls[CTL_SINGLE].elem &&
		    simple->ctls[CTL_SINGLE].values > 1) {
			if (caps & CAP_GSWITCH)
				caps &= ~CAP_PSWITCH_JOIN;
			else
				caps &= ~CAP_PVOLUME_JOIN;
		}
		if (simple->ctls[CTL_GLOBAL_ROUTE].elem ||
		    (simple->ctls[CTL_GLOBAL_SWITCH].elem &&
		     simple->ctls[CTL_GLOBAL_SWITCH].values > 1)) {
			caps &= ~(CAP_PSWITCH_JOIN|CAP_CSWITCH_JOIN);
		}
		if (simple->ctls[CTL_GLOBAL_VOLUME].elem &&
		    simple->ctls[CTL_GLOBAL_VOLUME].values > 1) {
			caps &= ~(CAP_PVOLUME_JOIN|CAP_CVOLUME_JOIN);
		}
	}
	if (pchannels > 1) {
		if (simple->ctls[CTL_PLAYBACK_ROUTE].elem ||
		    (simple->ctls[CTL_PLAYBACK_SWITCH].elem &&
		     simple->ctls[CTL_PLAYBACK_SWITCH].values > 1)) {
			caps &= ~CAP_PSWITCH_JOIN;
		}
		if (simple->ctls[CTL_PLAYBACK_VOLUME].elem &&
		    simple->ctls[CTL_PLAYBACK_VOLUME].values > 1) {
			caps &= ~CAP_PVOLUME_JOIN;
		}
	}
	if (cchannels > 1) {
		if (simple->ctls[CTL_CAPTURE_ROUTE].elem ||
		    (simple->ctls[CTL_CAPTURE_SWITCH].elem &&
		     simple->ctls[CTL_CAPTURE_SWITCH].values > 1)) {
			caps &= ~CAP_CSWITCH_JOIN;
		}
		if (simple->ctls[CTL_CAPTURE_VOLUME].elem &&
		    simple->ctls[CTL_CAPTURE_VOLUME].values > 1) {
			caps &= ~CAP_CVOLUME_JOIN;
		}
	}

	/* exceptions */
	if ((caps & (CAP_GSWITCH|CAP_PSWITCH|CAP_CSWITCH)) &&
	    (caps & (CAP_GSWITCH|CAP_PSWITCH|CAP_CSWITCH)) == (caps & CAP_GSWITCH)) {
		caps &= ~(CAP_GSWITCH|CAP_CSWITCH_JOIN|CAP_CSWITCH_EXCL);
		caps |= CAP_PSWITCH;
	}

	simple->caps = caps;
	simple->str[PLAY].channels = pchannels;
	if (!simple->str[PLAY].range) {
		simple->str[PLAY].min = pmin;
		simple->str[PLAY].max = pmax;
	}
	simple->str[CAPT].channels = cchannels;
	if (!simple->str[CAPT].range) {
		simple->str[CAPT].min = cmin;
		simple->str[CAPT].max = cmax;
	}
	return 0;
}	   

#ifndef DOC_HIDDEN
static struct suf {
	const char *suffix;
	selem_ctl_type_t type;
} suffixes[] = {
	{" Playback Switch", CTL_PLAYBACK_SWITCH},
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
#endif

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
			if (strncmp(name + l, p->suffix, slen) == 0 &&
			    (l < 1 || name[l-1] != '-')) {	/* 3D Control - Switch */
				*type = p->type;
				return l;
			}
		}
		p++;
	}
	return 0;
}

static int simple_add1(snd_mixer_class_t *class, const char *name,
		       snd_hctl_elem_t *helem, selem_ctl_type_t type,
		       unsigned int value)
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
	{
		unsigned int n;
		if (info.type != SND_CTL_ELEM_TYPE_BOOLEAN)
			return 0;
		n = sqrt((double)info.count);
		if (n * n != info.count)
			return 0;
		info.count = n;
		break;
	}
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
		melem->type = SND_MIXER_ELEM_SIMPLE;
		melem->private_data = simple;
		melem->private_free = selem_free;
		INIT_LIST_HEAD(&melem->helems);
		melem->compare_weight = get_compare_weight(simple->id.name, simple->id.index);
		new = 1;
	} else {
		simple = melem->private_data;
	}
	assert(!simple->ctls[type].elem);
	simple->ctls[type].elem = helem;
	simple->ctls[type].type = info.type;
	simple->ctls[type].access = info.access;
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
		err = snd_mixer_elem_add(melem, class);
	else
		err = snd_mixer_elem_info(melem);
	if (err < 0)
		return err;
	err = selem_read(melem);
	if (err < 0)
		return err;
	if (err)
		err = snd_mixer_elem_value(melem);
	return err;
}

static int simple_event_add(snd_mixer_class_t *class, snd_hctl_elem_t *helem)
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
		/* exception: Capture Volume and Capture Switch */
		if (type == CTL_GLOBAL_VOLUME && !strcmp(ename, "Capture"))
			type = CTL_CAPTURE_VOLUME;
		else if (type == CTL_GLOBAL_SWITCH && !strcmp(ename, "Capture"))
			type = CTL_CAPTURE_SWITCH;
		return simple_add1(class, ename, helem, type, 0);
	}
}

static int simple_event_remove(snd_hctl_elem_t *helem,
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
	return snd_mixer_elem_info(melem);
}

static int simple_event(snd_mixer_class_t *class, unsigned int mask,
			snd_hctl_elem_t *helem, snd_mixer_elem_t *melem)
{
	int err;
	if (mask == SND_CTL_EVENT_MASK_REMOVE)
		return simple_event_remove(helem, melem);
	if (mask & SND_CTL_EVENT_MASK_ADD) {
		err = simple_event_add(class, helem);
		if (err < 0)
			return err;
	}
	if (mask & SND_CTL_EVENT_MASK_INFO) {
		err = simple_event_remove(helem, melem);
		if (err < 0)
			return err;
		err = simple_event_add(class, helem);
		if (err < 0)
			return err;
		return 0;
	}
	if (mask & SND_CTL_EVENT_MASK_VALUE) {
		err = selem_read(melem);
		if (err < 0)
			return err;
		if (err) {
			err = snd_mixer_elem_value(melem);
			if (err < 0)
				return err;
		}
	}
	return 0;
}

static int simple_compare(const snd_mixer_elem_t *c1, const snd_mixer_elem_t *c2)
{
	selem_t *s1 = c1->private_data;
	selem_t *s2 = c2->private_data;
	int res = strcmp(s1->id.name, s2->id.name);
	if (res)
		return res;
	return s1->id.index - s2->id.index;
}


/**
 * \brief Register mixer simple element class
 * \param mixer Mixer handle
 * \param options Options container (not used now)
 * \param classp Pointer to returned mixer simple element class handle (or NULL)
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_register(snd_mixer_t *mixer, void *options ATTRIBUTE_UNUSED,
			     snd_mixer_class_t **classp)
{
	snd_mixer_class_t *class = calloc(1, sizeof(*class));
	int err;
	if (!class)
		return -ENOMEM;
	class->event = simple_event;
	class->compare = simple_compare;
	err = snd_mixer_class_register(class, mixer);
	if (err < 0) {
		free(class);
		return err;
	}
	if (classp)
		*classp = class;
	return 0;
}
	
/**
 * \brief Find a mixer simple element
 * \param mixer Mixer handle
 * \param id Mixer simple element identifier
 * \return mixer simple element handle or NULL if not found
 */
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
		s = e->private_data;
		if (!strcmp(s->id.name, id->name) && s->id.index == id->index)
			return e;
	}
	return NULL;
}

/**
 * \brief Get mixer simple element identifier
 * \param elem Mixer simple element handle
 * \param id returned mixer simple element identifier
 */
void snd_mixer_selem_get_id(snd_mixer_elem_t *elem,
			    snd_mixer_selem_id_t *id)
{
	selem_t *s;
	assert(elem && id);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	*id = s->id;
}

/**
 * \brief Get name part of mixer simple element identifier
 * \param elem Mixer simple element handle
 * \return name part of simple element identifier
 */
const char *snd_mixer_selem_get_name(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return s->id.name;
}

/**
 * \brief Get index part of mixer simple element identifier
 * \param elem Mixer simple element handle
 * \return index part of simple element identifier
 */
unsigned int snd_mixer_selem_get_index(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return s->id.index;
}

/**
 * \brief Return true if mixer simple element has only one volume control for both playback and capture
 * \param elem Mixer simple element handle
 * \return 0 separated control, 1 common control
 */
int snd_mixer_selem_has_common_volume(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_GVOLUME);
}

/**
 * \brief Return true if mixer simple element has only one switch control for both playback and capture
 * \param elem Mixer simple element handle
 * \return 0 separated control, 1 common control
 */
int snd_mixer_selem_has_common_switch(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_GSWITCH);
}

static int _snd_mixer_selem_set_volume(snd_mixer_elem_t *elem, int dir, snd_mixer_selem_channel_id_t channel, long value)
{
	selem_t *s = elem->private_data;
	assert((unsigned int) channel < s->str[dir].channels);
	assert(value >= s->str[dir].min && value <= s->str[dir].max);
	if (s->caps & 
	    (dir == PLAY ? CAP_PVOLUME_JOIN : CAP_CVOLUME_JOIN))
		channel = 0;
	if (value != s->str[dir].vol[channel]) {
		s->str[dir].vol[channel] = value;
		return 1;
	}
	return 0;
}

static int _snd_mixer_selem_set_volume_all(snd_mixer_elem_t *elem, int dir, long value)
{
	int changed = 0;
	snd_mixer_selem_channel_id_t channel;	
	selem_t *s = elem->private_data;
	assert(value >= s->str[dir].min && value <= s->str[dir].max);
	for (channel = 0; (unsigned int) channel < s->str[dir].channels; channel++) {
		if (value != s->str[dir].vol[channel]) {
			s->str[dir].vol[channel] = value;
			changed = 1;
		}
	}
	return changed;
}

static int _snd_mixer_selem_set_switch(snd_mixer_elem_t *elem, int dir, snd_mixer_selem_channel_id_t channel, int value)
{
	selem_t *s = elem->private_data;
	assert((unsigned int) channel < s->str[dir].channels);
	if (s->caps & 
	    (dir == PLAY ? CAP_PSWITCH_JOIN : CAP_CSWITCH_JOIN))
		channel = 0;
	if (value) {
		if (!(s->str[dir].sw & (1 << channel))) {
			s->str[dir].sw |= 1 << channel;
			return 1;
		}
	} else {
		if (s->str[dir].sw & (1 << channel)) {
			s->str[dir].sw &= ~(1 << channel);
			return 1;
		}
	}
	return 0;
}

static int _snd_mixer_selem_set_switch_all(snd_mixer_elem_t *elem, int dir, int value)
{
	selem_t *s = elem->private_data;
	if (value) {
		if (s->str[dir].sw != ~0U) {
			s->str[dir].sw = ~0U;
			return 1;
		}
	} else {
		if (s->str[dir].sw != 0U) {
			s->str[dir].sw = 0U;
			return 1;
		}
	}
	return 0;
}

/**
 * \brief Return name of mixer simple element channel
 * \param channel mixer simple element channel identifier
 * \return channel name
 */
const char *snd_mixer_selem_channel_name(snd_mixer_selem_channel_id_t channel)
{
	static const char *array[SND_MIXER_SCHN_LAST + 1] = {
		[SND_MIXER_SCHN_FRONT_LEFT] = "Front Left",
		[SND_MIXER_SCHN_FRONT_RIGHT] = "Front Right",
		[SND_MIXER_SCHN_FRONT_CENTER] = "Front Center",
		[SND_MIXER_SCHN_REAR_LEFT] = "Rear Left",
		[SND_MIXER_SCHN_REAR_RIGHT] = "Rear Right",
		[SND_MIXER_SCHN_WOOFER] = "Woofer"
	};
	const char *p;
	assert(channel <= SND_MIXER_SCHN_LAST);
	p = array[channel];
	if (!p)
		return "?";
	return p;
}

/**
 * \brief Get info about the active state of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if not active, 1 if active
 */
int snd_mixer_selem_is_active(snd_mixer_elem_t *elem)
{
	selem_t *s;
	selem_ctl_type_t ctl;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	for (ctl = CTL_SINGLE; ctl <= CTL_LAST; ctl++)
		if (s->ctls[ctl].elem != NULL && (s->ctls[ctl].access & SNDRV_CTL_ELEM_ACCESS_INACTIVE) != 0)
			return 0;
	return 1;
}

/**
 * \brief Get info about channels of playback stream of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if not mono, 1 if mono
 */
int snd_mixer_selem_is_playback_mono(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return s->str[PLAY].channels == 1;
}

/**
 * \brief Get info about channels of playback stream of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel Mixer simple element channel identifier
 * \return 0 if channel is not present, 1 if present
 */
int snd_mixer_selem_has_playback_channel(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return (unsigned int) channel < s->str[PLAY].channels;
}

/**
 * \brief Get range for playback volume of a mixer simple element
 * \param elem Mixer simple element handle
 * \param min Pointer to returned minimum
 * \param max Pointer to returned maximum
 */
void snd_mixer_selem_get_playback_volume_range(snd_mixer_elem_t *elem,
					       long *min, long *max)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	*min = s->str[PLAY].min;
	*max = s->str[PLAY].max;
}

/**
 * \brief Set range for playback volume of a mixer simple element
 * \param elem Mixer simple element handle
 * \param min minimum volume value
 * \param max maximum volume value
 */
void snd_mixer_selem_set_playback_volume_range(snd_mixer_elem_t *elem, 
					       long min, long max)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(min != max);
	s->str[PLAY].range = 1;
	s->str[PLAY].min = min;
	s->str[PLAY].max = max;
}

/**
 * \brief Return info about playback volume control of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if no control is present, 1 if it's present
 */
int snd_mixer_selem_has_playback_volume(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_PVOLUME) || !!(s->caps & CAP_GVOLUME);
}

/**
 * \brief Return info about playback volume control of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if control is separated per channel, 1 if control acts on all channels together
 */
int snd_mixer_selem_has_playback_volume_joined(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_PVOLUME_JOIN);
}

/**
 * \brief Return info about playback switch control existence of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if no control is present, 1 if it's present
 */
int snd_mixer_selem_has_playback_switch(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_PSWITCH) || !!(s->caps & CAP_GSWITCH);
}

/**
 * \brief Return info about playback switch control of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if control is separated per channel, 1 if control acts on all channels together
 */
int snd_mixer_selem_has_playback_switch_joined(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_PSWITCH_JOIN);
}

/**
 * \brief Return value of playback volume control of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel mixer simple element channel identifier
 * \param value pointer to returned value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_get_playback_volume(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel, long *value)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert((unsigned int) channel < s->str[PLAY].channels);
	assert(s->caps & (CAP_PVOLUME|CAP_GVOLUME));
	if (s->caps & CAP_PVOLUME_JOIN)
		channel = 0;
	*value = s->str[PLAY].vol[channel];
	return 0;
}

/**
 * \brief Return value of playback switch control of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel mixer simple element channel identifier
 * \param value pointer to returned value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_get_playback_switch(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel, int *value)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert((unsigned int) channel < s->str[PLAY].channels);
	assert(s->caps & (CAP_PSWITCH|CAP_GSWITCH));
	if (s->caps & CAP_PSWITCH_JOIN)
		channel = 0;
	*value = !!(s->str[PLAY].sw & (1 << channel));
	return 0;
}

/**
 * \brief Set value of playback volume control of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel mixer simple element channel identifier
 * \param value control value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_set_playback_volume(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel, long value)
{
	int changed;
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & (CAP_GVOLUME|CAP_PVOLUME));
	changed = _snd_mixer_selem_set_volume(elem, PLAY, channel, value);
	if (changed < 0)
		return changed;
	if (changed)
		return selem_write(elem);
	return 0;
}

/**
 * \brief Set value of playback volume control for all channels of a mixer simple element
 * \param elem Mixer simple element handle
 * \param value control value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_set_playback_volume_all(snd_mixer_elem_t *elem, long value)
{
	int changed;
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & (CAP_GVOLUME|CAP_PVOLUME));
	changed = _snd_mixer_selem_set_volume_all(elem, PLAY, value);
	if (changed < 0)
		return changed;
	if (changed)
		return selem_write(elem);
	return 0;
}

/**
 * \brief Set value of playback switch control of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel mixer simple element channel identifier
 * \param value control value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_set_playback_switch(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel, int value)
{
	int changed;
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & (CAP_GSWITCH|CAP_PSWITCH));
	changed = _snd_mixer_selem_set_switch(elem, PLAY, channel, value);
	if (changed < 0)
		return changed;
	if (changed)
		return selem_write(elem);
	return 0;
}

/**
 * \brief Set value of playback switch control for all channels of a mixer simple element
 * \param elem Mixer simple element handle
 * \param value control value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_set_playback_switch_all(snd_mixer_elem_t *elem, int value)
{
	int changed;
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & (CAP_GSWITCH|CAP_PSWITCH));
	changed = _snd_mixer_selem_set_switch_all(elem, PLAY, value);
	if (changed < 0)
		return changed;
	if (changed)
		return selem_write(elem);
	return 0;
}

/**
 * \brief Get info about channels of capture stream of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if not mono, 1 if mono
 */
int snd_mixer_selem_is_capture_mono(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return s->str[CAPT].channels == 1;
}

/**
 * \brief Get info about channels of capture stream of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel Mixer simple element channel identifier
 * \return 0 if channel is not present, 1 if present
 */
int snd_mixer_selem_has_capture_channel(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return (unsigned int) channel < s->str[CAPT].channels;
}

/**
 * \brief Get range for capture volume of a mixer simple element
 * \param elem Mixer simple element handle
 * \param min Pointer to returned minimum
 * \param max Pointer to returned maximum
 */
void snd_mixer_selem_get_capture_volume_range(snd_mixer_elem_t *elem,
					      long *min, long *max)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	*min = s->str[CAPT].min;
	*max = s->str[CAPT].max;
}

/**
 * \brief Set range for capture volume of a mixer simple element
 * \param elem Mixer simple element handle
 * \param min minimum volume value
 * \param max maximum volume value
 */
void snd_mixer_selem_set_capture_volume_range(snd_mixer_elem_t *elem, 
					      long min, long max)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(min != max);
	s->str[CAPT].range = 1;
	s->str[CAPT].min = min;
	s->str[CAPT].max = max;
}

/**
 * \brief Return info about capture volume control of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if no control is present, 1 if it's present
 */
int snd_mixer_selem_has_capture_volume(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_CVOLUME) /*|| !!(s->caps & CAP_GVOLUME)*/;
}

/**
 * \brief Return info about capture volume control of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if control is separated per channel, 1 if control acts on all channels together
 */
int snd_mixer_selem_has_capture_volume_joined(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_CVOLUME_JOIN);
}

/**
 * \brief Return info about capture switch control existence of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if no control is present, 1 if it's present
 */
int snd_mixer_selem_has_capture_switch(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_CSWITCH) /*|| !!(s->caps & CAP_GSWITCH)*/;
}

/**
 * \brief Return info about capture switch control of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if control is separated per channel, 1 if control acts on all channels together
 */
int snd_mixer_selem_has_capture_switch_joined(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_CSWITCH_JOIN);
}

/**
 * \brief Return info about capture switch control of a mixer simple element
 * \param elem Mixer simple element handle
 * \return 0 if control is separated per element, 1 if control acts on other elements too (i.e. only one active at a time inside a group)
 */
int snd_mixer_selem_has_capture_switch_exclusive(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	return !!(s->caps & CAP_CSWITCH_EXCL);
}

/**
 * \brief Return info about capture switch control of a mixer simple element
 * \param elem Mixer simple element handle
 * \return group for switch exclusivity (see #snd_mixer_selem_has_capture_switch_exclusive)
 */
int snd_mixer_selem_get_capture_group(snd_mixer_elem_t *elem)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & CAP_CSWITCH_EXCL);
	return s->capture_group;
}

/**
 * \brief Return value of capture volume control of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel mixer simple element channel identifier
 * \param value pointer to returned value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_get_capture_volume(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel, long *value)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert((unsigned int) channel < s->str[CAPT].channels);
	assert(s->caps & (CAP_GVOLUME|CAP_CVOLUME));
	if (s->caps & CAP_CVOLUME_JOIN)
		channel = 0;
	*value = s->str[CAPT].vol[channel];
	return 0;
}

/**
 * \brief Return value of capture switch control of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel mixer simple element channel identifier
 * \param value pointer to returned value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_get_capture_switch(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel, int *value)
{
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert((unsigned int) channel < s->str[CAPT].channels);
	assert(s->caps & (CAP_GSWITCH | CAP_CSWITCH));
	if (s->caps & CAP_CSWITCH_JOIN)
		channel = 0;
	*value = !!(s->str[CAPT].sw & (1 << channel));
	return 0;
}

/**
 * \brief Set value of capture volume control of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel mixer simple element channel identifier
 * \param value control value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_set_capture_volume(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel, long value)
{
	int changed;
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & (CAP_GVOLUME | CAP_CVOLUME));
	changed = _snd_mixer_selem_set_volume(elem, CAPT, channel, value);
	if (changed < 0)
		return changed;
	if (changed)
		return selem_write(elem);
	return 0;
}

/**
 * \brief Set value of capture volume control for all channels of a mixer simple element
 * \param elem Mixer simple element handle
 * \param value control value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_set_capture_volume_all(snd_mixer_elem_t *elem, long value)
{
	int changed;
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & (CAP_GVOLUME | CAP_CVOLUME));
	changed = _snd_mixer_selem_set_volume_all(elem, CAPT, value);
	if (changed < 0)
		return changed;
	if (changed)
		return selem_write(elem);
	return 0;
}

/**
 * \brief Set value of capture switch control of a mixer simple element
 * \param elem Mixer simple element handle
 * \param channel mixer simple element channel identifier
 * \param value control value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_set_capture_switch(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel, int value)
{
	int changed;
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & (CAP_GSWITCH | CAP_CSWITCH));
	changed = _snd_mixer_selem_set_switch(elem, CAPT, channel, value);
	if (changed < 0)
		return changed;
	if (changed)
		return selem_write(elem);
	return 0;
}

/**
 * \brief Set value of capture switch control for all channels of a mixer simple element
 * \param elem Mixer simple element handle
 * \param value control value
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_selem_set_capture_switch_all(snd_mixer_elem_t *elem, int value)
{
	int changed;
	selem_t *s;
	assert(elem);
	assert(elem->type == SND_MIXER_ELEM_SIMPLE);
	s = elem->private_data;
	assert(s->caps & (CAP_GSWITCH | CAP_CSWITCH));
	changed = _snd_mixer_selem_set_switch_all(elem, CAPT, value);
	if (changed < 0)
		return changed;
	if (changed)
		return selem_write(elem);
	return 0;
}

/**
 * \brief get size of #snd_mixer_selem_id_t
 * \return size in bytes
 */
size_t snd_mixer_selem_id_sizeof()
{
	return sizeof(snd_mixer_selem_id_t);
}

/**
 * \brief allocate an invalid #snd_mixer_selem_id_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_mixer_selem_id_malloc(snd_mixer_selem_id_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_mixer_selem_id_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_mixer_selem_id_t
 * \param pointer to object to free
 */
void snd_mixer_selem_id_free(snd_mixer_selem_id_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_mixer_selem_id_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_mixer_selem_id_copy(snd_mixer_selem_id_t *dst, const snd_mixer_selem_id_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Get name part of a mixer simple element identifier
 * \param obj Mixer simple element identifier
 * \return name part
 */
const char *snd_mixer_selem_id_get_name(const snd_mixer_selem_id_t *obj)
{
	assert(obj);
	return obj->name;
}

/**
 * \brief Get index part of a mixer simple element identifier
 * \param obj Mixer simple element identifier
 * \return index part
 */
unsigned int snd_mixer_selem_id_get_index(const snd_mixer_selem_id_t *obj)
{
	assert(obj);
	return obj->index;
}

/**
 * \brief Set name part of a mixer simple element identifier
 * \param obj Mixer simple element identifier
 * \param val name part
 */
void snd_mixer_selem_id_set_name(snd_mixer_selem_id_t *obj, const char *val)
{
	assert(obj);
	strncpy(obj->name, val, sizeof(obj->name));
}

/**
 * \brief Set index part of a mixer simple element identifier
 * \param obj Mixer simple element identifier
 * \param val index part
 */
void snd_mixer_selem_id_set_index(snd_mixer_selem_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->index = val;
}

