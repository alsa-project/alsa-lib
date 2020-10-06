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
#include <dirent.h>
#include <limits.h>

/* Directories to store UCM configuration files for components, like
 * off-soc codecs or embedded DSPs. Components can define their own
 * devices and sequences, to be reused by sound cards/machines. UCM
 * manager should not scan these component directories.
 * Machine use case files can include component configratuation files
 * via alsaconf syntax:
 * <searchdir:component-directory-name> and <component-conf-file-name>.
 * Alsaconf will import the included files automatically. After including
 * a component file, a machine device's sequence can enable or disable
 * a component device via syntax:
 * enadev "component_device_name"
 * disdev "component_device_name"
 */
static const char * const component_dir[] = {
	"codecs",	/* for off-soc codecs */
	"dsps",		/* for DSPs embedded in SoC */
	"platforms",	/* for common platform implementations */
	NULL,		/* terminator */
};

static int filename_filter(const struct dirent *dirent);
static int is_component_directory(const char *dir);

static int parse_sequence(snd_use_case_mgr_t *uc_mgr,
			  struct list_head *base,
			  snd_config_t *cfg);

/*
 * compose the absolute ucm filename
 */
static void ucm_filename(char *fn, size_t fn_len, long version,
			  const char *dir, const char *file)
{
	const char *env = getenv(version > 1 ? ALSA_CONFIG_UCM2_VAR : ALSA_CONFIG_UCM_VAR);

	if (env == NULL)
		snprintf(fn, fn_len, "%s/%s/%s%s%s",
			 snd_config_topdir(), version > 1 ? "ucm2" : "ucm",
			 dir ?: "", dir ? "/" : "", file);
	else
		snprintf(fn, fn_len, "%s/%s%s%s",
			 env, dir ?: "", dir ? "/" : "", file);
}

/*
 *
 */
int uc_mgr_config_load_file(snd_use_case_mgr_t *uc_mgr,
			     const char *file, snd_config_t **cfg)
{
	char filename[PATH_MAX];
	int err;

	ucm_filename(filename, sizeof(filename), uc_mgr->conf_format,
		     file[0] == '/' ? NULL : uc_mgr->conf_dir_name,
		     file);
	err = uc_mgr_config_load(uc_mgr->conf_format, filename, cfg);
	if (err < 0) {
		uc_error("error: failed to open file %s : %d", filename, -errno);
		return err;
	}
	return 0;
}

/*
 * Replace mallocated string
 */
static char *replace_string(char **dst, const char *value)
{
	free(*dst);
	*dst = value ? strdup(value) : NULL;
	return *dst;
}

/*
 * Parse string
 */
int parse_string(snd_config_t *n, char **res)
{
	int err;

	err = snd_config_get_string(n, (const char **)res);
	if (err < 0)
		return err;
	*res = strdup(*res);
	if (*res == NULL)
		return -ENOMEM;
	return 0;
}

/*
 * Parse string and substitute
 */
int parse_string_substitute(snd_use_case_mgr_t *uc_mgr,
			    snd_config_t *n, char **res)
{
	const char *str;
	char *s;
	int err;

	err = snd_config_get_string(n, &str);
	if (err < 0)
		return err;
	err = uc_mgr_get_substituted_value(uc_mgr, &s, str);
	if (err >= 0)
		*res = s;
	return err;
}

/*
 * Parse string and substitute
 */
int parse_string_substitute3(snd_use_case_mgr_t *uc_mgr,
			     snd_config_t *n, char **res)
{
	if (uc_mgr->conf_format < 3)
		return parse_string(n, res);
	return parse_string_substitute(uc_mgr, n, res);
}

/*
 * Parse integer with substitution
 */
int parse_integer_substitute(snd_use_case_mgr_t *uc_mgr,
			     snd_config_t *n, long *res)
{
	char *s1, *s2;
	int err;

	err = snd_config_get_ascii(n, &s1);
	if (err < 0)
		return err;
	err = uc_mgr_get_substituted_value(uc_mgr, &s2, s1);
	if (err >= 0)
		err = safe_strtol(s2, res);
	free(s2);
	free(s1);
	return err;
}

/*
 * Parse integer with substitution
 */
int parse_integer_substitute3(snd_use_case_mgr_t *uc_mgr,
			      snd_config_t *n, long *res)
{
	char *s1, *s2;
	int err;

	err = snd_config_get_ascii(n, &s1);
	if (err < 0)
		return err;
	if (uc_mgr->conf_format < 3)
		s2 = s1;
	else
		err = uc_mgr_get_substituted_value(uc_mgr, &s2, s1);
	if (err >= 0)
		err = safe_strtol(s2, res);
	if (s1 != s2)
		free(s2);
	free(s1);
	return err;
}

/*
 * Parse safe ID
 */
int parse_is_name_safe(const char *name)
{
	if (strchr(name, '.')) {
		uc_error("char '.' not allowed in '%s'", name);
		return 0;
	}
	return 1;
}

int get_string3(snd_use_case_mgr_t *uc_mgr, const char *s1, char **s)
{
	if (uc_mgr->conf_format < 3) {
		*s = strdup(s1);
		if (*s == NULL)
			return -ENOMEM;
		return 0;
	}
	return uc_mgr_get_substituted_value(uc_mgr, s, s1);
}

int parse_get_safe_name(snd_use_case_mgr_t *uc_mgr, snd_config_t *n,
			const char *alt, char **name)
{
	const char *id;
	int err;

	if (alt) {
		id = alt;
	} else {
		err = snd_config_get_id(n, &id);
		if (err < 0)
			return err;
	}
	err = get_string3(uc_mgr, id, name);
	if (err < 0)
		return err;
	if (!parse_is_name_safe(*name)) {
		free(*name);
		return -EINVAL;
	}
	return 0;
}

/*
 * Handle 'Error' configuration node.
 */
static int error_node(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg)
{
	int err;
	char *s;

	err = parse_string_substitute3(uc_mgr, cfg, &s);
	if (err < 0) {
		uc_error("error: failed to get Error string");
		return err;
	}
	uc_error("%s", s);
	free(s);
	return -ENXIO;
}

/*
 * Evaluate variable regex definitions (in-place delete)
 */
static int evaluate_regex(snd_use_case_mgr_t *uc_mgr,
			  snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *d, *n;
	const char *id;
	int err;

	err = snd_config_search(cfg, "DefineRegex", &d);
	if (err == -ENOENT)
		return 1;
	if (err < 0)
		return err;

	if (snd_config_get_type(d) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for DefineRegex");
		return -EINVAL;
	}

	if (uc_mgr->conf_format < 3) {
		uc_error("DefineRegex is supported in v3+ syntax");
		return -EINVAL;
	}

	snd_config_for_each(i, next, d) {
		n = snd_config_iterator_entry(i);
		err = snd_config_get_id(n, &id);
		if (err < 0)
			return err;
		err = uc_mgr_define_regex(uc_mgr, id, n);
		if (err < 0)
			return err;
	}

	snd_config_delete(d);
	return 0;
}

/*
 * Evaluate variable definitions (in-place delete)
 */
