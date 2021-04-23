SUBDIRS=cards ctl pcm

cfg_files = alsa.conf
if BUILD_ALISP
cfg_files += sndo-mixer.alisp
endif
if BUILD_MODULES
if BUILD_MIXER_MODULES
cfg_files += smixer.conf
endif
endif

EXTRA_DIST = $(cfg_files)

alsaconfigdir = @ALSA_CONFIG_DIR@
alsadir = $(alsaconfigdir)
alsa_DATA = $(cfg_files)
