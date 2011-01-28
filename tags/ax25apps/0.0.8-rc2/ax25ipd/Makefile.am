
sbin_PROGRAMS = ax25ipd

man_MANS = ax25ipd.8 ax25ipd.conf.5

EXTRA_DIST = $(man_MANS) $(etcfiles) $(doc_DATA)

docdir = ${datadir}/doc/ax25-apps
doc_DATA = README.ax25ipd HISTORY.ax25ipd COPYING.ax25ipd

AM_CFLAGS = -DUSE_TERMIO
ax25ipd_LDADD = $(AX25_LIB)

ax25ipd_SOURCES =	\
	config.c	\
	crc.c		\
	io.c		\
	kiss.c		\
	ax25ipd.c	\
	ax25ipd.h	\
	process.c	\
	routing.c	\
	bpqether.c

# Needed so that install is optional
etcfiles = ax25ipd.conf
installconf:
	$(mkinstalldirs) $(DESTDIR)$(AX25_SYSCONFDIR)
	@list='$(etcfiles)'; for p in $$list; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; \
	done

AX25_SYSCONFDIR=$(sysconfdir)/ax25
AX25_LOCALSTATEDIR=$(localstatedir)/ax25

INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	   -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

