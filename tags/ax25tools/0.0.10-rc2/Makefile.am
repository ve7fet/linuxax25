
installconf:
	@for app in $(SUBDIRS); do $(MAKE) -C $$app installconf; done

SUBDIRS = ax25 hdlcutil kiss 6pack netrom rose tcpip user_call yamdrv dmascc

EXTRA_DIST = pathnames.h ax25-tools.spec

INCLUDES = -DAX25_SYSCONFDIR=\""$(sysconfdir)/ax25/"\" \
	   -DAX25_LOCALSTATEDIR=\""$(localstatedir)/ax25/"\" 

AX25_SYSCONFDIR=@sysconfdir@/ax25/
AX25_LOCALSTATEDIR=@localstatedir@/ax25/
