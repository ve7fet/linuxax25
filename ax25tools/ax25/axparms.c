#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>

#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include "../pathnames.h"

void usage(void)
{
	fprintf(stderr, "usage: axparms --assoc|--forward|--route|--setcall|--version ...\n");
}

void usageassoc(void)
{
	fprintf(stderr, "usage: axparms --assoc show\n");
	fprintf(stderr, "usage: axparms --assoc policy default|deny\n");
	fprintf(stderr, "usage: axparms --assoc [callsign] [username]\n");
	fprintf(stderr, "usage: axparms --assoc [callsign] delete\n");
}

void usageforward(void)
{
	fprintf(stderr, "usage: axparms --forward <portfrom> <portto>\n");
	fprintf(stderr, "usage: axparms --forward <portfrom> delete\n");
}

void usageroute(void)
{
	fprintf(stderr, "usage: axparms --route add port callsign [digi ...] [--ipmode mode]\n");
	fprintf(stderr, "usage: axparms --route del port callsign\n");
	fprintf(stderr, "usage: axparms --route list\n");
}

void usagesetcall(void)
{
	fprintf(stderr, "usage: axparms --setcall interface callsign\n");
}

int routes(int s, int argc, char *argv[], ax25_address *callsign)
{
	struct ax25_routes_struct ax25_route;
	struct ax25_route_opt_struct ax25_opt;
	int i, j;
	int ip_mode = ' ';
	FILE* fp;
	char routebuf[80];

	if (strcmp(argv[2], "add") == 0) {
		ax25_route.port_addr  = *callsign;
		ax25_route.digi_count = 0;

		if (strcmp(argv[4], "default") == 0) {
			ax25_route.dest_addr = null_ax25_address;
		} else {
			if (ax25_aton_entry(argv[4], (char *)&ax25_route.dest_addr) == -1)
				return 1;
		}

		for (i = 5, j = 0; i < argc && j < 6; i++) {
			if (strncmp(argv[i], "--i", 3) == 0 || strncmp(argv[i], "-i", 2) == 0) {
				if (++i == argc) {
					fprintf(stderr, "axparms: -i must have a parameter\n");
					return 1;
				}
				switch (*argv[i]) {
					case 'd':
					case 'D':
						ip_mode = 'D';
						break;
					case 'v':
					case 'V':
						ip_mode = 'V';
						break;
					default:
						ip_mode = ' ';
						break;
				}
			} else {
				if (ax25_aton_entry(argv[i], (char *)&ax25_route.digi_addr[j]) == -1)
					return 1;
				ax25_route.digi_count++;
				j++;
			}
		}

		if (ioctl(s, SIOCADDRT, &ax25_route) != 0) {
			perror("axparms: SIOCADDRT");
			return 1;
		}
		
		ax25_opt.port_addr = *callsign;
		ax25_opt.dest_addr = ax25_route.dest_addr;
		ax25_opt.cmd = AX25_SET_RT_IPMODE;
		ax25_opt.arg = ip_mode;
		
		if (ioctl(s, SIOCAX25OPTRT, &ax25_opt) != 0) {
			perror("axparms: SIOCAX25OPTRT");
			return 1;
		}
	}
	
	if (strcmp(argv[2], "del") == 0) {
		ax25_route.port_addr  = *callsign;
		ax25_route.digi_count = 0;

		if (strcmp(argv[4], "default") == 0) {
			ax25_route.dest_addr = null_ax25_address;
		} else {
			if (ax25_aton_entry(argv[4], (char *)&ax25_route.dest_addr) == -1)
				return 1;
		}

		if (ioctl(s, SIOCDELRT, &ax25_route) != 0) {
			perror("axparms: SIOCDELRT");
			return 1;
		}
	}

	if (strcmp(argv[2], "list") == 0) {
		if ((fp=fopen(PROC_AX25_ROUTE_FILE,"r")) == NULL) {
			fprintf(stderr, "axparms: route: cannot open %s\n",
PROC_AX25_ROUTE_FILE);
			return 1;
		}
		while (fgets(routebuf,80,fp))
			printf(routebuf);
		puts("");
	}

	return 0;
}

