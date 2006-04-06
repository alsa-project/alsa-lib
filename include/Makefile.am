SUBDIRS = sound

sysincludedir = ${includedir}/sys
alsaincludedir = ${includedir}/alsa

alsainclude_HEADERS = asoundlib.h asoundef.h \
		      version.h global.h input.h output.h error.h \
		      conf.h pcm.h pcm_old.h pcm_plugin.h rawmidi.h timer.h \
		      hwdep.h control.h mixer.h mixer_abst.h \
		      seq_event.h seq.h seqmid.h seq_midi_event.h \
		      conv.h instr.h iatomic.h \
		      alisp.h pcm_external.h pcm_ioplug.h pcm_extplug.h \
		      pcm_rate.h control_external.h

noinst_HEADERS = alsa sys.h search.h list.h aserver.h local.h alsa-symbols.h

CLEANFILES = stamp-vh version.h alsa

alsa:
	ln -s $(top_srcdir)/include alsa

version.h: stamp-vh alsa
	@:

stamp-vh: $(top_builddir)/configure.in
	@echo "/*" > ver.tmp
	@echo " *  version.h" >> ver.tmp
	@echo " */" >> ver.tmp
	@echo "" >> ver.tmp
	@echo "#define SND_LIB_MAJOR		$(SND_LIB_MAJOR) /**< major number of library version */" >> ver.tmp
	@echo "#define SND_LIB_MINOR		$(SND_LIB_MINOR) /**< minor number of library version */" >> ver.tmp
	@echo "#define SND_LIB_SUBMINOR	$(SND_LIB_SUBMINOR) /**< subminor number of library version */" >> ver.tmp
	@echo "#define SND_LIB_EXTRAVER	$(SND_LIB_EXTRAVER) /**< extra version number, used mainly for betas */" >> ver.tmp
	@echo "/** library version */" >> ver.tmp
	@echo "#define SND_LIB_VERSION		((SND_LIB_MAJOR<<16)|\\" >> ver.tmp
	@echo "				 (SND_LIB_MINOR<<8)|\\" >> ver.tmp
	@echo "				  SND_LIB_SUBMINOR)" >> ver.tmp
	@echo "/** library version (string) */" >> ver.tmp
	@echo "#define SND_LIB_VERSION_STR	\"$(SND_LIB_VERSION)\"" >> ver.tmp
	@echo >> ver.tmp
	@cmp -s version.h ver.tmp \
          || (echo "Updating version.h"; \
              cp ver.tmp version.h; \
              echo timestamp > stamp-vh)
	-@rm -f ver.tmp

INCLUDES=-I$(top_srcdir)/include

install-data-hook:
	test -d $(DESTDIR)$(sysincludedir) || mkdir -p $(DESTDIR)$(sysincludedir)
	$(INSTALL_DATA) $(srcdir)/sys.h $(DESTDIR)$(sysincludedir)/asoundlib.h