static int evaluate_define(snd_use_case_mgr_t *uc_mgr,
			   snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *d, *n;
	const char *id;
	char *var, *s;
	int err;

	err = snd_config_search(cfg, "Define", &d);
	if (err == -ENOENT)
		return evaluate_regex(uc_mgr, cfg);
	if (err < 0)
		return err;

	if (snd_config_get_type(d) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for Define");
		return -EINVAL;
	}

	if (uc_mgr->conf_format < 3) {
		uc_error("Define is supported in v3+ syntax");
		return -EINVAL;
	}

	snd_config_for_each(i, next, d) {
		n = snd_config_iterator_entry(i);
		err = snd_config_get_id(n, &id);
		if (err < 0)
			return err;
		err = snd_config_get_ascii(n, &var);
		if (err < 0)
			return err;
		err = uc_mgr_get_substituted_value(uc_mgr, &s, var);
		free(var);
		if (err < 0)
			return err;
		uc_mgr_set_variable(uc_mgr, id, s);
		free(s);
	}

	snd_config_delete(d);

	return evaluate_regex(uc_mgr, cfg);
}

/*
 * Evaluate include (in-place)
 */
static int evaluate_include(snd_use_case_mgr_t *uc_mgr,
			    snd_config_t *cfg)
{
	snd_config_t *n;
	int err;

	err = snd_config_search(cfg, "Include", &n);
	if (err == -ENOENT)
		return 1;
	if (err < 0)
		return err;

	err = uc_mgr_evaluate_include(uc_mgr, cfg, n);
	snd_config_delete(n);
	return err;
}

/*
 * Evaluate condition (in-place)
 */
static int evaluate_condition(snd_use_case_mgr_t *uc_mgr,
			      snd_config_t *cfg)
{
	snd_config_t *n;
	int err;

	err = snd_config_search(cfg, "If", &n);
	if (err == -ENOENT)
		return 1;
	if (err < 0)
		return err;

	err = uc_mgr_evaluate_condition(uc_mgr, cfg, n);
	snd_config_delete(n);
	return err;
}

/*
 * In-place evaluate
 */
int uc_mgr_evaluate_inplace(snd_use_case_mgr_t *uc_mgr,
			    snd_config_t *cfg)
{
	int err1 = 0, err2 = 0, err3 = 0;

	while (err1 == 0 || err2 == 0 || err3 == 0) {
		/* variables at first */
		err1 = evaluate_define(uc_mgr, cfg);
		if (err1 < 0)
			return err1;
		/* include at second */
		err2 = evaluate_include(uc_mgr, cfg);
		if (err2 < 0)
			return err2;
		/* include may define another variables */
		/* conditions may depend on them */
		if (err2 == 0)
			continue;
		err3 = evaluate_condition(uc_mgr, cfg);
		if (err3 < 0)
			return err3;
	}
	return 0;
}

/*
 * Parse transition
 */
static int parse_transition(snd_use_case_mgr_t *uc_mgr,
			    struct list_head *tlist,
			    snd_config_t *cfg)
{
	struct transition_sequence *tseq;
	const char *id;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for %s", id);
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			return -EINVAL;

		tseq = calloc(1, sizeof(*tseq));
		if (tseq == NULL)
			return -ENOMEM;
		INIT_LIST_HEAD(&tseq->transition_list);

		err = get_string3(uc_mgr, id, &tseq->name);
		if (err < 0) {
			free(tseq);
			return err;
		}
	
		err = parse_sequence(uc_mgr, &tseq->transition_list, n);
		if (err < 0) {
			uc_mgr_free_transition_element(tseq);
			return err;
		}

		list_add(&tseq->list, tlist);
	}
	return 0;
}

/*
 * Parse compound
 */
static int parse_compound(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg,
	  int (*fcn)(snd_use_case_mgr_t *, snd_config_t *, void *, void *),
	  void *data1, void *data2)
{
	const char *id;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;
	
	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for %s", id);
		return -EINVAL;
	}
	/* parse compound */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
			uc_error("compound type expected for %s, is %d", id, snd_config_get_type(cfg));
			return -EINVAL;
		}
		
		err = fcn(uc_mgr, n, data1, data2);
		if (err < 0)
			return err;
	}

	return 0;
}

static int strip_legacy_dev_index(char *name)
{
	char *dot = strchr(name, '.');
	if (!dot)
		return 0;
	if (dot[1] != '0' || dot[2] != '\0') {
		uc_error("device name %s contains a '.',"
			 " and is not legacy foo.0 format", name);
		return -EINVAL;
	}
	*dot = '\0';
	return 0;
}

/*
 * Parse device list
 */
static int parse_device_list(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
			     struct dev_list *dev_list,
			     enum dev_list_type type,
			     snd_config_t *cfg)
{
	struct dev_list_node *sdev;
	const char *id;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (dev_list->type != DEVLIST_NONE) {
		uc_error("error: multiple supported or"
			" conflicting device lists");
		return -EEXIST;
	}

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for %s", id);
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			return -EINVAL;

		sdev = calloc(1, sizeof(struct dev_list_node));
		if (sdev == NULL)
			return -ENOMEM;
		err = parse_string_substitute3(uc_mgr, n, &sdev->name);
		if (err < 0) {
			free(sdev);
			return err;
		}
		err = strip_legacy_dev_index(sdev->name);
		if (err < 0) {
			free(sdev->name);
			free(sdev);
			return err;
		}
		list_add(&sdev->list, &dev_list->list);
	}

	dev_list->type = type;

	return 0;
}

/* Find a component device by its name, and remove it from machine device
 * list.
 *
 * Component devices are defined by machine components (usually off-soc
 * codes or DSP embeded in SoC). Since alsaconf imports their configuration
 * files automatically, we don't know which devices are component devices
 * until they are referenced by a machine device sequence. So here when we
 * find a referenced device, we move it from the machine device list to the
 * component device list. Component devices will not be exposed to applications
 * by the original API to list devices for backward compatibility. So sound
 * servers can only see the machine devices.
 */
struct use_case_device *find_component_dev(snd_use_case_mgr_t *uc_mgr,
	const char *name)
{
	struct list_head *pos, *posdev, *_posdev;
	struct use_case_verb *verb;
	struct use_case_device *dev;

	list_for_each(pos, &uc_mgr->verb_list) {
		verb = list_entry(pos, struct use_case_verb, list);

		/* search in the component device list */
		list_for_each(posdev, &verb->cmpt_device_list) {
			dev = list_entry(posdev, struct use_case_device, list);
			if (!strcmp(dev->name, name))
				return dev;
		}

		/* search the machine device list */
		list_for_each_safe(posdev, _posdev, &verb->device_list) {
			dev = list_entry(posdev, struct use_case_device, list);
			if (!strcmp(dev->name, name)) {
				/* find the component device, move it from the
				 * machine device list to the component device
				 * list.
				 */
				list_del(&dev->list);
				list_add_tail(&dev->list,
					      &verb->cmpt_device_list);
				return dev;
			}
		}
	}

	return NULL;
}

/* parse sequence of a component device
 *
 * This function will find the component device and mark if its enable or
 * disable sequence is needed by its parenet device.
 */
static int parse_component_seq(snd_use_case_mgr_t *uc_mgr,
			       snd_config_t *n, int enable,
			       struct component_sequence *cmpt_seq)
{
	char *val;
	int err;

