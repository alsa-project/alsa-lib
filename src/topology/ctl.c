/*
  Copyright(c) 2014-2015 Intel Corporation
  All rights reserved.

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  Authors: Mengdong Lin <mengdong.lin@intel.com>
           Yao Jin <yao.jin@intel.com>
           Liam Girdwood <liam.r.girdwood@linux.intel.com>
*/

#include "list.h"
#include "tplg_local.h"

#define ENUM_VAL_SIZE 	(SNDRV_CTL_ELEM_ID_NAME_MAXLEN >> 2)

struct ctl_access_elem {
	const char *name;
	unsigned int value;
};

/* CTL access strings and codes */
/* place the multi-bit values on top - like read_write - for save */
static const struct ctl_access_elem ctl_access[] = {
	{"read_write", SNDRV_CTL_ELEM_ACCESS_READWRITE},
	{"tlv_read_write", SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE},
	{"read", SNDRV_CTL_ELEM_ACCESS_READ},
	{"write", SNDRV_CTL_ELEM_ACCESS_WRITE},
	{"volatile", SNDRV_CTL_ELEM_ACCESS_VOLATILE},
	{"tlv_read", SNDRV_CTL_ELEM_ACCESS_TLV_READ},
	{"tlv_write", SNDRV_CTL_ELEM_ACCESS_TLV_WRITE},
	{"tlv_command", SNDRV_CTL_ELEM_ACCESS_TLV_COMMAND},
	{"inactive", SNDRV_CTL_ELEM_ACCESS_INACTIVE},
	{"lock", SNDRV_CTL_ELEM_ACCESS_LOCK},
	{"owner", SNDRV_CTL_ELEM_ACCESS_OWNER},
	{"tlv_callback", SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK},
};

/* find CTL access strings and conver to values */
static int parse_access_values(snd_config_t *cfg,
			       struct snd_soc_tplg_ctl_hdr *hdr)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *value = NULL;
	unsigned int j;

	tplg_dbg(" Access:");

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		/* match access value and set flags */
		for (j = 0; j < ARRAY_SIZE(ctl_access); j++) {
			if (strcmp(value, ctl_access[j].name) == 0) {
				hdr->access |= ctl_access[j].value;
				tplg_dbg("\t%s", value);
				break;
			}
		}
	}

	return 0;
}

/* Parse Access */
int parse_access(snd_config_t *cfg,
		 struct snd_soc_tplg_ctl_hdr *hdr)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err = 0;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "access") == 0) {
			err = parse_access_values(n, hdr);
			if (err < 0) {
				SNDERR("failed to parse access");
				return err;
			}
			continue;
		}
	}

	return err;
}

/* Save Access */
static int tplg_save_access(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			    struct snd_soc_tplg_ctl_hdr *hdr,
			    struct tplg_buf *dst, const char *pfx)
{
	const char *last;
	unsigned int j, count, access, cval;
	int err;

	if (hdr->access == 0)
		return 0;

	access = hdr->access;
	for (j = 0, count = 0, last = NULL; j < ARRAY_SIZE(ctl_access); j++) {
		cval = ctl_access[j].value;
		if ((access & cval) == cval) {
			access &= ~cval;
			last = ctl_access[j].name;
			count++;
		}
	}
	if (count == 1)
		return tplg_save_printf(dst, pfx, "access.0 %s\n", last);
	err = tplg_save_printf(dst, pfx, "access [\n");
	if (err < 0)
		return err;
	access = hdr->access;
	for (j = 0; j < ARRAY_SIZE(ctl_access); j++) {
		cval = ctl_access[j].value;
		if ((access & cval) == cval) {
			err = tplg_save_printf(dst, pfx, "\t%s\n",
					       ctl_access[j].name);
			if (err < 0)
				return err;
			access &= ~cval;
		}
	}
	return tplg_save_printf(dst, pfx, "]\n");
}

/* copy referenced TLV to the mixer control */
static int copy_tlv(struct tplg_elem *elem, struct tplg_elem *ref)
{
	struct snd_soc_tplg_mixer_control *mixer_ctrl =  elem->mixer_ctrl;
	struct snd_soc_tplg_ctl_tlv *tlv = ref->tlv;

	tplg_dbg("TLV '%s' used by '%s", ref->id, elem->id);

	/* TLV has a fixed size */
	mixer_ctrl->hdr.tlv = *tlv;
	return 0;
}

/* check referenced TLV for a mixer control */
static int tplg_build_mixer_control(snd_tplg_t *tplg,
				    struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err = 0;

	base = &elem->ref_list;

	/* for each ref in this control elem */
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->elem)
			continue;

		if (ref->type == SND_TPLG_TYPE_TLV) {
			ref->elem = tplg_elem_lookup(&tplg->tlv_list,
				ref->id, SND_TPLG_TYPE_TLV, elem->index);
			if (ref->elem)
				 err = copy_tlv(elem, ref->elem);

		} else if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}

		if (!ref->elem) {
			SNDERR("cannot find '%s' referenced by"
				" control '%s'", ref->id, elem->id);
			return -EINVAL;
		} else if (err < 0)
			return err;
	}

	return 0;
}

