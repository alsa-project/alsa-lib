/****************************************************************************
 *                                                                          *
 *                              control.h                                   *
 *                          Control Interface                               *
 *                                                                          *
 ****************************************************************************/

typedef struct sndrv_aes_iec958 snd_aes_iec958_t;
typedef struct _snd_ctl_info snd_ctl_info_t;
typedef struct _snd_control_id snd_control_id_t;
typedef struct _snd_control_list snd_control_list_t;
typedef struct _snd_control_info snd_control_info_t;
typedef struct _snd_control snd_control_t;
typedef struct _snd_ctl_event snd_ctl_event_t;

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_card_type *snd_card_type_t;
typedef struct __snd_control_type *snd_control_type_t;
typedef struct __snd_control_iface *snd_control_iface_t;
typedef struct __snd_ctl_event_type *snd_ctl_event_type_t;
#else
typedef enum sndrv_card_type snd_card_type_t;
typedef enum sndrv_control_type snd_control_type_t;
typedef enum sndrv_control_iface snd_control_iface_t;
typedef enum sndrv_ctl_event_type snd_ctl_event_type_t;
#endif

#define SND_CARD_TYPE_GUS_CLASSIC ((snd_card_type_t) SNDRV_CARD_TYPE_GUS_CLASSIC)
#define SND_CARD_TYPE_GUS_EXTREME ((snd_card_type_t) SNDRV_CARD_TYPE_GUS_EXTREME)
#define SND_CARD_TYPE_GUS_ACE ((snd_card_type_t) SNDRV_CARD_TYPE_GUS_ACE)
#define SND_CARD_TYPE_GUS_MAX ((snd_card_type_t) SNDRV_CARD_TYPE_GUS_MAX)
#define SND_CARD_TYPE_AMD_INTERWAVE ((snd_card_type_t) SNDRV_CARD_TYPE_AMD_INTERWAVE)
#define SND_CARD_TYPE_SB_10 ((snd_card_type_t) SNDRV_CARD_TYPE_SB_10)
#define SND_CARD_TYPE_SB_20 ((snd_card_type_t) SNDRV_CARD_TYPE_SB_20)
#define SND_CARD_TYPE_SB_PRO ((snd_card_type_t) SNDRV_CARD_TYPE_SB_PRO)
#define SND_CARD_TYPE_SB_16 ((snd_card_type_t) SNDRV_CARD_TYPE_SB_16)
#define SND_CARD_TYPE_SB_AWE ((snd_card_type_t) SNDRV_CARD_TYPE_SB_AWE)
#define SND_CARD_TYPE_ESS_ES1688 ((snd_card_type_t) SNDRV_CARD_TYPE_ESS_ES1688)
#define SND_CARD_TYPE_OPL3_SA2 ((snd_card_type_t) SNDRV_CARD_TYPE_OPL3_SA2)
#define SND_CARD_TYPE_MOZART ((snd_card_type_t) SNDRV_CARD_TYPE_MOZART)
#define SND_CARD_TYPE_S3_SONICVIBES ((snd_card_type_t) SNDRV_CARD_TYPE_S3_SONICVIBES)
#define SND_CARD_TYPE_ENS1370 ((snd_card_type_t) SNDRV_CARD_TYPE_ENS1370)
#define SND_CARD_TYPE_ENS1371 ((snd_card_type_t) SNDRV_CARD_TYPE_ENS1371)
#define SND_CARD_TYPE_CS4232 ((snd_card_type_t) SNDRV_CARD_TYPE_CS4232)
#define SND_CARD_TYPE_CS4236 ((snd_card_type_t) SNDRV_CARD_TYPE_CS4236)
#define SND_CARD_TYPE_AMD_INTERWAVE_STB ((snd_card_type_t) SNDRV_CARD_TYPE_AMD_INTERWAVE_STB)
#define SND_CARD_TYPE_ESS_ES1938 ((snd_card_type_t) SNDRV_CARD_TYPE_ESS_ES1938)
#define SND_CARD_TYPE_ESS_ES18XX ((snd_card_type_t) SNDRV_CARD_TYPE_ESS_ES18XX)
#define SND_CARD_TYPE_CS4231 ((snd_card_type_t) SNDRV_CARD_TYPE_CS4231)
#define SND_CARD_TYPE_OPTI92X ((snd_card_type_t) SNDRV_CARD_TYPE_OPTI92X)
#define SND_CARD_TYPE_SERIAL ((snd_card_type_t) SNDRV_CARD_TYPE_SERIAL)
#define SND_CARD_TYPE_AD1848 ((snd_card_type_t) SNDRV_CARD_TYPE_AD1848)
#define SND_CARD_TYPE_TRID4DWAVEDX ((snd_card_type_t) SNDRV_CARD_TYPE_TRID4DWAVEDX)
#define SND_CARD_TYPE_TRID4DWAVENX ((snd_card_type_t) SNDRV_CARD_TYPE_TRID4DWAVENX)
#define SND_CARD_TYPE_SGALAXY ((snd_card_type_t) SNDRV_CARD_TYPE_SGALAXY)
#define SND_CARD_TYPE_CS46XX ((snd_card_type_t) SNDRV_CARD_TYPE_CS46XX)
#define SND_CARD_TYPE_WAVEFRONT ((snd_card_type_t) SNDRV_CARD_TYPE_WAVEFRONT)
#define SND_CARD_TYPE_TROPEZ ((snd_card_type_t) SNDRV_CARD_TYPE_TROPEZ)
#define SND_CARD_TYPE_TROPEZPLUS ((snd_card_type_t) SNDRV_CARD_TYPE_TROPEZPLUS)
#define SND_CARD_TYPE_MAUI ((snd_card_type_t) SNDRV_CARD_TYPE_MAUI)
#define SND_CARD_TYPE_CMI8330 ((snd_card_type_t) SNDRV_CARD_TYPE_CMI8330)
#define SND_CARD_TYPE_DUMMY ((snd_card_type_t) SNDRV_CARD_TYPE_DUMMY)
#define SND_CARD_TYPE_ALS100 ((snd_card_type_t) SNDRV_CARD_TYPE_ALS100)
#define SND_CARD_TYPE_SHARE ((snd_card_type_t) SNDRV_CARD_TYPE_SHARE)
#define SND_CARD_TYPE_SI_7018 ((snd_card_type_t) SNDRV_CARD_TYPE_SI_7018)
#define SND_CARD_TYPE_OPTI93X ((snd_card_type_t) SNDRV_CARD_TYPE_OPTI93X)
#define SND_CARD_TYPE_MTPAV ((snd_card_type_t) SNDRV_CARD_TYPE_MTPAV)
#define SND_CARD_TYPE_VIRMIDI ((snd_card_type_t) SNDRV_CARD_TYPE_VIRMIDI)
#define SND_CARD_TYPE_EMU10K1 ((snd_card_type_t) SNDRV_CARD_TYPE_EMU10K1)
#define SND_CARD_TYPE_HAMMERFALL ((snd_card_type_t) SNDRV_CARD_TYPE_HAMMERFALL)
#define SND_CARD_TYPE_HAMMERFALL_LIGHT ((snd_card_type_t) SNDRV_CARD_TYPE_HAMMERFALL_LIGHT)
#define SND_CARD_TYPE_ICE1712 ((snd_card_type_t) SNDRV_CARD_TYPE_ICE1712)
#define SND_CARD_TYPE_CMI8338 ((snd_card_type_t) SNDRV_CARD_TYPE_CMI8338)
#define SND_CARD_TYPE_CMI8738 ((snd_card_type_t) SNDRV_CARD_TYPE_CMI8738)
#define SND_CARD_TYPE_AD1816A ((snd_card_type_t) SNDRV_CARD_TYPE_AD1816A)
#define SND_CARD_TYPE_INTEL8X0 ((snd_card_type_t) SNDRV_CARD_TYPE_INTEL8X0)
#define SND_CARD_TYPE_ESS_ESOLDM1 ((snd_card_type_t) SNDRV_CARD_TYPE_ESS_ESOLDM1)
#define SND_CARD_TYPE_ESS_ES1968 ((snd_card_type_t) SNDRV_CARD_TYPE_ESS_ES1968)
#define SND_CARD_TYPE_ESS_ES1978 ((snd_card_type_t) SNDRV_CARD_TYPE_ESS_ES1978)
#define SND_CARD_TYPE_DIGI96 ((snd_card_type_t) SNDRV_CARD_TYPE_DIGI96)
#define SND_CARD_TYPE_VIA82C686A ((snd_card_type_t) SNDRV_CARD_TYPE_VIA82C686A)
#define SND_CARD_TYPE_FM801 ((snd_card_type_t) SNDRV_CARD_TYPE_FM801)
#define SND_CARD_TYPE_AZT2320 ((snd_card_type_t) SNDRV_CARD_TYPE_AZT2320)
#define SND_CARD_TYPE_PRODIF_PLUS ((snd_card_type_t) SNDRV_CARD_TYPE_PRODIF_PLUS)
#define SND_CARD_TYPE_YMFPCI ((snd_card_type_t) SNDRV_CARD_TYPE_YMFPCI)
#define SND_CARD_TYPE_CS4281 ((snd_card_type_t) SNDRV_CARD_TYPE_CS4281)
#define SND_CARD_TYPE_MPU401_UART ((snd_card_type_t) SNDRV_CARD_TYPE_MPU401_UART)
#define SND_CARD_TYPE_ALS4000 ((snd_card_type_t) SNDRV_CARD_TYPE_ALS4000)
#define SND_CARD_TYPE_ALLEGRO_1 ((snd_card_type_t) SNDRV_CARD_TYPE_ALLEGRO_1)
#define SND_CARD_TYPE_ALLEGRO ((snd_card_type_t) SNDRV_CARD_TYPE_ALLEGRO)
#define SND_CARD_TYPE_MAESTRO3 ((snd_card_type_t) SNDRV_CARD_TYPE_MAESTRO3)
#define SND_CARD_TYPE_AWACS ((snd_card_type_t) SNDRV_CARD_TYPE_AWACS)
#define SND_CARD_TYPE_NM256AV ((snd_card_type_t) SNDRV_CARD_TYPE_NM256AV)
#define SND_CARD_TYPE_NM256ZX ((snd_card_type_t) SNDRV_CARD_TYPE_NM256ZX)
#define SND_CARD_TYPE_VIA8233 ((snd_card_type_t) SNDRV_CARD_TYPE_VIA8233)
#define SND_CARD_TYPE_LAST ((snd_card_type_t) SNDRV_CARD_TYPE_LAST)

