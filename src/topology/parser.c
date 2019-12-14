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

#include <sys/stat.h>
#include "list.h"
#include "tplg_local.h"

/*
 * Get integer value
 */
int tplg_get_integer(snd_config_t *n, int *val, int base)
{
	const char *str;
	long lval;
	int err;

	switch (snd_config_get_type(n)) {
	case SND_CONFIG_TYPE_INTEGER:
		err = snd_config_get_integer(n, &lval);
		if (err < 0)
			return err;
		if (lval < INT_MIN || lval > INT_MAX)
			return -ERANGE;
		*val = lval;
		return err;
	case SND_CONFIG_TYPE_STRING:
		err = snd_config_get_string(n, &str);
		if (err < 0)
			return err;
		errno = 0;
		*val = strtol(str, NULL, base);
		if (errno == ERANGE)
			return -ERANGE;
		if (errno && *val == 0)
			return -EINVAL;
		return 0;
	default:
		return -EINVAL;
	}
}

/*
 * Get unsigned integer value
 */
int tplg_get_unsigned(snd_config_t *n, unsigned *val, int base)
{
	const char *str;
	long lval;
	long long llval;
	unsigned long uval;
	int err;

	switch (snd_config_get_type(n)) {
	case SND_CONFIG_TYPE_INTEGER:
		err = snd_config_get_integer(n, &lval);
		if (err < 0)
			return err;
		if (lval < 0 || lval > UINT_MAX)
			return -ERANGE;
		*val = lval;
		return err;
	case SND_CONFIG_TYPE_INTEGER64:
		err = snd_config_get_integer64(n, &llval);
		if (err < 0)
			return err;
		if (llval < 0 || llval > UINT_MAX)
			return -ERANGE;
		*val = llval;
		return err;
	case SND_CONFIG_TYPE_STRING:
		err = snd_config_get_string(n, &str);
		if (err < 0)
			return err;
		errno = 0;
		uval = strtoul(str, NULL, base);
		if (errno == ERANGE && uval == ULONG_MAX)
			return -ERANGE;
		if (errno && uval == 0)
			return -EINVAL;
		if (uval > UINT_MAX)
			return -ERANGE;
		*val = uval;
		return 0;
	default:
		return -EINVAL;
	}
}

/*
 * Parse compound
 */
int tplg_parse_compound(snd_tplg_t *tplg, snd_config_t *cfg,
	int (*fcn)(snd_tplg_t *, snd_config_t *, void *),
	void *private)
{
	const char *id;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err = -EINVAL;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("error: compound type expected for %s", id);
		return -EINVAL;
	}

	/* parse compound */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("error: compound type expected for %s, is %d",
				id, snd_config_get_type(cfg));
			return -EINVAL;
		}

		err = fcn(tplg, n, private);
		if (err < 0)
			return err;
	}

	return err;
}

static int tplg_parse_config(snd_tplg_t *tplg, snd_config_t *cfg)
{
	static struct _parser {
		const char *id;
		int (*parser)(snd_tplg_t *tplg, snd_config_t *cfg, void *priv);
	} *p, parsers[] = {
		{
			.id = "SectionTLV",
			.parser = tplg_parse_tlv
		},
		{
			.id = "SectionControlMixer",
			.parser = tplg_parse_control_mixer
		},
		{
			.id = "SectionControlEnum",
			.parser = tplg_parse_control_enum
		},
		{
			.id = "SectionControlBytes",
			.parser = tplg_parse_control_bytes
		},
		{
			.id = "SectionWidget",
			.parser = tplg_parse_dapm_widget
		},
		{
			.id = "SectionPCMCapabilities",
			.parser = tplg_parse_stream_caps
		},
		{
			.id = "SectionPCM",
			.parser = tplg_parse_pcm
		},
		{
			.id = "SectionDAI",
			.parser = tplg_parse_dai
		},
		{
			.id = "SectionHWConfig",
			.parser = tplg_parse_hw_config
		},
		{
			.id = "SectionLink",
			.parser = tplg_parse_link
		},
		{
			.id = "SectionBE",
			.parser = tplg_parse_link
		},
		{
			.id = "SectionCC",
			.parser = tplg_parse_cc
		},
		{
			.id = "SectionGraph",
			.parser = tplg_parse_dapm_graph
		},
		{
			.id = "SectionText",
			.parser = tplg_parse_text
		},
		{
			.id = "SectionData",
			.parser = tplg_parse_data
		},
		{
			.id = "SectionVendorTokens",
			.parser = tplg_parse_tokens
		},
		{
			.id = "SectionVendorTuples",
			.parser = tplg_parse_tuples
		},
		{
			.id = "SectionManifest",
			.parser = tplg_parse_manifest_data
		},
	};
	int (*parser)(snd_tplg_t *tplg, snd_config_t *cfg, void *priv);
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	unsigned int idx;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("error: compound type expected at top level");
		return -EINVAL;
	}

	/* parse topology config sections */
	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		parser = NULL;
		for (idx = 0; idx < ARRAY_SIZE(parsers); idx++) {
			p = &parsers[idx];
			if (strcmp(id, p->id) == 0) {
				parser = p->parser;
				break;
			}
		}

		if (parser == NULL) {
			SNDERR("error: unknown section %s\n", id);
			continue;
		}

		err = tplg_parse_compound(tplg, n, parser, NULL);
		if (err < 0)
			return err;
	}
	return 0;
}

