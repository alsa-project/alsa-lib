/****************************************************************************
 *                                                                          *
 *                               timer.h                                    *
 *                           Timer interface                                *
 *                                                                          *
 ****************************************************************************/

/**
 *  \defgroup Timer Timer Interface
 *  Timer Interface
 *  \{
 */

/** timer identification */
typedef struct sndrv_timer_id snd_timer_id_t;
/** timer select structure */
typedef struct sndrv_timer_select snd_timer_select_t;
/** timer info structure */
typedef struct sndrv_timer_info snd_timer_info_t;
/** timer parameters structure */
typedef struct sndrv_timer_params snd_timer_params_t;
/** timer status structure */
typedef struct sndrv_timer_status snd_timer_status_t;
/** timer read structure */
typedef struct sndrv_timer_read snd_timer_read_t;

/** timer master class */
typedef enum _snd_timer_class {
	SND_TIMER_CLASS_NONE = SNDRV_TIMER_CLASS_NONE,		/**< invalid */
	SND_TIMER_CLASS_SLAVE = SNDRV_TIMER_CLASS_SLAVE,	/**< slave timer */
	SND_TIMER_CLASS_GLOBAL = SNDRV_TIMER_CLASS_GLOBAL,	/**< global timer */
	SND_TIMER_CLASS_CARD = SNDRV_TIMER_CLASS_CARD,		/**< card timer */
	SND_TIMER_CLASS_PCM = SNDRV_TIMER_CLASS_PCM,		/**< PCM timer */
	SND_TIMER_CLASS_LAST = SNDRV_TIMER_CLASS_LAST,		/**< last timer */
} snd_timer_class_t;

/** timer slave class */
typedef enum _snd_timer_slave_class {
	SND_TIMER_SCLASS_NONE = SNDRV_TIMER_SCLASS_NONE,		/**< none */
	SND_TIMER_SCLASS_APPLICATION = SNDRV_TIMER_SCLASS_APPLICATION,	/**< for internal use */
	SND_TIMER_SCLASS_SEQUENCER = SNDRV_TIMER_SCLASS_SEQUENCER,	/**< sequencer timer */
	SND_TIMER_SCLASS_OSS_SEQUENCER = SNDRV_TIMER_SCLASS_OSS_SEQUENCER, /**< OSS sequencer timer */
	SND_TIMER_SCLASS_LAST = SNDRV_TIMER_SCLASS_LAST,		/**< last slave timer */
} snd_timer_slave_class_t;

/** global timer - system */
#define SND_TIMER_GLOBAL_SYSTEM SNDRV_TIMER_GLOBAL_SYSTEM
/** global timer - RTC */
#define SND_TIMER_GLOBAL_RTC SNDRV_TIMER_GLOBAL_RTC

/** timer cannot be controlled */
#define SND_TIMER_FLG_SLAVE SNDRV_TIMER_FLG_SLAVE
/** timer supports auto-start */
#define SND_TIMER_PSFLG_AUTO SNDRV_TIMER_PSFLG_AUTO

/** timer open mode flag - nonblock */
#define SND_TIMER_OPEN_NONBLOCK		1

/** timer handle type */
typedef enum _snd_timer_type {
	/** Kernel level HwDep */
	SND_TIMER_TYPE_HW,
	/** Shared memory client timer (not yet implemented) */
	SND_TIMER_TYPE_SHM,
	/** INET client timer (not yet implemented) */
	SND_TIMER_TYPE_INET,
} snd_timer_type_t;

/** timer query handle */
typedef struct _snd_timer_query snd_timer_query_t;
/** timer handle */
typedef struct _snd_timer snd_timer_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_timer_query_open(snd_timer_query_t **handle, const char *name, int mode);
int snd_timer_query_close(snd_timer_query_t *handle);
int snd_timer_query_next_device(snd_timer_query_t *handle, snd_timer_id_t *tid);

int snd_timer_open(snd_timer_t **handle, const char *name, int mode);
int snd_timer_close(snd_timer_t *handle);
int snd_timer_poll_descriptors_count(snd_timer_t *handle);
int snd_timer_poll_descriptors(snd_timer_t *handle, struct pollfd *pfds, unsigned int space);
int snd_timer_info(snd_timer_t *handle, snd_timer_info_t *timer);
int snd_timer_params(snd_timer_t *handle, snd_timer_params_t *params);
int snd_timer_status(snd_timer_t *handle, snd_timer_status_t *status);
int snd_timer_start(snd_timer_t *handle);
int snd_timer_stop(snd_timer_t *handle);
int snd_timer_continue(snd_timer_t *handle);
ssize_t snd_timer_read(snd_timer_t *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

/** \} */