#define SND_CONTROL_TYPE_NONE ((snd_control_type_t) SNDRV_CONTROL_TYPE_NONE)
#define SND_CONTROL_TYPE_BOOLEAN ((snd_control_type_t) SNDRV_CONTROL_TYPE_BOOLEAN)
#define SND_CONTROL_TYPE_INTEGER ((snd_control_type_t) SNDRV_CONTROL_TYPE_INTEGER)
#define SND_CONTROL_TYPE_ENUMERATED ((snd_control_type_t) SNDRV_CONTROL_TYPE_ENUMERATED)
#define SND_CONTROL_TYPE_BYTES ((snd_control_type_t) SNDRV_CONTROL_TYPE_BYTES)
#define SND_CONTROL_TYPE_IEC958 ((snd_control_type_t) SNDRV_CONTROL_TYPE_IEC958)
#define SND_CONTROL_TYPE_LAST ((snd_control_type_t) SNDRV_CONTROL_TYPE_LAST)

#define SND_CONTROL_IFACE_CARD ((snd_control_iface_t) SNDRV_CONTROL_IFACE_CARD)
#define SND_CONTROL_IFACE_HWDEP ((snd_control_iface_t) SNDRV_CONTROL_IFACE_HWDEP)
#define SND_CONTROL_IFACE_MIXER ((snd_control_iface_t) SNDRV_CONTROL_IFACE_MIXER)
#define SND_CONTROL_IFACE_PCM ((snd_control_iface_t) SNDRV_CONTROL_IFACE_PCM)
#define SND_CONTROL_IFACE_RAWMIDI ((snd_control_iface_t) SNDRV_CONTROL_IFACE_RAWMIDI)
#define SND_CONTROL_IFACE_TIMER ((snd_control_iface_t) SNDRV_CONTROL_IFACE_TIMER)
#define SND_CONTROL_IFACE_SEQUENCER ((snd_control_iface_t) SNDRV_CONTROL_IFACE_SEQUENCER)
#define SND_CONTROL_IFACE_LAST ((snd_control_iface_t) SNDRV_CONTROL_IFACE_LAST)