int setifcall(int s, char *ifn, char *name)
{
	char call[7];
	struct ifreq ifr;
	
	if (ax25_aton_entry(name, call) == -1)
		return 1;

	strcpy(ifr.ifr_name, ifn);
	memcpy(ifr.ifr_hwaddr.sa_data, call, 7);
	ifr.ifr_hwaddr.sa_family = AF_AX25;
	
	if (ioctl(s, SIOCSIFHWADDR, &ifr) != 0) {
		perror("axparms: SIOCSIFHWADDR");
		return 1;
	}

	return 0;
}

int associate(int s, int argc, char *argv[])
{
	char buffer[80], *u, *c;
	struct sockaddr_ax25 sax25;
	struct passwd *pw;
	int opt;
	FILE *fp;

	
	if (strcmp(argv[2], "show") == 0) {
		if (argc < 3) {
			usageassoc();
			exit(1);
		}

		if ((fp = fopen(PROC_AX25_CALLS_FILE, "r")) == NULL) {
			fprintf(stderr, "axparms: associate: cannot open %s\n", PROC_AX25_CALLS_FILE);
			return 1;
		}

		fgets(buffer, 80, fp);

		printf("Userid     Callsign\n");

		while (fgets(buffer, 80, fp) != NULL) {
			u = strtok(buffer, " \t\n");
			c = strtok(NULL, " \t\n");
			if ((pw = getpwuid(atoi(u))) != NULL)
				printf("%-10s %s\n", pw->pw_name, c);
		}

		fclose(fp);

		return 0;
	}

	if (strcmp(argv[2], "policy") == 0) {
		if (argc < 4) {
			usageassoc();
			exit(1);
		}

		if (strcmp(argv[3], "default") == 0) {
			opt = AX25_NOUID_DEFAULT;

			if (ioctl(s, SIOCAX25NOUID, &opt) == -1) {
				perror("axparms: SIOCAX25NOUID");
				return 1;
			}

			return 0;
		}

		if (strcmp(argv[3], "deny") == 0) {
			opt = AX25_NOUID_BLOCK;

			if (ioctl(s, SIOCAX25NOUID, &opt) == -1) {
				perror("axparms: SIOCAX25NOUID");
				return 1;
			}

			return 0;
		}

		fprintf(stderr, "axparms: associate: 'default' or 'deny' required\n");

		return 1;
	}

	if (argc < 4) {
		usageassoc();
		exit(1);
	}
	
	if (ax25_aton_entry(argv[2], (char *)&sax25.sax25_call) == -1) {
		fprintf(stderr, "axparms: associate: invalid callsign %s\n", argv[2]);
		return 1;
	}

	if (strcmp(argv[3], "delete") == 0) {
		if (ioctl(s, SIOCAX25DELUID, &sax25) == -1) {
			perror("axparms: SIOCAX25DELUID");
			return 1;
		}

		return 0;
	}

	if ((pw = getpwnam(argv[3])) == NULL) {
		fprintf(stderr, "axparms: associate: unknown username %s\n", argv[3]);
		return 1;
	}

	sax25.sax25_uid = pw->pw_uid;
		
	if (ioctl(s, SIOCAX25ADDUID, &sax25) == -1) {
		perror("axparms: SIOCAX25ADDUID");
		return 1;
	}

	return 0;
}

