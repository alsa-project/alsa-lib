if HAVE_JACK
JACK_LIB = -ljack
else
JACK_LIB =
endif

bin_PROGRAMS = aserver
aserver_SOURCES = aserver.c
# aserver_LDADD = -lasound
aserver_LDADD = ../src/libasound.la $(JACK_LIB)

all: aserver

INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/src/pcm

../src/libasound.la:
	$(MAKE) -C ../src libasound.la

