#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>

#include <config.h>

#include <sys/time.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/rsconfig.h>

void alarm_handler(int sig)
{
}

int main(int argc, char **argv)
{
	unsigned char buffer[512], *addr, *p;
	char rose_address[11];
	fd_set read_fd;
	int n, s, dnicindex = -1, addrindex = -1;
	int yes = 1, verbose = 1;
	socklen_t addrlen;
	struct sockaddr_rose rosebind, roseconnect;
	struct full_sockaddr_ax25 ax25sock, ax25peer;

	openlog("rsuplnk", LOG_PID, LOG_DAEMON);

	/*
	 * Arguments should be "rsuplnk [-q] roseport"
	 */
	if (argc > 2 && strcmp(argv[1], "-q") == 0) {
		verbose = 0;
		--argc;
		++argv;
	}

	if (argc != 2) {
		syslog(LOG_ERR, "invalid number of parameters\n");
		closelog();
		return 1;
	}

	if (rs_config_load_ports() == 0) {
		syslog(LOG_ERR, "problem with rsports file\n");
		closelog();
		return 1;
	}

	addrlen = sizeof(struct full_sockaddr_ax25);

	if (getsockname(STDIN_FILENO, (struct sockaddr *)&ax25sock, &addrlen) == -1) {
		syslog(LOG_ERR, "cannot getsockname, %s\n", strerror(errno));
		closelog();
		return 1;
	}

	addrlen = sizeof(struct full_sockaddr_ax25);

	if (getpeername(STDIN_FILENO, (struct sockaddr *)&ax25peer, &addrlen) == -1) {
		syslog(LOG_ERR, "cannot getpeername, %s\n", strerror(errno));
		closelog();
		return 1;
	}
#ifdef HAVE_AX25_PIDINCL
	if (setsockopt(STDIN_FILENO, SOL_AX25, AX25_PIDINCL, &yes, sizeof(yes)) == -1) {
		syslog(LOG_ERR, "cannot setsockopt(AX25_PIDINCL) - %s\n", strerror(errno));
		closelog();
		return 1;
	}
#endif /* HAVE_AX25_PIDINCL */
	roseconnect.srose_family = rosebind.srose_family = AF_ROSE;
	roseconnect.srose_ndigis = rosebind.srose_ndigis = 0;
	addrlen = sizeof(struct sockaddr_rose);

	if ((addr = rs_config_get_addr(argv[1])) == NULL) {
		syslog(LOG_ERR, "invalid Rose port name - %s\n", argv[1]);
		closelog();
		return 1;
	}

	if (rose_aton(addr, rosebind.srose_addr.rose_addr) == -1) {
		syslog(LOG_ERR, "invalid ROSE port address - %s\n", argv[1]);
		closelog();
		return 1;
	}

	/*
	 *	Copy our DNIC in as default.
	 */
	memset(rose_address, 0x00, 11);
	memcpy(rose_address, addr, 4);

	for (n = 0; n < ax25peer.fsa_ax25.sax25_ndigis; n++) {
		addr = ax25_ntoa(&ax25peer.fsa_digipeater[n]);
		
		if (strspn(addr, "0123456789-") == strlen(addr)) {
			if ((p = strchr(addr, '-')) != NULL)
				*p = '\0';
			switch (strlen(addr)) {
				case 4:
					memcpy(rose_address + 0, addr, 4);
					dnicindex = n;
					break;
				case 6:
					memcpy(rose_address + 4, addr, 6);
					addrindex = n;
					break;
				default:
					break;
			}
		}
	}

	/*
	 *	The user didn't give an address.
	 */
	if (addrindex == -1) {
		strcpy(buffer, "*** No ROSE address given\r");
		write(STDOUT_FILENO, buffer, strlen(buffer));
		sleep(20);
		closelog();
		return 1;
	}

	if (rose_aton(rose_address, roseconnect.srose_addr.rose_addr) == -1) {
		syslog(LOG_ERR, "invalid Rose address - %s\n", argv[4]);
		closelog();
		return 1;
	}

	rosebind.srose_call    = ax25peer.fsa_ax25.sax25_call;

	roseconnect.srose_call = ax25sock.fsa_ax25.sax25_call;

	/*
	 *	A far end digipeater was specified.
	 */
	if (addrindex > 0) {
		roseconnect.srose_ndigis = 1;
		roseconnect.srose_digi   = ax25peer.fsa_digipeater[addrindex - 1];
	}

	/*
	 *	Check for a local digipeater.
	 */
	if (dnicindex != -1) {
		if (ax25peer.fsa_ax25.sax25_ndigis - dnicindex > 2) {
			rosebind.srose_ndigis = 1;
			rosebind.srose_digi   = ax25peer.fsa_digipeater[dnicindex + 2];
		}
	} else {
		if (ax25peer.fsa_ax25.sax25_ndigis - addrindex > 2) {
			rosebind.srose_ndigis = 1;
			rosebind.srose_digi   = ax25peer.fsa_digipeater[addrindex + 2];
		}
	}

	/*
	 * Open the socket into the kernel.
	 */
	if ((s = socket(AF_ROSE, SOCK_SEQPACKET, 0)) < 0) {
		syslog(LOG_ERR, "cannot open ROSE socket, %s\n", strerror(errno));
		closelog();
		return 1;
	}

	/*
	 * Set our AX.25 callsign and Rose address accordingly.
	 */
	if (bind(s, (struct sockaddr *)&rosebind, addrlen) != 0) {
		syslog(LOG_ERR, "cannot bind ROSE socket, %s\n", strerror(errno));
		closelog();
		close(s);
		return 1;
	}

	if (setsockopt(s, SOL_ROSE, ROSE_QBITINCL, &yes, sizeof(yes)) == -1) {
		syslog(LOG_ERR, "cannot setsockopt(ROSE_QBITINCL) - %s\n", strerror(errno));
		closelog();
		close(s);
		return 1;
	}

	if (verbose) {
		strcpy(buffer, "*** Connection in progress\r");
		write(STDOUT_FILENO, buffer, strlen(buffer));
	}

	/*
	 * If no response in 5 minutes, go away.
	 */
	alarm(300);

	signal(SIGALRM, alarm_handler);

	/*
	 * Lets try and connect to the far end.
	 */
	if (connect(s, (struct sockaddr *)&roseconnect, addrlen) != 0) {
		switch (errno) {
			case ECONNREFUSED:
				strcpy(buffer, "*** Disconnected - 0100 - Number Busy\r");
				break;
			case ENETUNREACH:
				strcpy(buffer, "*** Disconnected - 0D00 - Not Obtainable\r");
				break;
			case EINTR:
				strcpy(buffer, "*** Disconnected - 3900 - Ship Absent\r");
				break;
			default:
				sprintf(buffer, "*** Disconnected - %d - %s\r", errno, strerror(errno));
				break;
		}

		close(s);

		if (verbose) {
			write(STDOUT_FILENO, buffer, strlen(buffer));
			sleep(20);
		}

		return 0;
	}

	/*
	 * We got there.
	 */
	alarm(0);

	if (verbose) {
		strcpy(buffer, "*** Connected\r");
		write(STDOUT_FILENO, buffer, strlen(buffer));
	}

	/*
	 * Loop until one end of the connection goes away.
	 */
	for (;;) {
		FD_ZERO(&read_fd);
		FD_SET(STDIN_FILENO, &read_fd);
		FD_SET(s, &read_fd);
		
		select(s + 1, &read_fd, NULL, NULL, NULL);

		if (FD_ISSET(s, &read_fd)) {
			if ((n = read(s, buffer, 512)) == -1) {
				strcpy(buffer, "\r*** Disconnected - 0000 - DTE Originated\r");
				write(STDOUT_FILENO, buffer, strlen(buffer));
				break;
			}
			if (buffer[0] == 0) {		/* Q Bit not set */
				buffer[0] = 0xF0;
				write(STDOUT_FILENO, buffer, n);
			} else {
				/* Lose the leading 0x7F */
				write(STDOUT_FILENO, buffer + 2, n - 2);
			}
		}

		if (FD_ISSET(STDIN_FILENO, &read_fd)) {
			if ((n = read(STDIN_FILENO, buffer + 2, sizeof(buffer)-2)) == -1) {
				close(s);
				break;
			}
			if (buffer[2] == 0xF0) {
				buffer[2] = 0;
				write(s, buffer + 2, n);
			} else {
				buffer[0] = 1;		/* Set Q bit on */
				buffer[1] = 0x7F;	/* PID Escape   */
				write(s, buffer, n + 2);
			}
		}
	}

	closelog();

	return 0;
}
