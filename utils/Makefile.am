aclocaldir=$(datadir)/aclocal
aclocal_DATA=alsa.m4

rpm: buildrpm alsa-lib.spec
	VERSION=$(VERSION) $(srcdir)/buildrpm

INCLUDES=-I$(top_srcdir)/include
