/****************************************************************************
 *                                                                          *
 *                               mixer.h                                    *
 *                           Mixer Interface                                *
 *                                                                          *
 ****************************************************************************/

typedef struct snd_mixer_callbacks {
  void *private_data;		/* should be used by application */
  void (*channel_was_changed)( void *private_data, int channel );
  void *reserved[15];		/* reserved for future use - must be NULL!!! */
} snd_mixer_callbacks_t;
 
#ifdef __cplusplus
extern "C" {
#endif

int snd_mixer_open( void **handle, int card, int device );
int snd_mixer_close( void *handle );
int snd_mixer_file_descriptor( void *handle );
int snd_mixer_channels( void *handle );
int snd_mixer_info( void *handle, snd_mixer_info_t *info );
int snd_mixer_exact_mode( void *handle, int enable );
int snd_mixer_channel( void *handle, const char *channel_id );
int snd_mixer_channel_info( void *handle, int channel, snd_mixer_channel_info_t *info );
int snd_mixer_channel_read( void *handle, int channel, snd_mixer_channel_t *data );
int snd_mixer_channel_write( void *handle, int channel, snd_mixer_channel_t *data );
int snd_mixer_special_read( void *handle, snd_mixer_special_t *special );
int snd_mixer_special_write( void *handle, snd_mixer_special_t *special );
int snd_mixer_read( void *handle, snd_mixer_callbacks_t *callbacks );

#ifdef __cplusplus
}
#endif

