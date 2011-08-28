SUBDIRS = axgetput 

sbin_PROGRAMS = ax25d axctl axparms axspawn beacon bpqparms mheardd rxecho

bin_PROGRAMS = mheard

LDADD = $(AX25_LIB)
axspawn_LDADD = $(AX25_LIB) $(UTIL_LIB)

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	      -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=$(sysconfdir)/ax25/
AX25_LOCALSTATEDIR=$(localstatedir)/ax25/

ax25d_SOURCES = ax25d.c
axctl_SOURCES = axctl.c
axparms_SOURCES = axparms.c
axspawn_SOURCES = axspawn.c axspawn.h access.c access.h md5.c md5.h
beacon_SOURCES = beacon.c
bpqparms_SOURCES = bpqparms.c
mheard_SOURCES = mheard.c
mheardd_SOURCES = mheardd.c
rxecho_SOURCES = rxecho.c
