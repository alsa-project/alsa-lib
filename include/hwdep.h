/****************************************************************************
 *                                                                          *
 *                               hwdep.h                                    *
 *                     Hardware depedent interface                          *
 *                                                                          *
 ****************************************************************************/

/**
 *  \defgroup HwDep Hardware Dependant Interface
 *  The Hardware Dependant Interface.
 *  \{
 */

/** dlsym version for interface entry callback */
#define SND_HWDEP_DLSYM_VERSION		_dlsym_hwdep_001

/** HwDep information container */
typedef struct _snd_hwdep_info snd_hwdep_info_t;

/** HwDep interface */
typedef enum _snd_hwdep_iface {
	SND_HWDEP_IFACE_OPL2 = 0,	/**< OPL2 raw driver */
	SND_HWDEP_IFACE_OPL3,		/**< OPL3 raw driver */
	SND_HWDEP_IFACE_OPL4,		/**< OPL4 raw driver */
	SND_HWDEP_IFACE_SB16CSP,	/**< SB16CSP driver */
	SND_HWDEP_IFACE_EMU10K1,	/**< EMU10K1 driver */
	SND_HWDEP_IFACE_YSS225,		/**< YSS225 driver */
	SND_HWDEP_IFACE_ICS2115,	/**< ICS2115 driver */
	SND_HWDEP_IFACE_LAST = SND_HWDEP_IFACE_ICS2115,  /**< last know hwdep interface */
} snd_hwdep_iface_t;

/** open for reading */
#define SND_HWDEP_OPEN_READ		(O_RDONLY)
/** open for writing */
#define SND_HWDEP_OPEN_WRITE		(O_WRONLY)
/** open for reading and writing */
#define SND_HWDEP_OPEN_DUPLEX		(O_RDWR)
/** flag: open in nonblock mode */
#define SND_HWDEP_OPEN_NONBLOCK		(O_NONBLOCK)

/** HwDep handle type */
typedef enum _snd_hwdep_type {
	/** Kernel level HwDep */
	SND_HWDEP_TYPE_HW,
	/** Shared memory client HwDep (not yet implemented) */
	SND_HWDEP_TYPE_SHM,
	/** INET client HwDep (not yet implemented) */
	SND_HWDEP_TYPE_INET,
} snd_hwdep_type_t;

/** HwDep handle */
typedef struct _snd_hwdep snd_hwdep_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_hwdep_open(snd_hwdep_t **hwdep, const char *name, int mode);
int snd_hwdep_close(snd_hwdep_t *hwdep);
int snd_hwdep_poll_descriptors(snd_hwdep_t *hwdep, struct pollfd *pfds, unsigned int space);
int snd_hwdep_nonblock(snd_hwdep_t *hwdep, int nonblock);
int snd_hwdep_info(snd_hwdep_t *hwdep, snd_hwdep_info_t * info);
int snd_hwdep_ioctl(snd_hwdep_t *hwdep, unsigned int request, void * arg);
ssize_t snd_hwdep_write(snd_hwdep_t *hwdep, const void *buffer, size_t size);
ssize_t snd_hwdep_read(snd_hwdep_t *hwdep, void *buffer, size_t size);

size_t snd_hwdep_info_sizeof(void);
/** allocate #snd_hwdep_info_t container on stack */
#define snd_hwdep_info_alloca(ptr) do { assert(ptr); *ptr = (snd_hwdep_info_t *) alloca(snd_hwdep_info_sizeof()); memset(*ptr, 0, snd_hwdep_info_sizeof()); } while (0)
int snd_hwdep_info_malloc(snd_hwdep_info_t **ptr);
void snd_hwdep_info_free(snd_hwdep_info_t *obj);
void snd_hwdep_info_copy(snd_hwdep_info_t *dst, const snd_hwdep_info_t *src);

unsigned int snd_hwdep_info_get_device(const snd_hwdep_info_t *obj);
int snd_hwdep_info_get_card(const snd_hwdep_info_t *obj);
const char *snd_hwdep_info_get_id(const snd_hwdep_info_t *obj);
const char *snd_hwdep_info_get_name(const snd_hwdep_info_t *obj);
snd_hwdep_iface_t snd_hwdep_info_get_iface(const snd_hwdep_info_t *obj);
void snd_hwdep_info_set_device(snd_hwdep_info_t *obj, unsigned int val);

#ifdef __cplusplus
}
#endif

/** \} */

