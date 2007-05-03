SUBDIRS=cards pcm

cfg_files = alsa.conf
if BUILD_ALISP
cfg_files += sndo-mixer.alisp
endif
if BUILD_MODULES
cfg_files += smixer.conf
endif

EXTRA_DIST = $(cfg_files)

alsadir = @ALSA_CONFIG_DIR@
alsa_DATA = $(cfg_files)
