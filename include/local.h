/*
 * Application interface library for the ALSA driver
 * Copyright (c) 1994/98 by Jaroslav Kysela <perex@jcu.cz>
 */

#include <asm/byteorder.h>
#include "../../../include/ultraconf.h"

#ifdef DEBUG

#include <assert.h>
#define ASSERT( s ) assert( s )

void __snd_dprintf( char *, ... );
#define snd_dprintf( args... ) __ultra_dprintf( ##args )

#else

#define ASSERT( s ) { ; }

#define snd_dprintf( fmt... )	/* nothing */

#endif

extern inline int snd_p_rint( float x )
{
  return (int)( x + (float)0.5 );
}

/*
 *
 */

extern inline void snd_put_byte( unsigned char *array, unsigned int idx, unsigned char b )
{
  *(array + idx) = b;
}

extern inline unsigned char snd_get_byte( unsigned char *array, unsigned int idx )
{
  return *(array + idx);
}

#if defined( __i386__ )

extern inline void snd_put_le_word( unsigned char *array, unsigned int idx, unsigned short w )
{
  *(unsigned short *)(array + idx) = w;
}

extern inline unsigned short snd_get_le_word( unsigned char *array, unsigned int idx )
{
  return *(unsigned short *)(array + idx);
}

extern inline void snd_put_le_dword( unsigned char *array, unsigned int idx, unsigned int dw )
{
  *(unsigned int *)(array + idx) = dw;
}

extern inline unsigned int snd_get_le_dword( unsigned char *array, unsigned int idx )
{
  return *(unsigned int *)(array + idx );
}

#else

#ifndef WORDS_BIGENDIAN

extern inline void snd_put_le_word( unsigned char *array, unsigned int idx, unsigned short w )
{
  *(array + idx + 0) = (unsigned char)(w >> 0);
  *(array + idx + 1) = (unsigned char)(w >> 8);
}

extern inline unsigned short snd_get_le_word( unsigned char *array, unsigned int idx )
{
  return ( *(array + idx + 0) << 0 ) |
         ( *(array + idx + 1) << 8 );
}

extern inline void snd_put_le_dword( unsigned char *array, unsigned int idx, unsigned int dw )
{
  *(array + idx + 0) = (unsigned char)(dw >>  0);
  *(array + idx + 1) = (unsigned char)(dw >>  8);
  *(array + idx + 2) = (unsigned char)(dw >> 16);
  *(array + idx + 3) = (unsigned char)(dw >> 24);
}

extern inline unsigned int snd_get_le_dword( unsigned char *array, unsigned int idx )
{
  return ( *(array + idx + 0) <<  0 ) |
         ( *(array + idx + 1) <<  8 ) |
         ( *(array + idx + 2) << 16 ) |
         ( *(array + idx + 3) << 24 );
}

#else

extern inline void snd_put_le_word( unsigned char *array, unsigned int idx, unsigned short w )
{
  *(array + idx + 0) = (unsigned char)(w >> 8);
  *(array + idx + 1) = (unsigned char)(w >> 0);
}

extern inline unsigned short snd_get_le_word( unsigned char *array, unsigned int idx )
{
  return ( *(array + idx + 0) << 8 ) |
         ( *(array + idx + 1) << 0 );
}

extern inline void snd_put_le_dword( unsigned char *array, unsigned int idx, unsigned int dw )
{
  *(array + idx + 0) = (unsigned char)(dw >> 24);
  *(array + idx + 1) = (unsigned char)(dw >> 16);
  *(array + idx + 2) = (unsigned char)(dw >>  8);
  *(array + idx + 3) = (unsigned char)(dw >>  0);
}

extern inline unsigned int snd_get_le_dword( unsigned char *array, unsigned int idx )
{
  return ( *(array + idx + 0) << 24 ) |
         ( *(array + idx + 1) << 16 ) |
         ( *(array + idx + 2) <<  8 ) |
         ( *(array + idx + 3) <<  0 );
}

#endif

#endif

/*
 *
 */

extern inline unsigned char snd_get_unsigned_byte( unsigned char *ptr )
{
  return *ptr;
}

extern inline void snd_put_unsigned_byte( unsigned char *ptr, unsigned char val )
{
  *ptr = val;
}
 
extern inline unsigned char snd_get_signed_byte( signed char *ptr )
{
  return *ptr;
}

extern inline void snd_put_signed_byte( signed char *ptr, signed char val )
{
  *ptr = val;
}
 
#if !defined( WORDS_BIGENDIAN ) || defined( __i386__ )

extern inline signed short snd_get_signed_le_word( unsigned char *ptr )
{
  return *(signed short *)ptr;
}

extern inline void snd_put_signed_le_word( unsigned char *ptr, signed short val )
{
  *(signed short *)ptr = val;
}

extern inline unsigned short snd_get_unsigned_le_word( unsigned char *ptr )
{
  return *(unsigned short *)ptr;
}

extern inline void snd_put_unsigned_le_word( unsigned char *ptr, unsigned short val )
{
  *(unsigned short *)ptr = val;
}

extern inline unsigned int snd_get_unsigned_le_dword( unsigned char *ptr )
{
  return *(unsigned int *)ptr;
}

extern inline void snd_put_unsigned_le_dword( unsigned char *ptr, unsigned int val )
{
  *(unsigned int *)ptr = val;
}

#else

extern inline unsigned short snd_get_signed_le_word( unsigned char *ptr )
{
  return (signed short)( *ptr + ( *( ptr + 1 ) << 8 );
}

extern inline void snd_put_signed_le_word( unsigned char *ptr, signed short val )
{
  *ptr = (unsigned char)val;
  *(ptr + 1) = val >> 8;
}

extern inline unsigned short snd_get_unsigned_le_word( unsigned char *ptr )
{
  return (signed short)( *ptr + ( *( ptr + 1 ) << 8 );
}

extern inline void snd_put_unsigned_le_word( unsigned char *ptr, unsigned short val )
{
  *ptr = (unsigned char)val;
  *(ptr + 1) = val >> 8;
}

extern inline unsigned int snd_get_unsigned_le_dword( unsigned char *ptr )
{
  return (signed int)( *ptr + ( *( ptr + 1 ) << 8 ) + 
                       ( *( ptr + 2 ) << 8 ) + ( *( ptr + 3 ) << 8 ) );
}

extern inline void snd_put_unsigned_le_dword( unsigned char *ptr, unsigned int val )
{
  *ptr = (unsigned char)val;
  *(ptr + 1) = (unsigned char)val >> 8;
  *(ptr + 2) = (unsigned char)val >> 16;
  *(ptr + 3) = (unsigned char)val >> 24;
}

#endif

extern inline unsigned short snd_get_be_word( unsigned char *array, unsigned int idx )
{
  return ( *(array + idx + 1) << 0 ) |
         ( *(array + idx + 0) << 8 );
}

extern inline unsigned int snd_get_be_dword( unsigned char *array, unsigned int idx )
{
  return ( *(array + idx + 3) <<  0 ) |
         ( *(array + idx + 2) <<  8 ) |
         ( *(array + idx + 1) << 16 ) |
         ( *(array + idx + 0) << 24 );
}
