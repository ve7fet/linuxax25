bin_PROGRAMS = call

call_SOURCES = \
	call.c \
	call.h \
	menu.c \
	menu.h \
	crc.c \
	crc.h \
	yapp.c \
	dostime.c

call_LDADD = $(NCURSES_LIB) $(AX25_LIB)

AX25_SYSCONFDIR=$(sysconfdir)/ax25
AX25_LOCALSTATEDIR=$(localstatedir)/ax25

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	      -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\"
