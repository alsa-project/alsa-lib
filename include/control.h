/****************************************************************************
 *                                                                          *
 *                              control.h                                   *
 *                          Control Interface                               *
 *                                                                          *
 ****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

int snd_cards( void );
unsigned int snd_cards_mask( void );
int snd_card_name( const char *name );
 
int snd_ctl_open( void **handle, int card );
int snd_ctl_close( void *handle );
int snd_ctl_file_descriptor( void *handle );
int snd_ctl_hw_info( void *handle, struct snd_ctl_hw_info *info );
int snd_ctl_switches( void *handle );
int snd_ctl_switch( void *handle, const char *switch_id );
int snd_ctl_switch_read( void *handle, int switchn, snd_ctl_switch_t *data );
int snd_ctl_switch_write( void *handle, int switchn, snd_ctl_switch_t *data );
int snd_ctl_pcm_info( void *handle, int dev, snd_pcm_info_t *info );
int snd_ctl_pcm_playback_info( void *handle, int dev, snd_pcm_playback_info_t *info );
int snd_ctl_pcm_record_info( void *handle, int dev, snd_pcm_record_info_t *info );
int snd_ctl_pcm_playback_switches( void *handle, int dev );
int snd_ctl_pcm_playback_switch( void *handle, int dev, const char *switch_id );
int snd_ctl_pcm_playback_switch_read( void *handle, int dev, int switchn, snd_pcm_switch_t *data );
int snd_ctl_pcm_playback_switch_write( void *handle, int dev, int switchn, snd_pcm_switch_t *data );
int snd_ctl_pcm_record_switches( void *handle, int dev );
int snd_ctl_pcm_record_switch( void *handle, int dev, const char *switch_id );
int snd_ctl_pcm_record_switch_read( void *handle, int dev, int switchn, snd_pcm_switch_t *data );
int snd_ctl_pcm_record_switch_write( void *handle, int dev, int switchn, snd_pcm_switch_t *data );
int snd_ctl_mixer_info( void *handle, int dev, snd_mixer_info_t *info );
int snd_ctl_mixer_switches( void *handle, int dev );
int snd_ctl_mixer_switch( void *handle, int dev, const char *switch_id );
int snd_ctl_mixer_switch_read( void *handle, int dev, int switchn, snd_mixer_switch_t *data );
int snd_ctl_mixer_switch_write( void *handle, int dev, int switchn, snd_mixer_switch_t *data );
int snd_ctl_rawmidi_info( void *handle, int dev, snd_rawmidi_info_t *info );
int snd_ctl_rawmidi_output_info( void *handle, int dev, snd_rawmidi_output_info_t *info );
int snd_ctl_rawmidi_input_info( void *handle, int dev, snd_rawmidi_input_info_t *info );
int snd_ctl_rawmidi_output_switches( void *handle, int dev );
int snd_ctl_rawmidi_output_switch( void *handle, int dev, const char *switch_id );
int snd_ctl_rawmidi_output_switch_read( void *handle, int dev, int switchn, snd_rawmidi_switch_t *data );
int snd_ctl_rawmidi_output_switch_write( void *handle, int dev, int switchn, snd_rawmidi_switch_t *data );
int snd_ctl_rawmidi_input_switches( void *handle, int dev );
int snd_ctl_rawmidi_input_switch( void *handle, int dev, const char *switch_id );
int snd_ctl_rawmidi_input_switch_read( void *handle, int dev, int switchn, snd_rawmidi_switch_t *data );
int snd_ctl_rawmidi_input_switch_write( void *handle, int dev, int switchn, snd_rawmidi_switch_t *data );

#ifdef __cplusplus
}
#endif

