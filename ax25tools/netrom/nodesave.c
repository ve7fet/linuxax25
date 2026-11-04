#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netax25/ax25.h>

#include <netax25/axconfig.h>
#include <netax25/nrconfig.h>

#include <netax25/procutils.h>

#include "../pathnames.h"

static char *Nrparms = "/usr/sbin/nrparms";
static char *Usage   = "Usage: nodesave [-p <path>] [-v] [<file>]\n";

/*
 * The digipeaters of a neighbour are not in struct proc_nr_neigh: read
 * them from its line of nr_neigh, after the 7 fixed fields.
 */
static void put_digis(FILE *fp, int addr)
{
	char line[256], *p;
	FILE *fn;
	int n;

	fn = fopen(PROC_NR_NEIGH_FILE, "r");
	if (fn == NULL)
		return;

	if (fgets(line, sizeof(line), fn) != NULL) {	/* header */
		while (fgets(line, sizeof(line), fn) != NULL) {
			p = strtok(line, " \n");
			if (p == NULL || atoi(p) != addr)
				continue;
			for (n = 1; (p = strtok(NULL, " \n")) != NULL; n++)
				if (n >= 7)
					fprintf(fp, " %s", p);
			break;
		}
	}

	fclose(fn);
}

/*
 * Write one route of a node. Routes that nrparms would refuse (quality 0
 * for a node learnt from traffic, a locked or out of range count, a port
 * missing from axports) are written as comments.
 */
static void save_route(FILE *fp, struct proc_nr_nodes *nop, int qual, int obs,
		       int addr, struct proc_nr_neigh *neighs)
{
	struct proc_nr_neigh *nep;
	char buf[256];
	char *port;

	nep = find_neigh(addr, neighs);
	if (nep == NULL)
		return;

	port = ax25_config_get_name(nep->dev);

	if (port == NULL || qual < 1 || qual > 255 || obs < 1 || obs > 6) {
		fprintf(fp, "# not saved: node %s via %s on %s, quality %d, count %d\n",
			nop->call, nep->call, nep->dev, qual, obs);
		return;
	}

	fprintf(fp, "%s -nodes %-9s + ", Nrparms, nop->call);
	snprintf(buf, sizeof(buf), "\"%s\"", nop->alias);
	fprintf(fp, "%-8s %-3d %d ", buf, qual, obs);
	snprintf(buf, sizeof(buf), "\"%s\"", port);
	fprintf(fp, "%-8s %s", buf, nep->call);
	put_digis(fp, nep->addr);
	fprintf(fp, "\n");
}

int main(int argc, char **argv)
{
	FILE *fp = stdout;
	struct proc_nr_nodes *nodes, *nop;
	struct proc_nr_neigh *neighs, *nep;
	char buf[256];
	int s;

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "nodesave: no AX.25 port data configured\n");
		return 1;
	}

	if (nr_config_load_ports() == 0) {
		fprintf(stderr, "nodesave: no NET/ROM port data configured\n");
		return 1;
	}

	while ((s = getopt(argc, argv, "p:v")) != -1) {
		switch (s) {
		case 'p':
			if (snprintf(buf, sizeof(buf), "%s/nrparms", optarg) >= (int)sizeof(buf)) {
				fprintf(stderr, "nodesave: path too long\n");
				return 1;
			}
			Nrparms = strdup(buf);
			break;
		case 'v':
			printf("nodesave: %s\n", VERSION);
			return 0;
		case ':':
		case '?':
			fputs(Usage, stderr);
			return 1;
		}
	}

	if ((argc - optind) > 1) {
		fputs(Usage, stderr);
		return 1;
	}

	if ((argc - optind) == 1) {
		fp = fopen(argv[optind], "w");
		if (fp == NULL) {
			fprintf(stderr, "nodesave: cannot open file %s\n", argv[optind]);
			return 1;
		}
	}

	if ((neighs = read_proc_nr_neigh()) == NULL && errno != 0) {
		perror("nodesave: read_proc_nr_neigh");
		fclose(fp);
		return 1;
	}

	if ((nodes = read_proc_nr_nodes()) == NULL && errno != 0) {
		perror("nodesave: read_proc_nr_nodes");
		free_proc_nr_neigh(neighs);
		fclose(fp);
		return 1;
	}

	fprintf(fp, "#! /bin/sh\n#\n# Locked routes:\n#\n");

	for (nep = neighs; nep != NULL; nep = nep->next) {
		char *port;

		if (!nep->lock)
			continue;

		port = ax25_config_get_name(nep->dev);
		if (port == NULL || nep->qual < 1 || nep->qual > 255) {
			fprintf(fp, "# not saved: neighbour %s on %s, quality %d\n",
				nep->call, nep->dev, nep->qual);
			continue;
		}

		fprintf(fp, "%s -routes ", Nrparms);
		snprintf(buf, sizeof(buf), "\"%s\"", port);
		fprintf(fp, "%-8s %-9s", buf, nep->call);
		put_digis(fp, nep->addr);
		fprintf(fp, " + %d\n", nep->qual);
	}

	fprintf(fp, "#\n# Nodes:\n#\n");

	/* nop->n == 0 indicates a local node with no routes. */
	for (nop = nodes; nop != NULL; nop = nop->next) {
		if (nop->n > 0)
			save_route(fp, nop, nop->qual1, nop->obs1, nop->addr1, neighs);
		if (nop->n > 1)
			save_route(fp, nop, nop->qual2, nop->obs2, nop->addr2, neighs);
		if (nop->n > 2)
			save_route(fp, nop, nop->qual3, nop->obs3, nop->addr3, neighs);
	}

	free_proc_nr_neigh(neighs);
	free_proc_nr_nodes(nodes);

	fclose(fp);

	if ((argc - optind) == 1) {
		if (chmod(argv[optind], 0755) == -1) {
			perror("nodesave: chmod");
			return 1;
		}
	}

	return 0;
}
