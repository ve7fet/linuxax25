
etcfiles = ax25rtd.conf
varfiles = ax25_route ip_route

man_MANS = ax25rtd.8 ax25rtctl.8 ax25rtd.conf.5

installconf:
	$(mkinstalldirs) $(DESTDIR)$(AX25_SYSCONFDIR)
	@list='$(etcfiles)'; for p in $$list; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; \
	done
	$(mkinstalldirs) $(DESTDIR)$(AX25_LOCALSTATEDIR)/ax25rtd
	@list='$(varfiles)'; for p in $$list; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_LOCALSTATEDIR)/ax25rtd/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_LOCALSTATEDIR)/ax25rtd/$$p; \
	done

sbin_PROGRAMS = ax25rtd ax25rtctl

LDADD = $(AX25_LIB)

EXTRA_DIST = $(etcfiles) $(man_MANS) $(doc_DATA)

docdir=${datadir}/doc/ax25-apps
doc_DATA= TODO.ax25rtd README.ax25rtd

ax25rtd_SOURCES =	\
	ax25rtd.c	\
	ax25rtd.h	\
	cache_ctl.c	\
	cache_dump.c	\
	config.c	\
	listener.c

AX25_SYSCONFDIR=$(sysconfdir)/ax25
AX25_LOCALSTATEDIR=$(localstatedir)/ax25

INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	   -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 
