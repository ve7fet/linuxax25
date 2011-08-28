noinst_SCRIPTS = do_subst

etcfiles = ax25ipd.conf ax25mond.conf ax25rtd.conf

vardir = $(AX25_LOCALSTATEDIR)/ax25rtd
var_DATA = ax25_route ip_route

edit = sed \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@datadir[@]|$(datadir)|g'

do_subst:
	@list='$(etcfiles)'; for p in $$list; do \
	$(edit) $(srcdir)/$$p.in >$$p; \
	done

installconf:
	$(mkinstalldirs) $(DESTDIR)$(AX25_SYSCONFDIR)
	@list='$(etcfiles)'; for p in $$list; do \
	 echo " $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p"; \
	 $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; \
	done

CLEANFILES = $(etcfiles)

EXTRA_DIST = ax25ipd.conf.in ax25mond.conf.in ax25rtd.conf.in ax25_route ip_route

AX25_SYSCONFDIR = $(sysconfdir)/ax25
AX25_LOCALSTATEDIR = $(localstatedir)/ax25
