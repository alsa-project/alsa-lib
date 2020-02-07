/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software  
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Support for the verb/device/modifier core logic and API,
 *  command line tool and file parser was kindly sponsored by
 *  Texas Instruments Inc.
 *  Support for multiple active modifiers and devices,
 *  transition sequences, multiple client access and user defined use
 *  cases was kindly sponsored by Wolfson Microelectronics PLC.
 *
 *  Copyright (C) 2008-2010 SlimLogic Ltd
 *  Copyright (C) 2010 Wolfson Microelectronics PLC
 *  Copyright (C) 2010 Texas Instruments Inc.
 *  Copyright (C) 2010 Red Hat Inc.
 *  Authors: Liam Girdwood <lrg@slimlogic.co.uk>
 *	         Stefan Schmidt <stefan@slimlogic.co.uk>
 *	         Justin Xu <justinx@slimlogic.co.uk>
 *               Jaroslav Kysela <perex@perex.cz>
 */

#include "ucm_local.h"

void uc_mgr_error(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stderr, "ucm: ");
	vfprintf(stderr, fmt, va);
	va_end(va);
}

void uc_mgr_stdout(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stdout, fmt, va);
	va_end(va);
}

struct ctl_list *uc_mgr_get_one_ctl(snd_use_case_mgr_t *uc_mgr)
{
	struct list_head *pos;
	struct ctl_list *ctl_list = NULL;

	list_for_each(pos, &uc_mgr->ctl_list) {
		if (ctl_list) {
			uc_error("multiple control device names were found!");
			return NULL;
		}
		ctl_list = list_entry(pos, struct ctl_list, list);
	}
	return ctl_list;
}

snd_ctl_t *uc_mgr_get_ctl(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_one_ctl(uc_mgr);
	if (ctl_list)
		return ctl_list->ctl;
	return NULL;
}

static void uc_mgr_free_ctl(struct ctl_list *ctl_list)
{
	struct list_head *pos, *npos;
	struct ctl_dev *ctl_dev;

	list_for_each_safe(pos, npos, &ctl_list->dev_list) {
		ctl_dev = list_entry(pos, struct ctl_dev, list);
		free(ctl_dev->device);
		free(ctl_dev);
	}
	snd_ctl_card_info_free(ctl_list->ctl_info);
	free(ctl_list);
}

void uc_mgr_free_ctl_list(snd_use_case_mgr_t *uc_mgr)
{
	struct list_head *pos, *npos;
	struct ctl_list *ctl_list;

	list_for_each_safe(pos, npos, &uc_mgr->ctl_list) {
		ctl_list = list_entry(pos, struct ctl_list, list);
		snd_ctl_close(ctl_list->ctl);
		list_del(&ctl_list->list);
		uc_mgr_free_ctl(ctl_list);
	}
}

static int uc_mgr_ctl_add_dev(struct ctl_list *ctl_list, const char *device)
{
	struct list_head *pos;
	struct ctl_dev *ctl_dev;

	/* skip duplicates */
	list_for_each(pos, &ctl_list->dev_list) {
		ctl_dev = list_entry(pos, struct ctl_dev, list);
		if (strcmp(ctl_dev->device, device) == 0)
			return 0;
	}

	/* allocate new device name */
	ctl_dev = malloc(sizeof(*ctl_dev));
	if (ctl_dev == NULL)
		return -ENOMEM;
	ctl_dev->device = strdup(device);
	if (ctl_dev->device == NULL) {
		free(ctl_dev);
		return -ENOMEM;
	}
	list_add_tail(&ctl_dev->list, &ctl_list->dev_list);
	return 0;
}

