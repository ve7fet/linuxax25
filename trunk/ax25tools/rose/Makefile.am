sbin_PROGRAMS = rsattach rsdwnlnk rsmemsiz rsparms rsuplnk 

rsattach_SOURCES = rsattach.c
rsdwnlnk_SOURCES = rsdwnlnk.c
rsmemsiz_SOURCES = rsmemsiz.c
rsparms_SOURCES = rsparms.c
rsuplnk_SOURCES = rsuplnk.c

LDADD = $(AX25_LIB)

rsmemsiz_LDADD = 

sbin_SCRIPTS = rsusers.sh

EXTRA_DIST = $(sbin_SCRIPTS)

AM_CPPFLAGS = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	      -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=${sysconfdir}/ax25/
AX25_LOCALSTATEDIR=${localstatedir}/ax25/
