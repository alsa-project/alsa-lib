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

struct tplg_table tplg_table[] = {
	{
		.name  = "manifest",
		.loff  = offsetof(snd_tplg_t, manifest_list),
		.type  = SND_TPLG_TYPE_MANIFEST,
		.tsoc  = SND_SOC_TPLG_TYPE_MANIFEST,
		.size  = sizeof(struct snd_soc_tplg_manifest),
		.enew  = 1,
	},
	{
		.name  = "control mixer",
		.loff  = offsetof(snd_tplg_t, mixer_list),
		.type  = SND_TPLG_TYPE_MIXER,
		.tsoc  = SND_SOC_TPLG_TYPE_MIXER,
		.size  = sizeof(struct snd_soc_tplg_mixer_control),
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "control enum",
		.loff  = offsetof(snd_tplg_t, enum_list),
		.type  = SND_TPLG_TYPE_ENUM,
		.tsoc  = SND_SOC_TPLG_TYPE_ENUM,
		.size  = sizeof(struct snd_soc_tplg_enum_control),
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "control extended (bytes)",
		.loff  = offsetof(snd_tplg_t, bytes_ext_list),
		.type  = SND_TPLG_TYPE_BYTES,
		.tsoc  = SND_SOC_TPLG_TYPE_BYTES,
		.size  = sizeof(struct snd_soc_tplg_bytes_control),
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "dapm widget",
		.loff  = offsetof(snd_tplg_t, widget_list),
		.type  = SND_TPLG_TYPE_DAPM_WIDGET,
		.tsoc  = SND_SOC_TPLG_TYPE_DAPM_WIDGET,
		.size  = sizeof(struct snd_soc_tplg_dapm_widget),
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "pcm",
		.loff  = offsetof(snd_tplg_t, pcm_list),
		.type  = SND_TPLG_TYPE_PCM,
		.tsoc  = SND_SOC_TPLG_TYPE_PCM,
		.size  = sizeof(struct snd_soc_tplg_pcm),
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "physical dai",
		.loff  = offsetof(snd_tplg_t, dai_list),
		.type  = SND_TPLG_TYPE_DAI,
		.tsoc  = SND_SOC_TPLG_TYPE_DAI,
		.size  = sizeof(struct snd_soc_tplg_dai),
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "be",
		.loff  = offsetof(snd_tplg_t, be_list),
		.type  = SND_TPLG_TYPE_BE,
		.tsoc  = SND_SOC_TPLG_TYPE_BACKEND_LINK,
		.size  = sizeof(struct snd_soc_tplg_link_config),
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "cc",
		.loff  = offsetof(snd_tplg_t, cc_list),
		.type  = SND_TPLG_TYPE_CC,
		.tsoc  = SND_SOC_TPLG_TYPE_CODEC_LINK,
		.size  = sizeof(struct snd_soc_tplg_link_config),
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "route (dapm graph)",
		.loff  = offsetof(snd_tplg_t, route_list),
		.type  = SND_TPLG_TYPE_DAPM_GRAPH,
		.tsoc  = SND_SOC_TPLG_TYPE_DAPM_GRAPH,
		.build = 1,
	},
	{
		.name  = "private data",
		.loff  = offsetof(snd_tplg_t, pdata_list),
		.type  = SND_TPLG_TYPE_DATA,
		.tsoc  = SND_SOC_TPLG_TYPE_PDATA,
		.build = 1,
		.enew  = 1,
	},
	{
		.name  = "text",
		.loff  = offsetof(snd_tplg_t, text_list),
		.type  = SND_TPLG_TYPE_TEXT,
		.size  = sizeof(struct tplg_texts),
		.enew  = 1,
	},
	{
		.name  = "tlv",
		.loff  = offsetof(snd_tplg_t, tlv_list),
		.type  = SND_TPLG_TYPE_TLV,
		.size  = sizeof(struct snd_soc_tplg_ctl_tlv),
		.enew  = 1,
	},
	{
		.name  = "stream config",
		.loff  = offsetof(snd_tplg_t, pcm_config_list),
		.type  = SND_TPLG_TYPE_STREAM_CONFIG,
		.size  = sizeof(struct snd_soc_tplg_stream),
		.enew  = 1,
	},
	{
		.name  = "stream capabilities",
		.loff  = offsetof(snd_tplg_t, pcm_caps_list),
		.type  = SND_TPLG_TYPE_STREAM_CAPS,
		.size  = sizeof(struct snd_soc_tplg_stream_caps),
		.enew  = 1,
	},
	{
		.name  = "token",
		.loff  = offsetof(snd_tplg_t, token_list),
		.type  = SND_TPLG_TYPE_TOKEN,
		.enew  = 1,
	},
	{
		.name  = "tuple",
		.loff  = offsetof(snd_tplg_t, tuple_list),
		.type  = SND_TPLG_TYPE_TUPLE,
		.free  = tplg_free_tuples,
		.enew  = 1,
	},
	{
		.name  = "hw config",
		.loff  = offsetof(snd_tplg_t, hw_cfg_list),
		.type  = SND_TPLG_TYPE_HW_CONFIG,
		.size  = sizeof(struct snd_soc_tplg_hw_config),
		.enew  = 1,
	}
};

