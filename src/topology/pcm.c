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

#define RATE(v) [SND_PCM_RATE_##v] = #v

static const char *const snd_pcm_rate_names[] = {
	RATE(5512),
	RATE(8000),
	RATE(11025),
	RATE(16000),
	RATE(22050),
	RATE(32000),
	RATE(44100),
	RATE(48000),
	RATE(64000),
	RATE(88200),
	RATE(96000),
	RATE(176400),
	RATE(192000),
	RATE(CONTINUOUS),
	RATE(KNOT),
};

struct tplg_elem *lookup_pcm_dai_stream(struct list_head *base, const char* id)
{
	struct list_head *pos;
	struct tplg_elem *elem;
	struct snd_soc_tplg_pcm *pcm;

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (elem->type != SND_TPLG_TYPE_PCM)
			return NULL;

		pcm = elem->pcm;

		if (pcm && !strcmp(pcm->dai_name, id))
			return elem;
	}

	return NULL;
}

/* copy referenced caps to the parent (pcm or be dai) */
static void copy_stream_caps(const char *id ATTRIBUTE_UNUSED,
			     struct snd_soc_tplg_stream_caps *caps,
			     struct tplg_elem *ref_elem)
{
	struct snd_soc_tplg_stream_caps *ref_caps = ref_elem->stream_caps;

	tplg_dbg("Copy pcm caps (%ld bytes) from '%s' to '%s'",
		sizeof(*caps), ref_elem->id, id);

	*caps =  *ref_caps;
}

/* find and copy the referenced stream caps */
static int tplg_build_stream_caps(snd_tplg_t *tplg,
				  const char *id, int index,
				  struct snd_soc_tplg_stream_caps *caps)
{
	struct tplg_elem *ref_elem = NULL;
	unsigned int i;

	for (i = 0; i < 2; i++) {
		ref_elem = tplg_elem_lookup(&tplg->pcm_caps_list,
			caps[i].name, SND_TPLG_TYPE_STREAM_CAPS, index);

		if (ref_elem != NULL)
			copy_stream_caps(id, &caps[i], ref_elem);
	}

	return 0;
}

/* build a PCM (FE DAI & DAI link) element */
static int build_pcm(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err;

	err = tplg_build_stream_caps(tplg, elem->id, elem->index,
						elem->pcm->caps);
	if (err < 0)
		return err;

	/* merge private data from the referenced data elements */
	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}
		if (!ref->elem) {
			SNDERR("cannot find '%s' referenced by"
				" PCM '%s'", ref->id, elem->id);
			return -EINVAL;
		}
	}

	return 0;
}

/* build all PCM (FE DAI & DAI link) elements */
int tplg_build_pcms(snd_tplg_t *tplg, unsigned int type)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	base = &tplg->pcm_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (elem->type != type) {
			SNDERR("invalid elem '%s'", elem->id);
			return -EINVAL;
		}

		err = build_pcm(tplg, elem);
		if (err < 0)
			return err;

		/* add PCM to manifest */
		tplg->manifest.pcm_elems++;
	}

	return 0;
}

/* build a physical DAI */
static int tplg_build_dai(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err = 0;

	/* get playback & capture stream caps */
	err = tplg_build_stream_caps(tplg, elem->id, elem->index,
						elem->dai->caps);
	if (err < 0)
		return err;

	/* get private data */
	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);

		if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}
	}

	/* add DAI to manifest */
	tplg->manifest.dai_elems++;

	return 0;
}

/* build physical DAIs*/
int tplg_build_dais(snd_tplg_t *tplg, unsigned int type)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	base = &tplg->dai_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (elem->type != type) {
			SNDERR("invalid elem '%s'", elem->id);
			return -EINVAL;
		}

		err = tplg_build_dai(tplg, elem);
		if (err < 0)
			return err;
	}

	return 0;
}

static int tplg_build_stream_cfg(snd_tplg_t *tplg,
				 struct snd_soc_tplg_stream *stream,
				 int num_streams, int index)
{
	struct snd_soc_tplg_stream *strm;
	struct tplg_elem *ref_elem;
	int i;

	for (i = 0; i < num_streams; i++) {
		strm = stream + i;
		ref_elem = tplg_elem_lookup(&tplg->pcm_config_list,
			strm->name, SND_TPLG_TYPE_STREAM_CONFIG, index);

		if (ref_elem && ref_elem->stream_cfg)
			*strm = *ref_elem->stream_cfg;
	}

	return 0;
}

static int build_link(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct snd_soc_tplg_link_config *link = elem->link;
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int num_hw_configs = 0, err = 0;

	err = tplg_build_stream_cfg(tplg, link->stream,
				    link->num_streams, elem->index);
	if (err < 0)
		return err;

	/* hw configs & private data */
	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);

		switch (ref->type) {
		case SND_TPLG_TYPE_HW_CONFIG:
			ref->elem = tplg_elem_lookup(&tplg->hw_cfg_list,
				ref->id, SND_TPLG_TYPE_HW_CONFIG, elem->index);
			if (!ref->elem) {
				SNDERR("cannot find HW config '%s'"
				       " referenced by link '%s'",
				       ref->id, elem->id);
				return -EINVAL;
			}

			memcpy(&link->hw_config[num_hw_configs],
				ref->elem->hw_cfg,
				sizeof(struct snd_soc_tplg_hw_config));
			num_hw_configs++;
			break;

		case SND_TPLG_TYPE_DATA: /* merge private data */
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
			link = elem->link; /* realloc */
			break;

		default:
			break;
		}
	}

	/* add link to manifest */
	tplg->manifest.dai_link_elems++;

	return 0;
}

/* build physical DAI link configurations */
int tplg_build_links(snd_tplg_t *tplg, unsigned int type)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	switch (type) {
	case SND_TPLG_TYPE_LINK:
	case SND_TPLG_TYPE_BE:
		base = &tplg->be_list;
		break;
	case SND_TPLG_TYPE_CC:
		base = &tplg->cc_list;
		break;
	default:
		return -EINVAL;
	}

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		err =  build_link(tplg, elem);
		if (err < 0)
			return err;
	}

	return 0;
}

static int split_format(struct snd_soc_tplg_stream_caps *caps, char *str)
{
	char *s = NULL;
	snd_pcm_format_t format;
	int i = 0;

	s = strtok(str, ",");
	while ((s != NULL) && (i < SND_SOC_TPLG_MAX_FORMATS)) {
		format = snd_pcm_format_value(s);
		if (format == SND_PCM_FORMAT_UNKNOWN) {
			SNDERR("unsupported stream format %s", s);
			return -EINVAL;
		}

		caps->formats |= 1ull << format;
		s = strtok(NULL, ", ");
		i++;
	}

	return 0;
}

static int get_rate_value(const char* name)
{
	int rate;
	for (rate = 0; rate <= SND_PCM_RATE_LAST; rate++) {
		if (snd_pcm_rate_names[rate] &&
		    strcasecmp(name, snd_pcm_rate_names[rate]) == 0) {
			return rate;
		}
	}

	return SND_PCM_RATE_UNKNOWN;
}

static const char *get_rate_name(int rate)
{
	if (rate >= 0 && rate <= SND_PCM_RATE_LAST)
		return snd_pcm_rate_names[rate];
	return NULL;
}

static int split_rate(struct snd_soc_tplg_stream_caps *caps, char *str)
{
	char *s = NULL;
	snd_pcm_rates_t rate;
	int i = 0;

	s = strtok(str, ",");
	while (s) {
		rate = get_rate_value(s);

		if (rate == SND_PCM_RATE_UNKNOWN) {
			SNDERR("unsupported stream rate %s", s);
			return -EINVAL;
		}

		caps->rates |= 1 << rate;
		s = strtok(NULL, ", ");
		i++;
	}

	return 0;
}

