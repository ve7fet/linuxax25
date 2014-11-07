#include <stdio.h>
#define __USE_XOPEN
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>
#include <syslog.h>
#include <errno.h>

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/daemon.h>
#include <netax25/axlib.h>
#include <netax25/ttyutils.h>

#include "../pathnames.h"

#ifndef N_6PACK
#define N_6PACK 7	/* This is valid for all architectures in 2.2.x */
#endif

static char *callsign;
static int  speed	= 0;
static int  mtu		= 0;
static int  logging	= FALSE;
static char *progname	= NULL;
static char *kttyname	= NULL;
static char *portname	= NULL;
static char *inetaddr	= NULL;
static int allow_broadcast = 0;
static int i_am_unix98_pty_master = 0; /* unix98 ptmx support */

static char *kiss_basename(char *s)
{
	char *p = strrchr(s, '/');
	return p ? p + 1 : s;
}

static void terminate(int sig)
{
	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}

	if (!i_am_unix98_pty_master)
		tty_unlock(kttyname);

	exit(0);
}

static int readconfig(char *port)
{
	FILE *fp;
	char buffer[90], *s;
	int n = 0;
	
	if ((fp = fopen(CONF_AXPORTS_FILE, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open axports file %s\n", 
                        progname, CONF_AXPORTS_FILE);
		return FALSE;
	}

	while (fgets(buffer, 90, fp) != NULL) {
		n++;
	
		if ((s = strchr(buffer, '\n')) != NULL)
			*s = '\0';

		if (*buffer == 0 || *buffer == '#')
			continue;

		if ((s = strtok(buffer, " \t\r\n")) == NULL) {
			fprintf(stderr, "%s: unable to parse line %d of the axports file\n", progname, n);
			return FALSE;
		}
		
		if (strcmp(s, port) != 0)
			continue;
			
		if ((s = strtok(NULL, " \t\r\n")) == NULL) {
			fprintf(stderr, "%s: unable to parse line %d of the axports file\n", progname, n);
			return FALSE;
		}

		callsign = strdup(s);

		if ((s = strtok(NULL, " \t\r\n")) == NULL) {
			fprintf(stderr, "%s: unable to parse line %d of the axports file\n", progname, n);
			return FALSE;
		}

		speed = atoi(s);

		if ((s = strtok(NULL, " \t\r\n")) == NULL) {
			fprintf(stderr, "%s: unable to parse line %d of the axports file\n", progname, n);
			return FALSE;
		}

		if (mtu == 0) {
			if ((mtu = atoi(s)) <= 0) {
				fprintf(stderr, "%s: invalid paclen setting\n", progname);
				return FALSE;
			}
		}

		fclose(fp);
		
		return TRUE;
	}
	
	fclose(fp);

	fprintf(stderr, "%s: cannot find port %s in axports\n", progname, port);
	
	return FALSE;
}


static int setifcall(int fd, char *name)
{
	char call[7];

	if (ax25_aton_entry(name, call) == -1)
		return FALSE;
	
	if (ioctl(fd, SIOCSIFHWADDR, call) != 0) {
		close(fd);
		fprintf(stderr, "%s: ", progname);
		perror("SIOCSIFHWADDR");
		return FALSE;
	}

	return TRUE;
}


static int startiface(char *dev, struct hostent *hp)
{
	struct ifreq ifr;
	int fd;
	
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror("socket");
		return FALSE;
	}

	strcpy(ifr.ifr_name, dev);
	
	if (hp != NULL) {
		ifr.ifr_addr.sa_family = AF_INET;
		
		ifr.ifr_addr.sa_data[0] = 0;
		ifr.ifr_addr.sa_data[1] = 0;
		ifr.ifr_addr.sa_data[2] = hp->h_addr_list[0][0];
		ifr.ifr_addr.sa_data[3] = hp->h_addr_list[0][1];
		ifr.ifr_addr.sa_data[4] = hp->h_addr_list[0][2];
		ifr.ifr_addr.sa_data[5] = hp->h_addr_list[0][3];
		ifr.ifr_addr.sa_data[6] = 0;

		if (ioctl(fd, SIOCSIFADDR, &ifr) < 0) {
			fprintf(stderr, "%s: ", progname);
			perror("SIOCSIFADDR");
			return FALSE;
		}
	}

	ifr.ifr_mtu = mtu;

	if (ioctl(fd, SIOCSIFMTU, &ifr) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror("SIOCSIFMTU");
		return FALSE;
	}

	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror("SIOCGIFFLAGS");
		return FALSE;
	}

	ifr.ifr_flags &= IFF_NOARP;
	ifr.ifr_flags |= IFF_UP;
	ifr.ifr_flags |= IFF_RUNNING;
	if (allow_broadcast)
		ifr.ifr_flags |= IFF_BROADCAST; /* samba broadcasts are a pain.. */
	else
		ifr.ifr_flags &= ~(IFF_BROADCAST); /* samba broadcasts are a pain.. */

	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror("SIOCSIFFLAGS");
		return FALSE;
	}
	
	close(fd);
	
	return TRUE;
}

