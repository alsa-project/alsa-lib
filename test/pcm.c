#include <stdio.h>
#include <string.h>
#include "../include/asoundlib.h"

#define BUFFER_SIZE 800000

static char *xitoa( int aaa )
{
  static char str[12];
  
  sprintf( str, "%i", aaa );
  return str;
}

void setformat( void *phandle, void *rhandle )
{
  int err;
  snd_pcm_format_t format;
  
  format.format = SND_PCM_SFMT_S16_LE;
  format.channels = 2;
  format.rate = 22050;
  if ( (err = snd_pcm_playback_format( phandle, &format )) < 0 ) {
    printf( "Playback format error: %s\n", snd_strerror( err ) );
  }
  if ( (err = snd_pcm_record_format( rhandle, &format )) < 0 ) {
    printf( "Record format error: %s\n", snd_strerror( err ) );
  }
}

void method1( void )
{
  void *phandle, *rhandle;
  char buffer[BUFFER_SIZE];
  int err;
  
  if ( (err = snd_pcm_open( &phandle, 0, 0, SND_PCM_OPEN_PLAYBACK )) < 0 ) {
    printf( "Playback open error: %s\n", snd_strerror( err ) );
    return;
  }
  if ( (err = snd_pcm_open( &rhandle, 0, 0, SND_PCM_OPEN_RECORD )) < 0 ) {
    printf( "Record open error: %s\n", snd_strerror( err ) );
    return;
  }
  setformat( phandle, rhandle );
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
  char buffer[BUFFER_SIZE];
  int err;
  
  if ( (err = snd_pcm_open( &phandle, 0, 0, SND_PCM_OPEN_PLAYBACK )) < 0 ) {
    printf( "Playback open error: %s\n", snd_strerror( err ) );
    return;
  }
  if ( (err = snd_pcm_open( &rhandle, 0, 0, SND_PCM_OPEN_RECORD )) < 0 ) {
    printf( "Record open error: %s\n", snd_strerror( err ) );
    return;
  }
  setformat( phandle, rhandle );
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

void method3( void )
{
  void *handle;
  char buffer[BUFFER_SIZE];
  int err;
  
  if ( (err = snd_pcm_open( &handle, 0, 0, SND_PCM_OPEN_DUPLEX )) < 0 ) {
    printf( "Duplex open error: %s\n", snd_strerror( err ) );
    return;
  }
  setformat( handle, handle );
  printf( "Recording... " ); fflush( stdout );
  if ( (err = snd_pcm_read( handle, buffer, sizeof( buffer ) )) != sizeof( buffer ) ) {
    printf( "Read error: %s\n", err < 0 ? snd_strerror( err ) : xitoa( err ) );
    return;
  }
  printf( "done...\n" );
  if ( (err = snd_pcm_flush_record( handle )) < 0 ) {
    printf( "Record flush error: %s\n", snd_strerror( err ) );
    return;
  }
  printf( "Record flush done...\n" );
  printf( "Playback... " ); fflush( stdout );
  if ( (err = snd_pcm_write( handle, buffer, sizeof( buffer ) )) != sizeof( buffer ) ) {
    printf( "Write error: %s\n", err < 0 ? snd_strerror( err ) : xitoa( err ) );
    return;
  }
  printf( "done...\n" );
  if ( (err = snd_pcm_flush_playback( handle )) < 0 ) {
    printf( "Playback flush error: %s\n", snd_strerror( err ) );
    return;
  }
  printf( "Playback flush done...\n" );
  snd_pcm_close( handle );
  printf( "Close...\n" );
}


void main( void )
{
  printf( ">>>>> METHOD 1\n" );
  method1();
  printf( ">>>>> METHOD 2\n" );
  method2();
  printf( ">>>>> METHOD 3\n" );
  method3();
}