static int parse_unsigned(snd_config_t *n, void *dst)
{
	int ival;

	if (tplg_get_integer(n, &ival, 0) < 0)
		return -EINVAL;

	unaligned_put32(dst, ival);
#if TPLG_DEBUG
	{
		const char *id;
		if (snd_config_get_id(n, &id) >= 0)
			tplg_dbg("\t\t%s: %d", id, ival);
	}
#endif
	return 0;
}

/* Parse pcm stream capabilities */
int tplg_parse_stream_caps(snd_tplg_t *tplg,
			   snd_config_t *cfg,
			   void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_stream_caps *sc;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val;
	char *s;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_STREAM_CAPS);
	if (!elem)
		return -ENOMEM;

	sc = elem->stream_caps;
	sc->size = elem->size;
	snd_strlcpy(sc->name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" PCM Capabilities: %s", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "formats") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			s = strdup(val);
			if (s == NULL)
				return -ENOMEM;

			err = split_format(sc, s);
			free(s);

			if (err < 0)
				return err;

			tplg_dbg("\t\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "rates") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			s = strdup(val);
			if (!s)
				return -ENOMEM;

			err = split_rate(sc, s);
			free(s);

			if (err < 0)
				return err;

			tplg_dbg("\t\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "rate_min") == 0) {
			if (parse_unsigned(n, &sc->rate_min))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "rate_max") == 0) {
			if (parse_unsigned(n, &sc->rate_max))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "channels_min") == 0) {
			if (parse_unsigned(n, &sc->channels_min))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "channels_max") == 0) {
			if (parse_unsigned(n, &sc->channels_max))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "periods_min") == 0) {
			if (parse_unsigned(n, &sc->periods_min))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "periods_max") == 0) {
			if (parse_unsigned(n, &sc->periods_max))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "period_size_min") == 0) {
			if (parse_unsigned(n, &sc->period_size_min))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "period_size_max") == 0) {
			if (parse_unsigned(n, &sc->period_size_max))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "buffer_size_min") == 0) {
			if (parse_unsigned(n, &sc->buffer_size_min))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "buffer_size_max") == 0) {
			if (parse_unsigned(n, &sc->buffer_size_max))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "sig_bits") == 0) {
			if (parse_unsigned(n, &sc->sig_bits))
				return -EINVAL;
			continue;
		}

	}

	return 0;
}

/* save stream caps */
int tplg_save_stream_caps(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			  struct tplg_elem *elem,
			  struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_stream_caps *sc = elem->stream_caps;
	const char *s;
	unsigned int i;
	int err, first;

	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err >= 0 && sc->formats) {
		err = tplg_save_printf(dst, pfx, "\tformats '");
		first = 1;
		for (i = 0; err >= 0 && i <= SND_PCM_FORMAT_LAST; i++) {
			if (sc->formats & (1ULL << i)) {
				s = snd_pcm_format_name(i);
				err = tplg_save_printf(dst, NULL, "%s%s",
						       !first ? ", " : "", s);
				first = 0;
			}
		}
		if (err >= 0)
			err = tplg_save_printf(dst, NULL, "'\n");
	}
	if (err >= 0 && sc->rates) {
		err = tplg_save_printf(dst, pfx, "\trates '");
		first = 1;
		for (i = 0; err >= 0 && i <= SND_PCM_RATE_LAST; i++) {
			if (sc->rates & (1ULL << i)) {
				s = get_rate_name(i);
				err = tplg_save_printf(dst, NULL, "%s%s",
						       !first ? ", " : "", s);
				first = 0;
			}
		}
		if (err >= 0)
			err = tplg_save_printf(dst, NULL, "'\n");
	}
	if (err >= 0 && sc->rate_min)
		err = tplg_save_printf(dst, pfx, "\trate_min %u\n",
				       sc->rate_min);
	if (err >= 0 && sc->rate_max)
		err = tplg_save_printf(dst, pfx, "\trate_max %u\n",
				       sc->rate_max);
	if (err >= 0 && sc->channels_min)
		err = tplg_save_printf(dst, pfx, "\tchannels_min %u\n",
				       sc->channels_min);
	if (err >= 0 && sc->channels_max)
		err = tplg_save_printf(dst, pfx, "\tchannels_max %u\n",
				       sc->channels_max);
	if (err >= 0 && sc->periods_min)
		err = tplg_save_printf(dst, pfx, "\tperiods_min %u\n",
				       sc->periods_min);
	if (err >= 0 && sc->periods_max)
		err = tplg_save_printf(dst, pfx, "\tperiods_max %u\n",
				       sc->periods_max);
	if (err >= 0 && sc->period_size_min)
		err = tplg_save_printf(dst, pfx, "\tperiod_size_min %u\n",
				       sc->period_size_min);
	if (err >= 0 && sc->period_size_max)
		err = tplg_save_printf(dst, pfx, "\tperiod_size_max %u\n",
				       sc->period_size_max);
	if (err >= 0 && sc->buffer_size_min)
		err = tplg_save_printf(dst, pfx, "\tbuffer_size_min %u\n",
				       sc->buffer_size_min);
	if (err >= 0 && sc->buffer_size_max)
		err = tplg_save_printf(dst, pfx, "\tbuffer_size_max %u\n",
				       sc->buffer_size_max);
	if (err >= 0 && sc->sig_bits)
		err = tplg_save_printf(dst, pfx, "\tsig_bits %u\n",
				       sc->sig_bits);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* Parse the caps and config of a pcm stream */
static int tplg_parse_streams(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			      snd_config_t *cfg, void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct tplg_elem *elem = private;
	struct snd_soc_tplg_pcm *pcm;
	struct snd_soc_tplg_dai *dai;
	void *playback, *capture;
	struct snd_soc_tplg_stream_caps *caps;
	const char *id, *value;
	int stream;

	snd_config_get_id(cfg, &id);

	tplg_dbg("\t%s:", id);

	switch (elem->type) {
	case SND_TPLG_TYPE_PCM:
		pcm = elem->pcm;
		playback = &pcm->playback;
		capture = &pcm->capture;
		caps = pcm->caps;
		break;

	case SND_TPLG_TYPE_DAI:
		dai = elem->dai;
		playback = &dai->playback;
		capture = &dai->capture;
		caps = dai->caps;
		break;

	default:
		return -EINVAL;
	}

	if (strcmp(id, "playback") == 0) {
		stream = SND_SOC_TPLG_STREAM_PLAYBACK;
		unaligned_put32(playback, 1);
	} else if (strcmp(id, "capture") == 0) {
		stream = SND_SOC_TPLG_STREAM_CAPTURE;
		unaligned_put32(capture, 1);
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
			/* store stream caps name, to find and merge
			 * the caps in building phase.
			 */
			snd_strlcpy(caps[stream].name, value,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

			tplg_dbg("\t\t%s\n\t\t\t%s", id, value);
			continue;
		}
	}

	return 0;
}

/* Save the caps and config of a pcm stream */
int tplg_save_streams(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		      struct tplg_elem *elem,
		      struct tplg_buf *dst, const char *pfx)
{
	static const char *stream_ids[2] = {
		"playback",
		"capture"
	};
	static unsigned int stream_types[2] = {
		SND_SOC_TPLG_STREAM_PLAYBACK,
		SND_SOC_TPLG_STREAM_CAPTURE
	};
	struct snd_soc_tplg_stream_caps *caps;
	unsigned int streams[2], stream;
	const char *s;
	int err;

