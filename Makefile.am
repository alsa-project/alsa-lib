SUBDIRS=doc include src test utils
EXTRA_DIST=ChangeLog INSTALL configure cvscompile

INCLUDES=-I$(top_srcdir)/include

rpm: dist
	$(MAKE) -C utils rpm