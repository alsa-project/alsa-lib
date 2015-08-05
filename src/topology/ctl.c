/*
  Copyright(c) 2014-2015 Intel Corporation
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Authors: Mengdong Lin <mengdong.lin@intel.com>
           Yao Jin <yao.jin@intel.com>
           Liam Girdwood <liam.r.girdwood@linux.intel.com>
*/

#include "list.h"
#include "tplg_local.h"

/* copy referenced TLV to the mixer control */
static int copy_tlv(struct tplg_elem *elem, struct tplg_elem *ref)
{
	struct snd_soc_tplg_mixer_control *mixer_ctrl =  elem->mixer_ctrl;
	struct snd_soc_tplg_ctl_tlv *tlv = ref->tlv;

	tplg_dbg("TLV '%s' used by '%s\n", ref->id, elem->id);

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
		if (ref->id == NULL || ref->elem)
			continue;

		if (ref->type == OBJECT_TYPE_TLV) {
			ref->elem = tplg_elem_lookup(&tplg->tlv_list,
						ref->id, OBJECT_TYPE_TLV);
			if (ref->elem)
				 err = copy_tlv(elem, ref->elem);

		} else if (ref->type == OBJECT_TYPE_DATA) {
			ref->elem = tplg_elem_lookup(&tplg->pdata_list,
						ref->id, OBJECT_TYPE_DATA);
			 err = tplg_copy_data(elem, ref->elem);
		}

		if (!ref->elem) {
			SNDERR("error: cannot find '%s' referenced by"
				" control '%s'\n", ref->id, elem->id);
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

	memcpy(ec->texts, ref_elem->texts,
		SND_SOC_TPLG_NUM_TEXTS * SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
}

/* check referenced text for a enum control */
static int tplg_build_enum_control(snd_tplg_t *tplg,
				struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err = 0;

	base = &elem->ref_list;

	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->id == NULL || ref->elem)
			continue;

		if (ref->type == OBJECT_TYPE_TEXT) {
			ref->elem = tplg_elem_lookup(&tplg->text_list,
						ref->id, OBJECT_TYPE_TEXT);
			if (ref->elem)
				copy_enum_texts(elem, ref->elem);

		} else if (ref->type == OBJECT_TYPE_DATA) {
			ref->elem = tplg_elem_lookup(&tplg->pdata_list,
						ref->id, OBJECT_TYPE_DATA);
			err = tplg_copy_data(elem, ref->elem);
		}
		if (!ref->elem) {
			SNDERR("error: cannot find '%s' referenced by"
				" control '%s'\n", ref->id, elem->id);
			return -EINVAL;
		} else if (err < 0)
			return err;
	}

	return 0;
}

/* check referenced private data for a byte control */
static int tplg_build_bytes_control(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;

	base = &elem->ref_list;

	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->id == NULL || ref->elem)
			continue;

		/* bytes control only reference one private data section */
		ref->elem = tplg_elem_lookup(&tplg->pdata_list,
			ref->id, OBJECT_TYPE_DATA);
		if (!ref->elem) {
			SNDERR("error: cannot find data '%s'"
				" referenced by control '%s'\n",
				ref->id, elem->id);
			return -EINVAL;
		}

		/* copy texts to enum elem */
		return tplg_copy_data(elem, ref->elem);
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
	struct snd_soc_tplg_ctl_tlv *tplg_tlv;
	struct snd_soc_tplg_tlv_dbscale *scale;
	const char *id = NULL, *value = NULL;

	tplg_dbg(" scale: %s\n", elem->id);

	tplg_tlv = calloc(1, sizeof(*tplg_tlv));
	if (!tplg_tlv)
		return -ENOMEM;

	elem->tlv = tplg_tlv;
	tplg_tlv->size = sizeof(struct snd_soc_tplg_ctl_tlv);
	tplg_tlv->type = SNDRV_CTL_TLVT_DB_SCALE;
	scale = &tplg_tlv->scale;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get ID */
		if (snd_config_get_id(n, &id) < 0) {
			SNDERR("error: cant get ID\n");
			return -EINVAL;
		}

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		tplg_dbg("\t%s = %s\n", id, value);

		/* get TLV data */
		if (strcmp(id, "min") == 0)
			scale->min = atoi(value);
		else if (strcmp(id, "step") == 0)
			scale->step = atoi(value);
		else if (strcmp(id, "mute") == 0)
			scale->mute = atoi(value);
		else
			SNDERR("error: unknown key %s\n", id);
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

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_TLV);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "scale") == 0) {
			err = tplg_parse_tlv_dbscale(n, elem);
			if (err < 0) {
				SNDERR("error: failed to DBScale");
				return err;
			}
			continue;
		}
	}

	return err;
}

