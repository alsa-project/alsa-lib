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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <pthread.h>

/**
 * \brief Execute the sequence
 * \param uc_mgr Use case manager
 * \param seq Sequence
 * \return zero on success, otherwise a negative error code
 */
static int execute_sequence(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
			    struct list_head *seq)
{
	struct list_head *pos;
	struct sequence_element *s;

	list_for_each(pos, seq) {
		s = list_entry(pos, struct sequence_element, list);
		switch (s->type) {
		case SEQUENCE_ELEMENT_TYPE_CSET:
			uc_error("cset not yet implemented: '%s'", s->data.cset);
			break;
		case SEQUENCE_ELEMENT_TYPE_SLEEP:
			usleep(s->data.sleep);
			break;
		case SEQUENCE_ELEMENT_TYPE_EXEC:
			uc_error("exec not yet implemented: '%s'", s->data.exec);
			break;
		default:
			uc_error("unknown sequence command %i", s->type);
			break;
		}
	}
	return 0;
}

/**
 * \brief Import master config and execute the default sequence
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
static int import_master_config(snd_use_case_mgr_t *uc_mgr)
{
	int err;
	
	err = uc_mgr_import_master_config(uc_mgr);
	if (err < 0)
		return err;
	err = execute_sequence(uc_mgr, &uc_mgr->default_list);
	if (err < 0)
		uc_error("Unable to execute default sequence");
	return err;
}

/**
 * \brief Universal find - string in a list
 * \param uc_mgr Use case manager
 * \param list List of structures
 * \param offset Offset of list structure
 * \param soffset Offset of string structure
 * \param match String to match
 * \return structure on success, otherwise a NULL (not found)
 */
static void *find0(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
		   struct list_head *list,
		   unsigned long offset,
		   unsigned long soffset,
		   const char *match)
{
	struct list_head *pos;
	char *ptr, *str;

	list_for_each(pos, list) {
		ptr = list_entry_offset(pos, char, offset);
		str = *((char **)(ptr + soffset));
		if (strcmp(str, match) == 0)
			return ptr;
	}
	return NULL;
}

#define find(uc_mgr, list, type, member, value, match) \
	find0(uc_mgr, list, \
		     (unsigned long)(&((type *)0)->member), \
		     (unsigned long)(&((type *)0)->value), match)

/**
 * \brief Find verb
 * \param uc_mgr Use case manager
 * \param verb_name verb to find
 * \return structure on success, otherwise a NULL (not found)
 */
static inline struct use_case_verb *find_verb(snd_use_case_mgr_t *uc_mgr,
					      const char *_name)
{
	return find(uc_mgr, &uc_mgr->verb_list,
		    struct use_case_verb, list, name,
		    _name);
}

/**
 * \brief Set verb
 * \param uc_mgr Use case manager
 * \param verb verb to set
 * \return zero on success, otherwise a negative error code
 */
static int set_verb(snd_use_case_mgr_t *uc_mgr,
		    struct use_case_verb *verb,
		    int enable)
{
	struct list_head *seq;
	int err;

	if (enable) {
		seq = &verb->enable_list;
	} else {
		seq = &verb->disable_list;
	}
	err = execute_sequence(uc_mgr, seq);
	if (enable && err >= 0)
		uc_mgr->active_verb = verb;
	return err;
}

/**
 * \brief Init sound card use case manager.
 * \param uc_mgr Returned use case manager pointer
 * \param card_name name of card to open
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_open(snd_use_case_mgr_t **mgr,
			  const char *card_name)
{
	snd_use_case_mgr_t *uc_mgr;
	int err;

	/* create a new UCM */
	uc_mgr = calloc(1, sizeof(snd_use_case_mgr_t));
	if (uc_mgr == NULL)
		return -ENOMEM;
	INIT_LIST_HEAD(&uc_mgr->verb_list);
	INIT_LIST_HEAD(&uc_mgr->default_list);
	pthread_mutex_init(&uc_mgr->mutex, NULL);

	uc_mgr->card_name = strdup(card_name);
	if (uc_mgr->card_name == NULL) {
		free(uc_mgr);
		return -ENOMEM;
	}

	/* get info on use_cases and verify against card */
	err = import_master_config(uc_mgr);
	if (err < 0) {
		uc_error("error: failed to import %s use case configuration %d",
			card_name, err);
		goto err;
	}

	*mgr = uc_mgr;
	return 0;

err:
	uc_mgr_free(uc_mgr);
	return err;
}

/**
 * \brief Reload and reparse all use case files.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_reload(snd_use_case_mgr_t *uc_mgr)
{
	int err;

	pthread_mutex_lock(&uc_mgr->mutex);

	uc_mgr_free_verb(uc_mgr);

	/* reload all use cases */
	err = import_master_config(uc_mgr);
	if (err < 0) {
		uc_error("error: failed to reload use cases\n");
		pthread_mutex_unlock(&uc_mgr->mutex);
		return -EINVAL;
	}

	pthread_mutex_unlock(&uc_mgr->mutex);
	return err;
}

