check_PROGRAMS=control mixer switches pause pcm pcmtest latency seq \
	       playmidi1 timer loopback

control_LDADD=../src/libasound.la
mixer_LDADD=../src/libasound.la
switches_LDADD=../src/libasound.la
pause_LDADD=../src/libasound.la
pcm_LDADD=../src/libasound.la
pcmtest_LDADD=../src/libasound.la
latency_LDADD=../src/libasound.la
seq_LDADD=../src/libasound.la
playmidi1_LDADD=../src/libasound.la
timer_LDADD=../src/libasound.la
loopback_LDADD=../src/libasound.la

INCLUDES=-I$(top_srcdir)/include
CFLAGS=-static -Wall -pipe -g

EXTRA_DIST=seq-decoder.c seq-sender.c midifile.h midifile.c midifile.3