static int uc_mgr_ctl_add(snd_use_case_mgr_t *uc_mgr,
			  struct ctl_list *ctl_list,
			  snd_ctl_t *ctl, int card,
			  snd_ctl_card_info_t *info,
			  const char *device)
{
	struct ctl_list *cl = NULL;
	const char *id = snd_ctl_card_info_get_id(info);
	char dev[MAX_CARD_LONG_NAME];
	int err, hit = 0;

	if (id == NULL || id[0] == '\0')
		return -ENOENT;
	if (!ctl_list) {
		cl = malloc(sizeof(*cl));
		if (cl == NULL)
			return -ENOMEM;
		INIT_LIST_HEAD(&cl->dev_list);
		cl->ctl = ctl;
		if (snd_ctl_card_info_malloc(&cl->ctl_info) < 0) {
			free(cl);
			return -ENOMEM;
		}
		snd_ctl_card_info_copy(cl->ctl_info, info);
		ctl_list = cl;
	}
	if (card >= 0) {
		snprintf(dev, sizeof(dev), "hw:%d", card);
		hit |= !!(device && (strcmp(dev, device) == 0));
		err = uc_mgr_ctl_add_dev(ctl_list, dev);
		if (err < 0)
			goto __nomem;
	}
	snprintf(dev, sizeof(dev), "hw:%s", id);
	hit |= !!(device && (strcmp(dev, device) == 0));
	err = uc_mgr_ctl_add_dev(ctl_list, dev);
	if (err < 0)
		goto __nomem;
	/* the UCM name not based on the card name / id */
	if (!hit && device) {
		err = uc_mgr_ctl_add_dev(ctl_list, device);
		if (err < 0)
			goto __nomem;
	}

	list_add_tail(&ctl_list->list, &uc_mgr->ctl_list);
	return 0;

__nomem:
	if (ctl_list == cl)
		uc_mgr_free_ctl(cl);
	return -ENOMEM;
}

int uc_mgr_open_ctl(snd_use_case_mgr_t *uc_mgr,
		    snd_ctl_t **ctl,
		    const char *device)
{
	struct list_head *pos1, *pos2;
	struct ctl_list *ctl_list;
	struct ctl_dev *ctl_dev;
	snd_ctl_card_info_t *info;
	const char *id;
	int err, card;

	snd_ctl_card_info_alloca(&info);

	/* cache lookup */
	list_for_each(pos1, &uc_mgr->ctl_list) {
		ctl_list = list_entry(pos1, struct ctl_list, list);
		list_for_each(pos2, &ctl_list->dev_list) {
			ctl_dev = list_entry(pos2, struct ctl_dev, list);
			if (strcmp(ctl_dev->device, device) == 0) {
				*ctl = ctl_list->ctl;
				return 0;
			}
		}
	}

	err = snd_ctl_open(ctl, device, 0);
	if (err < 0)
		return err;

	id = NULL;
	err = snd_ctl_card_info(*ctl, info);
	if (err == 0)
		id = snd_ctl_card_info_get_id(info);
	if (err < 0 || id == NULL || id[0] == '\0') {
		uc_error("control hardware info (%s): %s", device, snd_strerror(err));
		snd_ctl_close(*ctl);
		*ctl = NULL;
		return err;
	}

	/* insert to cache, if just name differs */
	list_for_each(pos1, &uc_mgr->ctl_list) {
		ctl_list = list_entry(pos1, struct ctl_list, list);
		if (strcmp(id, snd_ctl_card_info_get_id(ctl_list->ctl_info)) == 0) {
			card = snd_card_get_index(id);
			err = uc_mgr_ctl_add(uc_mgr, ctl_list, *ctl, card, info, device);
			if (err < 0)
				goto __nomem;
			snd_ctl_close(*ctl);
			*ctl = ctl_list->ctl;
			return 0;
		}
	}

	err = uc_mgr_ctl_add(uc_mgr, NULL, *ctl, -1, info, device);
	if (err < 0)
		goto __nomem;

	return 0;

__nomem:
	snd_ctl_close(*ctl);
	*ctl = NULL;
	return -ENOMEM;
}

int uc_mgr_config_load(int format, const char *file, snd_config_t **cfg)
{
	FILE *fp;
	snd_input_t *in;
	snd_config_t *top;
	const char *path, *default_paths[2];
	int err;

	fp = fopen(file, "r");
	if (!fp) {
		err = -errno;
  __err0:
		uc_error("could not open configuration file %s", file);
		return err;
	}
	err = snd_input_stdio_attach(&in, fp, 1);
	if (err < 0)
		goto __err0;
	err = snd_config_top(&top);
	if (err < 0)
		goto __err1;

	if (format >= 2) {
		path = getenv(ALSA_CONFIG_UCM2_VAR);
		if (!path || path[0] == '\0')
			path = ALSA_CONFIG_DIR "/ucm2";
	} else {
		path = getenv(ALSA_CONFIG_UCM_VAR);
		if (!path || path[0] == '\0')
			path = ALSA_CONFIG_DIR "/ucm";
	}

	default_paths[0] = path;
	default_paths[1] = NULL;
	err = _snd_config_load_with_include(top, in, 0, default_paths);
	if (err < 0) {
		uc_error("could not load configuration file %s", file);
		goto __err2;
	}
	err = snd_input_close(in);
	if (err < 0) {
		in = NULL;
		goto __err2;
	}
	*cfg = top;
	return 0;

 __err2:
	snd_config_delete(top);
 __err1:
	if (in)
		snd_input_close(in);
	return err;
}

