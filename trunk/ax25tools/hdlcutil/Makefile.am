if HAVE_X
X11_bin_programs = smdiag
endif

SUBDIRS = fl

bin_PROGRAMS = sethdlc smmixer $(X11_bin_programs)

sethdlc_SOURCES = sethdlc.c hdrvcomm.c hdrvcomm.h usersmdiag.h soundmodem.h
smmixer_SOURCES = smmixer.c hdrvcomm.c hdrvcomm.h usersmdiag.h soundmodem.h
smdiag_SOURCES = smdiag.c hdrvcomm.c hdrvcomm.h usersmdiag.h soundmodem.h

smdiag_LDADD = $(X_LIBS) -lX11 $(X_EXTRA_LIBS)

