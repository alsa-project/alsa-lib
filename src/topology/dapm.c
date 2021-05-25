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

/* mapping of widget text names to types */
static const struct map_elem widget_map[] = {
	{"input", SND_SOC_TPLG_DAPM_INPUT},
	{"output", SND_SOC_TPLG_DAPM_OUTPUT},
	{"mux", SND_SOC_TPLG_DAPM_MUX},
	{"mixer", SND_SOC_TPLG_DAPM_MIXER},
	{"pga", SND_SOC_TPLG_DAPM_PGA},
	{"out_drv", SND_SOC_TPLG_DAPM_OUT_DRV},
	{"adc", SND_SOC_TPLG_DAPM_ADC},
	{"dac", SND_SOC_TPLG_DAPM_DAC},
	{"switch", SND_SOC_TPLG_DAPM_SWITCH},
	{"pre", SND_SOC_TPLG_DAPM_PRE},
	{"post", SND_SOC_TPLG_DAPM_POST},
	{"aif_in", SND_SOC_TPLG_DAPM_AIF_IN},
	{"aif_out", SND_SOC_TPLG_DAPM_AIF_OUT},
	{"dai_in", SND_SOC_TPLG_DAPM_DAI_IN},
	{"dai_out", SND_SOC_TPLG_DAPM_DAI_OUT},
	{"dai_link", SND_SOC_TPLG_DAPM_DAI_LINK},
	{"buffer", SND_SOC_TPLG_DAPM_BUFFER},
	{"scheduler", SND_SOC_TPLG_DAPM_SCHEDULER},
	{"effect", SND_SOC_TPLG_DAPM_EFFECT},
	{"siggen", SND_SOC_TPLG_DAPM_SIGGEN},
	{"src", SND_SOC_TPLG_DAPM_SRC},
	{"asrc", SND_SOC_TPLG_DAPM_ASRC},
	{"encoder", SND_SOC_TPLG_DAPM_ENCODER},
	{"decoder", SND_SOC_TPLG_DAPM_DECODER},
};

static int lookup_widget(const char *w)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(widget_map); i++) {
		if (strcmp(widget_map[i].name, w) == 0)
			return widget_map[i].id;
	}

	return -EINVAL;
}

static const char *get_widget_name(unsigned int type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(widget_map); i++) {
		if ((unsigned int)widget_map[i].id == type)
			return widget_map[i].name;
	}

	return NULL;
}

/* move referenced controls to the widget */
static int copy_dapm_control(struct tplg_elem *elem, struct tplg_elem *ref)
{
	struct snd_soc_tplg_dapm_widget *widget = elem->widget;

	tplg_dbg("Control '%s' used by '%s'", ref->id, elem->id);
	tplg_dbg("\tparent size: %d + %d -> %d, priv size -> %d",
		elem->size, ref->size, elem->size + ref->size,
		widget->priv.size);

	widget = realloc(widget, elem->size + ref->size);
	if (!widget)
		return -ENOMEM;

	elem->widget = widget;

	/* append the control to the end of the widget */
	memcpy((void*)widget + elem->size, ref->obj, ref->size);
	elem->size += ref->size;

	widget->num_kcontrols++;
	ref->compound_elem = 1;
	return 0;
}

/* check referenced controls for a widget */
static int tplg_build_widget(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err = 0;

	base = &elem->ref_list;

	/* A widget's private data sits before the embedded controls.
	 * So merge the private data blocks at first
	 */
	list_for_each(pos, base) {
		ref = list_entry(pos, struct tplg_ref, list);

		if (ref->type != SND_TPLG_TYPE_DATA)
			continue;

		err = tplg_copy_data(tplg, elem, ref);
		if (err < 0)
			return err;
	}

	/* Merge the embedded controls */
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);

		switch (ref->type) {
		case SND_TPLG_TYPE_MIXER:
			if (!ref->elem)
				ref->elem = tplg_elem_lookup(&tplg->mixer_list,
				ref->id, SND_TPLG_TYPE_MIXER, elem->index);
			if (ref->elem)
				err = copy_dapm_control(elem, ref->elem);
			break;

		case SND_TPLG_TYPE_ENUM:
			if (!ref->elem)
				ref->elem = tplg_elem_lookup(&tplg->enum_list,
				ref->id, SND_TPLG_TYPE_ENUM, elem->index);
			if (ref->elem)
				err = copy_dapm_control(elem, ref->elem);
			break;

		case SND_TPLG_TYPE_BYTES:
			if (!ref->elem)
				ref->elem = tplg_elem_lookup(&tplg->bytes_ext_list,
				ref->id, SND_TPLG_TYPE_BYTES, elem->index);
			if (ref->elem)
				err = copy_dapm_control(elem, ref->elem);
			break;

		default:
			break;
		}

		if (!ref->elem) {
			SNDERR("cannot find '%s' referenced by widget '%s'",
				ref->id, elem->id);
			return -EINVAL;
		}

		if (err < 0)
			return err;
	}

	return 0;
}