/**
 * \brief Close use case manager.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_close(snd_use_case_mgr_t *uc_mgr)
{
	uc_mgr_free(uc_mgr);

	return 0;
}

/**
 * \brief Reset sound card controls to default values.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_reset(snd_use_case_mgr_t *uc_mgr)
{
	struct list_head *pos, *npos;
	struct use_case_modifier *modifier;
	struct use_case_device *device;
	int err;

	pthread_mutex_lock(&uc_mgr->mutex);

	list_for_each_safe(pos, npos, &uc_mgr->active_modifiers) {
		modifier = list_entry(pos, struct use_case_modifier,
				      active_list);
		err = disable_modifier(uc_mgr, modifier);
		if (err < 0)
			uc_error("Unable to disable modifier %s", modifier->name);
	}
	INIT_LIST_HEAD(&uc_mgr->active_modifiers);

	list_for_each_safe(pos, npos, &uc_mgr->active_devices) {
		device = list_entry(pos, struct use_case_device,
				    active_list);
		err = disable_device(uc_mgr, device);
		if (err < 0)
			uc_error("Unable to disable device %s", device->name);
	}
	INIT_LIST_HEAD(&uc_mgr->active_devices);

	err = disable_verb(uc_mgr, uc_mgr->active_verb);
	if (err < 0) {
		uc_error("Unable to disable verb %s", uc_mgr->active_verb->name);
		return err;
	}
	uc_mgr->active_verb = NULL;

	err = execute_sequence(uc_mgr, &uc_mgr->default_list);
	
	pthread_mutex_unlock(&uc_mgr->mutex);
	return err;
}

#if 0
static int enable_use_case_verb(snd_use_case_mgr_t *uc_mgr, int verb_id,
		snd_ctl_elem_list_t *list, snd_ctl_t *handle)
{
	struct use_case_verb *verb;
	int ret;

	if (verb_id >= uc_mgr->num_verbs) {
		uc_error("error: invalid verb id %d", verb_id);
		return -EINVAL;
	}
	verb = &uc_mgr->verb[verb_id];

	uc_dbg("verb %s", verb->name);
	ret = exec_sequence(verb->enable, uc_mgr, list, handle);
	if (ret < 0) {
		uc_error("error: could not enable verb %s", verb->name);
		return ret;
	}
	uc_mgr->card.current_verb = verb_id;

	return 0;
}

static int disable_use_case_verb(snd_use_case_mgr_t *uc_mgr, int verb_id,
		snd_ctl_elem_list_t *list, snd_ctl_t *handle)
{
	struct use_case_verb *verb;
	int ret;

	if (verb_id >= uc_mgr->num_verbs) {
		uc_error("error: invalid verb id %d", verb_id);
		return -EINVAL;
	}
	verb = &uc_mgr->verb[verb_id];

	/* we set the invalid verb at open() but we should still
	 * check that this succeeded */
	if (verb == NULL)
		return 0;

	uc_dbg("verb %s", verb->name);
	ret = exec_sequence(verb->disable, uc_mgr, list, handle);
	if (ret < 0) {
		uc_error("error: could not disable verb %s", verb->name);
		return ret;
	}

	return 0;
}

static int enable_use_case_device(snd_use_case_mgr_t *uc_mgr,
		int device_id, snd_ctl_elem_list_t *list, snd_ctl_t *handle)
{
	struct use_case_verb *verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	struct use_case_device *device = &verb->device[device_id];
	int ret;

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED)
		return -EINVAL;

	uc_dbg("device %s", device->name);
	ret = exec_sequence(device->enable, uc_mgr, list, handle);
	if (ret < 0) {
		uc_error("error: could not enable device %s", device->name);
		return ret;
	}

	set_device_status(uc_mgr, device_id, 1);
	return 0;
}

static int disable_use_case_device(snd_use_case_mgr_t *uc_mgr,
		int device_id, snd_ctl_elem_list_t *list, snd_ctl_t *handle)
{
	struct use_case_verb *verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	struct use_case_device *device = &verb->device[device_id];
	int ret;

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED)
		return -EINVAL;

	uc_dbg("device %s", device->name);
	ret = exec_sequence(device->disable, uc_mgr, list, handle);
	if (ret < 0) {
		uc_error("error: could not disable device %s", device->name);
		return ret;
	}

	set_device_status(uc_mgr, device_id, 0);
	return 0;
}

static int enable_use_case_modifier(snd_use_case_mgr_t *uc_mgr,
		int modifier_id, snd_ctl_elem_list_t *list, snd_ctl_t *handle)
{
	struct use_case_verb *verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	struct use_case_modifier *modifier = &verb->modifier[modifier_id];
	int ret;

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED)
		return -EINVAL;

	uc_dbg("modifier %s", modifier->name);
	ret = exec_sequence(modifier->enable, uc_mgr, list, handle);
	if (ret < 0) {
		uc_error("error: could not enable modifier %s", modifier->name);
		return ret;
	}

	set_modifier_status(uc_mgr, modifier_id, 1);
	return 0;
}

