pkglibdir = $(libdir)/@PACKAGE@/smixer

AM_CFLAGS = -g -O2 -W -Wall

INCLUDES=-I$(top_srcdir)/include

pkglib_LTLIBRARIES = smixer-sbase.la \
		     smixer-ac97.la \
		     smixer-hda.la

noinst_HEADERS = sbase.h

smixer_sbase_la_SOURCES = sbase.c
smixer_sbase_la_LDFLAGS = -module -avoid-version
smixer_sbase_la_LIBADD = ../../../src/libasound.la

smixer_ac97_la_SOURCES = ac97.c sbasedl.c
smixer_ac97_la_LDFLAGS = -module -avoid-version
smixer_ac97_la_LIBADD = ../../../src/libasound.la

smixer_hda_la_SOURCES = hda.c sbasedl.c
smixer_hda_la_LDFLAGS = -module -avoid-version
smixer_hda_la_LIBADD = ../../../src/libasound.la
