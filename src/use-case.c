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
 *  Authors: Liam Girdwood <lrg@slimlogic.co.uk>
 *	         Stefan Schmidt <stefan@slimlogic.co.uk>
 *	         Justin Xu <justinx@slimlogic.co.uk>
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <alsa/asoundlib.h>
#include <pthread.h>

#include "../include/use-case.h"
#include "../include/iatomic.h"

#define PRE_SEQ		0
#define POST_SEQ		1
#define MAX_VERB		32
#define MAX_DEVICE		64
#define MAX_MODIFIER		64
#define MAX_NAME		64
#define MAX_FILE		256
#define MAX_BUF		256
#define ALSA_USE_CASE_DIR	"/usr/share/alsa/ucm"
#define ARRAY_SIZE(x)		(sizeof(x)/sizeof(x[0]))
#define VERB_NOT_INITIALISED	-1

/*
 * Stores all use case settings for 1 kcontrol. Hence we have a
 * control_settings for each kcontrol in card.
 */
struct control_settings {
	char name[MAX_NAME];
	unsigned int id;
	snd_ctl_elem_type_t type;
	short count; /* 1 = mono, 2 = stereo, etc */
	unsigned short *value;
};

/*
 * If sleep is 0 the element contains the settings in control. Else sleep
 * contains the sleep time in micro seconds.
 */
struct sequence_element {
	unsigned int sleep; /* Sleep time in msecs if sleep element, else 0 */
	struct control_settings *control;
	struct sequence_element *next; /* Pointer to next list element */
};

/*
 * Transition sequences. i.e. transition between one verb, device, mod to another
 */
struct transition_sequence {
	char *name;
	struct sequence_element *transition;
	struct transition_sequence *next;
};

/*
 * Modifier Supported Devicees.
 */
struct dev_list {
	char *name;
	struct dev_list *next;
};


/*
 * Describes a Use Case Modifier and it's enable and disable sequences.
 * A use case verb can have N modifiers.
 */
struct use_case_modifier {
	char *name;
	char *comment;

	/* modifier enable and disable sequences */
	struct sequence_element *enable;
	struct sequence_element *disable;

	/* modifier transition list */
	struct transition_sequence *transition_list;

	/* list of supported devices per modifier */
	struct dev_list *dev_list;

	/* ALSA PCM devices associated with any modifier PCM streams */
	int capture_pcm;
	int playback_pcm;

	/* Any modifier stream QoS */
	enum snd_use_case_qos qos;

	/* aliased controls */
	char *playback_volume_id;
	char *playback_switch_id;
	char *capture_volume_id;
	char *capture_switch_id;
};

/*
 * Describes a Use Case Device and it's enable and disable sequences.
 * A use case verb can have N devices.
 */
struct use_case_device {
	char *name;
	char *comment;
	int idx; /* index for similar devices i.e. 2 headphone jacks */

	/* device enable and disable sequences */
	struct sequence_element *enable;
	struct sequence_element *disable;

	/* device transition list */
	struct transition_sequence *transition_list;

	/* aliased controls */
	char *playback_volume_id;
	char *playback_switch_id;
	char *capture_volume_id;
	char *capture_switch_id;
};

/*
 * Describes a Use Case Verb and it's enable and disable sequences.
 * A use case verb can have N devices and N modifiers.
 */
struct use_case_verb {
	char *name;
	char *comment;

	/* verb enable and disable sequences */
	struct sequence_element *enable;
	struct sequence_element *disable;

	/* verb transition list */
	struct transition_sequence *transition_list;

	/* verb PCMs and QoS */
	enum snd_use_case_qos qos;
	int capture_pcm;
	int playback_pcm;

	/* hardware devices that can be used with this use case */
	int num_devices;
	struct use_case_device *device;
	/*
	 * device_list[i] shares string with device[i].name,
	 * so device_list don't need not be freed
	 */
	const char *device_list[MAX_DEVICE];

	/* modifiers that can be used with this use case */
	int num_modifiers;
	struct use_case_modifier *modifier;
	/*
	 * modifier_list[i] shares string with modifier[i].name,
	 * so modifier_list don't need not be freed
	 */
	const char *modifier_list[MAX_MODIFIER];
};

struct ucm_card {
	int current_verb;
	int current_device[MAX_DEVICE];
	int current_modifier[MAX_MODIFIER];
};

/*
 *  Manages a sound card and all its use cases.
 */
struct snd_use_case_mgr {
	pthread_mutex_t mutex;
	char *card_name;
	char *ctl_name;
	struct ucm_card card;

	/* use case verb, devices and modifier configs parsed from files */
	int num_verbs; /* number of supported use case verbs */
	struct use_case_verb *verb; /* var len array of use case info */
	/*
	 * verb_list[i] shares string with verb[i].name,
	 * so verb_list don't need not be freed
	 */
	const char *verb_list[MAX_VERB];

	/* sound card ALSA kcontrol read from sound card device */
	struct control_settings *control; /* var len array of controls */

	/* snd ctl */
	int count;
	snd_ctl_t *handle;
	snd_ctl_card_info_t *info;
	snd_ctl_elem_list_t *list;
	snd_ctl_elem_id_t *id;
};

static void uc_mgr_error(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stderr, "ucm: ");
	vfprintf(stderr, fmt, va);
	va_end(va);
}

#define uc_error(fmt, arg...) do { \
        uc_mgr_error("%s() - " fmt "\n", __FUNCTION__ , ## arg); \
} while (0)

static void uc_mgr_stdout(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stdout, fmt, va);
	va_end(va);
}

#undef UC_MGR_DEBUG

#ifdef UC_MGR_DEBUG
#define uc_dbg(fmt, arg...) do { \
        uc_mgr_stdout("%s() - " fmt "\n", __FUNCTION__ , ## arg); \
} while (0)
#else
#define uc_dbg(fmt, arg...)
#endif

static int set_control(snd_ctl_t *handle, snd_ctl_elem_id_t *id,
	snd_use_case_mgr_t *uc_mgr, unsigned short value[]);

static inline void set_value(struct control_settings *control,
		int count, unsigned short val)
{
	uc_dbg("value %d, count %d", val, count);
	control->value[count] = val;
}

static inline unsigned short get_value(struct control_settings *control,
		int count)
{
	return control->value[count];
}

static inline void set_device_status(struct snd_use_case_mgr *uc_mgr,
	int device_id, int status)
{
	struct use_case_verb *verb;

	verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	uc_mgr->card.current_device[device_id] = status;
}

static inline void set_modifier_status(struct snd_use_case_mgr *uc_mgr,
	int modifier_id, int status)
{
	struct use_case_verb *verb;

	verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	uc_mgr->card.current_modifier[modifier_id] = status;
}

static inline int get_device_status(struct snd_use_case_mgr *uc_mgr,
	int device_id)
{
	struct use_case_verb *verb;

	verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	return uc_mgr->card.current_device[device_id];
}

static inline int get_modifier_status(struct snd_use_case_mgr *uc_mgr,
	int modifier_id)
{
	struct use_case_verb *verb;

	verb = &uc_mgr->verb[uc_mgr->card.current_verb];
	return uc_mgr->card.current_modifier[modifier_id];
}

