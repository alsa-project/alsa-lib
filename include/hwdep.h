/****************************************************************************
 *                                                                          *
 *                               hwdep.h                                    *
 *                     Hardware depedent interface                          *
 *                                                                          *
 ****************************************************************************/

#define SND_HWDEP_OPEN_READ		(O_RDONLY)
#define SND_HWDEP_OPEN_WRITE		(O_WRONLY)
#define SND_HWDEP_OPEN_DUPLEX		(O_RDWR)
#define SND_HWDEP_OPEN_NONBLOCK		(O_NONBLOCK)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_hwdep snd_hwdep_t;

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

