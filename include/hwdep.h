/****************************************************************************
 *                                                                          *
 *                               hwdep.h                                    *
 *                     Hardware depedent interface                          *
 *                                                                          *
 ****************************************************************************/

typedef struct _snd_hwdep_info snd_hwdep_info_t;

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_hwdep_type *snd_hwdep_type_t;
#else
typedef enum sndrv_hwdep_type snd_hwdep_type_t;
#endif

#define SND_HWDEP_TYPE_OPL2 ((snd_hwdep_type_t) SNDRV_HWDEP_TYPE_OPL2)
#define SND_HWDEP_TYPE_OPL3 ((snd_hwdep_type_t) SNDRV_HWDEP_TYPE_OPL3)
#define SND_HWDEP_TYPE_OPL4 ((snd_hwdep_type_t) SNDRV_HWDEP_TYPE_OPL4)
#define SND_HWDEP_TYPE_SB16CSP ((snd_hwdep_type_t) SNDRV_HWDEP_TYPE_SB16CSP)
#define SND_HWDEP_TYPE_EMU10K1 ((snd_hwdep_type_t) SNDRV_HWDEP_TYPE_EMU10K1)
#define SND_HWDEP_TYPE_YSS225 ((snd_hwdep_type_t) SNDRV_HWDEP_TYPE_YSS225)
#define SND_HWDEP_TYPE_ICS2115 ((snd_hwdep_type_t) SNDRV_HWDEP_TYPE_ICS2115)
#define SND_HWDEP_TYPE_LAST ((snd_hwdep_type_t) SNDRV_HWDEP_TYPE_LAST)

#define SND_HWDEP_OPEN_READ		(O_RDONLY)
#define SND_HWDEP_OPEN_WRITE		(O_WRONLY)
#define SND_HWDEP_OPEN_DUPLEX		(O_RDWR)
#define SND_HWDEP_OPEN_NONBLOCK		(O_NONBLOCK)

typedef struct _snd_hwdep snd_hwdep_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_hwdep_open(snd_hwdep_t **hwdep, int card, int device, int mode);
int snd_hwdep_close(snd_hwdep_t *hwdep);
int snd_hwdep_poll_descriptors(snd_hwdep_t *hwdep, struct pollfd *pfds, unsigned int space);
int snd_hwdep_block_mode(snd_hwdep_t *hwdep, int enable);
int snd_hwdep_info(snd_hwdep_t *hwdep, snd_hwdep_info_t * info);
int snd_hwdep_ioctl(snd_hwdep_t *hwdep, int request, void * arg);
ssize_t snd_hwdep_write(snd_hwdep_t *hwdep, const void *buffer, size_t size);
ssize_t snd_hwdep_read(snd_hwdep_t *hwdep, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

