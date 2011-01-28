#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <config.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netrom/netrom.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/nrconfig.h>

#include "user_io.h"

void alarm_handler(int sig)
{
}

int main(int argc, char **argv)
{
	char buffer[256], *addr;
	int s, addrlen = sizeof(struct full_sockaddr_ax25);
	struct full_sockaddr_ax25 nrbind, nrconnect;

	paclen_in = 236;
	paclen_out = 236;

	while ((s = getopt(argc, argv, "ci:o:")) != -1) {
		switch (s) {
		case 'c':
			init_compress();
			compression = 1;
			break;
		case 'i':
			paclen_in = atoi(optarg);
			break;
		case 'o':
			paclen_out = atoi(optarg);
			break;
		case ':':
		case '?':
			err("ERROR: invalid option usage\r");
			return 1;
		}
	}

	if (paclen_in < 1 || paclen_out < 1) {
		err("ERROR: invalid paclen\r");
		return 1;
	}

	/*
	 * Arguments should be "netrom_call port mycall remaddr"
	 */
	if ((argc - optind) != 3) {
		strcpy(buffer, "ERROR: invalid number of parameters\r");
		err(buffer);
	}

	if (nr_config_load_ports() == 0) {
		strcpy(buffer, "ERROR: problem with nrports file\r");
		err(buffer);
	}

	/*
	 * Parse the passed values for correctness.
	 */
	nrconnect.fsa_ax25.sax25_family = nrbind.fsa_ax25.sax25_family = AF_NETROM;
	nrbind.fsa_ax25.sax25_ndigis    = 1;
	nrconnect.fsa_ax25.sax25_ndigis = 0;

	if ((addr = nr_config_get_addr(argv[optind])) == NULL) {
		sprintf(buffer, "ERROR: invalid NET/ROM port name - %s\r", argv[optind]);
		err(buffer);
	}

	if (ax25_aton_entry(addr, nrbind.fsa_ax25.sax25_call.ax25_call) == -1) {
		sprintf(buffer, "ERROR: invalid NET/ROM port callsign - %s\r", argv[optind]);
		err(buffer);
	}

	if (ax25_aton_entry(argv[optind + 1], nrbind.fsa_digipeater[0].ax25_call) == -1) {
		sprintf(buffer, "ERROR: invalid callsign - %s\r", argv[optind + 1]);
		err(buffer);
	}

	if ((addr = strchr(argv[optind + 2], ':')) == NULL)
		addr = argv[optind + 2];
	else
		addr++;

	if (ax25_aton_entry(addr, nrconnect.fsa_ax25.sax25_call.ax25_call) == -1) {
		sprintf(buffer, "ERROR: invalid callsign - %s\r", argv[optind + 2]);
		err(buffer);
	}

	/*
	 * Open the socket into the kernel.
	 */
	if ((s = socket(AF_NETROM, SOCK_SEQPACKET, 0)) < 0) {
		sprintf(buffer, "ERROR: cannot open NET/ROM socket, %s\r", strerror(errno));
		err(buffer);
	}

	/*
	 * Set our AX.25 callsign and NET/ROM callsign accordingly.
	 */
	if (bind(s, (struct sockaddr *)&nrbind, addrlen) != 0) {
		sprintf(buffer, "ERROR: cannot bind NET/ROM socket, %s\r", strerror(errno));
		err(buffer);
	}

	sprintf(buffer, "Connecting to %s ...\r", argv[optind + 2]);
	user_write(STDOUT_FILENO, buffer, strlen(buffer));

	/*
	 * If no response in 30 seconds, go away.
	 */
	alarm(30);

	signal(SIGALRM, alarm_handler);

	/*
	 * Lets try and connect to the far end.
	 */
	if (connect(s, (struct sockaddr *)&nrconnect, addrlen) != 0) {
		switch (errno) {
			case ECONNREFUSED:
				strcpy(buffer, "*** Connection refused - aborting\r");
				break;
			case ENETUNREACH:
				strcpy(buffer, "*** No known route - aborting\r");
				break;
			case EINTR:
				strcpy(buffer, "*** Connection timed out - aborting\r");
				break;
			default:
				sprintf(buffer, "ERROR: cannot connect to NET/ROM node, %s\r", strerror(errno));
				break;
		}

		err(buffer);
	}

	/*
	 * We got there.
	 */
	alarm(0);

	strcpy(buffer, "*** Connected\r");
	user_write(STDOUT_FILENO, buffer, strlen(buffer));

	select_loop(s);

	strcpy(buffer, "\r*** Disconnected\r");
	user_write(STDOUT_FILENO, buffer, strlen(buffer));

	end_compress();

	return 0;
}
