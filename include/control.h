/****************************************************************************
 *                                                                          *
 *                              control.h                                   *
 *                          Control Interface                               *
 *                                                                          *
 ****************************************************************************/

typedef struct sndrv_aes_iec958 snd_aes_iec958_t;
typedef struct _snd_ctl_card_info snd_ctl_card_info_t;
typedef struct _snd_ctl_elem_id snd_ctl_elem_id_t;
typedef struct _snd_ctl_elem_list snd_ctl_elem_list_t;
typedef struct _snd_ctl_elem_info snd_ctl_elem_info_t;
typedef struct _snd_ctl_elem_value snd_ctl_elem_value_t;
typedef struct _snd_ctl_event snd_ctl_event_t;

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_card_type *snd_card_type_t;
typedef struct __snd_ctl_elem_type *snd_ctl_elem_type_t;
typedef struct __snd_ctl_elem_iface *snd_ctl_elem_iface_t;
typedef struct __snd_ctl_event_type *snd_ctl_event_type_t;
#else
typedef enum sndrv_card_type snd_card_type_t;
typedef enum sndrv_ctl_elem_type snd_ctl_elem_type_t;
typedef enum sndrv_ctl_elem_iface snd_ctl_elem_iface_t;
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

#define SND_CTL_ELEM_TYPE_NONE ((snd_ctl_elem_type_t) SNDRV_CTL_ELEM_TYPE_NONE)
#define SND_CTL_ELEM_TYPE_BOOLEAN ((snd_ctl_elem_type_t) SNDRV_CTL_ELEM_TYPE_BOOLEAN)
#define SND_CTL_ELEM_TYPE_INTEGER ((snd_ctl_elem_type_t) SNDRV_CTL_ELEM_TYPE_INTEGER)
#define SND_CTL_ELEM_TYPE_ENUMERATED ((snd_ctl_elem_type_t) SNDRV_CTL_ELEM_TYPE_ENUMERATED)
#define SND_CTL_ELEM_TYPE_BYTES ((snd_ctl_elem_type_t) SNDRV_CTL_ELEM_TYPE_BYTES)
#define SND_CTL_ELEM_TYPE_IEC958 ((snd_ctl_elem_type_t) SNDRV_CTL_ELEM_TYPE_IEC958)
#define SND_CTL_ELEM_TYPE_LAST ((snd_ctl_elem_type_t) SNDRV_CTL_ELEM_TYPE_LAST)

#define SND_CTL_ELEM_IFACE_CARD ((snd_ctl_elem_iface_t) SNDRV_CTL_ELEM_IFACE_CARD)
#define SND_CTL_ELEM_IFACE_HWDEP ((snd_ctl_elem_iface_t) SNDRV_CTL_ELEM_IFACE_HWDEP)
#define SND_CTL_ELEM_IFACE_MIXER ((snd_ctl_elem_iface_t) SNDRV_CTL_ELEM_IFACE_MIXER)
#define SND_CTL_ELEM_IFACE_PCM ((snd_ctl_elem_iface_t) SNDRV_CTL_ELEM_IFACE_PCM)
#define SND_CTL_ELEM_IFACE_RAWMIDI ((snd_ctl_elem_iface_t) SNDRV_CTL_ELEM_IFACE_RAWMIDI)
#define SND_CTL_ELEM_IFACE_TIMER ((snd_ctl_elem_iface_t) SNDRV_CTL_ELEM_IFACE_TIMER)
#define SND_CTL_ELEM_IFACE_SEQUENCER ((snd_ctl_elem_iface_t) SNDRV_CTL_ELEM_IFACE_SEQUENCER)
#define SND_CTL_ELEM_IFACE_LAST ((snd_ctl_elem_iface_t) SNDRV_CTL_ELEM_IFACE_LAST)

#define SND_CTL_EVENT_REBUILD ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_REBUILD)
#define SND_CTL_EVENT_VALUE ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_VALUE)
#define SND_CTL_EVENT_INFO ((snd_ctl_event_type_t) SNDRV_CTL_EVENT_INFO)
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