static void usage(void)
{
        fprintf(stderr, "usage: %s [-b] [-l] [-m mtu] [-v] tty port [inetaddr]\n", progname);
}

int main(int argc, char *argv[])
{
	int  fd;
	int  disc = N_AX25;
	char dev[64];
	int  v = 4;
	char *namepts = NULL;  /* name of the unix98 pts slave, which
	                        * the client has to use */
	struct hostent *hp = NULL;

	progname = kiss_basename(argv[0]);

	if (!strcmp(progname, "spattach"))
		disc = N_6PACK;

	while ((fd = getopt(argc, argv, "b6i:lm:v")) != -1) {
		switch (fd) {
			case '6':
				disc = N_6PACK;
				break;
			case 'b':
				allow_broadcast = 1;
				break;
			case 'i':
				fprintf(stderr, "%s: -i flag depreciated, use new command line format instead.\n", progname);
				inetaddr = optarg;
				break;
			case 'l':
				logging = TRUE;
				break;
			case 'm':
				if ((mtu = atoi(optarg)) <= 0) {
					fprintf(stderr, "%s: invalid mtu size - %s\n", progname, optarg);
					return 1;
				}
				break;
			case 'v':
				printf("%s: %s\n", progname, VERSION);
				return 0;
			case ':':
			case '?':
				usage();
				return 1;
		}
	}

#ifdef	notdef
	if ((argc - optind) != 3 && ((argc - optind) != 2 || !inetaddr)) {
#else
	if ((argc - optind) < 2) {
#endif
		usage();
		return 1;
	}

	kttyname = argv[optind++];
	portname = argv[optind++];

	if (argc-1 >= optind && !inetaddr)
		inetaddr = argv[optind];

	if (!strcmp("/dev/ptmx", kttyname))
		i_am_unix98_pty_master = 1;

	if (!i_am_unix98_pty_master) {
		if (tty_is_locked(kttyname)) {
			fprintf(stderr, "%s: device %s already in use\n", progname, kttyname);
			return 1;
		}
	}

	if (!readconfig(portname))
		return 1;

        if (inetaddr && (hp = gethostbyname(inetaddr)) == NULL) {
		fprintf(stderr, "%s: invalid internet name/address - %s\n", progname, inetaddr);
		return 1;
	}

	if ((fd = open(kttyname, O_RDONLY | O_NONBLOCK)) == -1) {
		if (errno == ENOENT) {
			fprintf(stderr, "%s: Cannot find serial device %s, no such file or directory.\n", progname, kttyname);
		} else {
			fprintf(stderr, "%s: %s: ", progname, kttyname);
			perror("open");
		}
		return 1;
	}

	if (i_am_unix98_pty_master) {
		/* get name of pts-device */
		if ((namepts = ptsname(fd)) == NULL) {
			fprintf(stderr, "%s: Cannot get name of pts-device.\n", progname);
			return 1;
		}
		/* unlock pts-device */
		if (unlockpt(fd) == -1) {
			fprintf(stderr, "%s: Cannot unlock pts-device %s\n", progname, namepts);
			return 1;
		}
	}

	if (speed != 0 && !tty_speed(fd, speed))
		return 1;

	if (ioctl(fd, TIOCSETD, &disc) == -1) {
		fprintf(stderr, "%s: Error setting line discipline: ", progname);
		perror("TIOCSETD");
		fprintf(stderr, "Are you sure you have enabled %s support in the kernel\n", 
			disc == N_AX25 ? "MKISS" : "6PACK");
		fprintf(stderr, "or, if you made it a module, that the module is loaded?\n");
		return 1;
	}

	if (ioctl(fd, SIOCGIFNAME, dev) == -1) {
		fprintf(stderr, "%s: ", progname);
		perror("SIOCGIFNAME");
		return 1;
	}

	if (!setifcall(fd, callsign))
		return 1;

	/* Now set the encapsulation */
	if (ioctl(fd, SIOCSIFENCAP, &v) == -1) {
		fprintf(stderr, "%s: ", progname);
		perror("SIOCSIFENCAP");
		return 1;
	}

	/* ax25 ifaces should not really need to have an IP address assigned to */
	if (!startiface(dev, hp))
		return 1;		

	printf("AX.25 port %s bound to device %s\n", portname, dev);
	if (i_am_unix98_pty_master) {
		/* Users await the slave pty to be referenced in the 3d line */
		printf("Awaiting client connects on\n%s\n", namepts);
	}
	if (logging) {
		openlog(progname, LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "AX.25 port %s bound to device %s%s%s\n", portname, dev, (i_am_unix98_pty_master ? " with slave pty " : ""), (i_am_unix98_pty_master ? namepts : ""));

	}

	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, terminate);

	/*
	 * Become a daemon if we can.
	 */
	if (!daemon_start(FALSE)) {
		fprintf(stderr, "%s: cannot become a daemon\n", progname);
		return 1;
	}
	if (!i_am_unix98_pty_master) {
		if (!tty_lock(kttyname))
			return 1;
	}

	fflush(stdout);
	fflush(stderr);
	close(0);
	close(1);
	close(2);

	while (1)
		sleep(10000);

	/* NOT REACHED */
	return 0;
}
