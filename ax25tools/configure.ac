AC_PREREQ(2.61)
AC_INIT(ax25-tools, 1.0.1, linux-hams@vger.kernel.org)
AC_CONFIG_SRCDIR([pathnames.h])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2])

# Determine versions for package managers.
basever=$(echo $PACKAGE_VERSION | sed -e 's@-.*$@@')
AC_SUBST(BASEVERSION, [$basever])
extraver=$(echo $PACKAGE_VERSION | sed -e 's@'$basever'@@' -e 's@^-@_@')
AC_SUBST(EXTRAVERSION, [$extraver])

# Checks for programs.
AC_PROG_AWK
AC_PROG_SED
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_GCC_TRADITIONAL
AC_PROG_LN_S

# Check for X and set a conditional
AC_PATH_XTRA
AM_CONDITIONAL(HAVE_X, test -z "$no_x")

dnl Checks for libraries.
AC_SUBST(Z_LIB)
AC_SUBST(AX25_LIB)
AC_SUBST(UTIL_LIB)
AC_SUBST(FLTK_LIB)

AC_CHECK_LIB(z, zlibVersion,Z_LIB="-lz",Z_LIB=)

AC_CHECK_LIB(ax25, ax25_config_load_ports, AX25_LIB="-lax25", AC_MSG_ERROR([Could not find the libax25 libraries; aborting]))

AC_CHECK_LIB(util, openpty, UTIL_LIB="-lutil", AC_MSG_ERROR([Could not find the util libraries; aborting]))

AC_CHECK_LIB(GL, main, GL_LIB="-lGL",
AC_CHECK_LIB(MesaGL, main, GL_LIB="-lMesaGL", GL_LIB=, ${X_LIBS} ${X_PRE_LIBS} -lXi -lXext -lX11 ${X_EXTRA_LIBS}),${X_LIBS} ${X_PRE_LIBS} -lXi -lXext -lX11 ${X_EXTRA_LIBS})
if test -n "$GL_LIB"; then
  AC_CHECK_LIB(fltk, main, FLTK_LIB="-lfltk $GL_LIB $X_PRE_LIBS -lXi -lXext -lX11 $X_EXTRA_LIBS", FLTK_LIB=, ${GL_LIB} ${X_LIBS} ${X_PRE_LIBS} -lXi -lXext -lX11 ${X_EXTRA_LIBS})
fi
AM_CONDITIONAL(HAVE_FLTK, test -n "$FLTK_LIB" )


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h paths.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h syslog.h termios.h unistd.h utime.h utmp.h zlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRTOD
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([alarm atexit dup2 endgrent endpwent gethostbyname gethostname gettimeofday inet_ntoa memmove memset mkdir select socket strcasecmp strchr strdup strerror strncasecmp strrchr strspn strstr strtol strtoul uname utime])

AC_CONFIG_FILES([netrom/Makefile tcpip/Makefile ax25/Makefile ax25/axgetput/Makefile Makefile rose/Makefile user_call/Makefile kiss/Makefile 6pack/Makefile hdlcutil/Makefile hdlcutil/fl/Makefile yamdrv/Makefile dmascc/Makefile doc/Makefile man/Makefile etc/Makefile ax25-tools.spec])
AC_OUTPUT
