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

#define TEXT_SIZE_MAX \
	(SND_SOC_TPLG_NUM_TEXTS * SNDRV_CTL_ELEM_ID_NAME_MAXLEN)

static int parse_text_values(snd_config_t *cfg, struct tplg_elem *elem)
{
	struct tplg_texts *texts = elem->texts;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *value = NULL;
	int j = 0;

	tplg_dbg(" Text Values: %s", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (j == SND_SOC_TPLG_NUM_TEXTS) {
			tplg_dbg("text string number exceeds %d", j);
			return -ENOMEM;
		}

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		snd_strlcpy(&texts->items[j][0], value,
			SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		tplg_dbg("\t%s", &texts->items[j][0]);

		j++;
	}

	texts->num_items = j;
	return 0;
}

/* Parse Text data */
int tplg_parse_text(snd_tplg_t *tplg, snd_config_t *cfg,
		    void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err = 0;
	struct tplg_elem *elem;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_TEXT);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "values") == 0) {
			err = parse_text_values(n, elem);
			if (err < 0) {
				SNDERR("error: failed to parse text values");
				return err;
			}
			continue;
		}
	}

	return err;
}

/* save text data */
int tplg_save_text(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		   struct tplg_elem *elem,
		   struct tplg_buf *dst, const char *pfx)
{
	struct tplg_texts *texts = elem->texts;
	unsigned int i;
	int err;

	if (!texts || texts->num_items == 0)
		return 0;
	err = tplg_save_printf(dst, pfx, "'%s'.values [\n", elem->id);
	for (i = 0; err >= 0 && i < texts->num_items; i++)
		err = tplg_save_printf(dst, pfx, "\t'%s'\n", texts->items[i]);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "]\n");
	return err;
}
