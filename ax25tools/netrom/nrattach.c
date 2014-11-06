#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>
#include <limits.h>

#include <config.h>

#include <sys/ioctl.h>

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>


#include <netax25/ax25.h>
#include <netrom/netrom.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/nrconfig.h>

#include "../pathnames.h"

char *callsign;
int  mtu   = 0;

int readconfig(char *port)
{
	FILE *fp;
	char buffer[90], *s;
	int n = 0;
	
	if ((fp = fopen(CONF_NRPORTS_FILE, "r")) == NULL) {
		fprintf(stderr, "nrattach: cannot open nrports file\n");
		return FALSE;
	}

	while (fgets(buffer, 90, fp) != NULL) {
		n++;
	
		if ((s = strchr(buffer, '\n')) != NULL)
			*s = '\0';

		if (strlen(buffer) > 0 && *buffer == '#')
			continue;

		if ((s = strtok(buffer, " \t\r\n")) == NULL) {
			fprintf(stderr, "nrattach: unable to parse line %d of the nrports file\n", n);
			return FALSE;
		}
		
		if (strcmp(s, port) != 0)
			continue;
			
		if ((s = strtok(NULL, " \t\r\n")) == NULL) {
			fprintf(stderr, "nrattach: unable to parse line %d of the nrports file\n", n);
			return FALSE;
		}

		callsign = strdup(s);

		if ((s = strtok(NULL, " \t\r\n")) == NULL) {
			fprintf(stderr, "nrattach: unable to parse line %d of the nrports file\n", n);
			return FALSE;
		}

		if ((s = strtok(NULL, " \t\r\n")) == NULL) {
			fprintf(stderr, "nrattach: unable to parse line %d of the nrports file\n", n);
			return FALSE;
		}

		if (mtu == 0) {
			if ((mtu = atoi(s)) <= 0) {
				fprintf(stderr, "nrattach: invalid paclen setting\n");
				return FALSE;
			}
		}

		fclose(fp);
		
		return TRUE;
	}
	
	fclose(fp);

	fprintf(stderr, "nrattach: cannot find port %s in nrports\n", port);
	
	return FALSE;
}

int getfreedev(char *dev)
{
	struct ifreq ifr;
	int fd;
	int i;
	
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("nrattach: socket");
		return FALSE;
	}

	for (i = 0; i < INT_MAX; i++) {
		sprintf(dev, "nr%d", i);
		strcpy(ifr.ifr_name, dev);
	
		if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
			perror("nrattach: SIOCGIFFLAGS");
			return FALSE;
		}

		if (!(ifr.ifr_flags & IFF_UP)) {
			close(fd);
			return TRUE;
		}
	}

	close(fd);

	return FALSE;
}

int startiface(char *dev, struct hostent *hp)
{
	struct ifreq ifr;
	char call[7];
	int fd;
	
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("nrattach: socket");
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
			perror("nrattach: SIOCSIFADDR");
			return FALSE;
		}
	}

	if (ax25_aton_entry(callsign, call) == -1)
		return FALSE;

	ifr.ifr_hwaddr.sa_family = ARPHRD_NETROM;
	memcpy(ifr.ifr_hwaddr.sa_data, call, 7);
	
	if (ioctl(fd, SIOCSIFHWADDR, &ifr) != 0) {
		perror("nrattach: SIOCSIFHWADDR");
		return FALSE;
	}

	ifr.ifr_mtu = mtu;

	if (ioctl(fd, SIOCSIFMTU, &ifr) < 0) {
		perror("nrattach: SIOCSIFMTU");
		return FALSE;
	}

	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		perror("nrattach: SIOCGIFFLAGS");
		return FALSE;
	}

	ifr.ifr_flags &= IFF_NOARP;
	ifr.ifr_flags |= IFF_UP;
	ifr.ifr_flags |= IFF_RUNNING;

	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		perror("nrattach: SIOCSIFFLAGS");
		return FALSE;
	}
	
	close(fd);
	
	return TRUE;
}
	

int main(int argc, char *argv[])
{
	int fd;
	char dev[64];
	struct hostent *hp = NULL;

	while ((fd = getopt(argc, argv, "i:m:v")) != -1) {
		switch (fd) {
			case 'i':
				if ((hp = gethostbyname(optarg)) == NULL) {
					fprintf(stderr, "nrattach: invalid internet name/address - %s\n", optarg);
					return 1;
				}
				break;
			case 'm':
				if ((mtu = atoi(optarg)) <= 0) {
					fprintf(stderr, "nrattach: invalid mtu size - %s\n", optarg);
					return 1;
				}
				break;
			case 'v':
				printf("nrattach: %s\n", VERSION);
				return 0;
			case ':':
			case '?':
				fprintf(stderr, "usage: nrattach [-i inetaddr] [-m mtu] [-v] port\n");
				return 1;
		}
	}
	
	if ((argc - optind) != 1) {
		fprintf(stderr, "usage: nrattach [-i inetaddr] [-m mtu] [-v] port\n");
		return 1;
	}

	if (!readconfig(argv[optind]))
		return 1;

	if (!getfreedev(dev)) {
		fprintf(stderr, "nrattach: cannot find free NET/ROM device\n");
		return 1;
	}
	
	if (!startiface(dev, hp))
		return 1;		

	printf("NET/ROM port %s bound to device %s\n", argv[optind], dev);
		
	return 0;
}
