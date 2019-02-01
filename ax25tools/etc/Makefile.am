BUILT_SOURCES = do_subst

AX25_SYSCONFDIR = $(sysconfdir)/ax25
AX25_LOCALSTATEDIR = $(localstatedir)/ax25

etcfiles = \
	ax25d.conf \
	axspawn.conf\
	nrbroadcast\
	rip98d.conf\
	rxecho.conf\
	ax25.profile\
	axports\
	nrports\
	rsports\
	ttylinkd.conf

vardir = $(AX25_LOCALSTATEDIR)/mheard
var_DATA = mheard.dat

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
	##Fix permissions on mheard
	chmod a+w $(DESTDIR)$(AX25_LOCALSTATEDIR)/mheard/mheard.dat

uninstall-hook:
	@for p in $(etcfiles); do \
	 $(RM) $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; \
	 $(RM) $(DESTDIR)$(AX25_SYSCONFDIR)/$$p.dist; \
	done

edit = sed \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@prefix[@]|$(prefix)|g'

do_subst:
	@list='$(etcfiles)'; for p in $$list; do \
	$(edit) $(srcdir)/$$p.in >$$p; \
	done

CLEANFILES = $(etcfiles)

EXTRA_DIST = \
	axports.in \
	nrbroadcast.in \
	rsports.in \
	ttylinkd.conf.in \
	ax25d.conf.in \
	axspawn.conf.in \
	nrports.in \
	rxecho.conf.in \
	rip98d.conf.in \
	ax25.profile.in \
	mheard.dat

