/*
 *  error.h
 */

#define SND_ERROR_BEGIN				500000
#define SND_ERROR_INCOMPATIBLE_VERSION		(SND_ERROR_BEGIN+0)

#ifdef __cplusplus
extern "C" {
#endif

const char *snd_strerror(int errnum);

typedef void (snd_lib_error_handler_t)(const char *file, int line, const char *function, int err, const char *fmt, ...) /* __attribute__ ((weak, format (printf, 5, 6))) */;
extern snd_lib_error_handler_t *snd_lib_error;
extern int snd_lib_error_set_handler(snd_lib_error_handler_t *handler);

#ifdef __cplusplus
}
#endif

