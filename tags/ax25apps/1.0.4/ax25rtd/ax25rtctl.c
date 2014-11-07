/*
 * Copyright (c) 1996 Joerg Reuter (jreuter@poboxes.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>

#include <config.h>
#include "../pathnames.h"

static const struct option lopts[] = {
	{"add", 1, 0, 'a'},
	{"del", 1, 0, 'd'},
	{"list", 1, 0, 'l'},
	{"expire", 1, 0, 'e'},
	{"save", 0, 0, 's'},
	{"reload", 0, 0, 'r'},
	{"shutdown", 0, 0, 'q'},
	{"Version", 0, 0, 'V'},
	{"help", 0, 0, 'h'},
	{"debug", 0, 0, 'x'},
	{"version", 0, 0, 'v'},
	{NULL, 0, 0, 0}
};

static const char *sopts = "a:d:l:e:srqvVh";

static void usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr,
		"ax25rtctl -a|--add ax25 <callsign> <dev> <time> [digipeater]\n");
	fprintf(stderr,
		"          -a|--add ip <ip> <dev> <time> <call> <ipmode>\n");
	fprintf(stderr, "          -d|--del ax25 <callsign> <dev>\n");
	fprintf(stderr, "          -d|--del ip <ip>\n");
	fprintf(stderr, "          -l|--list ax25|ip\n");
	fprintf(stderr, "          -e|--expire <minutes>\n");
	fprintf(stderr, "          -s|--save\n");
	fprintf(stderr, "          -r|--reload\n");
	fprintf(stderr, "          -q|--shutdown\n");
	fprintf(stderr, "          -V|--Version\n");
	fprintf(stderr, "	  -h|--help\n");
	fprintf(stderr, "          -v|--version\n");
	exit(1);
}

static int open_socket(void)
{
	int sock, addrlen;
	struct sockaddr_un addr;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("ax25rtctl socket");
		exit(1);
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, DATA_AX25ROUTED_CTL_SOCK);
	addrlen =
	    strlen(DATA_AX25ROUTED_CTL_SOCK) + sizeof(addr.sun_family);

	if (connect(sock, (struct sockaddr *) &addr, addrlen) < 0) {
		perror("ax25rtctl connect");
		exit(1);
	}

	return sock;
}

static int wsock(int sock, char *s)
{
	return write(sock, s, strlen(s));
}

static char *get_next_arg(char **p)
{
	char *p2;

	if (p == NULL || *p == NULL)
		return NULL;

	p2 = *p;
	for (; *p2 && *p2 == ' '; p2++);
	if (!*p2)
		return NULL;

	*p = strchr(p2, ' ');
	if (*p != NULL) {
		**p = '\0';
		(*p)++;
	}

	return p2;
}

static void list_ax25(void)
{
	int sock, len, offs;
	char buf[512], *b, *s, *digi, *call, *dev, *ct;
	time_t t;

	sock = open_socket();

	wsock(sock, "list ax25\n");

	offs = 0;

	printf("Callsign  Port   Last update         Path\n");

/*
                DB0PRA-15 scc3   Tue Aug  6 16:35:38 1996 
*/

	while (1) {
		len = read(sock, buf + offs, sizeof(buf) - offs - 1);
		if (len <= 0) {
			close(sock);
			return;
		}

		buf[len + offs] = '\0';
		s = buf;
		for (s = buf; (b = strchr(s, '\n')) != NULL; s = b + 1) {
			*b = '\0';

			if (s[0] == '.') {
				close(sock);
				return;
			}

			call = get_next_arg(&s);
			dev = get_next_arg(&s);
			t = strtol(get_next_arg(&s), NULL, 16);

			if (t == 0) {
				ct = "(permanent)";
				ct = strdup(ct);
			} else {
				ct = strdup(ctime(&t));
				ct[strlen(ct) - 6] = '\0';
			}

			printf("%-9s %-6s %s", call, dev, ct);

			free(ct);

			while ((digi = get_next_arg(&s)) != NULL)
				printf(" %s", digi);

			printf("\n");
		}

		if (b == NULL && s != NULL) {
			offs = strlen(s);
			if (offs)
				memcpy(buf, s, offs);
		}
	}

	close(sock);
}

