sysincludedir = ${includedir}/sys
sysinclude_HEADERS = asoundlib.h

# This is the order they will be concatenated into asoundlib.h!
#
header_files=header.h version.h error.h control.h mixer.h pcm.h rawmidi.h \
             timer.h seq.h conv.h instr.h footer.h

noinst_HEADERS=$(header_files)

asoundlib.h: $(header_files)
	cat $^ > $@

version.h: stamp-vh
	@:

stamp-vh: $(top_builddir)/configure.in
	@echo "/*" > ver.tmp
	@echo " *  version.h" >> ver.tmp
	@echo " */" >> ver.tmp
	@echo "" >> ver.tmp
	@echo "#define SND_LIB_MAJOR		$(SND_LIB_MAJOR)" >> ver.tmp
	@echo "#define SND_LIB_MINOR		$(SND_LIB_MINOR)" >> ver.tmp
	@echo "#define SND_LIB_SUBMINOR	$(SND_LIB_SUBMINOR)" >> ver.tmp
	@echo "#define SND_LIB_VERSION		((SND_LIB_MAJOR<<16)|\\" >> ver.tmp
	@echo "				 (SND_LIB_MINOR<<8)|\\" >> ver.tmp
	@echo "				  SND_LIB_SUBMINOR)" >> ver.tmp
	@echo "#define SND_LIB_VERSION_STR	\"$(SND_LIB_VERSION)\"" >> ver.tmp
	@echo >> ver.tmp
	@echo "/* OBSOLETE DEFINES WHICH WON'T BE IN 0.3.0 FINAL!!! */" >> ver.tmp
	@echo "#define SOUNDLIB_VERSION_MAJOR		SND_LIB_MAJOR" >> ver.tmp
	@echo "#define SOUNDLIB_VERSION_MINOR		SND_LIB_MINOR" >> ver.tmp
	@echo "#define SOUNDLIB_VERSION_SUBMINOR	SND_LIB_SUBMINOR" >> ver.tmp
	@echo >> ver.tmp
	@cmp -s version.h ver.tmp \
          || (echo "Updating version.h"; \
              cp ver.tmp version.h; \
              echo timestamp > stamp-vh)
	-@rm -f ver.tmp

INCLUDES=-I$(top_srcdir)/include
