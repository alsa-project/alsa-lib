/****************************************************************************
 *                                                                          *
 *                               hwdep.h                                    *
 *                     Hardware depedent interface                          *
 *                                                                          *
 ****************************************************************************/

/* sndrv aliasing */
typedef enum sndrv_hwdep_type snd_hwdep_type_t;
typedef struct sndrv_hwdep_info snd_hwdep_info_t;
#define SND_HWDEP_TYPE_OPL2 SNDRV_HWDEP_TYPE_OPL2
#define SND_HWDEP_TYPE_OPL3 SNDRV_HWDEP_TYPE_OPL3
#define SND_HWDEP_TYPE_OPL4 SNDRV_HWDEP_TYPE_OPL4
#define SND_HWDEP_TYPE_SB16CSP SNDRV_HWDEP_TYPE_SB16CSP
#define SND_HWDEP_TYPE_EMU10K1 SNDRV_HWDEP_TYPE_EMU10K1
#define SND_HWDEP_TYPE_YSS225 SNDRV_HWDEP_TYPE_YSS225
#define SND_HWDEP_TYPE_ICS2115 SNDRV_HWDEP_TYPE_ICS2115
#define SND_HWDEP_TYPE_LAST SNDRV_HWDEP_TYPE_LAST

#define SND_HWDEP_OPEN_READ		(O_RDONLY)
#define SND_HWDEP_OPEN_WRITE		(O_WRONLY)
#define SND_HWDEP_OPEN_DUPLEX		(O_RDWR)
#define SND_HWDEP_OPEN_NONBLOCK		(O_NONBLOCK)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _snd_hwdep snd_hwdep_t;

int snd_hwdep_open(snd_hwdep_t **handle, int card, int device, int mode);
int snd_hwdep_close(snd_hwdep_t *handle);
int snd_hwdep_poll_descriptor(snd_hwdep_t *handle);
int snd_hwdep_block_mode(snd_hwdep_t *handle, int enable);
int snd_hwdep_info(snd_hwdep_t *handle, snd_hwdep_info_t * info);
int snd_hwdep_ioctl(snd_hwdep_t *handle, int request, void * arg);
ssize_t snd_hwdep_write(snd_hwdep_t *handle, const void *buffer, size_t size);
ssize_t snd_hwdep_read(snd_hwdep_t *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

