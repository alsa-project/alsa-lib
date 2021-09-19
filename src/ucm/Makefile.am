EXTRA_LTLIBRARIES = libucm.la

libucm_la_SOURCES = utils.c parser.c ucm_cond.c ucm_subs.c ucm_include.c \
		    ucm_regex.c ucm_exec.c main.c

noinst_HEADERS = ucm_local.h ucm_confdoc.h

all: libucm.la


AM_CPPFLAGS=-I$(top_srcdir)/include
