/****************************************************************************
 *                                                                          *
 *                               mixer.h                                    *
 *                           Mixer Interface                                *
 *                                                                          *
 ****************************************************************************/

typedef struct _snd_mixer snd_mixer_t;
typedef struct _snd_mixer_info snd_mixer_info_t;
typedef struct _snd_mixer_elem snd_mixer_elem_t;
typedef int (*snd_mixer_callback_t)(snd_mixer_t *ctl,
				    snd_ctl_event_type_t event,
				    snd_mixer_elem_t *elem);
typedef int (*snd_mixer_elem_callback_t)(snd_mixer_elem_t *elem,
					    snd_ctl_event_type_t event);

enum _snd_mixer_elem_type {
	SND_MIXER_ELEM_SIMPLE,
	SND_MIXER_ELEM_LAST = SND_MIXER_ELEM_SIMPLE,
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_mixer_elem_type *snd_mixer_elem_type_t;
#else
typedef enum _snd_mixer_elem_type snd_mixer_elem_type_t;
#endif

#define SND_MIXER_ELEM_SIMPLE ((snd_mixer_elem_type_t) SND_MIXER_ELEM_SIMPLE)
#define SND_MIXER_ELEM_LAST ((snd_mixer_elem_type_t) SND_MIXER_ELEM_LAST)


#ifdef __cplusplus
extern "C" {
#endif

int snd_mixer_open(snd_mixer_t **mixer, char *name);
int snd_mixer_close(snd_mixer_t *mixer);
int snd_mixer_poll_descriptor(snd_mixer_t *mixer);
int snd_mixer_info(snd_mixer_t *mixer, snd_mixer_info_t *info);
snd_mixer_elem_t *snd_mixer_first_elem(snd_mixer_t *mixer);
snd_mixer_elem_t *snd_mixer_last_elem(snd_mixer_t *mixer);
snd_mixer_elem_t *snd_mixer_elem_next(snd_mixer_elem_t *elem);
snd_mixer_elem_t *snd_mixer_elem_prev(snd_mixer_elem_t *helem);
int snd_mixer_events(snd_mixer_t *mixer);

#ifdef __cplusplus
}
#endif

/*
 *  Simple (legacy) mixer API
 */

enum _snd_mixer_channel_id {
	SND_MIXER_CHN_UNKNOWN = -1,
	SND_MIXER_CHN_FRONT_LEFT = 0,
	SND_MIXER_CHN_FRONT_RIGHT,
	SND_MIXER_CHN_FRONT_CENTER,
	SND_MIXER_CHN_REAR_LEFT,
	SND_MIXER_CHN_REAR_RIGHT,
	SND_MIXER_CHN_WOOFER,
	SND_MIXER_CHN_LAST = 31,
	SND_MIXER_CHN_MONO = SND_MIXER_CHN_FRONT_LEFT
};

/* Simple mixer */

typedef struct _snd_mixer_selem snd_mixer_selem_t;
typedef struct _snd_mixer_selem_id snd_mixer_selem_id_t;

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_mixer_channel_id *snd_mixer_channel_id_t;
#else
typedef enum _snd_mixer_channel_id snd_mixer_channel_id_t;
#endif

#define SND_MIXER_CHN_UNKNOWN ((snd_mixer_channel_id_t) SND_MIXER_CHN_UNKNOWN)
#define SND_MIXER_CHN_FRONT_LEFT ((snd_mixer_channel_id_t) SND_MIXER_CHN_FRONT_LEFT)
#define SND_MIXER_CHN_FRONT_RIGHT ((snd_mixer_channel_id_t) SND_MIXER_CHN_FRONT_RIGHT)
#define SND_MIXER_CHN_FRONT_CENTER ((snd_mixer_channel_id_t) SND_MIXER_CHN_FRONT_CENTER)
#define SND_MIXER_CHN_REAR_LEFT ((snd_mixer_channel_id_t) SND_MIXER_CHN_REAR_LEFT)
#define SND_MIXER_CHN_REAR_RIGHT ((snd_mixer_channel_id_t) SND_MIXER_CHN_REAR_RIGHT)
#define SND_MIXER_CHN_WOOFER ((snd_mixer_channel_id_t) SND_MIXER_CHN_WOOFER)
#define SND_MIXER_CHN_LAST ((snd_mixer_channel_id_t) SND_MIXER_CHN_LAST)
#define SND_MIXER_CHN_MONO ((snd_mixer_channel_id_t) SND_MIXER_CHN_MONO)

#ifdef __cplusplus
extern "C" {
#endif

const char *snd_mixer_channel_name(snd_mixer_channel_id_t channel);
int snd_mixer_simple_build(snd_mixer_t *mixer);
snd_mixer_elem_t *snd_mixer_find_selem(snd_mixer_t *mixer,
				       const snd_mixer_selem_id_t *id);
void snd_mixer_selem_get_id(snd_mixer_elem_t *element,
			    snd_mixer_selem_id_t *id);
int snd_mixer_selem_read(snd_mixer_elem_t *element,
			 snd_mixer_selem_t *simple);
int snd_mixer_selem_write(snd_mixer_elem_t *element,
			  const snd_mixer_selem_t *simple);

int snd_mixer_selem_is_mono(const snd_mixer_selem_t *obj);
int snd_mixer_selem_has_channel(const snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel);
long snd_mixer_selem_get_volume(const snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel);
void snd_mixer_selem_set_volume(snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel, long value);
int snd_mixer_selem_get_mute(const snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel);
int snd_mixer_selem_get_capture(const snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel);
void snd_mixer_selem_set_mute(snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel, int mute);
void snd_mixer_selem_set_capture(snd_mixer_selem_t *obj, snd_mixer_channel_id_t channel, int capture);
void snd_mixer_selem_set_mute_all(snd_mixer_selem_t *obj, int mute);
void snd_mixer_selem_set_capture_all(snd_mixer_selem_t *obj, int capture);
void snd_mixer_selem_set_volume_all(snd_mixer_selem_t *obj, long value);

#ifdef __cplusplus
}
#endif

