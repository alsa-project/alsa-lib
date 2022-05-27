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

const char *uc_mgr_sysfs_root(void)
{
	const char *e = getenv("SYSFS_PATH");
	if (e == NULL)
		return "/sys";
	if (*e == '\0')
		uc_error("no sysfs root!");
	return e;
}

struct ctl_list *uc_mgr_get_master_ctl(snd_use_case_mgr_t *uc_mgr)
{
	struct list_head *pos;
	struct ctl_list *ctl_list = NULL, *ctl_list2;

	list_for_each(pos, &uc_mgr->ctl_list) {
		ctl_list2 = list_entry(pos, struct ctl_list, list);
		if (ctl_list2->slave)
			continue;
		if (ctl_list) {
			uc_error("multiple control device names were found!");
			return NULL;
		}
		ctl_list = ctl_list2;
	}
	return ctl_list;
}

struct ctl_list *uc_mgr_get_ctl_by_card(snd_use_case_mgr_t *uc_mgr, int card)
{
	struct ctl_list *ctl_list;
	char cname[32];
	int err;

	sprintf(cname, "hw:%d", card);
	err = uc_mgr_open_ctl(uc_mgr, &ctl_list, cname, 1);
	if (err < 0)
		return NULL;
	return ctl_list;
}

struct ctl_list *uc_mgr_get_ctl_by_name(snd_use_case_mgr_t *uc_mgr, const char *name, int idx)
{
	struct list_head *pos;
	struct ctl_list *ctl_list;
	const char *s;
	int idx2, card;

	idx2 = idx;
	list_for_each(pos, &uc_mgr->ctl_list) {
		ctl_list = list_entry(pos, struct ctl_list, list);
		s = snd_ctl_card_info_get_name(ctl_list->ctl_info);
		if (s == NULL)
			continue;
		if (strcmp(s, name) == 0) {
			if (idx2 == 0)
				return ctl_list;
			idx2--;
		}
	}

	idx2 = idx;
	card = -1;
	if (snd_card_next(&card) < 0 || card < 0)
		return NULL;

	while (card >= 0) {
		ctl_list = uc_mgr_get_ctl_by_card(uc_mgr, card);
		if (ctl_list == NULL)
			continue;	/* really? */
		s = snd_ctl_card_info_get_name(ctl_list->ctl_info);
		if (s && strcmp(s, name) == 0) {
			if (idx2 == 0)
				return ctl_list;
			idx2--;
		}
		if (snd_card_next(&card) < 0)
			break;
	}

	return NULL;
}

snd_ctl_t *uc_mgr_get_ctl(snd_use_case_mgr_t *uc_mgr)
{
	struct ctl_list *ctl_list;

	ctl_list = uc_mgr_get_master_ctl(uc_mgr);
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
			  struct ctl_list **ctl_list,
			  snd_ctl_t *ctl, int card,
			  snd_ctl_card_info_t *info,
			  const char *device,
			  int slave)
{
	struct ctl_list *cl = NULL;
	const char *id = snd_ctl_card_info_get_id(info);
	char dev[MAX_CARD_LONG_NAME];
	int err, hit = 0;

	if (id == NULL || id[0] == '\0')
		return -ENOENT;
	if (!(*ctl_list)) {
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
		cl->slave = slave;
		*ctl_list = cl;
	} else {
		if (!slave)
			(*ctl_list)->slave = slave;
	}
	if (card >= 0) {
		snprintf(dev, sizeof(dev), "hw:%d", card);
		hit |= !!(device && (strcmp(dev, device) == 0));
		err = uc_mgr_ctl_add_dev(*ctl_list, dev);
		if (err < 0)
			goto __nomem;
	}
	snprintf(dev, sizeof(dev), "hw:%s", id);
	hit |= !!(device && (strcmp(dev, device) == 0));
	err = uc_mgr_ctl_add_dev(*ctl_list, dev);
	if (err < 0)
		goto __nomem;
	/* the UCM name not based on the card name / id */
	if (!hit && device) {
		err = uc_mgr_ctl_add_dev(*ctl_list, device);
		if (err < 0)
			goto __nomem;
	}

	list_add_tail(&(*ctl_list)->list, &uc_mgr->ctl_list);
	return 0;

__nomem:
	if (*ctl_list == cl) {
		uc_mgr_free_ctl(cl);
		*ctl_list = NULL;
	}
	return -ENOMEM;
}

