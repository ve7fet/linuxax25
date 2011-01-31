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
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include "user_io.h"

void alarm_handler(int sig)
{
}

int main(int argc, char **argv)
{
	char buffer[256], *addr;
	int s, addrlen = sizeof(struct full_sockaddr_ax25);
	struct full_sockaddr_ax25 axbind, axconnect;

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
	 * Arguments should be "ax25_call port mycall remcall [digis ...]"
	 */
	if ((argc - optind) < 3) {
		strcpy(buffer, "ERROR: invalid number of parameters\r");
		err(buffer);
	}

	if (ax25_config_load_ports() == 0) {
		strcpy(buffer, "ERROR: problem with axports file\r");
		err(buffer);
	}

	/*
	 * Parse the passed values for correctness.
	 */
	axconnect.fsa_ax25.sax25_family = axbind.fsa_ax25.sax25_family = AF_AX25;
	axbind.fsa_ax25.sax25_ndigis = 1;

	if ((addr = ax25_config_get_addr(argv[optind])) == NULL) {
		sprintf(buffer, "ERROR: invalid AX.25 port name - %s\r", argv[optind]);
		err(buffer);
	}

	if (ax25_aton_entry(addr, axbind.fsa_digipeater[0].ax25_call) == -1) {
		sprintf(buffer, "ERROR: invalid AX.25 port callsign - %s\r", argv[optind]);
		err(buffer);
	}

	if (ax25_aton_entry(argv[optind + 1], axbind.fsa_ax25.sax25_call.ax25_call) == -1) {
		sprintf(buffer, "ERROR: invalid callsign - %s\r", argv[optind + 1]);
		err(buffer);
	}

	if (ax25_aton_arglist((const char**)(&argv[optind + 2]), &axconnect) == -1) {
		sprintf(buffer, "ERROR: invalid destination callsign or digipeater\r");
		err(buffer);
	}

	/*
	 * Open the socket into the kernel.
	 */
	if ((s = socket(AF_AX25, SOCK_SEQPACKET, 0)) < 0) {
		sprintf(buffer, "ERROR: cannot open AX.25 socket, %s\r", strerror(errno));
		err(buffer);
	}

	/*
	 * Set our AX.25 callsign and AX.25 port callsign accordingly.
	 */
	if (bind(s, (struct sockaddr *)&axbind, addrlen) != 0) {
		sprintf(buffer, "ERROR: cannot bind AX.25 socket, %s\r", strerror(errno));
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
	if (connect(s, (struct sockaddr *)&axconnect, addrlen) != 0) {
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
				sprintf(buffer, "ERROR: cannot connect to AX.25 callsign, %s\r", strerror(errno));
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