static int dump_control(snd_ctl_t *handle, snd_ctl_elem_id_t *id)
{
	int err, count, i;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_type_t type;
	snd_ctl_elem_value_t *control;

	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_value_alloca(&control);

	snd_ctl_elem_info_set_id(info, id);
	err = snd_ctl_elem_info(handle, info);
	if (err < 0) {
		uc_error("error: failed to get ctl info: %s\n",
			snd_strerror(err));
		return err;
	}

	snd_ctl_elem_value_set_id(control, id);
	snd_ctl_elem_read(handle, control);

	type = snd_ctl_elem_info_get_type(info);
	count = snd_ctl_elem_info_get_count(info);
	if (count == 0)
		return 0;

	uc_mgr_stdout("'%s':%d:",
	       snd_ctl_elem_id_get_name(id), count);

	switch (type) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		for (i = 0; i < count - 1; i++)
			uc_mgr_stdout("%d,",
				snd_ctl_elem_value_get_boolean(control, i));
		uc_mgr_stdout("%d",
			snd_ctl_elem_value_get_boolean(control, i));
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		for (i = 0; i < count - 1; i++)
			uc_mgr_stdout("%d,",
				snd_ctl_elem_value_get_integer(control, i));
		uc_mgr_stdout("%d",
			snd_ctl_elem_value_get_integer(control, i));
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		for (i = 0; i < count - 1; i++)
			uc_mgr_stdout("%ld,",
				snd_ctl_elem_value_get_integer64(control, i));
		uc_mgr_stdout("%ld",
				snd_ctl_elem_value_get_integer64(control, i));
		break;
	case SND_CTL_ELEM_TYPE_ENUMERATED:
		for (i = 0; i < count - 1; i++)
			uc_mgr_stdout("%d,",
				snd_ctl_elem_value_get_enumerated(control, i));
		uc_mgr_stdout("%d",
				snd_ctl_elem_value_get_enumerated(control, i));
		break;
	case SND_CTL_ELEM_TYPE_BYTES:
		for (i = 0; i < count - 1; i++)
			uc_mgr_stdout("%2.2x,",
				snd_ctl_elem_value_get_byte(control, i));
		uc_mgr_stdout("%2.2x",
			snd_ctl_elem_value_get_byte(control, i));
		break;
	default:
		break;
	}
	uc_mgr_stdout("\n");
	return 0;
}

/*
 * Add new kcontrol from sound card into memory database.
 */
static int add_control(snd_ctl_t *handle, snd_ctl_elem_id_t *id,
	struct control_settings *control_settings)
{
	int err;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *control;

	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_value_alloca(&control);

	snd_ctl_elem_info_set_id(info, id);
	err = snd_ctl_elem_info(handle, info);
	if (err < 0) {
		uc_error("error: failed to get ctl info: %s\n",
			snd_strerror(err));
		return err;
	}

	snd_ctl_elem_value_set_id(control, id);
	snd_ctl_elem_read(handle, control);

	strncpy(control_settings->name, snd_ctl_elem_id_get_name(id),
		MAX_NAME);

	control_settings->name[MAX_NAME - 1] = 0;
	control_settings->count = snd_ctl_elem_info_get_count(info);
	control_settings->type = snd_ctl_elem_info_get_type(info);
	control_settings->id = snd_ctl_elem_id_get_numid(id);
	uc_dbg("control name %s", control_settings->name);
	uc_dbg("control count %d", control_settings->count);
	uc_dbg("control type %d", control_settings->type);
	uc_dbg("control id %d", control_settings->id);
	return 0;
}

static int set_control_default(snd_use_case_mgr_t *uc_mgr,
	struct control_settings *control)
{
	snd_ctl_elem_id_t *id;
	int i, ret = -ENODEV;
	unsigned int numid;

	snd_ctl_elem_id_alloca(&id);

	/* Where is id lookup from numid if you need it? */
	for (i = 0; i < uc_mgr->count; ++i) {
		snd_ctl_elem_list_get_id(uc_mgr->list, i, id);
		numid = snd_ctl_elem_id_get_numid(id);
		if (numid == control->id) {
			ret = set_control(uc_mgr->handle, id, uc_mgr,
				control->value);
			goto out;
		}
	}
	uc_error("error: could not find control ID %s : %d",
		control->name, control->id);
out:
	return ret;
}

static int get_control_id(snd_use_case_mgr_t *uc_mgr,
			struct control_settings *control)
{
	int i = 0;
	struct control_settings *card_control;

	uc_dbg("name %s count %d",
			control->name, control->count);

	for (i = 0; i < uc_mgr->count; i++) {
		card_control = &uc_mgr->control[i];
		if (!strcmp(card_control->name, control->name)) {
			control->id = uc_mgr->control[i].id;
			uc_dbg("Get id %d", control->id);
			return 0;
		}
	}

	uc_error("error: control name %s is not available", control->name);

	return -EINVAL;
}

static char *get_control_name(char *buf, int line, char *file)
{
	char name[MAX_NAME];
	char *name_start, *name_end, *tbuf = buf;

	/* get name start */
	while (*tbuf != 0 && *tbuf != '\'')
		tbuf++;
	if (*tbuf == 0)
		return NULL;
	name_start = ++tbuf;

	/* get name end */
	while (*tbuf != 0 && *tbuf != '\'')
		tbuf++;
	if (*tbuf == 0)
		return NULL;
	name_end = tbuf++;

	/* copy name */
	if ((name_end - name_start) > MAX_NAME) {
		uc_error("error: %s:%d name too big at %d chars",
				file, line, name_end - name_start);
		return NULL;
	}
	strncpy(name, name_start, name_end - name_start);
	name[name_end - name_start] = 0;
	return strdup(name);
}

/*
 * Parse a single control from file.
 *
 * Controls are in the following form:-
 *
 * 'name':channels:value0,value1,...,valueN
 *
 * e.g.
 *	'Master Playback Switch':2:0,0
 */
static int parse_control(snd_use_case_mgr_t *uc_mgr,
			struct control_settings *control, char *buf,
			int line, char *file)
{
	char name[MAX_NAME];
	int count, i;
	char *name_start, *name_end, *tbuf;

	uc_dbg("%s", buf);

	tbuf = buf;

	/* get name start */
	while (*tbuf != 0 && *tbuf != '\'')
		tbuf++;
	if (*tbuf == 0)
		return -EINVAL;
	name_start = ++tbuf;

	/* get name end */
	while (*tbuf != 0 && *tbuf != '\'')
		tbuf++;
	if (*tbuf == 0)
		return -EINVAL;
	name_end = tbuf++;

	/* copy name */
	if ((name_end - name_start) > MAX_NAME) {
		uc_error("error: %s:%d name too big at %d chars",
				file, line, name_end - name_start);
		return -EINVAL;
	}
	strncpy(name, name_start, name_end - name_start);
	name[name_end - name_start] = 0;
	strncpy(control->name, name, name_end - name_start +1);

	/* get count */
	uc_dbg("%s", tbuf);
	tbuf++;
	count = atoi(tbuf);
	if (count == 0) {
		uc_error("error: %s:%d count == 0 on line %d", file, line);
		return -EINVAL;
	}
	control->count = count;

	/* get vals */
	control->value = calloc(count, sizeof(unsigned short));
	if (control->value == NULL)
		return -ENOMEM;

	while (*tbuf != 0 && *tbuf != ':')
		tbuf++;
	if (*tbuf == 0)
		return -EINVAL;
	tbuf++;

	for (i = 0; i < count; i++) {
		set_value(control, i, atoi(tbuf));
		while (*tbuf != 0 && *tbuf != ',')
			tbuf++;

		if (*tbuf++ == 0 && i < (count - 1))
			return -EINVAL;
	}

	return get_control_id(uc_mgr, control);
}

