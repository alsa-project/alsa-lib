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

/* mapping of kcontrol text names to types */
static const struct map_elem control_map[] = {
	{"volsw", SND_SOC_TPLG_CTL_VOLSW},
	{"volsw_sx", SND_SOC_TPLG_CTL_VOLSW_SX},
	{"volsw_xr_sx", SND_SOC_TPLG_CTL_VOLSW_XR_SX},
	{"enum", SND_SOC_TPLG_CTL_ENUM},
	{"bytes", SND_SOC_TPLG_CTL_BYTES},
	{"enum_value", SND_SOC_TPLG_CTL_ENUM_VALUE},
	{"range", SND_SOC_TPLG_CTL_RANGE},
	{"strobe", SND_SOC_TPLG_CTL_STROBE},
};

static int lookup_ops(const char *c)
{
	int i;
	long ret;

	for (i = 0; i < (int)ARRAY_SIZE(control_map); i++) {
		if (strcmp(control_map[i].name, c) == 0)
			return control_map[i].id;
	}

	/* cant find string name in our table so we use its ID number */
	i = safe_strtol(c, &ret);
	if (i < 0) {
		SNDERR("wrong kcontrol ops value string '%s'", c);
		return i;
	}

	return ret;
}

const char *tplg_ops_name(int type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(control_map); i++) {
		if (control_map[i].id == type)
			return control_map[i].name;
	}

	return NULL;
}

/* Parse Control operations. Ops can come from standard names above or
 * bespoke driver controls with numbers >= 256
 */
int tplg_parse_ops(snd_tplg_t *tplg ATTRIBUTE_UNUSED, snd_config_t *cfg,
		   void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct snd_soc_tplg_ctl_hdr *hdr = private;
	const char *id, *value;
	int ival;

	tplg_dbg("\tOps");
	hdr->size = sizeof(*hdr);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* get value - try strings then ints */
		if (snd_config_get_type(n) == SND_CONFIG_TYPE_STRING) {
			if (snd_config_get_string(n, &value) < 0)
				continue;
			ival = lookup_ops(value);
		} else {
			if (tplg_get_integer(n, &ival, 0))
				continue;
		}

		if (strcmp(id, "info") == 0)
			hdr->ops.info = ival;
		else if (strcmp(id, "put") == 0)
			hdr->ops.put = ival;
		else if (strcmp(id, "get") == 0)
			hdr->ops.get = ival;

		tplg_dbg("\t\t%s = %d", id, ival);
	}

	return 0;
}

/* save control operations */
int tplg_save_ops(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		  struct snd_soc_tplg_ctl_hdr *hdr,
		  struct tplg_buf *dst, const char *pfx)
{
	const char *s;
	int err;

	if (hdr->ops.info + hdr->ops.get + hdr->ops.put == 0)
		return 0;
	err = tplg_save_printf(dst, pfx, "ops.0 {\n");
	if (err >= 0 && hdr->ops.info > 0) {
		s = tplg_ops_name(hdr->ops.info);
		if (s == NULL)
			err = tplg_save_printf(dst, pfx, "\tinfo %u\n",
					       hdr->ops.info);
		else
			err = tplg_save_printf(dst, pfx, "\tinfo %s\n", s);
	}
	if (err >= 0 && hdr->ops.get > 0) {
		s = tplg_ops_name(hdr->ops.get);
		if (s == NULL)
			err = tplg_save_printf(dst, pfx, "\tget %u\n",
					       hdr->ops.get);
		else
			err = tplg_save_printf(dst, pfx, "\tget %s\n", s);
	}
	if (err >= 0 && hdr->ops.put > 0) {
		s = tplg_ops_name(hdr->ops.put);
		if (s == NULL)
			err = tplg_save_printf(dst, pfx, "\tput %u\n",
					       hdr->ops.put);
		else
			err = tplg_save_printf(dst, pfx, "\tput %s\n", s);
	}
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* Parse External Control operations. Ops can come from standard names above or
 * bespoke driver controls with numbers >= 256
 */
int tplg_parse_ext_ops(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		       snd_config_t *cfg, void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct snd_soc_tplg_bytes_control *be = private;
	const char *id, *value;
	int ival;

	tplg_dbg("\tExt Ops");

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* get value - try strings then ints */
		if (snd_config_get_type(n) == SND_CONFIG_TYPE_STRING) {
			if (snd_config_get_string(n, &value) < 0)
				continue;
			ival = lookup_ops(value);
		} else {
			if (tplg_get_integer(n, &ival, 0))
				continue;
		}

		if (strcmp(id, "info") == 0)
			be->ext_ops.info = ival;
		else if (strcmp(id, "put") == 0)
			be->ext_ops.put = ival;
		else if (strcmp(id, "get") == 0)
			be->ext_ops.get = ival;

		tplg_dbg("\t\t%s = %s", id, value);
	}

	return 0;
}

/* save external control operations */
int tplg_save_ext_ops(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		      struct snd_soc_tplg_bytes_control *be,
		      struct tplg_buf *dst, const char *pfx)
{
	const char *s;
	int err;

	if (be->ext_ops.info + be->ext_ops.get + be->ext_ops.put == 0)
		return 0;
	err = tplg_save_printf(dst, pfx, "extops.0 {\n");
	if (err >= 0 && be->ext_ops.info > 0) {
		s = tplg_ops_name(be->ext_ops.info);
		if (s == NULL)
			err = tplg_save_printf(dst, pfx, "\tinfo %u\n",
					       be->ext_ops.info);
		else
			err = tplg_save_printf(dst, pfx, "\tinfo %s\n", s);
	}
	if (err >= 0 && be->ext_ops.get > 0) {
		s = tplg_ops_name(be->ext_ops.get);
		if (s == NULL)
			err = tplg_save_printf(dst, pfx, "\tget %u\n",
					       be->ext_ops.get);
		else
			err = tplg_save_printf(dst, pfx, "\tget %s\n", s);
	}
	if (err >= 0 && be->ext_ops.put > 0) {
		s = tplg_ops_name(be->ext_ops.put);
		if (s == NULL)
			err = tplg_save_printf(dst, pfx, "\tput %u\n",
					       be->ext_ops.put);
		else
			err = tplg_save_printf(dst, pfx, "\tput %s\n", s);
	}
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}
