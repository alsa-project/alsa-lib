/****************************************************************************
 *                                                                          *
 *                              control.h                                   *
 *                          Control Interface                               *
 *                                                                          *
 ****************************************************************************/

/* sndrv aliasing */

typedef struct sndrv_aes_iec958 snd_aes_iec958_t;
typedef union sndrv_digital_audio snd_digital_audio_t;
typedef enum sndrv_card_type snd_card_type;
typedef struct sndrv_ctl_hw_info snd_ctl_hw_info_t;
typedef enum sndrv_control_type snd_control_type_t;
typedef enum sndrv_control_iface snd_control_iface_t;
typedef struct sndrv_control_id snd_control_id_t;
typedef struct sndrv_control_list snd_control_list_t;
typedef struct sndrv_control_info snd_control_info_t;
typedef struct sndrv_control snd_control_t;
typedef enum sndrv_ctl_event_type snd_ctl_event_type_t;
typedef struct sndrv_ctl_event snd_ctl_event_t;
#define SND_CARD_TYPE_GUS_CLASSIC SNDRV_CARD_TYPE_GUS_CLASSIC
#define SND_CARD_TYPE_GUS_EXTREME SNDRV_CARD_TYPE_GUS_EXTREME
#define SND_CARD_TYPE_GUS_ACE SNDRV_CARD_TYPE_GUS_ACE
#define SND_CARD_TYPE_GUS_MAX SNDRV_CARD_TYPE_GUS_MAX
#define SND_CARD_TYPE_AMD_INTERWAVE SNDRV_CARD_TYPE_AMD_INTERWAVE
#define SND_CARD_TYPE_SB_10 SNDRV_CARD_TYPE_SB_10
#define SND_CARD_TYPE_SB_20 SNDRV_CARD_TYPE_SB_20
#define SND_CARD_TYPE_SB_PRO SNDRV_CARD_TYPE_SB_PRO
#define SND_CARD_TYPE_SB_16 SNDRV_CARD_TYPE_SB_16
#define SND_CARD_TYPE_SB_AWE SNDRV_CARD_TYPE_SB_AWE
#define SND_CARD_TYPE_ESS_ES1688 SNDRV_CARD_TYPE_ESS_ES1688
#define SND_CARD_TYPE_OPL3_SA2 SNDRV_CARD_TYPE_OPL3_SA2
#define SND_CARD_TYPE_MOZART SNDRV_CARD_TYPE_MOZART
#define SND_CARD_TYPE_S3_SONICVIBES SNDRV_CARD_TYPE_S3_SONICVIBES
#define SND_CARD_TYPE_ENS1370 SNDRV_CARD_TYPE_ENS1370
#define SND_CARD_TYPE_ENS1371 SNDRV_CARD_TYPE_ENS1371
#define SND_CARD_TYPE_CS4232 SNDRV_CARD_TYPE_CS4232
#define SND_CARD_TYPE_CS4236 SNDRV_CARD_TYPE_CS4236
#define SND_CARD_TYPE_AMD_INTERWAVE_STB SNDRV_CARD_TYPE_AMD_INTERWAVE_STB
#define SND_CARD_TYPE_ESS_ES1938 SNDRV_CARD_TYPE_ESS_ES1938
#define SND_CARD_TYPE_ESS_ES18XX SNDRV_CARD_TYPE_ESS_ES18XX
#define SND_CARD_TYPE_CS4231 SNDRV_CARD_TYPE_CS4231
#define SND_CARD_TYPE_OPTI92X SNDRV_CARD_TYPE_OPTI92X
#define SND_CARD_TYPE_SERIAL SNDRV_CARD_TYPE_SERIAL
#define SND_CARD_TYPE_AD1848 SNDRV_CARD_TYPE_AD1848
#define SND_CARD_TYPE_TRID4DWAVEDX SNDRV_CARD_TYPE_TRID4DWAVEDX
#define SND_CARD_TYPE_TRID4DWAVENX SNDRV_CARD_TYPE_TRID4DWAVENX
#define SND_CARD_TYPE_SGALAXY SNDRV_CARD_TYPE_SGALAXY
#define SND_CARD_TYPE_CS46XX SNDRV_CARD_TYPE_CS46XX
#define SND_CARD_TYPE_WAVEFRONT SNDRV_CARD_TYPE_WAVEFRONT
#define SND_CARD_TYPE_TROPEZ SNDRV_CARD_TYPE_TROPEZ
#define SND_CARD_TYPE_TROPEZPLUS SNDRV_CARD_TYPE_TROPEZPLUS
#define SND_CARD_TYPE_MAUI SNDRV_CARD_TYPE_MAUI
#define SND_CARD_TYPE_CMI8330 SNDRV_CARD_TYPE_CMI8330
#define SND_CARD_TYPE_DUMMY SNDRV_CARD_TYPE_DUMMY
#define SND_CARD_TYPE_ALS100 SNDRV_CARD_TYPE_ALS100
#define SND_CARD_TYPE_SHARE SNDRV_CARD_TYPE_SHARE
#define SND_CARD_TYPE_SI_7018 SNDRV_CARD_TYPE_SI_7018
#define SND_CARD_TYPE_OPTI93X SNDRV_CARD_TYPE_OPTI93X
#define SND_CARD_TYPE_MTPAV SNDRV_CARD_TYPE_MTPAV
#define SND_CARD_TYPE_VIRMIDI SNDRV_CARD_TYPE_VIRMIDI
#define SND_CARD_TYPE_EMU10K1 SNDRV_CARD_TYPE_EMU10K1
#define SND_CARD_TYPE_HAMMERFALL SNDRV_CARD_TYPE_HAMMERFALL
#define SND_CARD_TYPE_HAMMERFALL_LIGHT SNDRV_CARD_TYPE_HAMMERFALL_LIGHT
#define SND_CARD_TYPE_ICE1712 SNDRV_CARD_TYPE_ICE1712
#define SND_CARD_TYPE_CMI8338 SNDRV_CARD_TYPE_CMI8338
#define SND_CARD_TYPE_CMI8738 SNDRV_CARD_TYPE_CMI8738
#define SND_CARD_TYPE_AD1816A SNDRV_CARD_TYPE_AD1816A
#define SND_CARD_TYPE_INTEL8X0 SNDRV_CARD_TYPE_INTEL8X0
#define SND_CARD_TYPE_ESS_ESOLDM1 SNDRV_CARD_TYPE_ESS_ESOLDM1
#define SND_CARD_TYPE_ESS_ES1968 SNDRV_CARD_TYPE_ESS_ES1968
#define SND_CARD_TYPE_ESS_ES1978 SNDRV_CARD_TYPE_ESS_ES1978
#define SND_CARD_TYPE_DIGI96 SNDRV_CARD_TYPE_DIGI96
#define SND_CARD_TYPE_VIA82C686A SNDRV_CARD_TYPE_VIA82C686A
#define SND_CARD_TYPE_FM801 SNDRV_CARD_TYPE_FM801
#define SND_CARD_TYPE_AZT2320 SNDRV_CARD_TYPE_AZT2320
#define SND_CARD_TYPE_PRODIF_PLUS SNDRV_CARD_TYPE_PRODIF_PLUS
#define SND_CARD_TYPE_YMFPCI SNDRV_CARD_TYPE_YMFPCI
#define SND_CARD_TYPE_CS4281 SNDRV_CARD_TYPE_CS4281
#define SND_CARD_TYPE_MPU401_UART SNDRV_CARD_TYPE_MPU401_UART
#define SND_CARD_TYPE_ALS4000 SNDRV_CARD_TYPE_ALS4000
#define SND_CARD_TYPE_ALLEGRO_1 SNDRV_CARD_TYPE_ALLEGRO_1
#define SND_CARD_TYPE_ALLEGRO SNDRV_CARD_TYPE_ALLEGRO
#define SND_CARD_TYPE_MAESTRO3 SNDRV_CARD_TYPE_MAESTRO3
#define SND_CARD_TYPE_AWACS SNDRV_CARD_TYPE_AWACS
#define SND_CARD_TYPE_NM256AV SNDRV_CARD_TYPE_NM256AV
#define SND_CARD_TYPE_NM256ZX SNDRV_CARD_TYPE_NM256ZX
#define SND_CARD_TYPE_VIA8233 SNDRV_CARD_TYPE_VIA8233
#define SND_CARD_TYPE_LAST SNDRV_CARD_TYPE_LAST
#define SND_CONTROL_TYPE_NONE SNDRV_CONTROL_TYPE_NONE
#define SND_CONTROL_TYPE_BOOLEAN SNDRV_CONTROL_TYPE_BOOLEAN
#define SND_CONTROL_TYPE_INTEGER SNDRV_CONTROL_TYPE_INTEGER
#define SND_CONTROL_TYPE_ENUMERATED SNDRV_CONTROL_TYPE_ENUMERATED
#define SND_CONTROL_TYPE_BYTES SNDRV_CONTROL_TYPE_BYTES
#define SND_CONTROL_TYPE_IEC958 SNDRV_CONTROL_TYPE_IEC958
#define SND_CONTROL_IFACE_CARD SNDRV_CONTROL_IFACE_CARD
#define SND_CONTROL_IFACE_HWDEP SNDRV_CONTROL_IFACE_HWDEP
#define SND_CONTROL_IFACE_MIXER SNDRV_CONTROL_IFACE_MIXER
#define SND_CONTROL_IFACE_PCM SNDRV_CONTROL_IFACE_PCM
#define SND_CONTROL_IFACE_RAWMIDI SNDRV_CONTROL_IFACE_RAWMIDI
#define SND_CONTROL_IFACE_TIMER SNDRV_CONTROL_IFACE_TIMER
#define SND_CONTROL_IFACE_SEQUENCER SNDRV_CONTROL_IFACE_SEQUENCER
#define SND_CONTROL_ACCESS_READ SNDRV_CONTROL_ACCESS_READ
#define SND_CONTROL_ACCESS_WRITE SNDRV_CONTROL_ACCESS_WRITE
#define SND_CONTROL_ACCESS_READWRITE SNDRV_CONTROL_ACCESS_READWRITE
#define SND_CONTROL_ACCESS_VOLATILE SNDRV_CONTROL_ACCESS_VOLATILE
#define SND_CONTROL_ACCESS_INACTIVE SNDRV_CONTROL_ACCESS_INACTIVE
#define SND_CONTROL_ACCESS_LOCK SNDRV_CONTROL_ACCESS_LOCK
#define SND_CONTROL_ACCESS_INDIRECT SNDRV_CONTROL_ACCESS_INDIRECT
#define SND_CTL_EVENT_REBUILD SNDRV_CTL_EVENT_REBUILD
#define SND_CTL_EVENT_VALUE SNDRV_CTL_EVENT_VALUE
#define SND_CTL_EVENT_CHANGE SNDRV_CTL_EVENT_CHANGE
#define SND_CTL_EVENT_ADD SNDRV_CTL_EVENT_ADD
#define SND_CTL_EVENT_REMOVE SNDRV_CTL_EVENT_REMOVE

