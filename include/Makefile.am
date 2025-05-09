SUBDIRS = sound

sysincludedir = ${includedir}/sys
alsaincludedir = ${includedir}/alsa

alsainclude_HEADERS = asoundlib.h asoundef.h \
		      version.h global.h input.h output.h error.h \
		      conf.h control.h

if BUILD_CTL_PLUGIN
alsainclude_HEADERS += control_plugin.h
endif
if BUILD_CTL_PLUGIN_EXT
alsainclude_HEADERS += control_external.h
endif

if BUILD_PCM
alsainclude_HEADERS += pcm.h pcm_old.h timer.h
if BUILD_PCM_PLUGIN
alsainclude_HEADERS += pcm_plugin.h
endif
if BUILD_PCM_PLUGIN_RATE
alsainclude_HEADERS += pcm_rate.h
endif
if BUILD_PCM_PLUGIN_EXTERNAL
# FIXME: pcm_external.h includes both pcm_extplug.h and pcm_ioplug.h
alsainclude_HEADERS += pcm_external.h pcm_extplug.h pcm_ioplug.h
endif
#if BUILD_PCM_PLUGIN_EXTPLUG
#alsainclude_HEADERS += pcm_extplug.h
#endif
#if BUILD_PCM_PLUGIN_IOPLUG
#alsainclude_HEADERS += pcm_ioplug.h
#endif
endif

if BUILD_RAWMIDI
alsainclude_HEADERS += rawmidi.h ump.h ump_msg.h
endif

if BUILD_HWDEP
alsainclude_HEADERS += hwdep.h
endif

if BUILD_MIXER
alsainclude_HEADERS += mixer.h mixer_abst.h
endif

if BUILD_SEQ
alsainclude_HEADERS += seq_event.h seq.h seqmid.h seq_midi_event.h
endif

if BUILD_UCM
alsainclude_HEADERS += use-case.h
endif

if BUILD_TOPOLOGY
alsainclude_HEADERS += topology.h
endif

noinst_HEADERS = alsa sys.h search.h list.h aserver.h local.h alsa-symbols.h \
	asoundlib-head.h asoundlib-tail.h bswap.h type_compat.h

DISTCLEANFILES = stamp-vh version.h alsa asoundlib.h

.DUMMY: alsa_link
alsa_link:
	if ! test -r alsa/local.h; then \
		ln -s $(top_srcdir)/include alsa; \
	fi

version.h: stamp-vh alsa_link
	for f in asoundlib.h version.h; do \
		if ! test -r $(top_srcdir)/include/$$f; then \
			ln -s $(abs_top_builddir)/include/$$f $(top_srcdir)/include/$$f; \
		fi; \
	done

stamp-vh: $(top_builddir)/configure.ac
	@echo "/*" > ver.tmp
	@echo " *  version.h" >> ver.tmp
	@echo " */" >> ver.tmp
	@echo "" >> ver.tmp
	@echo "#define SND_LIB_MAJOR		$(SND_LIB_MAJOR) /**< major number of library version */" >> ver.tmp
	@echo "#define SND_LIB_MINOR		$(SND_LIB_MINOR) /**< minor number of library version */" >> ver.tmp
	@echo "#define SND_LIB_SUBMINOR	$(SND_LIB_SUBMINOR) /**< subminor number of library version */" >> ver.tmp
	@echo "#define SND_LIB_EXTRAVER	$(SND_LIB_EXTRAVER) /**< extra version number, used mainly for betas */" >> ver.tmp
	@echo "/** library version */" >> ver.tmp
	@echo "#define SND_LIB_VER(maj, min, sub) (((maj)<<16)|((min)<<8)|(sub))" >> ver.tmp
	@echo "#define SND_LIB_VERSION SND_LIB_VER(SND_LIB_MAJOR, SND_LIB_MINOR, SND_LIB_SUBMINOR)" >> ver.tmp
	@echo "/** library version (string) */" >> ver.tmp
	@echo "#define SND_LIB_VERSION_STR	\"$(SND_LIB_VERSION)\"" >> ver.tmp
	@echo >> ver.tmp
	@cmp -s version.h ver.tmp \
          || (echo "Updating version.h"; \
              cp ver.tmp version.h; \
              echo timestamp > stamp-vh)
	-@rm -f ver.tmp

AM_CPPFLAGS=-I$(top_srcdir)/include

install-data-hook:
	test -d $(DESTDIR)$(sysincludedir) || mkdir -p $(DESTDIR)$(sysincludedir)
	$(INSTALL_DATA) $(srcdir)/sys.h $(DESTDIR)$(sysincludedir)/asoundlib.h
	$(INSTALL_DATA) $(srcdir)/sys.h $(DESTDIR)$(includedir)/asoundlib.h
