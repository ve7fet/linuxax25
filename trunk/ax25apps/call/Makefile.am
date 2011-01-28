
vardir = $(sysconfdir)/ax25

installconf:
	$(mkinstalldirs) $(DESTDIR)$(etcdir)


bin_PROGRAMS = call

man_MANS = call.1

EXTRA_DIST = $(man_MANS)

call_LDADD = $(NCURSES_LIB) $(AX25_LIB)

call_SOURCES =		\
	call.c		\
	call.h		\
	menu.c		\
	menu.h		\
	crc.c		\
	crc.h		\
	yapp.c		\
	dostime.c

