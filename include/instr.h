/****************************************************************************
 *                                                                          *
 *                               instr.h                                    *
 *                          Instrument Interface                            *
 *                                                                          *
 ****************************************************************************/

/* Simple Wave support */

typedef void snd_instr_simple_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_instr_simple_convert_to_stream(snd_instr_simple_t *simple, const char *name, snd_seq_instr_put_t **put, long *size);
int snd_instr_simple_convert_from_stream(snd_seq_instr_get_t *data, long size, snd_instr_simple_t **simple);
int snd_instr_simple_free(snd_instr_simple_t *simple);

#ifdef __cplusplus
}
#endif

/* InterWave FFFF support */

typedef void snd_instr_iwffff_t;
typedef struct snd_iwffff_handle snd_iwffff_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_instr_iwffff_open(snd_iwffff_handle_t **handle, const char *name_fff, const char *name_dta);
int snd_instr_iwffff_open_rom(snd_iwffff_handle_t **handle, int card, int bank, int file);
int snd_instr_iwffff_open_rom_file(snd_iwffff_handle_t **handle, const char *name, int bank, int file);
int snd_instr_iwffff_close(snd_iwffff_handle_t *handle);
int snd_instr_iwffff_load(snd_iwffff_handle_t *handle, int bank, int prg, snd_instr_iwffff_t **iwffff);
int snd_instr_iwffff_convert_to_stream(snd_instr_iwffff_t *iwffff, const char *name, snd_seq_instr_put_t **data, long *size);
int snd_instr_iwffff_convert_from_stream(snd_seq_instr_get_t *data, long size, snd_instr_iwffff_t **iwffff);
int snd_instr_iwffff_free(snd_instr_iwffff_t *iwffff);

#ifdef __cplusplus
}
#endif