	switch (elem->type) {
	case SND_TPLG_TYPE_PCM:
		streams[0] = elem->pcm->playback;
		streams[1] = elem->pcm->capture;
		caps = elem->pcm->caps;
		break;
	case SND_TPLG_TYPE_DAI:
		streams[0] = elem->dai->playback;
		streams[1] = elem->dai->capture;
		caps = elem->dai->caps;
		break;
	default:
		return -EINVAL;
	}

	for (stream = 0; stream < 2; stream++) {
		if (streams[stream] == 0)
			continue;
		if (!caps)
			continue;
		s = caps[stream_types[stream]].name;
		if (s[0] == '\0')
			continue;
		err = tplg_save_printf(dst, pfx, "pcm.%s {\n", stream_ids[stream]);
		if (err < 0)
			return err;
		err = tplg_save_printf(dst, pfx, "\tcapabilities '%s'\n", s);
		if (err < 0)
			return err;
		err = tplg_save_printf(dst, pfx, "}\n");
		if (err < 0)
			return err;
	}

	return 0;
}

/* Parse name and id of a front-end DAI (ie. cpu dai of a FE DAI link) */
static int tplg_parse_fe_dai(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			     snd_config_t *cfg, void *private)
{
	struct tplg_elem *elem = private;
	struct snd_soc_tplg_pcm *pcm = elem->pcm;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	unsigned int dai_id;

	snd_config_get_id(cfg, &id);
	tplg_dbg("\t\tFE DAI %s:", id);
	snd_strlcpy(pcm->dai_name, id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "id") == 0) {
			if (tplg_get_unsigned(n, &dai_id, 0)) {
				SNDERR("invalid fe dai ID");
				return -EINVAL;
			}

			unaligned_put32(&pcm->dai_id, dai_id);
			tplg_dbg("\t\t\tindex: %d", dai_id);
		}
	}

	return 0;
}

/* Save the caps and config of a pcm stream */
int tplg_save_fe_dai(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		     struct tplg_elem *elem,
		     struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_pcm *pcm = elem->pcm;
	int err = 0;

	if (strlen(pcm->dai_name))
		err = tplg_save_printf(dst, pfx, "dai.'%s'.id %u\n", pcm->dai_name, pcm->dai_id);
	else if (pcm->dai_id > 0)
		err = tplg_save_printf(dst, pfx, "dai.0.id %u\n", pcm->dai_id);
	return err;
}

/* parse a flag bit of the given mask */
static int parse_flag(snd_config_t *n, unsigned int mask_in,
		      void *mask, void *flags)
{
	int ret;

	ret = snd_config_get_bool(n);
	if (ret < 0)
		return ret;

	unaligned_put32(mask, unaligned_get32(mask) | mask_in);
	if (ret)
		unaligned_put32(flags, unaligned_get32(flags) | mask_in);
	else
		unaligned_put32(flags, unaligned_get32(flags) & (~mask_in));

	return 0;
}

static int save_flags(unsigned int flags, unsigned int mask,
		      struct tplg_buf *dst, const char *pfx)
{
	static unsigned int flag_masks[4] = {
		SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_RATES,
		SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_CHANNELS,
		SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_SAMPLEBITS,
		SND_SOC_TPLG_LNK_FLGBIT_VOICE_WAKEUP,
	};
	static const char *flag_ids[4] = {
		"symmetric_rates",
		"symmetric_channels",
		"symmetric_sample_bits",
		"ignore_suspend",
	};
	unsigned int i;
	int err = 0;

	for (i = 0; err >= 0 && i < ARRAY_SIZE(flag_masks); i++) {
		if (mask & flag_masks[i]) {
			unsigned int v = (flags & flag_masks[i]) ? 1 : 0;
			err = tplg_save_printf(dst, pfx, "%s %u\n",
					       flag_ids[i], v);
		}
	}
	return err;
}

/* Parse PCM (for front end DAI & DAI link) in text conf file */
int tplg_parse_pcm(snd_tplg_t *tplg, snd_config_t *cfg,
		   void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_pcm *pcm;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err, ival;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_PCM);
	if (!elem)
		return -ENOMEM;

	pcm = elem->pcm;
	pcm->size = elem->size;
	snd_strlcpy(pcm->pcm_name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" PCM: %s", elem->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "id") == 0) {
			if (parse_unsigned(n, &pcm->pcm_id))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "pcm") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_streams, elem);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "compress") == 0) {
			ival = snd_config_get_bool(n);
			if (ival < 0)
				return -EINVAL;

			pcm->compress = ival;

			tplg_dbg("\t%s: %d", id, ival);
			continue;
		}

		if (strcmp(id, "dai") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_fe_dai, elem);
			if (err < 0)
				return err;
			continue;
		}

		/* flags */
		if (strcmp(id, "symmetric_rates") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_RATES,
				&pcm->flag_mask, &pcm->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "symmetric_channels") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_CHANNELS,
				&pcm->flag_mask, &pcm->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "symmetric_sample_bits") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_SAMPLEBITS,
				&pcm->flag_mask, &pcm->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "ignore_suspend") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_VOICE_WAKEUP,
				&pcm->flag_mask, &pcm->flags);
			if (err < 0)
				return err;
			continue;
		}

		/* private data */
		if (strcmp(id, "data") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_DATA);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/* save PCM */
int tplg_save_pcm(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		  struct tplg_elem *elem,
		  struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_pcm *pcm = elem->pcm;
	char pfx2[16];
	int err;

	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err >= 0 && elem->index)
		err = tplg_save_printf(dst, pfx, "\tindex %u\n",
				       elem->index);
	if (err >= 0 && pcm->pcm_id)
		err = tplg_save_printf(dst, pfx, "\tid %u\n",
				       pcm->pcm_id);
	if (err >= 0 && pcm->compress)
		err = tplg_save_printf(dst, pfx, "\tcompress 1\n");
	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	if (err >= 0)
		err = tplg_save_fe_dai(tplg, elem, dst, pfx2);
	if (err >= 0)
		err = tplg_save_streams(tplg, elem, dst, pfx2);
	if (err >= 0)
		err = save_flags(pcm->flags, pcm->flag_mask, dst, pfx);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_DATA,
				     "data", dst, pfx2);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* Parse physical DAI */
int tplg_parse_dai(snd_tplg_t *tplg, snd_config_t *cfg,
		   void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_dai *dai;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_DAI);
	if (!elem)
		return -ENOMEM;

	dai = elem->dai;
	dai->size = elem->size;
	snd_strlcpy(dai->dai_name, elem->id,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" DAI: %s", elem->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "id") == 0) {
			if (parse_unsigned(n, &dai->dai_id))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "playback") == 0) {
			if (parse_unsigned(n, &dai->playback))
				return -EINVAL;
			continue;
		}


		if (strcmp(id, "capture") == 0) {
			if (parse_unsigned(n, &dai->capture))
				return -EINVAL;
			continue;
		}


		/* stream capabilities */
		if (strcmp(id, "pcm") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_streams, elem);
			if (err < 0)
				return err;
			continue;
		}

		/* flags */
		if (strcmp(id, "symmetric_rates") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_DAI_FLGBIT_SYMMETRIC_RATES,
				&dai->flag_mask, &dai->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "symmetric_channels") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_DAI_FLGBIT_SYMMETRIC_CHANNELS,
				&dai->flag_mask, &dai->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "symmetric_sample_bits") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_DAI_FLGBIT_SYMMETRIC_SAMPLEBITS,
				&dai->flag_mask, &dai->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "ignore_suspend") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_VOICE_WAKEUP,
				&dai->flag_mask, &dai->flags);
			if (err < 0)
				return err;
			continue;
		}

		/* private data */
		if (strcmp(id, "data") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_DATA);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/* save DAI */
