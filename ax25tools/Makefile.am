SUBDIRS = ax25 hdlcutil kiss 6pack netrom rose tcpip user_call yamdrv dmascc man doc etc

EXTRA_DIST = pathnames.h ax25tools.spec SVNREV

AM_CPPFLAGS = -D_GNU_SOURCE \
	      -DAX25_SYSCONFDIR=\""$(sysconfdir)/ax25/"\" \
	      -DAX25_LOCALSTATEDIR=\""$(localstatedir)/ax25/"\" 

AX25_SYSCONFDIR=$(sysconfdir)/ax25/
AX25_LOCALSTATEDIR=$(localstatedir)/ax25/
