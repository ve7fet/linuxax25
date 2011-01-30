lib_LTLIBRARIES = libax25io.la

libax25io_la_SOURCES = ax25io.c

libax25io_la_LIBADD = $(Z_LIB)

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(sysconfdir)/ax25/"\" \
	      -DAX25_LOCALSTATEDIR=\""$(localstatedir)/ax25/"\" \
	      -I$(top_srcdir) \
	      -I$(top_srcdir)/netax25

AX25_SYSCONFDIR=@sysconfdir@/ax25/
AX25_LOCALSTATEDIR=@localstatedir@/ax25/

