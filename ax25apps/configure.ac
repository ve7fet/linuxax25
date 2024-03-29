AC_PREREQ(2.61)
AC_INIT(ax25apps, 2.1.0, linux-hams@vger.kernel.org)
AC_CONFIG_SRCDIR([pathnames.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2])

# Determine versions for package managers.
basever=$(echo $PACKAGE_VERSION | sed -e 's@-.*$@@')
AC_SUBST(BASEVERSION, [$basever])
extraver=$(echo $PACKAGE_VERSION | sed -e 's@'$basever'@@' -e 's@^-@_@')
AC_SUBST(EXTRAVERSION, [$extraver])

# Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_INSTALL

# Checks for libraries.
AC_SUBST(AX25_LIB)
AC_SUBST(NCURSES_LIB)
AC_CHECK_LIB(ax25, ax25_config_load_ports, AX25_LIB="-lax25", AC_MSG_ERROR(Could not find the libax25 libraries; aborting))
AC_CHECK_LIB(ncursesw, initscr, NCURSES_LIB="-lncursesw", AC_MSG_ERROR(Could not find the ncursesw library; aborting))

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_INCLUDES_DEFAULT # Checks for sys/types.h, sys/stat.h, stdlib.h, string.h, memory.h, strings.h, inttypes.h, stdint.h and unistd.h
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h memory.h netdb.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h syslog.h termio.h termios.h utime.h ncursesw/curses.h],,AC_MSG_ERROR([Required headers missing; aborting]))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MKTIME
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([bzero gethostbyname inet_ntoa memchr memmove memset select socket strcasecmp strchr strdup strerror strrchr strtol strtoul utime])

AC_CONFIG_FILES([call/Makefile ax25ipd/Makefile listen/Makefile Makefile ax25rtd/Makefile ax25mond/Makefile doc/Makefile man/Makefile etc/Makefile ax25apps.spec])

AC_OUTPUT
