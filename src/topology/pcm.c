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

struct tplg_elem *lookup_pcm_dai_stream(struct list_head *base, const char* id)
{
	struct list_head *pos;
	struct tplg_elem *elem;
	struct snd_soc_tplg_pcm_dai *pcm_dai;

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (elem->type != OBJECT_TYPE_PCM)
			return NULL;

		pcm_dai = elem->pcm;

		if (pcm_dai && (!strcmp(pcm_dai->capconf[0].caps.name, id)
			|| !strcmp(pcm_dai->capconf[1].caps.name, id)))
			return elem;
	}

	return NULL;
}

/* copy referenced caps to the pcm */
static void copy_pcm_caps(const char *id, struct snd_soc_tplg_stream_caps *caps,
	struct tplg_elem *ref_elem)
{
	struct snd_soc_tplg_stream_caps *ref_caps = ref_elem->stream_caps;

	tplg_dbg("Copy pcm caps (%ld bytes) from '%s' to '%s' \n",
		sizeof(*caps), ref_elem->id, id);

	*caps =  *ref_caps;
}

/* copy referenced config to the pcm */
static void copy_pcm_config(const char *id,
	struct snd_soc_tplg_stream_config *cfg, struct tplg_elem *ref_elem)
{
	struct snd_soc_tplg_stream_config *ref_cfg = ref_elem->stream_cfg;

	tplg_dbg("Copy pcm config (%ld bytes) from '%s' to '%s' \n",
		sizeof(*cfg), ref_elem->id, id);

	*cfg = *ref_cfg;
}

/* check referenced config and caps for a pcm */
static int tplg_build_pcm_cfg_caps(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_elem *ref_elem = NULL;
	struct snd_soc_tplg_pcm_cfg_caps *capconf;
	struct snd_soc_tplg_pcm_dai *pcm_dai;
	unsigned int i, j;

	switch (elem->type) {
	case OBJECT_TYPE_PCM:
		pcm_dai = elem->pcm;
		break;
	case OBJECT_TYPE_BE:
		pcm_dai = elem->be;
		break;
	case OBJECT_TYPE_CC:
		pcm_dai = elem->cc;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < 2; i++) {
		capconf = &pcm_dai->capconf[i];

		ref_elem = tplg_elem_lookup(&tplg->pcm_caps_list,
			capconf->caps.name, OBJECT_TYPE_STREAM_CAPS);

		if (ref_elem != NULL)
			copy_pcm_caps(elem->id, &capconf->caps, ref_elem);

		for (j = 0; j < capconf->num_configs; j++) {
			ref_elem = tplg_elem_lookup(&tplg->pcm_config_list,
				capconf->configs[j].name,
				OBJECT_TYPE_STREAM_CONFIG);

			if (ref_elem != NULL)
				copy_pcm_config(elem->id,
					&capconf->configs[j],
					ref_elem);
		}
	}

	return 0;
}

int tplg_build_pcm_dai(snd_tplg_t *tplg, unsigned int type)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	switch (type) {
	case OBJECT_TYPE_PCM:
		base = &tplg->pcm_list;
		break;
	case OBJECT_TYPE_BE:
		base = &tplg->be_list;
		break;
	case OBJECT_TYPE_CC:
		base = &tplg->cc_list;
		break;
	default:
		return -EINVAL;
	}

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (elem->type != type) {
			SNDERR("error: invalid elem '%s'\n", elem->id);
			return -EINVAL;
		}

		err = tplg_build_pcm_cfg_caps(tplg, elem);
		if (err < 0)
			return err;
	}

	return 0;
}

/* PCM stream configuration */
static int tplg_parse_stream_cfg(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
	snd_config_t *cfg, void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct snd_soc_tplg_stream_config *sc = private;
	struct snd_soc_tplg_stream *stream;
	const char *id, *val;
	snd_pcm_format_t format;
	int ret;

	snd_config_get_id(cfg, &id);

	if (strcmp(id, "playback") == 0)
		stream = &sc->playback;
	else if (strcmp(id, "capture") == 0)
		stream = &sc->capture;
	else
		return -EINVAL;

	tplg_dbg("\t%s:\n", id);

	stream->size = sizeof(*stream);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			return -EINVAL;

		if (snd_config_get_string(n, &val) < 0)
			return -EINVAL;

		if (strcmp(id, "format") == 0) {
			format = snd_pcm_format_value(val);
			if (format == SND_PCM_FORMAT_UNKNOWN) {
				SNDERR("error: unsupported stream format %s\n",
					val);
				return -EINVAL;
			}

			stream->format = format;
			tplg_dbg("\t\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "rate") == 0) {
			stream->rate = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, stream->rate);
			continue;
		}

		if (strcmp(id, "channels") == 0) {
			stream->channels = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, stream->channels);
			continue;
		}

		if (strcmp(id, "tdm_slot") == 0) {
			stream->tdm_slot = strtol(val, NULL, 16);
			tplg_dbg("\t\t%s: 0x%x\n", id, stream->tdm_slot);
			continue;
		}
	}

	return 0;
}

