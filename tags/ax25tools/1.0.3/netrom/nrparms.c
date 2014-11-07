#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

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

char nodes_usage[]  = "usage: nrparms -nodes nodecall +|- ident quality count port neighbour [digicall...]\n";
char routes_usage[] = "usage: nrparms -routes port nodecall [digicall...] +|- pathquality\n";

void nodes(int s, char *nodecall, char *op, char *ident, int quality, int count, char *port, char *neighbour, char *digis[])
{
	struct nr_route_struct nr_node;
	char *p, *q, *dev;
	int i;

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "nrparms: nodes: no AX.25 ports configured\n");
		exit(1);
	}

	nr_node.type   = NETROM_NODE;
	nr_node.ndigis = 0;

	if (op[0] != '+' && op[0] != '-') {
		fprintf(stderr, "nrparms: nodes: invalid operation %s\n", op);
		close(s);
		exit(1);
	}

	if (quality < 1 || quality > 255) {
		fprintf(stderr, "nrparms: nodes: invalid quality %d\n", quality);
		close(s);
		exit(1);
	}

	if (count < 1 || count > 6) {
		fprintf(stderr, "nrparms: nodes: invalid obs count %d\n", count);
		close(s);
		exit(1);
	}

	if (ax25_aton_entry(nodecall, nr_node.callsign.ax25_call) != 0) {
		fprintf(stderr, "nrparms: nodes: invalid callsign %s\n", nodecall);
		close(s);
		exit(1);
	}

	if (strlen(ident) > 7) {
		fprintf(stderr, "nrparms: nodes: invalid length of mnemonic %s\n", ident);
		close(s);
		exit(1);
	}
		
	if (strcmp(ident, "*") != 0) {
		for (p = ident, q = nr_node.mnemonic; *p != '\0'; p++, q++)
			*q = toupper(*p);
		*q = '\0';
		
		if (strspn(nr_node.mnemonic, "&#-_/ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != strlen(nr_node.mnemonic)) {
			fprintf(stderr, "nrparms: nodes: invalid ident %s\n", ident);
			close(s);
			exit(1);
		}
	} else {
		strcpy(nr_node.mnemonic, "");
	}

	if (ax25_aton_entry(neighbour, nr_node.neighbour.ax25_call) != 0) {
		fprintf(stderr, "nrparms: nodes: invalid callsign %s\n", neighbour);
		close(s);
		exit(1);
	}

	for (i = 0; i < AX25_MAX_DIGIS && digis[i] != NULL; i++) {
		if (ax25_aton_entry(digis[i], nr_node.digipeaters[i].ax25_call) != 0) {
			fprintf(stderr, "nrparms: invalid callsign %s\n", digis[i]);
			close(s);
			exit(1);
		}
		nr_node.ndigis++;
	}

	if ((dev = ax25_config_get_dev(port)) == NULL) {
		fprintf(stderr, "nrparms: nodes: invalid port name - %s\n", port);
		close(s);
		exit(1);
	}

	strcpy(nr_node.device, dev);
	
	nr_node.quality   = quality;
	nr_node.obs_count = count;

	if (op[0] == '+') {
		if (ioctl(s, SIOCADDRT, &nr_node) == -1) {
			perror("nrparms: SIOCADDRT");
			close(s);
			exit(1);
		}
	} else {
		if (ioctl(s, SIOCDELRT, &nr_node) == -1) {
			perror("nrparms: SIOCDELRT");
			close(s);
			exit(1);
		}
	}
}

void routes(int s, char *port, char *nodecall, char *rest[])
{
	struct nr_route_struct nr_neigh;
	char *dev, *op;
	int i, quality;

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "nrparms: routes: no AX.25 ports configured\n");
		exit(1);
	}

	nr_neigh.type   = NETROM_NEIGH;
	nr_neigh.ndigis = 0;

	for (i = 0; i < AX25_MAX_DIGIS && rest[i][0] != '-' && rest[i][0] != '+'; i++) {
		if (ax25_aton_entry(rest[i], nr_neigh.digipeaters[i].ax25_call) != 0) {
			fprintf(stderr, "nrparms: routes: invalid callsign %s\n", rest[i]);
			close(s);
			exit(1);
		}
		nr_neigh.ndigis++;
	}

	op      = rest[i + 0];
	quality = atoi(rest[i + 1]);

	if (op[0] != '+' && op[0] != '-') {
		fprintf(stderr, "nrparms: routes: invalid operation %s\n", op);
		close(s);
		exit(1);
	}

	if (quality < 1 || quality > 255) {
		fprintf(stderr, "nrparms: routes: invalid quality %d\n", quality);
		close(s);
		exit(1);
	}

	if ((dev = ax25_config_get_dev(port)) == NULL) {
		fprintf(stderr, "nrparms: routes: invalid port name - %s\n", port);
		close(s);
		exit(1);
	}

	strcpy(nr_neigh.device, dev);

	if (ax25_aton_entry(nodecall, nr_neigh.callsign.ax25_call) != 0) {
		fprintf(stderr, "nrparms: routes: invalid callsign %s\n", nodecall);
		close(s);
		exit(1);
	}

	nr_neigh.quality   = quality;

	if (op[0] == '+') {
		if (ioctl(s, SIOCADDRT, &nr_neigh) == -1) {
			perror("nrparms: SIOCADDRT");
			close(s);
			exit(1);
		}
	} else {
		if (ioctl(s, SIOCDELRT, &nr_neigh) == -1) {
			perror("nrparms: SIOCDELRT");
			close(s);
			exit(1);
		}
	}
}

int main(int argc, char *argv[])
{
	int s;
	
	if (argc == 1) {
		fprintf(stderr, "usage: nrparms -nodes|-routes|-version ...\n");
		return 1;
	}

	if (strncmp(argv[1], "-v", 2) == 0) {
		printf("nrparms: %s\n", VERSION);
		return 0;
	}
	
	if ((s = socket(AF_NETROM, SOCK_SEQPACKET, 0)) < 0) {
		perror("nrparms: socket");
		return 1;
	}
	
	if (strncmp(argv[1], "-n", 2) == 0) {
		if (argc < 9) {
			fprintf(stderr, nodes_usage);
			close(s);
			return 1;
		}
		nodes(s, argv[2], argv[3], argv[4], atoi(argv[5]), atoi(argv[6]), argv[7], argv[8], argv + 9);
		close(s);
		return 0;
	}

	if (strncmp(argv[1], "-r", 2) == 0) {
		if (argc < 6) {
			fprintf(stderr, routes_usage);
			close(s);
			return 1;
		}
		routes(s, argv[2], argv[3], argv + 4);
		close(s);
		return 0;
	}

	fprintf(stderr, "usage: nrparms -nodes|-routes|-version ...\n");
	
	close(s);
	
	return 1;
}