static int parse_controls(snd_use_case_mgr_t *uc_mgr, FILE *f, int *line_,
		char *file)
{
	struct control_settings *control = NULL;
	char buf[MAX_BUF], name[MAX_NAME];
	int count, i, ret = 0, line = *line_;
	char *name_start, *name_end, *tbuf;

	while (fgets(buf, MAX_BUF, f) != NULL) {

		uc_dbg("%s: get line %d\n%s", file, line, buf);

		tbuf = buf;
		while (isblank(*tbuf))
		tbuf++;
		line ++;

		/* end of section ?*/
		if (strncmp(tbuf, "EndSectionDefaults", 18) == 0)
			return 0;

		/* get name start */
		while (*tbuf != 0 && *tbuf != '\'')
			tbuf++;
		if (*tbuf == 0)
			return -EINVAL;
		name_start = ++tbuf;

		/* get name end */
		while (*tbuf != 0 && *tbuf != '\'')
			tbuf++;
		if (*tbuf == 0)
			return -EINVAL;
		name_end = tbuf++;

		/* copy name */
		if ((name_end - name_start) > MAX_NAME) {
			uc_error("error: %s:%d name too big at %d chars",
					file, line, name_end - name_start);
			return -EINVAL;
		}
		strncpy(name, name_start, name_end - name_start);
		name[name_end - name_start] = 0;

		for (i = 0; i < uc_mgr->count; i++) {
			struct control_settings *card_control =
							&uc_mgr->control[i];
			if (!strcmp(card_control->name, name))
				control = &uc_mgr->control[i];
		}

		uc_dbg("control id %d", control->id);

		/* get count */
		tbuf++;
		count = atoi(tbuf);
		if (count == 0) {
			uc_error("error: %s:%d count == 0", file, line);
			return -EINVAL;
		}
		if (count != control->count) {
			uc_error("error: %s:%d count %d does not match card count"
				" %d", file, line, count, control->count);
			return -EINVAL;
		}

		/* get vals */
		control->value = malloc(control->count *uc_mgr->num_verbs *
			sizeof(unsigned short));
		if (control->value == NULL)
			return -ENOMEM;

		while (*tbuf != 0 && *tbuf != ':')
			tbuf++;
		if (*tbuf == 0)
			return -EINVAL;
		tbuf++;

		for (i = 0; i < count; i++) {
			set_value(control, i, atoi(tbuf));
			while (*tbuf != 0 && *tbuf != ',')
				tbuf++;

			if (*tbuf++ == 0 && i < (count - 1))
				return -EINVAL;
		}
	}

	*line_ = line;
	return ret;
}

static char *get_string (char *buf)
{
	char *str, *end;

	uc_dbg("%s", buf);

	while (isblank(*buf))
		buf++;

	/* find leading '"' */
	if (*buf == 0 || *buf != '"') {
		uc_error("error: missing start '\"'");
		return NULL;
	}
	str = ++buf;

	/* get value */
	while (*buf != 0 && *buf != '"')
		buf++;
	end = buf;

	/* find '"' terminator */
	if (*buf == 0 || *buf != '"') {
		uc_error("error: missing terminator '\"' %s", buf);
		return NULL;
	}

	*end = 0;
	return strdup(str);
}

static enum snd_use_case_qos get_enum (char *buf)
{
	while (isblank(*buf))
		buf++;

	if (!strncmp(buf, "Music", 5))
		return SND_USE_CASE_QOS_MUSIC;
	if (!strncmp(buf, "Voice", 5))
		return SND_USE_CASE_QOS_VOICE;
	if (!strncmp(buf, "Tones", 5))
		return SND_USE_CASE_QOS_TONES;

	return SND_USE_CASE_QOS_MUSIC;
}

static void seq_list_append(struct sequence_element **base,
			struct sequence_element *curr)
{
	struct sequence_element *last, *tmp;

	if (!*base)
		*base = curr;
	else {
		tmp = *base;
		while (tmp) {
			last = tmp;
			tmp = tmp->next;
		}
		last->next = curr;
		curr->next = NULL;
	}
}
/*
 * Parse sequences.
 *
 * Sequence controls elements  are in the following form:-
 *
 * 'name':value0,value1,...,valueN
 *
 * e.g.
 *	'Master Playback Switch':0,0
 */
static int parse_sequence(snd_use_case_mgr_t *uc_mgr,
			struct sequence_element **base, FILE *f,
			int *line_, char *file)
{
	char buf[MAX_BUF], *tbuf;
	int ret, line = *line_;
	struct sequence_element *curr;

	/* read line by line */
	while (fgets(buf, MAX_BUF, f) != NULL) {

		uc_dbg("%s", buf);
		line++;

		/* Check for lines with comments and ignore */
		if (buf[0] == '#')
			continue;

		/* Parse current line and skip blanks */
		tbuf = buf;
		while (isblank(*tbuf))
			tbuf++;

		/* end of sequence ? */
		if (strncmp(tbuf, "EndSequence", 11) == 0)
			goto out;

		if (strncmp(tbuf, "EndTransition", 13) == 0)
			goto out;

		/* alloc new sequence element */
		curr = calloc(1, sizeof(struct sequence_element));
		if (curr == NULL)
			return -ENOMEM;

		/* is element a sleep ? */
		if (strncmp(tbuf, "msleep", 6) == 0) {
			curr->sleep = atoi(tbuf + 6);
			uc_dbg("msleep %d", curr->sleep);
			seq_list_append(base, curr);
			continue;
		}

		/* alloc new sequence control */
		curr->control = calloc(1, sizeof(struct control_settings));
		if (curr->control == NULL)
			return -ENOMEM;

		ret = parse_control(uc_mgr, curr->control, tbuf, line, file);
		if (ret < 0) {
			uc_error("error: %s:%d failed to get parse sequence"
				" controls", file, line);
				goto err;
		}

		uc_dbg("name %s, id %d, count %d", curr->control->name,
			curr->control->id, curr->control->count);
		seq_list_append(base, curr);
	}

out:
	*line_ = line;
	return 0;

err:
	free(curr);
	return ret;
}

static void prepend_transition(
				struct transition_sequence **transition_list,
				struct transition_sequence *trans_seq)
{
	if (*transition_list == NULL)
		*transition_list = trans_seq;
	else {
		trans_seq->next = *transition_list;
		*transition_list = trans_seq;
	}

}

static void prepend_dev(struct dev_list **dev_list,
				struct dev_list *sdev)
{
	if (*dev_list == NULL)
		*dev_list = sdev;
	else {
		sdev->next = *dev_list;
		*dev_list = sdev;
	}

}

/*
 * Parse Modifier Use cases
 *
 *	# Each modifier is described in new section. N modifier are allowed
 *	SectionModifier
 *
 *		Name "Capture Voice"
 *		Comment "Record voice call"
 *		SupportedDevice "x"
 *		SupportedDevice "y"
 *
 *		EnableSequence
 *			....
 *		EndSequence
 *
 *		DisableSequence
 *			...
 *		EndSequence
 *
 *		# Optional QoS and ALSA PCMs
 *		QoS Voice
 *		CapturePCM 1
 *		MasterPlaybackVolume  'Device Master Playback Volume'
 *		MasterPlaybackSwitch  'Device Master Playback Switch'
 *
 *	 EndSection
 */
static int parse_modifier(snd_use_case_mgr_t *uc_mgr,
		struct use_case_verb *verb, FILE *f, int *line_, char *file)
{
	struct use_case_modifier *modifier;
	int line = *line_, end = 0, en_seq = 0, dis_seq = 0;
	int id = 0, dev = 0, ret;
	char buf[MAX_BUF], *tbuf;
	char *name = NULL, *comment = NULL;

	/* allocate modifier  */
	verb->modifier = realloc(verb->modifier,
		(verb->num_modifiers + 1) * sizeof(struct use_case_modifier));
	if (verb->modifier == NULL)
		return -ENOMEM;
	modifier = verb->modifier + verb->num_modifiers;
	bzero(modifier, sizeof(struct use_case_modifier));

