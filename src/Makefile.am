SUBDIRS=control mixer pcm rawmidi timer seq instr

lib_LTLIBRARIES = libasound.la
libasound_la_SOURCES = error.c 
libasound_la_LIBADD = control/libcontrol.la mixer/libmixer.la pcm/libpcm.la \
                      rawmidi/librawmidi.la timer/libtimer.la seq/libseq.la \
		      instr/libinstr.la
libasound_la_LDFLAGS = -version-info 3:0:3

control/libcontrol.la:
	$(MAKE) -C control libcontrol.la

mixer/libmixer.la:
	$(MAKE) -C mixer libmixer.la

pcm/libpcm.la:
	$(MAKE) -C pcm libpcm.la

rawmidi/librawmidi.la:
	$(MAKE) -C rawmidi librawmidi.la



INCLUDES=-I$(top_srcdir)/include
