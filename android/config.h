/* include/config.h.  Generated from config.h.in by configure.  */
/* include/config.h.in.  Generated from configure.ac by autoheader.  */

/* Directory with aload* device files */
#define ALOAD_DEVICE_DIRECTORY "/dev/"

/* directory containing ALSA configuration database */
#define ALSA_CONFIG_DIR "/system/usr/share/alsa"

/* Enable assert at error message handler */
/* #undef ALSA_DEBUG_ASSERT */

/* Directory with ALSA device files */
#define ALSA_DEVICE_DIRECTORY "/dev/snd/"

/* directory containing ALSA add-on modules */
#define ALSA_PLUGIN_DIR "/system/usr/lib/alsa-lib"

/* Build hwdep component */
#define BUILD_HWDEP "1"

/* Build mixer component */
#define BUILD_MIXER "1"

/* Build PCM component */
#define BUILD_PCM "1"

/* Build PCM adpcm plugin */
#define BUILD_PCM_PLUGIN_ADPCM "1"

/* Build PCM alaw plugin */
#define BUILD_PCM_PLUGIN_ALAW "1"

/* Build PCM lfloat plugin */
#define BUILD_PCM_PLUGIN_LFLOAT "1"

/* Build PCM mmap-emul plugin */
#define BUILD_PCM_PLUGIN_MMAP_EMUL "1"

/* Build PCM mulaw plugin */
#define BUILD_PCM_PLUGIN_MULAW "1"

/* Build PCM rate plugin */
#define BUILD_PCM_PLUGIN_RATE "1"

/* Build PCM route plugin */
#define BUILD_PCM_PLUGIN_ROUTE "1"

/* Build raw MIDI component */
/* #undef BUILD_RAWMIDI */

/* Build sequencer component */
/* #undef BUILD_SEQ */

/* Build DSP Topology component */
#define BUILD_TOPOLOGY "1"

/* Build UCM component */
#define BUILD_UCM "1"

/* Have clock gettime */
#define HAVE_CLOCK_GETTIME 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `eaccess' function. */
/* #undef HAVE_EACCESS */

/* Define to 1 if you have the <endian.h> header file. */
#define HAVE_ENDIAN_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Have libdl */
#define HAVE_LIBDL 1

/* Have libpthread */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the `resmgr' library (-lresmgr). */
/* #undef HAVE_LIBRESMGR */

/* Have librt */
/* #undef HAVE_LIBRT */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* MMX technology is enabled */
#define HAVE_MMX "1"

/* Define if your pthreads implementation have PTHREAD_MUTEX_RECURSIVE */
#define HAVE_PTHREAD_MUTEX_RECURSIVE /**/

/* Avoid calculation in float */
/* #undef HAVE_SOFT_FLOAT */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/endian.h> header file. */
/* #undef HAVE_SYS_ENDIAN_H */

/* Define to 1 if you have the <sys/shm.h> header file. */
#define HAVE_SYS_SHM_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `uselocale' function. */
#define HAVE_USELOCALE 1

/* Enable use of wordexp */
/* #undef HAVE_WORDEXP */

/* Define to 1 if compiler supports __thread */
#define HAVE___THREAD 1

/* Lockless dmix as default */
/* #undef LOCKLESS_DMIX_DEFAULT */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* No assert debug */
/* #undef NDEBUG */

/* Name of package */
#define PACKAGE "alsa-lib"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "alsa-lib"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "alsa-lib"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Max number of cards */
#define SND_MAX_CARDS 32

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Support /dev/aload* access for auto-loading */
/* #undef SUPPORT_ALOAD */

/* Support resmgr with alsa-lib */
/* #undef SUPPORT_RESMGR */

/* Disable thread-safe API functions */
#define THREAD_SAFE_API "1"

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* directory to put tmp socket files */
#define TMPDIR "/tmp"

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* compiled with versioned symbols */
/* #undef VERSIONED_SYMBOLS */

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Toolchain Symbol Prefix */
#define __SYMBOL_PREFIX ""

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif
