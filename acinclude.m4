dnl Check for ALSA driver package.
AC_DEFUN(ALSA_CHECK_DRIVER, [
AC_MSG_CHECKING(for alsa-driver package)

AC_TRY_COMPILE([
#include <sound/asound.h>
],[
void main(void)
{
#if !defined(SNDRV_PROTOCOL_VERSION) || !defined(SNDRV_PROTOCOL_INCOMPATIBLE)
#error not found
#else
#if !defined(SNDRV_PCM_IOCTL_REWIND)
#error wrong version
#endif
  exit(0);
#endif
}
],
  AC_MSG_RESULT(present),
  [AC_MSG_RESULT(not found or wrong version);
   AC_MSG_ERROR([Install alsa-driver v0.9.0 package first...])]
)
])

AC_DEFUN(SAVE_LIBRARY_VERSION, [
SND_LIB_VERSION=$VERSION
echo $VERSION > $srcdir/version
AC_DEFINE_UNQUOTED(VERSION, "$SND_LIB_VERSION")
AC_SUBST(SND_LIB_VERSION)
SND_LIB_MAJOR=`echo $VERSION | cut -d . -f 1`
AC_SUBST(SND_LIB_MAJOR)
SND_LIB_MINOR=`echo $VERSION | cut -d . -f 2`
AC_SUBST(SND_LIB_MINOR)
SND_LIB_SUBMINOR=`echo $VERSION | cut -d . -f 3 | sed -e 's/pre[[0-9]]*//g' -e 's/[[[:alpha:]]]*//g'`
AC_SUBST(SND_LIB_SUBMINOR)
])
