SUBDIRS=control mixer pcm rawmidi timer hwdep seq instr compat conf
COMPATNUM=@LIBTOOL_VERSION_INFO@

lib_LTLIBRARIES = libasound.la
libasound_la_SOURCES = conf.c confmisc.c input.c output.c async.c error.c dlmisc.c
libasound_la_LIBADD = control/libcontrol.la mixer/libmixer.la pcm/libpcm.la \
                      rawmidi/librawmidi.la timer/libtimer.la \
		      hwdep/libhwdep.la seq/libseq.la instr/libinstr.la \
		      compat/libcompat.la -lm -ldl -lpthread

libasound_la_LDFLAGS = -version-info $(COMPATNUM)

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

compat/libcompat.la:
	$(MAKE) -C compat libcompat.la

INCLUDES=-I$(top_srcdir)/include
