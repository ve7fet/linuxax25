#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>

#include <config.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netrom/netrom.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/nrconfig.h>

#include "../pathnames.h"
#include "netromd.h"

static int build_header(unsigned char *message)
{
	message[0] = NODES_SIG;
	
	strcpy(message + 1, nr_config_get_alias(NULL));
	strncat(message + 1, "       ", MNEMONIC_LEN - strlen(message + 1));

	return 7;
}

static void build_mine(int s, struct full_sockaddr_ax25 *dest, int dlen, int localval, int pause)
{
	unsigned char message[100];
	char buffer[255], *port, *p;
	FILE *fp;
	int len;

	len = build_header(message);

	if ((fp = fopen(CONF_NRPORTS_FILE, "r")) == NULL) {
		if (logging)
			syslog(LOG_ERR, "netromt: cannot open nrports file\n");
		return;
	}

	while (fgets(buffer, 255, fp) != NULL) {
		if ((p = strchr(buffer, '\n')) != NULL)
			*p = '\0';

		if (strlen(buffer) == 0 || buffer[0] == '#')
			continue;

		port = strtok(buffer, " \t");

		if (nr_config_get_addr(port) == NULL)
			continue;

		if (strcmp(nr_config_get_addr(port), nr_config_get_addr(NULL)) == 0)
			continue;

		if (ax25_aton_entry(nr_config_get_addr(port), message + len) == -1) {
			if (logging)
				syslog(LOG_ERR, "netromt: invalid callsign in nrports\n");
			fclose(fp);
			return;
		}
		len += CALLSIGN_LEN;

		strcpy(message + len, nr_config_get_alias(port));
		strncat(message + len, "       ", MNEMONIC_LEN - strlen(message + len));
		len += MNEMONIC_LEN;

		ax25_aton_entry(nr_config_get_addr(NULL), message + len);
		len += CALLSIGN_LEN;

		message[len] = localval;
		len += QUALITY_LEN;
	}

	fclose(fp);
	
	if (sendto(s, message, len, 0, (struct sockaddr *)dest, dlen) == -1) {
		if (logging)
			syslog(LOG_ERR, "netromt: sendto: %m");
	}
	
	sleep(pause);
}