static void copy_enum_texts(struct tplg_elem *enum_elem,
			    struct tplg_elem *ref_elem)
{
	struct snd_soc_tplg_enum_control *ec = enum_elem->enum_ctrl;
	struct tplg_texts *texts = ref_elem->texts;

	memcpy(ec->texts, texts->items,
		SND_SOC_TPLG_NUM_TEXTS * SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	ec->items += texts->num_items;
}

/* check referenced text for a enum control */
static int tplg_build_enum_control(snd_tplg_t *tplg,
				   struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err;

	base = &elem->ref_list;

	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->elem)
			continue;

		if (ref->type == SND_TPLG_TYPE_TEXT) {
			ref->elem = tplg_elem_lookup(&tplg->text_list,
				ref->id, SND_TPLG_TYPE_TEXT, elem->index);
			if (ref->elem)
				copy_enum_texts(elem, ref->elem);

		} else if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}
		if (!ref->elem) {
			SNDERR("cannot find '%s' referenced by"
				" control '%s'", ref->id, elem->id);
			return -EINVAL;
		}
	}

	return 0;
}

/* check referenced private data for a byte control */
static int tplg_build_bytes_control(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err;

	base = &elem->ref_list;

	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->elem)
			continue;

		 if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

int tplg_build_controls(snd_tplg_t *tplg)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	base = &tplg->mixer_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		err = tplg_build_mixer_control(tplg, elem);
		if (err < 0)
			return err;

		/* add control to manifest */
		tplg->manifest.control_elems++;
	}

	base = &tplg->enum_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		err = tplg_build_enum_control(tplg, elem);
		if (err < 0)
			return err;

		/* add control to manifest */
		tplg->manifest.control_elems++;
	}

	base = &tplg->bytes_ext_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		err = tplg_build_bytes_control(tplg, elem);
		if (err < 0)
			return err;

		/* add control to manifest */
		tplg->manifest.control_elems++;
	}

	return 0;
}


/*
 * Parse TLV of DBScale type.
 *
 * Parse DBScale describing min, step, mute in DB.
 */
static int tplg_parse_tlv_dbscale(snd_config_t *cfg, struct tplg_elem *elem)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct snd_soc_tplg_ctl_tlv *tplg_tlv = elem->tlv;
	struct snd_soc_tplg_tlv_dbscale *scale;
	const char *id = NULL;
	int val;

	tplg_dbg(" scale: %s", elem->id);

	tplg_tlv->size = sizeof(struct snd_soc_tplg_ctl_tlv);
	tplg_tlv->type = SNDRV_CTL_TLVT_DB_SCALE;
	scale = &tplg_tlv->scale;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get ID */
		if (snd_config_get_id(n, &id) < 0)
			return -EINVAL;

		/* get value */
		if (tplg_get_integer(n, &val, 0))
			continue;

		tplg_dbg("\t%s = %i", id, val);

		/* get TLV data */
		if (strcmp(id, "min") == 0)
			scale->min = val;
		else if (strcmp(id, "step") == 0)
			scale->step = val;
		else if (strcmp(id, "mute") == 0)
			scale->mute = val;
		else
			SNDERR("unknown id '%s'", id);
	}

	return 0;
}

/* Parse TLV */
int tplg_parse_tlv(snd_tplg_t *tplg, snd_config_t *cfg,
		   void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err = 0;
	struct tplg_elem *elem;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_TLV);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "scale") == 0) {
			err = tplg_parse_tlv_dbscale(n, elem);
			if (err < 0) {
				SNDERR("failed to DBScale");
				return err;
			}
			continue;
		}
	}

	return err;
}