unsigned int tplg_table_items = ARRAY_SIZE(tplg_table);

int tplg_ref_add(struct tplg_elem *elem, int type, const char* id)
{
	struct tplg_ref *ref;

	ref = calloc(1, sizeof(*ref));
	if (!ref)
		return -ENOMEM;

	strncpy(ref->id, id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	ref->id[SNDRV_CTL_ELEM_ID_NAME_MAXLEN - 1] = 0;
	ref->type = type;

	list_add_tail(&ref->list, &elem->ref_list);
	return 0;
}

/* directly add a reference elem */
int tplg_ref_add_elem(struct tplg_elem *elem, struct tplg_elem *elem_ref)
{
	struct tplg_ref *ref;

	ref = calloc(1, sizeof(*ref));
	if (!ref)
		return -ENOMEM;

	ref->type = elem_ref->type;
	ref->elem = elem_ref;
	snd_strlcpy(ref->id, elem_ref->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	list_add_tail(&ref->list, &elem->ref_list);
	return 0;
}

void tplg_ref_free_list(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct tplg_ref *ref;

	list_for_each_safe(pos, npos, base) {
		ref = list_entry(pos, struct tplg_ref, list);
		list_del(&ref->list);
		free(ref);
	}
}

struct tplg_elem *tplg_elem_new(void)
{
	struct tplg_elem *elem;

	elem = calloc(1, sizeof(*elem));
	if (!elem)
		return NULL;

	INIT_LIST_HEAD(&elem->ref_list);
	return elem;
}

void tplg_elem_free(struct tplg_elem *elem)
{
	tplg_ref_free_list(&elem->ref_list);

	/* free struct snd_tplg_ object,
	 * the union pointers share the same address
	 */
	if (elem->obj) {
		if (elem->free)
			elem->free(elem->obj);

		free(elem->obj);
	}

	free(elem);
}

void tplg_elem_free_list(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct tplg_elem *elem;

	list_for_each_safe(pos, npos, base) {
		elem = list_entry(pos, struct tplg_elem, list);
		list_del(&elem->list);
		tplg_elem_free(elem);
	}
}

struct tplg_elem *tplg_elem_lookup(struct list_head *base, const char* id,
				   unsigned int type, int index)
{
	struct list_head *pos;
	struct tplg_elem *elem;

	if (!base || !id)
		return NULL;

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);

		if (!strcmp(elem->id, id) && elem->type == type)
			return elem;
		/* SND_TPLG_INDEX_ALL is the default value "0" and applicable
		   for all use cases */
		if ((index != SND_TPLG_INDEX_ALL)
			&& (elem->index > index))
			break;
	}

	return NULL;
}

/* insert a new element into list in the ascending order of index value */
void tplg_elem_insert(struct tplg_elem *elem_p, struct list_head *list)
{
	struct list_head *pos, *p = &(elem_p->list);
	struct tplg_elem *elem;

	list_for_each(pos, list) {
		elem = list_entry(pos, struct tplg_elem, list);
		if (elem_p->index < elem->index)
			break;
	}
	/* insert item before pos */
	list_insert(p, pos->prev, pos);
}

/* create a new common element and object */
struct tplg_elem* tplg_elem_new_common(snd_tplg_t *tplg,
				       snd_config_t *cfg,
				       const char *name,
				       enum snd_tplg_type type)
{
	struct tplg_table *tptr;
	struct tplg_elem *elem;
	struct list_head *list;
	const char *id;
	int obj_size = 0;
	unsigned index;
	void *obj;
	snd_config_iterator_t i, next;
	snd_config_t *n;

	if (!cfg && !name)
		return NULL;

	elem = tplg_elem_new();
	if (!elem)
		return NULL;

	/* do we get name from cfg */
	if (cfg) {
		snd_config_get_id(cfg, &id);
		snd_strlcpy(elem->id, id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		elem->id[SNDRV_CTL_ELEM_ID_NAME_MAXLEN - 1] = 0;
		/* as we insert new elem based on the index value, move index
		   parsing here */
		snd_config_for_each(i, next, cfg) {
			n = snd_config_iterator_entry(i);
			if (snd_config_get_id(n, &id))
				continue;
			if (strcmp(id, "index") == 0) {
				if (tplg_get_integer(n, &elem->index, 0)) {
					free(elem);
					return NULL;
				}
				if (elem->index < 0) {
					free(elem);
					return NULL;
				}
			}
		}
	} else if (name != NULL)
		snd_strlcpy(elem->id, name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	for (index = 0; index < tplg_table_items; index++) {
		tptr = &tplg_table[index];
		if (!tptr->enew)
			continue;
		if ((int)type != tptr->type)
			continue;
		break;
	}
	if (index >= tplg_table_items) {
		free(elem);
		return NULL;
	}

	list = (struct list_head *)((void *)tplg + tptr->loff);
	tplg_elem_insert(elem, list);
	obj_size = tptr->size;
	elem->free = tptr->free;

	/* create new object too if required */
	if (obj_size > 0) {
		obj = calloc(1, obj_size);
		if (obj == NULL) {
			free(elem);
			return NULL;
		}

		elem->obj = obj;
		elem->size = obj_size;
	}

	elem->type = type;
	return elem;
}
