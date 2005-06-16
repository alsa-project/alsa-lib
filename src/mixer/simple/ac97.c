/*
 *  Mixer Interface - AC97 simple abstact module
 *  Copyright (c) 2005 by Jaroslav Kysela <perex@suse.cz>
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
#include <alsa/asoundlib.h>
#include <alsa/mixer_abst.h>
#include <alsa/list.h>

#define MAX_CHANNEL	6

#define SID_MASTER	0

struct melem_sids {
	const char *sname;
	unsigned short sindex;
	unsigned short weight;
	unsigned int chanmap[2];
};

struct melem_sids sids[] = {
	{
		.sname	= "Master",
		.sindex	= 0,
		.weight = 1,
		.chanmap = { 3, 0 },
	}
};

#define PURPOSE_VOLUME		0
#define PURPOSE_SWITCH		1
#define PURPOSE_ENUMLIST	2

struct helem_selector {
	snd_ctl_elem_iface_t iface;
	const char *name;
	unsigned short index;
	unsigned short sid;
	unsigned short purpose;
	unsigned short caps;
};

#define SELECTORS (sizeof(selectors)/sizeof(selectors[0]))

struct helem_selector selectors[] = {
	{
		.iface =	SND_CTL_ELEM_IFACE_MIXER,
		.name =		"Master Playback Volume",
		.index = 	0,
		.sid =		SID_MASTER,
		.purpose =	PURPOSE_VOLUME,
		.caps = 	SM_CAP_PVOLUME,
	},
	{
		.iface =	SND_CTL_ELEM_IFACE_MIXER,
		.name =		"Master Playback Switch",
		.index = 	0,
		.sid =		SID_MASTER,
		.purpose =	PURPOSE_SWITCH,
		.caps = 	SM_CAP_PSWITCH,
	}
};

struct helem_ac97 {
	struct list_head list;
	snd_hctl_elem_t *helem;
	unsigned short purpose;
	unsigned int caps;
	unsigned int inactive: 1;
	long min, max;
	unsigned int count;
};

struct selem_ac97 {
	sm_selem_t selem;
	struct list_head helems;
	unsigned short sid;
	struct {
		unsigned int chanmap;
		unsigned int forced_range: 1;
		long min, max;
		long vol[MAX_CHANNEL];
	} dir[2];
};

/*
 * Prototypes
 */

static int selem_read(snd_mixer_elem_t *elem);

/*
 * Helpers
 */

static unsigned int chanmap_to_channels(unsigned int chanmap)
{
	unsigned int i, res;
	
	for (i = 0, res = 0; i < MAX_CHANNEL; i++)
		if (chanmap & (1 << i))
			res++;
	return res;
}

static long to_user(struct selem_ac97 *s, int dir, struct helem_ac97 *c, long value)
{
	int64_t n;
	if (c->max == c->min)
		return s->dir[dir].min;
	n = (int64_t) (value - c->min) * (s->dir[dir].max - s->dir[dir].min);
	return s->dir[dir].min + (n + (c->max - c->min) / 2) / (c->max - c->min);
}

static long from_user(struct selem_ac97 *s, int dir, struct helem_ac97 *c, long value)
{ 
        int64_t n;
	if (s->dir[dir].max == s->dir[dir].min)
		return c->min;
        n = (int64_t) (value - s->dir[dir].min) * (c->max - c->min);
	return c->min + (n + (s->dir[dir].max - s->dir[dir].min) / 2) / (s->dir[dir].max - s->dir[dir].min);
}

static void update_ranges(struct selem_ac97 *s)
{
	static unsigned int mask[2] = { SM_CAP_PVOLUME, SM_CAP_CVOLUME };
	static unsigned int gmask[2] = { SM_CAP_GVOLUME, SM_CAP_GVOLUME };
	unsigned int dir, ok_flag;
	struct list_head *pos;
	struct helem_ac97 *helem;
	
	for (dir = 0; dir < 2; dir++) {
		s->dir[dir].min = 0;
		s->dir[dir].max = 0;
		ok_flag = 0;
		list_for_each(pos, &s->helems) {
			helem = list_entry(pos, struct helem_ac97, list);
			printf("min = %li, max = %li\n", helem->min, helem->max);
			if (helem->caps & mask[dir]) {
				s->dir[dir].min = helem->min;
				s->dir[dir].max = helem->max;
				ok_flag = 1;
				break;
			}
		}
		if (ok_flag)
			continue;
		list_for_each(pos, &s->helems) {
			helem = list_entry(pos, struct helem_ac97, list);
			if (helem->caps & gmask[dir]) {
				s->dir[dir].min = helem->min;
				s->dir[dir].max = helem->max;
				break;
			}
		}
	}
}

/*
 * Simple Mixer Operations
 */