int tplg_save_dai(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		  struct tplg_elem *elem,
		  struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_dai *dai = elem->dai;
	char pfx2[16];
	int err;

	if (!dai)
		return 0;
	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err >= 0 && elem->index)
		err = tplg_save_printf(dst, pfx, "\tindex %u\n",
				       elem->index);
	if (err >= 0 && dai->dai_id)
		err = tplg_save_printf(dst, pfx, "\tid %u\n",
				       dai->dai_id);
	if (err >= 0 && dai->playback)
		err = tplg_save_printf(dst, pfx, "\tplayback %u\n",
				       dai->playback);
	if (err >= 0 && dai->capture)
		err = tplg_save_printf(dst, pfx, "\tcapture %u\n",
				       dai->capture);
	if (err >= 0)
		err = tplg_save_streams(tplg, elem, dst, pfx2);
	if (err >= 0)
		err = save_flags(dai->flags, dai->flag_mask, dst, pfx);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_DATA,
				     "data", dst, pfx2);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* parse physical link runtime supported HW configs in text conf file */
static int parse_hw_config_refs(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
				snd_config_t *cfg,
				struct tplg_elem *elem)
{
	struct snd_soc_tplg_link_config *link = elem->link;
	int err;

	err = tplg_parse_refs(cfg, elem, SND_TPLG_TYPE_HW_CONFIG);
	if (err < 0)
		return err;
	link->num_hw_configs = err;
	return 0;
}

/* Parse a physical link element in text conf file */
int tplg_parse_link(snd_tplg_t *tplg, snd_config_t *cfg,
		    void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_link_config *link;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_BE);
	if (!elem)
		return -ENOMEM;

	link = elem->link;
	link->size = elem->size;
	snd_strlcpy(link->name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" Link: %s", elem->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "id") == 0) {
			if (parse_unsigned(n, &link->id))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "stream_name") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			snd_strlcpy(link->stream_name, val,
				       SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
			tplg_dbg("\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "hw_configs") == 0) {
			err = parse_hw_config_refs(tplg, n, elem);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "default_hw_conf_id") == 0) {
			if (parse_unsigned(n, &link->default_hw_config_id))
				return -EINVAL;
			continue;
		}

		/* flags */
		if (strcmp(id, "symmetric_rates") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_RATES,
				&link->flag_mask, &link->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "symmetric_channels") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_CHANNELS,
				&link->flag_mask, &link->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "symmetric_sample_bits") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_SYMMETRIC_SAMPLEBITS,
				&link->flag_mask, &link->flags);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "ignore_suspend") == 0) {
			err = parse_flag(n,
				SND_SOC_TPLG_LNK_FLGBIT_VOICE_WAKEUP,
				&link->flag_mask, &link->flags);
			if (err < 0)
				return err;
			continue;
		}

		/* private data */
		if (strcmp(id, "data") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_DATA);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/* save physical link */
int tplg_save_link(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		   struct tplg_elem *elem,
		   struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_link_config *link = elem->link;
	char pfx2[16];
	int err;

	if (!link)
		return 0;
	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err >= 0 && elem->index)
		err = tplg_save_printf(dst, pfx, "\tindex %u\n",
				       elem->index);
	if (err >= 0 && link->id)
		err = tplg_save_printf(dst, pfx, "\tid %u\n",
				       link->id);
	if (err >= 0 && link->stream_name[0])
		err = tplg_save_printf(dst, pfx, "\tstream_name '%s'\n",
				       link->stream_name);
	if (err >= 0 && link->default_hw_config_id)
		err = tplg_save_printf(dst, pfx, "\tdefault_hw_conf_id %u\n",
				       link->default_hw_config_id);
	if (err >= 0)
		err = save_flags(link->flags, link->flag_mask, dst, pfx);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_HW_CONFIG,
				     "hw_configs", dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_DATA,
				     "data", dst, pfx2);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* Parse cc */
int tplg_parse_cc(snd_tplg_t *tplg, snd_config_t *cfg,
		  void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_link_config *link;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_CC);
	if (!elem)
		return -ENOMEM;

	link = elem->link;
	link->size = elem->size;

	tplg_dbg(" CC: %s", elem->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "id") == 0) {
			if (parse_unsigned(n, &link->id))
				return -EINVAL;
			continue;
		}

	}

	return 0;
}

/* save CC */
int tplg_save_cc(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		 struct tplg_elem *elem,
		 struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_link_config *link = elem->link;
	char pfx2[16];
	int err;

	if (!link)
		return 0;
	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err >= 0 && elem->index)
		err = tplg_save_printf(dst, pfx, "\tindex %u\n",
				       elem->index);
	if (err >= 0 && link->id)
		err = tplg_save_printf(dst, pfx, "\tid %u\n",
				       link->id);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

struct audio_hw_format {
	unsigned int type;
	const char *name;
};

static struct audio_hw_format audio_hw_formats[] = {
	{
		.type = SND_SOC_DAI_FORMAT_I2S,
		.name = "I2S",
	},
	{
		.type = SND_SOC_DAI_FORMAT_RIGHT_J,
		.name = "RIGHT_J",
	},
	{
		.type = SND_SOC_DAI_FORMAT_LEFT_J,
		.name = "LEFT_J",
	},
	{
		.type = SND_SOC_DAI_FORMAT_DSP_A,
		.name = "DSP_A",
	},
	{
		.type = SND_SOC_DAI_FORMAT_DSP_B,
		.name = "DSP_B",
	},
	{
		.type = SND_SOC_DAI_FORMAT_AC97,
		.name = "AC97",
	},
	{
		.type = SND_SOC_DAI_FORMAT_PDM,
		.name = "PDM",
	},
};

static int get_audio_hw_format(const char *val)
{
	unsigned int i;

	if (val[0] == '\0')
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(audio_hw_formats); i++)
		if (strcasecmp(audio_hw_formats[i].name, val) == 0)
			return audio_hw_formats[i].type;

	SNDERR("invalid audio HW format %s", val);
	return -EINVAL;
}

static const char *get_audio_hw_format_name(unsigned int type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(audio_hw_formats); i++)
		if (audio_hw_formats[i].type == type)
			return audio_hw_formats[i].name;
	return NULL;
}