	/* read line by line */
	while(fgets(buf, MAX_BUF, f) != NULL) {

		line++;

		/* skip comments */
		if (buf[0] == '#')
			continue;

		tbuf = buf;
		/* skip spaces */
		while (isblank(*tbuf))
			tbuf++;

		/* get use case modifier name */
		if (strncmp(tbuf, "Name", 4) == 0) {
			name = get_string(tbuf + 4);
			if (name == NULL) {
				uc_error("error: %s:%d failed to get modifier name",
						file, line);
				goto err;
			}
			modifier->name = name;
			id = 1;
			continue;
		}

		if (strncmp(tbuf, "Comment", 8) == 0) {
			name = get_string(tbuf + 8);
			if (name == NULL) {
				uc_error("error: %s:%d failed to get modifier comment",
						file, line);
				goto err;
			}
			modifier->comment = comment;
			continue;
		}

		if (strncmp(tbuf, "SupportedDevice", 15) == 0) {
			struct dev_list *sdev;
			name = get_string(tbuf + 15);
			if (name == NULL) {
				uc_error("error: %s:%d failed to get modifier"
					" supported device", file, line);
				goto err;
			}

			sdev = calloc(1, sizeof(struct dev_list));
			if (sdev == NULL)
				goto err;

			dev = 1;
			sdev->name = name;
			prepend_dev(&modifier->dev_list, sdev);
			continue;
		}

		if (strncmp(tbuf, "EnableSequence", 14) == 0) {
			ret = parse_sequence(uc_mgr, &modifier->enable, f,
				&line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse modifier"
					" enable sequence", file, line);
				goto err;
			}
			en_seq = 1;
			continue;
		}