static int disable_use_case_modifier(snd_use_case_mgr_t *uc_mgr,
		int modifier_id, snd_ctl_elem_list_t *list, snd_ctl_t *handle)
{
	struct use_case_verb *verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	struct use_case_modifier *modifier = &verb->modifier[modifier_id];
	int ret;

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED)
		return -EINVAL;

	uc_dbg("modifier %s", modifier->name);
	ret = exec_sequence(modifier->disable, uc_mgr, list, handle);
	if (ret < 0) {
		uc_error("error: could not disable modifier %s", modifier->name);
		return ret;
	}

	set_modifier_status(uc_mgr, modifier_id, 0);
	return 0;
}

/*
 * Tear down current use case verb, device and modifier.
 */
static int dismantle_use_case(snd_use_case_mgr_t *uc_mgr,
		snd_ctl_elem_list_t *list, snd_ctl_t *handle)
{
	struct use_case_verb *verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	int ret, i;

	/* No active verb */
	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED)
		return 0;

	/* disable all modifiers that are active */
	for (i = 0; i < verb->num_modifiers; i++) {
		if (get_modifier_status(uc_mgr,i)) {
			ret = disable_use_case_modifier(uc_mgr, i, list, handle);
			if (ret < 0)
				return ret;
		}
	}

	/* disable all devices that are active */
	for (i = 0; i < verb->num_devices; i++) {
		if (get_device_status(uc_mgr,i)) {
			ret = disable_use_case_device(uc_mgr, i, list, handle);
			if (ret < 0)
				return ret;
		}
	}

	/* disable verb */
	ret = disable_use_case_verb(uc_mgr, uc_mgr->card.current_verb, list, handle);
	if (ret < 0)
		return ret;

	return 0;
}

 /**
 * \brief Dump sound card controls in format required for sequencer.
 * \param card_name The name of the sound card to be dumped
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_dump(const char *card_name)
{
	snd_ctl_t *handle;
	snd_ctl_card_info_t *info;
	snd_ctl_elem_list_t *list;
	int ret, i, count, idx;
	char ctl_name[8];

	snd_ctl_card_info_alloca(&info);
	snd_ctl_elem_list_alloca(&list);

	idx = snd_card_get_index(card_name);
	if (idx < 0)
		return idx;
	sprintf(ctl_name, "hw:%d", idx);

	/* open and load snd card */
	ret = snd_ctl_open(&handle, ctl_name, SND_CTL_READONLY);
	if (ret < 0) {
		uc_error("error: could not open controls for  %s: %s",
			card_name, snd_strerror(ret));
		return ret;
	}

	ret = snd_ctl_card_info(handle, info);
	if (ret < 0) {
		uc_error("error: could not get control info for %s:%s",
			card_name, snd_strerror(ret));
		goto close;
	}

	ret = snd_ctl_elem_list(handle, list);
	if (ret < 0) {
		uc_error("error: cannot determine controls for  %s: %s",
			card_name, snd_strerror(ret));
		goto close;
	}

	count = snd_ctl_elem_list_get_count(list);
	if (count < 0) {
		ret = 0;
		goto close;
	}

	snd_ctl_elem_list_set_offset(list, 0);
	if (snd_ctl_elem_list_alloc_space(list, count) < 0) {
		uc_error("error: not enough memory for control elements");
		ret =  -ENOMEM;
		goto close;
	}
	if ((ret = snd_ctl_elem_list(handle, list)) < 0) {
		uc_error("error: cannot determine controls: %s",
			snd_strerror(ret));
		goto free;
	}

	/* iterate through each kcontrol and add to use
	 * case manager control list */
	for (i = 0; i < count; ++i) {
		snd_ctl_elem_id_t *id;
		snd_ctl_elem_id_alloca(&id);
		snd_ctl_elem_list_get_id(list, i, id);

		/* dump to stdout in friendly format */
		ret = dump_control(handle, id);
		if (ret < 0) {
			uc_error("error: control dump failed: %s",
				snd_strerror(ret));
			goto free;
		}
	}
free:
	snd_ctl_elem_list_free_space(list);
close:
	snd_ctl_close(handle);
	return ret;
}

/**
 * \brief List supported use case verbs for given soundcard
 * \param uc_mgr use case manager
 * \param verb returned list of supported use case verb id and names
 * \return number of use case verbs if success, otherwise a negative error code
 */
int snd_use_case_get_verb_list(snd_use_case_mgr_t *uc_mgr,
		const char **verb[])
{
	int ret;

	pthread_mutex_lock(&uc_mgr->mutex);

	*verb = uc_mgr->verb_list;
	ret = uc_mgr->num_verbs;

	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}

/**
 * \brief List supported use case devices for given verb
 * \param uc_mgr use case manager
 * \param verb verb id.
 * \param device returned list of supported use case device id and names
 * \return number of use case devices if success, otherwise a negative error code
 */