void uc_mgr_free_value(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct ucm_value *val;
	
	list_for_each_safe(pos, npos, base) {
		val = list_entry(pos, struct ucm_value, list);
		free(val->name);
		free(val->data);
		list_del(&val->list);
		free(val);
	}
}

void uc_mgr_free_dev_list(struct dev_list *dev_list)
{
	struct list_head *pos, *npos;
	struct dev_list_node *dlist;
	
	list_for_each_safe(pos, npos, &dev_list->list) {
		dlist = list_entry(pos, struct dev_list_node, list);
		free(dlist->name);
		list_del(&dlist->list);
		free(dlist);
	}
}

int uc_mgr_put_to_dev_list(struct dev_list *dev_list, const char *name)
{
	struct list_head *pos;
	struct dev_list_node *dlist;
	char *n;

	list_for_each(pos, &dev_list->list) {
		dlist = list_entry(pos, struct dev_list_node, list);
		if (strcmp(dlist->name, name) == 0)
			return 0;
	}

	dlist = calloc(1, sizeof(*dlist));
	if (dlist == NULL)
		return -ENOMEM;
	n = strdup(name);
	if (n == NULL) {
		free(dlist);
		return -ENOMEM;
	}
	dlist->name = n;
	list_add(&dlist->list, &dev_list->list);
	return 0;
}

int uc_mgr_rename_in_dev_list(struct dev_list *dev_list, const char *src,
			      const char *dst)
{
	struct list_head *pos;
	struct dev_list_node *dlist;
	char *dst1;

	list_for_each(pos, &dev_list->list) {
		dlist = list_entry(pos, struct dev_list_node, list);
		if (strcmp(dlist->name, src) == 0) {
			dst1 = strdup(dst);
			if (dst1 == NULL)
				return -ENOMEM;
			free(dlist->name);
			dlist->name = dst1;
			return 0;
		}
	}
	return -ENOENT;
}

int uc_mgr_remove_from_dev_list(struct dev_list *dev_list, const char *name)
{
	struct list_head *pos;
	struct dev_list_node *dlist;

	list_for_each(pos, &dev_list->list) {
		dlist = list_entry(pos, struct dev_list_node, list);
		if (strcmp(dlist->name, name) == 0) {
			free(dlist->name);
			list_del(&dlist->list);
			free(dlist);
			return 0;
		}
	}
	return -ENODEV;
}

void uc_mgr_free_sequence_element(struct sequence_element *seq)
{
	if (seq == NULL)
		return;
	switch (seq->type) {
	case SEQUENCE_ELEMENT_TYPE_CDEV:
		free(seq->data.cdev);
		break;
	case SEQUENCE_ELEMENT_TYPE_CSET:
	case SEQUENCE_ELEMENT_TYPE_CSET_BIN_FILE:
	case SEQUENCE_ELEMENT_TYPE_CSET_TLV:
		free(seq->data.cset);
		break;
	case SEQUENCE_ELEMENT_TYPE_EXEC:
		free(seq->data.exec);
		break;
	default:
		break;
	}
	free(seq);
}

void uc_mgr_free_sequence(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct sequence_element *seq;
	
	list_for_each_safe(pos, npos, base) {
		seq = list_entry(pos, struct sequence_element, list);
		list_del(&seq->list);
		uc_mgr_free_sequence_element(seq);
	}
}

void uc_mgr_free_transition_element(struct transition_sequence *tseq)
{
	free(tseq->name);
	uc_mgr_free_sequence(&tseq->transition_list);
	free(tseq);
}

void uc_mgr_free_transition(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct transition_sequence *tseq;
	
	list_for_each_safe(pos, npos, base) {
		tseq = list_entry(pos, struct transition_sequence, list);
		list_del(&tseq->list);
		uc_mgr_free_transition_element(tseq);
	}
}

void uc_mgr_free_dev_name_list(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct ucm_dev_name *dev;

	list_for_each_safe(pos, npos, base) {
		dev = list_entry(pos, struct ucm_dev_name, list);
		list_del(&dev->list);
		free(dev->name1);
		free(dev->name2);
		free(dev);
	}
}