static int tplg_load_config(const char *file, snd_config_t **cfg)
{
	FILE *fp;
	snd_input_t *in;
	snd_config_t *top;
	int ret;

	fp = fopen(file, "r");
	if (fp == NULL) {
		SNDERR("error: could not open configuration file %s",
			file);
		return -errno;
	}

	ret = snd_input_stdio_attach(&in, fp, 1);
	if (ret < 0) {
		fclose(fp);
		SNDERR("error: could not attach stdio %s", file);
		return ret;
	}
	ret = snd_config_top(&top);
	if (ret < 0)
		goto err;

	ret = snd_config_load(top, in);
	if (ret < 0) {
		SNDERR("error: could not load configuration file %s",
			file);
		goto err_load;
	}

	ret = snd_input_close(in);
	if (ret < 0) {
		in = NULL;
		goto err_load;
	}

	*cfg = top;
	return 0;

err_load:
	snd_config_delete(top);
err:
	if (in)
		snd_input_close(in);
	return ret;
}

static int tplg_build_integ(snd_tplg_t *tplg)
{
	int err;

	err = tplg_build_data(tplg);
	if (err <  0)
		return err;

	err = tplg_build_manifest_data(tplg);
	if (err <  0)
		return err;

	err = tplg_build_controls(tplg);
	if (err <  0)
		return err;

	err = tplg_build_widgets(tplg);
	if (err <  0)
		return err;

	err = tplg_build_pcms(tplg, SND_TPLG_TYPE_PCM);
	if (err <  0)
		return err;

	err = tplg_build_dais(tplg, SND_TPLG_TYPE_DAI);
	if (err <  0)
		return err;

	err = tplg_build_links(tplg, SND_TPLG_TYPE_BE);
	if (err <  0)
		return err;

	err = tplg_build_links(tplg, SND_TPLG_TYPE_CC);
	if (err <  0)
		return err;

	err = tplg_build_routes(tplg);
	if (err <  0)
		return err;

	return err;
}

static int tplg_load(snd_tplg_t *tplg, const char *infile)
{
	snd_config_t *cfg = NULL;
	int err = 0;

	err = tplg_load_config(infile, &cfg);
	if (err < 0) {
		SNDERR("error: failed to load topology file %s\n",
			infile);
		return err;
	}

	err = tplg_parse_config(tplg, cfg);
	if (err < 0) {
		SNDERR("error: failed to parse topology\n");
		return err;
	}

	snd_config_delete(cfg);
	return 0;
}

static int tplg_build(snd_tplg_t *tplg)
{
	int err;

	err = tplg_build_integ(tplg);
	if (err < 0) {
		SNDERR("error: failed to check topology integrity\n");
		return err;
	}

	err = tplg_write_data(tplg);
	if (err < 0) {
		SNDERR("error: failed to write data %d\n", err);
		return err;
	}
	return 0;
}

int snd_tplg_build_file(snd_tplg_t *tplg,
			const char *infile,
			const char *outfile)
{
	int err;

	err = tplg_load(tplg, infile);
	if (err < 0)
		return err;

	return snd_tplg_build(tplg, outfile);
}

int snd_tplg_build_bin_file(snd_tplg_t *tplg,
			    const char *infile,
			    void **bin, size_t *size)
{
	int err;

	err = tplg_load(tplg, infile);
	if (err < 0)
		return err;

	return snd_tplg_build_bin(tplg, bin, size);
}

int snd_tplg_add_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	switch (t->type) {
	case SND_TPLG_TYPE_MIXER:
		return tplg_add_mixer_object(tplg, t);
	case SND_TPLG_TYPE_ENUM:
		return tplg_add_enum_object(tplg, t);
	case SND_TPLG_TYPE_BYTES:
		return tplg_add_bytes_object(tplg, t);
	case SND_TPLG_TYPE_DAPM_WIDGET:
		return tplg_add_widget_object(tplg, t);
	case SND_TPLG_TYPE_DAPM_GRAPH:
		return tplg_add_graph_object(tplg, t);
	case SND_TPLG_TYPE_PCM:
		return tplg_add_pcm_object(tplg, t);
	case SND_TPLG_TYPE_DAI:
		return tplg_add_dai_object(tplg, t);
	case SND_TPLG_TYPE_LINK:
	case SND_TPLG_TYPE_BE:
	case SND_TPLG_TYPE_CC:
		return tplg_add_link_object(tplg, t);
	default:
		SNDERR("error: invalid object type %d\n", t->type);
		return -EINVAL;
	};
}

