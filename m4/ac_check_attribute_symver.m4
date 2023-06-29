dnl Check compiler support for symver function attribute
AC_DEFUN([AC_CHECK_ATTRIBUTE_SYMVER], [
	saved_CFLAGS=$CFLAGS
	CFLAGS="-O0 -Werror"
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM(
			[[
				void _test_attribute_symver(void);
				__attribute__((__symver__("sym@VER_1.2.3"))) void _test_attribute_symver(void) {}
			]],
			[[
				_test_attribute_symver()
			]]
		)],
		[
			AC_DEFINE([HAVE_ATTRIBUTE_SYMVER], 1, [Define to 1 if __attribute__((symver)) is supported])
		],
		[
			AC_DEFINE([HAVE_ATTRIBUTE_SYMVER], 0, [Define to 0 if __attribute__((symver)) is not supported])
		]
	)
	CFLAGS=$saved_CFLAGS
])