	err = parse_string_substitute3(uc_mgr, n, &val);
	if (err < 0)
		return err;

	cmpt_seq->device = find_component_dev(uc_mgr, val);
	if (!cmpt_seq->device) {
		uc_error("error: Cannot find component device %s", val);
		free(val);
		return -EINVAL;
	}
	free(val);

	/* Parent needs its enable or disable sequence */
	cmpt_seq->enable = enable;

	return 0;
}

/*
 * Parse sequences.
 *
 * Sequence controls elements  are in the following form:-
 *
 * cdev "hw:0"
 * cset "element_id_syntax value_syntax"
 * usleep time
 * exec "any unix command with arguments"
 * enadev "component device name"
 * disdev "component device name"
 *
 * e.g.
 *	cset "name='Master Playback Switch' 0,0"
 *      cset "iface=PCM,name='Disable HDMI',index=1 0"
 *	enadev "rt286:Headphones"
 *	disdev "rt286:Speaker"
 */
static int parse_sequence(snd_use_case_mgr_t *uc_mgr,
			  struct list_head *base,
			  snd_config_t *cfg)
{
	struct sequence_element *curr;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err, idx = 0;
	const char *cmd = NULL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("error: compound is expected for sequence definition");
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		const char *id;
		idx ^= 1;
		n = snd_config_iterator_entry(i);
		err = snd_config_get_id(n, &id);
		if (err < 0)
			continue;
		if (idx == 1) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_STRING) {
				uc_error("error: string type is expected for sequence command");
				return -EINVAL;
			}
			snd_config_get_string(n, &cmd);
			continue;
		}

		/* alloc new sequence element */
		curr = calloc(1, sizeof(struct sequence_element));
		if (curr == NULL)
			return -ENOMEM;
		list_add_tail(&curr->list, base);

		if (strcmp(cmd, "cdev") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_CDEV;
			err = parse_string_substitute3(uc_mgr, n, &curr->data.cdev);
			if (err < 0) {
				uc_error("error: cdev requires a string!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "cset") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_CSET;
			err = parse_string_substitute3(uc_mgr, n, &curr->data.cset);
			if (err < 0) {
				uc_error("error: cset requires a string!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "enadev") == 0) {
			/* need to enable a component device */
			curr->type = SEQUENCE_ELEMENT_TYPE_CMPT_SEQ;
			err = parse_component_seq(uc_mgr, n, 1,
						&curr->data.cmpt_seq);
			if (err < 0) {
				uc_error("error: enadev requires a valid device!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "disdev") == 0) {
			/* need to disable a component device */
			curr->type = SEQUENCE_ELEMENT_TYPE_CMPT_SEQ;
			err = parse_component_seq(uc_mgr, n, 0,
						&curr->data.cmpt_seq);
			if (err < 0) {
				uc_error("error: disdev requires a valid device!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "cset-bin-file") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_CSET_BIN_FILE;
			err = parse_string_substitute3(uc_mgr, n, &curr->data.cset);
			if (err < 0) {
				uc_error("error: cset-bin-file requires a string!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "cset-tlv") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_CSET_TLV;
			err = parse_string_substitute3(uc_mgr, n, &curr->data.cset);
			if (err < 0) {
				uc_error("error: cset-tlv requires a string!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "usleep") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_SLEEP;
			err = parse_integer_substitute3(uc_mgr, n, &curr->data.sleep);
			if (err < 0) {
				uc_error("error: usleep requires integer!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "msleep") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_SLEEP;
			err = parse_integer_substitute3(uc_mgr, n, &curr->data.sleep);
			if (err < 0) {
				uc_error("error: msleep requires integer!");
				return err;
			}
			curr->data.sleep *= 1000L;
			continue;
		}

		if (strcmp(cmd, "exec") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_EXEC;
			err = parse_string_substitute3(uc_mgr, n, &curr->data.exec);
			if (err < 0) {
				uc_error("error: exec requires a string!");
				return err;
			}
			continue;
		}
		
		list_del(&curr->list);
		uc_mgr_free_sequence_element(curr);
	}

	return 0;
}

/*
 *
 */
int uc_mgr_add_value(struct list_head *base, const char *key, char *val)
{
	struct ucm_value *curr;

	curr = calloc(1, sizeof(struct ucm_value));
	if (curr == NULL)
		return -ENOMEM;
	curr->name = strdup(key);
	if (curr->name == NULL) {
		free(curr);
		return -ENOMEM;
	}
	list_add_tail(&curr->list, base);
	curr->data = val;
	return 0;
}

/*
 * Parse values.
 *
 * Parse values describing PCM, control/mixer settings and stream parameters.
 *
 * Value {
 *   TQ Voice
 *   CapturePCM "hw:1"
 *   PlaybackVolume "name='Master Playback Volume',index=2"
 *   PlaybackSwitch "name='Master Playback Switch',index=2"
 * }
 */
static int parse_value(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
			  struct list_head *base,
			  snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	char *s;
	snd_config_type_t type;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("error: compound is expected for value definition");
		return -EINVAL;
	}

	/* in-place evaluation */
	err = uc_mgr_evaluate_inplace(uc_mgr, cfg);
	if (err < 0)
		return err;

	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		err = snd_config_get_id(n, &id);
		if (err < 0)
			continue;

		type = snd_config_get_type(n);
		switch (type) {
		case SND_CONFIG_TYPE_INTEGER:
		case SND_CONFIG_TYPE_INTEGER64:
		case SND_CONFIG_TYPE_REAL:
			err = snd_config_get_ascii(n, &s);
			if (err < 0) {
				uc_error("error: unable to parse value for id '%s': %s!", id, snd_strerror(err));
				return err;
			}
			break;
		case SND_CONFIG_TYPE_STRING:
			err = parse_string_substitute(uc_mgr, n, &s);
			if (err < 0) {
				uc_error("error: unable to parse a string for id '%s'!", id);
				return err;
			}
			break;
		default:
			uc_error("error: invalid type %i in Value compound '%s'", type, id);
			return -EINVAL;
		}
		err = uc_mgr_add_value(base, id, s);
		if (err < 0) {
			free(s);
			return err;
		}
	}

	return 0;
}

/*
 * Parse Modifier Use cases
 *
 * # Each modifier is described in new section. N modifiers are allowed
 * SectionModifier."Capture Voice" {
 *
 *	Comment "Record voice call"
 *
 *	SupportedDevice [
 *		"x"
 *		"y"
 *	]
 *
 *	ConflictingDevice [
 *		"x"
 *		"y"
 *	]
 *
 *	EnableSequence [
 *		....
 *	]
 *
 *	DisableSequence [
 *		...
 *	]
 *
 *      TransitionSequence."ToModifierName" [
 *		...
 *	]
 *
 *	# Optional TQ and ALSA PCMs
 *	Value {
 *		TQ Voice
 *		CapturePCM "hw:1"
 *		PlaybackVolume "name='Master Playback Volume',index=2"
 *		PlaybackSwitch "name='Master Playback Switch',index=2"
 *	}
 * }
 *
 * SupportedDevice and ConflictingDevice cannot be specified together.
 * Both are optional.
 */
static int parse_modifier(snd_use_case_mgr_t *uc_mgr,
			  snd_config_t *cfg,
			  void *data1, void *data2)
{
	struct use_case_verb *verb = data1;
	struct use_case_modifier *modifier;
	char *name;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (parse_get_safe_name(uc_mgr, cfg, data2, &name) < 0)
		return -EINVAL;

	/* allocate modifier */
	modifier = calloc(1, sizeof(*modifier));
	if (modifier == NULL) {
		free(name);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&modifier->enable_list);
	INIT_LIST_HEAD(&modifier->disable_list);
	INIT_LIST_HEAD(&modifier->transition_list);
	INIT_LIST_HEAD(&modifier->dev_list.list);
	INIT_LIST_HEAD(&modifier->value_list);
	list_add_tail(&modifier->list, &verb->modifier_list);
	modifier->name = name;

	/* in-place evaluation */
	err = uc_mgr_evaluate_inplace(uc_mgr, cfg);
	if (err < 0)
		return err;

	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "Comment") == 0) {
			err = parse_string_substitute3(uc_mgr, n, &modifier->comment);
			if (err < 0) {
				uc_error("error: failed to get modifier comment");
				return err;
			}
			continue;
		}

		if (strcmp(id, "SupportedDevice") == 0) {
			err = parse_device_list(uc_mgr, &modifier->dev_list,
						DEVLIST_SUPPORTED, n);
			if (err < 0) {
				uc_error("error: failed to parse supported"
					" device list");
				return err;
			}
		}

		if (strcmp(id, "ConflictingDevice") == 0) {
			err = parse_device_list(uc_mgr, &modifier->dev_list,
						DEVLIST_CONFLICTING, n);
			if (err < 0) {
				uc_error("error: failed to parse conflicting"
					" device list");
				return err;
			}
		}

		if (strcmp(id, "EnableSequence") == 0) {
			err = parse_sequence(uc_mgr, &modifier->enable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse modifier"
					" enable sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "DisableSequence") == 0) {
			err = parse_sequence(uc_mgr, &modifier->disable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse modifier"
					" disable sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "TransitionSequence") == 0) {
			err = parse_transition(uc_mgr, &modifier->transition_list, n);
			if (err < 0) {
				uc_error("error: failed to parse transition"
					" modifier");
				return err;
			}
			continue;
		}

		if (strcmp(id, "Value") == 0) {
			err = parse_value(uc_mgr, &modifier->value_list, n);
			if (err < 0) {
				uc_error("error: failed to parse Value");
				return err;
			}
			continue;
		}
	}

	return 0;
}

/*
 * Parse Device Use Cases
 *
 * # Each device is described in new section. N devices are allowed
 * SectionDevice."Headphones" {
 *	Comment "Headphones connected to 3.5mm jack"
 *
 *	SupportedDevice [
 *		"x"
 *		"y"
 *	]
 *
 *	ConflictingDevice [
 *		"x"
 *		"y"
 *	]
 *
 *	EnableSequence [
 *		....
 *	]
 *
 *	DisableSequence [
 *		...
 *	]
 *
 *      TransitionSequence."ToDevice" [
 *		...
 *	]
 *
 *	Value {
 *		PlaybackVolume "name='Master Playback Volume',index=2"
 *		PlaybackSwitch "name='Master Playback Switch',index=2"
 *	}
 * }
 */
static int parse_device(snd_use_case_mgr_t *uc_mgr,
			snd_config_t *cfg,
			void *data1, void *data2)
{
	struct use_case_verb *verb = data1;
	char *name;
	struct use_case_device *device;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (parse_get_safe_name(uc_mgr, cfg, data2, &name) < 0)
		return -EINVAL;

	device = calloc(1, sizeof(*device));
	if (device == NULL) {
		free(name);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&device->enable_list);
	INIT_LIST_HEAD(&device->disable_list);
	INIT_LIST_HEAD(&device->transition_list);
	INIT_LIST_HEAD(&device->dev_list.list);
	INIT_LIST_HEAD(&device->value_list);
	list_add_tail(&device->list, &verb->device_list);
	device->name = name;

	/* in-place evaluation */
	err = uc_mgr_evaluate_inplace(uc_mgr, cfg);
	if (err < 0)
		return err;

	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "Comment") == 0) {
			err = parse_string_substitute3(uc_mgr, n, &device->comment);
			if (err < 0) {
				uc_error("error: failed to get device comment");
				return err;
			}
			continue;
		}

		if (strcmp(id, "SupportedDevice") == 0) {
			err = parse_device_list(uc_mgr, &device->dev_list,
						DEVLIST_SUPPORTED, n);
			if (err < 0) {
				uc_error("error: failed to parse supported"
					" device list");
				return err;
			}
		}

		if (strcmp(id, "ConflictingDevice") == 0) {
			err = parse_device_list(uc_mgr, &device->dev_list,
						DEVLIST_CONFLICTING, n);
			if (err < 0) {
				uc_error("error: failed to parse conflicting"
					" device list");
				return err;
			}
		}

		if (strcmp(id, "EnableSequence") == 0) {
			uc_dbg("EnableSequence");
			err = parse_sequence(uc_mgr, &device->enable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse device enable"
					 " sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "DisableSequence") == 0) {
			uc_dbg("DisableSequence");
			err = parse_sequence(uc_mgr, &device->disable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse device disable"
					 " sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "TransitionSequence") == 0) {
			uc_dbg("TransitionSequence");
			err = parse_transition(uc_mgr, &device->transition_list, n);
			if (err < 0) {
				uc_error("error: failed to parse transition"
					" device");
				return err;
			}
			continue;
		}

		if (strcmp(id, "Value") == 0) {
			err = parse_value(uc_mgr, &device->value_list, n);
			if (err < 0) {
				uc_error("error: failed to parse Value");
				return err;
			}
			continue;
		}
	}
	return 0;
}

/*
 * Parse Device Rename/Delete Command
 *
 * # The devices might be renamed to allow the better conditional runtime
 * # evaluation. Bellow example renames Speaker1 device to Speaker and
 * # removes Speaker2 device.
 * RenameDevice."Speaker1" "Speaker"
 * RemoveDevice."Speaker2" "Speaker2"
 */
static int parse_dev_name_list(snd_use_case_mgr_t *uc_mgr,
			       snd_config_t *cfg,
			       struct list_head *list)
{
	snd_config_t *n;
	snd_config_iterator_t i, next;
	const char *id, *name1;
	char *name1s, *name2;
	struct ucm_dev_name *dev;
	snd_config_iterator_t pos;
	int err;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for %s", id);
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &name1) < 0)
			return -EINVAL;

		err = get_string3(uc_mgr, name1, &name1s);
		if (err < 0)
			return err;

		err = parse_string_substitute3(uc_mgr, n, &name2);
		if (err < 0) {
			free(name1s);
			uc_error("error: failed to get target device name for '%s'", name1);
			return err;
		}

		/* skip duplicates */
		list_for_each(pos, list) {
			dev = list_entry(pos, struct ucm_dev_name, list);
			if (strcmp(dev->name1, name1s) == 0) {
				free(name2);
				free(name1s);
				return 0;
			}
		}

		free(name1s);

		dev = calloc(1, sizeof(*dev));
		if (dev == NULL) {
			free(name2);
			return -ENOMEM;
		}
		dev->name1 = strdup(name1);
		if (dev->name1 == NULL) {
			free(dev);
			free(name2);
			return -ENOMEM;
		}
		dev->name2 = name2;
		list_add_tail(&dev->list, list);
	}

	return 0;
}

static int parse_compound_check_legacy(snd_use_case_mgr_t *uc_mgr,
	  snd_config_t *cfg,
	  int (*fcn)(snd_use_case_mgr_t *, snd_config_t *, void *, void *),
	  void *data1)
{
	const char *id, *idchild;
	int child_ctr = 0, legacy_format = 1;
	snd_config_iterator_t i, next;
	snd_config_t *child;
	int err;

	err = snd_config_get_id(cfg, &id);
	if (err < 0)
		return err;

	snd_config_for_each(i, next, cfg) {
		child_ctr++;
		if (child_ctr > 1) {
			break;
		}

		child = snd_config_iterator_entry(i);

		if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
			legacy_format = 0;
			break;
		}

		if (snd_config_get_id(child, &idchild) < 0)
			return -EINVAL;

		if (strcmp(idchild, "0")) {
			legacy_format = 0;
			break;
		}
	}
	if (child_ctr != 1) {
		legacy_format = 0;
	}

	if (legacy_format)
		return parse_compound(uc_mgr, cfg, fcn, data1, (void *)id);
	else
		return fcn(uc_mgr, cfg, data1, NULL);
}

static int parse_device_name(snd_use_case_mgr_t *uc_mgr,
			     snd_config_t *cfg,
			     void *data1,
			     void *data2 ATTRIBUTE_UNUSED)
{
	return parse_compound_check_legacy(uc_mgr, cfg, parse_device, data1);
}

static int parse_modifier_name(snd_use_case_mgr_t *uc_mgr,
			     snd_config_t *cfg,
			     void *data1,
			     void *data2 ATTRIBUTE_UNUSED)
{
	return parse_compound(uc_mgr, cfg, parse_modifier, data1, data2);
}

static int verb_dev_list_add(struct use_case_verb *verb,
			     enum dev_list_type dst_type,
			     const char *dst,
			     const char *src)
{
	struct use_case_device *device;
	struct list_head *pos;

	list_for_each(pos, &verb->device_list) {
		device = list_entry(pos, struct use_case_device, list);
		if (strcmp(device->name, dst) != 0)
			continue;
		if (device->dev_list.type != dst_type) {
			if (list_empty(&device->dev_list.list)) {
				device->dev_list.type = dst_type;
			} else {
				uc_error("error: incompatible device list type ('%s', '%s')",
					 device->name, src);
				return -EINVAL;
			}
		}
		return uc_mgr_put_to_dev_list(&device->dev_list, src);
	}
	uc_error("error: unable to find device '%s'", dst);
	return -ENOENT;
}

static int verb_dev_list_check(struct use_case_verb *verb)
{
	struct list_head *pos, *pos2;
	struct use_case_device *device;
	struct dev_list_node *dlist;
	int err;

	list_for_each(pos, &verb->device_list) {
		device = list_entry(pos, struct use_case_device, list);
		list_for_each(pos2, &device->dev_list.list) {
			dlist = list_entry(pos2, struct dev_list_node, list);
			err = verb_dev_list_add(verb, device->dev_list.type,
						dlist->name, device->name);
			if (err < 0)
				return err;
		}
	}
	return 0;
}

static int verb_device_management(struct use_case_verb *verb)
{
	struct list_head *pos;
	struct ucm_dev_name *dev;
	int err;

	/* rename devices */
	list_for_each(pos, &verb->rename_list) {
		dev = list_entry(pos, struct ucm_dev_name, list);
		err = uc_mgr_rename_device(verb, dev->name1, dev->name2);
		if (err < 0) {
			uc_error("error: cannot rename device '%s' to '%s'", dev->name1, dev->name2);
			return err;
		}
	}

	/* remove devices */
	list_for_each(pos, &verb->remove_list) {
		dev = list_entry(pos, struct ucm_dev_name, list);
		err = uc_mgr_remove_device(verb, dev->name2);
		if (err < 0) {
			uc_error("error: cannot remove device '%s'", dev->name2);
			return err;
		}
	}

	/* those lists are no longer used */
	uc_mgr_free_dev_name_list(&verb->rename_list);
	uc_mgr_free_dev_name_list(&verb->remove_list);

	/* handle conflicting/supported lists */
	return verb_dev_list_check(verb);
}

/*
 * Parse Verb Section
 *
 * # Example Use case verb section for Voice call blah
 * # By Joe Blogs <joe@blogs.com>
 *
 * SectionVerb {
 *	# enable and disable sequences are compulsory
 *	EnableSequence [
 *		cset "name='Master Playback Switch',index=2 0,0"
 *		cset "name='Master Playback Volume',index=2 25,25"
 *		msleep 50
 *		cset "name='Master Playback Switch',index=2 1,1"
 *		cset "name='Master Playback Volume',index=2 50,50"
 *	]
 *
 *	DisableSequence [
 *		cset "name='Master Playback Switch',index=2 0,0"
 *		cset "name='Master Playback Volume',index=2 25,25"
 *		msleep 50
 *		cset "name='Master Playback Switch',index=2 1,1"
 *		cset "name='Master Playback Volume',index=2 50,50"
 *	]
 *
 *      # Optional transition verb
 *      TransitionSequence."ToCaseName" [
 *		msleep 1
 *      ]
 *
 *	# Optional TQ and ALSA PCMs
 *	Value {
 *		TQ HiFi
 *		CapturePCM "hw:0"
 *		PlaybackPCM "hw:0"
 *	}
 * }
 */
static int parse_verb(snd_use_case_mgr_t *uc_mgr,
		      struct use_case_verb *verb,
		      snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;
	
	/* in-place evaluation */
	err = uc_mgr_evaluate_inplace(uc_mgr, cfg);
	if (err < 0)
		return err;

	/* parse verb section */
	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "EnableSequence") == 0) {
			uc_dbg("Parse EnableSequence");
			err = parse_sequence(uc_mgr, &verb->enable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse verb enable sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "DisableSequence") == 0) {
			uc_dbg("Parse DisableSequence");
			err = parse_sequence(uc_mgr, &verb->disable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse verb disable sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "TransitionSequence") == 0) {
			uc_dbg("Parse TransitionSequence");
			err = parse_transition(uc_mgr, &verb->transition_list, n);
			if (err < 0) {
				uc_error("error: failed to parse transition sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "Value") == 0) {
			uc_dbg("Parse Value");
			err = parse_value(uc_mgr, &verb->value_list, n);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
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
			   const char *use_case_name,
			   const char *comment,
			   const char *file)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct use_case_verb *verb;
	snd_config_t *cfg;
	int err;

	/* allocate verb */
	verb = calloc(1, sizeof(struct use_case_verb));
	if (verb == NULL)
		return -ENOMEM;
	INIT_LIST_HEAD(&verb->enable_list);
	INIT_LIST_HEAD(&verb->disable_list);
	INIT_LIST_HEAD(&verb->transition_list);
	INIT_LIST_HEAD(&verb->device_list);
	INIT_LIST_HEAD(&verb->cmpt_device_list);
	INIT_LIST_HEAD(&verb->modifier_list);
	INIT_LIST_HEAD(&verb->value_list);
	INIT_LIST_HEAD(&verb->rename_list);
	INIT_LIST_HEAD(&verb->remove_list);
	list_add_tail(&verb->list, &uc_mgr->verb_list);
	if (use_case_name == NULL)
		return -EINVAL;
	verb->name = strdup(use_case_name);
	if (verb->name == NULL)
		return -ENOMEM;

	if (comment != NULL) {
		verb->comment = strdup(comment);
		if (verb->comment == NULL)
			return -ENOMEM;
	}

	/* open Verb file for reading */
	err = uc_mgr_config_load_file(uc_mgr, file, &cfg);
	if (err < 0)
		return err;

	/* in-place evaluation */
	err = uc_mgr_evaluate_inplace(uc_mgr, cfg);
	if (err < 0)
		return err;

	/* parse master config sections */
	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* find verb section and parse it */
		if (strcmp(id, "SectionVerb") == 0) {
			err = parse_verb(uc_mgr, verb, n);
			if (err < 0) {
				uc_error("error: %s failed to parse verb",
						file);
				goto _err;
			}
			continue;
		}

		/* find device sections and parse them */
		if (strcmp(id, "SectionDevice") == 0) {
			err = parse_compound(uc_mgr, n,
						parse_device_name, verb, NULL);
			if (err < 0) {
				uc_error("error: %s failed to parse device",
						file);
				goto _err;
			}
			continue;
		}

		/* find modifier sections and parse them */
		if (strcmp(id, "SectionModifier") == 0) {
			err = parse_compound(uc_mgr, n,
					     parse_modifier_name, verb, NULL);
			if (err < 0) {
				uc_error("error: %s failed to parse modifier",
						file);
				goto _err;
			}
			continue;
		}

		/* device renames */
		if (strcmp(id, "RenameDevice") == 0) {
			err = parse_dev_name_list(uc_mgr, n, &verb->rename_list);
			if (err < 0) {
				uc_error("error: %s failed to parse device rename",
						file);
				goto _err;
			}
		}

		/* device remove */
		if (strcmp(id, "RemoveDevice") == 0) {
			err = parse_dev_name_list(uc_mgr, n, &verb->remove_list);
			if (err < 0) {
				uc_error("error: %s failed to parse device remove",
						file);
				goto _err;
			}
		}
	}

	snd_config_delete(cfg);

	/* use case verb must have at least 1 device */
	if (list_empty(&verb->device_list)) {
		uc_error("error: no use case device defined", file);
		return -EINVAL;
	}

	/* do device rename and delete */
	err = verb_device_management(verb);
	if (err < 0) {
		uc_error("error: device management error in verb '%s'", verb->name);
		return err;
	}

	return 0;

       _err:
	snd_config_delete(cfg);
	return err;
}

/*
 * Parse master section for "Use Case" and "File" tags.
 */
static int parse_master_section(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg,
				void *data1 ATTRIBUTE_UNUSED,
				void *data2 ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	char *use_case_name, *file = NULL, *comment = NULL;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for use case section");
		return -EINVAL;
	}

	err = parse_get_safe_name(uc_mgr, cfg, NULL, &use_case_name);
	if (err < 0) {
		uc_error("unable to get name for use case section");
		return err;
	}

	/* in-place evaluation */
	err = uc_mgr_evaluate_inplace(uc_mgr, cfg);
	if (err < 0)
		goto __error;

	/* parse master config sections */
	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* get use case verb file name */
		if (strcmp(id, "File") == 0) {
			err = parse_string_substitute3(uc_mgr, n, &file);
			if (err < 0) {
				uc_error("failed to get File");
				goto __error;
			}
			continue;
		}

		/* get optional use case comment */
		if (strncmp(id, "Comment", 7) == 0) {
			err = parse_string_substitute3(uc_mgr, n, &comment);
			if (err < 0) {
				uc_error("error: failed to get Comment");
				goto __error;
			}
			continue;
		}

		uc_error("unknown field %s in master section");
	}

	uc_dbg("use_case_name %s file '%s'", use_case_name, file);

	/* do we have both use case name and file ? */
	if (!file) {
		uc_error("error: use case missing file");
		err = -EINVAL;
		goto __error;
	}

	/* parse verb file */
	err = parse_verb_file(uc_mgr, use_case_name, comment, file);

__error:
	free(use_case_name);
	free(file);
	free(comment);
	return err;
}

/*
 * parse controls which should be run only at initial boot
 */
static int parse_controls_boot(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg)
{
	int err;

	if (!list_empty(&uc_mgr->boot_list)) {
		uc_error("Boot list is not empty");
		return -EINVAL;
	}
	err = parse_sequence(uc_mgr, &uc_mgr->boot_list, cfg);
	if (err < 0) {
		uc_error("Unable to parse BootSequence");
		return err;
	}

	return 0;
}

/*
 * parse controls
 */
static int parse_controls(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg)
{
	int err;

	if (!list_empty(&uc_mgr->default_list)) {
		uc_error("Default list is not empty");
		return -EINVAL;
	}
	err = parse_sequence(uc_mgr, &uc_mgr->default_list, cfg);
	if (err < 0) {
		uc_error("Unable to parse SectionDefaults");
		return err;
	}

	return 0;
}

/*
 * Each sound card has a master sound card file that lists all the supported
 * use case verbs for that sound card. i.e.
 *
 * #Example master file for blah sound card
 * #By Joe Blogs <joe@bloggs.org>
 *
 * Comment "Nice Abstracted Soundcard"
 *
 * # The file is divided into Use case sections. One section per use case verb.
 *
 * SectionUseCase."Voice Call" {
 *	File "voice_call_blah"
 *	Comment "Make a voice phone call."
 * }
 *
 * SectionUseCase."HiFi" {
 *	File "hifi_blah"
 *	Comment "Play and record HiFi quality Music."
 * }
 *
 * # Define Value defaults
 *
 * ValueDefaults {
 *	PlaybackCTL "hw:CARD=0"
 *	CaptureCTL "hw:CARD=0"
 * }
 *
 * # The initial boot (run once) configuration.
 *
 * BootSequence [
 *      cset "name='Master Playback Switch',index=2 1,1"
 *	cset "name='Master Playback Volume',index=2 25,25"
 * ]
 *
 * # This file also stores the default sound card state.
 *
 * SectionDefaults [
 *	cset "name='Master Mono Playback',index=1 0"
 *	cset "name='Master Mono Playback Volume',index=1 0"
 *	cset "name='PCM Switch',index=2 1,1"
 *      exec "some binary here"
 *      msleep 50
 *	........
 * ]
 *
 * # End of example file.
 */
static int parse_master_file(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	long l;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for master file");
		return -EINVAL;
	}

	if (uc_mgr->conf_format >= 2) {
		err = snd_config_search(cfg, "Syntax", &n);
		if (err < 0) {
			uc_error("Syntax field not found in %s", uc_mgr->conf_file_name);
			return -EINVAL;
		}
		err = snd_config_get_integer(n, &l);
		if (err < 0) {
			uc_error("Syntax field is invalid in %s", uc_mgr->conf_file_name);
			return err;
		}
		if (l < 2 || l > SYNTAX_VERSION_MAX) {
			uc_error("Incompatible syntax %d in %s", l, uc_mgr->conf_file_name);
			return -EINVAL;
		}
		/* delete this field to avoid strcmp() call in the loop */
		snd_config_delete(n);
		uc_mgr->conf_format = l;
	}

	/* in-place evaluation */
	err = uc_mgr_evaluate_inplace(uc_mgr, cfg);
	if (err < 0)
		return err;

	/* parse master config sections */
	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "Comment") == 0) {
			err = parse_string_substitute3(uc_mgr, n, &uc_mgr->comment);
			if (err < 0) {
				uc_error("error: failed to get master comment");
				return err;
			}
			continue;
		}

		/* find use case section and parse it */
		if (strcmp(id, "SectionUseCase") == 0) {
			err = parse_compound(uc_mgr, n,
					     parse_master_section,
					     NULL, NULL);
			if (err < 0)
				return err;
			continue;
		}

		/* find default control values section (first boot only) */
		if (strcmp(id, "BootSequence") == 0) {
			err = parse_controls_boot(uc_mgr, n);
			if (err < 0)
				return err;
			continue;
		}

		/* find default control values section and parse it */
		if (strcmp(id, "SectionDefaults") == 0) {
			err = parse_controls(uc_mgr, n);
			if (err < 0)
				return err;
			continue;
		}

		/* get the default values */
		if (strcmp(id, "ValueDefaults") == 0) {
			err = parse_value(uc_mgr, &uc_mgr->value_list, n);
			if (err < 0) {
				uc_error("error: failed to parse ValueDefaults");
				return err;
			}
			continue;
		}

		/* error */
		if (strcmp(id, "Error") == 0)
			return error_node(uc_mgr, n);

		uc_error("uknown master file field %s", id);
	}
	return 0;
}