typedef struct _snd_ctl snd_ctl_t;

typedef enum _snd_ctl_type { SND_CTL_TYPE_HW,
	       SND_CTL_TYPE_SHM,
	       SND_CTL_TYPE_INET
 } snd_ctl_type_t;

typedef struct _snd_ctl_callbacks {
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
int snd_card_next(int *card);
int snd_card_get_index(const char *name);
int snd_card_get_name(int card, char **name);
int snd_card_get_longname(int card, char **name);

int snd_defaults_card(void);
int snd_defaults_mixer_card(void);
int snd_defaults_pcm_card(void);
int snd_defaults_pcm_device(void);
int snd_defaults_rawmidi_card(void);
int snd_defaults_rawmidi_device(void);

snd_ctl_type_t snd_ctl_type(snd_ctl_t *handle);
int snd_ctl_open(snd_ctl_t **handle, char *name);
int snd_ctl_close(snd_ctl_t *handle);
int snd_ctl_card(snd_ctl_t *handle);
int snd_ctl_poll_descriptor(snd_ctl_t *handle);
int snd_ctl_hw_info(snd_ctl_t *handle, snd_ctl_hw_info_t *info);
int snd_ctl_clist(snd_ctl_t *handle, snd_control_list_t * list);
int snd_ctl_cinfo(snd_ctl_t *handle, snd_control_info_t * sw);
int snd_ctl_cread(snd_ctl_t *handle, snd_control_t * control);
int snd_ctl_cwrite(snd_ctl_t *handle, snd_control_t * control);
int snd_ctl_hwdep_next_device(snd_ctl_t *handle, int * device);
int snd_ctl_hwdep_info(snd_ctl_t *handle, snd_hwdep_info_t * info);
int snd_ctl_pcm_next_device(snd_ctl_t *handle, int *device);
int snd_ctl_pcm_info(snd_ctl_t *handle, snd_pcm_info_t * info);
int snd_ctl_pcm_prefer_subdevice(snd_ctl_t *handle, int subdev);
int snd_ctl_rawmidi_next_device(snd_ctl_t *handle, int * device);
int snd_ctl_rawmidi_info(snd_ctl_t *handle, snd_rawmidi_info_t * info);
int snd_ctl_rawmidi_prefer_subdevice(snd_ctl_t *handle, int subdev);

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


typedef struct _snd_hcontrol_list snd_hcontrol_list_t;
typedef struct _snd_hcontrol snd_hcontrol_t;

struct _snd_hcontrol_list {
	unsigned int controls_offset;	/* W: first control ID to get */
	unsigned int controls_request;	/* W: count of control IDs to get */
	unsigned int controls_count;	/* R: count of available (set) controls */
	unsigned int controls;		/* R: count of all available controls */
	snd_control_id_t *pids;		/* W: IDs */
};

struct _snd_hcontrol {
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

