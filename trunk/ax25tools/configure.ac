AC_PREREQ(2.59)
AC_INIT(ax25-tools, 0.0.10-rc2, linux-hams@vger.kernel.org)
AC_CONFIG_SRCDIR(ax25/ax25d.c)
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

dnl Check for X and set a conditional
AC_PATH_XTRA
AM_CONDITIONAL(HAVE_X, test -z "$no_x")

dnl Checks for libraries.
AC_SUBST(Z_LIB)
AC_SUBST(AX25_LIB)
AC_SUBST(AX25IO_LIB)
AC_SUBST(NCURSES_LIB)
AC_SUBST(UTIL_LIB)
AC_SUBST(FLTK_LIB)
AC_CHECK_LIB(z, zlibVersion,Z_LIB="-lz",Z_LIB=)
AC_CHECK_LIB(ax25, ax25_config_load_ports, AX25_LIB="-lax25", AC_MSG_ERROR([Could not find the libax25 libraries; aborting]))
AC_CHECK_LIB(ncurses, initscr,NCURSES_LIB="-lncurses",NCURSES_LIB=)
AC_CHECK_LIB(util, openpty, UTIL_LIB="-lutil",UTIL_LIB=)
AC_CHECK_LIB(GL, main, GL_LIB="-lGL",
  AC_CHECK_LIB(MesaGL, main, GL_LIB="-lMesaGL", GL_LIB=, ${X_LIBS} ${X_PRE_LIBS} -lXi -lXext -lX11 ${X_EXTRA_LIBS}),${X_LIBS} ${X_PRE_LIBS} -lXi -lXext -lX11 ${X_EXTRA_LIBS})
if test -n "$GL_LIB"; then
  AC_CHECK_LIB(fltk, main, FLTK_LIB="-lfltk $GL_LIB $X_PRE_LIBS -lXi -lXext -lX11 $X_EXTRA_LIBS", FLTK_LIB=, ${GL_LIB} ${X_LIBS} ${X_PRE_LIBS} -lXi -lXext -lX11 ${X_EXTRA_LIBS})
fi
AM_CONDITIONAL(HAVE_FLTK, test -n "$FLTK_LIB" )


dnl Checks for header files.
dnl AC_PATH_X
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h limits.h paths.h strings.h sys/file.h sys/ioctl.h sys/time.h syslog.h unistd.h zlib.h)

dnl Checks for working glibc 2.1 headers
AC_CHECK_TYPES([struct ax25_fwd_struct], [],
	[AC_MSG_ERROR([Both glibc and libax25 are too old or both installed])],
	[[#include <netax25/ax25.h>]])
AC_CHECK_MEMBER([struct nr_route_struct.ndigis], [],
	[AC_MSG_ERROR([Both glibc and libax25 are too old or both installed])],
	[[#include <netrom/netrom.h>]])
AC_CHECK_TYPES([struct rose_facilities_struct], [],
	[AC_MSG_ERROR([Both glibc and libax25 are too old or both installed])],
	[[#include <netax25/ax25.h>	/* Or <netrose/rose.h> will blow up */
	#include <netrose/rose.h>
	]])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_FUNC_SETPGRP
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_FUNC_WAIT3
AC_CHECK_FUNCS(gethostname gettimeofday mkdir select socket strdup strerror strspn strstr strtol strtoul uname)

dnl Only use -Wall if we have gcc
if test "x$GCC" = "xyes"; then
  if test -z "`echo "$CFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
    CFLAGS="$CFLAGS -Wall"
  fi
fi

basever=$(echo $PACKAGE_VERSION | sed -e 's@-.*$@@')
AC_SUBST(BASEVERSION, [$basever])
extraver=$(echo $PACKAGE_VERSION | sed -e 's@@<:@^-@:>@*-@@' -e 's@-@_@')
AC_SUBST(EXTRAVERSION, [$extraver])

AC_CONFIG_FILES([netrom/Makefile tcpip/Makefile ax25/Makefile ax25/axgetput/Makefile Makefile rose/Makefile user_call/Makefile kiss/Makefile 6pack/Makefile hdlcutil/Makefile hdlcutil/fl/Makefile yamdrv/Makefile dmascc/Makefile ax25-tools.spec])
AC_OUTPUT
