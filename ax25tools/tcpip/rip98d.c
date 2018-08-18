
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>

#include <netax25/daemon.h>


#include "../pathnames.h"
#include "rip98d.h"

struct dest_struct dest_list[50];

int dest_count;

int debug            = FALSE;
int route_restrict   = FALSE;
int logging          = FALSE;

struct route_struct *first_route;

static void terminate(int sig)
{
	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}

	exit(0);
}

unsigned int mask2bits(unsigned int mask)
{
	mask = ~mask;

	if (!mask)
		return 32;

	return __builtin_clz(mask);
}

unsigned int bits2mask(unsigned int bits)
{
	if (!bits)
		return 0;

	return ~0 << (sizeof(unsigned int) * 8 - bits);
}

static unsigned int hex2int(char *buffer)
{
	return strtoul(buffer, NULL, 16);
}

static int read_routes(void)
{
	struct route_struct *route, *temp;
	char buffer[1023], iface[16], net_addr[64], gate_addr[64], mask_addr[64];
	int n, refcnt, use, metric, mss, window;
	unsigned int iflags;
	struct in_addr address;
	unsigned long int netmask;
	unsigned long int network;
	FILE *fp;

	if (first_route != NULL) {
		route = first_route;

		while (route != NULL) {
			temp = route->next;
			free(route);
			route = temp;
		}

		first_route = NULL;
	}

	fp = fopen(PROC_IP_ROUTE_FILE, "r");
	if (fp == NULL) {
		if (logging)
			syslog(LOG_ERR, "error cannot open %s\n", PROC_IP_ROUTE_FILE);
		return FALSE;
	}

	while (fgets(buffer, 1023, fp) != NULL) {
		n = sscanf(buffer, "%s %s %s %X %d %d %d %s %d %d\n",
			iface, net_addr, gate_addr, &iflags, &refcnt, &use,
			&metric, mask_addr, &mss, &window);

		if (n != 10)
			continue;

		address.s_addr = hex2int(net_addr);
		netmask        = mask2bits(ntohl(hex2int(mask_addr)));

		network = inet_netof(address);

		if (network == 0 || network == 127) {
			if (debug && logging)
				syslog(LOG_DEBUG, "rejecting route to %s/%ld - should not be propogated\n", inet_ntoa(address), netmask);
			continue;
		}

		if (route_restrict) {
			if (inet_netof(address) != 44) {
				if (debug && logging)
					syslog(LOG_DEBUG, "rejecting route to %s/%ld - not ampr.org\n", inet_ntoa(address), netmask);
				continue;
			}
		}

		route = malloc(sizeof(struct route_struct));
		if (route == NULL) {
			if (logging)
				syslog(LOG_ERR, "out of memory !\n");
			return FALSE;
		}

		route->addr   = address;
		route->bits   = netmask;
		route->metric = metric;
		route->action = (iflags & RTF_DYNAMIC) ? ORIG_ROUTE : FIXED_ROUTE;

		route->next = first_route;
		first_route = route;
	}

	fclose(fp);

	return TRUE;
}

static int load_dests(void)
{
	struct hostent *host;
	char buffer[255], *s;
	FILE *fp;

	fp = fopen(CONF_RIP98D_FILE, "r");
	if (fp == NULL) {
		fprintf(stderr, "rip98d: cannot open config file\n");
		return FALSE;
	}

	while (fgets(buffer, 255, fp) != NULL) {
		s = strchr(buffer, '\n');
		if (s != NULL) *s = '\0';

		host = gethostbyname(buffer);
		if (host == NULL) {
			fprintf(stderr, "rip98d: cannot resolve name %s\n", buffer);
			fclose(fp);
			return FALSE;
		}

		memcpy(&dest_list[dest_count].dest_addr, host->h_addr,
		       host->h_length);
		dest_count++;
	}

	fclose(fp);

	if (dest_count == 0)
		return FALSE;

	return TRUE;
}

int main(int argc, char **argv)
{
	int s, i;
	struct sockaddr_in loc_addr;
	struct timeval timeout;
	time_t timenow, timelast = 0;
	int interval = 3600;
	fd_set fdset;

	if (!load_dests()) {
		fprintf(stderr, "rip98d: no destination routers defined\n");
		return 1;
	}

	while ((i = getopt(argc, argv, "dlrt:v")) != -1) {
		switch (i) {
		case 'd':
			debug = TRUE;
			break;
		case 'l':
			logging = TRUE;
			break;
		case 'r':
			route_restrict = TRUE;
			break;
		case 't':
			interval = atoi(optarg) * 60;
			if (interval < 60 || interval > 7200) {
				fprintf(stderr, "rip98d: invalid time interval\n");
				return 1;
			}
			break;
		case 'v':
			printf("rip98d: %s\n", VERSION);
			return 0;
		case ':':
			fprintf(stderr, "rip98d: invalid time interval\n");
			return 1;
		case '?':
			fprintf(stderr, "usage: rip98d [-d] [-l] [-r] [-t interval] [-v]\n");
			return 1;
		}
	}

	signal(SIGTERM, terminate);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("rip98d: socket");
		return 1;
	}

	memset(&loc_addr, 0, sizeof(loc_addr));
	loc_addr.sin_family      = AF_INET;
	loc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	loc_addr.sin_port        = htons(RIP_PORT);

	if (bind(s, &loc_addr, sizeof(loc_addr)) < 0) {
		perror("rip98d: bind");
		close(s);
		return 1;
	}

	if (!daemon_start(FALSE)) {
		fprintf(stderr, "rip98d: cannot become a daemon\n");
		close(s);
		return 1;
	}

	if (logging) {
		openlog("rip98d", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "starting");
	}

	for (;;) {
		FD_ZERO(&fdset);
		FD_SET(s, &fdset);

		timeout.tv_sec  = 60;
		timeout.tv_usec = 0;

		select(s + 1, &fdset, NULL, NULL, &timeout);

		if (!read_routes()) {
			if (logging)
				closelog();
			return 1;
		}

		if (FD_ISSET(s, &fdset))
			receive_routes(s);

		time(&timenow);

		if ((timenow - timelast) > interval) {
			timelast = timenow;
			if (first_route != NULL)
				transmit_routes(s);
		}
	}
}
