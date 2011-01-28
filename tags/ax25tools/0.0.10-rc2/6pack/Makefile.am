
installconf:

sbin_PROGRAMS = m6pack

LDADD= $(AX25_LIB)

man_MANS = m6pack.8

EXTRA_DIST = $(man_MANS)

INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	   -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=${sysconfdir}/ax25/
AX25_LOCALSTATEDIR=${localstatedir}/ax25/

install-exec-hook:
	(cd $(DESTDIR)$(sbindir))

