EXTRA_DIST = Versions.in.in
COMPATNUM=@LIBTOOL_VERSION_INFO@

if VERSIONED_SYMBOLS
VSYMS = -Wl,--version-script=Versions
BUILT_SOURCES = $(top_builddir)/src/Versions
else
VSYMS =
BUILT_SOURCES =
endif

if SYMBOLIC_FUNCTIONS
SYMFUNCS = -Wl,-Bsymbolic-functions
else
SYMFUNCS =
endif

VERSION_CPPFLAGS =

lib_LTLIBRARIES = libasound.la
libasound_la_SOURCES = conf.c confeval.c confmisc.c input.c output.c async.c error.c dlmisc.c socket.c shmarea.c userfile.c names.c

SUBDIRS=control
libasound_la_LIBADD = control/libcontrol.la
if BUILD_MIXER
SUBDIRS += mixer
libasound_la_LIBADD += mixer/libmixer.la
endif
if BUILD_PCM
if VERSIONED_SYMBOLS
VERSION_CPPFLAGS += -DHAVE_PCM_SYMS -DHAVE_TIMER_SYMS
endif
SUBDIRS += pcm timer
libasound_la_LIBADD += pcm/libpcm.la timer/libtimer.la
endif
if BUILD_RAWMIDI
SUBDIRS += rawmidi
libasound_la_LIBADD += rawmidi/librawmidi.la
VERSION_CPPFLAGS += -DHAVE_RAWMIDI_SYMS
endif
if BUILD_HWDEP
SUBDIRS += hwdep
libasound_la_LIBADD += hwdep/libhwdep.la
endif
if BUILD_SEQ
SUBDIRS += seq
libasound_la_LIBADD += seq/libseq.la
VERSION_CPPFLAGS += -DHAVE_SEQ_SYMS
endif
if BUILD_UCM
SUBDIRS += ucm
libasound_la_LIBADD += ucm/libucm.la
endif
SUBDIRS += conf
libasound_la_LIBADD += @ALSA_DEPLIBS@

libasound_la_LDFLAGS = -version-info $(COMPATNUM) $(VSYMS) $(SYMFUNCS) $(LDFLAGS_NOUNDEFINED)

$(top_builddir)/src/Versions: $(top_builddir)/src/Versions.in
	$(COMPILE) -E $(VERSION_CPPFLAGS) -x assembler-with-cpp -o $@ $<

control/libcontrol.la:
	$(MAKE) -C control libcontrol.la

mixer/libmixer.la:
	$(MAKE) -C mixer libmixer.la

ordinary_mixer/libordinarymixer.la:
	$(MAKE) -C ordinary_mixer libordinarymixer.la

pcm/libpcm.la:
	$(MAKE) -C pcm libpcm.la

ordinary_pcm/libordinarypcm.la:
	$(MAKE) -C ordinary_pcm libordinarypcm.la

rawmidi/librawmidi.la:
	$(MAKE) -C rawmidi librawmidi.la

timer/libtimer.la:
	$(MAKE) -C timer libtimer.la

hwdep/libhwdep.la:
	$(MAKE) -C hwdep libhwdep.la

seq/libseq.la:
	$(MAKE) -C seq libseq.la

ucm/libucm.la:
	$(MAKE) -C ucm libucm.la

topology/libtopology.la:
	$(MAKE) -C topology libtopology.la

instr/libinstr.la:
	$(MAKE) -C instr libinstr.la

AM_CPPFLAGS=-I$(top_srcdir)/include