/* save TLV data */
int tplg_save_tlv(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		  struct tplg_elem *elem,
		  struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_ctl_tlv *tlv = elem->tlv;
	struct snd_soc_tplg_tlv_dbscale *scale;
	int err;

	if (tlv->type != SNDRV_CTL_TLVT_DB_SCALE) {
		SNDERR("unknown TLV type");
		return -EINVAL;
	}

	scale = &tlv->scale;
	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "\tscale {\n");
	if (err >= 0 && scale->min)
		err = tplg_save_printf(dst, pfx, "\t\tmin %i\n", scale->min);
	if (err >= 0 && scale->step > 0)
		err = tplg_save_printf(dst, pfx, "\t\tstep %i\n", scale->step);
	if (err >= 0 && scale->mute > 0)
		err = tplg_save_printf(dst, pfx, "\t\tmute %i\n", scale->mute);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "\t}\n");
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* Parse Control Bytes */
int tplg_parse_control_bytes(snd_tplg_t *tplg,
			     snd_config_t *cfg,
			     void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_bytes_control *be;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err, ival;
	bool access_set = false, tlv_set = false;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_BYTES);
	if (!elem)
		return -ENOMEM;

	be = elem->bytes_ext;
	be->size = elem->size;
	snd_strlcpy(be->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	be->hdr.type = SND_SOC_TPLG_TYPE_BYTES;

	tplg_dbg(" Control Bytes: %s", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "base") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			be->base = ival;
			tplg_dbg("\t%s: %d", id, be->base);
			continue;
		}

		if (strcmp(id, "num_regs") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			be->num_regs = ival;
			tplg_dbg("\t%s: %d", id, be->num_regs);
			continue;
		}

		if (strcmp(id, "max") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			be->max = ival;
			tplg_dbg("\t%s: %d", id, be->max);
			continue;
		}

		if (strcmp(id, "mask") == 0) {
			if (tplg_get_integer(n, &ival, 16))
				return -EINVAL;

			be->mask = ival;
			tplg_dbg("\t%s: %d", id, be->mask);
			continue;
		}

		if (strcmp(id, "data") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_DATA);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "tlv") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			err = tplg_ref_add(elem, SND_TPLG_TYPE_TLV, val);
			if (err < 0)
				return err;

			tlv_set = true;
			tplg_dbg("\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "ops") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_ops,
				&be->hdr);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "extops") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_ext_ops,
				be);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "access") == 0) {
			err = parse_access(cfg, &be->hdr);
			if (err < 0)
				return err;
			access_set = true;
			continue;
		}
	}

	/* set CTL access to default values if none are provided */
	if (!access_set) {

		be->hdr.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
		if (tlv_set)
			be->hdr.access |= SNDRV_CTL_ELEM_ACCESS_TLV_READ;
	}

	return 0;
}

/* save control bytes */
int tplg_save_control_bytes(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			    struct tplg_elem *elem,
			    struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_bytes_control *be = elem->bytes_ext;
	char pfx2[16];
	int err;

	if (!be)
		return 0;

	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err < 0)
		return err;
	if (err >= 0 && elem->index > 0)
		err = tplg_save_printf(dst, pfx, "\tindex %u\n", elem->index);
	if (err >= 0 && be->base > 0)
		err = tplg_save_printf(dst, pfx, "\tbase %u\n", be->base);
	if (err >= 0 && be->num_regs > 0)
		err = tplg_save_printf(dst, pfx, "\tnum_regs %u\n", be->num_regs);
	if (err >= 0 && be->max > 0)
		err = tplg_save_printf(dst, pfx, "\tmax %u\n", be->max);
	if (err >= 0 && be->mask > 0)
		err = tplg_save_printf(dst, pfx, "\tmask %u\n", be->mask);
	if (err >= 0)
		err = tplg_save_ops(tplg, &be->hdr, dst, pfx2);
	if (err >= 0)
		err = tplg_save_ext_ops(tplg, be, dst, pfx2);
	if (err >= 0)
		err = tplg_save_access(tplg, &be->hdr, dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_TLV,
				     "tlv", dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_DATA,
				     "data", dst, pfx2);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* Parse Control Enums. */
int tplg_parse_control_enum(snd_tplg_t *tplg, snd_config_t *cfg,
			    void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_enum_control *ec;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err, j;
	bool access_set = false;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_ENUM);
	if (!elem)
		return -ENOMEM;

	ec = elem->enum_ctrl;
	snd_strlcpy(ec->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	ec->hdr.type = SND_SOC_TPLG_TYPE_ENUM;
	ec->size = elem->size;
	tplg->channel_idx = 0;

	/* set channel reg to default state */
	for (j = 0; j < SND_SOC_TPLG_MAX_CHAN; j++) {
		ec->channel[j].reg = -1;
	}

	tplg_dbg(" Control Enum: %s", elem->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "texts") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			tplg_ref_add(elem, SND_TPLG_TYPE_TEXT, val);
			tplg_dbg("\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "channel") == 0) {
			if (ec->num_channels >= SND_SOC_TPLG_MAX_CHAN) {
				SNDERR("too many channels %s", elem->id);
				return -EINVAL;
			}

			err = tplg_parse_compound(tplg, n, tplg_parse_channel,
				ec->channel);
			if (err < 0)
				return err;

			ec->num_channels = tplg->channel_idx;
			continue;
		}

		if (strcmp(id, "ops") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_ops,
				&ec->hdr);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "data") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_DATA);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "access") == 0) {
			err = parse_access(cfg, &ec->hdr);
			if (err < 0)
				return err;
			access_set = true;
			continue;
		}
	}

	/* set CTL access to default values if none are provided */
	if (!access_set) {
		ec->hdr.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
	}

	return 0;
}

