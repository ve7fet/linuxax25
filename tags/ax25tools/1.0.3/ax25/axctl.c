#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>

#include <config.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>

int main(int argc, char **argv)
{
	struct ax25_ctl_struct ax25_ctl;
	char *addr;
	int s;
	
	if (argc == 2 && strncmp(argv[1], "-v", 2) == 0) {
		printf("axctl: %s\n", VERSION);
		return 0;
	}

	if (argc < 5) {
		fprintf(stderr, "Usage: axctl [-v] port dest src t1|t2|t3|n2|paclen|idle|window|maxq|kill [parm]\n");
		return 1;
	}

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "axctl: no AX.25 port data configured\n");
		return 1;
	}

	if ((addr = ax25_config_get_addr(argv[1])) == NULL) {
		fprintf(stderr, "axctl: invalid port name - %s\n", argv[1]);
		return 1;
	}

	if (ax25_aton_entry(addr, (char *)&ax25_ctl.port_addr) == -1)
		return 1;
	if (ax25_aton_entry(argv[2], (char *)&ax25_ctl.dest_addr) == -1)
		return 1;
	if (ax25_aton_entry(argv[3], (char *)&ax25_ctl.source_addr) == -1)
		return 1;
		
	if ((s = socket(AF_AX25, SOCK_SEQPACKET, 0)) < 0) {
		perror("axctl: socket");
		return 1;
	}

	if (strcmp(argv[4], "kill") == 0 || strcmp(argv[4], "-kill") == 0) {
		ax25_ctl.cmd = AX25_KILL;
		ax25_ctl.arg = 0;
	} else {
		if (argc < 6) {
			fprintf(stderr,"axctl: parameter missing\n");
			return 1;
		}
		ax25_ctl.arg = atoi(argv[5]);
		
		if (strcmp(argv[4], "t1") == 0 || strcmp(argv[4], "-t1") == 0)
			ax25_ctl.cmd = AX25_T1;
		else if (strcmp(argv[4], "t2") == 0 || strcmp(argv[4], "-t2") == 0)
			ax25_ctl.cmd = AX25_T2;
		else if (strcmp(argv[4], "t3") == 0 || strcmp(argv[4], "-t3") == 0)
			ax25_ctl.cmd = AX25_T3;
		else if (strcmp(argv[4], "idle") == 0 || strcmp(argv[4], "-idle") == 0)
			ax25_ctl.cmd = AX25_IDLE;
		else if (strcmp(argv[4], "n2") == 0 || strcmp(argv[4], "-n2") == 0)
			ax25_ctl.cmd = AX25_N2;
		else if (strcmp(argv[4], "window") == 0 || strcmp(argv[4], "-window") == 0)
			ax25_ctl.cmd = AX25_WINDOW;
		else if (strcmp(argv[4], "paclen") == 0 || strcmp(argv[4], "-paclen") == 0)
			ax25_ctl.cmd = AX25_PACLEN;
	}

	ax25_ctl.digi_count = 0;
	
	if (ioctl(s, SIOCAX25CTLCON, &ax25_ctl) != 0) {
		perror("axctl: SIOCAX25CTLCON");
		return 1;
	}
	
	return 0;
}