int tplg_build_widgets(snd_tplg_t *tplg)
{

	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err;

	base = &tplg->widget_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (!elem->widget || elem->type != SND_TPLG_TYPE_DAPM_WIDGET) {
			SNDERR("invalid widget '%s'", elem->id);
			return -EINVAL;
		}

		err = tplg_build_widget(tplg, elem);
		if (err < 0)
			return err;

		/* add widget to manifest */
		tplg->manifest.widget_elems++;
	}

	return 0;
}

int tplg_build_routes(snd_tplg_t *tplg)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	struct snd_soc_tplg_dapm_graph_elem *route;

	base = &tplg->route_list;

	list_for_each(pos, base) {
		elem = list_entry(pos, struct tplg_elem, list);

		if (!elem->route || elem->type != SND_TPLG_TYPE_DAPM_GRAPH) {
			SNDERR("invalid route '%s'", elem->id);
			return -EINVAL;
		}

		route = elem->route;
		tplg_dbg("Check route: sink '%s', control '%s', source '%s'",
			 route->sink, route->control, route->source);

		/* validate sink */
		if (strlen(route->sink) <= 0) {
			SNDERR("no sink");
			return -EINVAL;

		}
		if (!tplg_elem_lookup(&tplg->widget_list, route->sink,
			SND_TPLG_TYPE_DAPM_WIDGET, SND_TPLG_INDEX_ALL)) {
			SNDERR("undefined sink widget/stream '%s'", route->sink);
		}

		/* validate control name */
		if (strlen(route->control)) {
			if (!tplg_elem_lookup(&tplg->mixer_list, route->control,
					SND_TPLG_TYPE_MIXER, elem->index) &&
			!tplg_elem_lookup(&tplg->enum_list, route->control,
					SND_TPLG_TYPE_ENUM, elem->index)) {
				SNDERR("undefined mixer/enum control '%s'",
				       route->control);
			}
		}

		/* validate source */
		if (strlen(route->source) <= 0) {
			SNDERR("no source");
			return -EINVAL;

		}
		if (!tplg_elem_lookup(&tplg->widget_list, route->source,
			SND_TPLG_TYPE_DAPM_WIDGET, SND_TPLG_INDEX_ALL)) {
			SNDERR("undefined source widget/stream '%s'",
			       route->source);
		}

		/* add graph to manifest */
		tplg->manifest.graph_elems++;
	}

	return 0;
}

struct tplg_elem *tplg_elem_new_route(snd_tplg_t *tplg, int index)
{
	struct tplg_elem *elem;
	struct snd_soc_tplg_dapm_graph_elem *line;

	elem = tplg_elem_new();
	if (!elem)
		return NULL;

	elem->index = index;
	if (tplg->dapm_sort)
		tplg_elem_insert(elem, &tplg->route_list);
	else
		list_add_tail(&elem->list, &tplg->route_list);
	strcpy(elem->id, "line");
	elem->type = SND_TPLG_TYPE_DAPM_GRAPH;
	elem->size = sizeof(*line);

	line = calloc(1, sizeof(*line));
	if (!line) {
		tplg_elem_free(elem);
		return NULL;
	}
	elem->route = line;

	return elem;
}

#define LINE_SIZE	1024

