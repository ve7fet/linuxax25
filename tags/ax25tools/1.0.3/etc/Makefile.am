noinst_SCRIPTS = do_subst 

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

edit = sed \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@prefix[@]|$(prefix)|g'

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

install-data-hook: 
	chmod a+w $(DESTDIR)$(AX25_LOCALSTATEDIR)/mheard/mheard.dat

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

AX25_SYSCONFDIR = $(sysconfdir)/ax25
AX25_LOCALSTATEDIR = $(localstatedir)/ax25
