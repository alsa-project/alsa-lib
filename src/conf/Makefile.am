SUBDIRS=cards pcm

cfg_files = alsa.conf
if BUILD_ALISP
cfg_files += sndo-mixer.alisp
endif

EXTRA_DIST = $(cfg_files)

alsadir = $(datadir)/alsa
alsa_DATA = $(cfg_files)
