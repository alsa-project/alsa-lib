/****************************************************************************
 *                                                                          *
 *                              control.h                                   *
 *                          Control Interface                               *
 *                                                                          *
 ****************************************************************************/

typedef struct sndrv_aes_iec958 snd_aes_iec958_t;

/** CTL card info container */
typedef struct _snd_ctl_card_info snd_ctl_card_info_t;

/** CTL element identificator container */
typedef struct _snd_ctl_elem_id snd_ctl_elem_id_t;

/** CTL element identificator list container */
typedef struct _snd_ctl_elem_list snd_ctl_elem_list_t;

/** CTL element info container */
typedef struct _snd_ctl_elem_info snd_ctl_elem_info_t;

/** CTL element value container */
typedef struct _snd_ctl_elem_value snd_ctl_elem_value_t;

/** CTL event container */
typedef struct _snd_ctl_event snd_ctl_event_t;

/** Card type */
typedef enum _snd_card_type {
	/** Gravis UltraSound Classic */
	SND_CARD_TYPE_GUS_CLASSIC = SNDRV_CARD_TYPE_GUS_CLASSIC,
	/** Gravis UltraSound Extreme */
	SND_CARD_TYPE_GUS_EXTREME = SNDRV_CARD_TYPE_GUS_EXTREME,
	/** Gravis UltraSound ACE */
	SND_CARD_TYPE_GUS_ACE = SNDRV_CARD_TYPE_GUS_ACE,
	/** Gravis UltraSound Max */
	SND_CARD_TYPE_GUS_MAX = SNDRV_CARD_TYPE_GUS_MAX,
	/** AMD InterWave */
	SND_CARD_TYPE_AMD_INTERWAVE = SNDRV_CARD_TYPE_AMD_INTERWAVE,
	/** SoundBlaster v1.0 */
	SND_CARD_TYPE_SB_10 = SNDRV_CARD_TYPE_SB_10,
	/** SoundBlaster v2.0 */
	SND_CARD_TYPE_SB_20 = SNDRV_CARD_TYPE_SB_20,
	/** SoundBlaster Pro */
	SND_CARD_TYPE_SB_PRO = SNDRV_CARD_TYPE_SB_PRO,
	/** SoundBlaster 16 */
	SND_CARD_TYPE_SB_16 = SNDRV_CARD_TYPE_SB_16,
	/** SoundBlaster AWE */
	SND_CARD_TYPE_SB_AWE = SNDRV_CARD_TYPE_SB_AWE,
	/** ESS ES1688 */
	SND_CARD_TYPE_ESS_ES1688 = SNDRV_CARD_TYPE_ESS_ES1688,
	/** Yamaha OPL3 SA2/3 */
	SND_CARD_TYPE_OPL3_SA2 = SNDRV_CARD_TYPE_OPL3_SA2,
	/** Mozart */
	SND_CARD_TYPE_MOZART = SNDRV_CARD_TYPE_MOZART,
	/** S3 SonicVibes */
	SND_CARD_TYPE_S3_SONICVIBES = SNDRV_CARD_TYPE_S3_SONICVIBES,
	/** Ensoniq AudioPCI ES1370 */
	SND_CARD_TYPE_ENS1370 = SNDRV_CARD_TYPE_ENS1370,
	/** Ensoniq AudioPCI ES1371 / Creative CT5880 */
	SND_CARD_TYPE_ENS1371 = SNDRV_CARD_TYPE_ENS1371,
	/** Cirrus Logic CS4232 */
	SND_CARD_TYPE_CS4232 = SNDRV_CARD_TYPE_CS4232,
	/** Cirrus Logic CS4236+ */
	SND_CARD_TYPE_CS4236 = SNDRV_CARD_TYPE_CS4236,
	/** AMD InterWave + TEA6330T */
	SND_CARD_TYPE_AMD_INTERWAVE_STB = SNDRV_CARD_TYPE_AMD_INTERWAVE_STB,
	/** ESS ES1938 */
	SND_CARD_TYPE_ESS_ES1938 = SNDRV_CARD_TYPE_ESS_ES1938,
	/** ESS ES18XX */
	SND_CARD_TYPE_ESS_ES18XX = SNDRV_CARD_TYPE_ESS_ES18XX,
	/** Cirus Logic CS4231 */
	SND_CARD_TYPE_CS4231 = SNDRV_CARD_TYPE_CS4231,
	/** Opti92x */
	SND_CARD_TYPE_OPTI92X = SNDRV_CARD_TYPE_OPTI92X,
	/** Serial MIDI */
	SND_CARD_TYPE_SERIAL = SNDRV_CARD_TYPE_SERIAL,
	/** Analog Devices AD1848 */
	SND_CARD_TYPE_AD1848 = SNDRV_CARD_TYPE_AD1848,
	/** Trident 4D-Wave DX */
	SND_CARD_TYPE_TRID4DWAVEDX = SNDRV_CARD_TYPE_TRID4DWAVEDX,
	/** Trident 4D-Wave NX */
	SND_CARD_TYPE_TRID4DWAVENX = SNDRV_CARD_TYPE_TRID4DWAVENX,
	/** Sound Galaxy */
	SND_CARD_TYPE_SGALAXY = SNDRV_CARD_TYPE_SGALAXY,
	/** Cirrus Logic CS46xx */
	SND_CARD_TYPE_CS46XX = SNDRV_CARD_TYPE_CS46XX,
	/** WaveFront */
	SND_CARD_TYPE_WAVEFRONT = SNDRV_CARD_TYPE_WAVEFRONT,
	/** Turtle Beach Tropez */
	SND_CARD_TYPE_TROPEZ = SNDRV_CARD_TYPE_TROPEZ,
	/** Turtle Beach Tropez+ */
	SND_CARD_TYPE_TROPEZPLUS = SNDRV_CARD_TYPE_TROPEZPLUS,
	/** Turtle Beach Maui */
	SND_CARD_TYPE_MAUI = SNDRV_CARD_TYPE_MAUI,
	/** CMI8330 */
	SND_CARD_TYPE_CMI8330 = SNDRV_CARD_TYPE_CMI8330,
	/** Dummy (/dev/null) soundcard */
	SND_CARD_TYPE_DUMMY = SNDRV_CARD_TYPE_DUMMY,
	/** Avance Logic ALS100 */
	SND_CARD_TYPE_ALS100 = SNDRV_CARD_TYPE_ALS100,
	/** Obsolete */
	SND_CARD_TYPE_SHARE = SNDRV_CARD_TYPE_SHARE,
	/** SiS SI7018 */
	SND_CARD_TYPE_SI_7018 = SNDRV_CARD_TYPE_SI_7018,
	/** Opti93x */
	SND_CARD_TYPE_OPTI93X = SNDRV_CARD_TYPE_OPTI93X,
	/** MidiTimePiece AV multiport MIDI */
	SND_CARD_TYPE_MTPAV = SNDRV_CARD_TYPE_MTPAV,
	/** Virtual MIDI */
	SND_CARD_TYPE_VIRMIDI = SNDRV_CARD_TYPE_VIRMIDI,
	/** EMU10K1 (SB Live, Emu APS) */
	SND_CARD_TYPE_EMU10K1 = SNDRV_CARD_TYPE_EMU10K1,
	/** RME Hammerfall */
	SND_CARD_TYPE_HAMMERFALL = SNDRV_CARD_TYPE_HAMMERFALL,
	/** RME Hammerfall light */
	SND_CARD_TYPE_HAMMERFALL_LIGHT = SNDRV_CARD_TYPE_HAMMERFALL_LIGHT,
	/** ICE1712 (Envy24) */
	SND_CARD_TYPE_ICE1712 = SNDRV_CARD_TYPE_ICE1712,
	/** CMI8338 */
	SND_CARD_TYPE_CMI8338 = SNDRV_CARD_TYPE_CMI8338,
	/** CMI8738 */
	SND_CARD_TYPE_CMI8738 = SNDRV_CARD_TYPE_CMI8738,
	/** Analog Device AD1816A */
	SND_CARD_TYPE_AD1816A = SNDRV_CARD_TYPE_AD1816A,
	/** Intel i8x0 */
	SND_CARD_TYPE_INTEL8X0 = SNDRV_CARD_TYPE_INTEL8X0,
	/** ESS Maestro 1 (old) */
	SND_CARD_TYPE_ESS_ESOLDM1 = SNDRV_CARD_TYPE_ESS_ESOLDM1,
	/** ESS Maestro 2 */
	SND_CARD_TYPE_ESS_ES1968 = SNDRV_CARD_TYPE_ESS_ES1968,
	/** ESS Maestro 2E */
	SND_CARD_TYPE_ESS_ES1978 = SNDRV_CARD_TYPE_ESS_ES1978,
	/** RME Digi96 */
	SND_CARD_TYPE_DIGI96 = SNDRV_CARD_TYPE_DIGI96,
	/** VIA SouthBridge 686A/686B */
	SND_CARD_TYPE_VIA82C686A = SNDRV_CARD_TYPE_VIA82C686A,
	/** Forte Media FM801 */
	SND_CARD_TYPE_FM801 = SNDRV_CARD_TYPE_FM801,
	/** Aztech AZT2320 */
	SND_CARD_TYPE_AZT2320 = SNDRV_CARD_TYPE_AZT2320,
	/** Prodif Plus */
	SND_CARD_TYPE_PRODIF_PLUS = SNDRV_CARD_TYPE_PRODIF_PLUS,
	/** Yamaha PCI */
	SND_CARD_TYPE_YMFPCI = SNDRV_CARD_TYPE_YMFPCI,
	/** Cirrus Logic CS4281 */
	SND_CARD_TYPE_CS4281 = SNDRV_CARD_TYPE_CS4281,
	/** Generic MPU401 UART */
	SND_CARD_TYPE_MPU401_UART = SNDRV_CARD_TYPE_MPU401_UART,
	/** Avance Logic ALS4000 */
	SND_CARD_TYPE_ALS4000 = SNDRV_CARD_TYPE_ALS4000,
	/** ESS Allegro 1 */
	SND_CARD_TYPE_ALLEGRO_1 = SNDRV_CARD_TYPE_ALLEGRO_1,
	/** ESS Allegro */
	SND_CARD_TYPE_ALLEGRO = SNDRV_CARD_TYPE_ALLEGRO,
	/** ESS Maestro 3 */
	SND_CARD_TYPE_MAESTRO3 = SNDRV_CARD_TYPE_MAESTRO3,
	/** AWACS */
	SND_CARD_TYPE_AWACS = SNDRV_CARD_TYPE_AWACS,
	/** NeoMagic NM256AV */
	SND_CARD_TYPE_NM256AV = SNDRV_CARD_TYPE_NM256AV,
	/** NeoMagic NM256ZX */
	SND_CARD_TYPE_NM256ZX = SNDRV_CARD_TYPE_NM256ZX,
	/** VIA VT8233 bridge */
	SND_CARD_TYPE_VIA8233 = SNDRV_CARD_TYPE_VIA8233,
	SND_CARD_TYPE_LAST = SNDRV_CARD_TYPE_LAST,
} snd_card_type_t;

