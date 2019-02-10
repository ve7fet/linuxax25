
if HAVE_FLTK
sbin_PROGRAMS = xfsmmixer xfhdlcchpar xfhdlcst xfsmdiag
endif

AM_CPPFLAGS = -D_GNU_SOURCE $(X_CFLAGS) -I$(srcdir)/..
LDADD = ../hdrvcomm.o $(X_LIBS) -lX11 $(FLTK_LIB)

xfsmmixer_SOURCES = xfsmmixer.cxx xfsmmixer_main.cxx xfsmmixer.h
xfhdlcchpar_SOURCES = xfhdlcchpar.cxx xfhdlcchpar_main.cxx xfhdlcchpar.h
xfhdlcst_SOURCES = xfhdlcst.cxx xfhdlcst_main.cxx xfhdlcst.h
xfsmdiag_SOURCES = xfsmdiag.cxx xfsmdiag_main.cxx xfsmdiag.h xfsmdiag_main.h

hdrvcomm.o: ../hdrvcomm.o
