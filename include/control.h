/****************************************************************************
 *                                                                          *
 *                              control.h                                   *
 *                          Control Interface                               *
 *                                                                          *
 ****************************************************************************/

typedef struct snd_ctl snd_ctl_t;

typedef struct snd_ctl_callbacks {
	void *private_data;	/* may be used by an application */
	void (*rebuild) (snd_ctl_t *handle, void *private_data);
	void (*value) (snd_ctl_t *handle, void *private_data, snd_control_id_t * id);
	void (*change) (snd_ctl_t *handle, void *private_data, snd_control_id_t * id);
	void (*add) (snd_ctl_t *handle, void *private_data, snd_control_id_t * id);
	void (*remove) (snd_ctl_t *handle, void *private_data, snd_control_id_t * id);
	void *reserved[58];	/* reserved for the future use - must be NULL!!! */
} snd_ctl_callbacks_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_card_load(int card);
int snd_cards(void);
unsigned int snd_cards_mask(void);
int snd_card_name(const char *name);
int snd_card_get_name(int card, char **name);
int snd_card_get_longname(int card, char **name);

int snd_defaults_card(void);
int snd_defaults_mixer_card(void);
int snd_defaults_pcm_card(void);
int snd_defaults_pcm_device(void);
int snd_defaults_rawmidi_card(void);
int snd_defaults_rawmidi_device(void);

int snd_ctl_open(snd_ctl_t **handle, int card);
int snd_ctl_close(snd_ctl_t *handle);
int snd_ctl_file_descriptor(snd_ctl_t *handle);
int snd_ctl_hw_info(snd_ctl_t *handle, struct snd_ctl_hw_info *info);
int snd_ctl_clist(snd_ctl_t *handle, snd_control_list_t * list);
int snd_ctl_cinfo(snd_ctl_t *handle, snd_control_info_t * sw);
int snd_ctl_cread(snd_ctl_t *handle, snd_control_t * control);
int snd_ctl_cwrite(snd_ctl_t *handle, snd_control_t * control);
int snd_ctl_hwdep_info(snd_ctl_t *handle, snd_hwdep_info_t * info);
int snd_ctl_pcm_info(snd_ctl_t *handle, snd_pcm_info_t * info);
int snd_ctl_pcm_prefer_subdevice(snd_ctl_t *handle, int subdev);
int snd_ctl_rawmidi_info(snd_ctl_t *handle, snd_rawmidi_info_t * info);

int snd_ctl_read(snd_ctl_t *handle, snd_ctl_callbacks_t * callbacks);

/*
 *  Highlevel API for controls
 */

typedef struct snd_hcontrol_stru snd_hcontrol_t;

struct snd_hcontrol_stru {
	snd_control_id_t id; 	/* must be always on top */
	int change: 1,		/* structure change */
	    value: 1;		/* value change */
	/* event callbacks */
	void (*event_change)(snd_ctl_t *handle, snd_hcontrol_t *hcontrol);
	void (*event_value)(snd_ctl_t *handle, snd_hcontrol_t *hcontrol);
	void (*event_remove)(snd_ctl_t *handle, snd_hcontrol_t *hcontrol);
	/* private data */
	void *private_data;
	void (*private_free)(void *private_data);
	/* links */
	snd_ctl_t *handle;	/* associated handle */
};

typedef int (snd_ctl_csort_t)(const snd_hcontrol_t *c1, const snd_hcontrol_t *c2);
typedef int (snd_ctl_ccallback_rebuild_t)(snd_ctl_t *handle, void *private_data);
typedef int (snd_ctl_ccallback_add_t)(snd_ctl_t *handle, void *private_data, snd_hcontrol_t *hcontrol);

int snd_ctl_cbuild(snd_ctl_t *handle, snd_ctl_csort_t *csort);
int snd_ctl_cfree(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_cfind(snd_ctl_t *handle, snd_control_id_t *id);
int snd_ctl_csort(const snd_hcontrol_t *c1, const snd_hcontrol_t *c2);
int snd_ctl_cresort(snd_ctl_t *handle, snd_ctl_csort_t *csort);
int snd_ctl_ccallback_rebuild(snd_ctl_t *handle, snd_ctl_ccallback_rebuild_t *callback, void *private_data);
int snd_ctl_ccallback_add(snd_ctl_t *handle, snd_ctl_ccallback_add_t *callback, void *private_data);
int snd_ctl_cevent(snd_ctl_t *handle);

#ifdef __cplusplus
}
#endif