static void list_ip(void)
{
	int sock, len, offs;
	char buf[512], *b, *s, *ip, *call, *dev, *ct, *mode;
	time_t t;

	sock = open_socket();

	wsock(sock, "list ip\n");

	offs = 0;
	printf("IP Address      Port   Callsign  Mode Last update\n");

/*	
	        255.255.255.255 scc3   DB0PRA-15 v    Thu Jan  7 06:54:19 1971
 */
	while (1) {
		len = read(sock, buf + offs, sizeof(buf) - offs - 1);
		if (len <= 0) {
			close(sock);
			return;
		}

		buf[len + offs] = '\0';
		s = buf;
		for (s = buf; (b = strchr(s, '\n')) != NULL; s = b + 1) {
			*b = '\0';

			if (s[0] == '.') {
				close(sock);
				return;
			}

			ip = get_next_arg(&s);
			dev = get_next_arg(&s);
			t = strtol(get_next_arg(&s), NULL, 16);
			call = get_next_arg(&s);
			mode = get_next_arg(&s);

			if (t == 0) {
				ct = "(permanent)";
			} else {
				ct = ctime(&t);
				ct[strlen(ct) - 6] = '\0';
			}

			printf("%-15s %-6s %-9s %-4s %s\n", ip, dev, call,
			       mode, ct);
		}

		if (b == NULL && s != NULL) {
			offs = strlen(s);
			if (offs)
				memcpy(buf, s, offs);
		}
	}

	close(sock);
}

static void Version(void)
{
	int sock;
	char buf[256];

	printf("ax25rtctl version " VERSION "\n");
	sock = open_socket();
	wsock(sock, "version\n");
	read(sock, buf, sizeof(buf));
	printf("%s", buf);
	close(sock);
}

static void debug(void)
{
	int sock, n, k;
	unsigned char buf[256];
	fd_set read_fds, write_fds;
	struct timeval tv;


	sock = open_socket();

	while (1) {
		FD_ZERO(&read_fds);
		FD_SET(0, &read_fds);
		FD_SET(sock, &read_fds);

		FD_ZERO(&write_fds);
		FD_SET(sock, &write_fds);

		tv.tv_sec = 0;
		tv.tv_usec = 0;
		if (select(sock + 1, &read_fds, NULL, &write_fds, &tv) < 0) {
			perror("socket gone");
			exit(1);
		}

		if (FD_ISSET(sock, &write_fds))
			exit(0);

		if (FD_ISSET(0, &read_fds)) {
			n = read(0, buf, sizeof(buf));
			if (n) {
				k = write(sock, buf, n);
				if (k <= 0)
					exit(0);
			}
		}

		if (FD_ISSET(sock, &read_fds)) {
			n = read(sock, buf, sizeof(buf));
			if (n)
				write(0, buf, n);
		}
	}
}

int main(int argc, char **argv)
{
	int sock, cmd, k, len;
	unsigned char buf[256];
	int opt_ind = 0;
	long when;

	cmd = getopt_long(argc, argv, sopts, lopts, &opt_ind);
	switch (cmd) {
	case 'a':
		if (!strcmp(optarg, "ax25")) {
			if (argc < optind + 3)
				usage();

			len = sprintf(buf, "add ax25");
			for (k = optind; k < argc; k++)
				len += sprintf(buf + len, " %s", argv[k]);
			sprintf(buf + len, "\n");

			sock = open_socket();
			wsock(sock, buf);
			close(sock);
		} else if (!strcmp(optarg, "ip")) {
			if (argc < optind + 5)
				usage();

			len = sprintf(buf, "add ip");
			for (k = optind; k < argc; k++)
				len += sprintf(buf + len, " %s", argv[k]);
			sprintf(buf + len, "\n");

			sock = open_socket();
			wsock(sock, buf);
			close(sock);
		} else
			usage();
		return 0;
	case 'd':
		if (!strcmp(optarg, "ax25")) {
			if (argc < optind + 2)
				usage();

			sprintf(buf, "del ax25 %s %s\n", argv[optind],
				argv[optind + 1]);

			sock = open_socket();
			wsock(sock, buf);
			close(sock);
		} else if (!strcmp(optarg, "ip")) {
			if (argc < optind + 1)
				usage();

			sprintf(buf, "del ip %s\n", argv[optind]);

			sock = open_socket();
			wsock(sock, buf);
			close(sock);
		} else
			usage();
		return 0;
	case 'l':
		if (!strcmp(optarg, "ax25"))
			list_ax25();
		else if (!strcmp(optarg, "ip"))
			list_ip();
		else
			usage();
		return 0;
	case 'e':
		when = atoi(optarg);
		if (when <= 0)
			usage();
		sock = open_socket();
		sprintf(buf, "expire %ld\n", when);
		wsock(sock, buf);
		close(sock);
		return 0;
	case 's':
		sock = open_socket();
		wsock(sock, "save\n");
		close(sock);
		return 0;
	case 'r':
		sock = open_socket();
		wsock(sock, "reload\n");
		close(sock);
		return 0;
	case 'q':
		sock = open_socket();
		wsock(sock, "shutdown\n");
		close(sock);
		return 0;
	case 'V':
		Version();
		return 0;
	case 'v':
		printf("ax25rtctl: %s\n", VERSION);
		return 0;
	case 'x':
		debug();
	case ':':
	case 'h':
	case '?':
	default:
		usage();
	}
	usage();
	return 1;
}