int tplg_parse_hw_config(snd_tplg_t *tplg, snd_config_t *cfg,
			 void *private ATTRIBUTE_UNUSED)
{

	struct snd_soc_tplg_hw_config *hw_cfg;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int ret, ival;
	bool provider_legacy;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_HW_CONFIG);
	if (!elem)
		return -ENOMEM;

	hw_cfg = elem->hw_cfg;
	hw_cfg->size = elem->size;

	tplg_dbg(" Link HW config: %s", elem->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "id") == 0) {
			if (parse_unsigned(n, &hw_cfg->id))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "format") == 0 ||
		    strcmp(id, "fmt") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			ret = get_audio_hw_format(val);
			if (ret < 0)
				return ret;
			hw_cfg->fmt = ret;
			continue;
		}

		provider_legacy = false;
		if (strcmp(id, "bclk_master") == 0) {
			SNDERR("deprecated option %s, please use 'bclk'\n", id);
			provider_legacy = true;
		}

		if (provider_legacy ||
		    strcmp(id, "bclk") == 0) {

			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			if (!strcmp(val, "master")) {
				/* For backwards capability,
				 * "master" == "codec is slave"
				 */
				SNDERR("deprecated bclk value '%s'", val);

				hw_cfg->bclk_provider = SND_SOC_TPLG_BCLK_CC;
			} else if (!strcmp(val, "codec_slave")) {
				SNDERR("deprecated bclk value '%s', use 'codec_consumer'", val);

				hw_cfg->bclk_provider = SND_SOC_TPLG_BCLK_CC;
			} else if (!strcmp(val, "codec_consumer")) {
				hw_cfg->bclk_provider = SND_SOC_TPLG_BCLK_CC;
			} else if (!strcmp(val, "codec_master")) {
				SNDERR("deprecated bclk value '%s', use 'codec_provider", val);

				hw_cfg->bclk_provider = SND_SOC_TPLG_BCLK_CP;
			} else if (!strcmp(val, "codec_provider")) {
				hw_cfg->bclk_provider = SND_SOC_TPLG_BCLK_CP;
			}
			continue;
		}

		if (strcmp(id, "bclk_freq") == 0 ||
		    strcmp(id, "bclk_rate") == 0) {
			if (parse_unsigned(n, &hw_cfg->bclk_rate))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "bclk_invert") == 0 ||
		    strcmp(id, "invert_bclk") == 0) {
			ival = snd_config_get_bool(n);
			if (ival < 0)
				return -EINVAL;

			hw_cfg->invert_bclk = ival;
			continue;
		}

		provider_legacy = false;
		if (strcmp(id, "fsync_master") == 0) {
			SNDERR("deprecated option %s, please use 'fsync'\n", id);
			provider_legacy = true;
		}

		if (provider_legacy ||
		    strcmp(id, "fsync") == 0) {

			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			if (!strcmp(val, "master")) {
				/* For backwards capability,
				 * "master" == "codec is slave"
				 */
				SNDERR("deprecated fsync value '%s'", val);

				hw_cfg->fsync_provider = SND_SOC_TPLG_FSYNC_CC;
			} else if (!strcmp(val, "codec_slave")) {
				SNDERR("deprecated fsync value '%s', use 'codec_consumer'", val);

				hw_cfg->fsync_provider = SND_SOC_TPLG_FSYNC_CC;
			} else if (!strcmp(val, "codec_consumer")) {
				hw_cfg->fsync_provider = SND_SOC_TPLG_FSYNC_CC;
			} else if (!strcmp(val, "codec_master")) {
				SNDERR("deprecated fsync value '%s', use 'codec_provider'", val);

				hw_cfg->fsync_provider = SND_SOC_TPLG_FSYNC_CP;
			} else if (!strcmp(val, "codec_provider")) {
				hw_cfg->fsync_provider = SND_SOC_TPLG_FSYNC_CP;
			}
			continue;
		}

		if (strcmp(id, "fsync_invert") == 0 ||
		    strcmp(id, "invert_fsync") == 0) {
			ival = snd_config_get_bool(n);
			if (ival < 0)
				return -EINVAL;

			hw_cfg->invert_fsync = ival;
			continue;
		}

		if (strcmp(id, "fsync_freq") == 0 ||
		    strcmp(id, "fsync_rate") == 0) {
			if (parse_unsigned(n, &hw_cfg->fsync_rate))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "mclk_freq") == 0 ||
		    strcmp(id, "mclk_rate") == 0) {
			if (parse_unsigned(n, &hw_cfg->mclk_rate))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "mclk") == 0 ||
		    strcmp(id, "mclk_direction") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			if (!strcmp(val, "master")) {
				/* For backwards capability,
				 * "master" == "for codec, mclk is input"
				 */
				SNDERR("deprecated mclk value '%s'", val);

				hw_cfg->mclk_direction = SND_SOC_TPLG_MCLK_CI;
			} else if (!strcmp(val, "codec_mclk_in")) {
				hw_cfg->mclk_direction = SND_SOC_TPLG_MCLK_CI;
			} else if (!strcmp(val, "codec_mclk_out")) {
				hw_cfg->mclk_direction = SND_SOC_TPLG_MCLK_CO;
			}
			continue;
		}

		if (strcmp(id, "pm_gate_clocks") == 0 ||
		    strcmp(id, "clock_gated") == 0) {
			ival = snd_config_get_bool(n);
			if (ival < 0)
				return -EINVAL;

			if (ival)
				hw_cfg->clock_gated =
					SND_SOC_TPLG_DAI_CLK_GATE_GATED;
			else
				hw_cfg->clock_gated =
					SND_SOC_TPLG_DAI_CLK_GATE_CONT;
			continue;
		}

		if (strcmp(id, "tdm_slots") == 0) {
			if (parse_unsigned(n, &hw_cfg->tdm_slots))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "tdm_slot_width") == 0) {
			if (parse_unsigned(n, &hw_cfg->tdm_slot_width))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "tx_slots") == 0) {
			if (parse_unsigned(n, &hw_cfg->tx_slots))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "rx_slots") == 0) {
			if (parse_unsigned(n, &hw_cfg->rx_slots))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "tx_channels") == 0) {
			if (parse_unsigned(n, &hw_cfg->tx_channels))
				return -EINVAL;
			continue;
		}

		if (strcmp(id, "rx_channels") == 0) {
			if (parse_unsigned(n, &hw_cfg->rx_channels))
				return -EINVAL;
			continue;
		}

	}

	return 0;
}

/* save hw config */
int tplg_save_hw_config(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			struct tplg_elem *elem,
			struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_hw_config *hc = elem->hw_cfg;
	int err;

	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err >= 0 && hc->id)
		err = tplg_save_printf(dst, pfx, "\tid %u\n",
				       hc->id);
	if (err >= 0 && hc->fmt)
		err = tplg_save_printf(dst, pfx, "\tformat '%s'\n",
				       get_audio_hw_format_name(hc->fmt));
	if (err >= 0 && hc->bclk_provider)
		err = tplg_save_printf(dst, pfx, "\tbclk '%s'\n",
				       hc->bclk_provider == SND_SOC_TPLG_BCLK_CC ?
						"codec_consumer" : "codec_provider");
	if (err >= 0 && hc->bclk_rate)
		err = tplg_save_printf(dst, pfx, "\tbclk_freq %u\n",
				       hc->bclk_rate);
	if (err >= 0 && hc->invert_bclk)
		err = tplg_save_printf(dst, pfx, "\tbclk_invert 1\n");
	if (err >= 0 && hc->fsync_provider)
		err = tplg_save_printf(dst, pfx, "\tfsync_provider '%s'\n",
				       hc->fsync_provider == SND_SOC_TPLG_FSYNC_CC ?
						"codec_consumer" : "codec_provider");
	if (err >= 0 && hc->fsync_rate)
		err = tplg_save_printf(dst, pfx, "\tfsync_freq %u\n",
				       hc->fsync_rate);
	if (err >= 0 && hc->invert_fsync)
		err = tplg_save_printf(dst, pfx, "\tfsync_invert 1\n");
	if (err >= 0 && hc->mclk_rate)
		err = tplg_save_printf(dst, pfx, "\tmclk_freq %u\n",
				       hc->mclk_rate);
	if (err >= 0 && hc->mclk_direction)
		err = tplg_save_printf(dst, pfx, "\tmclk '%s'\n",
				       hc->mclk_direction == SND_SOC_TPLG_MCLK_CI ?
						"codec_mclk_in" : "codec_mclk_out");
	if (err >= 0 && hc->clock_gated)
		err = tplg_save_printf(dst, pfx, "\tpm_gate_clocks 1\n");
	if (err >= 0 && hc->tdm_slots)
		err = tplg_save_printf(dst, pfx, "\ttdm_slots %u\n",
				       hc->tdm_slots);
	if (err >= 0 && hc->tdm_slot_width)
		err = tplg_save_printf(dst, pfx, "\ttdm_slot_width %u\n",
				       hc->tdm_slot_width);
	if (err >= 0 && hc->tx_slots)
		err = tplg_save_printf(dst, pfx, "\ttx_slots %u\n",
				       hc->tx_slots);
	if (err >= 0 && hc->rx_slots)
		err = tplg_save_printf(dst, pfx, "\trx_slots %u\n",
				       hc->rx_slots);
	if (err >= 0 && hc->tx_channels)
		err = tplg_save_printf(dst, pfx, "\ttx_channels %u\n",
				       hc->tx_channels);
	if (err >= 0 && hc->rx_channels)
		err = tplg_save_printf(dst, pfx, "\trx_channels %u\n",
				       hc->rx_channels);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* copy stream object */
