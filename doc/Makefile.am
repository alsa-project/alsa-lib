SUBDIRS=pictures

EXTRA_DIST=README.1st asoundrc.txt doxygen.cfg index.doxygen

AM_CPPFLAGS=-I$(top_srcdir)/include

doc:
	test -e doxygen.cfg || sed s:[@]top_srcdir[@]:..:g doxygen.cfg.in > doxygen.cfg
	doxygen doxygen.cfg

doc-pack: doc
	-chmod -R a+r $(top_srcdir)/doc/doxygen/html/*
	-chmod -R a-w $(top_srcdir)/doc/doxygen/html/*
	if ! test -z "$(AMTAR)"; then \
		$(AMTAR) --create --directory=$(top_srcdir)/doc/doxygen/html --verbose --file=- . | bzip2 -c -9 > $(top_srcdir)/../alsa-lib-doc.tar.bz2 ; \
	else \
		$(TAR) --create --directory=$(top_srcdir)/doc/doxygen/html --verbose --file=- . | bzip2 -c -9 > $(top_srcdir)/../alsa-lib-doc.tar.bz2 ; \
	fi
	-chmod -R u+w $(top_srcdir)/doc/doxygen/html/*
	rm -rf $(top_srcdir)/doc/doxygen/html/*

doc-clean:
	-chmod -R u+w $(top_srcdir)/doc/doxygen/html/*
	rm -rf $(top_srcdir)/doc/doxygen/html/*