/* Parse pcm configuration */
int tplg_parse_pcm_config(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_stream_config *sc;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_STREAM_CONFIG);
	if (!elem)
		return -ENOMEM;

	sc = elem->stream_cfg;
	sc->size = elem->size;

	tplg_dbg(" PCM Config: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "config") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_stream_cfg, sc);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

static int split_format(struct snd_soc_tplg_stream_caps *caps, char *str)
{
	char *s = NULL;
	snd_pcm_format_t format;
	int i = 0, ret;

	s = strtok(str, ",");
	while ((s != NULL) && (i < SND_SOC_TPLG_MAX_FORMATS)) {
		format = snd_pcm_format_value(s);
		if (format == SND_PCM_FORMAT_UNKNOWN) {
			SNDERR("error: unsupported stream format %s\n", s);
			return -EINVAL;
		}

		caps->formats[i] = format;
		s = strtok(NULL, ", ");
		i++;
	}

	return 0;
}

/* Parse pcm Capabilities */
int tplg_parse_pcm_caps(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_stream_caps *sc;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val;
	char *s;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_STREAM_CAPS);
	if (!elem)
		return -ENOMEM;

	sc = elem->stream_caps;
	sc->size = elem->size;
	elem_copy_text(sc->name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" PCM Capabilities: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (snd_config_get_string(n, &val) < 0)
			return -EINVAL;

		if (strcmp(id, "formats") == 0) {
			s = strdup(val);
			if (s == NULL)
				return -ENOMEM;

			err = split_format(sc, s);
			free(s);

			if (err < 0)
				return err;

			tplg_dbg("\t\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "rate_min") == 0) {
			sc->rate_min = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, sc->rate_min);
			continue;
		}

		if (strcmp(id, "rate_max") == 0) {
			sc->rate_max = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, sc->rate_max);
			continue;
		}

		if (strcmp(id, "channels_min") == 0) {
			sc->channels_min = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, sc->channels_min);
			continue;
		}

		if (strcmp(id, "channels_max") == 0) {
			sc->channels_max = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, sc->channels_max);
			continue;
		}
	}

	return 0;
}

static int tplg_parse_pcm_cfg(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
	snd_config_t *cfg, void *private)
{
	struct snd_soc_tplg_pcm_cfg_caps *capconf = private;
	struct snd_soc_tplg_stream_config *configs = capconf->configs;
	unsigned int *num_configs = &capconf->num_configs;
	const char *value;

	if (*num_configs == SND_SOC_TPLG_STREAM_CONFIG_MAX)
		return -EINVAL;

	if (snd_config_get_string(cfg, &value) < 0)
		return EINVAL;

	elem_copy_text(configs[*num_configs].name, value,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	*num_configs += 1;

	tplg_dbg("\t\t\t%s\n", value);

	return 0;
}

/* Parse the cap and config of a pcm */
int tplg_parse_pcm_cap_cfg(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct tplg_elem *elem = private;
	struct snd_soc_tplg_pcm_dai *pcm_dai;
	const char *id, *value;
	int err, stream;

	if (elem->type == OBJECT_TYPE_PCM)
		pcm_dai = elem->pcm;
	else if (elem->type == OBJECT_TYPE_BE)
		pcm_dai = elem->be;
	else if (elem->type == OBJECT_TYPE_CC)
		pcm_dai = elem->cc;
	else
		return -EINVAL;

	snd_config_get_id(cfg, &id);

	tplg_dbg("\t%s:\n", id);

	if (strcmp(id, "playback") == 0) {
		stream = SND_SOC_TPLG_STREAM_PLAYBACK;
		pcm_dai->playback = 1;
	} else if (strcmp(id, "capture") == 0) {
		stream = SND_SOC_TPLG_STREAM_CAPTURE;
		pcm_dai->capture = 1;
	} else
		return -EINVAL;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "capabilities") == 0) {
			if (snd_config_get_string(n, &value) < 0)
				continue;

			elem_copy_text(pcm_dai->capconf[stream].caps.name, value,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

			tplg_dbg("\t\t%s\n\t\t\t%s\n", id, value);
			continue;
		}

		if (strcmp(id, "configs") == 0) {
			tplg_dbg("\t\tconfigs:\n");
			err = tplg_parse_compound(tplg, n, tplg_parse_pcm_cfg,
				&pcm_dai->capconf[stream]);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/* Parse pcm */
int tplg_parse_pcm(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_pcm_dai *pcm_dai;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_PCM);
	if (!elem)
		return -ENOMEM;

	pcm_dai = elem->pcm;
	pcm_dai->size = elem->size;
	elem_copy_text(pcm_dai->name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" PCM: %s\n", elem->id);

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

		if (strcmp(id, "id") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			pcm_dai->id = atoi(val);
			tplg_dbg("\t%s: %d\n", id, pcm_dai->id);
			continue;
		}

		if (strcmp(id, "pcm") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_pcm_cap_cfg, elem);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/* Parse be */
int tplg_parse_be(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_pcm_dai *pcm_dai;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_BE);
	if (!elem)
		return -ENOMEM;

	pcm_dai = elem->be;
	pcm_dai->size = elem->size;
	elem_copy_text(pcm_dai->name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" BE: %s\n", elem->id);

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

		if (strcmp(id, "id") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			pcm_dai->id = atoi(val);
			tplg_dbg("\t%s: %d\n", id, pcm_dai->id);
			continue;
		}

		if (strcmp(id, "be") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_pcm_cap_cfg, elem);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/* Parse cc */
int tplg_parse_cc(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_pcm_dai *pcm_dai;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, OBJECT_TYPE_CC);
	if (!elem)
		return -ENOMEM;

	pcm_dai = elem->cc;
	pcm_dai->size = elem->size;

	tplg_dbg(" CC: %s\n", elem->id);

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

		if (strcmp(id, "id") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			pcm_dai->id = atoi(val);
			tplg_dbg("\t%s: %d\n", id, pcm_dai->id);
			continue;
		}

		if (strcmp(id, "cc") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_pcm_cap_cfg, elem);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}
