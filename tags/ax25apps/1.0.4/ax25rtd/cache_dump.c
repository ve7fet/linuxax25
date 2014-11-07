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
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <config.h>
#include <netax25/ax25.h>
#include <netrose/rose.h>
#include <netax25/axlib.h>

#include "ax25rtd.h"

void dump_ip_routes(int fd, int cmd)
{
	ip_rt_entry *bp;
	config *config;
	char buf[256], *dev;
	unsigned long ip;
	int len;

	for (bp = ip_routes; bp; bp = bp->next) {
		ip = htonl(bp->ip);

		if (cmd) {
			len = sprintf(buf, "add ip ");
			dev = bp->iface;
		} else {
			len = 0;
			config = dev_get_config(bp->iface);
			if (config != NULL)
				dev = config->port;
			else
				dev = bp->iface;
		}

		len += sprintf(buf + len, "%d.%d.%d.%d",
			       (int) ((ip & 0xFF000000) >> 24),
			       (int) ((ip & 0x00FF0000) >> 16),
			       (int) ((ip & 0x0000FF00) >> 8),
			       (int) (ip & 0x000000FF));

		len +=
		    sprintf(buf + len, " %-4s %8.8lx %-9s ", dev,
			    bp->timestamp, ax25_ntoa(&bp->call));
		if (bp->invalid)
			len += sprintf(buf + len, "X\n");
		else
			len +=
			    sprintf(buf + len, "%c\n",
				    bp->ipmode ? 'v' : 'd');

		write(fd, buf, len);
	}

	if (!cmd)
		write(fd, ".\n", 2);

}

void dump_ax25_routes(int fd, int cmd)
{
	ax25_rt_entry *bp;
	config *config;
	char buf[256], *dev;
	int k, len;

	for (bp = ax25_routes; bp; bp = bp->next) {
		if (cmd) {
			len = sprintf(buf, "add ax25 ");
			dev = bp->iface;
		} else {
			len = 0;
			config = dev_get_config(bp->iface);
			if (config != NULL)
				dev = config->port;
			else
				dev = bp->iface;
		}

		len +=
		    sprintf(buf + len, "%-9s %-4s %8.8lx",
			    ax25_ntoa(&bp->call), dev, bp->timestamp);

		for (k = 0; k < bp->ndigi; k++)
			len +=
			    sprintf(buf + len, " %s",
				    ax25_ntoa(&bp->digipeater[k]));
		len += sprintf(buf + len, "\n");
		write(fd, buf, len);
	}

	if (!cmd)
		write(fd, ".\n", 2);
}

void dump_config(int fd)
{
	config *config;
	int k;

	fprintf(stderr, "config:\n");
	for (config = Config; config; config = config->next) {
		fprintf(stderr, "Device           = %s\n", config->dev);
		fprintf(stderr, "Port             = %s\n", config->port);
		fprintf(stderr, "ax25_add_route   = %d\n",
			config->ax25_add_route);
		fprintf(stderr, "ax25_for_me      = %d\n",
			config->ax25_for_me);
		fprintf(stderr, "ax25_add_default = %d\n",
			config->ax25_add_default);
		fprintf(stderr, "ip_add_route     = %d\n",
			config->ip_add_route);
		fprintf(stderr, "ip_add_arp       = %d\n",
			config->ip_add_arp);
		fprintf(stderr, "ip_adjust_mode   = %d\n",
			config->ip_adjust_mode);
		fprintf(stderr, "netmask          = %8.8lx\n",
			config->netmask);
		fprintf(stderr, "ip               = %8.8lx\n", config->ip);
		fprintf(stderr, "nmycalls         = %d\n",
			config->nmycalls);
		fprintf(stderr, "calls            =");
		for (k = 0; k < config->nmycalls; k++)
			fprintf(stderr, " %s",
				ax25_ntoa(&config->mycalls[k]));
		fprintf(stderr, "\n");
		fprintf(stderr, "ax25_default_path=");
		for (k = 0;
		     k < config->ax25_default_path.fsa_ax25.sax25_ndigis;
		     k++)
			fprintf(stderr, " %s",
				ax25_ntoa(&config->ax25_default_path.
					  fsa_digipeater[k]));
		fprintf(stderr, "\n.\n");
	}
}