int snd_use_case_get_device_list(snd_use_case_mgr_t *uc_mgr,
		const char *verb_name, const char **device[])
{
	struct use_case_verb *verb = NULL;
	int i, ret = -EINVAL;

	pthread_mutex_lock(&uc_mgr->mutex);

	/* find verb name */
	for (i = 0; i < uc_mgr->num_verbs; i++) {
		verb = &uc_mgr->verb[i];
		if (!strcmp(uc_mgr->verb[i].name, verb_name))
			goto found;
	}

	uc_error("error: use case verb %s not found", verb_name);
	goto out;

found:
	*device = verb->device_list;
	ret = verb->num_devices;
out:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}

/**
 * \brief List supported use case verb modifiers for given verb
 * \param uc_mgr use case manager
 * \param verb verb id.
 * \param mod returned list of supported use case modifier id and names
 * \return number of use case modifiers if success, otherwise a negative error code
 */
int snd_use_case_get_mod_list(snd_use_case_mgr_t *uc_mgr,
		const char *verb_name, const char **mod[])
{
	struct use_case_verb *verb = NULL;
	int i, ret = -EINVAL;

	pthread_mutex_lock(&uc_mgr->mutex);

	/* find verb name */
	for (i = 0; i <uc_mgr->num_verbs; i++) {
		verb = &uc_mgr->verb[i];
		if (!strcmp(uc_mgr->verb[i].name, verb_name))
			goto found;
	}

	uc_error("error: use case verb %s not found", verb_name);
	goto out;

found:
	*mod = verb->modifier_list;
	ret = verb->num_modifiers;
out:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}

static struct sequence_element *get_transition_sequence(
		struct transition_sequence *trans_list, const char *name)
{
	struct transition_sequence *trans = trans_list;

	while (trans) {
		if (trans->name && !strcmp(trans->name, name))
			return trans->transition;

		trans =  trans->next;
	}

	return NULL;
}

static int exec_transition_sequence(snd_use_case_mgr_t *uc_mgr,
			struct sequence_element *trans_sequence)
{
	int ret;

	ret = exec_sequence(trans_sequence, uc_mgr, uc_mgr->list,
			uc_mgr->handle);
	if (ret < 0)
		uc_error("error: could not exec transition sequence");

	return ret;
}

static int handle_transition_verb(snd_use_case_mgr_t *uc_mgr,
		int new_verb_id)
{
	struct use_case_verb *old_verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	struct use_case_verb *new_verb;
	static struct sequence_element *trans_sequence;

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED)
		return -EINVAL;

	if (new_verb_id >= uc_mgr->num_verbs) {
		uc_error("error: invalid new_verb id %d", new_verb_id);
		return -EINVAL;
	}

	new_verb = &uc_mgr->verb[new_verb_id];

	uc_dbg("new verb %s", new_verb->name);

	trans_sequence = get_transition_sequence(old_verb->transition_list,
							new_verb->name);
	if (trans_sequence != NULL) {
		int ret, i;

		uc_dbg("find transition sequence %s->%s",
				old_verb->name, new_verb->name);

		/* disable all modifiers that are active */
		for (i = 0; i < old_verb->num_modifiers; i++) {
			if (get_modifier_status(uc_mgr,i)) {
				ret = disable_use_case_modifier(uc_mgr, i,
					uc_mgr->list, uc_mgr->handle);
				if (ret < 0)
					return ret;
			}
		}

		/* disable all devices that are active */
		for (i = 0; i < old_verb->num_devices; i++) {
			if (get_device_status(uc_mgr,i)) {
				ret = disable_use_case_device(uc_mgr, i,
					uc_mgr->list, uc_mgr->handle);
				if (ret < 0)
					return ret;
			}
		}

		ret = exec_transition_sequence(uc_mgr, trans_sequence);
		if (ret)
			return ret;

		uc_mgr->card.current_verb = new_verb_id;

		return 0;
	}

	return-EINVAL;
}

/**
 * \brief Set new use case verb for sound card
 * \param uc_mgr use case manager
 * \param verb verb id
 * \return zero if success, otherwise a negative error code
 */