/* Parse Control Bytes */
int tplg_parse_control_bytes(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_bytes_control *be;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_BYTES);
	if (!elem)
		return -ENOMEM;

	be = elem->bytes_ext;
	be->size = elem->size;
	elem_copy_text(be->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	be->hdr.type = SND_SOC_TPLG_TYPE_BYTES;

	tplg_dbg(" Control Bytes: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "index") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			elem->index = atoi(val);
			tplg_dbg("\t%s: %d\n", id, elem->index);
			continue;
		}

		if (strcmp(id, "base") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			be->base = atoi(val);
			tplg_dbg("\t%s: %d\n", id, be->base);
			continue;
		}

		if (strcmp(id, "num_regs") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			be->num_regs = atoi(val);
			tplg_dbg("\t%s: %d\n", id, be->num_regs);
			continue;
		}

		if (strcmp(id, "max") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			be->max = atoi(val);
			tplg_dbg("\t%s: %d\n", id, be->num_regs);
			continue;
		}

		if (strcmp(id, "mask") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			be->mask = strtol(val, NULL, 16);
			tplg_dbg("\t%s: %d\n", id, be->mask);
			continue;
		}

		if (strcmp(id, "data") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			tplg_ref_add(elem, OBJECT_TYPE_DATA, val);
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "tlv") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			err = tplg_ref_add(elem, OBJECT_TYPE_TLV, val);
			if (err < 0)
				return err;

			be->hdr.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
				SNDRV_CTL_ELEM_ACCESS_READWRITE;
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}
	}

	return 0;
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

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_ENUM);
	if (!elem)
		return -ENOMEM;

	ec = elem->enum_ctrl;
	elem_copy_text(ec->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	ec->hdr.type = SND_SOC_TPLG_TYPE_ENUM;
	ec->size = elem->size;
	tplg->channel_idx = 0;

	/* set channel reg to default state */
	for (j = 0; j < SND_SOC_TPLG_MAX_CHAN; j++)
		ec->channel[j].reg = -1;

	tplg_dbg(" Control Enum: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "index") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			elem->index = atoi(val);
			tplg_dbg("\t%s: %d\n", id, elem->index);
			continue;
		}

		if (strcmp(id, "texts") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			tplg_ref_add(elem, OBJECT_TYPE_TEXT, val);
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "channel") == 0) {
			if (ec->num_channels >= SND_SOC_TPLG_MAX_CHAN) {
				SNDERR("error: too many channels %s\n",
					elem->id);
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
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			tplg_ref_add(elem, OBJECT_TYPE_DATA, val);
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}
	}

	return 0;
}

/* Parse Controls.
 *
 * Mixer control. Supports multiple channels.
 */
int tplg_parse_control_mixer(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_mixer_control *mc;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err, j;

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_MIXER);
	if (!elem)
		return -ENOMEM;

	/* init new mixer */
	mc = elem->mixer_ctrl;
	elem_copy_text(mc->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	mc->hdr.type = SND_SOC_TPLG_TYPE_MIXER;
	mc->size = elem->size;
	tplg->channel_idx = 0;

	/* set channel reg to default state */
	for (j = 0; j < SND_SOC_TPLG_MAX_CHAN; j++)
		mc->channel[j].reg = -1;

	tplg_dbg(" Control Mixer: %s\n", elem->id);

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

		if (strcmp(id, "index") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			elem->index = atoi(val);
			tplg_dbg("\t%s: %d\n", id, elem->index);
			continue;
		}

		if (strcmp(id, "channel") == 0) {
			if (mc->num_channels >= SND_SOC_TPLG_MAX_CHAN) {
				SNDERR("error: too many channels %s\n",
					elem->id);
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
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			mc->max = atoi(val);
			tplg_dbg("\t%s: %d\n", id, mc->max);
			continue;
		}

		if (strcmp(id, "invert") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			if (strcmp(val, "true") == 0)
				mc->invert = 1;
			else if (strcmp(val, "false") == 0)
				mc->invert = 0;

			tplg_dbg("\t%s: %d\n", id, mc->invert);
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

			err = tplg_ref_add(elem, OBJECT_TYPE_TLV, val);
			if (err < 0)
				return err;

			mc->hdr.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
				SNDRV_CTL_ELEM_ACCESS_READWRITE;
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "data") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			tplg_ref_add(elem, OBJECT_TYPE_DATA, val);
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}
	}

	return 0;
}
