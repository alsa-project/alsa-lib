/*
 *  error.h
 */

#define SND_ERROR_BEGIN				500000
#define SND_ERROR_INCOMPATIBLE_VERSION		(SND_ERROR_BEGIN+0)

#ifdef __cplusplus
extern "C" {
#endif

const char *snd_strerror( int errnum );

#ifdef __cplusplus
}
#endif