/* get the card info */
static int get_card_info(snd_use_case_mgr_t *mgr,
			 const char *ctl_name,
			 snd_ctl_card_info_t **info)
{
	struct ctl_list *ctl_list;
	int err;

	err = uc_mgr_open_ctl(mgr, &ctl_list, ctl_name, 0);
	if (err < 0)
		return err;

	if (info)
		*info = ctl_list->ctl_info;
	return err;
}

/* find the card in the local machine */
static int get_by_card_name(snd_use_case_mgr_t *mgr, const char *card_name)
{
	int card, err;
	snd_ctl_card_info_t *info;
	const char *_driver, *_name, *_long_name;

	snd_ctl_card_info_alloca(&info);

	card = -1;
	if (snd_card_next(&card) < 0 || card < 0) {
		uc_error("no soundcards found...");
		return -1;
	}

	while (card >= 0) {
		char name[32];

		/* clear the list, keep the only one CTL device */
		uc_mgr_free_ctl_list(mgr);

		sprintf(name, "hw:%d", card);
		err = get_card_info(mgr, name, &info);

		if (err == 0) {
			_driver = snd_ctl_card_info_get_driver(info);
			_name = snd_ctl_card_info_get_name(info);
			_long_name = snd_ctl_card_info_get_longname(info);
			if (!strcmp(card_name, _driver) ||
			    !strcmp(card_name, _name) ||
			    !strcmp(card_name, _long_name))
				return 0;
		}

		if (snd_card_next(&card) < 0) {
			uc_error("snd_card_next");
			break;
		}
	}

	uc_mgr_free_ctl_list(mgr);

	return -1;
}

