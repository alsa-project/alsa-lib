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

/** Async notification client handler */
typedef struct _snd_async_handler snd_async_handler_t;

/** Async notification callback */
typedef void (*snd_async_callback_t)(snd_async_handler_t *handler);

int snd_async_add_handler(snd_async_handler_t **handler, int fd, 
			  snd_async_callback_t callback, void *private_data);
int snd_async_del_handler(snd_async_handler_t *handler);
int snd_async_handler_get_fd(snd_async_handler_t *handler);
void *snd_async_handler_get_callback_private(snd_async_handler_t *handler);