int snd_use_case_set_verb(snd_use_case_mgr_t *uc_mgr,
		const char *verb_name)
{
	int i = 0, ret = -EINVAL, inactive = 0;

	pthread_mutex_lock(&uc_mgr->mutex);

	uc_dbg("uc_mgr %p, verb_name %s", uc_mgr, verb_name);

	/* check for "Inactive" */
	if (!strcmp(verb_name, SND_USE_CASE_VERB_INACTIVE)) {
		inactive = 1;
		goto found;
	}

	/* find verb name */
	for (i = 0; i <uc_mgr->num_verbs; i++) {
		if (!strcmp(uc_mgr->verb[i].name, verb_name))
			goto found;
	}

	uc_error("error: use case verb %s not found", verb_name);
	goto out;
found:
	/* use case verb found - check that we actually changing the verb */
	if (i == uc_mgr->card.current_verb) {
		uc_dbg("current verb ID %d", i);
		ret = 0;
		goto out;
	}

	if (handle_transition_verb(uc_mgr, i) == 0)
		goto out;

	/*
	 * Dismantle the old use cases by running it's verb, device and modifier
	 * disable sequences
	 */
	ret = dismantle_use_case(uc_mgr, uc_mgr->list, uc_mgr->handle);
	if (ret < 0) {
		uc_error("error: failed to dismantle current use case: %s",
			uc_mgr->verb[i].name);
		goto out;
	}

	/* we don't need to initialise new verb if inactive */
	if (inactive)
		goto out;

	/* Initialise the new use case verb */
	ret = enable_use_case_verb(uc_mgr, i, uc_mgr->list, uc_mgr->handle);
	if (ret < 0)
		uc_error("error: failed to initialise new use case: %s",
				verb_name);
out:
	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

static int config_use_case_device(snd_use_case_mgr_t *uc_mgr,
		const char *device_name, int enable)
{
	struct use_case_verb *verb;
	int ret, i;

	pthread_mutex_lock(&uc_mgr->mutex);

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED) {
		uc_error("error: no valid use case verb set\n");
		ret = -EINVAL;
		goto out;
	}

	verb = &uc_mgr->verb[uc_mgr->card.current_verb];

	uc_dbg("current verb %s", verb->name);
	uc_dbg("uc_mgr %p device_name %s", uc_mgr, device_name);

	/* find device name and index */
	for (i = 0; i <verb->num_devices; i++) {
		uc_dbg("verb->num_devices %s", verb->device[i].name);
		if (!strcmp(verb->device[i].name, device_name))
			goto found;
	}

	uc_error("error: use case device %s not found", device_name);
	ret = -EINVAL;
	goto out;

found:
	if (enable) {
		/* Initialise the new use case device */
		ret = enable_use_case_device(uc_mgr, i, uc_mgr->list,
			uc_mgr->handle);
		if (ret < 0)
			goto out;
	} else {
		/* disable the old device */
		ret = disable_use_case_device(uc_mgr, i, uc_mgr->list,
			uc_mgr->handle);
		if (ret < 0)
			goto out;
	}

out:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}

/**
 * \brief Enable use case device
 * \param uc_mgr Use case manager
 * \param device the device to be enabled
 * \return 0 = successful negative = error
 */
int snd_use_case_enable_device(snd_use_case_mgr_t *uc_mgr,
		const char *device)
{
	return config_use_case_device(uc_mgr, device, 1);
}

/**
 * \brief Disable use case device
 * \param uc_mgr Use case manager
 * \param device the device to be disabled
 * \return 0 = successful negative = error
 */
int snd_use_case_disable_device(snd_use_case_mgr_t *uc_mgr,
		const char *device)
{
	return config_use_case_device(uc_mgr, device, 0);
}

static struct use_case_device *get_device(snd_use_case_mgr_t *uc_mgr,
							const char *name, int *id)
{
	struct use_case_verb *verb;
	int i;

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED)
		return NULL;

	verb = &uc_mgr->verb[uc_mgr->card.current_verb];

	uc_dbg("current verb %s", verb->name);

	for (i = 0; i < verb->num_devices; i++) {
		uc_dbg("device %s", verb->device[i].name);

		if (!strcmp(verb->device[i].name, name)) {
			if (id)
				*id = i;
			return &verb->device[i];
		}
	}

	return NULL;
}

/**
 * \brief Disable old_device and then enable new_device.
 *        If from_device is not enabled just return.
 *        Check transition sequence firstly.
 * \param uc_mgr Use case manager
 * \param old the device to be closed
 * \param new the device to be opened
 * \return 0 = successful negative = error
 */
int snd_use_case_switch_device(snd_use_case_mgr_t *uc_mgr,
			const char *old, const char *new)
{
	static struct sequence_element *trans_sequence;
	struct use_case_device *old_device;
	struct use_case_device *new_device;
	int ret = 0, old_id, new_id;

	uc_dbg("old %s, new %s", old, new);

	pthread_mutex_lock(&uc_mgr->mutex);

	old_device = get_device(uc_mgr, old, &old_id);
	if (!old_device) {
		uc_error("error: device %s not found", old);
		ret = -EINVAL;
		goto out;
	}

	if (!get_device_status(uc_mgr, old_id)) {
		uc_error("error: device %s not enabled", old);
		goto out;
	}

	new_device = get_device(uc_mgr, new, &new_id);
	if (!new_device) {
		uc_error("error: device %s not found", new);
		ret = -EINVAL;
		goto out;
	}

	trans_sequence = get_transition_sequence(old_device->transition_list, new);
	if (trans_sequence != NULL) {

		uc_dbg("find transition sequece %s->%s", old, new);

		ret = exec_transition_sequence(uc_mgr, trans_sequence);
		if (ret)
			goto out;

		set_device_status(uc_mgr, old_id, 0);
		set_device_status(uc_mgr, new_id, 1);
	} else {
		/* use lock in config_use_case_device */
		pthread_mutex_unlock(&uc_mgr->mutex);

		config_use_case_device(uc_mgr, old, 0);
		config_use_case_device(uc_mgr, new, 1);

		return 0;
	}
out:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}