/* save control eunm */
int tplg_save_control_enum(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			   struct tplg_elem *elem,
			   struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_enum_control *ec = elem->enum_ctrl;
	char pfx2[16];
	int err;

	if (!ec)
		return 0;

	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err < 0)
		return err;
	if (err >= 0 && elem->index > 0)
		err = tplg_save_printf(dst, pfx, "\tindex %u\n", elem->index);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_TEXT,
				     "texts", dst, pfx2);
	if (err >= 0)
		err = tplg_save_channels(tplg, ec->channel, ec->num_channels,
					 dst, pfx2);
	if (err >= 0)
		err = tplg_save_ops(tplg, &ec->hdr, dst, pfx2);
	if (err >= 0)
		err = tplg_save_access(tplg, &ec->hdr, dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_DATA,
				     "data", dst, pfx2);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* Parse Controls.
 *
 * Mixer control. Supports multiple channels.
 */
int tplg_parse_control_mixer(snd_tplg_t *tplg,
			     snd_config_t *cfg,
			     void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_mixer_control *mc;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err, j, ival;
	bool access_set = false, tlv_set = false;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_MIXER);
	if (!elem)
		return -ENOMEM;

	/* init new mixer */
	mc = elem->mixer_ctrl;
	snd_strlcpy(mc->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	mc->hdr.type = SND_SOC_TPLG_TYPE_MIXER;
	mc->size = elem->size;
	tplg->channel_idx = 0;

	/* set channel reg to default state */
	for (j = 0; j < SND_SOC_TPLG_MAX_CHAN; j++)
		mc->channel[j].reg = -1;

	tplg_dbg(" Control Mixer: %s", elem->id);

	/* giterate trough each mixer elment */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "channel") == 0) {
			if (mc->num_channels >= SND_SOC_TPLG_MAX_CHAN) {
				SNDERR("too many channels %s", elem->id);
				return -EINVAL;
			}

			err = tplg_parse_compound(tplg, n, tplg_parse_channel,
				mc->channel);
			if (err < 0)
				return err;

			mc->num_channels = tplg->channel_idx;
			continue;
		}

		if (strcmp(id, "max") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			mc->max = ival;
			tplg_dbg("\t%s: %d", id, mc->max);
			continue;
		}

		if (strcmp(id, "invert") == 0) {
			ival = snd_config_get_bool(n);
			if (ival < 0)
				return -EINVAL;
			mc->invert = ival;

			tplg_dbg("\t%s: %d", id, mc->invert);
			continue;
		}

		if (strcmp(id, "ops") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_ops,
				&mc->hdr);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "tlv") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			err = tplg_ref_add(elem, SND_TPLG_TYPE_TLV, val);
			if (err < 0)
				return err;

			tlv_set = true;
			tplg_dbg("\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "data") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_DATA);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "access") == 0) {
			err = parse_access(cfg, &mc->hdr);
			if (err < 0)
				return err;
			access_set = true;
			continue;
		}
	}

	/* set CTL access to default values if none are provided */
	if (!access_set) {

		mc->hdr.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
		if (tlv_set)
			mc->hdr.access |= SNDRV_CTL_ELEM_ACCESS_TLV_READ;
	}

	return 0;
}

int tplg_save_control_mixer(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			    struct tplg_elem *elem,
			    struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_mixer_control *mc = elem->mixer_ctrl;
	char pfx2[16];
	int err;

	if (!mc)
		return 0;
	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err < 0)
		return err;
	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	if (err >= 0 && elem->index > 0)
		err = tplg_save_printf(dst, pfx, "\tindex %u\n", elem->index);
	if (err >= 0)
		err = tplg_save_channels(tplg, mc->channel, mc->num_channels,
					 dst, pfx2);
	if (err >= 0 && mc->max > 0)
		err = tplg_save_printf(dst, pfx, "\tmax %u\n", mc->max);
	if (err >= 0 && mc->invert > 0)
		err = tplg_save_printf(dst, pfx, "\tinvert 1\n");
	if (err >= 0 && mc->invert > 0)
		err = tplg_save_printf(dst, pfx, "\tinvert 1\n");
	if (err >= 0)
		err = tplg_save_ops(tplg, &mc->hdr, dst, pfx2);
	if (err >= 0)
		err = tplg_save_access(tplg, &mc->hdr, dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_TLV,
				     "tlv", dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_DATA,
				     "data", dst, pfx2);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

static int init_ctl_hdr(snd_tplg_t *tplg,
			struct tplg_elem *parent,
			struct snd_soc_tplg_ctl_hdr *hdr,
			struct snd_tplg_ctl_template *t)
{
	struct tplg_elem *elem;
	int err;

	hdr->size = sizeof(struct snd_soc_tplg_ctl_hdr);
	hdr->type = t->type;