		if (strncmp(tbuf, "DisableSequence", 15) == 0) {
			ret = parse_sequence(uc_mgr, &modifier->disable, f,
				&line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse modifier"
					" disable sequence", file, line);
				goto err;
			}
			dis_seq = 1;
			continue;
		}

		if (strncmp(tbuf, "TransitionModifier", 16) == 0) {
			struct transition_sequence *trans_seq;

			name = get_string(tbuf + 16);
			if (name == NULL)
				continue;

			trans_seq = calloc(1, sizeof(struct transition_sequence));
			if (trans_seq == NULL)
				return -ENOMEM;

			trans_seq->name = name;

			ret = parse_sequence(uc_mgr, &trans_seq->transition, f,
				&line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse transition"
					" modifier", file, line);
				goto err;
			}

			prepend_transition(&modifier->transition_list, trans_seq);
			continue;
		}

		if (strncmp(tbuf, "QoS", 3) == 0) {
			modifier->qos = get_enum(tbuf + 3);
			continue;
		}

		if (strncmp(tbuf, "CapturePCM", 3) == 0) {
			modifier->capture_pcm = atoi(tbuf + 3);
			if (modifier->capture_pcm < 0) {
				uc_error("error: %s:%d failed to get Capture PCM ID",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "PlaybackPCM", 3) == 0) {
			modifier->playback_pcm = atoi(tbuf + 3);
			if (modifier->playback_pcm < 0) {
				uc_error("error: %s:%d failed to get Playback PCM ID",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "MasterPlaybackVolume", 20) == 0) {
			modifier->playback_volume_id =
				get_control_name(tbuf + 20, line, file);
			if (modifier->playback_volume_id == NULL) {
				uc_error("error: %s:%d failed to get MasterPlaybackVolume",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "MasterPlaybackSwitch", 20) == 0) {
			modifier->playback_switch_id =
				get_control_name(tbuf + 20, line, file);
			if (modifier->playback_switch_id == NULL) {
				uc_error("error: %s:%d failed to get MasterPlaybackSwitch",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "MasterCaptureVolume", 19) == 0) {
			modifier->capture_volume_id =
				get_control_name(tbuf + 19, line, file);
			if (modifier->capture_volume_id == NULL) {
				uc_error("error: %s:%d failed to get MasterCaptureVolume",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "MasterCaptureSwitch", 19) == 0) {
			modifier->capture_switch_id =
				get_control_name(tbuf + 19, line, file);
			if (modifier->capture_switch_id == NULL) {
				uc_error("error: %s:%d failed to get MasterCaptureSwitch",
						file, line);
				goto err;
			}
			continue;
		}

		/* end of section ?*/
		if (strncmp(tbuf, "EndSection", 10) == 0) {
			end = 1;
			break;
		}
	}

	/* do we have the modifier basics ? */
	if (!en_seq || !dis_seq || !end || !dev) {
		uc_error("error: invalid modifier");
		if (!en_seq)
			uc_error("error: %s: modifier missing enable sequence",
					file);
		if (!dis_seq)
			uc_error("error: %s: modifier missing disable sequence",
					file);
		if (!dev)
			uc_error("error: %s: modifier missing supported device"
					" sequence", file);
		if (!end)
			uc_error("error: %s: modifier missing end", file);
		return -EINVAL;
	}

	verb->modifier_list[verb->num_modifiers++] = modifier->name;
	*line_ = line;
	return 0;

err:
	if (ferror(f)) {
		uc_error("error: %s: failed to read modifier master section",
			file);
		return ferror(f);
	}
	return -ENOMEM;
}

/*
 * Parse Device Use Cases
 *
 *# Each device is described in new section. N devices are allowed
 *SectionDevice
 *
 *	Name "Headphones"
 *	Comment "Headphones connected to 3.5mm jack"
 *	Index 0
 *
 *	EnableSequence
 *		....
 *	EndSequence
 *
 *	DisableSequence
 *		...
 *	EndSequence
 *
 *	#Optional control aliases
 *	MasterPlaybackVolume  'Device Master Playback Volume'
 *	MasterPlaybackSwitch  'Device Master Playback Switch'
 *
 * EndSection
 */
static int parse_device(snd_use_case_mgr_t *uc_mgr,
		struct use_case_verb *verb, FILE *f, int *line_, char *file)
{
	struct use_case_device *device;
	int line = *line_, end = 0, en_seq = 0, dis_seq = 0, id = 0, ret;
	char buf[MAX_BUF], *tbuf;
	char *name = NULL, *comment;

	/* allocate device  */
	verb->device = realloc(verb->device,
		(verb->num_devices + 1) * sizeof(struct use_case_device));
	if (verb->device == NULL)
		return -ENOMEM;
	device = verb->device + verb->num_devices;
	bzero(device, sizeof(struct use_case_device));

	/* read line by line */
	while(fgets(buf, MAX_BUF, f) != NULL) {

		line++;

		/* skip comments */
		if (buf[0] == '#')
			continue;

		tbuf = buf;
		/* skip spaces */
		while (isblank(*tbuf))
			tbuf++;

		/* get use case device name */
		if (strncmp(tbuf, "Name", 4) == 0) {
			name = get_string(tbuf + 4);
			if (name == NULL) {
				uc_error("error: %s:%d failed to get device name",
						file, line);
				goto err;
			}
			device->name = name;
			id = 1;
			continue;
		}

		if (strncmp(tbuf, "Comment", 8) == 0) {
			comment = get_string(tbuf + 8);
			if (name == NULL) {
				uc_error("error: %s: %d failed to get device comment",
						file, line);
				goto err;
			}

			device->comment = comment;
			continue;
		}

		if (strncmp(tbuf, "Index", 5) == 0) {
			device->idx = atoi(tbuf + 5);
			if (device->idx < 0) {
				uc_error("error: %s:%d failed to get device index",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "EnableSequence", 14) == 0) {
			ret = parse_sequence(uc_mgr, &device->enable, f,
					&line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse device enable"
					" sequence", file, line);
				goto err;
			}
			en_seq = 1;
			continue;
		}

		if (strncmp(tbuf, "DisableSequence", 15) == 0) {
			uc_dbg("DisableSequence");
			ret = parse_sequence(uc_mgr, &device->disable, f,
					&line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse device disable sequence",
						file, line);
				goto err;
			}
			dis_seq = 1;
			continue;
		}

		if (strncmp(tbuf, "TransitionDevice", 14) == 0) {
			struct transition_sequence *trans_seq;

			name = get_string(tbuf + 14);
			if (name == NULL)
				continue;

			uc_dbg("TransitionDevice %s", name);

			trans_seq = calloc(1, sizeof(struct transition_sequence));
			if (trans_seq == NULL)
				return -ENOMEM;

			trans_seq->name = name;

			ret = parse_sequence(uc_mgr, &trans_seq->transition, f,
					&line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse transition"
					" device", file, line);
				goto err;
			}

			prepend_transition(&device->transition_list, trans_seq);
			continue;
		}

		if (strncmp(tbuf, "MasterPlaybackVolume", 20) == 0) {
			device->playback_volume_id =
				get_control_name(tbuf + 20, line, file);
			if (device->playback_volume_id == NULL) {
				uc_error("error: %s:%d failed to get MasterPlaybackVolume",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "MasterPlaybackSwitch", 20) == 0) {
			device->playback_switch_id =
				get_control_name(tbuf + 20, line, file);
			if (device->playback_switch_id == NULL) {
				uc_error("error: %s:%d failed to get MasterPlaybackSwitch",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "MasterCaptureVolume", 19) == 0) {
			device->capture_volume_id =
				get_control_name(tbuf + 19, line, file);
			if (device->capture_volume_id == NULL) {
				uc_error("error: %s:%d failed to get MasterCaptureVolume%d",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "MasterCaptureSwitch", 19) == 0) {
			device->capture_switch_id =
				get_control_name(tbuf + 19, line, file);
			if (device->capture_switch_id == NULL) {
				uc_error("error: %s:%d failed to get MasterCaptureSwitch",
						file, line);
				goto err;
			}
			continue;
		}

		/* end of section ?*/
		if (strncmp(tbuf, "EndSection", 10) == 0) {
			end = 1;
			break;
		}
	}

	/* do we have the basics for this device ? */
	if (!en_seq || !dis_seq || !end || !id) {
		uc_error("error: invalid device");
		if (!en_seq)
			uc_error("error: %s: device missing enable sequence", file);
		if (!dis_seq)
			uc_error("error: %s: device missing disable sequence", file);
		if (!end)
			uc_error("error: %s: device missing end", file);
		return -EINVAL;
	}

	verb->device_list[verb->num_devices++] = device->name;
	*line_ = line;
	return 0;

err:
	if (ferror(f)) {
		uc_error("%s: failed to read device section", file);
		return ferror(f);
	}
	return -ENOMEM;
}

/*
 * Parse Verb Section
 *
 * # Example Use case verb section for Voice call blah
 * # By Joe Blogs <joe@blogs.com>
 *
 * SectionVerb
 *	# enable and disable sequences are compulsory
 *	EnableSequence
 *		'Master Playback Switch':2:0,0
 *		'Master Playback Volume':2:25,25
 *		msleep 50
 *		'Master Playback Switch':2:1,1
 *		'Master Playback Volume':2:50,50
 *	EndSequence
 *
 *	DisableSequence
 *		'Master Playback Switch':2:0,0
 *		'Master Playback Volume':2:25,25
 *		msleep 50
 *		'Master Playback Switch':2:1,1
 *		'Master Playback Volume':2:50,50
 *	EndSequence
 *
 *	# Optional QoS and ALSA PCMs
 *	QoS HiFi
 *	CapturePCM 0
 *	PlaybackPCM 0
 *
 * EndSection
 */
static int parse_verb(snd_use_case_mgr_t *uc_mgr,
		struct use_case_verb *verb, FILE *f, int *line_, char *file)
{
	int line = *line_, end = 0, en_seq = 0, dis_seq = 0, ret;
	char buf[MAX_BUF], *tbuf;

	/* read line by line */
	while(fgets(buf, MAX_BUF, f) != NULL) {

		line++;

		/* skip comments */
		if (buf[0] == '#')
			continue;

		tbuf = buf;
		/* skip spaces */
		while (isblank(*tbuf))
			tbuf++;

		if (strncmp(tbuf, "EnableSequence", 14) == 0) {
			uc_dbg("Parse EnableSequence");
			ret = parse_sequence(uc_mgr, &verb->enable, f, &line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse verb enable sequence",
						file, line);
				goto err;
			}
			en_seq = 1;
			continue;
		}

		if (strncmp(tbuf, "DisableSequence", 15) == 0) {
			uc_dbg("Parse DisableSequence");
			ret = parse_sequence(uc_mgr, &verb->disable, f, &line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse verb disable sequence",
						file, line);
				goto err;
			}
			dis_seq = 1;
			continue;
		}

		if (strncmp(tbuf, "TransitionVerb", 12) == 0) {
			struct transition_sequence *trans_seq;
			char *name;

			name = get_string(tbuf + 12);
			if (name == NULL)
				continue;

			uc_dbg("TransitionVerb %s", name);

			trans_seq = calloc(1, sizeof(struct transition_sequence));
			if (trans_seq == NULL)
				return -ENOMEM;

			trans_seq->name = name;

			ret = parse_sequence(uc_mgr, &trans_seq->transition, f,
				&line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse transition verb",
						file, line);
				goto err;
			}

			prepend_transition(&verb->transition_list, trans_seq);
			continue;
		}

		if (strncmp(tbuf, "QoS", 3) == 0) {
			uc_dbg("Parse Qos");
			verb->qos = get_enum(tbuf + 3);
			continue;
		}

		if (strncmp(tbuf, "CapturePCM", 3) == 0) {
			uc_dbg("Parse CapTurePCM");
			verb->capture_pcm = atoi(tbuf + 3);
			if (verb->capture_pcm < 0) {
				uc_error("error: %s:%d failed to get Capture PCM ID",
						file, line);
				goto err;
			}
			continue;
		}

		if (strncmp(tbuf, "PlaybackPCM", 3) == 0) {
			uc_dbg("Parse PlaybackPCM");
			verb->playback_pcm = atoi(tbuf + 3);
			if (verb->playback_pcm < 0) {
				uc_error("error: %s: %d failed to get Playback PCM ID",
						file, line);
				goto err;
			}
			continue;
		}

		/* end of section ?*/
		if (strncmp(tbuf, "EndSection", 10) == 0) {
			end = 1;
			break;
		}
	}

	/* do we have both use case name and file ? */
	if (!en_seq || !dis_seq || !end) {
			uc_error("error: invalid verb");
		if (!en_seq)
			uc_error("error: %s: verb missing enable sequence", file);
		if (!dis_seq)
			uc_error("error: %s: verb missing disable sequence", file);
		if (!end)
			uc_error("error: %s: verb missing end", file);
		return -EINVAL;
	}

	*line_ = line;
	return 0;

err:
	if (ferror(f)) {
		uc_error("error: failed to read verb master section");
		return ferror(f);
	}
	return -ENOMEM;
}

/*
 * Parse a Use case verb file.
 *
 * This file contains the following :-
 *  o Verb enable and disable sequences.
 *  o Supported Device enable and disable sequences for verb.
 *  o Supported Modifier enable and disable sequences for verb
 *  o Optional QoS for the verb and modifiers.
 *  o Optional PCM device ID for verb and modifiers
 *  o Alias kcontrols IDs for master and volumes and mutes.
 */
static int parse_verb_file(snd_use_case_mgr_t *uc_mgr,
			char *use_case_name, char *file)
{
	struct use_case_verb *verb;
	FILE *f;
	int line = 0, ret = 0, dev_idx = 0, verb_idx = 0, mod_idx = 0;
	char buf[MAX_BUF], *tbuf;
	char filename[MAX_FILE];

	/* allocate verb */
	uc_mgr->verb = realloc(uc_mgr->verb,
		(uc_mgr->num_verbs + 1) * sizeof(struct use_case_verb));
	if (uc_mgr->verb == NULL)
		return -ENOMEM;
	verb = uc_mgr->verb +uc_mgr->num_verbs;
	bzero(verb, sizeof(struct use_case_verb));

	/* open Verb file for reading */
	sprintf(filename, "%s/%s/%s", ALSA_USE_CASE_DIR,
		uc_mgr->card_name, file);

	f = fopen(filename, "r");
	if (f == NULL) {
		uc_error("error: failed to open verb file %s : %d",
			filename, -errno);
		return -errno;
	}

	/* read line by line */
	while(fgets(buf, MAX_BUF, f) != NULL) {

		line++;

		/* skip comments */
		if (buf[0] == '#')
			continue;

		/* skip leading spaces */
		tbuf = buf;
		while (isblank(*tbuf))
			tbuf++;

		/* find verb section and parse it */
		if (strncmp(tbuf, "SectionVerb", 11) == 0) {
			ret = parse_verb(uc_mgr, verb, f, &line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse verb %s",
					file, line, use_case_name);
				goto err;
			}
			verb_idx++;
			continue;
		}

		/* find device sections and parse them */
		if (strncmp(tbuf, "SectionDevice", 13) == 0) {

			if (verb->num_devices >= MAX_DEVICE) {
				uc_error("error: %s:%d verb number of devices %d"
					"exceeds max %d", file, line,
					verb->num_devices, MAX_DEVICE);
				goto err;
			}
			ret = parse_device(uc_mgr, verb, f, &line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse device",
						file, line);
				goto err;
			}
			dev_idx++;
			continue;
		}

		/* find modifier sections and parse them */
		if (strncmp(tbuf, "SectionModifier", 15) == 0) {
			if (verb->num_modifiers >= MAX_MODIFIER) {
				uc_error("error: %s:%d verb number of modifiers %d"
					" exceeds max %d", file, line,
					verb->num_modifiers, MAX_MODIFIER);
				goto err;
			}

			ret = parse_modifier(uc_mgr, verb, f, &line, file);
			if (ret < 0) {
				uc_error("error: %s:%d failed to parse modifier",
						file, line);
				goto err;
			}
			mod_idx++;
			continue;
		}
	}

	/* use case verb must have at least verb and 1 device */
	if (verb_idx && dev_idx) {
		uc_mgr->verb_list[uc_mgr->num_verbs++] = use_case_name;
		verb->name = use_case_name;
	}
	else {
		uc_error("error: failed to parse use case %s", file);
		if (verb_idx == 0)
			uc_error("error: no use case verb defined", file);
		if (dev_idx == 0)
			uc_error("error: no use case device defined", file);
		ret = -EINVAL;
	}

