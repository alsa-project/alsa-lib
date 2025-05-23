dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT(alsa-lib, 1.2.14)

AC_CONFIG_SRCDIR([src/control/control.c])
AC_CONFIG_MACRO_DIR([m4])

dnl *************************************************
dnl current:revision:age
dnl change (without API) = c:r+1:a
dnl change API = c+1:0:a
dnl add API = c+1:0:a+1
dnl remove API = c+1:0:0
dnl *************************************************
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE
eval LIBTOOL_VERSION_INFO="2:0:0"
dnl *************************************************
AM_CONDITIONAL([INSTALL_M4], [test -n "${ACLOCAL}"])

AM_MAINTAINER_MODE([enable])

# Test for new silent rules and enable only if they are available
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PREFIX_DEFAULT(/usr)

dnl Checks for programs.

AC_PROG_CC
AC_PROG_CPP
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_INSTALL
AC_PROG_LN_S 
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

CC_NOUNDEFINED

dnl Checks for header files.
AC_HEADER_STDC
AC_CONFIG_HEADERS(include/config.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_CHECK_ATTRIBUTE_SYMVER

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_CHECK_FUNCS([uselocale])
AC_CHECK_FUNCS([eaccess])

dnl Enable largefile support
AC_SYS_LARGEFILE

SAVE_LIBRARY_VERSION
AC_SUBST(LIBTOOL_VERSION_INFO)

test "x$prefix" = xNONE && prefix=$ac_default_prefix

dnl Do not build static and shared libraries together
if test "$enable_static" = "$enable_shared" -a "$enable_static" = "yes"; then
cat <<EOF
  Please, do not try to compile static and shared libraries together.
  See INSTALL file for more details (do not use --enable-shared=yes with
  --enable-static=yes).
EOF
  exit 1
fi

dnl Do not use --with-pic for shared libraries (consider to use PIE)
if test "$enable_static" = "yes" -a "$pic_mode" = "yes"; then
cat <<EOF
  Please, do not enable PIC (--with-pic) for static library. Consider
  to remove this option or use PIE flags.
EOF
  exit 1
fi

dnl ALSA configuration directory
AC_ARG_WITH(configdir,
    AS_HELP_STRING([--with-configdir=dir],
	[path where ALSA config files are stored]),
    confdir="$withval", confdir="")
if test -z "$confdir"; then
    eval dir="$datadir"
    case "$dir" in
    /*) ;;
    *) dir="$prefix/share"
    esac
    confdir="$dir/alsa"
fi
ALSA_CONFIG_DIR="$confdir"
AC_DEFINE_UNQUOTED(ALSA_CONFIG_DIR, "$confdir", [directory containing ALSA configuration database])
AC_SUBST(ALSA_CONFIG_DIR)

dnl ALSA plugin directory
test "x$exec_prefix" = xNONE && exec_prefix=$prefix

AC_ARG_WITH(plugindir,
    AS_HELP_STRING([--with-plugindir=dir],
	[path where ALSA plugin files are stored]),
    plugindir="$withval", plugindir="")
if test -z "$plugindir"; then
    eval dir="$libdir"
    case "$dir" in
    /*) ;;
    *) dir="$dir"
    esac
    plugindir="$dir/$PACKAGE"
fi
AC_DEFINE_UNQUOTED(ALSA_PLUGIN_DIR, "$plugindir", [directory containing ALSA add-on modules])
ALSA_PLUGIN_DIR="$plugindir"
AC_SUBST(ALSA_PLUGIN_DIR)

AC_ARG_WITH(pkgconfdir,
    AS_HELP_STRING([--with-pkgconfdir=dir],
	[path where pkgconfig files are stored]),
    pkgconfdir="$withval", pkgconfdir="")
if test -z "$pkgconfdir"; then
    eval dir="$libdir"
    case "$dir" in
    /*) ;;
    *) dir="$dir"
    esac
    pkgconfdir="$dir/pkgconfig"
fi
AC_DEFINE_UNQUOTED(ALSA_PKGCONF_DIR, "$pkgconfdir", [directory containing pkgconfig files])
ALSA_PKGCONF_DIR="$pkgconfdir"
AC_SUBST(ALSA_PKGCONF_DIR)

dnl Check for versioned symbols
AC_MSG_CHECKING(for versioned symbols)
AC_ARG_WITH(versioned,
  AS_HELP_STRING([--with-versioned],
    [shared library will be compiled with versioned symbols (default = yes)]),
  versioned="$withval", versioned="yes")
if test "$versioned" = "yes"; then
  # it seems that GNU ld versions since 2.10 are not broken
  xres=`grep '^VERSION=' ${srcdir}/ltmain.sh | cut -d = -f 2 | cut -d \" -f 2`
  major=`echo $xres | cut -d . -f 1`
  minor=`echo $xres | cut -d . -f 2`
  pass=0
  if test $major -eq 1 && test $minor -gt 3; then
    pass=1
  else
    if test $major -gt 1; then
      pass=1
    fi
  fi
  if test $pass -eq 1; then
    AC_DEFINE(VERSIONED_SYMBOLS,,[compiled with versioned symbols])
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(broken libtool - use libtool v1.4+; no versions)
  fi
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([VERSIONED_SYMBOLS], [test x$versioned = xyes])

dnl Check for symbolic-functions
AC_MSG_CHECKING(for symbolic-functions)
AC_ARG_ENABLE(symbolic-functions,
  AS_HELP_STRING([--enable-symbolic-functions],
    [use -Bsymbolic-functions option if available (optmization for size and speed)]),
  symfuncs="$enableval", symfuncs="no")
if test "$symfuncs" = "yes"; then
  if ld --help | grep -q -- '-Bsymbolic-functions'; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(not supported by ld)
    symfuncs="no"
  fi
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([SYMBOLIC_FUNCTIONS], [test x"$symfuncs" = xyes])

dnl See if toolchain has a custom prefix for symbols ...
AC_MSG_CHECKING(for custom symbol prefixes)
SYMBOL_PREFIX=` \
	echo "PREFIX=__USER_LABEL_PREFIX__" \
		| ${CPP-${CC-gcc} -E} - 2>&1 \
		| ${EGREP-grep} "^PREFIX=" \
		| ${SED-sed} "s:^PREFIX=::"`
AC_DEFINE_UNQUOTED([__SYMBOL_PREFIX], "$SYMBOL_PREFIX", [Toolchain Symbol Prefix])
AC_SUBST(SYMBOL_PREFIX)
AC_MSG_RESULT($SYMBOL_PREFIX)

dnl Check for debug...
AC_MSG_CHECKING(for debug)
AC_ARG_WITH(debug,
  AS_HELP_STRING([--with-debug],
    [library will be compiled with asserts (default = yes)]),
  debug="$withval", debug="yes")
if test "$debug" = "yes"; then
  AC_MSG_RESULT(yes)
else
  AC_DEFINE(NDEBUG,,[No assert debug])
  AC_MSG_RESULT(no)
fi

if test "$debug" = "yes"; then
  AC_MSG_CHECKING(for debug assert)
  AC_ARG_ENABLE(debug-assert,
    AS_HELP_STRING([--enable-debug],
      [enable assert call at the default error message handler]),
    debug_assert="$enableval", debug_assert="no")
  if test "$debug_assert" = "yes"; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(ALSA_DEBUG_ASSERT,,[Enable assert at error message handler])
  else
    AC_MSG_RESULT(no)
  fi
fi

dnl Temporary directory
AC_MSG_CHECKING(for tmpdir)
AC_ARG_WITH(tmpdir,
  AS_HELP_STRING([--with-tmpdir=directory],
    [directory to put tmp socket files (/tmp)]),
  tmpdir="$withval", tmpdir="/tmp")
AC_MSG_RESULT($tmpdir)
AC_DEFINE_UNQUOTED(TMPDIR, "$tmpdir", [directory to put tmp socket files])

dnl Check for softfloat...
AC_MSG_CHECKING(for softfloat)
AC_ARG_WITH(softfloat,
  AS_HELP_STRING([--with-softfloat],
    [do you have floating point unit on this machine? (optional)]),
  [case "$withval" in
	y|yes) softfloat=yes ;;
	*) softfloat=no ;;
   esac],)
if test "$softfloat" = "yes" ; then
  AC_DEFINE(HAVE_SOFT_FLOAT, "1", [Avoid calculation in float])
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

ALSA_DEPLIBS=""
if test "$softfloat" != "yes"; then
  ALSA_DEPLIBS="-lm"
fi

dnl Check for scandir64
AC_MSG_CHECKING(for LFS calls)
AC_TRY_LINK([#include <dirent.h>],
    [struct dirent64 a; ],
    [have_lfs=yes],
    [have_lfs=no])
if test "$have_lfs" = "yes"; then
  AC_MSG_RESULT(yes)
  AC_DEFINE([HAVE_LFS], 1, [Have LFS])
else
  AC_MSG_RESULT(no)
fi

dnl Check for libdl
AC_MSG_CHECKING(for libdl)
AC_ARG_WITH(libdl,
  AS_HELP_STRING([--with-libdl], [Use libdl for plugins (default = yes)]),
  [ have_libdl="$withval" ], [ have_libdl="yes" ])
HAVE_LIBDL=
if test "$have_libdl" = "yes"; then
  AC_SEARCH_LIBS([dlsym], [dl], [HAVE_LIBDL="yes"])
  if test "$HAVE_LIBDL" = "yes" ; then
    AC_DEFINE([HAVE_LIBDL], 1, [Have libdl])
  fi
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([BUILD_MODULES], [test "$HAVE_LIBDL" = "yes"])

dnl Check for pthread
AC_MSG_CHECKING(for pthread)
AC_ARG_WITH(pthread,
  AS_HELP_STRING([--with-pthread], [Use pthread (default = yes)]),
  [ have_pthread="$withval" ], [ have_pthread="yes" ])
if test "$have_pthread" = "yes"; then
  AC_CHECK_LIB([pthread], [pthread_join], [HAVE_LIBPTHREAD="yes"])
  if test "$HAVE_LIBPTHREAD" = "yes"; then
    ALSA_DEPLIBS="$ALSA_DEPLIBS -lpthread"
    AC_DEFINE([HAVE_LIBPTHREAD], 1, [Have libpthread])
  fi
else
  AC_MSG_RESULT(no)
fi

dnl Check for pthread
if test "$HAVE_LIBPTHREAD" = "yes"; then
  AC_CHECK_DECL(PTHREAD_MUTEX_RECURSIVE,
    AC_DEFINE(HAVE_PTHREAD_MUTEX_RECURSIVE, [],
      [Define if your pthreads implementation have PTHREAD_MUTEX_RECURSIVE]),
    ,
    [#include <pthread.h>])
fi

dnl Check for __thread
AC_MSG_CHECKING([for __thread])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && ((__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 1) || (__GNUC__ == 4 && __GNUC_MINOR__ == 1 && __GNUC_PATCHLEVEL__ < 2))
#error gcc has this bug: http://gcc.gnu.org/ml/gcc-bugs/2006-09/msg02275.html
#endif], [static __thread int p = 0])],
[AC_DEFINE(HAVE___THREAD, 1,
Define to 1 if compiler supports __thread)
AC_MSG_RESULT([yes])],
[AC_MSG_RESULT([no])])

dnl Check for librt
AC_MSG_CHECKING(for librt)
AC_ARG_WITH(librt,
  AS_HELP_STRING([--with-librt], [Use librt for monotonic clock (default = yes)]),
  [ have_librt="$withval" ], [ have_librt="yes" ])
if test "$have_librt" = "yes"; then
  AC_CHECK_LIB([rt], [clock_gettime], [HAVE_LIBRT="yes"])
  if test "$HAVE_LIBRT" = "yes" ; then
    ALSA_DEPLIBS="$ALSA_DEPLIBS -lrt"
    AC_DEFINE([HAVE_LIBRT], 1, [Have librt])
    AC_DEFINE([HAVE_CLOCK_GETTIME], 1, [Have clock gettime])
  fi
else
  AC_MSG_RESULT(no)
fi

AC_SUBST(ALSA_DEPLIBS)

dnl Check for use of wordexp...
AC_MSG_CHECKING(for use of wordexp)
AC_ARG_WITH(wordexp,
  AS_HELP_STRING([--with-wordexp],
    [Use wordexp when expanding configs (default = no)]),
  [case "$withval" in
	y|yes) wordexp=yes ;;
	*) wordexp=no ;;
   esac],)
if test "$wordexp" = "yes" ; then
  AC_DEFINE(HAVE_WORDEXP, "1", [Enable use of wordexp])
  AC_MSG_RESULT(yes)
  AC_CHECK_HEADER([wordexp.h],[], [AC_MSG_ERROR([Couldn't find wordexp.h])])
else
  AC_MSG_RESULT(no)
fi

dnl Check for headers
AC_CHECK_HEADERS([endian.h sys/endian.h sys/shm.h malloc.h])

dnl Check for resmgr support...
AC_MSG_CHECKING(for resmgr support)
AC_ARG_ENABLE(resmgr,
  AS_HELP_STRING([--enable-resmgr], [support resmgr (optional)]),
  resmgr="$enableval", resmgr="no")
AC_MSG_RESULT($resmgr)
if test "$resmgr" = "yes"; then
  AC_CHECK_LIB(resmgr, rsm_open_device,,
    AC_ERROR([Cannot find libresmgr]))
  AC_DEFINE(SUPPORT_RESMGR, "1", [Support resmgr with alsa-lib])
fi

dnl Check for aload* support...
AC_MSG_CHECKING(for aload* support)
AC_ARG_ENABLE(aload,
  AS_HELP_STRING([--disable-aload], [disable reading /dev/aload*]),
  aload="$enableval", aload="yes")
AC_MSG_RESULT($aload)
if test "$aload" = "yes"; then
  AC_DEFINE(SUPPORT_ALOAD, "1", [Support /dev/aload* access for auto-loading])
fi

dnl Check for non-standard /dev directory
AC_MSG_CHECKING([for ALSA device file directory])
AC_ARG_WITH(alsa-devdir,
  AS_HELP_STRING([--with-alsa-devdir=dir],
    [directory with ALSA device files (default /dev/snd)]),
  [alsa_dev_dir="$withval"],
  [alsa_dev_dir="/dev/snd"])
dnl make sure it has a trailing slash
if echo "$alsa_dev_dir" | grep -v '/$' > /dev/null; then
  alsa_dev_dir="$alsa_dev_dir/"
fi
AC_DEFINE_UNQUOTED(ALSA_DEVICE_DIRECTORY, "$alsa_dev_dir", [Directory with ALSA device files])
AC_MSG_RESULT([$alsa_dev_dir])

AC_MSG_CHECKING([for aload* device file directory])
AC_ARG_WITH(aload-devdir,
  AS_HELP_STRING([--with-aload-devdir=dir],
    [directory with aload* device files (default /dev)]),
  [aload_dev_dir="$withval"],
  [aload_dev_dir="/dev"])
if echo "$aload_dev_dir" | grep -v '/$' > /dev/null; then
  aload_dev_dir="$aload_dev_dir/"
fi
AC_DEFINE_UNQUOTED(ALOAD_DEVICE_DIRECTORY, "$aload_dev_dir", [Directory with aload* device files])
AC_MSG_RESULT([$aload_dev_dir])

dnl Build conditions
AC_ARG_ENABLE(mixer,
  AS_HELP_STRING([--disable-mixer], [disable the mixer component]),
  [build_mixer="$enableval"], [build_mixer="yes"])
AC_ARG_ENABLE(pcm,
  AS_HELP_STRING([--disable-pcm], [disable the PCM component]),
  [build_pcm="$enableval"], [build_pcm="yes"])
AC_ARG_ENABLE(rawmidi,
  AS_HELP_STRING([--disable-rawmidi], [disable the raw MIDI component]),
  [build_rawmidi="$enableval"], [build_rawmidi="yes"])
AC_ARG_ENABLE(hwdep,
  AS_HELP_STRING([--disable-hwdep], [disable the hwdep component]),
  [build_hwdep="$enableval"], [build_hwdep="yes"])
AC_ARG_ENABLE(seq,
  AS_HELP_STRING([--disable-seq], [disable the sequencer component]),
  [build_seq="$enableval"], [build_seq="yes"])
AC_ARG_ENABLE(ucm,
  AS_HELP_STRING([--disable-ucm], [disable the use-case-manager component]),
  [build_ucm="$enableval"], [build_ucm="yes"])
AC_ARG_ENABLE(topology,
  AS_HELP_STRING([--disable-topology], [disable the DSP topology component]),
  [build_topology="$enableval"], [build_topology="yes"])
AC_ARG_ENABLE(old-symbols,
  AS_HELP_STRING([--disable-old-symbols], [disable old obsoleted symbols]),
  [keep_old_symbols="$enableval"], [keep_old_symbols="yes"])
AM_CONDITIONAL([KEEP_OLD_SYMBOLS], [test x$keep_old_symbols = xyes])

AC_ARG_ENABLE(mixer-modules,
  AS_HELP_STRING([--enable-mixer-modules], [enable the additional mixer modules (experimental)]),
  [build_mixer_modules="$enableval"], [build_mixer_modules="no"])

AC_ARG_ENABLE(mixer-pymods,
  AS_HELP_STRING([--enable-mixer-pymods], [enable the mixer python modules (experimental)]),
  [build_mixer_pymodules="$enableval"], [build_mixer_pymodules="no"])

AC_ARG_ENABLE(python,
  AS_HELP_STRING([--disable-python], [disable the python components]),
  [build_python="$enableval"], [build_python="yes"])

AC_ARG_ENABLE(python2,
  AS_HELP_STRING([--enable-python2], [prefer python2]),
  [build_python2="$enableval"], [build_python2="no"])
PYTHON_LIBS=""
PYTHON_INCLUDES=""
if test "$build_python" = "yes" -a "$build_mixer_pymodules" = "yes"; then
  pythonlibs0=
  pythoninc0=
  if test "$build_python2" != "yes"; then
    pythonlibs0=$(python3-config --libs --embed 2> /dev/null)
    if test -z "$pythonlibs0"; then
      pythonlibs0=$(python3-config --libs)
    fi
    pythoninc0=$(python3-config --includes)
  fi
  if test -z "$pythonlibs0"; then
    pythonlibs0=$(python-config --libs)
    pythoninc0=$(python-config --includes)
  fi
  AC_ARG_WITH(pythonlibs,
    AS_HELP_STRING([--with-pythonlibs=ldflags],
      [specify python libraries (-lpthread -lm -ldl -lpython2.4)]),
    pythonlibs="$withval", pythonlibs=$pythonlibs0)
  AC_ARG_WITH(pythonincludes,
    AS_HELP_STRING([--with-pythonincludes=Cflags],
      [specify python C header files (-I/usr/include/python)]),
    pythonincludes="$withval", pythonincludes=$pythoninc0)
  if test -z "$pythonlibs"; then
    echo "Unable to determine python libraries! Probably python-config is not"
    echo "available on this system. Please, use --with-pythonlibs and"
    echo "--with-pythonincludes options. Python components are disabled in this build."
    build_python="no"
  else
    PYTHON_LIBS="$pythonlibs"
    PYTHON_INCLUDES="$pythonincludes"
  fi
fi
if test "$build_python" != "yes"; then
  build_mixer_pymodules=
fi
AC_SUBST(PYTHON_LIBS)
AC_SUBST(PYTHON_INCLUDES)

if test "$build_rawmidi" != "yes"; then
  if test "$build_seq" = "yes"; then
    AC_ERROR([Cannot enable sequencer without rawmidi])
  fi
fi

AM_CONDITIONAL([BUILD_MIXER], [test x$build_mixer = xyes])
AM_CONDITIONAL([BUILD_PCM], [test x$build_pcm = xyes])
AM_CONDITIONAL([BUILD_RAWMIDI], [test x$build_rawmidi = xyes])
AM_CONDITIONAL([BUILD_HWDEP], [test x$build_hwdep = xyes])
AM_CONDITIONAL([BUILD_SEQ], [test x$build_seq = xyes])
AM_CONDITIONAL([BUILD_UCM], [test x$build_ucm = xyes])
AM_CONDITIONAL([BUILD_TOPOLOGY], [test x$build_topology = xyes])
AM_CONDITIONAL([BUILD_MIXER_MODULES], [test x$build_mixer_modules = xyes])
AM_CONDITIONAL([BUILD_MIXER_PYMODULES], [test x$build_mixer_pymodules = xyes])

if test "$build_mixer" = "yes"; then
  AC_DEFINE([BUILD_MIXER], "1", [Build mixer component])
fi
if test "$build_pcm" = "yes"; then
  AC_DEFINE([BUILD_PCM], "1", [Build PCM component])
fi
if test "$build_rawmidi" = "yes"; then
  AC_DEFINE([BUILD_RAWMIDI], "1", [Build raw MIDI component])
fi
if test "$build_hwdep" = "yes"; then
  AC_DEFINE([BUILD_HWDEP], "1", [Build hwdep component])
fi
if test "$build_seq" = "yes"; then
  AC_DEFINE([BUILD_SEQ], "1", [Build sequencer component])
fi
if test "$build_ucm" = "yes"; then
  AC_DEFINE([BUILD_UCM], "1", [Build UCM component])
fi
if test "$build_topology" = "yes"; then
  AC_DEFINE([BUILD_TOPOLOGY], "1", [Build DSP Topology component])
fi

dnl PCM Plugins

if test "$build_pcm" = "yes"; then
AC_ARG_WITH(pcm-plugins,
  AS_HELP_STRING([--with-pcm-plugins=<list>],
    [build PCM plugins (default = all)]),
  [pcm_plugins="$withval"], [pcm_plugins="all"])
else
pcm_plugins=""
fi

dnl check atomics for pcm_meter

AC_MSG_CHECKING([whether GCC supports builtin atomic intrinsics])
if test -z "$gcc_have_atomics"; then
  gcc_have_atomics=no
  AC_TRY_LINK([],
    [int i;
     __atomic_load_n(&i, __ATOMIC_SEQ_CST);
     __atomic_add_fetch(&i, 0, __ATOMIC_SEQ_CST);
    ],
    [gcc_have_atomics=yes],
    [gcc_have_atomics=no])
fi
AC_MSG_RESULT($gcc_have_atomics)

dnl check mmx register for pcm_dmix_i386

AC_TRY_LINK([],
    [__asm__ volatile ("" : : : "mm0");],
    [AC_DEFINE([HAVE_MMX], "1", [MMX technology is enabled])],
    [])

PCM_PLUGIN_LIST="copy linear route mulaw alaw adpcm rate plug multi shm file null empty share meter hooks lfloat ladspa dmix dshare dsnoop asym iec958 softvol extplug ioplug mmap_emul"

build_pcm_plugin="no"
for t in $PCM_PLUGIN_LIST; do
  eval build_pcm_$t="no"
done

pcm_plugins=`echo $pcm_plugins | sed 's/,/ /g'`
for p in $pcm_plugins; do
  for t in $PCM_PLUGIN_LIST; do
    if test "$p" = "$t" -o "$p" = "all"; then
      eval build_pcm_$t="yes"
      build_pcm_plugin="yes"
    fi
  done
done

dnl special dependencies
if test "$build_pcm_plug" = "yes"; then
  build_pcm_linear="yes"
  build_pcm_copy="yes"
fi

if test "$build_pcm_ioplug" = "yes"; then
  build_pcm_extplug="yes"
fi

if test "$HAVE_LIBDL" != "yes"; then
  build_pcm_meter="no"
  build_pcm_ladspa="no"
  build_pcm_pcm_ioplug="no"
  build_pcm_pcm_extplug="no"
fi

if test "$HAVE_LIBPTHREAD" != "yes"; then
  build_pcm_share="no"
fi

if test "$softfloat" = "yes"; then
  build_pcm_lfloat="no"
  build_pcm_ladspa="no"
fi

if test "$gcc_have_atomics" != "yes"; then
  build_pcm_meter="no"
fi

if test "$ac_cv_header_sys_shm_h" != "yes"; then
  build_pcm_dmix="no"
  build_pcm_dshare="no"
  build_pcm_dsnoop="no"
  build_pcm_shm="no"
fi

AM_CONDITIONAL([BUILD_PCM_PLUGIN], [test x$build_pcm_plugin = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_COPY], [test x$build_pcm_copy = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_LINEAR], [test x$build_pcm_linear = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_ROUTE], [test x$build_pcm_route = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_MULAW], [test x$build_pcm_mulaw = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_ALAW], [test x$build_pcm_alaw = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_ADPCM], [test x$build_pcm_adpcm = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_RATE], [test x$build_pcm_rate = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_PLUG], [test x$build_pcm_plug = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_MULTI], [test x$build_pcm_multi = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_SHM], [test x$build_pcm_shm = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_FILE], [test x$build_pcm_file = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_NULL], [test x$build_pcm_null = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_EMPTY], [test x$build_pcm_empty = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_SHARE], [test x$build_pcm_share = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_METER], [test x$build_pcm_meter = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_HOOKS], [test x$build_pcm_hooks = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_LFLOAT], [test x$build_pcm_lfloat = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_LADSPA], [test x$build_pcm_ladspa = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_DMIX], [test x$build_pcm_dmix = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_DSHARE], [test x$build_pcm_dshare = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_DSNOOP], [test x$build_pcm_dsnoop = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_ASYM], [test x$build_pcm_asym = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_IEC958], [test x$build_pcm_iec958 = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_SOFTVOL], [test x$build_pcm_softvol = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_EXTERNAL], [test x$build_pcm_extplug = xyes -o x$build_pcm_ioplug = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_EXTPLUG], [test x$build_pcm_extplug = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_IOPLUG], [test x$build_pcm_ioplug = xyes])
AM_CONDITIONAL([BUILD_PCM_PLUGIN_MMAP_EMUL], [test x$build_pcm_mmap_emul = xyes])

dnl Defines for plug plugin
if test "$build_pcm_rate" = "yes"; then
  AC_DEFINE([BUILD_PCM_PLUGIN_RATE], "1", [Build PCM rate plugin])
fi
if test "$build_pcm_route" = "yes"; then
  AC_DEFINE([BUILD_PCM_PLUGIN_ROUTE], "1", [Build PCM route plugin])
fi
if test "$build_pcm_lfloat" = "yes"; then
  AC_DEFINE([BUILD_PCM_PLUGIN_LFLOAT], "1", [Build PCM lfloat plugin])
fi
if test "$build_pcm_adpcm" = "yes"; then
  AC_DEFINE([BUILD_PCM_PLUGIN_ADPCM], "1", [Build PCM adpcm plugin])
fi
if test "$build_pcm_mulaw" = "yes"; then
  AC_DEFINE([BUILD_PCM_PLUGIN_MULAW], "1", [Build PCM mulaw plugin])
fi
if test "$build_pcm_alaw" = "yes"; then
  AC_DEFINE([BUILD_PCM_PLUGIN_ALAW], "1", [Build PCM alaw plugin])
fi
if test "$build_pcm_iec958" = "yes"; then
  AC_DEFINE([BUILD_PCM_PLUGIN_IEC958], "1", [Build PCM iec958 plugin])
fi
if test "$build_pcm_mmap_emul" = "yes"; then
  AC_DEFINE([BUILD_PCM_PLUGIN_MMAP_EMUL], "1", [Build PCM mmap-emul plugin])
fi

if test "$build_pcm_dmix" = "yes"; then
AC_MSG_CHECKING(for default lockless dmix)
AC_ARG_ENABLE(lockless-dmix,
  AS_HELP_STRING([--enable-lockless-dmix],
    [use lockless dmix as default on x86]),
  lockless_dmix="$enableval", lockless_dmix="no")
if test "$lockless_dmix" = "yes"; then
  AC_MSG_RESULT(yes)
  AC_DEFINE([LOCKLESS_DMIX_DEFAULT], "1", [Lockless dmix as default])
else
  AC_MSG_RESULT(no)
fi
fi

dnl Create PCM plugin symbol list for static library
rm -f "$srcdir"/src/pcm/pcm_symbols_list.c
touch "$srcdir"/src/pcm/pcm_symbols_list.c
for t in $PCM_PLUGIN_LIST; do
  if eval test \$build_pcm_$t = yes; then
    echo \&_snd_module_pcm_$t, >> "$srcdir"/src/pcm/pcm_symbols_list.c
  fi
done

dnl Control Plugins

AC_ARG_WITH(ctl-plugins,
  AS_HELP_STRING([--with-ctl-plugins=<list>],
    [build control plugins (default = all)]),
  [ctl_plugins="$withval"], [ctl_plugins="all"])

CTL_PLUGIN_LIST="remap shm ext"

build_ctl_plugin="no"
for t in $CTL_PLUGIN_LIST; do
  eval build_ctl_$t="no"
done

ctl_plugins=`echo $ctl_plugins | sed 's/,/ /g'`
for p in $ctl_plugins; do
  for t in $CTL_PLUGIN_LIST; do
    if test "$p" = "$t" -o "$p" = "all"; then
      eval build_ctl_$t="yes"
      build_ctl_plugin="yes"
    fi
  done
done

if test "$ac_cv_header_sys_shm_h" != "yes"; then
  build_ctl_shm="no"
fi

AM_CONDITIONAL([BUILD_CTL_PLUGIN], [test x$build_ctl_plugin = xyes])
AM_CONDITIONAL([BUILD_CTL_PLUGIN_REMAP], [test x$build_ctl_remap = xyes])
AM_CONDITIONAL([BUILD_CTL_PLUGIN_SHM], [test x$build_ctl_shm = xyes])
AM_CONDITIONAL([BUILD_CTL_PLUGIN_EXT], [test x$build_ctl_ext = xyes])

dnl Create ctl plugin symbol list for static library
rm -f "$srcdir"/src/control/ctl_symbols_list.c
touch "$srcdir"/src/control/ctl_symbols_list.c
for t in $CTL_PLUGIN_LIST; do
  if eval test \$build_ctl_$t = yes; then
    echo \&_snd_module_control_$t, >> "$srcdir"/src/control/ctl_symbols_list.c
  fi
done

dnl Max number of cards
AC_MSG_CHECKING(for max number of cards)
AC_ARG_WITH(max-cards,
  AS_HELP_STRING([--with-max-cards], [Specify the max number of cards (default = 32)]),
  [ max_cards="$withval" ], [ max_cards="32" ])
AC_MSG_RESULT([$max_cards])

if test "$max_cards" -lt 1; then
   AC_ERROR([Invalid max cards $max_cards])
elif test "$max_cards" -gt 256; then
   AC_ERROR([Invalid max cards $max_cards])
fi
AC_DEFINE_UNQUOTED(SND_MAX_CARDS, $max_cards, [Max number of cards])

dnl Check for thread-safe API functions
if test "$HAVE_LIBPTHREAD" = "yes"; then
AC_MSG_CHECKING(for thread-safe API functions)
AC_ARG_ENABLE(thread-safety,
  AS_HELP_STRING([--disable-thread-safety],
    [disable thread-safe API functions]),
  threadsafe="$enableval", threadsafe="yes")
if test "$threadsafe" = "yes"; then
  AC_MSG_RESULT(yes)
  AC_DEFINE([THREAD_SAFE_API], "1", [Disable thread-safe API functions])
else
  AC_MSG_RESULT(no)
fi
fi

dnl Make a symlink for inclusion of alsa/xxx.h
if test ! -L "$srcdir"/include/alsa ; then
  echo "Making a symlink include/alsa"
  rm -f "$srcdir"/include/alsa
  ln -sf . "$srcdir"/include/alsa
fi

AC_CONFIG_FILES(Makefile doc/Makefile doc/pictures/Makefile doc/doxygen.cfg \
		include/Makefile include/sound/Makefile include/sound/uapi/Makefile \
		src/Versions.in src/Makefile \
		src/control/Makefile src/mixer/Makefile \
		src/pcm/Makefile src/pcm/scopes/Makefile \
		src/rawmidi/Makefile src/timer/Makefile \
		src/hwdep/Makefile src/seq/Makefile src/ucm/Makefile \
		src/topology/Makefile \
		src/conf/Makefile \
		src/conf/cards/Makefile \
		src/conf/ctl/Makefile \
		src/conf/pcm/Makefile \
		modules/Makefile modules/mixer/Makefile modules/mixer/simple/Makefile \
		aserver/Makefile \
		test/Makefile test/lsb/Makefile \
		utils/Makefile utils/alsa-lib.spec utils/alsa.pc utils/alsa-topology.pc)

AC_OUTPUT()

dnl Create asoundlib.h dynamically according to configure options
echo "Creating asoundlib.h..."
cp "$srcdir"/include/asoundlib-head.h include/asoundlib.h
if test "$ac_cv_header_sys_endian_h" != "yes"; then
test "$ac_cv_header_endian_h" = "yes" && echo "#include <endian.h>" >> include/asoundlib.h
else
cat >> include/asoundlib.h <<EOF
#include <sys/endian.h>
#ifndef __BYTE_ORDER
#define __BYTE_ORDER BYTE_ORDER
#endif
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#endif
EOF
fi
cat >> include/asoundlib.h <<EOF

#ifndef DOC_HIDDEN
#ifndef __GNUC__
#define __inline__ inline
#endif
#endif /* DOC_HIDDEN */

#include <alsa/asoundef.h>
#include <alsa/version.h>
#include <alsa/global.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/error.h>
#include <alsa/conf.h>
EOF
test "$build_pcm" = "yes" && echo "#include <alsa/pcm.h>" >> include/asoundlib.h
test "$build_rawmidi" = "yes" && echo "#include <alsa/rawmidi.h>" >> include/asoundlib.h
test "$build_rawmidi" = "yes" && echo "#include <alsa/ump.h>" >> include/asoundlib.h
test "$build_pcm" = "yes" && echo "#include <alsa/timer.h>" >> include/asoundlib.h
test "$build_hwdep" = "yes" && echo "#include <alsa/hwdep.h>" >> include/asoundlib.h
echo "#include <alsa/control.h>" >> include/asoundlib.h
test "$build_mixer" = "yes" && echo "#include <alsa/mixer.h>" >> include/asoundlib.h
test "$build_seq" = "yes" && echo "#include <alsa/ump_msg.h>" >> include/asoundlib.h
test "$build_seq" = "yes" && echo "#include <alsa/seq_event.h>" >> include/asoundlib.h
test "$build_seq" = "yes" && echo "#include <alsa/seq.h>" >> include/asoundlib.h
test "$build_seq" = "yes" && echo "#include <alsa/seqmid.h>" >> include/asoundlib.h
test "$build_seq" = "yes" && echo "#include <alsa/seq_midi_event.h>" >> include/asoundlib.h
cat "$srcdir"/include/asoundlib-tail.h >> include/asoundlib.h
