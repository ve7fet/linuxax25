sbin_PROGRAMS = netromd nodesave nrattach nrparms nrsdrv

LDADD = $(AX25_LIB)

netromd_SOURCES = \
	netromd.c \
	netromd.h \
	netromr.c \
	netromt.c 

nodesave_SOURCES = nodesave.c
nrattach_SOURCES = nrattach.c
nrparms_SOURCES = nrparms.c
nrsdrv_SOURCES = nrsdrv.c

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	      -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=${sysconfdir}/ax25/
AX25_LOCALSTATEDIR=${localstatedir}/ax25/

