check_PROGRAMS=control pcm latency seq \
	       playmidi1 timer rawmidi midiloop

control_LDADD=../src/libasound.la
pcm_LDADD=../src/libasound.la
latency_LDADD=../src/libasound.la
seq_LDADD=../src/libasound.la
playmidi1_LDADD=../src/libasound.la
timer_LDADD=../src/libasound.la
rawmidi_LDADD=../src/libasound.la
midiloop_LDADD=../src/libasound.la

INCLUDES=-I$(top_srcdir)/include
CFLAGS=-static -Wall -pipe -g

EXTRA_DIST=seq-decoder.c seq-sender.c midifile.h midifile.c midifile.3
