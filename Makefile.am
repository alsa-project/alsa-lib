SUBDIRS=doc include src aserver test utils
EXTRA_DIST=ChangeLog INSTALL TODO configure cvscompile libtool version

INCLUDES=-I$(top_srcdir)/include

rpm: dist
	$(MAKE) -C utils rpm

dist-hook:
	-chmod -R a+r $(distdir)
	@if ! test -z "$(AMTAR)"; then \
		$(AMTAR) cvf $(distdir).tar.bz2 --bzip2 $(distdir); \
	else \
		$(TAR) cvf $(distdir).tar.bz2 --bzip2 $(distdir); \
	fi

doc-dummy:

doc: doc-dummy
	$(MAKE) -C include all
	$(MAKE) -C doc doc
