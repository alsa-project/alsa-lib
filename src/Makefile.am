SUBDIRS=control mixer pcm rawmidi timer hwdep seq instr
COMPATNUM=@LIBTOOL_VERSION_INFO@

lib_LTLIBRARIES = libasound.la
libasound_la_SOURCES = error.c
libasound_la_LIBADD = control/libcontrol.la mixer/libmixer.la pcm/libpcm.la \
                      rawmidi/librawmidi.la timer/libtimer.la hwdep/libhwdep.la \
                      seq/libseq.la instr/libinstr.la
libasound_la_LDFLAGS = -version-info $(COMPATNUM) -lpthread

control/libcontrol.la:
	$(MAKE) -C control libcontrol.la

mixer/libmixer.la:
	$(MAKE) -C mixer libmixer.la

pcm/libpcm.la:
	$(MAKE) -C pcm libpcm.la

rawmidi/librawmidi.la:
	$(MAKE) -C rawmidi librawmidi.la

timer/libtimer.la:
	$(MAKE) -C timer libtimer.la

hwdep/libhwdep.la:
	$(MAKE) -C hwdep libhwdep.la

seq/libseq.la:
	$(MAKE) -C seq libseq.la

instr/libinstr.la:
	$(MAKE) -C instr libinstr.la

INCLUDES=-I$(top_srcdir)/include
