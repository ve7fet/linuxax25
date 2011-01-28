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
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/rsconfig.h>

#include "../pathnames.h"

char nodes_usage[]  = "usage: rsparms -node add|del nodeaddr[/mask] port neighbour [digis...]\n       rsparms -node list\n";

/* print the Rose neighbour whose number is supplied */
void printnb(char *neigh)
{
	FILE* fp;
	char addr[10], callsign[10], port[10], digi[10];
	char buff[80];
	int args;

	if ((fp=fopen(PROC_RS_NEIGH_FILE,"r"))==NULL) {
		fprintf(stderr,"rsparms: Couldn't open %s file\n",PROC_RS_NEIGH_FILE);
		exit(1);
	}

	while(fgets(buff, 80, fp)) {
		addr[0]=(char)NULL; callsign[0]=(char)NULL;
		port[0]=(char)NULL; digi[0]=(char)NULL;

		args=sscanf(buff,"%9s %9s %9s %*s %*s %*s %*s %*s %*s %9s",addr,callsign,port,digi);

		if (args==4) {		/* We have a digi */
			if (strcmp(addr,neigh)==0)
				printf("%-6s %-9s via %-9s",port,callsign,digi);
		} else
		if (args==3) {		/* No digi */
			if (strcmp(addr,neigh)==0)
				printf("%-6s %-9s",port,callsign);
		}
	}
	fclose(fp);
}

void nodes(int s, int argc, char *argv[])
{
	struct rose_route_struct rs_node;
	char *dev;
	char *mask;
	char nodeaddr[11];
	int i, n;

	FILE *fn;
	char address[12], rmask[5], neigh1[10], neigh2[10], neigh3[10];
	char buff[80];
	int args;

	if (argc < 3) {
		fprintf(stderr, nodes_usage);
		exit(1);
	}

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "rsparms: nodes: no AX.25 ports configured\n");
		exit(1);
	}

	if (argv[2][0] != 'a' && argv[2][0] != 'd' && argv[2][0] != 'l') {
		fprintf(stderr, "rsparms: nodes: invalid operation %s\n", argv[2]);
		close(s);
		exit(1);
	}

	if (argv[2][0] == 'l') {
		if ((fn=fopen(PROC_RS_NODES_FILE,"r"))==NULL) {
			fprintf(stderr,"rsparms: Couldn't open %s file\n",PROC_RS_NODES_FILE);
			exit(1);
		}

		while(fgets(buff,80,fn)) {
			args=sscanf(buff,"%10s %4s %*s %9s %9s %9s",address, rmask, neigh1, neigh2, neigh3);
			if (strcmp(address,"address")==0)
				continue;

			if (args>2) {
				printf("%10s/%4s -> ",address,rmask);
				printnb(neigh1);
				putchar('\n');
			}
			if (args>3) {
				printf("%15s -> ","");
				printnb(neigh2);
				putchar('\n');
			}
			if (args>4) {
				printf("%15s -> ","");
				printnb(neigh3);
				putchar('\n');
			}
		}
		exit(0);
	}

	if (argc < 6) {
		fprintf(stderr, nodes_usage);
		exit(1);
	}

	if ((mask = strchr(argv[3], '/')) != NULL) {
		*mask= (char)NULL;
		mask++;

		if (sscanf(mask, "%hd", &rs_node.mask) != 1) {
			fprintf(stderr, "rsparms: nodes: no mask supplied!\n");
			close(s);
			exit(1);
		}

		if (rs_node.mask > 10) {
			fprintf(stderr, "rsparms: nodes: invalid mask size: %s\n", mask);;
			close(s);
			exit(1);
		}
	} else {
		rs_node.mask = 10;
	}

	/* Make all non significant digits equal to zero */
	strcpy(nodeaddr, argv[3]);

	for (i = rs_node.mask; i < 10; i++)
		nodeaddr[i] = '0';

	nodeaddr[i] = '\0';

	if (rose_aton(nodeaddr, rs_node.address.rose_addr) != 0) {
		fprintf(stderr, "rsparms: nodes: invalid address %s\n", nodeaddr);
		close(s);
		exit(1);
	}

	if ((dev = ax25_config_get_dev(argv[4])) == NULL) {
		fprintf(stderr, "rsparms: nodes: invalid port name - %s\n", argv[4]);
		close(s);
		exit(1);
	}

	strcpy(rs_node.device, dev);

	if (ax25_aton_entry(argv[5], rs_node.neighbour.ax25_call) != 0) {
		fprintf(stderr, "rsparms: nodes: invalid callsign %s\n", argv[5]);
		close(s);
		exit(1);
	}

	for (i = 6, n = 0; i < argc && n < AX25_MAX_DIGIS; i++, n++) {
		if (ax25_aton_entry(argv[i], rs_node.digipeaters[n].ax25_call) != 0) {
			fprintf(stderr, "rsparms: nodes: invalid callsign %s\n", argv[i]);
			close(s);
			exit(1);
		}
	}

	rs_node.ndigis = n;

	if (argv[2][0] == 'a') {
		if (ioctl(s, SIOCADDRT, &rs_node) == -1) {
			perror("rsparms: SIOCADDRT");
			close(s);
			exit(1);
		}
	} else {
		if (ioctl(s, SIOCDELRT, &rs_node) == -1) {
			perror("rsparms: SIOCDELRT");
			close(s);
			exit(1);
		}
	}
}




int main(int argc, char *argv[])
{
	ax25_address rose_call;
	int s = 0;
	
	if (argc == 1) {
		fprintf(stderr, "usage: rsparms -call|-nodes|-version ...\n");
		return 1;
	}

	if (strncmp(argv[1], "-v", 2) == 0) {
		printf("rsparms: %s\n", VERSION);
		return 0;
	}

	if (strncmp(argv[1], "-c", 2) == 0) {
		if (argc < 3) {
			fprintf(stderr, "usage: rsparms -call <callsign>|none\n");
			return 1;
		}

		if (strcmp(argv[2], "none") != 0) {
			if (ax25_aton_entry(argv[2], rose_call.ax25_call) == -1) {
				fprintf(stderr, "rsparms: invalid callsign %s\n", argv[2]);
				return 1;
			}
		} else {
			rose_call = null_ax25_address;
		}

		if ((s = socket(AF_ROSE, SOCK_SEQPACKET, 0)) < 0) {
			perror("rsparms: socket");
			return 1;
		}
		
		if (ioctl(s, SIOCRSL2CALL, &rose_call) == -1) {
			perror("rsparms: ioctl");
			close(s);
			return 1;
		}

		close(s);

		return 0;
	}

	if (strncmp(argv[1], "-n", 2) == 0) {

		if ((s = socket(AF_ROSE, SOCK_SEQPACKET, 0)) < 0) {
			perror("rsparms: socket");
			return 1;
		}
		nodes(s, argc, argv);
		close(s);
		return 0;
	}

	fprintf(stderr, "usage: rsparms -call|-nodes|-version ...\n");
	
	close(s);
	
	return 1;
}
