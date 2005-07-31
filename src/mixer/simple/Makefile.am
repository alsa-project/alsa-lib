pkglibdir = $(libdir)/@PACKAGE@/smixer

AM_CFLAGS = -g -O2 -W -Wall

pkglib_LTLIBRARIES = smixer-sbase.la \
		     smixer-ac97.la \
		     smixer-hda.la

smixer_sbase_la_SOURCES = sbase.c
smixer_sbase_la_LDFLAGS = -module

smixer_ac97_la_SOURCES = ac97.c sbasedl.c
smixer_ac97_la_LDFLAGS = -module

smixer_hda_la_SOURCES = hda.c sbasedl.c
smixer_hda_la_LDFLAGS = -module