/*
 * Check to make sure that the modifier actually supports any of the
 * active devices.
 */
static int is_modifier_valid(snd_use_case_mgr_t *uc_mgr,
	struct use_case_verb *verb, struct use_case_modifier *modifier)
{
	struct dev_list *dev_list;
	int dev;

	/* check modifier list against each enabled device */
	for (dev = 0; dev < verb->num_devices; dev++) {
		if (!get_device_status(uc_mgr, dev))
			continue;

		dev_list = modifier->dev_list;
		uc_dbg("checking device %s for %s", verb->device[dev].name,
			dev_list->name ? dev_list->name : "");

		while (dev_list) {
			uc_dbg("device supports %s", dev_list->name);
			if (!strcmp(dev_list->name, verb->device[dev].name))
					return 1;
			dev_list = dev_list->next;
		}
	}
	return 0;
}

static int config_use_case_mod(snd_use_case_mgr_t *uc_mgr,
		const char *modifier_name, int enable)
{
	struct use_case_verb *verb;
	int ret, i;

	pthread_mutex_lock(&uc_mgr->mutex);

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED) {
		ret = -EINVAL;
		goto out;
	}

	verb = &uc_mgr->verb[uc_mgr->card.current_verb];

	uc_dbg("current verb %s", verb->name);
	uc_dbg("uc_mgr %p modifier_name %s", uc_mgr, modifier_name);

	/* find modifier name */
	for (i = 0; i <verb->num_modifiers; i++) {
		uc_dbg("verb->num_modifiers %d %s", i, verb->modifier[i].name);
		if (!strcmp(verb->modifier[i].name, modifier_name) &&
			is_modifier_valid(uc_mgr, verb, &verb->modifier[i]))
			goto found;
	}

	uc_error("error: use case modifier %s not found or invalid",
		modifier_name);
	ret = -EINVAL;
	goto out;

found:
	if (enable) {
		/* Initialise the new use case device */
		ret = enable_use_case_modifier(uc_mgr, i, uc_mgr->list,
			uc_mgr->handle);
		if (ret < 0)
			goto out;
	} else {
		/* disable the old device */
		ret = disable_use_case_modifier(uc_mgr, i, uc_mgr->list,
			uc_mgr->handle);
		if (ret < 0)
			goto out;
	}

out:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}

/**
 * \brief Enable use case modifier
 * \param uc_mgr Use case manager
 * \param modifier the modifier to be enabled
 * \return 0 = successful negative = error
 */
int snd_use_case_enable_modifier(snd_use_case_mgr_t *uc_mgr,
		const char *modifier)
{
	return config_use_case_mod(uc_mgr, modifier, 1);
}

/**
 * \brief Disable use case modifier
 * \param uc_mgr Use case manager
 * \param modifier the modifier to be disabled
 * \return 0 = successful negative = error
 */
int snd_use_case_disable_modifier(snd_use_case_mgr_t *uc_mgr,
		const char *modifier)
{
	return config_use_case_mod(uc_mgr, modifier, 0);
}

static struct use_case_modifier *get_modifier(snd_use_case_mgr_t *uc_mgr,
							const char *name, int *mod_id)
{
	struct use_case_verb *verb;
	int i;

	if (uc_mgr->card.current_verb == VERB_NOT_INITIALISED)
		return NULL;

	verb = &uc_mgr->verb[uc_mgr->card.current_verb];

	uc_dbg("current verb %s", verb->name);

	uc_dbg("uc_mgr %p modifier_name %s", uc_mgr, name);

	for (i = 0; i < verb->num_modifiers; i++) {
		uc_dbg("verb->num_devices %s", verb->modifier[i].name);

		if (!strcmp(verb->modifier[i].name, name)) {
			if (mod_id)
				*mod_id = i;
			return &verb->modifier[i];
		}
	}

	return NULL;
}

/**
 * \brief Disable old_modifier and then enable new_modifier.
 *        If old_modifier is not enabled just return.
 *        Check transition sequence firstly.
 * \param uc_mgr Use case manager
 * \param old the modifier to be closed
 * \param new the modifier to be opened
 * \return 0 = successful negative = error
 */