	snd_strlcpy(hdr->name, t->name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	/* clean up access flag */
	if (t->access == 0)
		t->access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
	t->access &= (SNDRV_CTL_ELEM_ACCESS_READWRITE |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE |
		SNDRV_CTL_ELEM_ACCESS_INACTIVE |
		SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE |
		SNDRV_CTL_ELEM_ACCESS_TLV_COMMAND |
		SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK);

	hdr->access = t->access;
	hdr->ops.get = t->ops.get;
	hdr->ops.put = t->ops.put;
	hdr->ops.info = t->ops.info;

	/* TLV */
	if (hdr->access & SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE
		&& !(hdr->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK)) {

		struct snd_tplg_tlv_template *tlvt = t->tlv;
		struct snd_soc_tplg_ctl_tlv *tlv;
		struct snd_tplg_tlv_dbscale_template *scalet;
		struct snd_soc_tplg_tlv_dbscale *scale;

		if (!tlvt) {
			SNDERR("missing TLV data");
			return -EINVAL;
		}

		elem = tplg_elem_new_common(tplg, NULL, parent->id,
					    SND_TPLG_TYPE_TLV);
		if (!elem)
			return -ENOMEM;

		tlv = elem->tlv;

		err = tplg_ref_add(parent, SND_TPLG_TYPE_TLV, parent->id);
		if (err < 0)
			return err;

		tlv->size = sizeof(struct snd_soc_tplg_ctl_tlv);
		tlv->type = tlvt->type;

		switch (tlvt->type) {
		case SNDRV_CTL_TLVT_DB_SCALE:
			scalet = container_of(tlvt,
				struct snd_tplg_tlv_dbscale_template, hdr);
			scale = &tlv->scale;
			scale->min = scalet->min;
			scale->step = scalet->step;
			scale->mute = scalet->mute;
			break;

		/* TODO: add support for other TLV types */
		default:
			SNDERR("unsupported TLV type %d", tlv->type);
			break;
		}
	}

	return 0;
}

int tplg_add_mixer(snd_tplg_t *tplg, struct snd_tplg_mixer_template *mixer,
		   struct tplg_elem **e)
{
	struct snd_soc_tplg_mixer_control *mc;
	struct snd_soc_tplg_private *priv;
	struct tplg_elem *elem;
	int ret, i, num_channels;

	tplg_dbg(" Control Mixer: %s", mixer->hdr.name);

	if (mixer->hdr.type != SND_SOC_TPLG_TYPE_MIXER) {
		SNDERR("invalid mixer type %d", mixer->hdr.type);
		return -EINVAL;
	}

	elem = tplg_elem_new_common(tplg, NULL, mixer->hdr.name,
		SND_TPLG_TYPE_MIXER);
	if (!elem)
		return -ENOMEM;

	/* init new mixer */
	mc = elem->mixer_ctrl;
	mc->size = elem->size;
	ret = init_ctl_hdr(tplg, elem, &mc->hdr, &mixer->hdr);
	if (ret < 0) {
		tplg_elem_free(elem);
		return ret;
	}

	mc->min = mixer->min;
	mc->max = mixer->max;
	mc->platform_max = mixer->platform_max;
	mc->invert = mixer->invert;

	/* set channel reg to default state */
	for (i = 0; i < SND_SOC_TPLG_MAX_CHAN; i++)
		mc->channel[i].reg = -1;

	num_channels = mixer->map ? mixer->map->num_channels : 0;
	mc->num_channels = num_channels;

	for (i = 0; i < num_channels; i++) {
		struct snd_tplg_channel_elem *channel = &mixer->map->channel[i];

		mc->channel[i].size = sizeof(mc->channel[0]);
		mc->channel[i].reg = channel->reg;
		mc->channel[i].shift = channel->shift;
		mc->channel[i].id = channel->id;
	}

	/* priv data */
	priv = mixer->priv;
	if (priv && priv->size > 0) {
		ret = tplg_add_data(tplg, elem, priv,
				    sizeof(*priv) + priv->size);
		if (ret < 0)
			return ret;
	}

	if (e)
		*e = elem;
	return 0;
}

int tplg_add_enum(snd_tplg_t *tplg, struct snd_tplg_enum_template *enum_ctl,
		  struct tplg_elem **e)
{
	struct snd_soc_tplg_enum_control *ec;
	struct snd_soc_tplg_private *priv;
	struct tplg_elem *elem;
	int ret, i, num_items, num_channels;

	tplg_dbg(" Control Enum: %s", enum_ctl->hdr.name);

	if (enum_ctl->hdr.type != SND_SOC_TPLG_TYPE_ENUM) {
		SNDERR("invalid enum type %d", enum_ctl->hdr.type);
		return -EINVAL;
	}

	elem = tplg_elem_new_common(tplg, NULL, enum_ctl->hdr.name,
		SND_TPLG_TYPE_ENUM);
	if (!elem)
		return -ENOMEM;

	ec = elem->enum_ctrl;
	ec->size = elem->size;
	ret = init_ctl_hdr(tplg, elem, &ec->hdr, &enum_ctl->hdr);
	if (ret < 0) {
		tplg_elem_free(elem);
		return ret;
	}

	num_items =  enum_ctl->items < SND_SOC_TPLG_NUM_TEXTS ?
		enum_ctl->items : SND_SOC_TPLG_NUM_TEXTS;
	ec->items = num_items;
	ec->mask = enum_ctl->mask;
	ec->count = enum_ctl->items;

	/* set channel reg to default state */
	for (i = 0; i < SND_SOC_TPLG_MAX_CHAN; i++)
		ec->channel[i].reg = -1;