/** CTL element type */
typedef enum _snd_ctl_elem_type {
	/** Invalid type */
	SND_CTL_ELEM_TYPE_NONE = SNDRV_CTL_ELEM_TYPE_NONE,
	/** Boolean contents */
	SND_CTL_ELEM_TYPE_BOOLEAN = SNDRV_CTL_ELEM_TYPE_BOOLEAN,
	/** Integer contents */
	SND_CTL_ELEM_TYPE_INTEGER = SNDRV_CTL_ELEM_TYPE_INTEGER,
	/** Enumerated contents */
	SND_CTL_ELEM_TYPE_ENUMERATED = SNDRV_CTL_ELEM_TYPE_ENUMERATED,
	/** Bytes contents */
	SND_CTL_ELEM_TYPE_BYTES = SNDRV_CTL_ELEM_TYPE_BYTES,
	/** IEC958 (S/PDIF) setting content */
	SND_CTL_ELEM_TYPE_IEC958 = SNDRV_CTL_ELEM_TYPE_IEC958,
	SND_CTL_ELEM_TYPE_LAST = SNDRV_CTL_ELEM_TYPE_LAST,
} snd_ctl_elem_type_t;

/** CTL related interface */
typedef enum _snd_ctl_elem_iface {
	/** Card level */
	SND_CTL_ELEM_IFACE_CARD = SNDRV_CTL_ELEM_IFACE_CARD,
	/** Hardware dependent device */
	SND_CTL_ELEM_IFACE_HWDEP = SNDRV_CTL_ELEM_IFACE_HWDEP,
	/** Mixer */
	SND_CTL_ELEM_IFACE_MIXER = SNDRV_CTL_ELEM_IFACE_MIXER,
	/** PCM */
	SND_CTL_ELEM_IFACE_PCM = SNDRV_CTL_ELEM_IFACE_PCM,
	/** RawMidi */
	SND_CTL_ELEM_IFACE_RAWMIDI = SNDRV_CTL_ELEM_IFACE_RAWMIDI,
	/** Timer */
	SND_CTL_ELEM_IFACE_TIMER = SNDRV_CTL_ELEM_IFACE_TIMER,
	/** Sequencer */
	SND_CTL_ELEM_IFACE_SEQUENCER = SNDRV_CTL_ELEM_IFACE_SEQUENCER,
	SND_CTL_ELEM_IFACE_LAST = SNDRV_CTL_ELEM_IFACE_LAST,
} snd_ctl_elem_iface_t;

