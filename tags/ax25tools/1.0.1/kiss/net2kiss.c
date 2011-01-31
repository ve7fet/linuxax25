/*****************************************************************************/

/*
 *      net2kiss.c - convert a network interface to KISS data stream
 *
 *	Copyright (C) 1996  Thomas Sailer (t.sailer@alumni.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 *
 * History:
 *  0.1  18.09.96  Started
 */

/*****************************************************************************/

#include <stdio.h>
#define __USE_XOPEN
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <grp.h>
#include <string.h>
#include <termios.h>
#include <limits.h>

#include <sys/socket.h>
#include <net/if.h>

#ifdef __GLIBC__
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif


/* --------------------------------------------------------------------- */

static int fdif, fdpty;
static struct ifreq ifr;
static char *progname;
static int verbose = 0;
static int i_am_unix98_pty_master = 0; /* unix98 ptmx support */
static char *namepts = NULL;  /* name of the unix98 pts slave, which
	                       * the client has to use */

/* --------------------------------------------------------------------- */

static void die(char *func) 
{
	fprintf(stderr, "%s: %s (%i)%s%s\n", progname, strerror(errno),
		errno, func ? " in " : "", func ? func : "");
	syslog(LOG_WARNING, "%s (%i)%s%s\n", strerror(errno),
		errno, func ? " in " : "", func ? func : "");
	exit(-1);
}

/* --------------------------------------------------------------------- */

static void display_packet(unsigned char *bp, unsigned int len)
{
	unsigned char v1=1,cmd=0;
	unsigned char i,j;

	if (!bp || !len) 
		return;
	if (len < 8)
		return;
	if (bp[1] & 1) {
		/*
		 * FlexNet Header Compression
		 */
		v1 = 0;
		cmd = (bp[1] & 2) != 0;
		printf("fm ? to ");
		i = (bp[2] >> 2) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = ((bp[2] << 4) | ((bp[3] >> 4) & 0xf)) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = ((bp[3] << 2) | ((bp[4] >> 6) & 3)) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = bp[4] & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = (bp[5] >> 2) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = ((bp[5] << 4) | ((bp[6] >> 4) & 0xf)) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		printf("-%u QSO Nr %u", bp[6] & 0xf, (bp[0] << 6) | 
		       (bp[1] >> 2));
		bp += 7;
		len -= 7;
	} else {
		/*
		 * normal header
		 */
		if (len < 15)
			return;
		if ((bp[6] & 0x80) != (bp[13] & 0x80)) {
			v1 = 0;
			cmd = (bp[6] & 0x80);
		}
		printf("fm ");
		for(i = 7; i < 13; i++) 
			if ((bp[i] &0xfe) != 0x40) 
				printf("%c",bp[i] >> 1);
		printf("-%u to ",(bp[13] >> 1) & 0xf);
		for(i = 0; i < 6; i++) 
			if ((bp[i] &0xfe) != 0x40) 
				printf("%c",bp[i] >> 1);
		printf("-%u",(bp[6] >> 1) & 0xf);
		bp += 14;
		len -= 14;
		if ((!(bp[-1] & 1)) && (len >= 7)) printf(" via ");
		while ((!(bp[-1] & 1)) && (len >= 7)) {
			for(i = 0; i < 6; i++) 
				if ((bp[i] &0xfe) != 0x40) 
					printf("%c",bp[i] >> 1);
			printf("-%u",(bp[6] >> 1) & 0xf);
			bp += 7;
			len -= 7;
			if ((!(bp[-1] & 1)) && (len >= 7)) 
				printf(",");
		}
	}
	if(!len) 
		return;
	i = *bp++;
	len--;
	j = v1 ? ((i & 0x10) ? '!' : ' ') : 
		((i & 0x10) ? (cmd ? '+' : '-') : (cmd ? '^' : 'v'));
	if (!(i & 1)) {
		/*
		 * Info frame
		 */
		printf(" I%u%u%c",(i >> 5) & 7,(i >> 1) & 7,j);
	} else if (i & 2) {
		/*
		 * U frame
		 */
		switch (i & (~0x10)) {
		case 0x03:
			printf(" UI%c",j);
			break;
		case 0x2f:
			printf(" SABM%c",j);
			break;
		case 0x43:
			printf(" DISC%c",j);
			break;
		case 0x0f:
			printf(" DM%c",j);
			break;
		case 0x63:
			printf(" UA%c",j);
			break;
		case 0x87:
			printf(" FRMR%c",j);
			break;
		default:
			printf(" unknown U (0x%x)%c",i & (~0x10),j);
			break;
		}
	} else {
		/*
		 * supervisory
		 */
		switch (i & 0xf) {
		case 0x1:
			printf(" RR%u%c",(i >> 5) & 7,j);
			break;
		case 0x5:
			printf(" RNR%u%c",(i >> 5) & 7,j);
			break;
		case 0x9:
			printf(" REJ%u%c",(i >> 5) & 7,j);
			break;
		default:
			printf(" unknown S (0x%x)%u%c", i & 0xf, 
			       (i >> 5) & 7, j);
			break;
		}
	}
	if (!len) {
		printf("\n");
		return;
	}
	printf(" pid=%02X\n", *bp++);
	len--;
	j = 0;
	while (len) {
		i = *bp++;
		if ((i >= 32) && (i < 128)) 
			printf("%c",i);
		else if (i == 13) {
			if (j) 
				printf("\n");
			j = 0;
		} else 
			printf(".");
		if (i >= 32) 
			j = 1;
		len--;
	}
	if (j) 
		printf("\n");
}

