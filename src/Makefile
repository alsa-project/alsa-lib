#
# Makefile for ALSA library
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@jcu.cz>
#

include ../Makefile.conf

TARGET=../lib/libasound.a
STARGET=../lib/libasound.so
STARGETX=../lib/libasound.so.$(SND_LIB_VERSION)
STARGETO=../lib/libasound.so.$(SND_LIB_MAJOR)
TARGETS=$(TARGET) $(STARGET)

STATIC_LIBS=	control/libcontrol.a \
            	mixer/libmixer.a \
	    	pcm/libpcm.a \
		rawmidi/librawmidi.a
DYNAMIC_LIBS=	control/libcontrol.Sa \
		mixer/libmixer.Sa \
		pcm/libpcm.Sa \
		rawmidi/librawmidi.Sa

OBJECTS=error.o
SOBJECTS=error.So

.SUFFIXES:
.SUFFIXES: .c .s .S .o .So .a .Sa

.c.o:
	$(CC) $(COPTS) $(INCLUDE) -c -o $*.o $<
.c.So:
	$(CC) $(COPTS) $(INCLUDE) -fPIC -c -o $*.So $<

all: $(TARGETS)

$(TARGET): .depend $(OBJECTS) $(STATIC_LIBS)
	rm -f ../lib/libasound.a
	$(LINKER) -r -o $(TARGET) $(STATIC_LIBS) $(OBJECTS)

$(STARGET): .depend $(SOBJECTS) $(DYNAMIC_LIBS)
	rm -f ../lib/libasound*.so*
	$(CC) -shared -Wl,-soname,libasound.so.$(SND_LIB_MAJOR) $(DYNAMIC_LIBS) $(SOBJECTS) -o $(STARGETX)
	ln -s libasound.so.$(SND_LIB_VERSION) $(STARGET)
	ln -s libasound.so.$(SND_LIB_VERSION) $(STARGETO)

control/libcontrol.a:
	$(MAKE) -C control
control/libcontrol.sa:
	$(MAKE) -C control

mixer/libmixer.a:
	$(MAKE) -C mixer
mixer/libmixer.sa:
	$(MAKE) -C mixer

pcm/libpcm.a:
	$(MAKE) -C pcm
pcm/libpcm.sa:
	$(MAKE) -C pcm

rawmidi/librawmidi.a:
	$(MAKE) -C rawmidi
rawmidi/librawmidi.sa:
	$(MAKE) -C rawmidi

clean:
	$(MAKE) -C control clean
	$(MAKE) -C pcm clean
	$(MAKE) -C mixer clean
	$(MAKE) -C rawmidi clean
	rm -f core .depend *.o *.So *.orig *~
	rm -f ../lib/libasound.*

.depend:
	$(CPP) $(COPTS) $(INCLUDE) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
