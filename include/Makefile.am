alsaincludedir = ${includedir}/alsa
alsainclude_HEADERS = asoundlib.h

sysincludedir = ${includedir}/sys

# This is the order they will be concatenated into asoundlib.h!
#
header_files=header.h version.h global.h input.h output.h error.h \
	     conf.h pcm.h rawmidi.h timer.h \
	     hwdep.h control.h mixer.h \
	     seq_event.h seq.h seqmid.h seq_midi_event.h \
             conv.h instr.h footer.h

noinst_HEADERS=$(header_files) search.h list.h aserver.h local.h

asoundlib.h: $(header_files)
	cat $^ > $@

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

install-exec-hook:
	rm -f $(alsaincludedir)/asoundef.h
	(cd $(DESTDIR)$(alsaincludedir) && $(LN_S) -f ../sound/asoundef.h asoundef.h)
	# should be removed in future
	(cd $(DESTDIR)$(sysincludedir) && $(LN_S) -f ../alsa/asoundlib.h asoundlib.h)