int snd_use_case_switch_modifier(snd_use_case_mgr_t *uc_mgr,
			const char *old, const char *new)
{
	struct use_case_modifier *old_modifier;
	struct use_case_modifier *new_modifier;
	static struct sequence_element *trans_sequence;
	int ret = 0, old_id, new_id

	uc_dbg("old %s, new %s", old, new);

	pthread_mutex_lock(&uc_mgr->mutex);

	old_modifier = get_modifier(uc_mgr, old, &old_id);
	if (!old_modifier) {
		uc_error("error: modifier %s not found", old);
		ret = -EINVAL;
		goto out;
	}

	if (!get_modifier_status(uc_mgr, old_id)) {
		uc_error("error: modifier %s not enabled", old);
		ret = -EINVAL;
		goto out;
	}

	new_modifier = get_modifier(uc_mgr, new, &new_id);
	if (!new_modifier) {
		uc_error("error: modifier %s not found", new);
		ret = -EINVAL;
		goto out;
	}

	trans_sequence = get_transition_sequence(
				old_modifier->transition_list, new);
	if (trans_sequence != NULL) {
		uc_dbg("find transition sequence %s->%s", old, new);

		ret = exec_transition_sequence(uc_mgr, trans_sequence);
		if (ret)
			goto out;

		set_device_status(uc_mgr, old_id, 0);
		set_device_status(uc_mgr, new_id, 1);
	} else {
		/* use lock in config_use_case_mod*/
		pthread_mutex_unlock(&uc_mgr->mutex);

		config_use_case_mod(uc_mgr, old, 0);
		config_use_case_mod(uc_mgr, new, 1);

		return 0;
	}
out:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}

/**
 * \brief Get current use case verb from sound card
 * \param uc_mgr use case manager
 * \return Verb Name if success, otherwise NULL
 */