/* line is defined as '"sink, control, source"' */
static int tplg_parse_line(const char *text,
			   struct snd_soc_tplg_dapm_graph_elem *line)
{
	char buf[LINE_SIZE];
	unsigned int len, i;
	const char *source = NULL, *sink = NULL, *control = NULL;

	snd_strlcpy(buf, text, LINE_SIZE);

	len = strlen(buf);
	if (len <= 2) {
		SNDERR("invalid route \"%s\"", buf);
		return -EINVAL;
	}

	/* find first , */
	for (i = 1; i < len; i++) {
		if (buf[i] == ',')
			goto second;
	}
	SNDERR("invalid route \"%s\"", buf);
	return -EINVAL;

second:
	/* find second , */
	sink = buf;
	control = &buf[i + 2];
	buf[i] = 0;

	for (; i < len; i++) {
		if (buf[i] == ',')
			goto done;
	}

	SNDERR("invalid route \"%s\"", buf);
	return -EINVAL;

done:
	buf[i] = 0;
	source = &buf[i + 2];

	strcpy(line->source, source);
	strcpy(line->control, control);
	strcpy(line->sink, sink);
	return 0;
}


static int tplg_parse_routes(snd_tplg_t *tplg, snd_config_t *cfg, int index)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct tplg_elem *elem;
	struct snd_soc_tplg_dapm_graph_elem *line;
	int err;

	snd_config_for_each(i, next, cfg) {
		const char *val;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_string(n, &val) < 0)
			continue;

		elem = tplg_elem_new_route(tplg, index);
		if (!elem)
			return -ENOMEM;
		line = elem->route;

		err = tplg_parse_line(val, line);
		if (err < 0)
			return err;

		tplg_dbg("route: sink '%s', control '%s', source '%s'",
				line->sink, line->control, line->source);
	}

	return 0;
}

int tplg_parse_dapm_graph(snd_tplg_t *tplg, snd_config_t *cfg,
			  void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;
	const char *graph_id;
	int index = -1;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("compound is expected for dapm graph definition");
		return -EINVAL;
	}

	snd_config_get_id(cfg, &graph_id);

	snd_config_for_each(i, next, cfg) {
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0) {
			continue;
		}

		if (strcmp(id, "index") == 0) {
			if (tplg_get_integer(n, &index, 0))
				return -EINVAL;
			if (index < 0)
				return -EINVAL;
		}

		if (strcmp(id, "lines") == 0) {
			if (index < 0) {
				SNDERR("failed to parse dapm graph %s, missing index",
					graph_id);
				return -EINVAL;
			}
			err = tplg_parse_routes(tplg, n, index);
			if (err < 0) {
				SNDERR("failed to parse dapm graph %s",
					graph_id);
				return err;
			}
			continue;
		}
	}

	return 0;
}

