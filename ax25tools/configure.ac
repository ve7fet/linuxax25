AC_PREREQ(2.61)
AC_INIT(ax25tools, 1.1.0, linux-hams@vger.kernel.org)
AC_CONFIG_SRCDIR([pathnames.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2])

dnl Determine versions for package managers.
basever=$(echo $PACKAGE_VERSION | sed -e 's@-.*$@@')
AC_SUBST(BASEVERSION, [$basever])
extraver=$(echo $PACKAGE_VERSION | sed -e 's@'$basever'@@' -e 's@^-@_@')
AC_SUBST(EXTRAVERSION, [$extraver])

dnl Check options
AC_ARG_WITH([xutils], 
	[AS_HELP_STRING([--with-xutils], [build tools that require X support (also requires GL and fltk libraries)])])

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_SED
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_GCC_TRADITIONAL
AC_PROG_LN_S

dnl Checks for libraries.
AC_SUBST(Z_LIB)
AC_SUBST(AX25_LIB)
AC_SUBST(UTIL_LIB)
AC_SUBST(FLTK_LIB)
FOUND_FLTK=""

AC_CHECK_LIB(z, zlibVersion,Z_LIB="-lz",AC_MSG_ERROR([Could not find the zlib libraries; aborting]))

AC_CHECK_LIB(ax25, ax25_config_load_ports, AX25_LIB="-lax25", AC_MSG_ERROR([Could not find the libax25 libraries; aborting]))

AC_CHECK_LIB(util, openpty, UTIL_LIB="-lutil", AC_MSG_ERROR([Could not find the util libraries; aborting]))

if test "$with_xutils" = "yes"; then
dnl Check for X and set a conditional
AC_PATH_XTRA
AM_CONDITIONAL([HAVE_X], [test -z "$no_x"])
dnl Check for required X headers
AC_CHECK_HEADERS([X11/X.h X11/Xlib.h X11/Xutil.h],,AC_MSG_ERROR([Required headers missing; aborting]))
AC_CHECK_LIB([fltk], [fl_local_ctrl], 
	[AC_DEFINE([HAVE_FLTK],[1],["Define to 1 if you have fltk."]) FOUND_FLTK="yes"; \
	FLTK_LIB="-lfltk $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"], 
	[AC_MSG_ERROR([Could not find the fltk libraries; aborting])], 
	[$LIBGL $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS]) 
AM_CONDITIONAL([HAVE_FLTK],[test -n "$FLTK_LIB"])
dnl Check for FLTK header PRESENCE ONLY, normal check fails
AC_CHECK_HEADERS([FL/Fl.H FL/Fl_Window.H FL/Fl_Box.H FL/Fl_Value_Slider.H \
		  FL/Fl_Check_Button.H FL/Fl_Button.H FL/Fl_Output.H FL/Fl_Round_Button.H \
		  FL/Fl_Group.H FL/x.H FL/fl_draw.H X11/X.h X11/Xlib.h \
		  X11/Xutil.h],,AC_MSG_ERROR([Required headers missing; aborting]),)
else
    AC_MSG_NOTICE([*** no --with-xutils, skipping tools requiring X ***])
dnl Since we're not building the X tools, set HAVE_X and HAVE_FLTK to no
    AM_CONDITIONAL([HAVE_X], [test x$xutils = xtrue])
    AM_CONDITIONAL([HAVE_FLTK], [test x$xutils = xtrue])
fi

dnl Checks for header files.
AC_INCLUDES_DEFAULT # Checks for sys/types.h, sys/stat.h, stdlib.h, string.h, memory.h, strings.h, inttypes.h, stdint.h and unistd.h
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h paths.h netpacket/packet.h \
		  pty.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h sys/types.h \
		  syslog.h termios.h utime.h utmp.h zlib.h],,AC_MSG_ERROR([Required headers missing; aborting]))

dnl Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_STRTOD
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([alarm atexit dup2 endgrent endpwent gethostbyname gethostname gettimeofday inet_ntoa memmove memset mkdir select socket strcasecmp strchr strdup strerror strncasecmp strrchr strspn strstr strtol strtoul uname utime])

AC_CONFIG_FILES([netrom/Makefile tcpip/Makefile ax25/Makefile ax25/axgetput/Makefile Makefile rose/Makefile user_call/Makefile kiss/Makefile 6pack/Makefile hdlcutil/Makefile hdlcutil/fl/Makefile yamdrv/Makefile dmascc/Makefile doc/Makefile man/Makefile etc/Makefile ax25tools.spec])
AC_OUTPUT
