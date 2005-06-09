EXTRA_LTLIBRARIES = libcontrol.la

libcontrol_la_SOURCES = cards.c hcontrol.c \
                        control.c control_hw.c control_shm.c \
			control_ext.c setup.c control_symbols.c

noinst_HEADERS = control_local.h

all: libcontrol.la


INCLUDES=-I$(top_srcdir)/include
