#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>

#include <config.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#ifdef __GLIBC__ 
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif

#include <netax25/ax25.h>
#include <netrom/netrom.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include "../pathnames.h"

#include "netromd.h"

extern int compliant;

static int validcallsign(ax25_address *a)
{
	char c;
	int n, end = 0;

	for (n = 0; n < 6; n++) {
		c = (a->ax25_call[n] >> 1) & 0x7F;

		if (!end && (isupper(c) || isdigit(c)))
			continue;

		if (c == ' ') {
			end = 1;
			continue;
		}

		return FALSE;
        }

	return TRUE;
}

static int validmnemonic(char *mnemonic)
{
	if (compliant) {
		if (strspn(mnemonic, "#_&-/ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == strlen(mnemonic))
			return TRUE;
	} else {
		if (strspn(mnemonic, "#_&-/abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == strlen(mnemonic))
			return TRUE;
	}

	return FALSE;
}

static int add_node(int s, unsigned char *buffer, struct nr_route_struct *nr_node, int quality, int index)
{
	ax25_address best_neighbour;
	int best_quality;
	char *p;

	memcpy(&nr_node->callsign, buffer + 0,  CALLSIGN_LEN);
	memcpy(nr_node->mnemonic,  buffer + 7,  MNEMONIC_LEN);
	memcpy(&best_neighbour,    buffer + 13, CALLSIGN_LEN);

	best_quality = buffer[20];

	nr_node->mnemonic[MNEMONIC_LEN] = '\0';
	if ((p = strchr(nr_node->mnemonic, ' ')) != NULL)
		*p = '\0';

	if (!validcallsign(&nr_node->callsign)) {
		if (debug && logging)
			syslog(LOG_DEBUG, "netromr: add_node: invalid node callsign - %s", ax25_ntoa(&nr_node->callsign));
		return -1;
	}

	if (!validmnemonic(nr_node->mnemonic)) {
		if (debug && logging)
			syslog(LOG_DEBUG, "netromr: add_node: invalid mnemonic - %s", nr_node->mnemonic);
		return -1;
	}

	if (!validcallsign(&best_neighbour)) {
		if (debug && logging)
			syslog(LOG_DEBUG, "netromr: add_node: invalid best neighbour callsign - %s", ax25_ntoa(&best_neighbour));
		return -1;
	}

	if (ax25_cmp(&my_call, &best_neighbour) == 0) {
		if (debug && logging)
			syslog(LOG_DEBUG, "netromr: add_node: route to me");
		return FALSE;
	}

	if (best_quality < port_list[index].worst_qual) {
		if (debug && logging)
			syslog(LOG_DEBUG, "netromr: add_node: quality less than worst_qual");
		return FALSE;
	}

	nr_node->quality = ((quality * best_quality) + 128) / 256;

	/* log this only when logging verbosely */
	if (debug > 1 && logging) {
		syslog(LOG_DEBUG, "Node update: %s:%s",
		       ax25_ntoa(&nr_node->callsign), nr_node->mnemonic);
		syslog(LOG_DEBUG, "Neighbour: %s device: %s",
		       ax25_ntoa(&nr_node->neighbour), nr_node->device);
		syslog(LOG_DEBUG, "Quality: %d obs: %d ndigis: %d",
		       nr_node->quality, nr_node->obs_count, nr_node->ndigis);
	}

	if (ioctl(s, SIOCADDRT, nr_node) == -1) {
		if (logging)
			syslog(LOG_ERR, "netromr: SIOCADDRT: %m");
		return -1;
	}

	return TRUE;
}

void receive_nodes(unsigned char *buffer, int length, ax25_address *neighbour, int index)
{
	struct nr_route_struct nr_node;
	char neigh_buffer[90], *portcall, *p;
	FILE *fp;
	int s;
	int quality, obs_count, qual, lock;
	char *addr, *callsign, *device;

	if (!validcallsign(neighbour)) {
		if (debug && logging)
			syslog(LOG_DEBUG, "rejecting frame, invalid neighbour callsign - %s\n", ax25_ntoa(neighbour));
		return;
	}

	nr_node.type   = NETROM_NODE;
	nr_node.ndigis = 0;

	sprintf(neigh_buffer, "%s/obsolescence_count_initialiser", PROC_NR_SYSCTL_DIR);

	if ((fp = fopen(neigh_buffer, "r")) == NULL) {
		if (logging)
			syslog(LOG_ERR, "netromr: cannot open %s\n", neigh_buffer);
		return;
	}

	fgets(neigh_buffer, 90, fp);
	
	obs_count = atoi(neigh_buffer);

	fclose(fp);

	if ((s = socket(AF_NETROM, SOCK_SEQPACKET, 0)) < 0) {
		if (logging)
			syslog(LOG_ERR, "netromr: socket: %m");
		return;
	}

	if ((fp = fopen(PROC_NR_NEIGH_FILE, "r")) == NULL) {
		if (logging)
			syslog(LOG_ERR, "netromr: cannot open %s\n", PROC_NR_NEIGH_FILE);
		close(s);
		return;
	}

	fgets(neigh_buffer, 90, fp);
	
	portcall = ax25_ntoa(neighbour);

	quality = port_list[index].default_qual;

	while (fgets(neigh_buffer, 90, fp) != NULL) {
		addr     = strtok(neigh_buffer, " ");
		callsign = strtok(NULL, " ");
		device   = strtok(NULL, " ");
		qual     = atoi(strtok(NULL, " "));
		lock     = atoi(strtok(NULL, " "));

		if (strcmp(callsign, portcall) == 0 &&
		    strcmp(device, port_list[index].device) == 0 &&
		    lock == 1) {
			quality = qual;
			break;
		}
	}

	fclose(fp);

	nr_node.callsign    = *neighbour;
	memcpy(nr_node.mnemonic, buffer, MNEMONIC_LEN);
	nr_node.mnemonic[MNEMONIC_LEN] = '\0';

	if ((p = strchr(nr_node.mnemonic, ' ')) != NULL)
		*p = '\0';

	if (!validmnemonic(nr_node.mnemonic)) {
		if (debug && logging)
			syslog(LOG_DEBUG, "rejecting route, invalid mnemonic - %s\n", nr_node.mnemonic);
	} else {
		nr_node.neighbour   = *neighbour;
		strcpy(nr_node.device, port_list[index].device);
		nr_node.quality     = quality;
		nr_node.obs_count   = obs_count;

		if (ioctl(s, SIOCADDRT, &nr_node) == -1) {
			if (logging)
				syslog(LOG_ERR, "netromr: SIOCADDRT: %m");
			close(s);
			return;
		}
	}

	buffer += MNEMONIC_LEN;
	length -= MNEMONIC_LEN;

	while (length >= ROUTE_LEN) {
		nr_node.neighbour   = *neighbour;
		strcpy(nr_node.device, port_list[index].device);
		nr_node.obs_count   = obs_count;

		if (add_node(s, buffer, &nr_node, quality, index) == -1) {
			close(s);
			return;
		}

		buffer += ROUTE_LEN;
		length -= ROUTE_LEN;
	}

	close(s);
}