#define SND_CTL_EVENT_REBUILD ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_REBUILD)
#define SND_CTL_EVENT_VALUE ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_VALUE)
#define SND_CTL_EVENT_CHANGE ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_CHANGE)
#define SND_CTL_EVENT_ADD ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_ADD)
#define SND_CTL_EVENT_REMOVE ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_REMOVE)
#define SND_CTL_EVENT_LAST ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_LAST)

enum _snd_ctl_type {
	SND_CTL_TYPE_HW,
	SND_CTL_TYPE_SHM,
	SND_CTL_TYPE_INET
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_ctl_type *snd_ctl_type_t;
#else
typedef enum _snd_ctl_type snd_ctl_type_t;
#endif

#define SND_CTL_TYPE_HW ((snd_ctl_type_t) SND_CTL_TYPE_HW)
#define SND_CTL_TYPE_SHM ((snd_ctl_type_t) SND_CTL_TYPE_SHM)
#define SND_CTL_TYPE_INET ((snd_ctl_type_t) SND_CTL_TYPE_INET)

typedef struct _snd_ctl snd_ctl_t;
typedef struct _snd_ctl_callbacks snd_ctl_callbacks_t;

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
int snd_ctl_info(snd_ctl_t *handle, snd_ctl_info_t *info);
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

void snd_control_set_bytes(snd_control_t *obj, void *data, size_t size);

const char *snd_control_type_name(snd_control_type_t type);
const char *snd_control_iface_name(snd_control_iface_t iface);
const char *snd_ctl_event_type_name(snd_ctl_event_type_t type);

int snd_control_list_alloc_space(snd_control_list_t *obj, unsigned int entries);
void snd_control_list_free_space(snd_control_list_t *obj);

#ifdef __cplusplus
}
#endif