int uc_mgr_open_ctl(snd_use_case_mgr_t *uc_mgr,
		    struct ctl_list **ctll,
		    const char *device,
		    int slave)
{
	struct list_head *pos1, *pos2;
	snd_ctl_t *ctl;
	struct ctl_list *ctl_list;
	struct ctl_dev *ctl_dev;
	snd_ctl_card_info_t *info;
	const char *id;
	int err, card, ucm_group, ucm_offset;

	snd_ctl_card_info_alloca(&info);

	ucm_group = _snd_is_ucm_device(device);
	ucm_offset = ucm_group ? 8 : 0;

	/* cache lookup */
	list_for_each(pos1, &uc_mgr->ctl_list) {
		ctl_list = list_entry(pos1, struct ctl_list, list);
		if (ctl_list->ucm_group != ucm_group)
			continue;
		list_for_each(pos2, &ctl_list->dev_list) {
			ctl_dev = list_entry(pos2, struct ctl_dev, list);
			if (strcmp(ctl_dev->device, device + ucm_offset) == 0) {
				*ctll = ctl_list;
				if (!slave)
					ctl_list->slave = 0;
				return 0;
			}
		}
	}

	err = snd_ctl_open(&ctl, device, 0);
	if (err < 0)
		return err;

	id = NULL;
	err = snd_ctl_card_info(ctl, info);
	if (err == 0)
		id = snd_ctl_card_info_get_id(info);
	if (err < 0 || id == NULL || id[0] == '\0') {
		uc_error("control hardware info (%s): %s", device, snd_strerror(err));
		snd_ctl_close(ctl);
		return err >= 0 ? -EINVAL : err;
	}

	/* insert to cache, if just name differs */
	list_for_each(pos1, &uc_mgr->ctl_list) {
		ctl_list = list_entry(pos1, struct ctl_list, list);
		if (ctl_list->ucm_group != ucm_group)
			continue;
		if (strcmp(id, snd_ctl_card_info_get_id(ctl_list->ctl_info)) == 0) {
			card = snd_card_get_index(id);
			err = uc_mgr_ctl_add(uc_mgr, &ctl_list, ctl, card, info, device + ucm_offset, slave);
			if (err < 0)
				goto __nomem;
			snd_ctl_close(ctl);
			ctl_list->ucm_group = ucm_group;
			*ctll = ctl_list;
			return 0;
		}
	}

	ctl_list = NULL;
	err = uc_mgr_ctl_add(uc_mgr, &ctl_list, ctl, -1, info, device + ucm_offset, slave);
	if (err < 0)
		goto __nomem;

	ctl_list->ucm_group = ucm_group;
	*ctll = ctl_list;
	return 0;

__nomem:
	snd_ctl_close(ctl);
	return -ENOMEM;
}

const char *uc_mgr_config_dir(int format)
{
	const char *path;

	if (format >= 2) {
		path = getenv(ALSA_CONFIG_UCM2_VAR);
		if (!path || path[0] == '\0')
			path = ALSA_CONFIG_DIR "/ucm2";
	} else {
		path = getenv(ALSA_CONFIG_UCM_VAR);
		if (!path || path[0] == '\0')
			path = ALSA_CONFIG_DIR "/ucm";
	}
	return path;
}

int uc_mgr_config_load_into(int format, const char *file, snd_config_t *top)
{
	FILE *fp;
	snd_input_t *in;
	const char *default_paths[2];
	int err;

	fp = fopen(file, "r");
	if (!fp) {
		err = -errno;
  __err_open:
		uc_error("could not open configuration file %s", file);
		return err;
	}
	err = snd_input_stdio_attach(&in, fp, 1);
	if (err < 0)
		goto __err_open;

	default_paths[0] = uc_mgr_config_dir(format);
	default_paths[1] = NULL;
	err = _snd_config_load_with_include(top, in, 0, default_paths);
	if (err < 0) {
		uc_error("could not load configuration file %s", file);
		if (in)
			snd_input_close(in);
		return err;
	}
	err = snd_input_close(in);
	if (err < 0)
		return err;
	return 0;
}

