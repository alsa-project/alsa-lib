SUBDIRS=doc include src test utils
EXTRA_DIST=ChangeLog INSTALL configure cvscompile libtool

INCLUDES=-I$(top_srcdir)/include

rpm: dist
	$(MAKE) -C utils rpm

pack: clean-recursive
	rm -f config.log config.cache config.status
	chown -R root.root ../alsa-lib
	mv ../alsa-lib ../alsa-lib-$(SND_LIB_VERSION)
	tar --exclude=CVS -cvz -C .. -f ../alsa-lib-$(SND_LIB_VERSION).tar.gz alsa-lib-$(SND_LIB_VERSION)
	mv ../alsa-lib-$(SND_LIB_VERSION) ../alsa-lib 
