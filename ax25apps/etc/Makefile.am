BUILT_SOURCES = do_subst

AX25_SYSCONFDIR = $(sysconfdir)/ax25
AX25_LOCALSTATEDIR = $(localstatedir)/ax25

etcfiles = ax25ipd.conf ax25mond.conf ax25rtd.conf

vardir = $(AX25_LOCALSTATEDIR)/ax25rtd
var_DATA = ax25_route ip_route

install-data-hook:
	##Make sure that the /etc/ax25 folder exists
	$(mkinstalldirs) $(DESTDIR)$(AX25_SYSCONFDIR)
	##On a new install, install the config files with their default names. 
	##On an upgeade, if we find files with the same name, install these ones 
	##with a .dist suffix, so we don't overwrite any user changed files.
	@for p in $(etcfiles); do \
	  if test -f $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; then \
	  echo " $(DESTDIR)$(AX25_SYSCONFDIR)/$$p already exists; installing new file as $$p.dist"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p.dist; \
	  else \
	  echo " Existing $$p not found; installing default $$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; \
	  fi; \
	done

uninstall-hook:
	$(RM) $(DESTDIR)$(AX25_SYSCONFDIR)/ax25ipd.conf
	$(RM) $(DESTDIR)$(AX25_SYSCONFDIR)/ax25mond.conf
	$(RM) $(DESTDIR)$(AX25_SYSCONFDIR)/ax25rtd.conf
	$(RM) $(DESTDIR)$(AX25_SYSCONFDIR)/ax25ipd.conf.dist
	$(RM) $(DESTDIR)$(AX25_SYSCONFDIR)/ax25mond.conf.dist
	$(RM) $(DESTDIR)$(AX25_SYSCONFDIR)/ax25rtd.conf.dist

#This will replace variables in the .in files when make processes them
edit = sed \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@datadir[@]|$(datadir)|g'

do_subst:
	@list='$(etcfiles)'; for p in $$list; do \
	$(edit) $(srcdir)/$$p.in >$$p; \
	done

CLEANFILES = $(etcfiles)

EXTRA_DIST = ax25ipd.conf.in ax25mond.conf.in ax25rtd.conf.in ax25_route ip_route