/* ---------------------------------------------------------------------- */

static int openpty(int *amaster, int *aslave, char *name, 
		   struct termios *termp, struct winsize *winp)
{
	char line[PATH_MAX];
        const char *cp1, *cp2;
	int master, slave;
	struct group *gr = getgrnam("tty");

	strcpy(line, "/dev/ptyXX");
	for (cp1 = "pqrstuvwxyzPQRST"; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = "0123456789abcdef"; *cp2; cp2++) {
			line[9] = *cp2;
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					return (-1);	/* out of ptys */
			} else {
				line[5] = 't';
				(void) chown(line, getuid(), 
					     gr ? gr->gr_gid : -1);
				(void) chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
#if 0
				(void) revoke(line);
#endif
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name)
						strcpy(name, line);
					if (termp)
						(void) tcsetattr(slave,
								 TCSAFLUSH, 
								 termp);
					if (winp)
						(void) ioctl(slave, 
							     TIOCSWINSZ, 
							     (char *)winp);
					return 0;
				}
				(void) close(master);
				line[5] = 'p';
			}
		}
	}
	errno = ENOENT;	/* out of ptys */
	return (-1);
}

/* ---------------------------------------------------------------------- */

static void restore_ifflags(int signum)
{
	if (ioctl(fdif, SIOCSIFFLAGS, &ifr) < 0)
		die("ioctl SIOCSIFFLAGS");
	close(fdif);
	close(fdpty);
	exit(0);
}

/* --------------------------------------------------------------------- */

#define KISS_FEND   ((unsigned char)0300)
#define KISS_FESC   ((unsigned char)0333)
#define KISS_TFEND  ((unsigned char)0334)
#define KISS_TFESC  ((unsigned char)0335)

#define KISS_CMD_DATA       0
#define KISS_CMD_TXDELAY    1
#define KISS_CMD_PPERSIST   2
#define KISS_CMD_SLOTTIME   3
#define KISS_CMD_TXTAIL     4
#define KISS_CMD_FULLDUP    5

#define KISS_HUNT    0
#define KISS_RX      1
#define KISS_ESCAPED 2

/* --------------------------------------------------------------------- */

static void kiss_overflow(void) 
{
	if (verbose)
		printf("KISS: packet overflow\n");
}

static void kiss_bad_escape(void) 
{
	if (verbose)
		printf("KISS: bad escape sequence\n");
}

