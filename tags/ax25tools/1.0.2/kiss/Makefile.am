sbin_PROGRAMS = kissattach kissnetd kissparms mkiss net2kiss

LDADD= $(AX25_LIB)

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	      -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=${sysconfdir}/ax25/
AX25_LOCALSTATEDIR=${localstatedir}/ax25/

install-exec-hook:
	(cd $(DESTDIR)$(sbindir) ; ln -sf kissattach spattach)

