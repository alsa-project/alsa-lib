#
# Makefile for ALSA library
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@jcu.cz>
#

include ../../Makefile.conf

TARGET=libcontrol.a
STARGET=libcontrol.Sa
OBJECTS=cards.o control.o
SOBJECTS=cards.So control.So
TARGETS=$(TARGET) $(STARGET)

.SUFFIXES:
.SUFFIXES: .c .s .S .o .So .a .Sa

.c.o:
	$(CC) $(COPTS) $(INCLUDE) -c -o $*.o $<
.c.So:
	$(CC) $(COPTS) $(INCLUDE) -fPIC -c -o $*.So $<


all: $(TARGETS)	

$(TARGET): .depend $(OBJECTS)
	$(LINKER) -r -o $@ $(OBJECTS)

$(STARGET): .depend $(SOBJECTS)
	$(LINKER) -r -o $@ $(SOBJECTS)

clean:
	rm -f core .depend *.o *.So *.a *.Sa *.orig *~

.depend:
	$(CPP) $(COPTS) $(INCLUDE) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