/** Event class */
typedef enum _snd_ctl_event_type {
	/** Elements related event */
	SND_CTL_EVENT_ELEM = SNDRV_CTL_EVENT_ELEM,
	SND_CTL_EVENT_LAST = SNDRV_CTL_EVENT_LAST,
}snd_ctl_event_type_t;

/** Element has been removed (Warning: test this first and if set don't
 * test the other masks) \hideinitializer */
#define SND_CTL_EVENT_MASK_REMOVE SNDRV_CTL_EVENT_MASK_REMOVE
/** Element has been added \hideinitializer */
#define SND_CTL_EVENT_MASK_ADD SNDRV_CTL_EVENT_MASK_ADD
/** Element info has been changed \hideinitializer */
#define SND_CTL_EVENT_MASK_INFO SNDRV_CTL_EVENT_MASK_INFO
/** Element value has been changed \hideinitializer */
#define SND_CTL_EVENT_MASK_VALUE SNDRV_CTL_EVENT_MASK_VALUE

#define SND_CTL_NAME_IEC958 SNDRV_CTL_NAME_IEC958

/** CTL type */
typedef enum _snd_ctl_type {
	/** Kernel level CTL */
	SND_CTL_TYPE_HW,
	/** Shared memory client CTL */
	SND_CTL_TYPE_SHM,
	/** INET client CTL (not yet implemented) */
	SND_CTL_TYPE_INET
} snd_ctl_type_t;