static void tplg_add_stream_object(struct snd_soc_tplg_stream *strm,
				   struct snd_tplg_stream_template *strm_tpl)
{
	snd_strlcpy(strm->name, strm_tpl->name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	strm->format = strm_tpl->format;
	strm->rate = strm_tpl->rate;
	strm->period_bytes = strm_tpl->period_bytes;
	strm->buffer_bytes = strm_tpl->buffer_bytes;
	strm->channels = strm_tpl->channels;
}

static int tplg_add_stream_caps(snd_tplg_t *tplg,
				struct snd_tplg_stream_caps_template *caps_tpl)
{
	struct snd_soc_tplg_stream_caps *caps;
	struct tplg_elem *elem;

	elem = tplg_elem_new_common(tplg, NULL, caps_tpl->name,
				    SND_TPLG_TYPE_STREAM_CAPS);
	if (!elem)
		return -ENOMEM;

	caps = elem->stream_caps;

	snd_strlcpy(caps->name, caps_tpl->name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	caps->formats = caps_tpl->formats;
	caps->rates = caps_tpl->rates;
	caps->rate_min = caps_tpl->rate_min;
	caps->rate_max = caps_tpl->rate_max;
	caps->channels_min = caps_tpl->channels_min;
	caps->channels_max = caps_tpl->channels_max;
	caps->periods_min = caps_tpl->periods_min;
	caps->periods_max = caps_tpl->periods_max;
	caps->period_size_min = caps_tpl->period_size_min;
	caps->period_size_max = caps_tpl->period_size_max;
	caps->buffer_size_min = caps_tpl->buffer_size_min;
	caps->buffer_size_max = caps_tpl->buffer_size_max;
	caps->sig_bits = caps_tpl->sig_bits;
	return 0;
}

/* Add a PCM element (FE DAI & DAI link) from C API */
int tplg_add_pcm_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_pcm_template *pcm_tpl = t->pcm;
	struct snd_soc_tplg_private *priv;
	struct snd_soc_tplg_pcm *pcm;
	struct tplg_elem *elem;
	int ret, i;

	tplg_dbg("PCM: %s, DAI %s", pcm_tpl->pcm_name, pcm_tpl->dai_name);

	if (pcm_tpl->num_streams > SND_SOC_TPLG_STREAM_CONFIG_MAX)
		return -EINVAL;

	elem = tplg_elem_new_common(tplg, NULL, pcm_tpl->pcm_name,
		SND_TPLG_TYPE_PCM);
	if (!elem)
		return -ENOMEM;

	pcm = elem->pcm;
	pcm->size = elem->size;

