
typedef struct _snd_input snd_input_t;

enum _snd_input_type {
	SND_INPUT_STDIO,
	SND_INPUT_BUFFER,
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_input_type *snd_input_type_t;
#else
typedef enum _snd_input_type snd_input_type_t;
#endif

#define SND_INPUT_STDIO ((snd_input_type_t) SND_INPUT_STDIO)
#define SND_INPUT_BUFFER ((snd_input_type_t) SND_INPUT_BUFFER)

#ifdef __cplusplus
extern "C" {
#endif

int snd_input_stdio_open(snd_input_t **inputp, const char *file);
int snd_input_stdio_attach(snd_input_t **inputp, FILE *fp, int close);
int snd_input_buffer_open(snd_input_t **inputp, const char *buffer, int size);
int snd_input_close(snd_input_t *input);
int snd_input_scanf(snd_input_t *input, const char *format, ...) __attribute__ ((format (scanf, 2, 3)));
char *snd_input_gets(snd_input_t *input, char *str, size_t size);
int snd_input_getc(snd_input_t *input);

#ifdef __cplusplus
}
#endif
