COMPATNUM=@LIBTOOL_VERSION_INFO@

if VERSIONED_SYMBOLS
VSYMS = -Wl,--version-script=../Versions
else
VSYMS =
endif

if SYMBOLIC_FUNCTIONS
SYMFUNCS = -Wl,-Bsymbolic-functions
else
SYMFUNCS =
endif

lib_LTLIBRARIES = libatopology.la

libatopology_la_LIBADD = ../libasound.la
libatopology_la_LDFLAGS = -version-info $(COMPATNUM) $(VSYMS) $(SYMFUNCS) $(LDFLAGS_NOUNDEFINED)

libatopology_la_SOURCES =\
	parser.c \
	builder.c \
	ctl.c \
	dapm.c \
	pcm.c \
	data.c \
	text.c \
	channel.c \
	ops.c \
	elem.c \
	save.c \
	decoder.c \
	log.c

noinst_HEADERS = tplg_local.h

AM_CPPFLAGS=-I$(top_srcdir)/include
