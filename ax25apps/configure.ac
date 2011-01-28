AC_PREREQ(2.61)
AC_INIT(ax25-apps, 1.0.0, linux-hams@vger.kernel.org)
AC_CONFIG_SRCDIR([pathnames.h])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2])

# Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_INSTALL

# Checks for libraries.
AC_SUBST(AX25_LIB)
AC_SUBST(NCURSES_LIB)
AC_CHECK_LIB(ax25, ax25_config_load_ports, AX25_LIB="-lax25", AC_MSG_ERROR(Could not find the libax25 libraries; aborting))
AC_CHECK_LIB(ncurses, initscr, NCURSES_LIB="-lncurses", AC_MSG_ERROR(Could not find the ncurses library; aborting))

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h memory.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h termio.h termios.h unistd.h utime.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MKTIME
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([bzero gethostbyname inet_ntoa memchr memmove memset select socket strcasecmp strchr strdup strerror strrchr strtol strtoul utime])

# Parse out some variables for creating rpm packages
basever=$(echo $PACKAGE_VERSION | sed -e 's@-.*$@@')
AC_SUBST(BASEVERSION, [$basever])
extraver=$(echo $PACKAGE_VERSION | sed -e 's@@<:@^-@:>@*-@@' -e 's@-@_@')
AC_SUBST(EXTRAVERSION, [$extraver])

AC_CONFIG_FILES([call/Makefile ax25ipd/Makefile listen/Makefile Makefile ax25rtd/Makefile ax25mond/Makefile doc/Makefile man/Makefile etc/Makefile ax25-apps.spec])

AC_OUTPUT
