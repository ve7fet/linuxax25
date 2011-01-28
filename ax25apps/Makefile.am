SUBDIRS = ax25ipd ax25rtd call listen ax25mond

EXTRA_DIST = pathnames.h ax25-apps.spec

installconf:
	@for app in $(SUBDIRS); do $(MAKE) -C $$app installconf; done

AX25_SYSCONFDIR=$(sysconfdir)/ax25
AX25_LOCALSTATEDIR=$(localstatedir)/ax25

INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	   -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 


