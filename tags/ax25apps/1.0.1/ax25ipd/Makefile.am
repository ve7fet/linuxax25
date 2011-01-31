sbin_PROGRAMS = ax25ipd

ax25ipd_SOURCES = \
	config.c        \
	crc.c           \
	io.c            \
	kiss.c          \
	ax25ipd.c       \
	ax25ipd.h       \
	process.c       \
	routing.c       \
	bpqether.c

ax25ipd_LDADD = $(AX25_LIB)

AX25_SYSCONFDIR=$(sysconfdir)/ax25
AX25_LOCALSTATEDIR=$(localstatedir)/ax25

AM_CFLAGS = -DUSE_TERMIOS -pthread

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	      -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

