/*
 *  ax25mond.c - Wrapper for offering AX.25 traffic to non-root processes
 *
 *  Copyright (C) 1998-1999 Johann Hanne, DH3MB. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*--------------------------------------------------------------------------*/

#define VERSION "1.0"
#define CONFFILE "/etc/ax25/ax25mond.conf"
#define MAX_SOCKETS  5
#define MAX_CONNECTS 50

/*--------------------------------------------------------------------------*/

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* For older kernels  */
#ifndef PF_PACKET
#define PF_PACKET PF_INET
#endif

/*--------------------------------------------------------------------------*/

static union {
	struct sockaddr sa;
	struct sockaddr_in si;
	struct sockaddr_un su;
} addr;

/*--------------------------------------------------------------------------*/

int sock_list[MAX_SOCKETS];
char sock_monmode[MAX_SOCKETS];
char sock_filename[MAX_SOCKETS][100];
int sock_num = 0;
int conn_list[MAX_CONNECTS];
struct sockaddr conn_addr[MAX_CONNECTS];
socklen_t conn_addrlen[MAX_CONNECTS];
char conn_monmode[MAX_CONNECTS];
int conn_num = 0;
int highest_sock_fd;
int end = 0;

/*--------------------------------------------------------------------------*/

/* from buildsaddr.c */
struct sockaddr *build_sockaddr(const char *name, int *addrlen)
{
	char *host_name;
	char *serv_name;
	char buf[1024];

	memset(&addr, 0, sizeof(addr));
	*addrlen = 0;

	host_name = strcpy(buf, name);
	serv_name = strchr(buf, ':');
	if (!serv_name)
		return 0;
	*serv_name++ = 0;
	if (!*host_name || !*serv_name)
		return 0;

	if (!strcmp(host_name, "local") || !strcmp(host_name, "unix")) {
		addr.su.sun_family = AF_UNIX;
		*addr.su.sun_path = 0;
		strcat(addr.su.sun_path, serv_name);
		*addrlen = sizeof(struct sockaddr_un);
		return &addr.sa;
	}

	addr.si.sin_family = AF_INET;

	if (!strcmp(host_name, "*")) {
		addr.si.sin_addr.s_addr = INADDR_ANY;
	} else if (!strcmp(host_name, "loopback")) {
		addr.si.sin_addr.s_addr = inet_addr("127.0.0.1");
	} else if ((addr.si.sin_addr.s_addr = inet_addr(host_name)) == -1) {
		struct hostent *hp = gethostbyname(host_name);
		endhostent();
		if (!hp)
			return 0;
		addr.si.sin_addr.s_addr =
		    ((struct in_addr *) (hp->h_addr))->s_addr;
	}

	if (isdigit(*serv_name & 0xff)) {
		addr.si.sin_port = htons(atoi(serv_name));
	} else {
		struct servent *sp = getservbyname(serv_name, NULL);
		endservent();
		if (!sp)
			return 0;
		addr.si.sin_port = sp->s_port;
	}

	*addrlen = sizeof(struct sockaddr_in);
	return &addr.sa;
}

/*--------------------------------------------------------------------------*/

