/**
 *  \defgroup Global Global defines
 *  Global defines
 *  \{
 */

#ifdef SNDRV_LITTLE_ENDIAN
#define SND_LITTLE_ENDIAN SNDRV_LITTLE_ENDIAN
#endif

#ifdef SNDRV_BIG_ENDIAN
#define SND_BIG_ENDIAN SNDRV_BIG_ENDIAN
#endif

/** \} */

/** helper macro for SND_DLSYM_BUILD_VERSION */
#define __SND_DLSYM_VERSION(name, version) _ ## name ## version
/** build version for versioned dynamic symbol */
#define SND_DLSYM_BUILD_VERSION(name, version) char __SND_DLSYM_VERSION(name, version)
/** get version of dynamic symbol as string */
#define SND_DLSYM_VERSION(version) __STRING(version)

int snd_dlsym_verify(void *handle, const char *name, const char *version);

/** Async notification client handler */
typedef struct _snd_async_handler snd_async_handler_t;

/** Async notification callback */
typedef void (*snd_async_callback_t)(snd_async_handler_t *handler);

int snd_async_add_handler(snd_async_handler_t **handler, int fd, 
			  snd_async_callback_t callback, void *private_data);
int snd_async_del_handler(snd_async_handler_t *handler);
int snd_async_handler_get_fd(snd_async_handler_t *handler);
void *snd_async_handler_get_callback_private(snd_async_handler_t *handler);
