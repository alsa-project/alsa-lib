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



#if 0
#define UC_MGR_DEBUG
#endif

#include "local.h"
#include <pthread.h>
#include "use-case.h"

#define SYNTAX_VERSION_MAX	2

#define MAX_CARD_SHORT_NAME	32
#define MAX_CARD_LONG_NAME	80

#define SEQUENCE_ELEMENT_TYPE_CDEV	1
#define SEQUENCE_ELEMENT_TYPE_CSET	2
#define SEQUENCE_ELEMENT_TYPE_SLEEP	3
#define SEQUENCE_ELEMENT_TYPE_EXEC	4
#define SEQUENCE_ELEMENT_TYPE_CSET_BIN_FILE	5
#define SEQUENCE_ELEMENT_TYPE_CSET_TLV	6
#define SEQUENCE_ELEMENT_TYPE_CMPT_SEQ	7

struct ucm_value {
        struct list_head list;
        char *name;
        char *data;
};

/* sequence of a component device */
struct component_sequence {
	struct use_case_device *device; /* component device */
	int enable; /* flag to choose enable or disable list of the device */
};

struct sequence_element {
	struct list_head list;
	unsigned int type;
	union {
		long sleep; /* Sleep time in microseconds if sleep element, else 0 */
		char *cdev;
		char *cset;
		char *exec;
		struct component_sequence cmpt_seq; /* component sequence */
	} data;
};

/*
 * Transition sequences. i.e. transition between one verb, device, mod to another
 */
struct transition_sequence {
	struct list_head list;
	char *name;
	struct list_head transition_list;
};

/*
 * Modifier Supported Devices.
 */
enum dev_list_type {
	DEVLIST_NONE,
	DEVLIST_SUPPORTED,
	DEVLIST_CONFLICTING
};

struct dev_list_node {
	struct list_head list;
	char *name;
};

struct dev_list {
	enum dev_list_type type;
	struct list_head list;
};

struct ctl_dev {
	struct list_head list;
	char *device;
};

struct ctl_list {
	struct list_head list;
	struct list_head dev_list;
	snd_ctl_t *ctl;
	snd_ctl_card_info_t *ctl_info;
};

struct ucm_dev_name {
	struct list_head list;
	char *name1;
	char *name2;
};

/*
 * Describes a Use Case Modifier and it's enable and disable sequences.
 * A use case verb can have N modifiers.
 */
struct use_case_modifier {
	struct list_head list;
	struct list_head active_list;

	char *name;
	char *comment;

	/* modifier enable and disable sequences */
	struct list_head enable_list;
	struct list_head disable_list;

	/* modifier transition list */
	struct list_head transition_list;

	/* list of devices supported or conflicting */
	struct dev_list dev_list;

	/* values */
	struct list_head value_list;
};

/*
 * Describes a Use Case Device and it's enable and disable sequences.
 * A use case verb can have N devices.
 */
struct use_case_device {
	struct list_head list;
	struct list_head active_list;

	char *name;
	char *comment;

	/* device enable and disable sequences */
	struct list_head enable_list;
	struct list_head disable_list;

	/* device transition list */
	struct list_head transition_list;

	/* list of devices supported or conflicting */
	struct dev_list dev_list;

	/* value list */
	struct list_head value_list;
};

/*
 * Describes a Use Case Verb and it's enable and disable sequences.
 * A use case verb can have N devices and N modifiers.
 */
struct use_case_verb {
	struct list_head list;

	unsigned int active: 1;

	char *name;
	char *comment;

	/* verb enable and disable sequences */
	struct list_head enable_list;
	struct list_head disable_list;

	/* verb transition list */
	struct list_head transition_list;

	struct list_head device_list;

	/* component device list */
	struct list_head cmpt_device_list;

	/* modifiers that can be used with this use case */
	struct list_head modifier_list;

	/* value list */
	struct list_head value_list;

	/* temporary modifications lists */
	struct list_head rename_list;
	struct list_head remove_list;
};

/*
 *  Manages a sound card and all its use cases.
 */
struct snd_use_case_mgr {
	char *card_name;
	char *conf_file_name;
	char *conf_dir_name;
	char *comment;
	int conf_format;

	/* use case verb, devices and modifier configs parsed from files */
	struct list_head verb_list;

	/* default settings - sequence */
	struct list_head default_list;

	/* default settings - value list */
	struct list_head value_list;

	/* current status */
	struct use_case_verb *active_verb;
	struct list_head active_devices;
	struct list_head active_modifiers;

	/* locking */
	pthread_mutex_t mutex;

	/* list of opened control devices */
	struct list_head ctl_list;

	/* Components don't define cdev, the card device. When executing
	 * a sequence of a component device, ucm manager enters component
	 * domain and needs to provide cdev to the component. This cdev
	 * should be defined by the machine, parent of the component.
	 */
	int in_component_domain;
	char *cdev;
};

#define uc_error SNDERR

#ifdef UC_MGR_DEBUG
#define uc_dbg SNDERR
#else
#define uc_dbg(fmt, arg...) do { } while (0)
#endif

void uc_mgr_error(const char *fmt, ...);
void uc_mgr_stdout(const char *fmt, ...);

int uc_mgr_config_load(int format, const char *file, snd_config_t **cfg);
int uc_mgr_import_master_config(snd_use_case_mgr_t *uc_mgr);
int uc_mgr_scan_master_configs(const char **_list[]);

int uc_mgr_put_to_dev_list(struct dev_list *dev_list, const char *name);
int uc_mgr_remove_device(struct use_case_verb *verb, const char *name);
int uc_mgr_rename_device(struct use_case_verb *verb, const char *src,
			 const char *dst);

void uc_mgr_free_dev_name_list(struct list_head *base);
void uc_mgr_free_sequence_element(struct sequence_element *seq);
void uc_mgr_free_transition_element(struct transition_sequence *seq);
void uc_mgr_free_verb(snd_use_case_mgr_t *uc_mgr);
void uc_mgr_free(snd_use_case_mgr_t *uc_mgr);

int uc_mgr_open_ctl(snd_use_case_mgr_t *uc_mgr,
                    snd_ctl_t **ctl,
                    const char *device);

struct ctl_list *uc_mgr_get_one_ctl(snd_use_case_mgr_t *uc_mgr);
snd_ctl_t *uc_mgr_get_ctl(snd_use_case_mgr_t *uc_mgr);
void uc_mgr_free_ctl_list(snd_use_case_mgr_t *uc_mgr);

int uc_mgr_add_value(struct list_head *base, const char *key, char *val);

int uc_mgr_get_substituted_value(snd_use_case_mgr_t *uc_mgr,
				 char **_rvalue,
				 const char *value);

int uc_mgr_evaluate_condition(snd_use_case_mgr_t *uc_mgr,
			      snd_config_t *parent,
			      snd_config_t *cond);

/** The name of the environment variable containing the UCM directory */
#define ALSA_CONFIG_UCM_VAR "ALSA_CONFIG_UCM"

/** The name of the environment variable containing the UCM directory (new syntax) */
#define ALSA_CONFIG_UCM2_VAR "ALSA_CONFIG_UCM2"
