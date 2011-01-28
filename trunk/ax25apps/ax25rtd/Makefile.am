sbin_PROGRAMS = ax25rtd ax25rtctl

ax25rtd_SOURCES = \
	ax25rtd.c \
	ax25rtd.h \
	cache_ctl.c \
	cache_dump.c \
	config.c \
	listener.c

ax25rtd_LDADD = $(AX25_LIB)

ax25rtctl_SOURCES = ax25rtctl.c

AX25_SYSCONFDIR=$(sysconfdir)/ax25
AX25_LOCALSTATEDIR=$(localstatedir)/ax25

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	      -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\"

