installconf:
	(cd $(top_srcdir)/etc && $(MAKE) installconf)

install-data-hook:
	@echo
	@echo " *** Remember to run \`make installconf' if this is a new installation."
	@echo " *** This will install default configuration files (overwriting existing ones)."
	@echo

SUBDIRS = ax25ipd ax25rtd call listen ax25mond man doc etc

EXTRA_DIST = pathnames.h ax25apps.spec

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(sysconfdir)/ax25/"\" \
	      -DAX25_LOCALSTATEDIR=\""$(localstatedir)/ax25/"\"

AX25_SYSCONFDIR=$(sysconfdir)/ax25/
AX25_LOCALSTATEDIR=$(localstatedir)/ax25/
