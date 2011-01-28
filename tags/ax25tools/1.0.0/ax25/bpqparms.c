/* 
   bpqparms.c

   Copyright 1996, by Joerg Reuter jreuter@poboxes.com

   This program is free software; you can redistribute it and/or modify 
   it under the terms of the (modified) GNU General Public License 
   delivered with the LinuX kernel source.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should find a copy of the GNU General Public License in 
   /usr/src/linux/COPYING; 

*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <net/if.h>

#ifdef __GLIBC__
#include <net/ethernet.h> /* is this really needed ??  */
#endif

#include <linux/bpqether.h> /* xlz - dammit, we need this again */

#include <config.h>

#define RCS_ID "$Id: bpqparms.c,v 1.3 2007/01/23 13:40:01 ralf Exp $"

void usage(void)
{
		fprintf(stderr, "usage   : bpqparms dev -d address [-a address]\n");
		fprintf(stderr, "examples: bpqparms bpq0 -d 00:80:AD:1B:05:26\n");
		fprintf(stderr, "          bpqparms bpq0 -d broadcast -a 00:80:AD:1B:05:26\n");
		exit(1);
}

char *Version = "$Revision: 1.3 $";

int get_hwaddr(unsigned char *k, char *s)
{
	unsigned char broadcast[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	unsigned int  eth[ETH_ALEN];
	int n;

	if (strcmp(s, "default") == 0 || strcmp(s, "broadcast") == 0) {
		memcpy(k, broadcast, ETH_ALEN);
	} else {
		n = sscanf(s, "%x:%x:%x:%x:%x:%x", 
		&eth[0], &eth[1], &eth[2], &eth[3], &eth[4], &eth[5]);
		
		if (n < 6)
			return 1;

		for (n = 0; n < ETH_ALEN; n++)
			k[n] = eth[n];
	}

	return 0;
}

int main(int argc, char **argv)
{
	int fd;
	int cmd, flag;
	struct ifreq ifr;
	char dev[40];
	struct bpq_ethaddr addr;

	flag = 0;

	while ((cmd = getopt(argc, argv, "d:a:vVh")) != EOF) {
		switch (cmd) {
			case 'd':
				flag |= 1;
				if (get_hwaddr(addr.destination, optarg)) {
					fprintf(stderr, "bpqparms: invalid 'destination' address %s\n", optarg);
					return 1;
				}
				break;
				
			case 'a':
				flag |= 2;
				if (get_hwaddr(addr.accept, optarg)) {
					fprintf(stderr, "bpqparms: invalid 'accept' address %s\n", optarg);
					return 1;
				}
				break;

			case 'V':
				printf("bpqparms version %s\n", Version);
				printf("Copyright 1996, Jörg Reuter (jreuter@poboxes.com)\n");
				printf("This program is free software; you can redistribute it and/or modify\n");
				printf("it under the terms of the GNU General Public License as published by\n");
				printf("the Free Software Foundation; either version 2 of the License, or\n");
				printf(" (at your option) any later version.\n\n");
				printf("This program is distributed in the hope that it will be useful,\n");
				printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
				printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
				return 0;

			case 'v':
				printf("bpqparms: %s\n", VERSION);
				return(0);

			case 'h':
			case ':':
			case '?':
				usage();
		}
	}
	
	if (!(flag & 0x01) || optind+1 > argc)
		usage();
	
	strcpy(dev, argv[optind]);

	if ((flag & 0x02) == 0)
		memcpy(addr.accept, addr.destination, ETH_ALEN);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_data = (caddr_t) &addr;
	
	if (ioctl(fd, SIOCSBPQETHADDR, &ifr) < 0) {
		perror("bpqparms SIOCSBPQETHADDR");
		close(fd);
		return 1;
	}
	
	close(fd);

	return 0;
}
