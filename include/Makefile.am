

sysincludedir = ${includedir}/sys
sysinclude_HEADERS = asoundlib.h

# This is the order they will be concatenated into asoundlib.h!
#
header_files=header.h version.h error.h control.h mixer.h pcm.h rawmidi.h \
             footer.h

noinst_HEADERS=$(header_files)

$(srcdir)/asoundlib.h: $(header_files)
	cat $^ > $@

SOUNDLIB_MAJOR=`echo $(VERSION) | cut -d . -f 1`
SOUNDLIB_MINOR=`echo $(VERSION) | cut -d . -f 2`
SOUNDLIB_SUBMINOR=`echo $(VERSION) | cut -d . -f 3`

version.h: stamp-vh
	@:

stamp-vh: $(top_builddir)/configure.in
	@echo "/*" > ver.tmp
	@echo " *  ver.tmp" >> ver.tmp
	@echo " */" >> ver.tmp
	@echo "" >> ver.tmp
	@echo "\#define SOUNDLIB_VERSION_MAJOR		$(SOUNDLIB_MAJOR)" >> ver.tmp
	@echo "\#define SOUNDLIB_VERSION_MINOR		$(SOUNDLIB_MINOR)" >> ver.tmp
	@echo "\#define SOUNDLIB_VERSION_SUBMINOR	$(SOUNDLIB_SUBMINOR)" >> ver.tmp
	@echo "\#define SOUNDLIB_VERSION		( ( SOUNDLIB_VERSION_MAJOR << 16 ) | ( SOUNDLIB_VERSION_MINOR << 8 ) | SOUNDLIB_VERSION_SUBMINOR )" >> ver.tmp
	@cmp -s version.h ver.tmp
          || (echo "Updating version.h"; \
              cp ver.tmp version.h; \
              echo timestamp > stamp-vh)
        -@rm -f ver.tmp

INCLUDES=-I$(top_srcdir)/include

