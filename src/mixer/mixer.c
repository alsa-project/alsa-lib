/*
 *  Mixer Interface - main file
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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "soundlib.h"

#define SND_FILE_MIXER		"/dev/sndmixer%i%i"
#define SND_CTL_VERSION_MAX	SND_PROTOCOL_VERSION( 1, 0, 0 )
 
typedef struct {
  int card;
  int device;
  int fd;
} snd_mixer_t;
 
int snd_mixer_open( void **handle, int card, int device )
{
  int fd, ver;
  char filename[32];
  snd_mixer_t *mixer;

  *handle = NULL;
  if ( card < 0 || card >= SND_CARDS ) return -EINVAL;
  sprintf( filename, SND_FILE_MIXER, card, device );
  if ( (fd = open( filename, O_RDWR )) < 0 ) return -errno;
  if ( ioctl( fd, SND_MIXER_IOCTL_PVERSION, &ver ) < 0 ) {
    close( fd );
    return -errno;
  }
  if ( ver > SND_CTL_VERSION_MAX ) return -SND_ERROR_UNCOMPATIBLE_VERSION;
  mixer = (snd_mixer_t *)calloc( 1, sizeof( snd_mixer_t ) );
  if ( mixer == NULL ) {
    close( fd );
    return -ENOMEM;
  }
  mixer -> card = card;
  mixer -> device = device;
  mixer -> fd = fd;
  *handle = mixer;
  return 0;
}

int snd_mixer_close( void *handle )
{
  snd_mixer_t *mixer;
  int res;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  res = close( mixer -> fd ) < 0 ? -errno : 0;
  free( mixer );
  return res;
}

int snd_mixer_file_descriptor( void *handle )
{
  snd_mixer_t *mixer;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  return mixer -> fd;
}

int snd_mixer_channels( void *handle )
{
  snd_mixer_t *mixer;
  int result;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  if ( ioctl( mixer -> fd, SND_MIXER_IOCTL_CHANNELS, &result ) < 0 )
    return -errno;
  return result;
}

int snd_mixer_info( void *handle, snd_mixer_info_t *info )
{
  snd_mixer_t *mixer;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  if ( ioctl( mixer -> fd, SND_MIXER_IOCTL_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_mixer_exact_mode( void *handle, int enable )
{
  snd_mixer_t *mixer;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  if ( ioctl( mixer -> fd, SND_MIXER_IOCTL_EXACT, &enable ) < 0 )
    return -errno;
  return 0;  
}

int snd_mixer_channel( void *handle, const char *channel_id )
{
  snd_mixer_t *mixer;
  snd_mixer_channel_info_t info;
  int idx, channels, err;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  /* bellow implementation isn't optimized for speed */
  /* info about channels should be cached in the snd_mixer_t structure */
  if ( (channels = snd_mixer_channels( handle )) < 0 )
    return channels;
  for ( idx = 0; idx < channels; idx++ ) {
    if ( (err = snd_mixer_channel_info( handle, idx, &info )) < 0 )
      return err;
    if ( !strncmp( channel_id, info.name, sizeof( info.name ) ) )
      return idx;
  }
  return -EINVAL;
}

int snd_mixer_channel_info( void *handle, int channel, snd_mixer_channel_info_t *info )
{
  snd_mixer_t *mixer;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  info -> channel = channel;
  if ( ioctl( mixer -> fd, SND_MIXER_IOCTL_CHANNEL_INFO, info ) < 0 )
    return -errno;
  return 0;
}

int snd_mixer_channel_read( void *handle, int channel, snd_mixer_channel_t *data )
{
  snd_mixer_t *mixer;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  data -> channel = channel;
  if ( ioctl( mixer -> fd, SND_MIXER_IOCTL_CHANNEL_READ, data ) < 0 )
    return -errno;
  return 0;
}

int snd_mixer_channel_write( void *handle, int channel, snd_mixer_channel_t *data )
{
  snd_mixer_t *mixer;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  data -> channel = channel;
  if ( ioctl( mixer -> fd, SND_MIXER_IOCTL_CHANNEL_WRITE, data ) < 0 )
    return -errno;
  return 0;
}

int snd_mixer_special_read( void *handle, snd_mixer_special_t *special )
{
  snd_mixer_t *mixer;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  if ( ioctl( mixer -> fd, SND_MIXER_IOCTL_SPECIAL_READ, special ) < 0 )
    return -errno;
  return 0;
}

int snd_mixer_special_write( void *handle, snd_mixer_special_t *special )
{
  snd_mixer_t *mixer;
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  if ( ioctl( mixer -> fd, SND_MIXER_IOCTL_SPECIAL_WRITE, special ) < 0 )
    return -errno;
  return 0;
}

int snd_mixer_read( void *handle, snd_mixer_callbacks_t *callbacks )
{
  snd_mixer_t *mixer;
  int idx, result, count;
  unsigned int cmd, tmp;
  unsigned char buffer[ 64 ];
  
  mixer = (snd_mixer_t *)handle;
  if ( !mixer ) return -EINVAL;
  count = 0;
  while ( (result = read( mixer -> fd, &buffer, sizeof( buffer ))) > 0 ) {
    if ( result & 7 ) return -EIO;
    if ( !callbacks ) continue;
    for ( idx = 0; idx < result; idx += 8 ) {
      cmd = *(unsigned int *)&buffer[ idx ];
      tmp = *(unsigned int *)&buffer[ idx + 4 ];
      if ( cmd == 0 && callbacks -> channel_was_changed ) {        
        callbacks -> channel_was_changed( callbacks -> private_data, (int)tmp );
      }
    }
    count += result >> 3;	/* return only number of changes */
  }
  return result >= 0 ? count : -errno;
}
