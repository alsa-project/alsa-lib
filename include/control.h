/****************************************************************************
 *                                                                          *
 *                              control.h                                   *
 *                          Control Interface                               *
 *                                                                          *
 ****************************************************************************/

typedef struct snd_ctl_callbacks {
	void *private_data;	/* should be used by an application */
	void (*rebuild) (void *private_data);
	void (*xswitch) (void *private_data, int cmd, int iface, snd_switch_list_item_t *item);
	void *reserved[29];	/* reserved for the future use - must be NULL!!! */
} snd_ctl_callbacks_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_ctl snd_ctl_t;

int snd_card_load(int card);
int snd_cards(void);
unsigned int snd_cards_mask(void);
int snd_card_name(const char *name);

int snd_defaults_card(void);
int snd_defaults_mixer_card(void);
int snd_defaults_mixer_device(void);
int snd_defaults_pcm_card(void);
int snd_defaults_pcm_device(void);
int snd_defaults_rawmidi_card(void);
int snd_defaults_rawmidi_device(void);

int snd_ctl_open(snd_ctl_t **handle, int card);
int snd_ctl_close(snd_ctl_t *handle);
int snd_ctl_file_descriptor(snd_ctl_t *handle);
int snd_ctl_hw_info(snd_ctl_t *handle, struct snd_ctl_hw_info *info);
int snd_ctl_switch_list(snd_ctl_t *handle, snd_switch_list_t * list);
int snd_ctl_switch_read(snd_ctl_t *handle, snd_switch_t * sw);
int snd_ctl_switch_write(snd_ctl_t *handle, snd_switch_t * sw);
int snd_ctl_hwdep_info(snd_ctl_t *handle, int dev, snd_hwdep_info_t * info);
int snd_ctl_pcm_info(snd_ctl_t *handle, int dev, snd_pcm_info_t * info);
int snd_ctl_pcm_playback_info(snd_ctl_t *handle, int dev, snd_pcm_playback_info_t * info);
int snd_ctl_pcm_capture_info(snd_ctl_t *handle, int dev, snd_pcm_capture_info_t * info);
int snd_ctl_pcm_playback_switch_list(snd_ctl_t *handle, int dev, snd_switch_list_t * list);
int snd_ctl_pcm_playback_switch_read(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_pcm_playback_switch_write(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_pcm_capture_switch_list(snd_ctl_t *handle, int dev, snd_switch_list_t * list);
int snd_ctl_pcm_capture_switch_read(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_pcm_capture_switch_write(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_mixer_info(snd_ctl_t *handle, int dev, snd_mixer_info_t * info);
int snd_ctl_mixer_switch_list(snd_ctl_t *handle, int dev, snd_switch_list_t *list);
int snd_ctl_mixer_switch_read(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_mixer_switch_write(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_rawmidi_info(snd_ctl_t *handle, int dev, snd_rawmidi_info_t * info);
int snd_ctl_rawmidi_output_info(snd_ctl_t *handle, int dev, snd_rawmidi_output_info_t * info);
int snd_ctl_rawmidi_input_info(snd_ctl_t *handle, int dev, snd_rawmidi_input_info_t * info);
int snd_ctl_rawmidi_output_switch_list(snd_ctl_t *handle, int dev, snd_switch_list_t *list);
int snd_ctl_rawmidi_output_switch_read(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_rawmidi_output_switch_write(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_rawmidi_input_switch_list(snd_ctl_t *handle, int dev, snd_switch_list_t *list);
int snd_ctl_rawmidi_input_switch_read(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_rawmidi_input_switch_write(snd_ctl_t *handle, int dev, snd_switch_t * sw);
int snd_ctl_read(snd_ctl_t *handle, snd_ctl_callbacks_t * callbacks);

#ifdef __cplusplus
}
#endif