/* set the driver name and long name by the card ctl name */
static inline int get_by_card(snd_use_case_mgr_t *mgr, const char *ctl_name)
{
	return get_card_info(mgr, ctl_name, NULL);
}

static int parse_toplevel_path(snd_use_case_mgr_t *uc_mgr,
			       char *filename,
			       snd_config_t *cfg)
{
	snd_config_iterator_t i, next, i2, next2;
	snd_config_t *n, *n2;
	const char *id;
	char *dir = NULL, *file = NULL, fn[PATH_MAX];
	long version;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for UseCasePath node");
		return -EINVAL;
	}

	/* parse use case path config sections */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
			uc_error("compound type expected for UseCasePath.something node");
			return -EINVAL;
		}

			if (snd_config_get_id(n, &id) < 0)
				continue;

		version = 2;

		/* parse use case path config sections */
		snd_config_for_each(i2, next2, n) {

			n2 = snd_config_iterator_entry(i2);
			if (snd_config_get_id(n2, &id) < 0)
				continue;

			if (strcmp(id, "Version") == 0) {
				err = parse_integer_substitute(uc_mgr, n2, &version);
				if (err < 0) {
					uc_error("unable to parse UcmDirectory");
					goto __error;
				}
				if (version < 1 || version > 2) {
					uc_error("Version must be 1 or 2");
					err = -EINVAL;
					goto __error;
				}
				continue;
			}

			if (strcmp(id, "Directory") == 0) {
				err = parse_string_substitute(uc_mgr, n2, &dir);
				if (err < 0) {
					uc_error("unable to parse Directory");
					goto __error;
				}
				continue;
			}

			if (strcmp(id, "File") == 0) {
				err = parse_string_substitute(uc_mgr, n2, &file);
				if (err < 0) {
					uc_error("unable to parse File");
					goto __error;
				}
				continue;
			}

			uc_error("unknown UseCasePath field %s", id);
		}

		if (dir == NULL) {
			uc_error("Directory is not defined in %s!", filename);
			goto __next;
		}
		if (file == NULL) {
			uc_error("File is not defined in %s!", filename);
			goto __next;
		}

		ucm_filename(fn, sizeof(fn), version, dir, file);
		if (access(fn, R_OK) == 0) {
			if (replace_string(&uc_mgr->conf_dir_name, dir) == NULL) {
				err = -ENOMEM;
				goto __error;
			}
			if (replace_string(&uc_mgr->conf_file_name, file) == NULL) {
				err = -ENOMEM;
				goto __error;
			}
			strncpy(filename, fn, PATH_MAX);
			uc_mgr->conf_format = version;
			goto __ok;
		}