const char *snd_use_case_get_verb(snd_use_case_mgr_t *uc_mgr)
{
	const char *ret = NULL;

	pthread_mutex_lock(&uc_mgr->mutex);

	if (uc_mgr->card.current_verb != VERB_NOT_INITIALISED)
		ret = uc_mgr->verb_list[uc_mgr->card.current_verb];

	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get device status for current use case verb
 * \param uc_mgr Use case manager
 * \param device_name The device we are interested in.
 * \return - 1 = enabled, 0 = disabled, negative = error
 */
int snd_use_case_get_device_status(snd_use_case_mgr_t *uc_mgr,
		const char *device_name)
{
	struct use_case_device *device;
	int ret = -EINVAL, dev_id;

	pthread_mutex_lock(&uc_mgr->mutex);

	device = get_device(uc_mgr, device_name, &dev_id);
	if (device == NULL) {
		uc_error("error: use case device %s not found", device_name);
		goto out;
	}

	ret = get_device_status(uc_mgr, dev_id);
out:
	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get modifier status for current use case verb
 * \param uc_mgr Use case manager
 * \param device_name The device we are interested in.
 * \return - 1 = enabled, 0 = disabled, negative = error
 */
int snd_use_case_get_modifier_status(snd_use_case_mgr_t *uc_mgr,
		const char *modifier_name)
{
	struct use_case_modifier *modifier;
	int ret = -EINVAL, mod_id;

	pthread_mutex_lock(&uc_mgr->mutex);

	modifier = get_modifier(uc_mgr, modifier_name, &mod_id);
	if (modifier == NULL) {
		uc_error("error: use case modifier %s not found", modifier_name);
		goto out;
	}

	ret = get_modifier_status(uc_mgr, mod_id);
out:
	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get current use case verb QoS
 * \param uc_mgr use case manager
 * \return QoS level
 */
enum snd_use_case_qos
	snd_use_case_get_verb_qos(snd_use_case_mgr_t *uc_mgr)
{
	struct use_case_verb *verb;
	enum snd_use_case_qos ret = SND_USE_CASE_QOS_UNKNOWN;

	pthread_mutex_lock(&uc_mgr->mutex);

	if (uc_mgr->card.current_verb != VERB_NOT_INITIALISED) {
		verb = &uc_mgr->verb[uc_mgr->card.current_verb];
		ret = verb->qos;
	}

	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get current use case modifier QoS
 * \param uc_mgr use case manager
 * \return QoS level
 */
enum snd_use_case_qos
	snd_use_case_get_mod_qos(snd_use_case_mgr_t *uc_mgr,
					const char *modifier_name)
{
	struct use_case_modifier *modifier;
	enum snd_use_case_qos ret = SND_USE_CASE_QOS_UNKNOWN;

	pthread_mutex_lock(&uc_mgr->mutex);

	modifier = get_modifier(uc_mgr, modifier_name, NULL);
	if (modifier != NULL)
		ret = modifier->qos;
	else
		uc_error("error: use case modifier %s not found", modifier_name);
	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get current use case verb playback PCM
 * \param uc_mgr use case manager
 * \return PCM number if success, otherwise negative
 */
int snd_use_case_get_verb_playback_pcm(snd_use_case_mgr_t *uc_mgr)
{
	struct use_case_verb *verb;
	int ret = -EINVAL;

	pthread_mutex_lock(&uc_mgr->mutex);

	if (uc_mgr->card.current_verb != VERB_NOT_INITIALISED) {
		verb = &uc_mgr->verb[uc_mgr->card.current_verb];
		ret = verb->playback_pcm;
	}

	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get current use case verb playback PCM
 * \param uc_mgr use case manager
 * \return PCM number if success, otherwise negative
 */
int snd_use_case_get_verb_capture_pcm(snd_use_case_mgr_t *uc_mgr)
{
	struct use_case_verb *verb;
	int ret = -EINVAL;

	pthread_mutex_lock(&uc_mgr->mutex);

	if (uc_mgr->card.current_verb != VERB_NOT_INITIALISED) {
		verb = &uc_mgr->verb[uc_mgr->card.current_verb];
		ret = verb->capture_pcm;
	}

	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get current use case modifier playback PCM
 * \param uc_mgr use case manager
 * \return PCM number if success, otherwise negative
 */
int snd_use_case_get_mod_playback_pcm(snd_use_case_mgr_t *uc_mgr,
					const char *modifier_name)
{
	struct use_case_modifier *modifier;
	int ret = -EINVAL;

	pthread_mutex_lock(&uc_mgr->mutex);

	modifier = get_modifier(uc_mgr, modifier_name, NULL);
	if (modifier == NULL)
		uc_error("error: use case modifier %s not found",
						modifier_name);
	else
		ret = modifier->playback_pcm;

	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get current use case modifier playback PCM
 * \param uc_mgr use case manager
 * \return PCM number if success, otherwise negative
 */
int snd_use_case_get_mod_capture_pcm(snd_use_case_mgr_t *uc_mgr,
	const char *modifier_name)
{
	struct use_case_modifier *modifier;
	int ret = -EINVAL;

	pthread_mutex_lock(&uc_mgr->mutex);

	modifier = get_modifier(uc_mgr, modifier_name, NULL);
	if (modifier == NULL)
		uc_error("error: use case modifier %s not found",
						modifier_name);
	else
		ret = modifier->capture_pcm;

	pthread_mutex_unlock(&uc_mgr->mutex);

	return ret;
}

/**
 * \brief Get volume/mute control name depending on use case device.
 * \param uc_mgr use case manager
 * \param type the control type we are looking for
 * \param device_name The use case device we are interested in.
 * \return control name if success, otherwise NULL
 *
 * Get the control id for common volume and mute controls that are aliased
 * in the named use case device.
 */
const char *snd_use_case_get_device_ctl_elem_name(snd_use_case_mgr_t *uc_mgr,
		enum snd_use_case_control_alias type, const char *device_name)
{
	struct use_case_device *device;
	const char *kcontrol_name = NULL;

	pthread_mutex_lock(&uc_mgr->mutex);

	device = get_device(uc_mgr, device_name, NULL);
	if (!device) {
		uc_error("error: device %s not found", device_name);
		goto out;
	}

	switch (type) {
	case SND_USE_CASE_ALIAS_PLAYBACK_VOLUME:
		kcontrol_name = device->playback_volume_id;
		break;
	case SND_USE_CASE_ALIAS_CAPTURE_VOLUME:
		kcontrol_name = device->capture_volume_id;
		break;
	case SND_USE_CASE_ALIAS_PLAYBACK_SWITCH:
		kcontrol_name = device->playback_switch_id;
		break;
	case SND_USE_CASE_ALIAS_CAPTURE_SWITCH:
		kcontrol_name = device->capture_switch_id;
		break;
	default:
		uc_error("error: invalid control alias %d", type);
		break;
	}

out:
	pthread_mutex_unlock(&uc_mgr->mutex);

	return kcontrol_name;
}

/**
 * \brief Get volume/mute control IDs depending on use case modifier.
 * \param uc_mgr use case manager
 * \param type the control type we are looking for
 * \param modifier_name The use case modifier we are interested in.
 * \return ID if success, otherwise a negative error code
 *
 * Get the control id for common volume and mute controls that are aliased
 * in the named use case device.
 */
const char *snd_use_case_get_modifier_ctl_elem_name(snd_use_case_mgr_t *uc_mgr,
		enum snd_use_case_control_alias type, const char *modifier_name)
{
	struct use_case_modifier *modifier;
	const char *kcontrol_name = NULL;

	pthread_mutex_lock(&uc_mgr->mutex);

	modifier = get_modifier(uc_mgr, modifier_name, NULL);
	if (!modifier) {
		uc_error("error: modifier %s not found", modifier_name);
		goto out;
	}

	switch (type) {
	case SND_USE_CASE_ALIAS_PLAYBACK_VOLUME:
		kcontrol_name = modifier->playback_volume_id;
		break;
	case SND_USE_CASE_ALIAS_CAPTURE_VOLUME:
		kcontrol_name = modifier->capture_volume_id;
		break;
	case SND_USE_CASE_ALIAS_PLAYBACK_SWITCH:
		kcontrol_name = modifier->playback_switch_id;
		break;
	case SND_USE_CASE_ALIAS_CAPTURE_SWITCH:
		kcontrol_name = modifier->capture_switch_id;
		break;
	default:
		uc_error("error: invalid control alias %d", type);
		break;
	}

out:
	pthread_mutex_unlock(&uc_mgr->mutex);

	return kcontrol_name;
}
#endif
