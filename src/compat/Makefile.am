EXTRA_LTLIBRARIES = libcompat.la
EXTRA_libcompat_la_SOURCES = hsearch_r.c

if Have_hsearch_r
HSEARCH_R =
else
HSEARCH_R = hsearch_r.c
endif

libcompat_la_SOURCES = $(HSEARCH_R)

all: libcompat.la
