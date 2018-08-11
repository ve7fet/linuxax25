docdir = $(datadir)/doc/ax25tools/conf
doc_DATA = \
	ax25d.conf.dist \
	axspawn.conf.dist\
	nrbroadcast.dist\
	rip98d.conf.dist\
	rxecho.conf.dist\
	ax25.profile.dist\
	axports.dist\
	nrports.dist\
	rsports.dist\
	ttylinkd.conf.dist

vardir = $(AX25_LOCALSTATEDIR)/mheard
var_DATA = mheard.dat

install-data-hook: 
	chmod a+w $(DESTDIR)$(AX25_LOCALSTATEDIR)/mheard/mheard.dat

EXTRA_DIST = $(doc_DATA) mheard.dat

AX25_SYSCONFDIR = $(sysconfdir)/ax25
AX25_LOCALSTATEDIR = $(localstatedir)/ax25
