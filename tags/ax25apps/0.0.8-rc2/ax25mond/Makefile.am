
sbin_PROGRAMS = ax25mond

man_MANS = ax25mond.8 ax25mond.conf.5

EXTRA_DIST = $(man_MANS) $(etcfiles)

ax25mond_LDADD = $(AX25_LIB)

ax25mond_SOURCES =	\
	ax25mond.c

# Needed so that install is optional
etcfiles = ax25mond.conf
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

