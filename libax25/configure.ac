AC_PREREQ(2.61)
AC_INIT(libax25, 1.2.2, linux-hams@vger.kernel.org)
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
LT_INIT

# Checks for libraries.
AC_FUNC_SETPGRP
AC_SUBST(Z_LIB)
AC_CHECK_LIB(z, zlibVersion,Z_LIB="-lz",AC_MSG_ERROR([Could not find the zlib libraries; aborting]))


# Checks for header files.
AC_INCLUDES_DEFAULT # Checks for sys/types.h, sys/stat.h, stdlib.h, string.h, memory.h, strings.h, inttypes.h, stdint.h and unistd.h
AC_CHECK_HEADERS([fcntl.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h termios.h limits.h],,AC_MSG_ERROR([Required headers missing; aborting]))

AC_MSG_CHECKING(for working zlib.h header file)
AC_EGREP_HEADER(zlibVersion, zlib.h, has_zlib=yes, has_zlib=no)
if test "$has_zlib" = "yes" ; then
  AC_DEFINE(HAVE_ZLIB_H, 1, [Define if you have a proper zlib.h header file])
  AC_MSG_RESULT(yes)
else
  AC_MSG_ERROR([Could not find the zlib libraries; aborting])
fi
AM_CONDITIONAL([ZLIB_H], [test "$has_zlib" = "yes"])

AC_MSG_CHECKING(for working netax25/ax25.h header file)
AC_EGREP_HEADER(ax25_fwd_struct, netax25/ax25.h, has_ax25=yes, has_ax25=no)
if test "$has_ax25" = "yes" ; then
  AC_DEFINE(HAVE_NETAX25_AX25_H, 1, [Define if you have a proper netax25/ax25.h header file])
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([AX25_H], [test "$has_ax25" = "yes"])

AC_MSG_CHECKING(for working netrom/netrom.h header file)
AC_EGREP_HEADER(unsigned int ndigis, netrom/netrom.h, has_netrom=yes, has_netrom=no)
if test "$has_netrom" = "yes" ; then
  AC_DEFINE(HAVE_NETROM_NETROM_H, 1, [Define if you have a proper netrom/netrom.h header file])
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([NETROM_H], [test "$has_netrom" = "yes"])

AC_MSG_CHECKING(for working netrose/rose.h header file)
AC_EGREP_HEADER(rose_facilities_struct, netrose/rose.h, has_rose=yes, has_rose=no)
if test "$has_rose" = "yes" ; then
  AC_DEFINE(HAVE_NETROSE_ROSE_H, 1, [Define if you have a proper netrose/rose.h header file.])
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([ROSE_H], [test "$has_rose" = "yes"])

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
