
installconf:

bin_PROGRAMS = listen

man_MANS = listen.1

EXTRA_DIST = $(man_MANS) 

LDADD = $(AX25_LIB) $(NCURSES_LIB)

listen_SOURCES = 	\
	listen.c	\
	listen.h	\
	kissdump.c	\
	ax25dump.c	\
	nrdump.c	\
	arpdump.c	\
	ipdump.c	\
	icmpdump.c	\
	udpdump.c	\
	tcpdump.c	\
	rspfdump.c	\
	ripdump.c	\
	rosedump.c	\
	flexnetdump.c	\
	opentracdump.c \
	utils.c