	num_channels = enum_ctl->map ? enum_ctl->map->num_channels : 0;
	ec->num_channels = num_channels;

	for (i = 0; i < num_channels; i++) {
		struct snd_tplg_channel_elem *channel = &enum_ctl->map->channel[i];

		ec->channel[i].size = sizeof(ec->channel[0]);
		ec->channel[i].reg = channel->reg;
		ec->channel[i].shift = channel->shift;
		ec->channel[i].id = channel->id;
	}

	if (enum_ctl->texts != NULL) {
		struct tplg_elem *texts = tplg_elem_new_common(tplg, NULL,
						enum_ctl->hdr.name, SND_TPLG_TYPE_TEXT);

		texts->texts->num_items = num_items;
		for (i = 0; i < num_items; i++) {
			if (!enum_ctl->texts[i])
				continue;
			snd_strlcpy(ec->texts[i], enum_ctl->texts[i],
				    SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
			snd_strlcpy(texts->texts->items[i], enum_ctl->texts[i],
				    SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		}
		tplg_ref_add(elem, SND_TPLG_TYPE_TEXT, enum_ctl->hdr.name);
	}

	if (enum_ctl->values != NULL) {
		for (i = 0; i < num_items; i++) {
			if (enum_ctl->values[i] == NULL)
				continue;

			memcpy(&ec->values[i * sizeof(int) * ENUM_VAL_SIZE],
				enum_ctl->values[i],
				sizeof(int) * ENUM_VAL_SIZE);
		}
	}

	/* priv data */
	priv = enum_ctl->priv;
	if (priv && priv->size > 0) {
		ret = tplg_add_data(tplg, elem, priv,
				    sizeof(*priv) + priv->size);
		if (ret < 0)
			return ret;
	}

	if (e)
		*e = elem;
	return 0;
}

int tplg_add_bytes(snd_tplg_t *tplg, struct snd_tplg_bytes_template *bytes_ctl,
		   struct tplg_elem **e)
{
	struct snd_soc_tplg_bytes_control *be;
	struct snd_soc_tplg_private *priv;
	struct tplg_elem *elem;
	int ret;

	tplg_dbg(" Control Bytes: %s", bytes_ctl->hdr.name);

	if (bytes_ctl->hdr.type != SND_SOC_TPLG_TYPE_BYTES) {
		SNDERR("invalid bytes type %d", bytes_ctl->hdr.type);
		return -EINVAL;
	}

	elem = tplg_elem_new_common(tplg, NULL, bytes_ctl->hdr.name,
		SND_TPLG_TYPE_BYTES);
	if (!elem)
		return -ENOMEM;

	be = elem->bytes_ext;
	be->size = elem->size;
	ret = init_ctl_hdr(tplg, elem, &be->hdr, &bytes_ctl->hdr);
	if (ret < 0) {
		tplg_elem_free(elem);
		return ret;
	}

	be->max = bytes_ctl->max;
	be->mask = bytes_ctl->mask;
	be->base = bytes_ctl->base;
	be->num_regs = bytes_ctl->num_regs;
	be->ext_ops.put = bytes_ctl->ext_ops.put;
	be->ext_ops.get = bytes_ctl->ext_ops.get;

	/* priv data */
	priv = bytes_ctl->priv;
	if (priv && priv->size > 0) {
		ret = tplg_add_data(tplg, elem, priv,
				    sizeof(*priv) + priv->size);
		if (ret < 0)
			return ret;
	}

	/* check on TLV bytes control */
	if (be->hdr.access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
		if ((be->hdr.access & SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE)
			!= SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE) {
			SNDERR("Invalid TLV bytes control access 0x%x",
				be->hdr.access);
			tplg_elem_free(elem);
			return -EINVAL;
		}

		if (!be->max) {
			tplg_elem_free(elem);
			return -EINVAL;
		}
	}

	if (e)
		*e = elem;
	return 0;
}

int tplg_add_mixer_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	return tplg_add_mixer(tplg, t->mixer, NULL);
}

int tplg_add_enum_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	return tplg_add_enum(tplg, t->enum_ctl, NULL);
}

int tplg_add_bytes_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	return tplg_add_bytes(tplg, t->bytes_ctl, NULL);
}

