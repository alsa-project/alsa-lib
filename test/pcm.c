#include <stdio.h>
#include <string.h>
#include "../include/asoundlib.h"

static char *xitoa( int aaa )
{
  static char str[12];
  
  sprintf( str, "%i", aaa );
  return str;
}

void method1( void )
{
  void *phandle, *rhandle;
  char buffer[80000];
  int err;
  
  if ( (err = snd_pcm_open( &phandle, 0, 0, SND_PCM_OPEN_PLAYBACK )) < 0 ) {
    printf( "Playback open error: %s\n", snd_strerror( err ) );
    return;
  }
  if ( (err = snd_pcm_open( &rhandle, 0, 0, SND_PCM_OPEN_RECORD )) < 0 ) {
    printf( "Record open error: %s\n", snd_strerror( err ) );
    return;
  }
  printf( "Recording... " ); fflush( stdout );
  if ( (err = snd_pcm_read( rhandle, buffer, sizeof( buffer ) )) != sizeof( buffer ) ) {
    printf( "Read error: %s\n", err < 0 ? snd_strerror( err ) : xitoa( err ) );
    return;
  }
  printf( "done...\n" );
  printf( "Playback... " ); fflush( stdout );
  if ( (err = snd_pcm_write( phandle, buffer, sizeof( buffer ) )) != sizeof( buffer ) ) {
    printf( "Write error: %s\n", err < 0 ? snd_strerror( err ) : xitoa( err ) );
    return;
  }
  printf( "done...\n" );
  snd_pcm_close( phandle );
  printf( "Playback close...\n" );
  snd_pcm_close( rhandle );
  printf( "Record close...\n" );
}

void method2( void )
{
  void *phandle, *rhandle;
  char buffer[80000];
  int err;
  
  if ( (err = snd_pcm_open( &phandle, 0, 0, SND_PCM_OPEN_PLAYBACK )) < 0 ) {
    printf( "Playback open error: %s\n", snd_strerror( err ) );
    return;
  }
  if ( (err = snd_pcm_open( &rhandle, 0, 0, SND_PCM_OPEN_RECORD )) < 0 ) {
    printf( "Record open error: %s\n", snd_strerror( err ) );
    return;
  }
  printf( "Recording... " ); fflush( stdout );
  if ( (err = snd_pcm_read( rhandle, buffer, sizeof( buffer ) )) != sizeof( buffer ) ) {
    printf( "Read error: %s\n", err < 0 ? snd_strerror( err ) : xitoa( err ) );
    return;
  }
  printf( "done...\n" );
  if ( (err = snd_pcm_flush_record( rhandle )) < 0 ) {
    printf( "Record flush error: %s\n", snd_strerror( err ) );
    return;
  }
  printf( "Record flush done...\n" );
  printf( "Playback... " ); fflush( stdout );
  if ( (err = snd_pcm_write( phandle, buffer, sizeof( buffer ) )) != sizeof( buffer ) ) {
    printf( "Write error: %s\n", err < 0 ? snd_strerror( err ) : xitoa( err ) );
    return;
  }
  printf( "done...\n" );
  if ( (err = snd_pcm_flush_playback( phandle )) < 0 ) {
    printf( "Playback flush error: %s\n", snd_strerror( err ) );
    return;
  }
  printf( "Playback flush done...\n" );
  snd_pcm_close( phandle );
  printf( "Playback close...\n" );
  snd_pcm_close( rhandle );
  printf( "Record close...\n" );
}

void main( void )
{
  printf( ">>>>> METHOD 1\n" );
  method1();
  printf( ">>>>> METHOD 2\n" );
  method2();
}
