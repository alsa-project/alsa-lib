EXTRA_LTLIBRARIES=librawmidi.la

librawmidi_la_SOURCES = rawmidi.c rawmidi_hw.c rawmidi_symbols.c \
	ump.c
if BUILD_SEQ
librawmidi_la_SOURCES += rawmidi_virt.c
endif
noinst_HEADERS = rawmidi_local.h ump_local.h

all: librawmidi.la


AM_CPPFLAGS=-I$(top_srcdir)/include
