lib_LTLIBRARIES = libax25.la

libax25_la_SOURCES = \
	axconfig.c \
	axutils.c \
	daemon.c \
	nrconfig.c \
	rsconfig.c \
	procutils.c \
	ttyutils.c 

libax25_la_LDFLAGS = -version-info 1:0:0

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(sysconfdir)/ax25/"\" \
	      -DAX25_LOCALSTATEDIR=\""$(localstatedir)/ax25/"\" \
	      -I$(top_srcdir) \
	      -I$(top_srcdir)/netax25 \
	      -I$(top_srcdir)/netrose \
	      -I$(top_srcdir)/netrom 

AX25_SYSCONFDIR=@sysconfdir@/ax25/
AX25_LOCALSTATEDIR=@localstatedir@/ax25/

install-exec-hook:
	@echo
	@echo " *** Remember to run ldconfig as root, or reboot, if applicable to your platform."
	@echo

