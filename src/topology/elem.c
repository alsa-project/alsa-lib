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
	elem_copy_text(ref->id,  elem_ref->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

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

/* insert a new element into list in the ascending order of index value*/
static void tplg_elem_insert(struct tplg_elem *elem_p, struct list_head *list)
{
	struct list_head *pos, *p = &(elem_p->list);
	struct tplg_elem *elem;

	list_for_each(pos, list) {
		elem = list_entry(pos, struct tplg_elem, list);
		if (elem_p->index < elem->index)
			break;
	}
	p->prev = pos->prev;
	pos->prev->next = p;
	pos->prev = p;
	p->next = pos;
}

/* create a new common element and object */
struct tplg_elem* tplg_elem_new_common(snd_tplg_t *tplg,
	snd_config_t *cfg, const char *name, enum snd_tplg_type type)
{
	struct tplg_elem *elem;
	const char *id, *val = NULL;
	int obj_size = 0;
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
		elem_copy_text(elem->id, id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		elem->id[SNDRV_CTL_ELEM_ID_NAME_MAXLEN - 1] = 0;
		/* as we insert new elem based on the index value, move index
		   parsing here */
		snd_config_for_each(i, next, cfg) {
			n = snd_config_iterator_entry(i);
			if (snd_config_get_id(n, &id))
				continue;
			if (strcmp(id, "index") == 0) {
				if (snd_config_get_string(n, &val) < 0) {
					free(elem);
					return NULL;
				}
				elem->index = atoi(val);
			}
		}
	} else if (name != NULL)
		elem_copy_text(elem->id, name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	switch (type) {
	case SND_TPLG_TYPE_DATA:
		tplg_elem_insert(elem, &tplg->pdata_list);
		break;
	case SND_TPLG_TYPE_MANIFEST:
		tplg_elem_insert(elem, &tplg->manifest_list);
		obj_size = sizeof(struct snd_soc_tplg_manifest);
		break;
	case SND_TPLG_TYPE_TEXT:
		tplg_elem_insert(elem, &tplg->text_list);
		obj_size = sizeof(struct tplg_texts);
		break;
	case SND_TPLG_TYPE_TLV:
		tplg_elem_insert(elem, &tplg->tlv_list);
		elem->size = sizeof(struct snd_soc_tplg_ctl_tlv);
		break;
	case SND_TPLG_TYPE_BYTES:
		tplg_elem_insert(elem, &tplg->bytes_ext_list);
		obj_size = sizeof(struct snd_soc_tplg_bytes_control);
		break;
	case SND_TPLG_TYPE_ENUM:
		tplg_elem_insert(elem, &tplg->enum_list);
		obj_size = sizeof(struct snd_soc_tplg_enum_control);
		break;
	case SND_TPLG_TYPE_MIXER:
		tplg_elem_insert(elem, &tplg->mixer_list);
		obj_size = sizeof(struct snd_soc_tplg_mixer_control);
		break;
	case SND_TPLG_TYPE_DAPM_WIDGET:
		tplg_elem_insert(elem, &tplg->widget_list);
		obj_size = sizeof(struct snd_soc_tplg_dapm_widget);
		break;
	case SND_TPLG_TYPE_STREAM_CONFIG:
		tplg_elem_insert(elem, &tplg->pcm_config_list);
		obj_size = sizeof(struct snd_soc_tplg_stream);
		break;
	case SND_TPLG_TYPE_STREAM_CAPS:
		tplg_elem_insert(elem, &tplg->pcm_caps_list);
		obj_size = sizeof(struct snd_soc_tplg_stream_caps);
		break;
	case SND_TPLG_TYPE_PCM:
		tplg_elem_insert(elem, &tplg->pcm_list);
		obj_size = sizeof(struct snd_soc_tplg_pcm);
		break;
	case SND_TPLG_TYPE_DAI:
		tplg_elem_insert(elem, &tplg->dai_list);
		obj_size = sizeof(struct snd_soc_tplg_dai);
		break;
	case SND_TPLG_TYPE_BE:
	case SND_TPLG_TYPE_LINK:
		tplg_elem_insert(elem, &tplg->be_list);
		obj_size = sizeof(struct snd_soc_tplg_link_config);
		break;
	case SND_TPLG_TYPE_CC:
		tplg_elem_insert(elem, &tplg->cc_list);
		obj_size = sizeof(struct snd_soc_tplg_link_config);
		break;
	case SND_TPLG_TYPE_TOKEN:
		tplg_elem_insert(elem, &tplg->token_list);
		break;
	case SND_TPLG_TYPE_TUPLE:
		tplg_elem_insert(elem, &tplg->tuple_list);
		elem->free = tplg_free_tuples;
		break;
	case SND_TPLG_TYPE_HW_CONFIG:
		tplg_elem_insert(elem, &tplg->hw_cfg_list);
		obj_size = sizeof(struct snd_soc_tplg_hw_config);
		break;
	default:
		free(elem);
		return NULL;
	}

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
