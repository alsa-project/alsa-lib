/*
 *  PCM Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@jcu.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "soundlib.h"

#define SND_FILE_PCM		"/dev/sndpcm%i%i"
#define SND_CTL_VERSION_MAX	SND_PROTOCOL_VERSION( 1, 0, 0 )
 
typedef struct {
  int card;
  int device;
  int fd;
} snd_pcm_t;
 
int snd_pcm_open( void **handle, int card, int device, int mode )
{
  int fd, ver;
  char filename[32];
  snd_pcm_t *pcm;

  *handle = NULL;
  if ( card < 0 || card >= SND_CARDS ) return -EINVAL;
  sprintf( filename, SND_FILE_PCM, card, device );
  if ( (fd = open( filename, mode )) < 0 ) return -errno;
  if ( ioctl( fd, SND_PCM_IOCTL_PVERSION, &ver ) < 0 ) {
    close( fd );
    return -errno;
  }
  if ( ver > SND_CTL_VERSION_MAX ) return -SND_ERROR_UNCOMPATIBLE_VERSION;
  pcm = (snd_pcm_t *)calloc( 1, sizeof( snd_pcm_t ) );
  if ( pcm == NULL ) {
    close( fd );
    return -ENOMEM;
  }
  pcm -> card = card;
  pcm -> device = device;
  pcm -> fd = fd;
  *handle = pcm;
  return 0;
}

int snd_pcm_close( void *handle )
{
  snd_pcm_t *pcm;
  int res;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  res = close( pcm -> fd ) < 0 ? -errno : 0;
  free( pcm );
  return res;
}

int snd_pcm_file_descriptor( void *handle )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  return pcm -> fd;
}

int snd_pcm_block_mode( void *handle, int enable )
{
  snd_pcm_t *pcm;
  long flags;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( fcntl( pcm -> fd, F_GETFL, &flags ) < 0 )
    return -errno;
  if ( enable )
    flags |= O_NONBLOCK;
   else
    flags &= ~O_NONBLOCK;
  if ( fcntl( pcm -> fd, F_SETFL, &flags ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_info( void *handle, snd_pcm_info_t *info )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_playback_info( void *handle, snd_pcm_playback_info_t *info )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_PLAYBACK_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_record_info( void *handle, snd_pcm_record_info_t *info )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_RECORD_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_playback_format( void *handle, snd_pcm_format_t *format )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_PLAYBACK_FORMAT, format ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_record_format( void *handle, snd_pcm_format_t *format )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_RECORD_FORMAT, format ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_playback_params( void *handle, snd_pcm_playback_params_t *params )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_PLAYBACK_PARAMS, params ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_record_params( void *handle, snd_pcm_record_params_t *params )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_RECORD_PARAMS, params ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_playback_status( void *handle, snd_pcm_playback_status_t *status )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_PLAYBACK_STATUS, status ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_record_status( void *handle, snd_pcm_record_status_t *status )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_RECORD_STATUS, status ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_drain_playback( void *handle )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_DRAIN_PLAYBACK ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_flush_playback( void *handle )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_FLUSH_PLAYBACK ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_flush_record( void *handle )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_FLUSH_RECORD ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_playback_time( void *handle, int enable )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_PLAYBACK_TIME, &enable ) < 0 )
    return -errno;
  return 0;
}

int snd_pcm_record_time( void *handle, int enable )
{
  snd_pcm_t *pcm;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  if ( ioctl( pcm -> fd, SND_PCM_IOCTL_RECORD_TIME, &enable ) < 0 )
    return -errno;
  return 0;
}

ssize_t snd_pcm_write( void *handle, const void *buffer, size_t size )
{
  snd_pcm_t *pcm;
  ssize_t result;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  result = write( pcm -> fd, buffer, size );
  if ( result < 0 ) return -errno;
  return result;
}

ssize_t snd_pcm_read( void *handle, void *buffer, size_t size )
{
  snd_pcm_t *pcm;
  ssize_t result;
  
  pcm = (snd_pcm_t *)handle;
  if ( !pcm ) return -EINVAL;
  result = read( pcm -> fd, buffer, size );
  if ( result < 0 ) return -errno;
  return result;
}
