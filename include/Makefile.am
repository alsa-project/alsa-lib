sysincludedir = ${includedir}/sys
alsaincludedir = ${includedir}/alsa

alsainclude_HEADERS = asoundlib.h \
		      version.h global.h input.h output.h error.h \
		      conf.h pcm.h rawmidi.h timer.h \
		      hwdep.h control.h mixer.h \
		      seq_event.h seq.h seqmid.h seq_midi_event.h \
		      conv.h instr.h

noinst_HEADERS=search.h list.h aserver.h local.h config.h

version.h: stamp-vh
	@:

stamp-vh: $(top_builddir)/configure.in
	@echo "/*" > ver.tmp
	@echo " *  version.h" >> ver.tmp
	@echo " */" >> ver.tmp
	@echo "" >> ver.tmp
	@echo "#define SND_LIB_MAJOR		$(SND_LIB_MAJOR) /**< major number of library version */" >> ver.tmp
	@echo "#define SND_LIB_MINOR		$(SND_LIB_MINOR) /**< minor number of library version */" >> ver.tmp
	@echo "#define SND_LIB_SUBMINOR	$(SND_LIB_SUBMINOR) /**< subminor number of library version */" >> ver.tmp
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
	rm -f $(DESTDIR)$(alsaincludedir)/asoundef.h
	$(LN_S) -f ../sound/asoundef.h $(DESTDIR)$(alsaincludedir)/asoundef.h
	# should be removed in future
	test -d $(DESTDIR)$(sysincludedir) || mkdir -p $(DESTDIR)$(sysincludedir)
	$(INSTALL_DATA) sys.h $(DESTDIR)$(sysincludedir)/asoundlib.h