snd_ctl_type_t snd_ctl_type(snd_ctl_t *ctl);
int snd_ctl_open(snd_ctl_t **ctl, const char *name);
int snd_ctl_close(snd_ctl_t *ctl);
int snd_ctl_nonblock(snd_ctl_t *ctl, int nonblock);
int snd_ctl_async(snd_ctl_t *ctl, int sig, pid_t pid);
int snd_ctl_poll_descriptor(snd_ctl_t *ctl);
int snd_ctl_card_info(snd_ctl_t *ctl, snd_ctl_card_info_t *info);
int snd_ctl_elem_list(snd_ctl_t *ctl, snd_ctl_elem_list_t * list);
int snd_ctl_elem_info(snd_ctl_t *ctl, snd_ctl_elem_info_t *info);
int snd_ctl_elem_read(snd_ctl_t *ctl, snd_ctl_elem_value_t *value);
int snd_ctl_elem_write(snd_ctl_t *ctl, snd_ctl_elem_value_t *value);
int snd_ctl_hwdep_next_device(snd_ctl_t *ctl, int * device);
int snd_ctl_hwdep_info(snd_ctl_t *ctl, snd_hwdep_info_t * info);
int snd_ctl_pcm_next_device(snd_ctl_t *ctl, int *device);
int snd_ctl_pcm_info(snd_ctl_t *ctl, snd_pcm_info_t * info);
int snd_ctl_pcm_prefer_subdevice(snd_ctl_t *ctl, int subdev);
int snd_ctl_rawmidi_next_device(snd_ctl_t *ctl, int * device);
int snd_ctl_rawmidi_info(snd_ctl_t *ctl, snd_rawmidi_info_t * info);
int snd_ctl_rawmidi_prefer_subdevice(snd_ctl_t *ctl, int subdev);

int snd_ctl_read(snd_ctl_t *ctl, snd_ctl_event_t *event);
int snd_ctl_wait(snd_ctl_t *ctl, int timeout);
const char *snd_ctl_name(snd_ctl_t *ctl);
snd_ctl_type_t snd_ctl_type(snd_ctl_t *ctl);

void snd_ctl_elem_set_bytes(snd_ctl_elem_value_t *obj, void *data, size_t size);
const char *snd_ctl_elem_type_name(snd_ctl_elem_type_t type);
const char *snd_ctl_elem_iface_name(snd_ctl_elem_iface_t iface);
const char *snd_ctl_event_type_name(snd_ctl_event_type_t type);

int snd_ctl_elem_list_alloc_space(snd_ctl_elem_list_t *obj, unsigned int entries);
void snd_ctl_elem_list_free_space(snd_ctl_elem_list_t *obj);

#ifdef __cplusplus
}
#endif

/*
 *  Highlevel API for controls
 */

typedef struct _snd_hctl_elem snd_hctl_elem_t;

typedef struct _snd_hctl snd_hctl_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*snd_hctl_compare_t)(const snd_hctl_elem_t *e1,
				  const snd_hctl_elem_t *e2);
typedef int (*snd_hctl_callback_t)(snd_hctl_t *hctl,
				   snd_ctl_event_type_t event,
				   snd_hctl_elem_t *elem);
typedef int (*snd_hctl_elem_callback_t)(snd_hctl_elem_t *elem,
					snd_ctl_event_type_t event);

int snd_hctl_open(snd_hctl_t **hctl, const char *name);
int snd_hctl_close(snd_hctl_t *hctl);
int snd_hctl_nonblock(snd_hctl_t *hctl, int nonblock);
int snd_hctl_async(snd_hctl_t *hctl, int sig, pid_t pid);
int snd_hctl_poll_descriptor(snd_hctl_t *hctl);
unsigned int snd_hctl_get_count(snd_hctl_t *hctl);
int snd_hctl_set_compare(snd_hctl_t *hctl, snd_hctl_compare_t hsort);
snd_hctl_elem_t *snd_hctl_first_elem(snd_hctl_t *hctl);
snd_hctl_elem_t *snd_hctl_last_elem(snd_hctl_t *hctl);
snd_hctl_elem_t *snd_hctl_find_elem(snd_hctl_t *hctl, const snd_ctl_elem_id_t *id);
void snd_hctl_set_callback(snd_hctl_t *hctl, snd_hctl_callback_t callback);
void snd_hctl_set_callback_private(snd_hctl_t *hctl, void *data);
void *snd_hctl_get_callback_private(snd_hctl_t *hctl);
int snd_hctl_load(snd_hctl_t *hctl);
int snd_hctl_free(snd_hctl_t *hctl);
int snd_hctl_handle_event(snd_hctl_t *hctl, snd_ctl_event_t *event);
int snd_hctl_handle_events(snd_hctl_t *hctl);
const char *snd_hctl_name(snd_hctl_t *hctl);
snd_ctl_type_t snd_hctl_type(snd_hctl_t *hctl);

snd_hctl_elem_t *snd_hctl_elem_next(snd_hctl_elem_t *elem);
snd_hctl_elem_t *snd_hctl_elem_prev(snd_hctl_elem_t *elem);
int snd_hctl_elem_info(snd_hctl_elem_t *elem, snd_ctl_elem_info_t * info);
int snd_hctl_elem_read(snd_hctl_elem_t *elem, snd_ctl_elem_value_t * value);
int snd_hctl_elem_write(snd_hctl_elem_t *elem, snd_ctl_elem_value_t * value);

snd_hctl_t *snd_hctl_elem_get_handle(snd_hctl_elem_t *elem);

#ifdef __cplusplus
}
#endif

