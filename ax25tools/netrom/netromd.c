#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include <config.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netpacket/packet.h>

#include <net/if.h>

#include <net/ethernet.h>

#include <netax25/ax25.h>
#include <netrom/netrom.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/daemon.h>
#include <netax25/nrconfig.h>

#include "../pathnames.h"
#include "netromd.h"

struct port_struct port_list[20];

int port_count = FALSE;
int compliant  = FALSE;
int logging    = FALSE;
int sock, debug;

ax25_address my_call;
ax25_address node_call;

static void terminate(int sig)
{
	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}

	exit(0);
}

static int bcast_config_load_ports(void)
{
	char buffer[255], port[32], *s;
	struct ifreq ifr;
	FILE *fp;

	fp = fopen(CONF_NETROMD_FILE, "r");
	if (fp == NULL) {
		fprintf(stderr, "netromd: cannot open config file\n");
		return -1;
	}

	while (fgets(buffer, 255, fp) != NULL) {
		s = strchr(buffer, '\n');
		if (s != NULL)
			*s = '\0';

		if (strlen(buffer) == 0 || buffer[0] == '#')
			continue;

		if (sscanf(buffer, "%s %d %d %d %d",
				port,
				&port_list[port_count].minimum_obs,
				&port_list[port_count].default_qual,
				&port_list[port_count].worst_qual,
				&port_list[port_count].verbose) == -1) {
			fprintf(stderr, "netromd: unable to parse: %s", buffer);
			return -1;
		}

		if (ax25_config_get_addr(port) == NULL) {
			fprintf(stderr, "netromd: invalid port name - %s\n", port);
			return -1;
		}

		port_list[port_count].port   = strdup(port);
		port_list[port_count].device = strdup(ax25_config_get_dev(port_list[port_count].port));

		strncpy(ifr.ifr_name, port_list[port_count].device, sizeof(ifr.ifr_name));
		if (ioctl(sock, SIOCGIFINDEX, &ifr)) {
			fprintf(stderr, "netromd: SIOCGIFINDEX failed for device %s.\n", port_list[port_count].device);
			return -1;
		}
		port_list[port_count].index = ifr.ifr_ifindex;

		if (port_list[port_count].minimum_obs < 0 || port_list[port_count].minimum_obs > 6) {
			fprintf(stderr, "netromd: invalid minimum obsolescence\n");
			return -1;
		}

		if (port_list[port_count].default_qual < 0 || port_list[port_count].default_qual > 255) {
			fprintf(stderr, "netromd: invalid default quality\n");
			return -1;
		}

		if (port_list[port_count].worst_qual < 0 || port_list[port_count].worst_qual > 255) {
			fprintf(stderr, "netromd: invalid worst quality\n");
			return -1;
		}

		if (port_list[port_count].verbose != 0 && port_list[port_count].verbose != 1) {
			fprintf(stderr, "netromd: invalid verbose setting\n");
			return -1;
		}

		port_count++;
	}

	fclose(fp);

	if (port_count == 0)
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	unsigned char buffer[512];
	int size, i;
	struct sockaddr_ll sa;
	socklen_t asize;
	struct timeval timeout;
	time_t timenow, timelast;
	int interval = 3600;
	int localval = 255;
	int pause    = 5;
	fd_set fdset;

	time(&timelast);

	while ((i = getopt(argc, argv, "cdilp:q:t:v")) != -1) {
		switch (i) {
		case 'c':
			compliant = TRUE;
			break;
		case 'd':
			debug++;
			break;
		case 'i':
			timelast = 0;
			break;
		case 'l':
			logging = TRUE;
			break;
		case 'p':
			pause = atoi(optarg);
			if (pause < 1 || pause > 120) {
				fprintf(stderr, "netromd: invalid pause value\n");
				return 1;
			}
			break;
		case 'q':
			localval = atoi(optarg);
			if (localval < 10 || localval > 255) {
				fprintf(stderr, "netromd: invalid local quality\n");
				return 1;
			}
			break;
		case 't':
			interval = atoi(optarg) * 60;
			if (interval < 60 || interval > 7200) {
				fprintf(stderr, "netromd: invalid time interval\n");
				return 1;
			}
			break;
		case 'v':
			printf("netromd: %s\n", VERSION);
			return 0;
		case '?':
		case ':':
			fprintf(stderr, "usage: netromd [-d] [-i] [-l] [-q quality] [-t interval] [-v]\n");
			return 1;
		}
	}

	signal(SIGTERM, terminate);

	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_AX25));
	if (sock == -1) {
		perror("netromd: socket");
		return 1;
	}

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "netromd: no AX.25 ports defined\n");
		return 1;
	}

	if (nr_config_load_ports() == 0) {
		fprintf(stderr, "netromd: no NET/ROM ports defined\n");
		return 1;
	}

	if (bcast_config_load_ports() == -1) {
		fprintf(stderr, "netromd: no NET/ROM broadcast ports defined\n");
		return 1;
	}

	ax25_aton_entry(nr_config_get_addr(NULL), (char *)&my_call);
	ax25_aton_entry("NODES", (char *)&node_call);

	if (!daemon_start(TRUE)) {
		fprintf(stderr, "netromd: cannot become a daemon\n");
		return 1;
	}

	if (logging) {
		openlog("netromd", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "starting");
	}

	for (;;) {
		FD_ZERO(&fdset);
		FD_SET(sock, &fdset);

		timeout.tv_sec  = 30;
		timeout.tv_usec = 0;

		if (select(sock + 1, &fdset, NULL, NULL, &timeout) == -1)
			continue;		/* Signal received ? */

		if (FD_ISSET(sock, &fdset)) {
			asize = sizeof(sa);

			size = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &sa, &asize);
			if (size == -1) {
				if (logging) {
					syslog(LOG_ERR, "recvfrom: %m");
					closelog();
				}
				return 1;
			}

			if (ax25_cmp((ax25_address *)(buffer + 1), &node_call) == 0 &&
			    ax25_cmp((ax25_address *)(buffer + 8), &my_call)   != 0 &&
			    buffer[16] == NETROM_PID && buffer[17] == NODES_SIG) {
				for (i = 0; i < port_count; i++) {
					if (port_list[i].index == sa.sll_ifindex) {
						if (debug && logging)
							syslog(LOG_DEBUG, "receiving NODES broadcast on port %s from %s\n", port_list[i].port, ax25_ntoa((ax25_address *)(buffer + 8)));
						receive_nodes(buffer + 18, size - 18, (ax25_address *)(buffer + 8), i);
						break;
					}
				}
			}
		}

		time(&timenow);

		if ((timenow - timelast) > interval) {
			timelast = timenow;
			transmit_nodes(localval, pause);
		}
	}
}
