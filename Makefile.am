SUBDIRS=doc include src aserver test utils
EXTRA_DIST=ChangeLog INSTALL TODO configure cvscompile libtool version

INCLUDES=-I$(top_srcdir)/include

rpm: dist
	$(MAKE) -C utils rpm

dist-hook:
	-chmod -R a+r $(distdir)
	$(TAR) cvIf $(distdir).tar.bz2 $(distdir)

pack: clean-recursive
	rm -f config.log config.cache config.status
	rm -rf `find . -name ".deps"`
	mv ../alsa-lib ../alsa-lib-$(SND_LIB_VERSION)
	tar --exclude=CVS --owner=root --group=root -cvI -C .. -f ../alsa-lib-$(SND_LIB_VERSION).tar.bz2 alsa-lib-$(SND_LIB_VERSION)
	mv ../alsa-lib-$(SND_LIB_VERSION) ../alsa-lib

doc-dummy:

doc: doc-dummy
	$(MAKE) -C include all
	$(MAKE) -C doc doc
