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
int snd_ctl_pcm_info( void *handle, int dev, snd_pcm_info_t *info );
int snd_ctl_pcm_playback_info( void *handle, int dev, snd_pcm_playback_info_t *info );
int snd_ctl_pcm_record_info( void *handle, int dev, snd_pcm_record_info_t *info );
int snd_ctl_mixer_info( void *handle, int dev, snd_mixer_info_t *info );

#ifdef __cplusplus
}
#endif

