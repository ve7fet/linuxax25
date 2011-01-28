
if HAVE_X
X11_bin_programs = smdiag
endif

SUBDIRS = . fl

bin_PROGRAMS = sethdlc smmixer $(X11_bin_programs)

sethdlc_SOURCES = sethdlc.c hdrvcomm.c hdrvcomm.h usersmdiag.h soundmodem.h
smmixer_SOURCES = smmixer.c hdrvcomm.c hdrvcomm.h usersmdiag.h soundmodem.h
smdiag_SOURCES = smdiag.c hdrvcomm.c hdrvcomm.h usersmdiag.h soundmodem.h

smdiag_LDADD = $(X_LIBS) -lX11 $(X_EXTRA_LIBS)

INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	   -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=$(sysconfdir)/ax25/
AX25_LOCALSTATEDIR=$(localstatedir)/ax25/

man_MANS = sethdlc.8 smmixer.8 baycom.9 hdlcdrv.9 soundmodem.9 smdiag.8

EXTRA_DIST = $(man_MANS) 

