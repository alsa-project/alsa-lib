/*
 *  Control Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@jcu.cz>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "soundlib.h"

#define SND_FILE_CONTROL	"/dev/sndcontrol%i"
#define SND_CTL_VERSION_MAX	SND_PROTOCOL_VERSION( 1, 0, 0 )
 
typedef struct {
  int card;
  int fd;
} snd_ctl_t;
 
int snd_ctl_open( void **handle, int card )
{
  int fd, ver;
  char filename[32];
  snd_ctl_t *ctl;

  *handle = NULL;
  if ( card < 0 || card >= SND_CARDS ) return -EINVAL;
  sprintf( filename, SND_FILE_CONTROL, card );
  if ( (fd = open( filename, O_RDWR )) < 0 ) return -errno;
  if ( ioctl( fd, SND_CTL_IOCTL_PVERSION, &ver ) < 0 ) {
    close( fd );
    return -errno;
  }
  if ( ver > SND_CTL_VERSION_MAX ) return -SND_ERROR_UNCOMPATIBLE_VERSION;
  ctl = (snd_ctl_t *)calloc( 1, sizeof( snd_ctl_t ) );
  if ( ctl == NULL ) {
    close( fd );
    return -ENOMEM;
  }
  ctl -> card = card;
  ctl -> fd = fd;
  *handle = ctl;
  return 0;
}

int snd_ctl_close( void *handle )
{
  snd_ctl_t *ctl;
  int res;
  
  ctl = (snd_ctl_t *)handle;
  if ( !ctl ) return -EINVAL;
  res = close( ctl -> fd ) < 0 ? -errno : 0;
  free( ctl );
  return res;
}

int snd_ctl_file_descriptor( void *handle )
{
  snd_ctl_t *ctl;
  
  ctl = (snd_ctl_t *)handle;
  if ( !ctl ) return -EINVAL;
  return ctl -> fd;
}

int snd_ctl_hw_info( void *handle, struct snd_ctl_hw_info *info )
{
  snd_ctl_t *ctl;
  
  ctl = (snd_ctl_t *)handle;
  if ( !ctl ) return -EINVAL;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_HW_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_ctl_pcm_info( void *handle, int dev, snd_pcm_info_t *info )
{
  snd_ctl_t *ctl;
  
  ctl = (snd_ctl_t *)handle;
  if ( !ctl ) return -EINVAL;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_PCM_DEVICE, &dev ) < 0 )
    return -errno;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_PCM_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_ctl_pcm_playback_info( void *handle, int dev, snd_pcm_playback_info_t *info )
{
  snd_ctl_t *ctl;
  
  ctl = (snd_ctl_t *)handle;
  if ( !ctl ) return -EINVAL;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_PCM_DEVICE, &dev ) < 0 )
    return -errno;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_PCM_PLAYBACK_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_ctl_pcm_record_info( void *handle, int dev, snd_pcm_record_info_t *info )
{
  snd_ctl_t *ctl;
  
  ctl = (snd_ctl_t *)handle;
  if ( !ctl ) return -EINVAL;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_PCM_DEVICE, &dev ) < 0 )
    return -errno;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_PCM_RECORD_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_ctl_mixer_info( void *handle, int dev, snd_mixer_info_t *info )
{
  snd_ctl_t *ctl;
  
  ctl = (snd_ctl_t *)handle;
  if ( !ctl ) return -EINVAL;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_MIXER_DEVICE, &dev ) < 0 )
    return -errno;
  if ( ioctl( ctl -> fd, SND_CTL_IOCTL_MIXER_INFO, info ) < 0 )
    return -errno;
  return 0;
}
