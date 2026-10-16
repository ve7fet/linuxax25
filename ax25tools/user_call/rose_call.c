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
#include <netax25/rsconfig.h>

#include "user_io.h"

int main(int argc, char **argv)
{
	char buffer[256], *addr;
	int s, ret, addrlen = sizeof(struct sockaddr_rose);
	struct sockaddr_rose rosebind, roseconnect;

	while ((s = getopt(argc, argv, "ci:o:T:")) != -1) {
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
		case 'T':
			connect_seconds = atoi(optarg);
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

	if (connect_seconds < 0) {
		err("ERROR: invalid connect timeout\r");
		return 1;
	}

	/*
	 * Arguments should be "rose_call port mycall remcall remaddr"
	 */
	if ((argc - optind) != 4) {
		strcpy(buffer, "ERROR: invalid number of parameters\r");
		err(buffer);
	}

	if (rs_config_load_ports() == 0) {
		strcpy(buffer, "ERROR: problem with rsports file\r");
		err(buffer);
	}

	/*
	 * Parse the passed values for correctness.
	 */
	memset(&rosebind, 0, sizeof(rosebind));
	memset(&roseconnect, 0, sizeof(roseconnect));
	roseconnect.srose_family = rosebind.srose_family = AF_ROSE;
	roseconnect.srose_ndigis = rosebind.srose_ndigis = 0;

	addr = rs_config_get_addr(argv[optind]);
	if (addr == NULL) {
		snprintf(buffer, sizeof(buffer), "ERROR: invalid Rose port name - %s\r", argv[optind]);
		err(buffer);
	}

	if (rose_aton(addr, rosebind.srose_addr.rose_addr) == -1) {
		snprintf(buffer, sizeof(buffer), "ERROR: invalid Rose port address - %s\r", argv[optind]);
		err(buffer);
	}

	if (ax25_aton_entry(argv[optind + 1], rosebind.srose_call.ax25_call) == -1) {
		snprintf(buffer, sizeof(buffer), "ERROR: invalid callsign - %s\r", argv[optind + 1]);
		err(buffer);
	}

	if (ax25_aton_entry(argv[optind + 2], roseconnect.srose_call.ax25_call) == -1) {
		snprintf(buffer, sizeof(buffer), "ERROR: invalid callsign - %s\r", argv[optind + 2]);
		err(buffer);
	}

	if (rose_aton(argv[optind + 3], roseconnect.srose_addr.rose_addr) == -1) {
		snprintf(buffer, sizeof(buffer), "ERROR: invalid Rose address - %s\r", argv[optind + 3]);
		err(buffer);
	}

	/*
	 * Open the socket into the kernel.
	 */
	s = socket(AF_ROSE, SOCK_SEQPACKET, 0);
	if (s < 0) {
		snprintf(buffer, sizeof(buffer), "ERROR: cannot open Rose socket, %s\r", strerror(errno));
		err(buffer);
	}

	/*
	 * Set our AX.25 callsign and Rose address accordingly.
	 */
	if (bind(s, (struct sockaddr *)&rosebind, addrlen) != 0) {
		snprintf(buffer, sizeof(buffer), "ERROR: cannot bind Rose socket, %s\r", strerror(errno));
		err(buffer);
	}

	snprintf(buffer, sizeof(buffer), "Connecting to %s @ %s ...\r", argv[optind + 2], argv[optind + 3]);
	user_write(STDOUT_FILENO, buffer, strlen(buffer));

	/*
	 * With -T, give up if there is no response in time.
	 */
	connect_timeout_handler_init(connect_seconds,
		"*** Connection timed out - aborting\r");

	/*
	 * Lets try and connect to the far end.
	 */
	ret = connect(s, (struct sockaddr *)&roseconnect, addrlen);
	alarm(0);
	if (ret != 0) {
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
			snprintf(buffer, sizeof(buffer), "ERROR: cannot connect to Rose address, %s\r", strerror(errno));
			break;
		}

		err(buffer);
	}

	/*
	 * We got there.
	 */
	strcpy(buffer, "*** Connected\r");
	user_write(STDOUT_FILENO, buffer, strlen(buffer));

	select_loop(s);

	strcpy(buffer, "\r*** Disconnected\r");
	user_write(STDOUT_FILENO, buffer, strlen(buffer));

	end_compress();

	return 0;
}