__next:
		free(file);
		free(dir);
		dir = NULL;
		file = NULL;
	}

	err = -ENOENT;
	goto __error;

__ok:
	err = 0;
__error:
	free(file);
	free(dir);
	return err;
}

static int parse_toplevel_config(snd_use_case_mgr_t *uc_mgr,
				 char *filename,
				 snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	long l;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for toplevel file");
		return -EINVAL;
	}

	err = snd_config_search(cfg, "Syntax", &n);
	if (err < 0) {
		uc_error("Syntax field not found in %s", filename);
		return -EINVAL;
	}
	err = snd_config_get_integer(n, &l);
	if (err < 0) {
		uc_error("Syntax field is invalid in %s", filename);
		return err;
	}
	if (l < 2 || l > SYNTAX_VERSION_MAX) {
		uc_error("Incompatible syntax %d in %s", l, filename);
		return -EINVAL;
	}
	/* delete this field to avoid strcmp() call in the loop */
	snd_config_delete(n);
	uc_mgr->conf_format = l;

	/* in-place evaluation */
	err = uc_mgr_evaluate_inplace(uc_mgr, cfg);
	if (err < 0)
		return err;

	/* parse toplevel config sections */
	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "UseCasePath") == 0) {
			err = parse_toplevel_path(uc_mgr, filename, n);
			if (err == 0)
				return err;
			continue;
		}

		uc_error("uknown toplevel field %s", id);
	}

	return -ENOENT;
}

