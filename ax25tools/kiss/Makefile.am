
installconf:

sbin_PROGRAMS = kissattach kissnetd kissparms mkiss net2kiss

LDADD= $(AX25_LIB)

man_MANS = kissattach.8 spattach.8 kissnetd.8 kissparms.8 mkiss.8 net2kiss.8

EXTRA_DIST = $(man_MANS)

INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	   -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=${sysconfdir}/ax25/
AX25_LOCALSTATEDIR=${localstatedir}/ax25/

install-exec-hook:
	(cd $(DESTDIR)$(sbindir) ; ln -sf kissattach spattach)

