SUBDIRS=control mixer pcm rawmidi timer hwdep seq instr

lib_LTLIBRARIES = libasound.la
libasound_la_SOURCES = error.c 
libasound_la_LIBADD = control/libcontrol.la mixer/libmixer.la pcm/libpcm.la \
                      rawmidi/librawmidi.la timer/libtimer.la hwdep/libhwdep.la \
                      seq/libseq.la instr/libinstr.la
libasound_la_LDFLAGS = -release 0.5 -version-info 5:0:5

INCLUDES=-I$(top_srcdir)/include
