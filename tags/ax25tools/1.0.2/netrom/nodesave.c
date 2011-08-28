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

static char *Nrparms = "/usr/sbin/nrparms";
static char *Usage   = "Usage: nodesave [-p <path>] [-v] [<file>]\n";

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
				sprintf(buf, "%s/nrparms", optarg);
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
		if ((fp = fopen(argv[optind], "w")) == NULL) {
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
		if (nep->lock) {
			fprintf(fp, "%s -routes ", Nrparms);
			sprintf(buf, "\"%s\"", ax25_config_get_name(nep->dev));
			fprintf(fp, "%-8s %-9s + %d\n",
				buf,
				nep->call,
				nep->qual);
		}
	}

	fprintf(fp, "#\n# Nodes:\n#\n");

	for (nop = nodes; nop != NULL; nop = nop->next) {
		/*
		 * nop->n == 0 indicates a local node with no routes.
		 */
		if (nop->n > 0 && (nep = find_neigh(nop->addr1, neighs)) != NULL) {
			fprintf(fp, "%s -nodes %-9s + ",
				Nrparms,
				nop->call);
			sprintf(buf, "\"%s\"", nop->alias);
			fprintf(fp, "%-8s %-3d %d ",
				buf,
				nop->qual1,
				nop->obs1);
			sprintf(buf, "\"%s\"", ax25_config_get_name(nep->dev));
			fprintf(fp, "%-8s %s\n",
				buf,
				nep->call);
		}

		if (nop->n > 1 && (nep = find_neigh(nop->addr2, neighs)) != NULL) {
			fprintf(fp, "%s -nodes %-9s + ",
				Nrparms,
				nop->call);
			sprintf(buf, "\"%s\"", nop->alias);
			fprintf(fp, "%-8s %-3d %d ",
				buf,
				nop->qual2,
				nop->obs2);
			sprintf(buf, "\"%s\"", ax25_config_get_name(nep->dev));
			fprintf(fp, "%-8s %s\n",
				buf,
				nep->call);
		}

		if (nop->n > 2 && (nep = find_neigh(nop->addr3, neighs)) != NULL) {
			fprintf(fp, "%s -nodes %-9s + ",
				Nrparms,
				nop->call);
			sprintf(buf, "\"%s\"", nop->alias);
			fprintf(fp, "%-8s %-3d %d ",
				buf,
				nop->qual3,
				nop->obs3);
			sprintf(buf, "\"%s\"", ax25_config_get_name(nep->dev));
			fprintf(fp, "%-8s %s\n",
				buf,
				nep->call);
		}
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