static void display_kiss_packet(char *pfx, unsigned char *pkt, 
				unsigned int pktlen) 
{
	if (!verbose) 
		return;
	switch (*pkt) {
	case KISS_CMD_DATA:
		printf("%s: ", pfx);
		display_packet(pkt+1, pktlen-1);
		break;

	case KISS_CMD_TXDELAY:
		printf("%s: txdelay = %dms\n", pfx, (int)pkt[1] * 10);
		break;
     
	case KISS_CMD_PPERSIST:
		printf("%s: p persistence = %d\n", pfx, pkt[1]);
		break;

	case KISS_CMD_SLOTTIME:
		printf("%s: slottime = %dms\n", pfx, (int)pkt[1] * 10);
		break;

	case KISS_CMD_TXTAIL:
		printf("%s: txtail = %dms\n", pfx, (int)pkt[1] * 10);
		break;

	case KISS_CMD_FULLDUP:
		printf("%s: %sduplex\n", pfx, pkt[1] ? "full" : "half");
		break;

	default:
		printf("%s: unknown frame type 0x%02x, length %d\n", pfx,
		       *pkt, pktlen);
	}
}

static void kiss_packet(int fdif, char *addr, 
			unsigned char *pkt, unsigned int pktlen) 
{
	struct sockaddr to;
	int i;

	if (pktlen < 2)
		return;
	display_kiss_packet("KISS", pkt, pktlen);
	strncpy(to.sa_data, addr, sizeof(to.sa_data));
	i = sendto(fdif, pkt, pktlen, 0, &to, sizeof(to));
	if (i >= 0)
		return;
	if (errno == EMSGSIZE) {
		if (verbose)
			printf("sendto: %s: packet (size %d) too "
			       "long\n", addr, pktlen-1);
		return;
	}
	if (errno == EWOULDBLOCK) {
		if (verbose)
			printf("sendto: %s: busy\n", addr);
		return;
	}
	die("sendto");
	return;
}

/* --------------------------------------------------------------------- */

