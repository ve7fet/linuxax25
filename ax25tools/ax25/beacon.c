#include <config.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/daemon.h>

static int logging = FALSE;
static int mail = FALSE;
static int single = FALSE;

#define STR_USAGE "usage: beacon [-c <src_call>] [-d <dest_call>] [-f] [-H] [-l] [-m] [-s] [-t interval] [-v] <port> <message>\n"

static void terminate(int sig)
{
	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}

	exit(0);
}

int main(int argc, char *argv[])
{
	struct full_sockaddr_ax25 dest;
	struct full_sockaddr_ax25 src;
	int s, n, dlen, len;
	char *addr, *port, *message, *portcall;
	char *srccall = NULL, *destcall = NULL;
	int dofork = 1;
	time_t interval = 30;
	int interval_relative = 0;

	while ((n = getopt(argc, argv, "c:d:fHlmst:v")) != -1) {
		switch (n) {
		case 'c':
			srccall = optarg;
			break;
		case 'd':
			destcall = optarg;
			break;
		case 'f':
			dofork = 0;
			break;
		case 'H':
			interval_relative = 1;
			break;
		case 'l':
			logging = TRUE;
			break;
		case 'm':
			mail = TRUE;
			/* falls through */
		case 's':
			single = TRUE;
			break;
		case 't':
			interval = (time_t ) atoi(optarg);
			if (interval < 1) {
				fprintf(stderr, "beacon: interval must be greater than on minute\n");
				return 1;
			}
			break;
		case 'v':
			printf("beacon: %s\n", VERSION);
			return 0;
		case '?':
		case ':':
			fprintf(stderr, STR_USAGE);
			return 1;
		}
	}

	if (interval_relative && interval > 60L) {
		fprintf(stderr, "beacon: can't align interval > 60min to an hour\n");
		return 1;
	}

	signal(SIGTERM, terminate);

	if (optind == argc || optind == argc - 1) {
		fprintf(stderr, STR_USAGE);
		return 1;
	}

	port    = argv[optind];
	message = argv[optind + 1];

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "beacon: no AX.25 ports defined\n");
		return 1;
	}

	portcall = ax25_config_get_addr(port);
	if (portcall == NULL) {
		fprintf(stderr, "beacon: invalid AX.25 port setting - %s\n", port);
		return 1;
	}

	addr = NULL;
	if (mail)
		addr = strdup("MAIL");
	else if (destcall != NULL)
		addr = strdup(destcall);
	else
		addr = strdup("IDENT");
	if (addr == NULL)
		return 1;

	dlen = ax25_aton(addr, &dest);
	if (dlen == -1) {
		fprintf(stderr, "beacon: unable to convert callsign '%s'\n", addr);
		return 1;
	}
	if (addr != NULL) {
		free(addr);
		addr = NULL;
	}

	if (srccall != NULL && strcmp(srccall, portcall) != 0) {
		addr = malloc(strlen(srccall) + 1 + strlen(portcall) + 1);
		if (addr == NULL)
			return 1;
		sprintf(addr, "%s %s", srccall, portcall);
	} else {
		addr = strdup(portcall);
		if (addr == NULL)
			return 1;
	}

	len = ax25_aton(addr, &src);
	if (len == -1) {
		fprintf(stderr, "beacon: unable to convert callsign '%s'\n", addr);
		return 1;
	}
	if (addr != NULL) {
		free(addr);
		addr = NULL;
	}

	if (!single && dofork) {
		if (!daemon_start(FALSE)) {
			fprintf(stderr, "beacon: cannot become a daemon\n");
			return 1;
		}
	}

	if (logging) {
		openlog("beacon", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "starting");
	}

	/* interval has a one minute resolution */
	interval *= 60L;

	for (;;) {

		if (interval_relative) {
			time_t t_sleep = interval - (time(NULL) % interval);
			if (t_sleep > 0)
				sleep(t_sleep);
		}

		s = socket(AF_AX25, SOCK_DGRAM, 0);
		if (s == -1) {
			if (logging) {
				syslog(LOG_ERR, "socket: %m");
				closelog();
			}
			return 1;
		}

		if (bind(s, (struct sockaddr *)&src, len) == -1) {
			if (logging) {
				syslog(LOG_ERR, "bind: %m");
				closelog();
			}
			return 1;
		}

		if (sendto(s, message, strlen(message), 0, (struct sockaddr *)&dest, dlen) == -1) {
			if (logging) {
				syslog(LOG_ERR, "sendto: %m");
				closelog();
			}
			return 1;
		}

		close(s);

		if (single)
			break;

		if (!interval_relative)
			sleep(interval);
	}

	return 0;
}