/* save DAPM graph */
int tplg_save_dapm_graph(snd_tplg_t *tplg, int index,
			 struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_dapm_graph_elem *route;
	struct list_head *pos;
	struct tplg_elem *elem;
	int err, first, old_index;
	unsigned block, count;
	const char *fmt;

	old_index = -1;
	block = 0;
	count = 0;
	list_for_each(pos, &tplg->route_list) {
		elem = list_entry(pos, struct tplg_elem, list);
		if (!elem->route || elem->type != SND_TPLG_TYPE_DAPM_GRAPH)
			continue;
		if (index >= 0 && elem->index != index)
			continue;
		if (old_index != elem->index) {
			block++;
			old_index = elem->index;
		}
		count++;
	}
	if (count == 0)
		return 0;
	if (block < 10) {
		fmt = "\tset%u {\n";
	} else if (block < 100) {
		fmt = "\tset%02u {\n";
	} else if (block < 1000) {
		fmt = "\tset%03u {\n";
	} else {
		return -EINVAL;
	}
	old_index = -1;
	block = -1;
	first = 1;
	err = tplg_save_printf(dst, pfx, "SectionGraph {\n");
	list_for_each(pos, &tplg->route_list) {
		elem = list_entry(pos, struct tplg_elem, list);
		if (!elem->route || elem->type != SND_TPLG_TYPE_DAPM_GRAPH)
			continue;
		if (index >= 0 && elem->index != index)
			continue;
		if (old_index != elem->index) {
			if (old_index >= 0) {
				err = tplg_save_printf(dst, pfx, "\t\t]\n");
				if (err < 0)
					return err;
				err = tplg_save_printf(dst, pfx, "\t}\n");
				if (err < 0)
					return err;
			}
			old_index = elem->index;
			block++;
			first = 1;
			err = tplg_save_printf(dst, pfx, fmt, block);
			if (err >= 0)
				err = tplg_save_printf(dst, pfx, "\t\tindex %u\n",
						       elem->index);
			if (err < 0)
				return err;
		}
		if (first) {
			first = 0;
			err = tplg_save_printf(dst, pfx, "\t\tlines [\n");
			if (err < 0)
				return err;
		}
		route = elem->route;
		err = tplg_save_printf(dst, pfx, "\t\t\t'%s, %s, %s'\n",
					route->sink, route->control,
					route->source);
		if (err < 0)
			return err;
	}

	if (!first) {
		if (err >= 0)
			err = tplg_save_printf(dst, pfx, "\t\t]\n");
		if (err >= 0)
			err = tplg_save_printf(dst, pfx, "\t}\n");
	}

	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* DAPM Widget */
int tplg_parse_dapm_widget(snd_tplg_t *tplg,
			   snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_dapm_widget *widget;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int widget_type, err, ival;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_DAPM_WIDGET);
	if (!elem)
		return -ENOMEM;

	tplg_dbg(" Widget: %s", elem->id);

	widget = elem->widget;
	snd_strlcpy(widget->name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	widget->size = elem->size;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "type") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			widget_type = lookup_widget(val);
			if (widget_type < 0){
				SNDERR("widget '%s': Unsupported widget type %s",
					elem->id, val);
				return -EINVAL;
			}

			widget->id = widget_type;
			tplg_dbg("\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "stream_name") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			snd_strlcpy(widget->sname, val,
				       SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
			tplg_dbg("\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "no_pm") == 0) {
			ival = snd_config_get_bool(n);
			if (ival < 0)
				return -EINVAL;

			widget->reg = ival ? -1 : 0;

			tplg_dbg("\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "shift") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			widget->shift = ival;
			tplg_dbg("\t%s: %d", id, widget->shift);
			continue;
		}

		if (strcmp(id, "reg") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			widget->reg = ival;
			tplg_dbg("\t%s: %d", id, widget->reg);
			continue;
		}

		if (strcmp(id, "invert") == 0) {
			ival = snd_config_get_bool(n);
			if (ival < 0)
				return -EINVAL;

			widget->invert = ival;
			tplg_dbg("\t%s: %d", id, widget->invert);
			continue;
		}

		if (strcmp(id, "ignore_suspend") == 0) {
			ival = snd_config_get_bool(n);
			if (ival < 0)
				return -EINVAL;

			widget->ignore_suspend = ival;

			tplg_dbg("\t%s: %s", id, val);
			continue;
		}

		if (strcmp(id, "subseq") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			widget->subseq = ival;
			tplg_dbg("\t%s: %d", id, widget->subseq);
			continue;
		}

		if (strcmp(id, "event_type") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			widget->event_type = ival;
			tplg_dbg("\t%s: %d", id, widget->event_type);
			continue;
		}

		if (strcmp(id, "event_flags") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			widget->event_flags = ival;
			tplg_dbg("\t%s: %d", id, widget->event_flags);
			continue;
		}

		if (strcmp(id, "enum") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_ENUM);
			if (err < 0)
				return err;

			continue;
		}

		if (strcmp(id, "mixer") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_MIXER);
			if (err < 0)
				return err;

			continue;
		}

		if (strcmp(id, "bytes") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_BYTES);
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
	}

	return 0;
}