/** Non blocking mode \hideinitializer */
#define SND_CTL_NONBLOCK		0x0001

/** Async notification \hideinitializer */
#define SND_CTL_ASYNC			0x0002

/** CTL handle */
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

int snd_ctl_open(snd_ctl_t **ctl, const char *name, int mode);
int snd_ctl_close(snd_ctl_t *ctl);
int snd_ctl_nonblock(snd_ctl_t *ctl, int nonblock);
int snd_ctl_async(snd_ctl_t *ctl, int sig, pid_t pid);
int snd_ctl_poll_descriptors_count(snd_ctl_t *ctl);
int snd_ctl_poll_descriptors(snd_ctl_t *ctl, struct pollfd *pfds, unsigned int space);
int snd_ctl_subscribe_events(snd_ctl_t *ctl, int subscribe);
int snd_ctl_card_info(snd_ctl_t *ctl, snd_ctl_card_info_t *info);
int snd_ctl_elem_list(snd_ctl_t *ctl, snd_ctl_elem_list_t * list);
int snd_ctl_elem_info(snd_ctl_t *ctl, snd_ctl_elem_info_t *info);
int snd_ctl_elem_read(snd_ctl_t *ctl, snd_ctl_elem_value_t *value);
int snd_ctl_elem_write(snd_ctl_t *ctl, snd_ctl_elem_value_t *value);
int snd_ctl_elem_lock(snd_ctl_t *ctl, snd_ctl_elem_id_t *id);
int snd_ctl_elem_unlock(snd_ctl_t *ctl, snd_ctl_elem_id_t *id);
int snd_ctl_hwdep_next_device(snd_ctl_t *ctl, int * device);
int snd_ctl_hwdep_info(snd_ctl_t *ctl, snd_hwdep_info_t * info);
int snd_ctl_pcm_next_device(snd_ctl_t *ctl, int *device);
int snd_ctl_pcm_surround_next_device(snd_ctl_t *ctl, snd_pcm_surround_type_t type, int *device);
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