	snd_strlcpy(pcm->pcm_name, pcm_tpl->pcm_name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	snd_strlcpy(pcm->dai_name, pcm_tpl->dai_name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	pcm->pcm_id = pcm_tpl->pcm_id;
	pcm->dai_id = pcm_tpl->dai_id;
	pcm->playback = pcm_tpl->playback;
	pcm->capture = pcm_tpl->capture;
	pcm->compress = pcm_tpl->compress;

	for (i = 0; i < 2; i++) {
		if (!pcm_tpl->caps[i] || !pcm_tpl->caps[i]->name)
			continue;
		ret = tplg_add_stream_caps(tplg, pcm_tpl->caps[i]);
		if (ret < 0)
			return ret;
		snd_strlcpy(pcm->caps[i].name, pcm_tpl->caps[i]->name,
			    sizeof(pcm->caps[i].name));
	}

	pcm->flag_mask = pcm_tpl->flag_mask;
	pcm->flags = pcm_tpl->flags;

	pcm->num_streams = pcm_tpl->num_streams;
	for (i = 0; i < pcm_tpl->num_streams; i++)
		tplg_add_stream_object(&pcm->stream[i], &pcm_tpl->stream[i]);

	/* private data */
	priv = pcm_tpl->priv;
	if (priv && priv->size > 0) {
		ret = tplg_add_data(tplg, elem, priv,
				    sizeof(*priv) + priv->size);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* Set link HW config from C API template */
static int set_link_hw_config(struct snd_soc_tplg_hw_config *cfg,
			      struct snd_tplg_hw_config_template *tpl)
{
	unsigned int i;

	cfg->size = sizeof(*cfg);
	cfg->id = tpl->id;

	cfg->fmt = tpl->fmt;
	cfg->clock_gated = tpl->clock_gated;
	cfg->invert_bclk = tpl->invert_bclk;
	cfg->invert_fsync = tpl->invert_fsync;
	cfg->bclk_provider = tpl->bclk_provider;
	cfg->fsync_provider = tpl->fsync_provider;
	cfg->mclk_direction = tpl->mclk_direction;
	cfg->reserved = tpl->reserved;
	cfg->mclk_rate = tpl->mclk_rate;
	cfg->bclk_rate = tpl->bclk_rate;
	cfg->fsync_rate = tpl->fsync_rate;

	cfg->tdm_slots = tpl->tdm_slots;
	cfg->tdm_slot_width = tpl->tdm_slot_width;
	cfg->tx_slots = tpl->tx_slots;
	cfg->rx_slots = tpl->rx_slots;

	if (cfg->tx_channels > SND_SOC_TPLG_MAX_CHAN
		|| cfg->rx_channels > SND_SOC_TPLG_MAX_CHAN)
		return -EINVAL;

	cfg->tx_channels = tpl->tx_channels;
	for (i = 0; i < cfg->tx_channels; i++)
		cfg->tx_chanmap[i] = tpl->tx_chanmap[i];

	cfg->rx_channels = tpl->rx_channels;
	for (i = 0; i < cfg->rx_channels; i++)
		cfg->rx_chanmap[i] = tpl->rx_chanmap[i];

	return 0;
}

/* Add a physical DAI link element from C API */
int tplg_add_link_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_link_template *link_tpl = t->link;
	struct snd_soc_tplg_link_config *link;
	struct snd_soc_tplg_private *priv;
	struct tplg_elem *elem;
	unsigned int i;
	int ret;

	if (t->type != SND_TPLG_TYPE_LINK && t->type != SND_TPLG_TYPE_BE
	    && t->type != SND_TPLG_TYPE_CC)
		return -EINVAL;

	elem = tplg_elem_new_common(tplg, NULL, link_tpl->name, t->type);
	if (!elem)
		return -ENOMEM;

	tplg_dbg("Link: %s", link_tpl->name);

	link = elem->link;
	link->size = elem->size;

	/* ID and names */
	link->id = link_tpl->id;
	snd_strlcpy(link->name, link_tpl->name,
		       SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	snd_strlcpy(link->stream_name, link_tpl->stream_name,
		       SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	/* stream configs */
	if (link_tpl->num_streams > SND_SOC_TPLG_STREAM_CONFIG_MAX)
		return -EINVAL;
	link->num_streams = link_tpl->num_streams;
	for (i = 0; i < link->num_streams; i++)
		tplg_add_stream_object(&link->stream[i], &link_tpl->stream[i]);

	/* HW configs */
	if (link_tpl->num_hw_configs > SND_SOC_TPLG_HW_CONFIG_MAX)
		return -EINVAL;
	link->num_hw_configs = link_tpl->num_hw_configs;
	link->default_hw_config_id = link_tpl->default_hw_config_id;
	for (i = 0; i < link->num_hw_configs; i++)
		set_link_hw_config(&link->hw_config[i], &link_tpl->hw_config[i]);

	/* flags */
	link->flag_mask = link_tpl->flag_mask;
	link->flags = link_tpl->flags;

	/* private data */
	priv = link_tpl->priv;
	if (priv && priv->size > 0) {
		ret = tplg_add_data(tplg, elem, priv,
				    sizeof(*priv) + priv->size);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int tplg_add_dai_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_dai_template *dai_tpl = t->dai;
	struct snd_soc_tplg_dai *dai;
	struct snd_soc_tplg_private *priv;
	struct tplg_elem *elem;
	int ret, i;

	tplg_dbg("DAI %s", dai_tpl->dai_name);

	elem = tplg_elem_new_common(tplg, NULL, dai_tpl->dai_name,
				    SND_TPLG_TYPE_DAI);
	if (!elem)
		return -ENOMEM;

	dai = elem->dai;
	dai->size = elem->size;

	snd_strlcpy(dai->dai_name, dai_tpl->dai_name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	dai->dai_id = dai_tpl->dai_id;

	/* stream caps */
	dai->playback = dai_tpl->playback;
	dai->capture = dai_tpl->capture;

	for (i = 0; i < 2; i++) {
		if (!dai_tpl->caps[i] || !dai_tpl->caps[i]->name)
			continue;
		ret = tplg_add_stream_caps(tplg, dai_tpl->caps[i]);
		if (ret < 0)
			return ret;
		snd_strlcpy(dai->caps[i].name, dai_tpl->caps[i]->name,
			    sizeof(dai->caps[i].name));
	}

	/* flags */
	dai->flag_mask = dai_tpl->flag_mask;
	dai->flags = dai_tpl->flags;

	/* private data */
	priv = dai_tpl->priv;
	if (priv && priv->size > 0) {
		ret = tplg_add_data(tplg, elem, priv,
				    sizeof(*priv) + priv->size);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* decode pcm from the binary input */
int tplg_decode_pcm(snd_tplg_t *tplg,
		    size_t pos,
		    struct snd_soc_tplg_hdr *hdr,
		    void *bin, size_t size)
{
	struct snd_soc_tplg_pcm *pcm;
	snd_tplg_obj_template_t t;
	struct snd_tplg_pcm_template *pt;
	struct snd_tplg_stream_caps_template caps[2], *cap;
	struct snd_tplg_stream_template *stream;
	unsigned int i;
	size_t asize;
	int err;

	err = tplg_decode_template(tplg, pos, hdr, &t);
	if (err < 0)
		return err;

	asize = sizeof(*pt) + SND_SOC_TPLG_STREAM_CONFIG_MAX * sizeof(*stream);
	pt = alloca(asize);

next:
	memset(pt, 0, asize);
	pcm = bin;

	if (size < sizeof(*pcm)) {
		SNDERR("pcm: small size %d", size);
		return -EINVAL;
	}
	if (sizeof(*pcm) != pcm->size) {
		SNDERR("pcm: unknown element size %d (expected %zd)",
		       pcm->size, sizeof(*pcm));
		return -EINVAL;
	}
	if (pcm->num_streams > SND_SOC_TPLG_STREAM_CONFIG_MAX) {
		SNDERR("pcm: wrong number of streams %d", pcm->num_streams);
		return -EINVAL;
	}
	if (sizeof(*pcm) + pcm->priv.size > size) {
		SNDERR("pcm: wrong private data size %d", pcm->priv.size);
		return -EINVAL;
	}

	tplg_log(tplg, 'D', pos, "pcm: size %d private size %d streams %d",
		 pcm->size, pcm->priv.size, pcm->num_streams);

	pt->pcm_name = pcm->pcm_name;
	tplg_log(tplg, 'D', pos, "pcm: pcm_name '%s'", pt->pcm_name);
	pt->dai_name = pcm->dai_name;
	tplg_log(tplg, 'D', pos, "pcm: dai_name '%s'", pt->dai_name);
	pt->pcm_id = pcm->pcm_id;
	pt->dai_id = pcm->dai_id;
	tplg_log(tplg, 'D', pos, "pcm: pcm_id %d dai_id %d", pt->pcm_id, pt->dai_id);
	pt->playback = pcm->playback;
	pt->capture = pcm->capture;
	pt->compress = pcm->compress;
	tplg_log(tplg, 'D', pos, "pcm: playback %d capture %d compress %d",
		 pt->playback, pt->capture, pt->compress);
	pt->num_streams = pcm->num_streams;
	pt->flag_mask = pcm->flag_mask;
	pt->flags = pcm->flags;
	for (i = 0; i < pcm->num_streams; i++) {
		stream = &pt->stream[i];
		if (pcm->stream[i].size != sizeof(pcm->stream[0])) {
			SNDERR("pcm: unknown stream structure size %d",
			       pcm->stream[i].size);
			return -EINVAL;
		}
		stream->name = pcm->stream[i].name;
		tplg_log(tplg, 'D', pos + offsetof(struct snd_soc_tplg_pcm, stream[i]),
			 "stream %d: '%s'", i, stream->name);
		stream->format = pcm->stream[i].format;
		stream->rate = pcm->stream[i].rate;
		stream->period_bytes = pcm->stream[i].period_bytes;
		stream->buffer_bytes = pcm->stream[i].buffer_bytes;
		stream->channels = pcm->stream[i].channels;
	}
	for (i = 0; i < 2; i++) {
		if (i == 0 && !pcm->playback)
			continue;
		if (i == 1 && !pcm->capture)
			continue;
		cap = &caps[i];
		pt->caps[i] = cap;
		if (pcm->caps[i].size != sizeof(pcm->caps[0])) {
			SNDERR("pcm: unknown caps structure size %d",
			       pcm->caps[i].size);
			return -EINVAL;
		}
		cap->name = pcm->caps[i].name;
		tplg_log(tplg, 'D', pos + offsetof(struct snd_soc_tplg_pcm, caps[i]),
			 "caps %d: '%s'", i, cap->name);
		cap->formats = pcm->caps[i].formats;
		cap->rates = pcm->caps[i].rates;
		cap->rate_min = pcm->caps[i].rate_min;
		cap->rate_max = pcm->caps[i].rate_max;
		cap->channels_min = pcm->caps[i].channels_min;
		cap->channels_max = pcm->caps[i].channels_max;
		cap->periods_min = pcm->caps[i].periods_min;
		cap->periods_max = pcm->caps[i].periods_max;
		cap->period_size_min = pcm->caps[i].period_size_min;
		cap->period_size_max = pcm->caps[i].period_size_max;
		cap->buffer_size_min = pcm->caps[i].buffer_size_min;
		cap->buffer_size_max = pcm->caps[i].buffer_size_max;
		cap->sig_bits = pcm->caps[i].sig_bits;
	}

	tplg_log(tplg, 'D', pos + offsetof(struct snd_soc_tplg_pcm, priv),
		 "pcm: private start");
	pt->priv = &pcm->priv;

	bin += sizeof(*pcm) + pcm->priv.size;
	size -= sizeof(*pcm) + pcm->priv.size;
	pos += sizeof(*pcm) + pcm->priv.size;

	t.pcm = pt;
	err = snd_tplg_add_object(tplg, &t);
	if (err < 0)
		return err;

	if (size > 0)
		goto next;

	return 0;
}

/* decode dai from the binary input */
int tplg_decode_dai(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		    size_t pos ATTRIBUTE_UNUSED,
		    struct snd_soc_tplg_hdr *hdr ATTRIBUTE_UNUSED,
		    void *bin ATTRIBUTE_UNUSED,
		    size_t size ATTRIBUTE_UNUSED)
{
	SNDERR("not implemented");
	return -ENXIO;
}

/* decode cc from the binary input */
int tplg_decode_cc(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		   size_t pos ATTRIBUTE_UNUSED,
		   struct snd_soc_tplg_hdr *hdr ATTRIBUTE_UNUSED,
		   void *bin ATTRIBUTE_UNUSED,
		   size_t size ATTRIBUTE_UNUSED)
{
	SNDERR("not implemented");
	return -ENXIO;
}

/* decode link from the binary input */
int tplg_decode_link(snd_tplg_t *tplg,
		     size_t pos,
		     struct snd_soc_tplg_hdr *hdr,
		     void *bin, size_t size)
{
	struct snd_soc_tplg_link_config *link;
	snd_tplg_obj_template_t t;
	struct snd_tplg_link_template lt;
	struct snd_tplg_stream_template streams[SND_SOC_TPLG_STREAM_CONFIG_MAX];
	struct snd_tplg_stream_template *stream;
	struct snd_tplg_hw_config_template hws[SND_SOC_TPLG_HW_CONFIG_MAX];
	struct snd_tplg_hw_config_template *hw;
	unsigned int i, j;
	int err;

	err = tplg_decode_template(tplg, pos, hdr, &t);
	if (err < 0)
		return err;

next:
	memset(&lt, 0, sizeof(lt));
	memset(streams, 0, sizeof(streams));
	memset(hws, 0, sizeof(hws));
	link = bin;

	if (size < sizeof(*link)) {
		SNDERR("link: small size %d", size);
		return -EINVAL;
	}
	if (sizeof(*link) != link->size) {
		SNDERR("link: unknown element size %d (expected %zd)",
		       link->size, sizeof(*link));
		return -EINVAL;
	}
	if (link->num_streams > SND_SOC_TPLG_STREAM_CONFIG_MAX) {
		SNDERR("link: wrong number of streams %d", link->num_streams);
		return -EINVAL;
	}
	if (link->num_hw_configs > SND_SOC_TPLG_HW_CONFIG_MAX) {
		SNDERR("link: wrong number of streams %d", link->num_streams);
		return -EINVAL;
	}
	if (sizeof(*link) + link->priv.size > size) {
		SNDERR("link: wrong private data size %d", link->priv.size);
		return -EINVAL;
	}

	tplg_log(tplg, 'D', pos, "link: size %d private size %d streams %d "
		 "hw_configs %d",
		 link->size, link->priv.size, link->num_streams,
		 link->num_hw_configs);

	lt.id = link->id;
	lt.name = link->name;
	tplg_log(tplg, 'D', pos, "link: name '%s'", lt.name);
	lt.stream_name = link->stream_name;
	tplg_log(tplg, 'D', pos, "link: stream_name '%s'", lt.stream_name);
	lt.num_streams = link->num_streams;
	lt.num_hw_configs = link->num_hw_configs;
	lt.default_hw_config_id = link->default_hw_config_id;
	lt.flag_mask = link->flag_mask;
	lt.flags = link->flags;
	for (i = 0; i < link->num_streams; i++) {
		stream = &streams[i];
		if (link->stream[i].size != sizeof(link->stream[0])) {
			SNDERR("link: unknown stream structure size %d",
			       link->stream[i].size);
			return -EINVAL;
		}
		stream->name = link->stream[i].name;
		tplg_log(tplg, 'D',
			 pos + offsetof(struct snd_soc_tplg_link_config, stream[i]),
			 "stream %d: '%s'", i, stream->name);
		stream->format = link->stream[i].format;
		stream->rate = link->stream[i].rate;
		stream->period_bytes = link->stream[i].period_bytes;
		stream->buffer_bytes = link->stream[i].buffer_bytes;
		stream->channels = link->stream[i].channels;
	}
	lt.stream = streams;
	for (i = 0; i < link->num_hw_configs; i++) {
		hw = &hws[i];
		if (link->hw_config[i].size != sizeof(link->hw_config[0])) {
			SNDERR("link: unknown hw_config structure size %d",
			       link->hw_config[i].size);
			return -EINVAL;
		}
		hw->id = link->hw_config[i].id;
		hw->fmt = link->hw_config[i].fmt;
		hw->clock_gated = link->hw_config[i].clock_gated;
		hw->invert_bclk = link->hw_config[i].invert_bclk;
		hw->invert_fsync = link->hw_config[i].invert_fsync;
		hw->bclk_provider = link->hw_config[i].bclk_provider;
		hw->fsync_provider = link->hw_config[i].fsync_provider;
		hw->mclk_direction = link->hw_config[i].mclk_direction;
		hw->mclk_rate = link->hw_config[i].mclk_rate;
		hw->bclk_rate = link->hw_config[i].bclk_rate;
		hw->fsync_rate = link->hw_config[i].fsync_rate;
		hw->tdm_slots = link->hw_config[i].tdm_slots;
		hw->tdm_slot_width = link->hw_config[i].tdm_slot_width;
		hw->tx_slots = link->hw_config[i].tx_slots;
		hw->rx_slots = link->hw_config[i].rx_slots;
		hw->tx_channels = link->hw_config[i].tx_channels;
		if (hw->tx_channels > SND_SOC_TPLG_MAX_CHAN) {
			SNDERR("link: wrong tx channels %d", hw->tx_channels);
			return -EINVAL;
		}
		for (j = 0; j < hw->tx_channels; j++)
			hw->tx_chanmap[j] = link->hw_config[i].tx_chanmap[j];
		hw->rx_channels = link->hw_config[i].rx_channels;
		if (hw->rx_channels > SND_SOC_TPLG_MAX_CHAN) {
			SNDERR("link: wrong rx channels %d", hw->tx_channels);
			return -EINVAL;
		}
		for (j = 0; j < hw->rx_channels; j++)
			hw->rx_chanmap[j] = link->hw_config[i].rx_chanmap[j];
	}
	lt.hw_config = hws;

	tplg_log(tplg, 'D', pos + offsetof(struct snd_soc_tplg_pcm, priv),
		 "link: private start");
	lt.priv = &link->priv;

	bin += sizeof(*link) + link->priv.size;
	size -= sizeof(*link) + link->priv.size;
	pos += sizeof(*link) + link->priv.size;

	t.link = &lt;
	err = snd_tplg_add_object(tplg, &t);
	if (err < 0)
		return err;

	if (size > 0)
		goto next;

	return 0;
}