int uc_mgr_config_load(int format, const char *file, snd_config_t **cfg)
{
	snd_config_t *top;
	int err;

	err = snd_config_top(&top);
	if (err < 0)
		return err;
	err = uc_mgr_config_load_into(format, file, top);
	if (err < 0) {
		snd_config_delete(top);
		return err;
	}
	*cfg = top;
	return 0;
}

static void uc_mgr_free_value1(struct ucm_value *val)
{
	free(val->name);
	free(val->data);
	list_del(&val->list);
	free(val);
}

void uc_mgr_free_value(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct ucm_value *val;
	
	list_for_each_safe(pos, npos, base) {
		val = list_entry(pos, struct ucm_value, list);
		uc_mgr_free_value1(val);
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
	return -ENODEV;
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
	case SEQUENCE_ELEMENT_TYPE_CSET_NEW:
	case SEQUENCE_ELEMENT_TYPE_CSET_BIN_FILE:
	case SEQUENCE_ELEMENT_TYPE_CSET_TLV:
	case SEQUENCE_ELEMENT_TYPE_CTL_REMOVE:
		free(seq->data.cset);
		break;
	case SEQUENCE_ELEMENT_TYPE_SYSSET:
		free(seq->data.sysw);
		break;
	case SEQUENCE_ELEMENT_TYPE_EXEC:
	case SEQUENCE_ELEMENT_TYPE_SHELL:
		free(seq->data.exec);
		break;
	case SEQUENCE_ELEMENT_TYPE_CFGSAVE:
		free(seq->data.cfgsave);
		break;
	case SEQUENCE_ELEMENT_TYPE_DEV_ENABLE_SEQ:
	case SEQUENCE_ELEMENT_TYPE_DEV_DISABLE_SEQ:
		free(seq->data.device);
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
	int err, found = 0;

	list_for_each_safe(pos, npos, &verb->device_list) {
		device = list_entry(pos, struct use_case_device, list);
		if (strcmp(device->name, name) == 0) {
			uc_mgr_free_device(device);
			found++;
			continue;
		}
		err = uc_mgr_remove_from_dev_list(&device->dev_list, name);
		if (err < 0 && err != -ENODEV)
			return err;
		if (err == 0)
			found++;
	}
	return found == 0 ? -ENODEV : 0;
}

const char *uc_mgr_get_variable(snd_use_case_mgr_t *uc_mgr, const char *name)
{
	struct list_head *pos;
	struct ucm_value *value;

	list_for_each(pos, &uc_mgr->variable_list) {
		value = list_entry(pos, struct ucm_value, list);
		if (strcmp(value->name, name) == 0)
			return value->data;
	}
	return NULL;
}

int uc_mgr_set_variable(snd_use_case_mgr_t *uc_mgr, const char *name,
			const char *val)
{
	struct list_head *pos;
	struct ucm_value *curr;
	char *val2;

	list_for_each(pos, &uc_mgr->variable_list) {
		curr = list_entry(pos, struct ucm_value, list);
		if (strcmp(curr->name, name) == 0) {
			val2 = strdup(val);
			if (val2 == NULL)
				return -ENOMEM;
			free(curr->data);
			curr->data = val2;
			return 0;
		}
	}

	curr = calloc(1, sizeof(struct ucm_value));
	if (curr == NULL)
		return -ENOMEM;
	curr->name = strdup(name);
	if (curr->name == NULL) {
		free(curr);
		return -ENOMEM;
	}
	curr->data = strdup(val);
	if (curr->data == NULL) {
		free(curr->name);
		free(curr);
		return -ENOMEM;
	}
	list_add_tail(&curr->list, &uc_mgr->variable_list);
	return 0;
}

int uc_mgr_delete_variable(snd_use_case_mgr_t *uc_mgr, const char *name)
{
	struct list_head *pos;
	struct ucm_value *curr;

	list_for_each(pos, &uc_mgr->variable_list) {
		curr = list_entry(pos, struct ucm_value, list);
		if (strcmp(curr->name, name) == 0) {
			uc_mgr_free_value1(curr);
			return 0;
		}
	}

	return -ENOENT;
}

void uc_mgr_free_verb(snd_use_case_mgr_t *uc_mgr)
{
	struct list_head *pos, *npos;
	struct use_case_verb *verb;

	if (uc_mgr->local_config) {
		snd_config_delete(uc_mgr->local_config);
		uc_mgr->local_config = NULL;
	}
	if (uc_mgr->macros) {
		snd_config_delete(uc_mgr->macros);
		uc_mgr->macros = NULL;
	}
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
	uc_mgr_free_sequence(&uc_mgr->fixedboot_list);
	uc_mgr_free_sequence(&uc_mgr->boot_list);
	uc_mgr_free_sequence(&uc_mgr->default_list);
	uc_mgr_free_value(&uc_mgr->value_list);
	uc_mgr_free_value(&uc_mgr->variable_list);
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

/*
 * UCM card list stuff
 */

static pthread_mutex_t ucm_cards_mutex = PTHREAD_MUTEX_INITIALIZER;
static LIST_HEAD(ucm_cards);
static unsigned int ucm_card_assign;

static snd_use_case_mgr_t *uc_mgr_card_find(unsigned int card_number)
{
	struct list_head *pos;
	snd_use_case_mgr_t *uc_mgr;

	list_for_each(pos, &ucm_cards) {
		uc_mgr = list_entry(pos, snd_use_case_mgr_t, cards_list);
		if (uc_mgr->ucm_card_number == card_number)
			return uc_mgr;
	}
	return NULL;
}

int uc_mgr_card_open(snd_use_case_mgr_t *uc_mgr)
{
	unsigned int prev;

	pthread_mutex_lock(&ucm_cards_mutex);
	prev = ucm_card_assign++;
	while (uc_mgr_card_find(ucm_card_assign)) {
		ucm_card_assign++;
		ucm_card_assign &= 0xffff;
		/* avoid zero card instance number */
		if (ucm_card_assign == 0)
			ucm_card_assign++;
		if (ucm_card_assign == prev) {
			pthread_mutex_unlock(&ucm_cards_mutex);
			return -ENOMEM;
		}
	}
	uc_mgr->ucm_card_number = ucm_card_assign;
	list_add(&uc_mgr->cards_list, &ucm_cards);
	pthread_mutex_unlock(&ucm_cards_mutex);
	return 0;
}

void uc_mgr_card_close(snd_use_case_mgr_t *uc_mgr)
{
	pthread_mutex_lock(&ucm_cards_mutex);
	list_del(&uc_mgr->cards_list);
	pthread_mutex_unlock(&ucm_cards_mutex);
}

/**
 * \brief Get library configuration based on the private ALSA device name
 * \param name[in] ALSA device name
 * \retval config A configuration tree or NULL
 *
 * The returned configuration (non-NULL) should be unreferenced using
 * snd_config_unref() call.
 */
const char *uc_mgr_alibcfg_by_device(snd_config_t **top, const char *name)
{
	char buf[5];
	long card_num;
	snd_config_t *config;
	snd_use_case_mgr_t *uc_mgr;
	int err;

	if (strncmp(name, "_ucm", 4) || strlen(name) < 12 || name[8] != '.')
		return NULL;
	strncpy(buf, name + 4, 4);
	buf[4] = '\0';
	err = safe_strtol_base(buf, &card_num, 16);
	if (err < 0 || card_num < 0 || card_num > 0xffff)
		return NULL;
	config = NULL;
	pthread_mutex_lock(&ucm_cards_mutex);
	uc_mgr = uc_mgr_card_find(card_num);
	/* non-empty configs are accepted only */
	if (uc_mgr_has_local_config(uc_mgr)) {
		config = uc_mgr->local_config;
		snd_config_ref(config);
	}
	pthread_mutex_unlock(&ucm_cards_mutex);
	if (!config)
		return NULL;
	*top = config;
	return name + 9;
}
