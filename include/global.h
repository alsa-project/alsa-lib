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