int tplg_decode_control_mixer1(snd_tplg_t *tplg,
			       struct list_head *heap,
			       struct snd_tplg_mixer_template *mt,
			       size_t pos,
			       void *bin, size_t size)
{
	struct snd_soc_tplg_mixer_control *mc = bin;
	struct snd_tplg_channel_map_template *map;
	struct snd_tplg_tlv_dbscale_template *db;
	int i;

	if (size < sizeof(*mc)) {
		SNDERR("mixer: small size %d", size);
		return -EINVAL;
	}

	tplg_log(tplg, 'D', pos, "mixer: size %d TLV size %d private size %d",
		 mc->size, mc->hdr.tlv.size, mc->priv.size);
	if (size != mc->size + mc->priv.size) {
		SNDERR("mixer: unexpected element size %d", size);
		return -EINVAL;
	}

	memset(mt, 0, sizeof(*mt));
	mt->hdr.type = mc->hdr.type;
	mt->hdr.name = mc->hdr.name;
	mt->hdr.access = mc->hdr.access;
	mt->hdr.ops.get = mc->hdr.ops.get;
	mt->hdr.ops.put = mc->hdr.ops.put;
	mt->hdr.ops.info = mc->hdr.ops.info;
	mt->min = mc->min;
	mt->max = mc->max;
	mt->platform_max = mc->platform_max;
	tplg_log(tplg, 'D', pos, "mixer: name '%s' access 0x%x",
		mt->hdr.name, mt->hdr.access);
	if (mc->num_channels > 0) {
		map = tplg_calloc(heap, sizeof(*map));
		map->num_channels = mc->num_channels;
		for (i = 0; i < map->num_channels; i++) {
			map->channel[i].reg = mc->channel[i].reg;
			map->channel[i].shift = mc->channel[i].shift;
			map->channel[i].id = mc->channel[i].id;
		}
		mt->map = map;
	}
	if (mc->hdr.tlv.size == 0) {
		/* nothing */
	} else if (mc->hdr.tlv.size == sizeof(struct snd_soc_tplg_ctl_tlv)) {
		if (mc->hdr.tlv.type != SNDRV_CTL_TLVT_DB_SCALE) {
			SNDERR("mixer: unknown TLV type %d",
			       mc->hdr.tlv.type);
			return -EINVAL;
		}
		db = tplg_calloc(heap, sizeof(*db));
		if (db == NULL)
			return -ENOMEM;
		mt->hdr.tlv_scale = db;
		db->hdr.type = mc->hdr.tlv.type;
		db->min = mc->hdr.tlv.scale.min;
		db->step = mc->hdr.tlv.scale.step;
		db->mute = mc->hdr.tlv.scale.mute;
		tplg_log(tplg, 'D', pos, "mixer: dB scale TLV: min %d step %d mute %d",
			 db->min, db->step, db->mute);
	} else {
		SNDERR("mixer: wrong TLV size %d", mc->hdr.tlv.size);
		return -EINVAL;
	}

	mt->priv = &mc->priv;
	tplg_log(tplg, 'D', pos + offsetof(struct snd_soc_tplg_mixer_control, priv),
		 "mixer: private start");
	return 0;
}

int tplg_decode_control_mixer(snd_tplg_t *tplg,
			      size_t pos,
			      struct snd_soc_tplg_hdr *hdr,
			      void *bin, size_t size)
{
	struct list_head heap;
	snd_tplg_obj_template_t t;
	struct snd_tplg_mixer_template mt;
	struct snd_soc_tplg_mixer_control *mc;
	size_t size2;
	int err;

	err = tplg_decode_template(tplg, pos, hdr, &t);
	if (err < 0)
		return err;

next:
	if (size < sizeof(*mc)) {
		SNDERR("mixer: small size %d", size);
		return -EINVAL;
	}
	INIT_LIST_HEAD(&heap);
	mc = bin;
	size2 = mc->size + mc->priv.size;
	if (size2 > size) {
		SNDERR("mixer: wrong element size (%d, priv %d)",
		       mc->size, mc->priv.size);
		return -EINVAL;
	}

	err = tplg_decode_control_mixer1(tplg, &heap, &mt, pos, bin, size2);
	if (err >= 0) {
		t.mixer = &mt;
		err = snd_tplg_add_object(tplg, &t);
	}
	tplg_free(&heap);
	if (err < 0)
		return err;

	bin += size2;
	size -= size2;
	pos += size2;

	if (size > 0)
		goto next;

	return 0;
}

int tplg_decode_control_enum1(snd_tplg_t *tplg,
			      struct list_head *heap,
			      struct snd_tplg_enum_template *et,
			      size_t pos,
			      struct snd_soc_tplg_enum_control *ec)
{
	int i;

	if (ec->num_channels > SND_TPLG_MAX_CHAN ||
	    ec->num_channels > SND_SOC_TPLG_MAX_CHAN) {
		SNDERR("enum: unexpected channel count %d", ec->num_channels);
		return -EINVAL;
	}
	if (ec->items > SND_SOC_TPLG_NUM_TEXTS) {
		SNDERR("enum: unexpected texts count %d", ec->items);
		return -EINVAL;
	}

	memset(et, 0, sizeof(*et));
	et->hdr.type = ec->hdr.type;
	et->hdr.name = ec->hdr.name;
	et->hdr.access = ec->hdr.access;
	et->hdr.ops.get = ec->hdr.ops.get;
	et->hdr.ops.put = ec->hdr.ops.put;
	et->hdr.ops.info = ec->hdr.ops.info;
	et->mask = ec->mask;