err:
	fclose(f);
	return ret;
}

/*
 * Parse master section for "Use Case" and "File" tags.
 */
static int parse_master_section(snd_use_case_mgr_t *uc_mgr, FILE *f,
			int *line_)
{
	int line = *line_ - 1, end = 0;
	char buf[MAX_BUF], *tbuf;
	char *file = NULL, *use_case_name = NULL, *comment;

	/* read line by line */
	while(fgets(buf, MAX_BUF, f) != NULL) {

		line++;

		/* skip comments */
		if (buf[0] == '#')
			continue;

		uc_dbg("%s", buf);

		/* skip leading spaces */
		tbuf = buf;
		while (isblank(*tbuf))
			tbuf++;

		/* get use case name */
		if (strncmp(tbuf, "Use Case", 8) == 0) {
			use_case_name = get_string(tbuf + 8);
			if (use_case_name == NULL) {
				uc_error("error: failed to get Use Case at line %d",
						line);
				goto err;
			}

			if (uc_mgr->num_verbs >= MAX_VERB) {
				uc_error("error: verb number exceed max %d",
								uc_mgr->num_verbs, MAX_VERB);
				goto err;
			}
			continue;
		}

		/* get use case verb file name */
		if (strncmp(tbuf, "File", 4) == 0) {
			file = get_string(tbuf + 4);
			if (file == NULL) {
				uc_error("error: failed to get File at line %d", line);
				goto err;
			}
			continue;
		}

		/* get optional use case comment */
		if (strncmp(tbuf, "Comment", 7) == 0) {
			comment = get_string(tbuf + 7);
			if (comment == NULL) {
				uc_error("error: failed to get Comment at line %d",
						line);
				goto err;
			}
			continue;
		}

		/* end of section ?*/
		if (strncmp(tbuf, "EndSectionUseCase", 10) == 0) {
			end = 1;
			break;
		}
	}

	uc_dbg("use_case_name %s file %s end %d", use_case_name, file, end);

	/* do we have both use case name and file ? */
	if (!use_case_name || !file || !end) {
		uc_error("error: failed to find use case\n");
		if (!use_case_name)
			uc_error("error: use case missing name");
		if (!file)
			uc_error("error: use case missing file");
		if (!end)
			uc_error("error: use case missing end");
		return -EINVAL;
	}

	*line_ = line;

	/* parse verb file */
	return parse_verb_file(uc_mgr, use_case_name, file);

err:
	if (ferror(f)) {
		uc_error("error: failed to read use case master section");
		return ferror(f);
	}
	return -ENOMEM;
}

/*
 * Each sound card has a master sound card file that lists all the supported
 * use case verbs for that sound card. i.e.
 *
 * #Example master file for blah sound card
 * #By Joe Blogs <joe@bloggs.org>
 *
 * # The file is divided into Use case sections. One section per use case verb.
 *
 * SectionUseCase
 *		Use Case "Voice Call"
 *		File "voice_call_blah"
 *		Comment "Make a voice phone call."
 * EndSectionUseCase
 *
 * SectionUseCase
 *		Use Case "HiFi"
 *		File "hifi_blah"
 *		Comment "Play and record HiFi quality Music."
 * EndSectionUseCase
 *
 * # This file also stores the default sound card state.
 *
 * SectionDefaults
 *		'Master Playback Switch':2:1,1
 *		'Master Playback Volume':2:25,25
 *		'Master Mono Playback Switch':1:0
 *		'Master Mono Playback Volume':1:0
 *		'PCM Switch':2:1,1
 *		........
 * EndSectionDefaults
 *
 * # End of example file.
 */
static int parse_master_file(snd_use_case_mgr_t *uc_mgr, FILE *f,
		char *file)
{
	int line = 0, ret = 0;
	char buf[MAX_BUF], *tbuf;

	/* parse master config sections */
	while(fgets(buf, MAX_BUF, f) != NULL) {

		line++;

		/* ignore comments */
		if (buf[0] == '#') {
			line++;
			continue;
		}

		/* find use case section and parse it */
		if (strncmp(buf, "SectionUseCase", 14) == 0) {
			tbuf = buf + 14;
			while (isblank(*tbuf))
				tbuf++;
			ret = parse_master_section(uc_mgr, f, &line);
			if (ret < 0)
				goto err;
		}

		/* find default control values section and parse it */
		if (strncmp(buf, "SectionDefaults", 15) == 0) {
			tbuf = buf + 15;
			while (isblank(*tbuf))
				tbuf++;
			ret = parse_controls(uc_mgr, f, &line, file);
			if (ret < 0)
				goto err;
		}
	}

err:
	if (ferror(f)) {
		uc_error("error: %s: failed to read master file", file);
		return ferror(f);
	}
	return ret;
}

