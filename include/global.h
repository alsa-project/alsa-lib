/*
 *  Global defines
 */

#ifdef SNDRV_LITTLE_ENDIAN
#define SND_LITTLE_ENDIAN SNDRV_LITTLE_ENDIAN
#endif

#ifdef SNDRV_BIG_ENDIAN
#define SND_BIG_ENDIAN SNDRV_BIG_ENDIAN
#endif

#define snd_enum_to_int(v) (v)
#define snd_int_to_enum(v) (v)
#define snd_enum_incr(v) (++(v))
