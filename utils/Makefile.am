if INSTALL_M4
aclocaldir=$(datadir)/aclocal
aclocal_DATA=alsa.m4
endif

rpm: buildrpm alsa-lib.spec
	VERSION=$(VERSION) $(srcdir)/buildrpm

INCLUDES=-I$(top_srcdir)/include