/* load master use case file for sound card */
static int import_master_config(snd_use_case_mgr_t *uc_mgr)
{
	int ret;
	FILE *f;
	char filename[MAX_FILE];

	sprintf(filename, "%s/%s/%s.conf", ALSA_USE_CASE_DIR,
		uc_mgr->card_name, uc_mgr->card_name);

	uc_dbg("master config file %s", filename);

	f = fopen(filename, "r");
	if (f == NULL) {
		uc_error("error: couldn't open %s configuration file %s",
				uc_mgr->card_name, filename);
		return -errno;
	}

	ret = parse_master_file(uc_mgr, f, uc_mgr->card_name);
	fclose(f);
	return ret;
}

static int parse_card_controls(snd_use_case_mgr_t *uc_mgr)
{
	struct control_settings *control;
	int i, ret = 0;
	snd_ctl_elem_id_t *id;

	/* allocate memory for controls */
	uc_mgr->control = calloc(uc_mgr->count,
		sizeof(struct control_settings));
	if (uc_mgr->control == NULL) {
		uc_error("error: not enough memory to store controls.\n");
		return -ENOMEM;
	}
	control = uc_mgr->control;
	snd_ctl_elem_id_alloca(&id);

	/* iterate through each kcontrol and add to manager */
	for (i = 0; i < uc_mgr->count; ++i) {

		snd_ctl_elem_list_get_id(uc_mgr->list, i, id);
		ret = add_control(uc_mgr->handle, id, control++);
		if (ret < 0) {
			uc_error("error: failed to add control error %s\n",
				__func__, snd_strerror(ret));
			break;
		}
	}

	return ret;
}

static int import_use_case_files(snd_use_case_mgr_t *uc_mgr)
{
	int ret;

	/* import the master file and default kcontrol settings */
	ret = import_master_config(uc_mgr);
	if (ret < 0) {
		uc_error("error: failed to parse master use case config %s\n",
				uc_mgr->card_name);
		return ret;
	}

	return 0;
}


static void free_sequence(struct sequence_element *sequence)
{
	struct sequence_element *element = sequence;

	while (element) {
		struct sequence_element *pre_element;

		if (element->control) {
			free(element->control->value);
			free(element->control);
		}
		pre_element = element;
		element = element->next;
		free(pre_element);
	}
}

static void free_transition_sequence_element(struct transition_sequence *trans)
{
	free(trans->name);
	free_sequence(trans->transition);
	free(trans);
}

static void free_transition_sequence(struct transition_sequence *transition_list)
{
	struct transition_sequence *last, *trans_sequence = transition_list;

	while (trans_sequence) {
		last = trans_sequence;
		trans_sequence =  trans_sequence->next;
		free_transition_sequence_element(last);
	}
}

static void free_modifier(struct use_case_modifier *modifier)
{
	if (!modifier)
		return;

	free(modifier->name);
	free(modifier->comment);
	free(modifier->playback_volume_id);
	free(modifier->playback_switch_id);
	free(modifier->capture_volume_id);
	free(modifier->capture_switch_id);

	free_sequence(modifier->enable);
	free_sequence(modifier->disable);
	free_transition_sequence(modifier->transition_list);
}

static void free_device(struct use_case_device *device)
{
	if (!device)
		return;

	free(device->name);
	free(device->comment);
	free(device->playback_volume_id);
	free(device->playback_switch_id);
	free(device->capture_volume_id);
	free(device->capture_switch_id);

	free_sequence(device->enable);
	free_sequence(device->disable);
	free_transition_sequence(device->transition_list);
}

static void free_devices(struct use_case_device *devices, int num_devices)
{
	int i;

	if (!devices)
		return;

	for (i = 0; i< num_devices; i++)
		free_device(devices + i);

	free(devices);
}

static void free_modifiers(struct use_case_modifier *modifiers,
						int num_modifiers)
{
	int i;

	if (!modifiers)
		return;

	for (i = 0; i < num_modifiers; i++)
		free_modifier(modifiers + i);

	free(modifiers);
}

static void free_verb(struct use_case_verb *verb)
{
	if (!verb)
		return;

	free(verb->name);
	free(verb->comment);

	free_sequence(verb->enable);
	free_sequence(verb->disable);
	free_transition_sequence(verb->transition_list);

	free_devices(verb->device, verb->num_devices);
	free_modifiers(verb->modifier, verb->num_modifiers);
}

static void free_verbs(struct use_case_verb *verbs, int num_verbs)
{
	int i;

	if (!verbs)
		return;

	for (i = 0; i < num_verbs; i++)
		free_verb(verbs + i);

	free(verbs);
}

static void free_controls(struct control_settings *controls, int num_controls)
{
	int i = 0;

	if (!controls)
		return;

	for(i = 0; i < num_controls; i++){
		struct control_settings *control = controls + i;
		free(control->value);
	}
	free(controls);
}

/*
 * Free all use case manager resources.
 * Callers holds locks.
 */
static void free_uc_mgr(snd_use_case_mgr_t *uc_mgr)
{
	if (uc_mgr == NULL)
		return;

	if (uc_mgr->info)
		snd_ctl_card_info_free(uc_mgr->info);
	if (uc_mgr->list)
		snd_ctl_elem_list_free(uc_mgr->list);
	if (uc_mgr->id)
		snd_ctl_elem_id_free(uc_mgr->id);

	if (uc_mgr->handle)
		snd_ctl_close(uc_mgr->handle);

	free(uc_mgr->card_name);
	free(uc_mgr->ctl_name);

	free_verbs(uc_mgr->verb, uc_mgr->num_verbs);

	free_controls(uc_mgr->control, uc_mgr->count);

	pthread_mutex_destroy(&uc_mgr->mutex);

	free(uc_mgr);
}

 /**
 * \brief Init sound card use case manager.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
snd_use_case_mgr_t *snd_use_case_mgr_open(const char *card_name)
{
	snd_use_case_mgr_t *uc_mgr;
	char ctl_name[8];
	int err, idx;

	idx = snd_card_get_index(card_name);
	if (idx < 0) {
		uc_error("error: can't get sound card %s: %s",
				card_name, snd_strerror(idx));
		return NULL;
	}

	/* create a new UCM */
	uc_mgr = calloc(1, sizeof(snd_use_case_mgr_t));
	if (uc_mgr == NULL)
		return NULL;

	uc_mgr->card_name = strdup(card_name);
	if (uc_mgr->card_name == NULL) {
		free(uc_mgr);
		return NULL;
	}
	sprintf(ctl_name, "hw:%d", idx);
	uc_mgr->ctl_name = strdup(ctl_name);
	if (uc_mgr->ctl_name == NULL) {
		free_uc_mgr(uc_mgr);
		return NULL;
	}
	if (snd_ctl_card_info_malloc(&uc_mgr->info) < 0) {
		free_uc_mgr(uc_mgr);
		return NULL;
	}
	if (snd_ctl_elem_list_malloc(&uc_mgr->list) < 0) {
		free_uc_mgr(uc_mgr);
		return NULL;
	}
	if (snd_ctl_elem_id_malloc(&uc_mgr->id) < 0) {
		free_uc_mgr(uc_mgr);
		return NULL;
	}

	/* open and init CTLs */
	err = snd_ctl_open(&uc_mgr->handle, uc_mgr->ctl_name, 0);
	if (err) {
		uc_error("error: can't open sound card %s: %s",
			uc_mgr->card_name, snd_strerror(err));
		goto err;
	}

	err = snd_ctl_card_info(uc_mgr->handle, uc_mgr->info);
	if (err < 0) {
		uc_error("error: can't get sound card %s control info: %s",
			uc_mgr->card_name, snd_strerror(err));
		goto err;
	}

	err = snd_ctl_elem_list(uc_mgr->handle, uc_mgr->list);
	if (err < 0) {
		uc_error("error: can't get sound card %s elements: %s",
			uc_mgr->card_name, snd_strerror(err));
		goto err;
	}

	uc_mgr->count = snd_ctl_elem_list_get_count(uc_mgr->list);
	if (uc_mgr->count < 0) {
		uc_error("error: can't get sound card %s controls %s:",
				uc_mgr->card_name, snd_strerror(err));
		goto err;
	}

	snd_ctl_elem_list_set_offset(uc_mgr->list, 0);
	if (snd_ctl_elem_list_alloc_space(uc_mgr->list, uc_mgr->count) < 0) {
		uc_error("error: could not allocate elements for %s",
			uc_mgr->card_name);
		goto err;
	}

	err = snd_ctl_elem_list(uc_mgr->handle, uc_mgr->list);
	if (err < 0) {
		uc_error("error: could not get elements for %s : %s",
			uc_mgr->card_name, snd_strerror(err));
		goto err;
	}

	/* get info about sound card */
	err = parse_card_controls(uc_mgr);
	if (err < 0) {
		uc_error("error: failed to parse sound device %s controls %d",
			card_name, err);
		goto err;
	}

	/* get info on use_cases and verify against card */
	err = import_use_case_files(uc_mgr);
	if (err < 0) {
		uc_error("error: failed to import %s use case configuration %d",
			card_name, err);
		goto err;
	}

	pthread_mutex_init(&uc_mgr->mutex, NULL);
	uc_mgr->card.current_verb = VERB_NOT_INITIALISED;

	return uc_mgr;