static int load_toplevel_config(snd_use_case_mgr_t *uc_mgr,
				snd_config_t **cfg)
{
	char filename[PATH_MAX];
	snd_config_t *tcfg;
	int err;

	ucm_filename(filename, sizeof(filename), 2, NULL, "ucm.conf");

	if (access(filename, R_OK) != 0) {
		uc_error("Unable to find the top-level configuration file '%s'.", filename);
		return -ENOENT;
	}

	err = uc_mgr_config_load(2, filename, &tcfg);
	if (err < 0)
		goto __error;

	/* filename is shared for function input and output! */
	err = parse_toplevel_config(uc_mgr, filename, tcfg);
	snd_config_delete(tcfg);
	if (err < 0)
		goto __error;

	err = uc_mgr_config_load(uc_mgr->conf_format, filename, cfg);
	if (err < 0) {
		uc_error("error: could not parse configuration for card %s",
				uc_mgr->card_name);
		goto __error;
	}

	return 0;

__error:
	return err;
}

/* load master use case file for sound card based on rules in ucm2/ucm.conf
 */
int uc_mgr_import_master_config(snd_use_case_mgr_t *uc_mgr)
{
	snd_config_t *cfg;
	const char *name;
	int err;

	name = uc_mgr->card_name;
	if (strncmp(name, "hw:", 3) == 0) {
		err = get_by_card(uc_mgr, name);
		if (err < 0) {
			uc_error("card '%s' is not valid", name);
			goto __error;
		}
	} else if (strncmp(name, "strict:", 7)) {
		/* do not handle the error here */
		/* we can refer the virtual UCM config */
		get_by_card_name(uc_mgr, name);
	}

	err = load_toplevel_config(uc_mgr, &cfg);
	if (err < 0)
		goto __error;

	err = parse_master_file(uc_mgr, cfg);
	snd_config_delete(cfg);
	if (err < 0) {
		uc_mgr_free_ctl_list(uc_mgr);
		uc_mgr_free_verb(uc_mgr);
	}

	return err;

__error:
	uc_mgr_free_ctl_list(uc_mgr);
	replace_string(&uc_mgr->conf_dir_name, NULL);
	return err;
}

