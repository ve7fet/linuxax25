AC_PREREQ(2.59)
AC_INIT(ax25-apps, 0.0.8-rc2, linux-hams@vger.kernel.org)
AC_CONFIG_SRCDIR(ax25ipd/config.c)
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_GCC_TRADITIONAL
AC_PROG_LIBTOOL

dnl Checks for libraries.
AC_SUBST(AX25_LIB)
AC_SUBST(NCURSES_LIB)
AC_CHECK_LIB(ax25, ax25_config_load_ports, AX25_LIB="-lax25", AC_MSG_ERROR(Could not find the libax25 libraries; aborting))
AC_CHECK_LIB(ncurses, initscr,NCURSES_LIB="-lncurses", AC_MSG_ERROR(Could not find the ncurses library; aborting))

dnl Checks for working glibc 2.1 headers
AC_CHECK_TYPES([struct ax25_fwd_struct], [],
	[AC_MSG_ERROR([Both glibc and libax25 are too old or both installed])],
	[[#include <netax25/ax25.h>]])
AC_CHECK_MEMBER([struct nr_route_struct.ndigis], [],
	[AC_MSG_ERROR([Both glibc and libax25 are too old or both installed])],
	[[#include <netrom/netrom.h>]])
AC_CHECK_TYPES([struct rose_facilities_struct], [],
	[AC_MSG_ERROR([Both glibc and libax25 are too old or both installed])],
	[[#include <netax25/ax25.h>     /* Or <netrose/rose.h> will blow up */
	#include <netrose/rose.h>
	]])

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/file.h sys/ioctl.h sys/time.h syslog.h termio.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(gettimeofday mktime select socket strdup strerror strspn strstr strtol strtoul uname)


dnl Only use -Wall if we have gcc
if test "x$GCC" = "xyes"; then
  if test -z "`echo "$CFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
    CFLAGS="$CFLAGS -Wall"
  fi
fi

AC_CONFIG_FILES([call/Makefile ax25ipd/Makefile listen/Makefile Makefile ax25rtd/Makefile ax25mond/Makefile ax25-apps.spec])
AC_OUTPUT