unsigned int snd_ctl_event_elem_get_mask(const snd_ctl_event_t *obj);
unsigned int snd_ctl_event_elem_get_numid(const snd_ctl_event_t *obj);
void snd_ctl_event_elem_get_id(const snd_ctl_event_t *obj, snd_ctl_elem_id_t *ptr);
snd_ctl_elem_iface_t snd_ctl_event_elem_get_interface(const snd_ctl_event_t *obj);
unsigned int snd_ctl_event_elem_get_device(const snd_ctl_event_t *obj);
unsigned int snd_ctl_event_elem_get_subdevice(const snd_ctl_event_t *obj);
const char *snd_ctl_event_elem_get_name(const snd_ctl_event_t *obj);
unsigned int snd_ctl_event_elem_get_index(const snd_ctl_event_t *obj);

int snd_ctl_elem_list_alloc_space(snd_ctl_elem_list_t *obj, unsigned int entries);
void snd_ctl_elem_list_free_space(snd_ctl_elem_list_t *obj);

#ifdef __cplusplus
}
#endif

/*
 *  Highlevel API for controls
 */

/** HCTL element handle */
typedef struct _snd_hctl_elem snd_hctl_elem_t;

/** HCTL handle */
typedef struct _snd_hctl snd_hctl_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Compare function for sorting HCTL elements
 * \param e1 First element
 * \param e2 Second element
 * \return -1 if e1 < e2, 0 if e1 == e2, 1 if e1 > e2
 */
typedef int (*snd_hctl_compare_t)(const snd_hctl_elem_t *e1,
				  const snd_hctl_elem_t *e2);
int snd_hctl_compare_fast(const snd_hctl_elem_t *c1,
			  const snd_hctl_elem_t *c2);
/** 
 * \brief HCTL callback function
 * \param hctl HCTL handle
 * \param mask event mask
 * \param elem related HCTL element (if any)
 * \return 0 on success otherwise a negative error code
 */
typedef int (*snd_hctl_callback_t)(snd_hctl_t *hctl,
				   unsigned int mask,
				   snd_hctl_elem_t *elem);
/** 
 * \brief HCTL element callback function
 * \param elem HCTL element
 * \param mask event mask
 * \return 0 on success otherwise a negative error code
 */
typedef int (*snd_hctl_elem_callback_t)(snd_hctl_elem_t *elem,
					unsigned int mask);

int snd_hctl_open(snd_hctl_t **hctl, const char *name, int mode);
int snd_hctl_close(snd_hctl_t *hctl);
int snd_hctl_nonblock(snd_hctl_t *hctl, int nonblock);
int snd_hctl_async(snd_hctl_t *hctl, int sig, pid_t pid);
int snd_hctl_poll_descriptors_count(snd_hctl_t *hctl);
int snd_hctl_poll_descriptors(snd_hctl_t *hctl, struct pollfd *pfds, unsigned int space);
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
int snd_hctl_handle_events(snd_hctl_t *hctl);
const char *snd_hctl_name(snd_hctl_t *hctl);
snd_ctl_type_t snd_hctl_type(snd_hctl_t *hctl);
int snd_hctl_wait(snd_hctl_t *hctl, int timeout);

snd_hctl_elem_t *snd_hctl_elem_next(snd_hctl_elem_t *elem);
snd_hctl_elem_t *snd_hctl_elem_prev(snd_hctl_elem_t *elem);
int snd_hctl_elem_info(snd_hctl_elem_t *elem, snd_ctl_elem_info_t * info);
int snd_hctl_elem_read(snd_hctl_elem_t *elem, snd_ctl_elem_value_t * value);
int snd_hctl_elem_write(snd_hctl_elem_t *elem, snd_ctl_elem_value_t * value);

snd_hctl_t *snd_hctl_elem_get_hctl(snd_hctl_elem_t *elem);

size_t snd_ctl_elem_id_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_ctl_elem_id_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_ctl_elem_id_alloca(ptr) do { assert(ptr); *ptr = (snd_ctl_elem_id_t *) alloca(snd_ctl_elem_id_sizeof()); memset(*ptr, 0, snd_ctl_elem_id_sizeof()); } while (0)
int snd_ctl_elem_id_malloc(snd_ctl_elem_id_t **ptr);
void snd_ctl_elem_id_free(snd_ctl_elem_id_t *obj);
void snd_ctl_elem_id_copy(snd_ctl_elem_id_t *dst, const snd_ctl_elem_id_t *src);

unsigned int snd_ctl_elem_id_get_numid(const snd_ctl_elem_id_t *obj);

snd_ctl_elem_iface_t snd_ctl_elem_id_get_interface(const snd_ctl_elem_id_t *obj);

unsigned int snd_ctl_elem_id_get_device(const snd_ctl_elem_id_t *obj);

