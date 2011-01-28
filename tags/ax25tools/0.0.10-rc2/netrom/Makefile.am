
etcfiles = nrbroadcast 

installconf:
	$(mkinstalldirs) $(DESTDIR)$(AX25_SYSCONFDIR)
	@list='$(etcfiles)'; for p in $$list; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; \
	done


sbin_PROGRAMS = netromd nodesave nrattach nrparms nrsdrv

LDADD = $(AX25_LIB)

man_MANS = netrom.4 nrbroadcast.5 netromd.8 nodesave.8 \
	   nrattach.8 nrparms.8 nrsdrv.8

EXTRA_DIST = $(man_MANS) $(etcfiles)

netromd_SOURCES = \
	netromd.c \
	netromd.h \
	netromr.c \
	netromt.c 


INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	   -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=${sysconfdir}/ax25/
AX25_LOCALSTATEDIR=${localstatedir}/ax25/