void add_socket(char *sockname, char monmode)
{
	struct sockaddr *saddr;
	int saddrlen;

	if (sock_num == MAX_SOCKETS - 1) {
		fprintf(stderr,
			"WARNING: Too many sockets defined - only %d are "
			"allowed\n", MAX_SOCKETS);
		return;
	}

	if (!(saddr = build_sockaddr(sockname, &saddrlen))) {
		fprintf(stderr, "WARNING: Invalid socket name: \"%s\"\n",
			sockname);
		return;
	}

	if (saddr->sa_family == AF_UNIX)
		strcpy(sock_filename[sock_num], strchr(sockname, ':') + 1);
	else
		sock_filename[sock_num][0] = 0;

	if ((sock_list[sock_num] =
	     socket(saddr->sa_family, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr,
			"WARNING: Error opening socket \"%s\": %s\n",
			sockname, strerror(errno));
		return;
	}

	if (bind(sock_list[sock_num], saddr, saddrlen) < 0) {
		fprintf(stderr,
			"WARNING: Error binding socket \"%s\": %s\n",
			sockname, strerror(errno));
		return;
	}

	if (listen(sock_list[sock_num], 5) < 0) {
		fprintf(stderr,
			"WARNING: Error listening on socket \"%s\": %s\n",
			sockname, strerror(errno));
		return;
	}

	fcntl(sock_list[sock_num], F_SETFL, O_NONBLOCK);

	if (sock_list[sock_num] > highest_sock_fd)
		highest_sock_fd = sock_list[sock_num];

	sock_monmode[sock_num] = monmode;
	sock_num++;
}

/*--------------------------------------------------------------------------*/

void close_sockets(void)
{
	int i;

	for (i = 0; i < sock_num; i++) {
		close(sock_list[i]);

		if (sock_filename[i][0])
			unlink(sock_filename[i]);
	}
}

/*--------------------------------------------------------------------------*/

void quit_handler(int dummy)
{
	end = 1;
}

/*--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	fd_set monavail;
	int monrx_fd, monrxtx_fd;
	struct sockaddr monfrom;
	socklen_t monfromlen;
	fd_set conn_request;
	struct timeval tv;
	char buf[500];
	int size;
	int i;
	struct ifreq ifr;
	FILE *conffile;
	char confline[100];
	char *mode;
	char *sockname;

	if (argc > 1) {
		if (argc == 2 &&
		    (strcmp(argv[1], "-v") == 0
		     || strcmp(argv[1], "--version") == 0)) {
			printf("%s: Version " VERSION "\n\n", argv[0]);
			printf
			    ("Copyright (C) 1998-1999 Johann Hanne, DH3MB. All rights reserved.\n"
			     "This program is free software; you can redistribute it and/or modify\n"
			     "it under the terms of the GNU General Public License as published by\n"
			     "the Free Software Foundation; either version 2 of the License, or\n"
			     "(at your option) any later version.\n\n"
			     "This program is distributed in the hope that it will be useful,\n"
			     "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
			     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
			exit(1);
		} else {
			printf("Usage: %s [-v|--version]\n", argv[0]);
			exit(0);
		}
	}
	/* At first, read the configuration file  */
	if (!(conffile = fopen(CONFFILE, "r"))) {
		fprintf(stderr, "Unable to open " CONFFILE ".\n");
		exit(1);
	}
	while (fgets(confline, 100, conffile)) {
		if (confline[0] == '#')	/* Comment  */
			continue;

		confline[strlen(confline) - 1] = 0;	/* Cut the LF  */

		if (!(sockname = strchr(confline, ' '))) {
			fprintf(stderr,
				"WARNING: The following configuration line includes "
				"one or more errors:\n%s\n", confline);
			continue;
		}

		*(sockname++) = 0;
		mode = confline;
		if (strcasecmp(mode, "RX") == 0)
			add_socket(sockname, 0);
		else if (strcasecmp(mode, "RXTX") == 0)
			add_socket(sockname, 1);
		else
			fprintf(stderr,
				"WARNING: Mode \"%s\" not supported\n",
				mode);

	}
	fclose(conffile);

	if (sock_num == 0) {
		fprintf(stderr, "FATAL: No usable socket found\n");
		exit(1);
	}
	/* Fork into background  */
	if (fork())
		exit(0);

	/* Close stdout, stderr and stdin  */
	fclose(stdout);
	fclose(stderr);
	fclose(stdin);

	/* Set some signal handlers  */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, quit_handler);

	openlog("ax25mond", LOG_PID, LOG_DAEMON);

	/* Open AX.25 socket for monitoring RX traffic only  */
	if ((monrx_fd =
	     socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_AX25))) < 0) {
		syslog(LOG_ERR, "Error opening monitor socket: %s\n",
		       strerror(errno));
		exit(1);
	}
	fcntl(monrx_fd, F_SETFL, O_NONBLOCK);

	/* Open AX.25 socket for monitoring RX and TX traffic  */
	if ((monrxtx_fd =
	     socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ALL))) < 0) {
		syslog(LOG_ERR, "Error opening monitor socket: %s\n",
		       strerror(errno));
		exit(1);
	}
	fcntl(monrxtx_fd, F_SETFL, O_NONBLOCK);

	while (!end) {
		/* Look for incoming connects on all open sockets  */
		FD_ZERO(&conn_request);
		for (i = 0; i < sock_num; i++)
			FD_SET(sock_list[i], &conn_request);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		select(highest_sock_fd + 1, &conn_request, 0, 0, &tv);
		for (i = 0; i < sock_num; i++)
			if (FD_ISSET(sock_list[i], &conn_request)) {
				conn_list[conn_num] = accept(sock_list[i],
							     &conn_addr
							     [conn_num],
							     &conn_addrlen
							     [conn_num]);
				conn_monmode[conn_num] = sock_monmode[i];
				conn_num++;
			}
		/* Check if there is new data on the RX-only monitor socket  */
		FD_ZERO(&monavail);
		FD_SET(monrx_fd, &monavail);
		tv.tv_sec = 0;
		tv.tv_usec = 10;
		select(monrx_fd + 1, &monavail, 0, 0, &tv);
		if (FD_ISSET(monrx_fd, &monavail)) {
			monfromlen = sizeof(monfrom);
			size =
			    recvfrom(monrx_fd, buf, sizeof(buf), 0,
				     &monfrom, &monfromlen);
			/* Send the packet to all connected sockets  */
			for (i = 0; i < conn_num; i++) {
				if (conn_monmode[i] == 0)
					if (send
					    (conn_list[i], buf, size,
					     0) < 0)
						if (errno != EAGAIN) {
							syslog(LOG_ERR,
							       "Error sending monitor data: %s\n",
							       strerror
							       (errno));
							close(conn_list
							      [i]);
							conn_list[i] =
							    conn_list
							    [conn_num - 1];
							conn_num--;
						}
			}
		}
		/* Check if there is new data on the RX+TX monitor socket  */
		FD_ZERO(&monavail);
		FD_SET(monrxtx_fd, &monavail);
		tv.tv_sec = 0;
		tv.tv_usec = 10;
		select(monrxtx_fd + 1, &monavail, 0, 0, &tv);
		if (FD_ISSET(monrxtx_fd, &monavail)) {
			monfromlen = sizeof(monfrom);
			size =
			    recvfrom(monrxtx_fd, buf, sizeof(buf), 0,
				     &monfrom, &monfromlen);
			/* Check, if we have received a AX.25-packet  */
			strcpy(ifr.ifr_name, monfrom.sa_data);
			ioctl(monrxtx_fd, SIOCGIFHWADDR, &ifr);
			if (ifr.ifr_hwaddr.sa_family == AF_AX25)
				/* Send the packet to all connected sockets  */
				for (i = 0; i < conn_num; i++) {
					if (conn_monmode[i] == 1)
						if (send
						    (conn_list[i], buf,
						     size, 0) < 0)
							if (errno !=
							    EAGAIN) {
								syslog
								    (LOG_ERR,
								     "Error sending monitor data: %s\n",
								     strerror
								     (errno));
								close
								    (conn_list
								     [i]);
								conn_list
								    [i] =
								    conn_list
								    [conn_num
								     - 1];
								conn_num--;
							}
				}
		}

	}

	for (i = 0; i < conn_num; i++)
		close(conn_list[i]);
	close_sockets();
	close(monrx_fd);
	close(monrxtx_fd);
	closelog();
	return 0;
}