void uc_mgr_free_modifier(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct use_case_modifier *mod;
	
	list_for_each_safe(pos, npos, base) {
		mod = list_entry(pos, struct use_case_modifier, list);
		free(mod->name);
		free(mod->comment);
		uc_mgr_free_sequence(&mod->enable_list);
		uc_mgr_free_sequence(&mod->disable_list);
		uc_mgr_free_transition(&mod->transition_list);
		uc_mgr_free_dev_list(&mod->dev_list);
		uc_mgr_free_value(&mod->value_list);
		list_del(&mod->list);
		free(mod);
	}
}

void uc_mgr_free_device(struct use_case_device *dev)
{
	free(dev->name);
	free(dev->comment);
	uc_mgr_free_sequence(&dev->enable_list);
	uc_mgr_free_sequence(&dev->disable_list);
	uc_mgr_free_transition(&dev->transition_list);
	uc_mgr_free_dev_list(&dev->dev_list);
	uc_mgr_free_value(&dev->value_list);
	list_del(&dev->list);
	free(dev);
}

void uc_mgr_free_device_list(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct use_case_device *dev;
	
	list_for_each_safe(pos, npos, base) {
		dev = list_entry(pos, struct use_case_device, list);
		uc_mgr_free_device(dev);
	}
}

int uc_mgr_rename_device(struct use_case_verb *verb, const char *src,
			 const char *dst)
{
	struct use_case_device *device;
	struct list_head *pos, *npos;
	char *dst1;

	/* no errors when device is not found */
	list_for_each_safe(pos, npos, &verb->device_list) {
		device = list_entry(pos, struct use_case_device, list);
		if (strcmp(device->name, src) == 0) {
			dst1 = strdup(dst);
			if (dst1 == NULL)
				return -ENOMEM;
			free(device->name);
			device->name = dst1;
			continue;
		}
		uc_mgr_rename_in_dev_list(&device->dev_list, src, dst);
	}
	return 0;
}

int uc_mgr_remove_device(struct use_case_verb *verb, const char *name)
{
	struct use_case_device *device;
	struct list_head *pos, *npos;

	list_for_each_safe(pos, npos, &verb->device_list) {
		device = list_entry(pos, struct use_case_device, list);
		if (strcmp(device->name, name) == 0) {
			uc_mgr_free_device(device);
			continue;
		}
		uc_mgr_remove_from_dev_list(&device->dev_list, name);
		return 0;
	}
	return -ENOENT;
}

void uc_mgr_free_verb(snd_use_case_mgr_t *uc_mgr)
{
	struct list_head *pos, *npos;
	struct use_case_verb *verb;

	list_for_each_safe(pos, npos, &uc_mgr->verb_list) {
		verb = list_entry(pos, struct use_case_verb, list);
		free(verb->name);
		free(verb->comment);
		uc_mgr_free_sequence(&verb->enable_list);
		uc_mgr_free_sequence(&verb->disable_list);
		uc_mgr_free_transition(&verb->transition_list);
		uc_mgr_free_value(&verb->value_list);
		uc_mgr_free_device_list(&verb->device_list);
		uc_mgr_free_device_list(&verb->cmpt_device_list);
		uc_mgr_free_modifier(&verb->modifier_list);
		uc_mgr_free_dev_name_list(&verb->rename_list);
		uc_mgr_free_dev_name_list(&verb->remove_list);
		list_del(&verb->list);
		free(verb);
	}
	uc_mgr_free_sequence(&uc_mgr->default_list);
	uc_mgr_free_value(&uc_mgr->value_list);
	free(uc_mgr->comment);
	free(uc_mgr->conf_dir_name);
	free(uc_mgr->conf_file_name);
	uc_mgr->comment = NULL;
	uc_mgr->conf_dir_name = NULL;
	uc_mgr->conf_file_name = NULL;
	uc_mgr->active_verb = NULL;
	INIT_LIST_HEAD(&uc_mgr->active_devices);
	INIT_LIST_HEAD(&uc_mgr->active_modifiers);
}

void uc_mgr_free(snd_use_case_mgr_t *uc_mgr)
{
	uc_mgr_free_verb(uc_mgr);
	uc_mgr_free_ctl_list(uc_mgr);
	free(uc_mgr->card_name);
	free(uc_mgr);
}