static int doio(int fdif, int fdpty, char *ifaddr)
{
	unsigned char ibuf[2048];
	unsigned char *bp;
	unsigned char pktbuf[2048];
	unsigned char *pktptr = pktbuf;
	unsigned char pktstate = KISS_HUNT;
      	unsigned char obuf[16384];
	unsigned int ob_wp = 0, ob_rp = 0, ob_wpx;
	int i;
	fd_set rmask, wmask;
	struct sockaddr from;
	socklen_t from_len;
	
#define ADD_CHAR(c) \
	obuf[ob_wpx] = c; \
	ob_wpx = (ob_wpx + 1) % sizeof(obuf); \
	if (ob_wpx == ob_rp) goto kissencerr;
 
#define ADD_KISSCHAR(c) \
	if (((c) & 0xff) == KISS_FEND) \
                { ADD_CHAR(KISS_FESC); ADD_CHAR(KISS_TFEND); } \
	else if (((c) & 0xff) == KISS_FESC) \
                { ADD_CHAR(KISS_FESC); ADD_CHAR(KISS_TFESC); } \
	else { ADD_CHAR(c); }
	 
	for (;;) {
		FD_ZERO(&rmask);
		FD_ZERO(&wmask);
		FD_SET(fdif, &rmask);
		FD_SET(fdpty, &rmask);
		if (ob_rp != ob_wp)
			FD_SET(fdpty, &wmask);
		i = select((fdif > fdpty) ? fdif+1 : fdpty+1, &rmask, &wmask,
			   NULL, NULL);
		if (i < 0)
			die("select");
		if (FD_ISSET(fdpty, &wmask)) {
			if (ob_rp > ob_wp)
				i = write(fdpty, obuf+ob_rp, 
					  sizeof(obuf)-ob_rp);
			else 
				i = write(fdpty, obuf+ob_rp, ob_wp - ob_rp);
			if (i < 0)
				die("write");
			ob_rp = (ob_rp + i) % sizeof(obuf);
		}
		if (FD_ISSET(fdpty, &rmask)) {
			i = read(fdpty, bp = ibuf, sizeof(ibuf));
			if (i < 0) {
				if (errno != EIO)
					die("read");
				return 0;
			}
			for (; i > 0; i--, bp++) {
				switch (pktstate) {
				default:
				case KISS_HUNT:
					if (*bp != KISS_FEND)
						break;
					pktptr = pktbuf;
					pktstate = KISS_RX;
					break;

				case KISS_RX:
					if (*bp == KISS_FESC) {
						pktstate = KISS_ESCAPED;
						break;
					}
					if (*bp == KISS_FEND) {
						kiss_packet(fdif, ifaddr,
							    pktbuf, 
							    pktptr - pktbuf);
						pktptr = pktbuf;
						break;
					}
					if (pktptr >= pktbuf+sizeof(pktbuf)) {
						kiss_overflow();
						pktstate = KISS_HUNT;
						break;
					}
					*pktptr++ = *bp;
					break;

				case KISS_ESCAPED:
					if (pktptr >= pktbuf+sizeof(pktbuf)) {
						kiss_overflow();
						pktstate = KISS_HUNT;
						break;
					}
					if (*bp == KISS_TFESC)
						*pktptr++ = KISS_FESC;
					else if (*bp == KISS_TFEND)
						*pktptr++ = KISS_FEND;
					else {
						kiss_bad_escape();
						pktstate = KISS_HUNT;
						break;
					}
					pktstate = KISS_RX;
					break;
				}
			}				
		}
		if (FD_ISSET(fdif, &rmask)) {
			from_len = sizeof(from);
			i = recvfrom(fdif, bp = ibuf, sizeof(ibuf), 0, &from,
				     &from_len);
			if (i < 0) {
				if (errno == EWOULDBLOCK)
					continue;
				die("recvfrom");
			}
			if (verbose)
				display_kiss_packet(from.sa_data, ibuf, i);
			ob_wpx = ob_wp;
			ADD_CHAR(KISS_FEND);
			for (; i > 0; i--, bp++) {
				ADD_KISSCHAR(*bp);
			}
			ADD_CHAR(KISS_FEND);
			ob_wp = ob_wpx;
		}
		continue;
	kissencerr:
		if (verbose)
			printf("KISS: Encoder out of memory\n");
	}
#undef ADD_CHAR
#undef ADD_KISSCHAR
}