static int is_ops(snd_mixer_elem_t *elem, int dir, int cmd, int val)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);

	switch (cmd) {

	case SM_OPS_IS_ACTIVE: {
		struct list_head *pos;
		struct helem_ac97 *helem;
		list_for_each(pos, &s->helems) {
			helem = list_entry(pos, struct helem_ac97, list);
			if (helem->inactive)
				return 0;
		}
		return 1;
	}

	case SM_OPS_IS_MONO:
		return chanmap_to_channels(s->dir[dir].chanmap) == 1;

	case SM_OPS_IS_CHANNEL:
		if (val > MAX_CHANNEL)
			return 0;
		return !!((1 << val) & s->dir[dir].chanmap);

	case SM_OPS_IS_ENUMERATED: {
		struct helem_ac97 *helem;
		helem = list_entry(s->helems.next, struct helem_ac97, list);
		return !!(helem->purpose == PURPOSE_ENUMLIST);
	}
	
	case SM_OPS_IS_ENUMCNT: {
		struct helem_ac97 *helem;
		helem = list_entry(s->helems.next, struct helem_ac97, list);
		return helem->max;
	}

	}
	
	return 1;
}

static int get_range_ops(snd_mixer_elem_t *elem, int dir,
			 long *min, long *max)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	
	*min = s->dir[dir].min;
	*max = s->dir[dir].max;

	return 0;
}

static int get_dB_range_ops(snd_mixer_elem_t *elem ATTRIBUTE_UNUSED,
			    int dir ATTRIBUTE_UNUSED,
			    long *min ATTRIBUTE_UNUSED,
			    long *max ATTRIBUTE_UNUSED)
{
	return -ENXIO;
}

static int set_range_ops(snd_mixer_elem_t *elem, int dir,
			 long min, long max)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	int err;

	s->dir[dir].forced_range = 1;
	s->dir[dir].min = min;
	s->dir[dir].max = max;
	
	if ((err = selem_read(elem)) < 0)
		return err;
	return 0;
}

static int get_volume_ops(snd_mixer_elem_t *elem, int dir,
			  snd_mixer_selem_channel_id_t channel, long *value)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	
	*value = s->dir[dir].vol[channel];
	return 0;
}

static int get_dB_ops(snd_mixer_elem_t *elem ATTRIBUTE_UNUSED,
		      int dir ATTRIBUTE_UNUSED,
		      snd_mixer_selem_channel_id_t channel ATTRIBUTE_UNUSED,
		      long *value ATTRIBUTE_UNUSED)
{
	return -ENXIO;
}

static int get_switch_ops(snd_mixer_elem_t *elem, int dir,
			  snd_mixer_selem_channel_id_t channel, int *value)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	*value = 0;
	return 0;
}

static int set_volume_ops(snd_mixer_elem_t *elem, int dir,
			  snd_mixer_selem_channel_id_t channel, long value)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	return 0;
}

static int set_dB_ops(snd_mixer_elem_t *elem ATTRIBUTE_UNUSED,
		      int dir ATTRIBUTE_UNUSED,
		      snd_mixer_selem_channel_id_t channel ATTRIBUTE_UNUSED,
		      long value ATTRIBUTE_UNUSED,
		      int xdir ATTRIBUTE_UNUSED)
{
	return -ENXIO;
}

static int set_switch_ops(snd_mixer_elem_t *elem, int dir,
			  snd_mixer_selem_channel_id_t channel, int value)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	int changed;
	return 0;
}

static int enum_item_name_ops(snd_mixer_elem_t *elem,
			      unsigned int item,
			      size_t maxlen, char *buf)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	return 0;
}

static int get_enum_item_ops(snd_mixer_elem_t *elem,
			     snd_mixer_selem_channel_id_t channel,
			     unsigned int *itemp)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	return 0;
}

static int set_enum_item_ops(snd_mixer_elem_t *elem,
			     snd_mixer_selem_channel_id_t channel,
			     unsigned int item)
{
	struct selem_ac97 *s = snd_mixer_elem_get_private(elem);
	return 0;
}

static struct sm_elem_ops simple_ac97_ops = {
	.is		= is_ops,
	.get_range	= get_range_ops,
	.get_dB_range	= get_dB_range_ops,
	.set_range	= set_range_ops,
	.get_volume	= get_volume_ops,
	.get_dB		= get_dB_ops,
	.set_volume	= set_volume_ops,
	.set_dB		= set_dB_ops,
	.get_switch	= get_switch_ops,
	.set_switch	= set_switch_ops,
	.enum_item_name	= enum_item_name_ops,
	.get_enum_item	= get_enum_item_ops,
	.set_enum_item	= set_enum_item_ops
};

/*
 * event handling
 */

static int selem_read(snd_mixer_elem_t *elem)
{
	printf("elem read: %p\n", elem);
	return 0;
}

static int simple_event_remove(snd_hctl_elem_t *helem,
			       snd_mixer_elem_t *melem)
{
	printf("event remove: %p\n", helem);
	return 0;
}