unsigned int snd_ctl_elem_id_get_subdevice(const snd_ctl_elem_id_t *obj);

const char *snd_ctl_elem_id_get_name(const snd_ctl_elem_id_t *obj);

unsigned int snd_ctl_elem_id_get_index(const snd_ctl_elem_id_t *obj);

void snd_ctl_elem_id_set_numid(snd_ctl_elem_id_t *obj, unsigned int val);

void snd_ctl_elem_id_set_interface(snd_ctl_elem_id_t *obj, snd_ctl_elem_iface_t val);

void snd_ctl_elem_id_set_device(snd_ctl_elem_id_t *obj, unsigned int val);

void snd_ctl_elem_id_set_subdevice(snd_ctl_elem_id_t *obj, unsigned int val);

void snd_ctl_elem_id_set_name(snd_ctl_elem_id_t *obj, const char *val);

void snd_ctl_elem_id_set_index(snd_ctl_elem_id_t *obj, unsigned int val);

size_t snd_ctl_card_info_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_ctl_card_info_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_ctl_card_info_alloca(ptr) do { assert(ptr); *ptr = (snd_ctl_card_info_t *) alloca(snd_ctl_card_info_sizeof()); memset(*ptr, 0, snd_ctl_card_info_sizeof()); } while (0)
int snd_ctl_card_info_malloc(snd_ctl_card_info_t **ptr);
void snd_ctl_card_info_free(snd_ctl_card_info_t *obj);
void snd_ctl_card_info_copy(snd_ctl_card_info_t *dst, const snd_ctl_card_info_t *src);

int snd_ctl_card_info_get_card(const snd_ctl_card_info_t *obj);

snd_card_type_t snd_ctl_card_info_get_type(const snd_ctl_card_info_t *obj);

const char *snd_ctl_card_info_get_id(const snd_ctl_card_info_t *obj);

const char *snd_ctl_card_info_get_abbreviation(const snd_ctl_card_info_t *obj);

const char *snd_ctl_card_info_get_name(const snd_ctl_card_info_t *obj);

const char *snd_ctl_card_info_get_longname(const snd_ctl_card_info_t *obj);

const char *snd_ctl_card_info_get_mixerid(const snd_ctl_card_info_t *obj);

const char *snd_ctl_card_info_get_mixername(const snd_ctl_card_info_t *obj);

size_t snd_ctl_event_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_ctl_event_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_ctl_event_alloca(ptr) do { assert(ptr); *ptr = (snd_ctl_event_t *) alloca(snd_ctl_event_sizeof()); memset(*ptr, 0, snd_ctl_event_sizeof()); } while (0)
int snd_ctl_event_malloc(snd_ctl_event_t **ptr);
void snd_ctl_event_free(snd_ctl_event_t *obj);
void snd_ctl_event_copy(snd_ctl_event_t *dst, const snd_ctl_event_t *src);

snd_ctl_event_type_t snd_ctl_event_get_type(const snd_ctl_event_t *obj);

size_t snd_ctl_elem_list_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_ctl_elem_list_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_ctl_elem_list_alloca(ptr) do { assert(ptr); *ptr = (snd_ctl_elem_list_t *) alloca(snd_ctl_elem_list_sizeof()); memset(*ptr, 0, snd_ctl_elem_list_sizeof()); } while (0)
int snd_ctl_elem_list_malloc(snd_ctl_elem_list_t **ptr);
void snd_ctl_elem_list_free(snd_ctl_elem_list_t *obj);
void snd_ctl_elem_list_copy(snd_ctl_elem_list_t *dst, const snd_ctl_elem_list_t *src);

void snd_ctl_elem_list_set_offset(snd_ctl_elem_list_t *obj, unsigned int val);

unsigned int snd_ctl_elem_list_get_used(const snd_ctl_elem_list_t *obj);

unsigned int snd_ctl_elem_list_get_count(const snd_ctl_elem_list_t *obj);

void snd_ctl_elem_list_get_id(const snd_ctl_elem_list_t *obj, unsigned int idx, snd_ctl_elem_id_t *ptr);

unsigned int snd_ctl_elem_list_get_numid(const snd_ctl_elem_list_t *obj, unsigned int idx);

snd_ctl_elem_iface_t snd_ctl_elem_list_get_interface(const snd_ctl_elem_list_t *obj, unsigned int idx);

