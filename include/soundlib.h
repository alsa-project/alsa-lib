/*
 *  Application interface library for the ALSA driver
 *  Copyright (c) by Jaroslav Kysela <perex@jcu.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __SOUNDLIB_H
#define __SOUNDLIB_H

#include <linux/sound.h>
#include <unistd.h>

/*
 *  version.h
 */

#define SOUNDLIB_VERSION_MAJOR		0
#define SOUNDLIB_VERSION_MINOR		0
#define SOUNDLIB_VERSION_SUBMINOR	9
#define SOUNDLIB_VERSION		( ( LIBULTRA_VERSION_MAJOR << 16 ) | ( LIBULTRA_VERSION_MINOR << 8 ) | LIB_ULTRA_VERSION_SUBMINOR )

/*
 *  error.h
 */

#define SND_ERROR_BEGIN				500000
#define SND_ERROR_UNCOMPATIBLE_VERSION		(SND_ERROR_BEGIN+0)

#ifdef __cplusplus
extern "C" {
#endif

const char *snd_strerror( int errnum );

#ifdef __cplusplus
}
#endif

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

/****************************************************************************
 *                                                                          *
 *                                pcm.h                                     *
 *                        Digital Audio Interface                           *
 *                                                                          *
 ****************************************************************************/
 
#define SND_PCM_OPEN_PLAYBACK	(O_WRONLY)
#define SND_PCM_OPEN_RECORD	(O_RDONLY)
#define SND_PCM_OPEN_DUPLEX	(O_RDWR)
 
#ifdef __cplusplus
extern "C" {
#endif

int snd_pcm_open( void **handle, int card, int device, int mode );
int snd_pcm_close( void *handle );
int snd_pcm_file_descriptor( void *handle );
int snd_pcm_block_mode( void *handle, int enable );
int snd_pcm_info( void *handle, snd_pcm_info_t *info );
int snd_pcm_playback_info( void *handle, snd_pcm_playback_info_t *info );
int snd_pcm_record_info( void *handle, snd_pcm_record_info_t *info );
int snd_pcm_playback_format( void *handle, snd_pcm_format_t *format );
int snd_pcm_record_format( void *handle, snd_pcm_format_t *format );
int snd_pcm_playback_params( void *handle, snd_pcm_playback_params_t *params );
int snd_pcm_record_params( void *handle, snd_pcm_record_params_t *params );
int snd_pcm_playback_status( void *handle, snd_pcm_playback_status_t *status );
int snd_pcm_record_status( void *handle, snd_pcm_record_status_t *status );
int snd_pcm_drain_playback( void *handle );
int snd_pcm_flush_playback( void *handle );
int snd_pcm_flush_record( void *handle );
int snd_pcm_playback_time( void *handle, int enable );
int snd_pcm_record_time( void *handle, int enable );
ssize_t snd_pcm_write( void *handle, const void *buffer, size_t size );
ssize_t snd_pcm_read( void *handle, void *buffer, size_t size );

#ifdef __cplusplus
}
#endif

/*
 *
 */
 
#endif /* __SOUNDLIB_H */
