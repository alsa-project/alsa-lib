/*
 *  Global defines
 */

#ifdef SNDRV_LITTLE_ENDIAN
#define SND_LITTLE_ENDIAN SNDRV_LITTLE_ENDIAN
#endif

#ifdef SNDRV_BIG_ENDIAN
#define SND_BIG_ENDIAN SNDRV_BIG_ENDIAN
#endif

//#define SND_ENUM_TYPECHECK

#ifdef SND_ENUM_TYPECHECK
#define snd_enum_to_int(v) ((unsigned int)(unsigned long)(v))
#define snd_int_to_enum(v) ((void *)(unsigned long)(v))
#define snd_enum_incr(v) (++(unsigned long)(v))
#else
#define snd_enum_to_int(v) (v)
#define snd_int_to_enum(v) (v)
#define snd_enum_incr(v) (++(v))
#endif

enum _snd_set_mode {
	SND_CHANGE,
	SND_TRY,
	SND_TEST,
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_set_mode *snd_set_mode_t;
#else
typedef enum _snd_set_mode snd_set_mode_t;
#endif

#define SND_CHANGE ((snd_set_mode_t) SND_CHANGE)
#define SND_TRY ((snd_set_mode_t) SND_TRY)
#define SND_TEST ((snd_set_mode_t) SND_TEST)

