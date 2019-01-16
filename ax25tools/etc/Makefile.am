noinst_SCRIPTS = do_subst

docdir = $(datadir)/doc/ax25tools/conf
doc_DATA = \
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
	@list='$(doc_DATA)'; for p in $$list; do \
	$(edit) $(srcdir)/$$p.in >$$p; \
	done

install-data-hook:
	chmod a+w $(DESTDIR)$(AX25_LOCALSTATEDIR)/mheard/mheard.dat

CLEANFILES = $(doc_DATA)

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
