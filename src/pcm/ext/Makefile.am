COMPATNUM=@LIBTOOL_VERSION_INFO@

pkglib_LTLIBRARIES = libasound_module_pcm_jack.la

libasound_module_pcm_jack_la_SOURCES = pcm_jack.c
libasound_module_pcm_jack_la_LDFLAGS = -version-info $(COMPATNUM)
libasound_module_pcm_jack_la_LIBADD = -ljack
alsadir = $(libdir)/alsa_modules

INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/src/pcm

.PHONY : help

help:
	@echo "Specify target by hand, please"
	@echo "Type 'make jack' to build the jack pcm plugin..."
	@echo "Type 'make install-jack' to install the jack pcm plugin..."

all: help

all-am: help

all-redirect: help

install-am: help

hand-install: install-exec-am

jack: libasound_module_pcm_jack.la

install-jack:
	$(mkinstalldirs) $(DESTDIR)$(pkglibdir)
	$(LIBTOOL) --mode=install $(pkglibLTLIBRARIES_INSTALL) $(INSTALL_STRIP_FLAG) libasound_module_pcm_jack.la $(DESTDIR)$(pkglibdir)/libasound_module_pcm_jack.la
