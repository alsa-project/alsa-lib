/****************************************************************************
 *                                                                          *
 *                              rawmidi.h                                   *
 *                          RawMIDI interface                               *
 *                                                                          *
 ****************************************************************************/

#define SND_RAWMIDI_OPEN_OUTPUT		(O_WRONLY)
#define SND_RAWMIDI_OPEN_INPUT		(O_RDONLY)
#define SND_RAWMIDI_OPEN_DUPLEX		(O_RDWR)

#ifdef __cplusplus
extern "C" {
#endif

int snd_rawmidi_open(void **handle, int card, int device, int mode);
int snd_rawmidi_close(void *handle);
int snd_rawmidi_file_descriptor(void *handle);
int snd_rawmidi_block_mode(void *handle, int enable);
int snd_rawmidi_info(void *handle, snd_rawmidi_info_t * info);
int snd_rawmidi_output_switch_list(void *handle, snd_switch_list_t * list);
int snd_rawmidi_output_switch_read(void *handle, snd_switch_t * sw);
int snd_rawmidi_output_switch_write(void *handle, snd_switch_t * sw);
int snd_rawmidi_input_switch_list(void *handle, snd_switch_list_t * list);
int snd_rawmidi_input_switch_read(void *handle, snd_switch_t * sw);
int snd_rawmidi_input_switch_write(void *handle, snd_switch_t * sw);
int snd_rawmidi_output_params(void *handle, snd_rawmidi_output_params_t * params);
int snd_rawmidi_input_params(void *handle, snd_rawmidi_input_params_t * params);
int snd_rawmidi_output_status(void *handle, snd_rawmidi_output_status_t * status);
int snd_rawmidi_input_status(void *handle, snd_rawmidi_input_status_t * status);
int snd_rawmidi_drain_output(void *handle);
int snd_rawmidi_flush_output(void *handle);
int snd_rawmidi_flush_input(void *handle);
ssize_t snd_rawmidi_write(void *handle, const void *buffer, size_t size);
ssize_t snd_rawmidi_read(void *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