unsigned int snd_ctl_elem_list_get_device(const snd_ctl_elem_list_t *obj, unsigned int idx);

unsigned int snd_ctl_elem_list_get_subdevice(const snd_ctl_elem_list_t *obj, unsigned int idx);

const char *snd_ctl_elem_list_get_name(const snd_ctl_elem_list_t *obj, unsigned int idx);

unsigned int snd_ctl_elem_list_get_index(const snd_ctl_elem_list_t *obj, unsigned int idx);

size_t snd_ctl_elem_info_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_ctl_elem_info_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_ctl_elem_info_alloca(ptr) do { assert(ptr); *ptr = (snd_ctl_elem_info_t *) alloca(snd_ctl_elem_info_sizeof()); memset(*ptr, 0, snd_ctl_elem_info_sizeof()); } while (0)
int snd_ctl_elem_info_malloc(snd_ctl_elem_info_t **ptr);
void snd_ctl_elem_info_free(snd_ctl_elem_info_t *obj);
void snd_ctl_elem_info_copy(snd_ctl_elem_info_t *dst, const snd_ctl_elem_info_t *src);

snd_ctl_elem_type_t snd_ctl_elem_info_get_type(const snd_ctl_elem_info_t *obj);

int snd_ctl_elem_info_is_readable(const snd_ctl_elem_info_t *obj);

int snd_ctl_elem_info_is_writable(const snd_ctl_elem_info_t *obj);

int snd_ctl_elem_info_is_volatile(const snd_ctl_elem_info_t *obj);

int snd_ctl_elem_info_is_inactive(const snd_ctl_elem_info_t *obj);

int snd_ctl_elem_info_is_locked(const snd_ctl_elem_info_t *obj);

int snd_ctl_elem_info_is_indirect(const snd_ctl_elem_info_t *obj);

unsigned int snd_ctl_elem_info_get_count(const snd_ctl_elem_info_t *obj);

long snd_ctl_elem_info_get_min(const snd_ctl_elem_info_t *obj);

long snd_ctl_elem_info_get_max(const snd_ctl_elem_info_t *obj);

long snd_ctl_elem_info_get_step(const snd_ctl_elem_info_t *obj);

unsigned int snd_ctl_elem_info_get_items(const snd_ctl_elem_info_t *obj);

void snd_ctl_elem_info_set_item(snd_ctl_elem_info_t *obj, unsigned int val);

const char *snd_ctl_elem_info_get_item_name(const snd_ctl_elem_info_t *obj);

void snd_ctl_elem_info_get_id(const snd_ctl_elem_info_t *obj, snd_ctl_elem_id_t *ptr);

unsigned int snd_ctl_elem_info_get_numid(const snd_ctl_elem_info_t *obj);

snd_ctl_elem_iface_t snd_ctl_elem_info_get_interface(const snd_ctl_elem_info_t *obj);

unsigned int snd_ctl_elem_info_get_device(const snd_ctl_elem_info_t *obj);

unsigned int snd_ctl_elem_info_get_subdevice(const snd_ctl_elem_info_t *obj);

const char *snd_ctl_elem_info_get_name(const snd_ctl_elem_info_t *obj);

unsigned int snd_ctl_elem_info_get_index(const snd_ctl_elem_info_t *obj);

void snd_ctl_elem_info_set_id(snd_ctl_elem_info_t *obj, const snd_ctl_elem_id_t *ptr);

void snd_ctl_elem_info_set_numid(snd_ctl_elem_info_t *obj, unsigned int val);

void snd_ctl_elem_info_set_interface(snd_ctl_elem_info_t *obj, snd_ctl_elem_iface_t val);

void snd_ctl_elem_info_set_device(snd_ctl_elem_info_t *obj, unsigned int val);

void snd_ctl_elem_info_set_subdevice(snd_ctl_elem_info_t *obj, unsigned int val);

void snd_ctl_elem_info_set_name(snd_ctl_elem_info_t *obj, const char *val);

void snd_ctl_elem_info_set_index(snd_ctl_elem_info_t *obj, unsigned int val);

size_t snd_ctl_elem_value_sizeof(void);
/** \hideinitializer
 * \brief allocate an invalid #snd_ctl_elem_value_t using standard alloca
 * \param ptr returned pointer
 */
