
EXTRA_LTLIBRARIES = libpcm.la

libpcm_la_SOURCES = pcm.c pcm_hw.c pcm_plugin.c pcm_linear.c pcm_route.c \
		    pcm_mulaw.c pcm_alaw.c pcm_adpcm.c pcm_rate.c pcm_plug.c \
	            pcm_misc.c pcm_mmap.c pcm_multi.c pcm_shm.c pcm_file.c \
		    pcm_share.c
noinst_HEADERS = pcm_local.h pcm_plugin.h

all: libpcm.la

INCLUDES=-I$(top_srcdir)/include