	if (ec->items > 0) {
		et->items = ec->items;
		et->texts = tplg_calloc(heap, sizeof(char *) * ec->items);
		if (!et->texts)
			return -ENOMEM;
		for (i = 0; (unsigned int)i < ec->items; i++)
			et->texts[i] = ec->texts[i];
	}

	et->map = tplg_calloc(heap, sizeof(struct snd_tplg_channel_map_template));
	if (!et->map)
		return -ENOMEM;
	et->map->num_channels = ec->num_channels;
	for (i = 0; i < et->map->num_channels; i++) {
		struct snd_tplg_channel_elem *channel = &et->map->channel[i];

		tplg_log(tplg, 'D', pos + ((void *)&ec->channel[i] - (void *)ec),
			 "enum: channel size %d", ec->channel[i].size);
		channel->reg = ec->channel[i].reg;
		channel->shift = ec->channel[i].shift;
		channel->id = ec->channel[i].id;
	}

	et->priv = &ec->priv;
	return 0;
}

int tplg_decode_control_enum(snd_tplg_t *tplg,
			     size_t pos,
			     struct snd_soc_tplg_hdr *hdr,
			     void *bin, size_t size)
{
	struct list_head heap;
	snd_tplg_obj_template_t t;
	struct snd_tplg_enum_template et;
	struct snd_soc_tplg_enum_control *ec;
	size_t size2;
	int err;

	err = tplg_decode_template(tplg, pos, hdr, &t);
	if (err < 0)
		return err;

next:
	if (size < sizeof(*ec)) {
		SNDERR("enum: small size %d", size);
		return -EINVAL;
	}
	INIT_LIST_HEAD(&heap);
	ec = bin;
	size2 = ec->size + ec->priv.size;
	if (size2 > size) {
		SNDERR("enum: wrong element size (%d, priv %d)",
		       ec->size, ec->priv.size);
		return -EINVAL;
	}

	tplg_log(tplg, 'D', pos, "enum: size %d private size %d",
		 ec->size, ec->priv.size);

	err = tplg_decode_control_enum1(tplg, &heap, &et, pos, ec);
	if (err >= 0) {
		t.enum_ctl = &et;
		err = snd_tplg_add_object(tplg, &t);
	}
	tplg_free(&heap);
	if (err < 0)
		return err;

	bin += size2;
	size -= size2;
	pos += size2;

	if (size > 0)
		goto next;

	return 0;
}

int tplg_decode_control_bytes1(snd_tplg_t *tplg,
			       struct snd_tplg_bytes_template *bt,
			       size_t pos,
			       void *bin, size_t size)
{
	struct snd_soc_tplg_bytes_control *bc = bin;

	if (size < sizeof(*bc)) {
		SNDERR("bytes: small size %d", size);
		return -EINVAL;
	}

	tplg_log(tplg, 'D', pos, "control bytes: size %d private size %d",
		 bc->size, bc->priv.size);
	if (size != bc->size + bc->priv.size) {
		SNDERR("bytes: unexpected element size %d", size);
		return -EINVAL;
	}

	memset(bt, 0, sizeof(*bt));
	bt->hdr.type = bc->hdr.type;
	bt->hdr.name = bc->hdr.name;
	bt->hdr.access = bc->hdr.access;
	bt->hdr.ops.get = bc->hdr.ops.get;
	bt->hdr.ops.put = bc->hdr.ops.put;
	bt->hdr.ops.info = bc->hdr.ops.info;
	bt->max = bc->max;
	bt->mask = bc->mask;
	bt->base = bc->base;
	bt->num_regs = bc->num_regs;
	bt->ext_ops.get = bc->ext_ops.get;
	bt->ext_ops.put = bc->ext_ops.put;
	bt->ext_ops.info = bc->ext_ops.info;
	tplg_log(tplg, 'D', pos, "control bytes: name '%s' access 0x%x",
		 bt->hdr.name, bt->hdr.access);

	bt->priv = &bc->priv;
	return 0;
}

int tplg_decode_control_bytes(snd_tplg_t *tplg,
			      size_t pos,
			      struct snd_soc_tplg_hdr *hdr,
			      void *bin, size_t size)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_bytes_template bt;
	struct snd_soc_tplg_bytes_control *bc;
	size_t size2;
	int err;

	err = tplg_decode_template(tplg, pos, hdr, &t);
	if (err < 0)
		return err;

next:
	if (size < sizeof(*bc)) {
		SNDERR("bytes: small size %d", size);
		return -EINVAL;
	}
	bc = bin;
	size2 = bc->size + bc->priv.size;
	if (size2 > size) {
		SNDERR("bytes: wrong element size (%d, priv %d)",
		       bc->size, bc->priv.size);
		return -EINVAL;
	}

	err = tplg_decode_control_bytes1(tplg, &bt, pos, bin, size);
	if (err < 0)
		return err;

	t.bytes_ctl = &bt;
	err = snd_tplg_add_object(tplg, &t);
	if (err < 0)
		return err;

	bin += size2;
	size -= size2;
	pos += size2;

	if (size > 0)
		goto next;

	return 0;
}
