#
# Makefile for ALSA library
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@jcu.cz>
#

include ../Makefile.conf

TARGETS=soundapi.txt \
        soundapi.html

all: $(TARGETS)

soundapi.txt: soundapi.sgml
	sgml2txt soundapi.sgml

soundapi.html: soundapi.sgml
	sgml2html soundapi.sgml

clean:
	rm -f core .depend *.orig *~