static void build_others(int s, int min_obs, struct full_sockaddr_ax25 *dest, int dlen, int port, int pause)
{
	unsigned char message[300];
	FILE *fpnodes, *fpneigh;
	char nodes_buffer[90];
	char neigh_buffer[90];
	char *callsign, *mnemonic, *neighbour;
	int  which, number, quality, neigh_no, obs_count;
	int  olen, len;

	if ((fpnodes = fopen(PROC_NR_NODES_FILE, "r")) == NULL) {
		if (logging)
			syslog(LOG_ERR, "netromt: cannot open %s\n", PROC_NR_NODES_FILE);
		return;
	}

	if ((fpneigh = fopen(PROC_NR_NEIGH_FILE, "r")) == NULL) {
		if (logging)
			syslog(LOG_ERR, "netromt: cannot open %s\n", PROC_NR_NEIGH_FILE);
		fclose(fpnodes);
		return;
	}

	fgets(nodes_buffer, 90, fpnodes);

	do {
		len = olen = build_header(message);

		while (fgets(nodes_buffer, 90, fpnodes) != NULL) {
			callsign  = strtok(nodes_buffer, " ");
			mnemonic  = strtok(NULL, " ");
			which     = atoi(strtok(NULL, " "));
			number    = atoi(strtok(NULL, " "));
			quality   = atoi(strtok(NULL, " "));
			obs_count = atoi(strtok(NULL, " "));
			neigh_no  = atoi(strtok(NULL, " "));
			neighbour = NULL;
			
			if (obs_count < min_obs || quality == 0) continue;

			/* "Blank" mnemonic */
			if (strcmp(mnemonic, "*") == 0)
				mnemonic = "";

			fseek(fpneigh, 0L, SEEK_SET);

			fgets(neigh_buffer, 90, fpneigh);
			
			while (fgets(neigh_buffer, 90, fpneigh) != NULL) {
				if (atoi(strtok(neigh_buffer, " ")) == neigh_no) {
					neighbour = strtok(NULL, " ");
					break;
				}
			}

			if (neighbour == NULL) {
				if (logging)
					syslog(LOG_ERR, "netromt: corruption in nodes/neighbour matching\n");
				continue;
			}

			if (ax25_aton_entry(callsign, message + len) == -1) {
				if (logging)
					syslog(LOG_ERR, "netromt: invalid callsign '%s' in /proc/net/nr_nodes\n", callsign);
				continue;
			}
			len += CALLSIGN_LEN;

			strcpy(message + len, mnemonic);
			strncat(message + len, "       ", MNEMONIC_LEN - strlen(message + len));
			len += MNEMONIC_LEN;

			if (ax25_aton_entry(neighbour, message + len) == -1) {
				if (logging)
					syslog(LOG_ERR, "netromt: invalid callsign '%s' in /proc/net/nr_neigh\n", neighbour);
				len -= (CALLSIGN_LEN + MNEMONIC_LEN);
				continue;
			}
			len += CALLSIGN_LEN;

			message[len] = quality;		
			len += QUALITY_LEN;

			/* No room for another entry? */
			if (len + ROUTE_LEN > NODES_PACLEN)
				break;
		}

		/* Only send it if there is more that just the header */
		if (len > olen) {
			if (sendto(s, message, len, 0, (struct sockaddr *)dest, dlen) == -1) {
				if (logging)
					syslog(LOG_ERR, "netromt: sendto: %m");
			}
			
			sleep(pause);
		}

	/* If the packet was not full then we are done */
	} while (len + ROUTE_LEN > NODES_PACLEN);

	fclose(fpnodes);
	fclose(fpneigh);
}

void transmit_nodes(int localval, int pause)
{
	struct full_sockaddr_ax25 dest;
	struct full_sockaddr_ax25 src;
	int s, dlen, slen;
	char path[25], *addr;
	int i;

	switch (fork()) {
		case 0:
			break;
		case -1:
			if (logging)
				syslog(LOG_ERR, "netromt: fork: %m\n");
			return;
		default:
			return;
	}

	dlen = ax25_aton("NODES", &dest);

	for (i = 0; i < port_count; i++) {

		addr = ax25_config_get_addr(port_list[i].port);

		if (addr == NULL) continue;

		if (debug && logging)
			syslog(LOG_DEBUG, "transmitting NODES broadcast on port %s\n", port_list[i].port);

		sprintf(path, "%s %s", nr_config_get_addr(NULL), addr);

		ax25_aton(path, &src);
		slen = sizeof(struct full_sockaddr_ax25);

		if ((s = socket(AF_AX25, SOCK_DGRAM, NETROM_PID)) < 0) {
			if (logging)
				syslog(LOG_ERR, "netromt: socket: %m");
			continue;
		}

		if (bind(s, (struct sockaddr *)&src, slen) == -1) {
			if (logging)
				syslog(LOG_ERR, "netromt: bind: %m");
			close(s);
			continue;
		}

		build_mine(s, &dest, dlen, localval, pause);

		if (port_list[i].verbose)
			build_others(s, port_list[i].minimum_obs, &dest, dlen, i, pause);

		close(s);
	}

	if ((s = socket(AF_NETROM, SOCK_SEQPACKET, 0)) < 0) {
		if (logging)
			syslog(LOG_ERR, "netromt: socket: %m");
		exit(1);
	}

	if (ioctl(s, SIOCNRDECOBS, &i) == -1) {
		if (logging)
			syslog(LOG_ERR, "netromt: SIOCNRDECOBS: %m");
		exit(1);
	}

	close(s);

	exit(0);
}
