EXTRA_LTLIBRARIES = libcontrol.la

libcontrol_la_SOURCES = cards.c cards_id.c hcontrol.c defaults.c \
                        control.c control_hw.c control_shm.c \
			setup.c

noinst_HEADERS = control_local.h

all: libcontrol.la


INCLUDES=-I$(top_srcdir)/include

EXTRA_DIST = cards.conf

alsadir = $(datadir)/alsa
alsa_DATA = cards.conf
