
/** Input handle */
typedef struct _snd_input snd_input_t;

/** Input type */
typedef enum _snd_input_type {
	/** Input from a stdio stream */
	SND_INPUT_STDIO,
	/** Input from a memory buffer */
	SND_INPUT_BUFFER,
} snd_input_type_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_input_stdio_open(snd_input_t **inputp, const char *file, const char *mode);
int snd_input_stdio_attach(snd_input_t **inputp, FILE *fp, int _close);
int snd_input_buffer_open(snd_input_t **inputp, const char *buffer, int size);
int snd_input_close(snd_input_t *input);
int snd_input_scanf(snd_input_t *input, const char *format, ...) __attribute__ ((format (scanf, 2, 3)));
char *snd_input_gets(snd_input_t *input, char *str, size_t size);
int snd_input_getc(snd_input_t *input);
int snd_input_ungetc(snd_input_t *input, int c);

#ifdef __cplusplus
}
#endif