/*
 *  Highlevel API for controls
 */

typedef struct _snd_hcontrol_list snd_hcontrol_list_t;
typedef struct _snd_hcontrol snd_hcontrol_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*snd_ctl_hsort_t)(const snd_hcontrol_t *c1, const snd_hcontrol_t *c2);
typedef void (*snd_ctl_hcallback_rebuild_t)(snd_ctl_t *handle, void *private_data);
typedef void (*snd_ctl_hcallback_add_t)(snd_ctl_t *handle, void *private_data, snd_hcontrol_t *hcontrol);
typedef void (*snd_hcontrol_callback_t)(snd_ctl_t *handle, snd_hcontrol_t *hcontrol);
typedef void (*snd_hcontrol_private_free_t)(snd_hcontrol_t *hcontrol);

int snd_ctl_hbuild(snd_ctl_t *handle, snd_ctl_hsort_t csort);
int snd_ctl_hfree(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_hfirst(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_hlast(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_hnext(snd_ctl_t *handle, snd_hcontrol_t *hcontrol);
snd_hcontrol_t *snd_ctl_hprev(snd_ctl_t *handle, snd_hcontrol_t *hcontrol);
int snd_ctl_hcount(snd_ctl_t *handle);
snd_hcontrol_t *snd_ctl_hfind(snd_ctl_t *handle, snd_control_id_t *id);
int snd_ctl_hlist(snd_ctl_t *handle, snd_hcontrol_list_t *hlist);
int snd_ctl_hsort(const snd_hcontrol_t *c1, const snd_hcontrol_t *c2);
int snd_ctl_hresort(snd_ctl_t *handle, snd_ctl_hsort_t csort);
int snd_ctl_hcallback_rebuild(snd_ctl_t *handle, snd_ctl_hcallback_rebuild_t callback, void *private_data);
int snd_ctl_hcallback_add(snd_ctl_t *handle, snd_ctl_hcallback_add_t callback, void *private_data);
int snd_ctl_hevent(snd_ctl_t *handle);

int snd_ctl_hbag_create(void **bag);
int snd_ctl_hbag_destroy(void **bag, void (*hcontrol_free)(snd_hcontrol_t *hcontrol));
int snd_ctl_hbag_add(void **bag, snd_hcontrol_t *hcontrol);
int snd_ctl_hbag_del(void **bag, snd_hcontrol_t *hcontrol);
snd_hcontrol_t *snd_ctl_hbag_find(void **bag, snd_control_id_t *id);
int snd_hcontrol_list_alloc_space(snd_hcontrol_list_t *obj, unsigned int entries);
void snd_hcontrol_list_free_space(snd_hcontrol_list_t *obj);

#ifdef __cplusplus
}
#endif

