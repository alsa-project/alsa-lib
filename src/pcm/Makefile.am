SUBDIRS = plugin

EXTRA_LTLIBRARIES = libpcm.la

libpcm_la_SOURCES = pcm.c pcm_plugin.c pcm_plugin_build.c pcm_misc.c \
		    pcm_loopback.c
libpcm_la_LIBADD = plugin/libpcmplugin.la
all: libpcm.la


INCLUDES=-I$(top_srcdir)/include