/* --------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	struct ifreq ifr_new;
	struct sockaddr sa;
	char *name_iface = "bc0";
	char *name_pname = NULL;
	char slavename[PATH_MAX];
	char *master_name;
	struct termios termios;
	int c;
	int errflg = 0;
	int symlnk = 0;
	int symlnkforce = 0;
	short if_newflags = 0;
	int proto = htons(ETH_P_AX25);

	progname = argv[0];
	while ((c = getopt(argc, argv, "sfzvai:")) != EOF) {
		switch (c) {
		case 's':
			symlnk = 1;
			break;
		case 'f':
			symlnkforce = 1;
			break;
		case 'i':
			name_iface = optarg;
			break;
		case 'z':
			if_newflags |= IFF_PROMISC;
			break;
		case 'v':
			verbose++;
			break;
		case 'a':
			proto = htons(ETH_P_ALL);
			break;
		default:
			errflg++;
			break;
		}
	}
	if (argc > optind)
		name_pname = argv[optind];
	else
		errflg++;
	if (errflg) {
		fprintf(stderr, "usage: %s [-s] [-f] [-i iface] "
			"[-z] [-v] ptyname\n", progname);
		exit(1);
	}
	openlog(progname, LOG_PID, LOG_DAEMON);
	if (symlnk) {
		int fdtty;

		if (openpty(&fdpty, &fdtty, slavename, NULL, NULL)) {
			fprintf(stderr, "%s: out of pseudoterminals\n",
				progname);
			exit(1);
		}
		close(fdtty);
		fcntl(fdpty, F_SETFL, fcntl(fdpty, F_GETFL, 0) | O_NONBLOCK);
		if (symlnkforce)
			unlink(name_pname);
		if (symlink(slavename, name_pname))
			perror("symlink");
		/* Users await the slave pty to be referenced in the 2nd line */
		printf("Awaiting client connects on\n%s\n", slavename);
		slavename[5] = 'p';
		master_name = slavename;
	} else {
		if ((fdpty = open(name_pname, 
				  O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
			fprintf(stderr, "%s: cannot open \"%s\"\n", progname,
				name_pname);
			exit(1);
		}
		if (!strcmp("/dev/ptmx", name_pname))
			i_am_unix98_pty_master = 1;
		master_name = name_pname;
	}
	if ((fdif = socket(PF_INET, SOCK_PACKET, proto)) < 0) 
		die("socket");
	memset(&sa, 0, sizeof(struct sockaddr));
	memcpy(sa.sa_data, name_iface, sizeof(sa.sa_data));
	sa.sa_family = AF_INET;
	if (bind(fdif, &sa, sizeof(struct sockaddr)) < 0)
		die("bind"); 
	memcpy(ifr.ifr_name, name_iface, IFNAMSIZ);
	if (ioctl(fdif, SIOCGIFFLAGS, &ifr) < 0)
		die("ioctl SIOCGIFFLAGS");
       	ifr_new = ifr;	
	ifr_new.ifr_flags |= if_newflags;
	if (ioctl(fdif, SIOCSIFFLAGS, &ifr_new) < 0)
		die("ioctl SIOCSIFFLAGS");
	if (i_am_unix98_pty_master) {
		/* get name of pts-device */
		if ((namepts = ptsname(fdpty)) == NULL) {
			fprintf(stderr, "%s: Cannot get name of pts-device.\n", progname);
			exit (1);
		}
		/* unlock pts-device */
		if (unlockpt(fdpty) == -1) {
			fprintf(stderr, "%s: Cannot unlock pts-device %s\n", progname, namepts);
			exit (1);
		}
		/* Users await the slave pty to be referenced in the 2nd line */
		printf("Awaiting client connects on\n%s\n", namepts);
		if (!verbose){
			fflush(stdout);
			fflush(stderr);
			close(0);
			close(1);
			close(2);
		}
	}
	signal(SIGHUP, restore_ifflags);
	signal(SIGINT, restore_ifflags);
	signal(SIGTERM, restore_ifflags);
	signal(SIGQUIT, restore_ifflags);
	signal(SIGUSR1, restore_ifflags);
	signal(SIGUSR2, restore_ifflags);

	for (;;) {
		if (tcgetattr(fdpty, &termios))
			die("tcgetattr");
		termios.c_iflag = IGNBRK;
		termios.c_oflag = 0;
		termios.c_lflag = 0;
		termios.c_cflag &= ~(CSIZE|CSTOPB|PARENB|HUPCL|CRTSCTS);
		termios.c_cflag |= CS8|CREAD|CLOCAL;
		if (tcsetattr(fdpty, TCSANOW, &termios))
			die("tsgetattr");
		if (doio(fdif, fdpty, name_iface))
			break;
		if (i_am_unix98_pty_master) {
			if (verbose)
				printf("%s: Trying to poll port ptmx (slave %s).\nWaiting 30s...\n", progname, namepts);
			syslog(LOG_WARNING, "Trying to poll port ptmx (slave %s). Waiting 30s...\n", namepts);
			sleep(30);
		} else {
			/*
			 * try to reopen master
		 	 */
			if (verbose)
				printf("reopening master tty: %s\n", master_name);
			close(fdpty);

			if ((fdpty = open(master_name, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
				fprintf(stderr, "%s: cannot reopen \"%s\"\n", progname,
					master_name);
				exit(1);
			}
		}
	}

	restore_ifflags(0);
	exit(0);
}

/* --------------------------------------------------------------------- */
