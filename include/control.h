/****************************************************************************
 *                                                                          *
 *                              control.h                                   *
 *                          Control Interface                               *
 *                                                                          *
 ****************************************************************************/

typedef struct snd_ctl snd_ctl_t;

typedef enum { SND_CTL_TYPE_HW,
	       SND_CTL_TYPE_SHM,
	       SND_CTL_TYPE_INET
 } snd_ctl_type_t;

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
int snd_card_get_index(const char *name);
int snd_card_get_name(int card, char **name);
int snd_card_get_longname(int card, char **name);

int snd_defaults_card(void);
int snd_defaults_mixer_card(void);
int snd_defaults_pcm_card(void);
int snd_defaults_pcm_device(void);
int snd_defaults_rawmidi_card(void);
int snd_defaults_rawmidi_device(void);

int snd_ctl_hw_open(snd_ctl_t **handle, char *name, int card);
int snd_ctl_shm_open(snd_ctl_t **handlep, char *name, char *socket, char *sname);
snd_ctl_type_t snd_ctl_type(snd_ctl_t *handle);
int snd_ctl_open(snd_ctl_t **handle, char *name);
int snd_ctl_close(snd_ctl_t *handle);
int snd_ctl_poll_descriptor(snd_ctl_t *handle);
int snd_ctl_hw_info(snd_ctl_t *handle, snd_ctl_hw_info_t *info);
int snd_ctl_clist(snd_ctl_t *handle, snd_control_list_t * list);
int snd_ctl_cinfo(snd_ctl_t *handle, snd_control_info_t * sw);
int snd_ctl_cread(snd_ctl_t *handle, snd_control_t * control);
int snd_ctl_cwrite(snd_ctl_t *handle, snd_control_t * control);
int snd_ctl_hwdep_info(snd_ctl_t *handle, snd_hwdep_info_t * info);
int snd_ctl_pcm_info(snd_ctl_t *handle, snd_pcm_info_t * info);
int snd_ctl_pcm_prefer_subdevice(snd_ctl_t *handle, int subdev);
int snd_ctl_rawmidi_info(snd_ctl_t *handle, snd_rawmidi_info_t * info);

int snd_ctl_read(snd_ctl_t *handle, snd_ctl_callbacks_t * callbacks);

#ifdef __cplusplus
}
#endif

/*
 *  Highlevel API for controls
 */

#define LIST_HEAD_IS_DEFINED
struct list_head {
        struct list_head *next, *prev;
};        

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


typedef struct snd_hcontrol_list_stru snd_hcontrol_list_t;
typedef struct snd_hcontrol_stru snd_hcontrol_t;

struct snd_hcontrol_list_stru {
	unsigned int controls_offset;	/* W: first control ID to get */
	unsigned int controls_request;	/* W: count of control IDs to get */
	unsigned int controls_count;	/* R: count of available (set) controls */
	unsigned int controls;		/* R: count of all available controls */
	snd_control_id_t *pids;		/* W: IDs */
};

struct snd_hcontrol_stru {
	snd_control_id_t id; 	/* must be always on top */
	struct list_head list;	/* links for list of all hcontrols */
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

#ifdef __cplusplus
extern "C" {
#endif

typedef int (snd_ctl_hsort_t)(const snd_hcontrol_t *c1, const snd_hcontrol_t *c2);
typedef void (snd_ctl_hcallback_rebuild_t)(snd_ctl_t *handle, void *private_data);
typedef void (snd_ctl_hcallback_add_t)(snd_ctl_t *handle, void *private_data, snd_hcontrol_t *hcontrol);

int snd_ctl_hbuild(snd_ctl_t *handle, snd_ctl_hsort_t *csort);
int snd_ctl_hfree(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_hfirst(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_hlast(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_hnext(snd_ctl_t *handle, snd_hcontrol_t *hcontrol);
snd_hcontrol_t *snd_ctl_hprev(snd_ctl_t *handle, snd_hcontrol_t *hcontrol);
int snd_ctl_hcount(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_hfind(snd_ctl_t *handle, snd_control_id_t *id);
int snd_ctl_hlist(snd_ctl_t *handle, snd_hcontrol_list_t *hlist);
int snd_ctl_hsort(const snd_hcontrol_t *c1, const snd_hcontrol_t *c2);
int snd_ctl_hresort(snd_ctl_t *handle, snd_ctl_hsort_t *csort);
int snd_ctl_hcallback_rebuild(snd_ctl_t *handle, snd_ctl_hcallback_rebuild_t *callback, void *private_data);
int snd_ctl_hcallback_add(snd_ctl_t *handle, snd_ctl_hcallback_add_t *callback, void *private_data);
int snd_ctl_hevent(snd_ctl_t *handle);

int snd_ctl_hbag_create(void **bag);
int snd_ctl_hbag_destroy(void **bag, void (*hcontrol_free)(snd_hcontrol_t *hcontrol));
int snd_ctl_hbag_add(void **bag, snd_hcontrol_t *hcontrol);
int snd_ctl_hbag_del(void **bag, snd_hcontrol_t *hcontrol);
snd_hcontrol_t *snd_ctl_hbag_find(void **bag, snd_control_id_t *id);

#ifdef __cplusplus
}
#endif