#define snd_ctl_elem_value_alloca(ptr) do { assert(ptr); *ptr = (snd_ctl_elem_value_t *) alloca(snd_ctl_elem_value_sizeof()); memset(*ptr, 0, snd_ctl_elem_value_sizeof()); } while (0)
int snd_ctl_elem_value_malloc(snd_ctl_elem_value_t **ptr);
void snd_ctl_elem_value_free(snd_ctl_elem_value_t *obj);
void snd_ctl_elem_value_copy(snd_ctl_elem_value_t *dst, const snd_ctl_elem_value_t *src);

void snd_ctl_elem_value_get_id(const snd_ctl_elem_value_t *obj, snd_ctl_elem_id_t *ptr);

unsigned int snd_ctl_elem_value_get_numid(const snd_ctl_elem_value_t *obj);

snd_ctl_elem_iface_t snd_ctl_elem_value_get_interface(const snd_ctl_elem_value_t *obj);

unsigned int snd_ctl_elem_value_get_device(const snd_ctl_elem_value_t *obj);

unsigned int snd_ctl_elem_value_get_subdevice(const snd_ctl_elem_value_t *obj);

const char *snd_ctl_elem_value_get_name(const snd_ctl_elem_value_t *obj);

unsigned int snd_ctl_elem_value_get_index(const snd_ctl_elem_value_t *obj);

void snd_ctl_elem_value_set_id(snd_ctl_elem_value_t *obj, const snd_ctl_elem_id_t *ptr);

void snd_ctl_elem_value_set_numid(snd_ctl_elem_value_t *obj, unsigned int val);

void snd_ctl_elem_value_set_interface(snd_ctl_elem_value_t *obj, snd_ctl_elem_iface_t val);

void snd_ctl_elem_value_set_device(snd_ctl_elem_value_t *obj, unsigned int val);

void snd_ctl_elem_value_set_subdevice(snd_ctl_elem_value_t *obj, unsigned int val);

void snd_ctl_elem_value_set_name(snd_ctl_elem_value_t *obj, const char *val);

void snd_ctl_elem_value_set_index(snd_ctl_elem_value_t *obj, unsigned int val);

int snd_ctl_elem_value_get_boolean(const snd_ctl_elem_value_t *obj, unsigned int idx);

long snd_ctl_elem_value_get_integer(const snd_ctl_elem_value_t *obj, unsigned int idx);

unsigned int snd_ctl_elem_value_get_enumerated(const snd_ctl_elem_value_t *obj, unsigned int idx);

unsigned char snd_ctl_elem_value_get_byte(const snd_ctl_elem_value_t *obj, unsigned int idx);

void snd_ctl_elem_value_set_boolean(snd_ctl_elem_value_t *obj, unsigned int idx, long val);

void snd_ctl_elem_value_set_integer(snd_ctl_elem_value_t *obj, unsigned int idx, long val);

void snd_ctl_elem_value_set_enumerated(snd_ctl_elem_value_t *obj, unsigned int idx, unsigned int val);

void snd_ctl_elem_value_set_byte(snd_ctl_elem_value_t *obj, unsigned int idx, unsigned char val);

const void * snd_ctl_elem_value_get_bytes(const snd_ctl_elem_value_t *obj);

void snd_ctl_elem_value_get_iec958(const snd_ctl_elem_value_t *obj, snd_aes_iec958_t *ptr);

void snd_ctl_elem_value_set_iec958(snd_ctl_elem_value_t *obj, const snd_aes_iec958_t *ptr);

void snd_hctl_elem_get_id(const snd_hctl_elem_t *obj, snd_ctl_elem_id_t *ptr);

unsigned int snd_hctl_elem_get_numid(const snd_hctl_elem_t *obj);

snd_ctl_elem_iface_t snd_hctl_elem_get_interface(const snd_hctl_elem_t *obj);

unsigned int snd_hctl_elem_get_device(const snd_hctl_elem_t *obj);

unsigned int snd_hctl_elem_get_subdevice(const snd_hctl_elem_t *obj);

const char *snd_hctl_elem_get_name(const snd_hctl_elem_t *obj);

unsigned int snd_hctl_elem_get_index(const snd_hctl_elem_t *obj);

void snd_hctl_elem_set_callback(snd_hctl_elem_t *obj, snd_hctl_elem_callback_t val);

void * snd_hctl_elem_get_callback_private(const snd_hctl_elem_t *obj);

void snd_hctl_elem_set_callback_private(snd_hctl_elem_t *obj, void * val);

#ifdef __cplusplus
}
#endif