int forward(int s, int argc, char *argv[])
{
#ifdef HAVE_AX25_FWD_STRUCT
	struct ax25_fwd_struct ax25_fwd;
	char *addr;

	if (argc < 4) {
		usageforward();
		exit(1);
	}

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "axparms: no AX.25 port data configured\n");
		return 1;
	}

	if ((addr = ax25_config_get_addr(argv[2])) == NULL) {
		fprintf(stderr, "axparms: invalid port name - %s\n", argv[2]);
		return 1;
	}

	if (ax25_aton_entry(addr, (char *)&ax25_fwd.port_from) == -1) {
		fprintf(stderr, "axparms: invalid port name - %s\n", argv[2]);
		return 1;
	}

	if (strcmp(argv[3], "delete") == 0) {
		if (ioctl(s, SIOCAX25DELFWD, &ax25_fwd) == -1) {
			perror("axparms: SIOCAX25DELFWD");
			return 1;
		}

		return 0;
	}

	if ((addr = ax25_config_get_addr(argv[3])) == NULL) {
		fprintf(stderr, "axparms: invalid port name - %s\n", argv[3]);
		return 1;
	}

	if (ax25_aton_entry(addr, (char *)&ax25_fwd.port_to) == -1) {
		fprintf(stderr, "axparms: invalid port name - %s\n", argv[3]);
		return 1;
	}

	if (ioctl(s, SIOCAX25ADDFWD, &ax25_fwd) == -1) {
		perror("axparms: SIOCAX25ADDFWD");
		return 1;
	}
#else
        fprintf(stderr, "axparms: Not compiled in with forwarding option.\n");
#endif /* HAVE_AX25_FWD_STRUCT */

	return 0;
}

int main(int argc, char **argv)
{
	ax25_address callsign;
	int s, n;
	char *addr;

	if (argc == 1) {
		usage();
		return 1;
	}

	if (strncmp(argv[1], "--v", 3) == 0 || strncmp(argv[1], "-v", 2) == 0) {
		printf("axparms: %s\n", VERSION);
		return 0;
	}

	if (strncmp(argv[1], "--a", 3) == 0 || strncmp(argv[1], "-a", 2) == 0) {

		if (argc < 3) {
			usageassoc();
			return 1;
		}

		if ((s = socket(AF_AX25, SOCK_SEQPACKET, 0)) < 0) {
			perror("axparms: socket");
			return 1;
		}

		n = associate(s, argc, argv);

		close(s);

		return n;
	}

	if (strncmp(argv[1], "--f", 3) == 0 || strncmp(argv[1], "-f", 2) == 0) {
		if (argc == 2) {
			usageforward();
			return 1;
		}

		if ((s = socket(AF_AX25, SOCK_SEQPACKET, 0)) < 0) {
			perror("axparms: socket");
			return 1;
		}

		n = forward(s, argc, argv);

		close(s);

		return n;
	}

	if (strncmp(argv[1], "--r", 3) == 0 || strncmp(argv[1], "-r", 2) == 0) {
		if (argc < 3 ) {
			usageroute();
			return 1;
		}
		
		if (strcmp(argv[2], "add") != 0 && strcmp(argv[2], "del") != 0 && strcmp(argv[2], "list") != 0) {
			usageroute();
			return 1;
		}

		if (argc < 5 && strcmp(argv[2], "list") != 0) {
			usageroute();
			return 1;
		}
		
		if (ax25_config_load_ports() == 0) {
			fprintf(stderr, "axparms: no AX.25 port data configured\n");
			return 1;
		}

		if ((addr = ax25_config_get_addr(argv[3])) == NULL) {
			fprintf(stderr, "axparms: invalid port name - %s\n", argv[3]);
			return 1;
		}

		if (ax25_aton_entry(addr, callsign.ax25_call) == -1)
			return 1;

		if ((s = socket(AF_AX25, SOCK_SEQPACKET, 0)) < 0) {
			perror("axparms: socket");
			return 1;
		}

		n = routes(s, argc, argv, &callsign);

		close(s);
		
		return n;
	}

	if (strncmp(argv[1], "--s", 3) == 0 || strncmp(argv[1], "-s", 2) == 0) {
		if (argc != 4) {
			usagesetcall();
			return 1;
		}
	
		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("axparms: socket");
			return 1;
		}

		n = setifcall(s, argv[2], argv[3]);

		close(s);

		return n;
	}

	usage();

	return 1;
}