err:
	free_uc_mgr(uc_mgr);
	return NULL;
}

 /**
 * \brief Reload and reparse all use case files.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_reload(snd_use_case_mgr_t *uc_mgr)
{
	int ret;

	pthread_mutex_lock(&uc_mgr->mutex);

	free_verbs(uc_mgr->verb, uc_mgr->num_verbs);
	free_controls(uc_mgr->control, uc_mgr->count);

	/* reload all sound card controls */
	ret = parse_card_controls(uc_mgr);
	if (ret <= 0) {
		uc_error("error: failed to reload sound card controls %d\n",
			ret);
		free_uc_mgr(uc_mgr);
		return -EINVAL;
	}

	/* reload all use cases */
	uc_mgr->num_verbs = import_use_case_files(uc_mgr);
	if (uc_mgr->num_verbs <= 0) {
		uc_error("error: failed to reload use cases\n");
		return -EINVAL;
	}

	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}

 /**
 * \brief Close use case manager.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_close(snd_use_case_mgr_t *uc_mgr)
{
	free_uc_mgr(uc_mgr);

	return 0;
}

 /**
 * \brief Reset sound card controls to default values.
 * \param uc_mgr Use case manager
 * \return zero on success, otherwise a negative error code
 */
int snd_use_case_mgr_reset(snd_use_case_mgr_t *uc_mgr)
{
	int ret = 0, i;
	struct control_settings *control;

	pthread_mutex_lock(&uc_mgr->mutex);

	for (i = 0; i < uc_mgr->count; i++) {
		control = &uc_mgr->control[i];

		/* Only set default value specified in master config file */
		if (control->value == NULL)
			continue;

		ret = set_control_default(uc_mgr, control);
		if (ret < 0)
			goto out;
	}
	uc_mgr->card.current_verb = VERB_NOT_INITIALISED;
out:
	pthread_mutex_unlock(&uc_mgr->mutex);
	return ret;
}


/*
 * Change a sound card control to a new value.
 */
static int set_control(snd_ctl_t *handle, snd_ctl_elem_id_t *id,
	snd_use_case_mgr_t *uc_mgr, unsigned short value[])
{
	struct control_settings *setting;
	int ret, count, i;
	unsigned int idnum;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_type_t type;
	snd_ctl_elem_value_t *control;

	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_value_alloca(&control);

	snd_ctl_elem_info_set_id(info, id);
	ret = snd_ctl_elem_info(handle, info);
	if (ret < 0) {
		uc_error("error: failed to get ctl elem info %d: ",
			snd_strerror(ret));
		return ret;
	}

	snd_ctl_elem_value_set_id(control, id);
	snd_ctl_elem_read(handle, control);

	idnum = snd_ctl_elem_id_get_numid(id);
	for (i = 0; i < uc_mgr->count; i++) {
		setting = &uc_mgr->control[i];
		if (setting->id == idnum)
			goto set_val;
	}
	uc_error("error: failed to find control at id %d", idnum);
	return 0;

set_val:
	uc_dbg("set control %s id %d count %d", setting->name, setting->id,
			setting->count);

	type = snd_ctl_elem_info_get_type(info);
	count = snd_ctl_elem_info_get_count(info);
	if (count == 0)
		return 0;

	uc_dbg("type %d count %d", type, count);

	switch (type) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		for (i = 0; i < count; i++) {
			uc_dbg("count %d value %u", i, *(value + i));
			snd_ctl_elem_value_set_boolean(control, i, value[i]);
		}
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		uc_dbg("int");
		for (i = 0; i < count; i++) {
			uc_dbg("count %d value %u", i, value[i]);
			snd_ctl_elem_value_set_integer(control, i, value[i]);
		}
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		uc_dbg("int64");
		for (i = 0; i < count; i++) {
			uc_dbg("count %d value %u", i, value[i]);
			snd_ctl_elem_value_set_integer64(control, i, value[i]);
		}

		break;
	case SND_CTL_ELEM_TYPE_ENUMERATED:
		uc_dbg("enumerated");
		for (i = 0; i < count; i++) {
			uc_dbg("count %d value %u", i, value[i]);
			snd_ctl_elem_value_set_enumerated(control, i, value[i]);
		}
		break;
	case SND_CTL_ELEM_TYPE_BYTES:
		uc_dbg("bytes");
		for (i = 0; i < count; i++) {
			uc_dbg("count %d value %u", i, value[i]);
			snd_ctl_elem_value_set_byte(control, i, value[i]);
		}
		break;
	default:
		break;
	}

	ret = snd_ctl_elem_write(handle, control);
	if (ret < 0) {
		uc_error("error: failed to set control %s: %s",
			 setting->name, snd_strerror(ret));
		uc_error("error: count %d type: %d",
			count, type);
		for (i = 0; i < count; i++)
			fprintf(stderr, "%d ", get_value(setting, i));
		return ret;
	}
	return 0;
}

/*
 * Execute a sequence of control writes.
 */
static int exec_sequence(struct sequence_element *seq, snd_use_case_mgr_t
			*uc_mgr, snd_ctl_elem_list_t *list, snd_ctl_t *handle)
{
	int count = snd_ctl_elem_list_get_count(list);
	int ret, i;

	uc_dbg("");

	/* keep going until end of sequence */
	while (seq) {
		/* do we need to sleep */
		if (seq->sleep) {
			uc_dbg("msleep %d", seq->sleep);
			usleep(seq->sleep);
		} else {
			uc_dbg("control name %s, id %d, count %d, vale[1] %u",
				seq->control->name, seq->control->id,
				seq->control->count, seq->control->value[0]);

			/* control write */
			snd_ctl_elem_id_t *id;
			snd_ctl_elem_id_alloca(&id);
			unsigned int numid;

			/* Where is id lookup from numid if you need it? */
			for (i = 0; i < count; ++i) {

				snd_ctl_elem_list_get_id(list, i, id);
				numid = snd_ctl_elem_id_get_numid(id);

				if (numid == seq->control->id) {
					ret = set_control(handle, id, uc_mgr, seq->control->value);
					if (ret < 0) {
						uc_error("error: failed to set control %s",
							__func__, uc_mgr->card_name);
						return ret;
					}
					break;
				}
			}
		}
		seq = seq->next;
	}
	return 0;
}

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