int snd_tplg_build(snd_tplg_t *tplg, const char *outfile)
{
	int fd, err;
	ssize_t r;

	err = tplg_build(tplg);
	if (err < 0)
		return err;

	fd = open(outfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		SNDERR("error: failed to open %s err %d\n", outfile, -errno);
		return -errno;
	}
	r = write(fd, tplg->bin, tplg->bin_size);
	close(fd);
	if (r < 0) {
		err = -errno;
		SNDERR("error: write error: %s\n", strerror(errno));
		return err;
	}
	if ((size_t)r != tplg->bin_size) {
		SNDERR("error: partial write (%zd != %zd)\n", r, tplg->bin_size);
		return -EIO;
	}
	return 0;
}

int snd_tplg_build_bin(snd_tplg_t *tplg,
		       void **bin, size_t *size)
{
	int err;

	err = tplg_build(tplg);
	if (err < 0)
		return err;

	err = tplg_build(tplg);
	if (err < 0)
		return err;

	*bin = tplg->bin;
	*size = tplg->bin_size;
	tplg->bin = NULL;
	tplg->bin_size = tplg->bin_pos = 0;
	return 0;
}

int snd_tplg_set_manifest_data(snd_tplg_t *tplg, const void *data, int len)
{
	if (len <= 0)
		return 0;

	tplg->manifest.priv.size = len;

	tplg->manifest_pdata = malloc(len);
	if (!tplg->manifest_pdata)
		return -ENOMEM;

	memcpy(tplg->manifest_pdata, data, len);
	return 0;
}

int snd_tplg_set_version(snd_tplg_t *tplg, unsigned int version)
{
	tplg->version = version;

	return 0;
}

void snd_tplg_verbose(snd_tplg_t *tplg, int verbose)
{
	tplg->verbose = verbose;
}

static bool is_little_endian(void)
{
#ifdef __BYTE_ORDER
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		return true;
	#endif
#endif
	return false;
}

snd_tplg_t *snd_tplg_new(void)
{
	snd_tplg_t *tplg;

	if (!is_little_endian()) {
		SNDERR("error: cannot support big-endian machines\n");
		return NULL;
	}

	tplg = calloc(1, sizeof(snd_tplg_t));
	if (!tplg)
		return NULL;

	tplg->manifest.size = sizeof(struct snd_soc_tplg_manifest);

	INIT_LIST_HEAD(&tplg->tlv_list);
	INIT_LIST_HEAD(&tplg->widget_list);
	INIT_LIST_HEAD(&tplg->pcm_list);
	INIT_LIST_HEAD(&tplg->dai_list);
	INIT_LIST_HEAD(&tplg->be_list);
	INIT_LIST_HEAD(&tplg->cc_list);
	INIT_LIST_HEAD(&tplg->route_list);
	INIT_LIST_HEAD(&tplg->pdata_list);
	INIT_LIST_HEAD(&tplg->manifest_list);
	INIT_LIST_HEAD(&tplg->text_list);
	INIT_LIST_HEAD(&tplg->pcm_config_list);
	INIT_LIST_HEAD(&tplg->pcm_caps_list);
	INIT_LIST_HEAD(&tplg->mixer_list);
	INIT_LIST_HEAD(&tplg->enum_list);
	INIT_LIST_HEAD(&tplg->bytes_ext_list);
	INIT_LIST_HEAD(&tplg->token_list);
	INIT_LIST_HEAD(&tplg->tuple_list);
	INIT_LIST_HEAD(&tplg->hw_cfg_list);

	return tplg;
}

void snd_tplg_free(snd_tplg_t *tplg)
{
	free(tplg->bin);
	free(tplg->manifest_pdata);

	tplg_elem_free_list(&tplg->tlv_list);
	tplg_elem_free_list(&tplg->widget_list);
	tplg_elem_free_list(&tplg->pcm_list);
	tplg_elem_free_list(&tplg->dai_list);
	tplg_elem_free_list(&tplg->be_list);
	tplg_elem_free_list(&tplg->cc_list);
	tplg_elem_free_list(&tplg->route_list);
	tplg_elem_free_list(&tplg->pdata_list);
	tplg_elem_free_list(&tplg->manifest_list);
	tplg_elem_free_list(&tplg->text_list);
	tplg_elem_free_list(&tplg->pcm_config_list);
	tplg_elem_free_list(&tplg->pcm_caps_list);
	tplg_elem_free_list(&tplg->mixer_list);
	tplg_elem_free_list(&tplg->enum_list);
	tplg_elem_free_list(&tplg->bytes_ext_list);
	tplg_elem_free_list(&tplg->token_list);
	tplg_elem_free_list(&tplg->tuple_list);
	tplg_elem_free_list(&tplg->hw_cfg_list);

	free(tplg);
}
