
EXTRA_LTLIBRARIES = libpcm.la

libpcm_la_SOURCES = atomic.c mask.c interval.c \
		    pcm.c pcm_params.c \
		    pcm_hw.c pcm_plugin.c pcm_copy.c pcm_linear.c \
		    pcm_route.c pcm_mulaw.c pcm_alaw.c pcm_adpcm.c \
		    pcm_rate.c pcm_plug.c pcm_misc.c pcm_mmap.c pcm_multi.c \
	            pcm_shm.c pcm_file.c pcm_null.c pcm_share.c \
		    pcm_meter.c pcm_hooks.c pcm_lfloat.c pcm_ladspa.c pcm_symbols.c
noinst_HEADERS = pcm_local.h pcm_plugin.h mask.h mask_inline.h \
	         interval.h interval_inline.h plugin_ops.h ladspa.h

alsadir = $(datadir)/alsa

all: libpcm.la

INCLUDES=-I$(top_srcdir)/include