/* save DAPM widget */
int tplg_save_dapm_widget(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			  struct tplg_elem *elem,
			  struct tplg_buf *dst, const char *pfx)
{
	struct snd_soc_tplg_dapm_widget *widget = elem->widget;
	const char *s;
	char pfx2[16];
	int err;

	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err >= 0 && elem->index)
		err = tplg_save_printf(dst, pfx, "\tindex %u\n",
				       elem->index);
	if (err >= 0) {
		s = get_widget_name(widget->id);
		if (s)
			err = tplg_save_printf(dst, pfx, "\ttype %s\n", s);
		else
			err = tplg_save_printf(dst, pfx, "\ttype %u\n",
					       widget->id);
	}
	if (err >= 0 && widget->sname[0])
		err = tplg_save_printf(dst, pfx, "\tstream_name '%s'\n",
				       widget->sname);
	if (err >= 0 && widget->reg)
		err = tplg_save_printf(dst, pfx, "\tno_pm 1\n");
	if (err >= 0 && widget->shift)
		err = tplg_save_printf(dst, pfx, "\tshift %u\n",
				       widget->shift);
	if (err >= 0 && widget->invert)
		err = tplg_save_printf(dst, pfx, "\tinvert %u\n",
				       widget->invert);
	if (err >= 0 && widget->ignore_suspend)
		err = tplg_save_printf(dst, pfx, "\tignore_suspend %u\n",
				       widget->ignore_suspend);
	if (err >= 0 && widget->subseq)
		err = tplg_save_printf(dst, pfx, "\tsubseq %u\n",
				       widget->subseq);
	if (err >= 0 && widget->event_type)
		err = tplg_save_printf(dst, pfx, "\tevent_type %u\n",
				       widget->event_type);
	if (err >= 0 && widget->event_flags)
		err = tplg_save_printf(dst, pfx, "\tevent_flags %u\n",
				       widget->event_flags);
	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_ENUM,
				     "enum", dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_MIXER,
				     "mixer", dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_BYTES,
				     "bytes", dst, pfx2);
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_DATA,
				     "data", dst, pfx2);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

