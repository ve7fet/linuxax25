AC_PREREQ(2.61)
AC_INIT(libax25, 1.0.3, linux-hams@vger.kernel.org)
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2])
AC_CONFIG_SRCDIR([pathnames.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
m4_pattern_allow([AM_PROG_AR])
AM_PROG_AR

# Determine versions for package managers.
basever=$(echo $PACKAGE_VERSION | sed -e 's@-.*$@@')
AC_SUBST(BASEVERSION, [$basever])
extraver=$(echo $PACKAGE_VERSION | sed -e 's@'$basever'@@' -e 's@^-@_@')
AC_SUBST(EXTRAVERSION, [$extraver])

# Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_LIBTOOL

# Checks for libraries.
AC_FUNC_SETPGRP
AC_TYPE_SIGNAL
AC_SUBST(Z_LIB)
AC_CHECK_LIB(z, zlibVersion,Z_LIB="-lz",Z_LIB=)


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h termios.h unistd.h zlib.h])

AC_MSG_CHECKING(for working zlib.h header file)
AC_EGREP_HEADER(zlibVersion, zlib.h, has_zlib=yes, has_zlib=no)
if test "$has_zlib" = "yes" ; then
  AC_DEFINE(HAVE_ZLIB_H, 1, [Define if you have a proper zlib.h header file])
  AC_MSG_RESULT(yes)
else
  AC_MSG_ERROR([Could not find the zlib libraries; aborting])
fi
AM_CONDITIONAL([ZLIB_H], [test "$has_zlib" = "yes"])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([memmove memset socket strcasecmp strchr strcspn strdup strerror strrchr strspn strstr strtoul])

AC_CONFIG_FILES([Makefile man/Makefile lib/Makefile lib/ax25/Makefile lib/ax25io/Makefile netax25/Makefile netrose/Makefile netrom/Makefile libax25.spec])

AC_OUTPUT
