/*
 *  Error Routines
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@jcu.cz>
 *
 *  snd_strerror routine needs to be recoded for locale support
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
#include <string.h>
#include "soundlib.h"

static const char *snd_error_codes[] = {
  "Sound protocol isn't compatible"
};

const char *snd_strerror( int errnum )
{
  if ( errnum < 0 ) errnum = -errnum;
  if ( errnum < SND_ERROR_BEGIN ) 
    return (const char *)strerror( errnum );
  errnum -= SND_ERROR_BEGIN;
  if ( errnum >= sizeof( snd_error_codes ) / sizeof( const char * ) )
    return "Unknown error";
  return snd_error_codes[ errnum ];
}
