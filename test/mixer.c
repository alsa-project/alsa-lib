#include <stdio.h>
#include <string.h>
#include "../include/soundlib.h"

static void mixer_test( int card, int device )
{
  int err, channels;
  void *handle;
  snd_mixer_info_t info;
  
  if ( (err = snd_mixer_open( &handle, card, device)) < 0 ) {
    printf( "Mixer open error: %s\n", snd_strerror( err ) );
    return;
  }
  printf( "Mixer %i/%i open ok...\n", card, device );
  channels = snd_mixer_channels( handle );
  if ( channels < 0 ) {
    printf( "Mixer channels error: %s\n", snd_strerror( channels ) );
    return;
  }
  printf( "  Mixer have %i channel%s.\n", channels, channels > 1 ? "s" : "" );
  if ( (err = snd_mixer_info( handle, &info )) < 0 ) {
    printf( "Mixer info error: %s\n", snd_strerror( err ) );
    return;
  }  
  printf( "  Info:\n" );
  printf( "    type - %i\n", info.type );
  printf( "    channels - %i\n", info.channels );
  printf( "    caps - 0x%x\n", info.caps );
  printf( "    id - '%s'\n", info.id );
  printf( "    name - '%s'\n", info.name );
  snd_mixer_close( handle );
}

void main( void )
{
  int idx, idx1, cards, err;
  void *handle;
  struct snd_ctl_hw_info info;
  
  cards = snd_cards();
  printf( "Detected %i soundcard%s...\n", cards, cards > 1 ? "s" : "" );
  if ( cards <= 0 ) {
    printf( "Giving up...\n" );
    return;
  }
  for ( idx = 0; idx < cards; idx++ ) {
    if ( (err = snd_ctl_open( &handle, idx )) < 0 ) {
      printf( "Open error: %s\n", snd_strerror( err ) );
      continue;
    }
    if ( (err = snd_ctl_hw_info( handle, &info )) < 0 ) {
      printf( "HW info error: %s\n", snd_strerror( err ) );
      continue;
    }
    for ( idx1 = 0; idx1 < info.mixerdevs; idx1++ )
      mixer_test( idx, idx1 );
    snd_ctl_close( handle );
  }
}