static void selem_free(snd_mixer_elem_t *elem)
{
	struct selem_ac97 *simple = snd_mixer_elem_get_private(elem);
	struct helem_ac97 *hsimple;
	struct list_head *pos, *npos;

	if (simple->selem.id)
		snd_mixer_selem_id_free(simple->selem.id);
	list_for_each_safe(pos, npos, &simple->helems) {
		hsimple = list_entry(pos, struct helem_ac97, list);
		free(hsimple);
	}
	free(simple);
}

static int simple_event_add(snd_mixer_class_t *class, snd_hctl_elem_t *helem)
{
	int count;
	struct helem_selector *sel;
	snd_ctl_elem_iface_t iface = snd_hctl_elem_get_interface(helem);
	const char *name = snd_hctl_elem_get_name(helem);
	unsigned int index = snd_hctl_elem_get_index(helem);
	snd_mixer_elem_t *melem;
	snd_mixer_selem_id_t *id;
	struct selem_ac97 *simple;
	struct helem_ac97 *hsimple;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_type_t ctype;
	unsigned long values;
	long min, max;
	int err, new = 0;
	
	snd_ctl_elem_info_alloca(&info);
	for (count = SELECTORS, sel = selectors; count > 0; count--, sel++) {
		if (sel->iface == iface && !strcmp(sel->name, name) && sel->index == index)
			break;
	}
	if (count == 0)
		return 0;	/* ignore this helem */
	err = snd_hctl_elem_info(helem, info);
	if (err < 0)
		return err;
	ctype = snd_ctl_elem_info_get_type(info);
	values = snd_ctl_elem_info_get_count(info);
	switch (ctype) {
	case SND_CTL_ELEM_TYPE_ENUMERATED:
		min = 0;
		max = snd_ctl_elem_info_get_items(info);
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		min = snd_ctl_elem_info_get_min(info);
		max = snd_ctl_elem_info_get_max(info);
		break;
	default:
		min = max = 0;
		break;
	}
	
	printf("event add: %p, %p (%s)\n", helem, sel, name);
	if (snd_mixer_selem_id_malloc(&id))
		return -ENOMEM;
	hsimple = calloc(1, sizeof(*hsimple));
	if (hsimple == NULL) {
		snd_mixer_selem_id_free(id);
		return -ENOMEM;
	}
	switch (sel->purpose) {
	case PURPOSE_SWITCH:
		if (ctype != SND_CTL_ELEM_TYPE_BOOLEAN) {
		      __invalid_type:
		      	snd_mixer_selem_id_free(id);
			return -EINVAL;
		}
		break;
	case PURPOSE_VOLUME:
		if (ctype != SND_CTL_ELEM_TYPE_INTEGER)
			goto __invalid_type;
		break;
	}
	hsimple->purpose = sel->purpose;
	hsimple->caps = sel->caps;
	hsimple->min = min;
	hsimple->max = max;
	snd_mixer_selem_id_set_name(id, sids[sel->sid].sname);
	snd_mixer_selem_id_set_index(id, sids[sel->sid].sindex);
	melem = snd_mixer_find_selem(snd_mixer_class_get_mixer(class), id);
	if (!melem) {
		simple = calloc(1, sizeof(*simple));
		if (!simple) {
			snd_mixer_selem_id_free(id);
			free(hsimple);
			return -ENOMEM;
		}
		simple->selem.id = id;
		simple->selem.ops = &simple_ac97_ops;
		INIT_LIST_HEAD(&simple->helems);
		simple->sid = sel->sid;
		err = snd_mixer_elem_new(&melem, SND_MIXER_ELEM_SIMPLE,
					 sids[sel->sid].weight,
					 simple, selem_free);
		if (err < 0) {
			snd_mixer_selem_id_free(id);
			free(hsimple);
			free(simple);
			return err;
		}
		new = 1;
	} else {
		simple = snd_mixer_elem_get_private(melem);
		snd_mixer_selem_id_free(id);
	}
	list_add_tail(&hsimple->list, &simple->helems);
	hsimple->inactive = snd_ctl_elem_info_is_inactive(info);
	err = snd_mixer_elem_attach(melem, helem);
	if (err < 0)
		goto __error;
	simple->dir[0].chanmap |= sids[sel->sid].chanmap[0];
	simple->dir[1].chanmap |= sids[sel->sid].chanmap[1];
	simple->selem.caps |= hsimple->caps;
	update_ranges(simple);
#if 0
	err = simple_update(melem);
	if (err < 0) {
		if (new)
			goto __error;
		return err;
	}
#endif
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
      __error:
      	if (new)
      		snd_mixer_elem_free(melem);
      	return -EINVAL;
	return 0;
}

int alsa_mixer_simple_event(snd_mixer_class_t *class, unsigned int mask,
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
