
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _snd_output snd_output_t;

typedef enum _snd_output_type {
	SND_OUTPUT_STDIO,
	SND_OUTPUT_BUFFER,
} snd_output_type_t;

int snd_output_stdio_open(snd_output_t **outputp, const char *file);
int snd_output_stdio_attach(snd_output_t **outputp, FILE *fp, int close);
int snd_output_buffer_open(snd_output_t **outputp);
size_t snd_output_buffer_string(snd_output_t *output, char **buf);
int snd_output_close(snd_output_t *output);
int snd_output_printf(snd_output_t *output, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
int snd_output_puts(snd_output_t *output, const char *str);
int snd_output_putc(snd_output_t *output, int c);
int snd_output_flush(snd_output_t *output);

#ifdef __cplusplus
}
#endif
