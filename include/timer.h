/****************************************************************************
 *                                                                          *
 *                               timer.h                                    *
 *                           Timer interface                                *
 *                                                                          *
 ****************************************************************************/

typedef struct sndrv_timer_id snd_timer_id_t;
typedef struct sndrv_timer_select snd_timer_select_t;
typedef struct sndrv_timer_info snd_timer_info_t;
typedef struct sndrv_timer_params snd_timer_params_t;
typedef struct sndrv_timer_status snd_timer_status_t;
typedef struct sndrv_timer_read snd_timer_read_t;

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_timer_type *snd_timer_type_t;
typedef struct __snd_timer_slave_type *snd_timer_slave_type_t;
#else
typedef enum sndrv_timer_type snd_timer_type_t;
typedef enum sndrv_timer_slave_type snd_timer_slave_type_t;
#endif

#define SND_TIMER_TYPE_NONE ((snd_timer_type_t) SNDRV_TIMER_TYPE_NONE)
#define SND_TIMER_TYPE_SLAVE ((snd_timer_type_t) SNDRV_TIMER_TYPE_SLAVE)
#define SND_TIMER_TYPE_GLOBAL ((snd_timer_type_t) SNDRV_TIMER_TYPE_GLOBAL)
#define SND_TIMER_TYPE_CARD ((snd_timer_type_t) SNDRV_TIMER_TYPE_CARD)
#define SND_TIMER_TYPE_PCM ((snd_timer_type_t) SNDRV_TIMER_TYPE_PCM)
#define SND_TIMER_TYPE_LAST ((snd_timer_type_t) SNDRV_TIMER_TYPE_LAST)

#define SND_TIMER_STYPE_NONE ((snd_timer_slave_type_t) SNDRV_TIMER_STYPE_NONE
#define SND_TIMER_STYPE_APPLICATION ((snd_timer_slave_type_t) SNDRV_TIMER_STYPE_APPLICATION
#define SND_TIMER_STYPE_SEQUENCER ((snd_timer_slave_type_t) SNDRV_TIMER_STYPE_SEQUENCER
#define SND_TIMER_STYPE_OSS_SEQUENCER ((snd_timer_slave_type_t) SNDRV_TIMER_STYPE_OSS_SEQUENCER
#define SND_TIMER_STYPE_LAST ((snd_timer_slave_type_t) SNDRV_TIMER_STYPE_LAST

#define SND_TIMER_GLOBAL_SYSTEM SNDRV_TIMER_GLOBAL_SYSTEM
#define SND_TIMER_GLOBAL_RTC SNDRV_TIMER_GLOBAL_RTC

#define SND_TIMER_FLG_SLAVE SNDRV_TIMER_FLG_SLAVE
#define SND_TIMER_PARBIT_FLAGS SNDRV_TIMER_PARBIT_FLAGS
#define SND_TIMER_PARBIT_TICKS SNDRV_TIMER_PARBIT_TICKS
#define SND_TIMER_PARBIT_QUEUE_SIZE SNDRV_TIMER_PARBIT_QUEUE_SIZE
#define SND_TIMER_PSFLG_AUTO SNDRV_TIMER_PSFLG_AUTO

typedef struct _snd_timer snd_timer_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_timer_open(snd_timer_t **handle);
int snd_timer_close(snd_timer_t *handle);
int snd_timer_poll_descriptors_count(snd_timer_t *handle);
int snd_timer_poll_descriptors(snd_timer_t *handle, struct pollfd *pfds, unsigned int space);
int snd_timer_next_device(snd_timer_t *handle, snd_timer_id_t *tid);
int snd_timer_select(snd_timer_t *handle, snd_timer_select_t *tselect);
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

