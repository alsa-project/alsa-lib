sysincludedir = ${includedir}/sys
sysinclude_HEADERS = asoundlib.h

# This is the order they will be concatenated into asoundlib.h!
#
header_files=header.h version.h error.h control.h mixer.h pcm.h rawmidi.h \
             footer.h

noinst_HEADERS=$(header_files)

$(srcdir)/asoundlib.h: $(header_files)
	cat $^ > $@


INCLUDES=-I$(top_srcdir)/include
