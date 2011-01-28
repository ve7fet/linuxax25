lib_LTLIBRARIES = libax25.la libax25io.la

libax25includedir = $(includedir)
libax25ioincludedir = $(includedir)

INCLUDES = -DAX25_SYSCONFDIR=\""$(sysconfdir)/ax25/"\" \
	   -DAX25_LOCALSTATEDIR=\""$(localstatedir)/ax25/"\" 

if !AX25_H
USE_LIBAX25_AX25_H = netax25/ax25.h
endif
if !NETROM_H
USE_LIBAX25_NETROM_H = netrom/netrom.h
endif
if !ROSE_H
USE_LIBAX25_ROSE_H = netrose/rose.h
endif

nobase_libax25include_HEADERS = 	\
	netax25/axlib.h			\
	netax25/axconfig.h		\
	netax25/nrconfig.h		\
	netax25/rsconfig.h		\
	netax25/daemon.h		\
	netax25/ttyutils.h		\
	netax25/procutils.h		\
	netax25/mheard.h		\
	$(USE_LIBAX25_AX25_H)		\
	$(USE_LIBAX25_NETROM_H)		\
	$(USE_LIBAX25_ROSE_H)

nobase_libax25ioinclude_HEADERS = 	\
	netax25/ax25io.h

libax25_la_SOURCES = \
	axconfig.c \
	netax25/axconfig.h \
	netax25/axlib.h \
	axutils.c \
	config.h \
	daemon.c \
	netax25/daemon.h \
	netax25/mheard.h \
	nrconfig.c \
	netax25/nrconfig.h \
	pathnames.h \
	procutils.c \
	netax25/procutils.h \
	rsconfig.c \
	netax25/rsconfig.h \
	ttyutils.c \
	netax25/ttyutils.h  \
	$(USE_LIBAX25_AX25_H) \
	$(USE_LIBAX25_ROSE_H)
	
libax25io_la_SOURCES = \
	ax25io.c \
	netax25/ax25io.h 

man_MANS = ax25.3 rose.3 axports.5 rsports.5 nrports.5


EXTRA_DIST= $(man_MANS) glibc.patch $(etcfiles) libax25.spec

AX25_SYSCONFDIR=@sysconfdir@/ax25/
AX25_LOCALSTATEDIR=@localstatedir@/ax25/

# Needed so that the install is optional
etcdir = $(AX25_SYSCONFDIR)
etcfiles =  axports rsports nrports
installconf:
	$(mkinstalldirs) $(DESTDIR)$(etcdir)
	@list='$(etcfiles)'; for p in $$list; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(etcdir)/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(etcdir)/$$p; \
	done