int tplg_add_route(snd_tplg_t *tplg, struct snd_tplg_graph_elem *t, int index)
{
	struct tplg_elem *elem;
	struct snd_soc_tplg_dapm_graph_elem *line;

	if (!t->src || !t->sink)
		return -EINVAL;

	elem = tplg_elem_new_route(tplg, index);
	if (!elem)
		return -ENOMEM;

	line = elem->route;
	snd_strlcpy(line->source, t->src, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	if (t->ctl)
		snd_strlcpy(line->control, t->ctl,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	snd_strlcpy(line->sink, t->sink, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	return 0;
}

int tplg_add_graph_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_graph_template *gt =  t->graph;
	int i, ret;

	for (i = 0; i < gt->count; i++) {
		ret = tplg_add_route(tplg, gt->elem + i, t->index);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int tplg_add_widget_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_widget_template *wt = t->widget;
	struct snd_soc_tplg_dapm_widget *w;
	struct tplg_elem *elem;
	int i, ret = 0;

	tplg_dbg("Widget: %s", wt->name);

	elem = tplg_elem_new_common(tplg, NULL, wt->name,
		SND_TPLG_TYPE_DAPM_WIDGET);
	if (!elem)
		return -ENOMEM;

	/* init new widget */
	w = elem->widget;
	w->size = elem->size;

	w->id = wt->id;
	snd_strlcpy(w->name, wt->name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	if (wt->sname)
		snd_strlcpy(w->sname, wt->sname, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	w->reg = wt->reg;
	w->shift = wt->shift;
	w->mask = wt->mask;
	w->subseq = wt->subseq;
	w->invert = wt->invert;
	w->ignore_suspend = wt->ignore_suspend;
	w->event_flags = wt->event_flags;
	w->event_type = wt->event_type;

	/* add private data */
	if (wt->priv != NULL && wt->priv->size > 0) {
		ret = tplg_add_data(tplg, elem, wt->priv,
				    sizeof(*wt->priv) + wt->priv->size);
		if (ret < 0) {
			tplg_elem_free(elem);
			return ret;
		}
	}

	/* add controls to the widget's reference list */
	for (i = 0 ; i < wt->num_ctls; i++) {
		struct snd_tplg_ctl_template *ct = wt->ctl[i];
		struct tplg_elem *elem_ctl;
		struct snd_tplg_mixer_template *mt;
		struct snd_tplg_bytes_template *bt;
		struct snd_tplg_enum_template *et;

		if (!ct) {
			tplg_elem_free(elem);
			return -EINVAL;
		}

		switch (ct->type) {
		case SND_SOC_TPLG_TYPE_MIXER:
			mt = container_of(ct, struct snd_tplg_mixer_template, hdr);
			ret = tplg_add_mixer(tplg, mt, &elem_ctl);
			break;

		case SND_SOC_TPLG_TYPE_BYTES:
			bt = container_of(ct, struct snd_tplg_bytes_template, hdr);
			ret = tplg_add_bytes(tplg, bt, &elem_ctl);
			break;

		case SND_SOC_TPLG_TYPE_ENUM:
			et = container_of(ct, struct snd_tplg_enum_template, hdr);
			ret = tplg_add_enum(tplg, et, &elem_ctl);
			break;

		default:
			SNDERR("widget %s: invalid type %d for ctl %d",
				wt->name, ct->type, i);
			ret = -EINVAL;
			break;
		}

		if (ret < 0) {
			tplg_elem_free(elem);
			return ret;
		}

		ret = tplg_ref_add_elem(elem, elem_ctl);
		if (ret < 0) {
			tplg_elem_free(elem);
			return ret;
		}
	}

	return 0;
}

/* decode dapm widget from the binary input */
int tplg_decode_dapm_widget(snd_tplg_t *tplg,
			    size_t pos,
			    struct snd_soc_tplg_hdr *hdr,
			    void *bin, size_t size)
{
	struct list_head heap;
	struct snd_soc_tplg_dapm_widget *w;
	snd_tplg_obj_template_t t;
	struct snd_tplg_widget_template *wt;
	struct snd_tplg_mixer_template *mt;
	struct snd_tplg_enum_template *et;
	struct snd_tplg_bytes_template *bt;
	struct snd_soc_tplg_ctl_hdr *chdr;
	struct snd_soc_tplg_mixer_control *mc;
	struct snd_soc_tplg_enum_control *ec;
	struct snd_soc_tplg_bytes_control *bc;
	size_t size2;
	unsigned int index;
	int err;

	err = tplg_decode_template(tplg, pos, hdr, &t);
	if (err < 0)
		return err;

next:
	INIT_LIST_HEAD(&heap);
	w = bin;

	if (size < sizeof(*w)) {
		SNDERR("dapm widget: small size %d", size);
		return -EINVAL;
	}
	if (sizeof(*w) != w->size) {
		SNDERR("dapm widget: unknown element size %d (expected %zd)",
		       w->size, sizeof(*w));
		return -EINVAL;
	}
	if (w->num_kcontrols > 16) {
		SNDERR("dapm widget: too many kcontrols %d",
		       w->num_kcontrols);
		return -EINVAL;
	}

	tplg_log(tplg, 'D', pos, "dapm widget: size %d private size %d kcontrols %d",
		 w->size, w->priv.size, w->num_kcontrols);

	wt = tplg_calloc(&heap, sizeof(*wt) + sizeof(void *) * w->num_kcontrols);
	if (wt == NULL)
		return -ENOMEM;
	wt->id = w->id;
	wt->name = w->name;
	wt->sname = w->sname;
	wt->reg = w->reg;
	wt->shift = w->shift;
	wt->mask = w->mask;
	wt->subseq = w->subseq;
	wt->invert = w->invert;
	wt->ignore_suspend = w->ignore_suspend;
	wt->event_flags = w->event_flags;
	wt->event_type = w->event_type;

	tplg_log(tplg, 'D', pos, "dapm widget: name '%s' sname '%s'",
		 wt->name, wt->sname);

	if (sizeof(*w) + w->priv.size > size) {
		SNDERR("dapm widget: wrong private data size %d",
		       w->priv.size);
		return -EINVAL;
	}

	tplg_log(tplg, 'D', pos + offsetof(struct snd_soc_tplg_dapm_widget, priv),
		 "dapm widget: private start");

	wt->priv = &w->priv;
	bin += sizeof(*w) + w->priv.size;
	size -= sizeof(*w) + w->priv.size;
	pos += sizeof(*w) + w->priv.size;

	for (index = 0; index < w->num_kcontrols; index++) {
		chdr = bin;
		switch (chdr->type) {
		case SND_SOC_TPLG_TYPE_MIXER:
			mt = tplg_calloc(&heap, sizeof(*mt));
			if (mt == NULL) {
				err = -ENOMEM;
				goto retval;
			}
			wt->ctl[index] = (void *)mt;
			wt->num_ctls++;
			mc = bin;
			size2 = mc->size + mc->priv.size;
			tplg_log(tplg, 'D', pos, "kcontrol mixer size %zd", size2);
			if (size2 > size) {
				SNDERR("dapm widget: small mixer size %d",
				       size2);
				err = -EINVAL;
				goto retval;
			}
			err = tplg_decode_control_mixer1(tplg, &heap, mt, pos,
							 bin, size2);
			break;
		case SND_SOC_TPLG_TYPE_ENUM:
			et = tplg_calloc(&heap, sizeof(*mt));
			if (et == NULL) {
				err = -ENOMEM;
				goto retval;
			}
			wt->ctl[index] = (void *)et;
			wt->num_ctls++;
			ec = bin;
			size2 = ec->size + ec->priv.size;
			tplg_log(tplg, 'D', pos, "kcontrol enum size %zd", size2);
			if (size2 > size) {
				SNDERR("dapm widget: small enum size %d",
				       size2);
				err = -EINVAL;
				goto retval;
			}
			err = tplg_decode_control_enum1(tplg, &heap, et, pos, ec);
			break;
		case SND_SOC_TPLG_TYPE_BYTES:
			bt = tplg_calloc(&heap, sizeof(*bt));
			if (bt == NULL) {
				err = -ENOMEM;
				goto retval;
			}
			wt->ctl[index] = (void *)bt;
			wt->num_ctls++;
			bc = bin;
			size2 = bc->size + bc->priv.size;
			tplg_log(tplg, 'D', pos, "kcontrol bytes size %zd", size2);
			if (size2 > size) {
				SNDERR("dapm widget: small bytes size %d",
				       size2);
				err = -EINVAL;
				goto retval;
			}
			err = tplg_decode_control_bytes1(tplg, bt, pos,
							 bin, size2);
			break;
		default:
			SNDERR("dapm widget: wrong control type %d",
			       chdr->type);
			err = -EINVAL;
			goto retval;
		}
		if (err < 0)
			goto retval;
		bin += size2;
		size -= size2;
		pos += size2;
	}

	t.widget = wt;
	err = snd_tplg_add_object(tplg, &t);
	tplg_free(&heap);
	if (err < 0)
		return err;
	if (size > 0)
		goto next;
	return 0;

retval:
	tplg_free(&heap);
	return err;
}

/* decode dapm link from the binary input */
int tplg_decode_dapm_graph(snd_tplg_t *tplg,
			   size_t pos,
			   struct snd_soc_tplg_hdr *hdr,
			   void *bin, size_t size)
{
	struct snd_soc_tplg_dapm_graph_elem *g;
	snd_tplg_obj_template_t t;
	struct snd_tplg_graph_template *gt;
	struct snd_tplg_graph_elem *ge;
	size_t asize;
	int err;

	err = tplg_decode_template(tplg, pos, hdr, &t);
	if (err < 0)
		return err;

	asize = sizeof(*gt) + (size / sizeof(*g)) * sizeof(*ge);
	gt = alloca(asize);
	memset(gt, 0, asize);
	for (ge = gt->elem; size > 0; ge++) {
		g = bin;
		if (size < sizeof(*g)) {
			SNDERR("dapm graph: small size %d", size);
			return -EINVAL;
		}
		ge->src = g->source;
		ge->ctl = g->control;
		ge->sink = g->sink;
		gt->count++;
		tplg_log(tplg, 'D', pos, "dapm graph: src='%s' ctl='%s' sink='%s'",
			ge->src, ge->ctl, ge->sink);
		bin += sizeof(*g);
		size -= sizeof(*g);
		pos += sizeof(*g);
	}

	t.graph = gt;
	return snd_tplg_add_object(tplg, &t);
}
