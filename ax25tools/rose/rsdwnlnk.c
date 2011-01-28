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

#define	AX25_HBIT	0x80

void alarm_handler(int sig)
{
}

int main(int argc, char **argv)
{
	unsigned char buffer[512], *addr;
	fd_set read_fd;
	int n = 0, s, yes = 1;
	struct full_sockaddr_ax25 axbind, axconnect;
	struct sockaddr_rose rosesock, rosepeer;
	socklen_t addrlen;

	openlog("rsdwnlnk", LOG_PID, LOG_DAEMON);

	/*
	 * Arguments should be "rsdwnlnk ax25port ax25call"
	 */
	if (argc != 3) {
		syslog(LOG_ERR, "invalid number of parameters\n");
		closelog();
		return 1;
	}

	if (ax25_config_load_ports() == 0) {
		syslog(LOG_ERR, "problem with axports file\n");
		closelog();
		return 1;
	}

	addrlen = sizeof(struct sockaddr_rose);

	if (getsockname(STDIN_FILENO, (struct sockaddr *)&rosesock, &addrlen) == -1) {
		syslog(LOG_ERR, "cannot getsockname - %s\n", strerror(errno));
		closelog();
		return 1;
	}

	addrlen = sizeof(struct sockaddr_rose);

	if (getpeername(STDIN_FILENO, (struct sockaddr *)&rosepeer, &addrlen) == -1) {
		syslog(LOG_ERR, "cannot getpeername - %s\n", strerror(errno));
		closelog();
		return 1;
	}

	if (setsockopt(STDIN_FILENO, SOL_ROSE, ROSE_QBITINCL, &yes, sizeof(yes)) == -1) {
		syslog(LOG_ERR, "cannot setsockopt(ROSE_QBITINCL) - %s\n", strerror(errno));
		closelog();
		return 1;
	}

	/*
	 * Parse the passed values for correctness.
	 */
	axbind.fsa_ax25.sax25_family = AF_AX25;
	axbind.fsa_ax25.sax25_ndigis = 1;
	axbind.fsa_ax25.sax25_call   = rosepeer.srose_call;

	if ((addr = ax25_config_get_addr(argv[1])) == NULL) {
		syslog(LOG_ERR, "invalid AX.25 port name - %s\n", argv[1]);
		closelog();
		return 1;
	}

	if (ax25_aton_entry(addr, axbind.fsa_digipeater[0].ax25_call) == -1) {
		syslog(LOG_ERR, "invalid AX.25 port callsign - %s\n", argv[1]);
		closelog();
		return 1;
	}

	axconnect.fsa_ax25.sax25_family = AF_AX25;
	axconnect.fsa_ax25.sax25_call   = rosesock.srose_call;

	/*
	 *	The path at the far end has a digi in it.
	 */
	if (rosepeer.srose_ndigis == 1) {
		axconnect.fsa_digipeater[n] = rosepeer.srose_digi;
		axconnect.fsa_digipeater[n].ax25_call[6] |= AX25_HBIT;
		n++;
	}

	/*
	 *	Incoming call has a different DNIC
	 */
	if (memcmp(rosepeer.srose_addr.rose_addr, rosesock.srose_addr.rose_addr, 2) != 0) {
		addr = rose_ntoa(&rosepeer.srose_addr);
		addr[4] = '\0';
		if (ax25_aton_entry(addr, axconnect.fsa_digipeater[n].ax25_call) == -1) {
			syslog(LOG_ERR, "invalid callsign - %s\n", addr);
			closelog();
			return 1;
		}
		axconnect.fsa_digipeater[n].ax25_call[6] |= AX25_HBIT;
		n++;		
	}

	/*
	 *	Put the remote address sans DNIC into the digi chain.
	 */
	addr = rose_ntoa(&rosepeer.srose_addr);
	if (ax25_aton_entry(addr + 4, axconnect.fsa_digipeater[n].ax25_call) == -1) {
		syslog(LOG_ERR, "invalid callsign - %s\n", addr + 4);
		closelog();
		return 1;
	}
	axconnect.fsa_digipeater[n].ax25_call[6] |= AX25_HBIT;
	n++;		

	/*
	 *	And my local ROSE callsign.
	 */
	if (ax25_aton_entry(argv[2], axconnect.fsa_digipeater[n].ax25_call) == -1) {
		syslog(LOG_ERR, "invalid callsign - %s\n", argv[2]);
		closelog();
		return 1;
	}
	axconnect.fsa_digipeater[n].ax25_call[6] |= AX25_HBIT;
	n++;

	/*
	 *	A digi has been specified for this end.
	 */
	if (rosesock.srose_ndigis == 1) {
		axconnect.fsa_digipeater[n] = rosesock.srose_digi;
		n++;
	}

	axconnect.fsa_ax25.sax25_ndigis = n;

	addrlen = sizeof(struct full_sockaddr_ax25);

	/*
	 * Open the socket into the kernel.
	 */
	if ((s = socket(AF_AX25, SOCK_SEQPACKET, 0)) < 0) {
		syslog(LOG_ERR, "cannot open AX.25 socket, %s\n", strerror(errno));
		closelog();
		return 1;
	}
#ifdef HAVE_AX25_IAMDIGI
	if (setsockopt(s, SOL_AX25, AX25_IAMDIGI, &yes, sizeof(yes)) == -1) {
		syslog(LOG_ERR, "cannot setsockopt(AX25_IAMDIGI), %s\n", strerror(errno));
		close(s);
		closelog();
		return 1;
	}
#endif /* HAVE_AX25_IAMDIGI */
#ifdef HAVE_AX25_PIDINCL
	if (setsockopt(s, SOL_AX25, AX25_PIDINCL, &yes, sizeof(yes)) == -1) {
		syslog(LOG_ERR, "cannot setsockopt(AX25_PIDINCL), %s\n", strerror(errno));
		close(s);
		closelog();
		return 1;
	}
#endif /* HAVE_AX25_PIDINCL */
	/*
	 * Set our AX.25 callsign and AX.25 port callsign accordingly.
	 */
	if (bind(s, (struct sockaddr *)&axbind, addrlen) != 0) {
		syslog(LOG_ERR, "cannot bind AX.25 socket, %s\n", strerror(errno));
		close(s);
		closelog();
		return 1;
	}

	/*
	 * If no response in 60 seconds, go away.
	 */
	alarm(60);

	signal(SIGALRM, alarm_handler);

	/*
	 * Lets try and connect to the far end.
	 */
	if (connect(s, (struct sockaddr *)&axconnect, addrlen) != 0) {
		switch (errno) {
			case ECONNREFUSED:
				strcpy(buffer, "*** Connection refused\r");
				break;
			case ENETUNREACH:
				strcpy(buffer, "*** No known route\r");
				break;
			case EINTR:
				strcpy(buffer, "*** Connection timed out\r");
				break;
			default:
				sprintf(buffer, "ERROR: cannot connect to AX.25 callsign, %s\r", strerror(errno));
				break;
		}

		close(s);

		write(STDOUT_FILENO, buffer, strlen(buffer));
		
		sleep(20);
		
		return 0;
	}

	/*
	 * We got there.
	 */
	alarm(0);

	strcpy(buffer, "*** Connected\r");
	write(STDOUT_FILENO, buffer, strlen(buffer));

	/*
	 * Loop until one end of the connection goes away.
	 */
	for (;;) {
		FD_ZERO(&read_fd);
		FD_SET(STDIN_FILENO, &read_fd);
		FD_SET(s, &read_fd);
		
		select(s + 1, &read_fd, NULL, NULL, NULL);

		if (FD_ISSET(s, &read_fd)) {
			if ((n = read(s, buffer + 2, sizeof(buffer)-2)) == -1)
				break;
			if (buffer[2] == 0xF0) {
				buffer[2] = 0;
				write(STDOUT_FILENO, buffer + 2, n);
			} else {
				buffer[0] = 1;		/* Set Q Bit on */
				buffer[1] = 0x7F;	/* Q Bit escape */
				write(STDOUT_FILENO, buffer, n + 2);
			}
		}

		if (FD_ISSET(STDIN_FILENO, &read_fd)) {
			if ((n = read(STDIN_FILENO, buffer, 512)) == -1) {
				close(s);
				break;
			}
			if (buffer[0] == 0) {		/* Q Bit not set */
				buffer[0] = 0xF0;
				write(s, buffer, n);
			} else {
				/* Lose the leading 0x7F */
				write(s, buffer + 2, n - 2);
			}
		}
	}

	closelog();

	return 0;
}