static int filename_filter(const struct dirent *dirent)
{
	if (dirent == NULL)
		return 0;
	if (dirent->d_type == DT_DIR) {
		if (dirent->d_name[0] == '.') {
			if (dirent->d_name[1] == '\0')
				return 0;
			if (dirent->d_name[1] == '.' &&
			    dirent->d_name[2] == '\0')
				return 0;
		}
		return 1;
	}
	return 0;
}

/* whether input dir is a predefined component directory */
static int is_component_directory(const char *dir)
{
	int i = 0;

	while (component_dir[i]) {
		if (!strncmp(dir, component_dir[i], PATH_MAX))
			return 1;
		i++;
	};

	return 0;
}

/* scan all cards and comments
 *
 * Cards are defined by machines. Each card/machine installs its UCM
 * configuration files in a subdirectory with the same name as the sound
 * card under /usr/share/alsa/ucm2. This function will scan all the card
 * directories and skip the component directories defined in the array
 * component_dir.
 */
int uc_mgr_scan_master_configs(const char **_list[])
{
	char filename[PATH_MAX], dfl[PATH_MAX], fn[FILENAME_MAX];
	char *env = getenv(ALSA_CONFIG_UCM2_VAR);
	const char **list, *d_name;
	snd_config_t *cfg, *c;
	int i, j, cnt, err;
	long l;
	ssize_t ss;
	struct dirent **namelist;

	if (env)
		snprintf(filename, sizeof(filename), "%s", env);
	else
		snprintf(filename, sizeof(filename), "%s/ucm2",
			 snd_config_topdir());

#if defined(_GNU_SOURCE) && !defined(__NetBSD__) && !defined(__FreeBSD__) && !defined(__sun) && !defined(ANDROID)
#define SORTFUNC	versionsort
#else
#define SORTFUNC	alphasort
#endif
	err = scandir(filename, &namelist, filename_filter, SORTFUNC);
	if (err < 0) {
		err = -errno;
		uc_error("error: could not scan directory %s: %s",
				filename, strerror(-err));
		return err;
	}
	cnt = err;

	dfl[0] = '\0';
	if (strlen(filename) + 8 < sizeof(filename)) {
		strcat(filename, "/default");
		ss = readlink(filename, dfl, sizeof(dfl)-1);
		if (ss >= 0) {
			dfl[ss] = '\0';
			dfl[sizeof(dfl)-1] = '\0';
			if (dfl[0] && dfl[strlen(dfl)-1] == '/')
				dfl[strlen(dfl)-1] = '\0';
		} else {
			dfl[0] = '\0';
		}
	}

	list = calloc(1, cnt * 2 * sizeof(char *));
	if (list == NULL) {
		err = -ENOMEM;
		goto __err;
	}

	for (i = j = 0; i < cnt; i++) {

		d_name = namelist[i]->d_name;

		/* Skip the directories for component devices */
		if (is_component_directory(d_name))
			continue;

		snprintf(fn, sizeof(fn), "%s.conf", d_name);
		ucm_filename(filename, sizeof(filename), 2, d_name, fn);
		if (eaccess(filename, R_OK))
			continue;

		err = uc_mgr_config_load(2, filename, &cfg);
		if (err < 0)
			goto __err;
		err = snd_config_search(cfg, "Syntax", &c);
		if (err < 0) {
			uc_error("Syntax field not found in %s", d_name);
			snd_config_delete(cfg);
			continue;
		}
		err = snd_config_get_integer(c, &l);
		if (err < 0) {
			uc_error("Syntax field is invalid in %s", d_name);
			snd_config_delete(cfg);
			goto __err;
		}
		if (l < 2 || l > SYNTAX_VERSION_MAX) {
			uc_error("Incompatible syntax %d in %s", l, d_name);
			snd_config_delete(cfg);
			goto __err;
		}
		err = snd_config_search(cfg, "Comment", &c);
		if (err >= 0) {
			err = parse_string(c, (char **)&list[j+1]);
			if (err < 0) {
				snd_config_delete(cfg);
				goto __err;
			}
		}
		snd_config_delete(cfg);
		list[j] = strdup(d_name);
		if (list[j] == NULL) {
			err = -ENOMEM;
			goto __err;
		}
		if (strcmp(dfl, list[j]) == 0) {
			/* default to top */
			const char *save1 = list[j];
			const char *save2 = list[j + 1];
			memmove(list + 2, list, j * sizeof(char *));
			list[0] = save1;
			list[1] = save2;
		}
		j += 2;
	}
	err = j;

      __err:
	for (i = 0; i < cnt; i++) {
		free(namelist[i]);
		if (err < 0) {
			free((void *)list[i * 2]);
			free((void *)list[i * 2 + 1]);
		}
	}
	free(namelist);

	if (err >= 0) {
		*_list = list;
	} else {
		free(list);
	}

	return err;
}
