/****************************************************************************
 *                                                                          *
 *                               instr.h                                    *
 *                          Instrument Interface                            *
 *                                                                          *
 ****************************************************************************/

typedef void snd_instr_iwffff_t;

#ifdef __cplusplus
extern "C" {
#endif

/* InterWave FFFF support */
int snd_instr_iwffff_open(void **handle, const char *name_fff, const char *name_dta);
int snd_instr_iwffff_open_rom(void **handle, int card, int bank, int file);
int snd_instr_iwffff_open_rom_file(void **handle, const char *name, int bank, int file);
int snd_instr_iwffff_close(void *handle);
int snd_instr_iwffff_load(void *handle, int bank, int prg, snd_instr_iwffff_t **iwffff);
int snd_instr_iwffff_convert_to_stream(snd_instr_iwffff_t *iwffff, const char *name, snd_seq_instr_data_t **data, int *size);
int snd_instr_iwffff_convert_from_stream(snd_seq_instr_data_t *data, int size, snd_instr_iwffff_t **iwffff);
int snd_instr_iwffff_free(snd_instr_iwffff_t *iwffff);

#ifdef __cplusplus
}
#endif

