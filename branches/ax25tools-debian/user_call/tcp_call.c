#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include "user_io.h"

void alarm_handler(int sig)
{
}

int main(int argc, char **argv)
{
	char buffer[256];
	int s, addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	struct hostent *hp;
	struct servent *sp;
	int verbose = 1;

	paclen_in = 1024;
	paclen_out = 1024;

	while ((s = getopt(argc, argv, "ci:o:q")) != -1) {
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
		case 'q':
			verbose = 0;
			break;
		case ':':
		case '?':
			err("ERROR: invalid option usage\n");
			return 1;
		}
	}

	if (paclen_in < 1 || paclen_out < 1) {
		err("ERROR: invalid paclen\n");
		return 1;
	}

	/*
	 * Arguments should be "tcp_call remaddr remport"
	 */
	if ((argc - optind) != 2) {
		strcpy(buffer, "ERROR: invalid number of parameters\n");
		err(buffer);
	}

	/*
	 * Open the socket into the kernel.
	 */
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		sprintf(buffer, "ERROR: can't open socket: %s\n", strerror(errno));
		err(buffer);
	}

	/*
	 * Resolve the hostname.
	 */
	hp = gethostbyname(argv[optind]);
	if (hp == NULL) {
		err("ERROR: Unknown host\n");
	}
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

	/*
	 * And the service name.
	 */
	if ((sp = getservbyname(argv[optind+1], "tcp")) != NULL)
		addr.sin_port = sp->s_port;
	else
		addr.sin_port = htons(atoi(argv[optind+1]));

	if (addr.sin_port == 0) {
		err("ERROR: Unknown service\n");
	}

	if (verbose) {
		sprintf(buffer, "*** Connecting to %s ...\n", hp->h_name);
		user_write(STDOUT_FILENO, buffer, strlen(buffer));
	}

	/*
	 * If no response in 30 seconds, go away.
	 */
	alarm(30);

	signal(SIGALRM, alarm_handler);

	/*
	 * Lets try and connect to the far end.
	 */
	if (connect(s, (struct sockaddr *)&addr, addrlen) != 0) {
		sprintf(buffer, "ERROR: can't connect: %s\n", strerror(errno));
		err(buffer);
	}

	/*
	 * We got there.
	 */
	alarm(0);

	if (verbose) {
		strcpy(buffer, "*** Connected\n");
		user_write(STDOUT_FILENO, buffer, strlen(buffer));
	}

	select_loop(s);

	if (verbose) {
		strcpy(buffer, "\n*** Disconnected\n");
		user_write(STDOUT_FILENO, buffer, strlen(buffer));
	}

	end_compress();

	return 0;
}
