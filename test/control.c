#include <stdio.h>
#include <string.h>
#include "../include/soundlib.h"

void main( void )
{
  int idx, idx1, cards, err;
  void *handle;
  struct snd_ctl_hw_info info;
  snd_pcm_info_t pcminfo;
  snd_mixer_info_t mixerinfo;
  char str[128];
  
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
    printf( "Soundcard #%i:\n", idx + 1 );
    printf( "  type - %i\n", info.type );
    printf( "  gcaps - 0x%x\n", info.gcaps );
    printf( "  lcaps - 0x%x\n", info.lcaps );
    printf( "  pcm devs - 0x%x\n", info.pcmdevs );
    printf( "  mixer devs - 0x%x\n", info.mixerdevs );
    printf( "  midi devs - 0x%x\n", info.mididevs );
    memset( str, 0, sizeof( str ) );
    strncpy( str, info.id, sizeof( info.id ) );
    printf( "  id - '%s'\n", str );
    printf( "  abbreviation - '%s'\n", info.abbreviation );
    printf( "  name - '%s'\n", info.name );
    printf( "  longname - '%s'\n", info.longname );
    for ( idx1 = 0; idx1 < info.pcmdevs; idx1++ ) {
      printf( "PCM info, device #%i:\n", idx1 );
      if ( (err = snd_ctl_pcm_info( handle, idx1, &pcminfo )) < 0 ) {
        printf( "  PCM info error: %s\n", snd_strerror( err ) );
        continue;
      }
      printf( "  type - %i\n", pcminfo.type );
      printf( "  flags - 0x%x\n", pcminfo.flags );
      printf( "  id - '%s'\n", pcminfo.id );
      printf( "  name - '%s'\n", pcminfo.name );
    }
    for ( idx1 = 0; idx1 < info.mixerdevs; idx1++ ) {
      printf( "MIXER info, device #%i:\n", idx1 );
      if ( (err = snd_ctl_mixer_info( handle, idx1, &mixerinfo )) < 0 ) {
        printf( "  MIXER info error: %s\n", snd_strerror( err ) );
        continue;
      }
      printf( "  type - %i\n", mixerinfo.type );
      printf( "  channels - %i\n", mixerinfo.channels );
      printf( "  caps - 0x%x\n", mixerinfo.caps );
      printf( "  id - '%s'\n", mixerinfo.id );
      printf( "  name - '%s'\n", mixerinfo.name );
    }
    snd_ctl_close( handle );
  }
}
