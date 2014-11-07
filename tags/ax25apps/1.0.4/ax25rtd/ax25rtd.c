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

/*
 * This daemon tries to learn AX.25, ARP, IP route entries by listening
 * to the AX.25 traffic. It caches up to 256 entries (in "FIFO" mode) 
 * and saves the cache on demand or at shutdown in /var/ax25/ax25rtd/ip_routes 
 * and /var/ax25/ax25rtd/ax25_routes. The configuration file is 
 * /etc/ax25/ax25rtd.conf, you can almost everything configure
 * there. See ax25rtcl.c for runtime maintainance.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#ifdef __GLIBC__
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif

#include <netax25/ax25.h>
#include <netrose/rose.h>
#include <netax25/axconfig.h>
#include <netax25/axlib.h>

#include "../pathnames.h"
#include "ax25rtd.h"

config *Config = NULL;

int reload = 0;

ip_rt_entry *ip_routes = NULL;
int ip_routes_cnt = 0;
int ip_maxroutes = IP_MAXROUTES;

ax25_rt_entry *ax25_routes = NULL;
int ax25_routes_cnt = 0;
int ax25_maxroutes = AX25_MAXROUTES;

char ip_encaps_dev[32] = "";
char iproute2_table[32] = "";

config *dev_get_config(char *dev)
{
	config *config;

	for (config = Config; config; config = config->next)
		if (!strcmp(config->dev, dev))
			return config;

	return port_get_config(dev);
}

config *port_get_config(char *port)
{
	config *config;

	for (config = Config; config; config = config->next)
		if (!strcmp(config->port, port))
			return config;
	return NULL;
}

void sig_reload(int d)
{
	reload = 1;
	signal(SIGHUP, sig_reload);
}

void sig_debug(int d)
{
	fprintf(stderr, "config:\n");
	dump_config(2);
	fprintf(stderr, "ip-routes:\n");
	dump_ip_routes(2, 0);
	fprintf(stderr, "ax25-routes:\n");
	dump_ax25_routes(2, 0);
	signal(SIGUSR1, sig_debug);
}

void sig_term(int d)
{
	save_cache();
	daemon_shutdown(0);
}

void daemon_shutdown(int reason)
{
	unlink(DATA_AX25ROUTED_CTL_SOCK);
	exit(reason);
}

#define FD_MAX(fd) {fd_max = (fd > fd_max? fd : fd_max); FD_SET(fd, &read_fds);}

int main(int argc, char **argv)
{
	unsigned char buf[256];
	int size, s, cntrl_s, cntrl_fd;
	socklen_t cntrl_len;
	struct sockaddr_un cntrl_addr;
	fd_set read_fds, write_fds;
	int fd_max;

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "ax25rtd: no AX.25 port configured\n");
		return 1;
	}

	load_config();
	load_cache();

	if (fork())
		return 0;

	if ((s = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_AX25))) == -1) {
		perror("AX.25 socket");
		return 1;
	}

	if ((cntrl_s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("Control socket");
		return 1;
	}

	unlink(DATA_AX25ROUTED_CTL_SOCK);

	cntrl_addr.sun_family = AF_UNIX;
	strcpy(cntrl_addr.sun_path, DATA_AX25ROUTED_CTL_SOCK);
	cntrl_len =
	    sizeof(cntrl_addr.sun_family) +
	    strlen(DATA_AX25ROUTED_CTL_SOCK);

	if (bind(cntrl_s, (struct sockaddr *) &cntrl_addr, cntrl_len) < 0) {
		perror("bind Control socket");
		daemon_shutdown(1);
	}

	chmod(DATA_AX25ROUTED_CTL_SOCK, 0600);
	listen(cntrl_s, 1);

	signal(SIGUSR1, sig_debug);
	signal(SIGHUP, sig_reload);
	signal(SIGTERM, sig_term);

	cntrl_fd = -1;

	for (;;) {
		fd_max = 0;
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_MAX(s);
		if (cntrl_fd > 0) {
			FD_MAX(cntrl_fd);
			FD_SET(cntrl_fd, &write_fds);
		} else {
			FD_MAX(cntrl_s);
		}

		if (select(fd_max + 1, &read_fds, NULL, &write_fds, NULL) < 0) {
			if (errno == EINTR)	/* woops! */
				continue;

			if (!FD_ISSET(cntrl_fd, &write_fds)) {
				perror("select");
				save_cache();
				daemon_shutdown(1);
			} else {
				close(cntrl_fd);
				cntrl_fd = -1;
				continue;
			}
		}

		if (cntrl_fd > 0) {
			if (FD_ISSET(cntrl_fd, &read_fds)) {
				size = read(cntrl_fd, buf, sizeof(buf));
				if (size > 0) {
					buf[size] = '\0';
					interpret_command(cntrl_fd, buf);
				} else {
					close(cntrl_fd);
					cntrl_fd = -1;
				}
			}
		} else if (FD_ISSET(cntrl_s, &read_fds)) {
			if ((cntrl_fd =
			     accept(cntrl_s,
				    (struct sockaddr *) &cntrl_addr,
				    &cntrl_len)) < 0) {
				perror("accept Control");
				save_cache();
				daemon_shutdown(1);
			}
		}

		if (reload)
			reload_config();

		if (FD_ISSET(s, &read_fds))
			ax25_receive(s);
	}

	return 0;		/* what ?! */
}
