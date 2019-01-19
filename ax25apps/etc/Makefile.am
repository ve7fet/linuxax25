BUILT_SOURCES = do_subst

docdir = $(datadir)/doc/ax25apps/conf
doc_DATA = ax25ipd.conf ax25mond.conf ax25rtd.conf

vardir = $(AX25_LOCALSTATEDIR)/ax25rtd
var_DATA = ax25_route ip_route

edit = sed \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@datadir[@]|$(datadir)|g'

do_subst:
	@list='$(doc_DATA)'; for p in $$list; do \
	$(edit) $(srcdir)/$$p.in >$$p; \
	done

CLEANFILES = $(doc_DATA)

EXTRA_DIST = ax25ipd.conf.in ax25mond.conf.in ax25rtd.conf.in ax25_route ip_route

AX25_SYSCONFDIR = $(sysconfdir)/ax25
AX25_LOCALSTATEDIR = $(localstatedir)/ax25
