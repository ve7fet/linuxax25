#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/route.h>


#include "rip98d.h"

#define	UNMATCH_ROUTE		0
#define	NO_ROUTE		1
#define	REPLACE_ROUTE		2
#define	ADDITIONAL_ROUTE	3

static int cmp_route(struct route_struct *route, struct in_addr addr, int bits, int metric)
{
	unsigned long int old_mask, new_mask;
	unsigned long int old_addr, new_addr;

	old_mask = ntohl(bits2mask(route->bits));
	new_mask = ntohl(bits2mask(bits));

	old_addr = route->addr.s_addr;
	new_addr = addr.s_addr;

	if (bits < route->bits) {
		new_addr = ntohl(new_addr) & new_mask;
		old_addr = ntohl(old_addr) & new_mask;
	} else {
		new_addr = ntohl(new_addr) & old_mask;
		old_addr = ntohl(old_addr) & old_mask;
	}

	if (route->action == DEL_ROUTE || route->action == NEW_ROUTE)
		return UNMATCH_ROUTE;
		
	if (old_addr != new_addr)
		return UNMATCH_ROUTE;

	if (route->metric <= metric)
		return NO_ROUTE;

	if (debug && logging) {
		syslog(LOG_DEBUG, "comparing old: %s/%d to\n", inet_ntoa(route->addr), route->bits);
		syslog(LOG_DEBUG, "          new: %s/%d\n", inet_ntoa(addr), bits);
	}

	if (route->bits <= bits) {
		if (route->action == ORIG_ROUTE) {
			if (debug && logging)
				syslog(LOG_DEBUG, "    better route, replacing existing route\n");
			return REPLACE_ROUTE;
		} else {
			if (debug && logging)
				syslog(LOG_DEBUG, "    better route, adding additional route\n");
			return ADDITIONAL_ROUTE;
		}
	} else {
		if (debug && logging)
			syslog(LOG_DEBUG, "    better route, adding additional route\n");
		return ADDITIONAL_ROUTE;
	}
}

void receive_routes(int s)
{
	unsigned char message[500], *p;
	struct sockaddr_in rem_addr;
	struct in_addr addr;
	struct route_struct *route, *new;
	struct sockaddr_in trg;
	struct rtentry rt;
	unsigned long int netmask;
	unsigned long int network;
	int bits, metric;
	int found, matched;
	socklen_t size;
	int mess_len;
	int i;

	size = sizeof(struct sockaddr_in);

	mess_len = recvfrom(s, message, 500, 0, (struct sockaddr *)&rem_addr, &size);

	if (message[0] != RIPCMD_RESPONSE || message[1] != RIP_VERSION_98) {
		if (debug && logging) {
			syslog(LOG_DEBUG, "invalid RIP98 header received\n");
			syslog(LOG_DEBUG, "    cmd: %d vers: %d\n", message[0], message[1]);
		}
		return;
	}

	for (found = FALSE, i = 0; i < dest_count; i++) {
		if (rem_addr.sin_addr.s_addr == dest_list[i].dest_addr.s_addr) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		if (debug && logging)
			syslog(LOG_DEBUG, "RIP98 message from unknown address %s\n", inet_ntoa(rem_addr.sin_addr));
		return;
	}

	if (debug && logging)
		syslog(LOG_DEBUG, "RIP98 message received from %s\n", inet_ntoa(rem_addr.sin_addr));

	for (p = message + RIP98_HEADER; p < message + mess_len; p += RIP98_ENTRY) {
		memcpy((char *)&addr, (char *)p, sizeof(addr));
		bits   = p[4];
		metric = p[5];	

		network = inet_netof(addr);

		if (network == 0 || network == 127) {
			if (debug && logging)
				syslog(LOG_DEBUG, "    route to %s/%d metric %d - rejected\n", inet_ntoa(addr), bits, metric);
			continue;
		}

		if (restrict) {
			if (network != 44) {
				if (debug && logging)
					syslog(LOG_DEBUG, "    route to %s/%d metric %d - rejected\n", inet_ntoa(addr), bits, metric);
				continue;
			}
		}

		if (debug && logging)
			syslog(LOG_DEBUG, "    route to %s/%d metric %d\n", inet_ntoa(addr), bits, metric);

		metric++;
		
		if (metric > RIP98_INFINITY)
			metric = RIP98_INFINITY;

		found   = FALSE;
		matched = FALSE;

		for (route = first_route; route != NULL; route = route->next) {

			switch (cmp_route(route, addr, bits, metric)) {

				case NO_ROUTE:
					matched = TRUE;
					break;

				case REPLACE_ROUTE:
					route->action = DEL_ROUTE;
				
				case ADDITIONAL_ROUTE:
					if (!found) {
						if ((new = malloc(sizeof(struct route_struct))) == NULL) {
							if (logging)
								syslog(LOG_ERR, "out of memory\n");
							return;
						}

						new->addr   = addr;
						new->bits   = bits;
						new->metric = metric;
						new->action = NEW_ROUTE;
				
						new->next   = first_route;
						first_route = new;
	
						found = TRUE;
					}

					matched = TRUE;
					break;

				default:
					break;
			}
		}

		if (!matched) {
			if ((new = malloc(sizeof(struct route_struct))) == NULL) {
				if (logging)
					syslog(LOG_ERR, "out of memory\n");
				return;
			}

			new->addr   = addr;
			new->bits   = bits;
			new->metric = metric;
			new->action = NEW_ROUTE;
				
			new->next   = first_route;
			first_route = new;
		}
	}
	
	for (route = first_route; route != NULL; route = route->next) {
		if (route->action == DEL_ROUTE) {
			memset((char *)&rt, 0, sizeof(rt));

			trg.sin_family = AF_INET;
			trg.sin_addr   = route->addr;
			trg.sin_port   = 0;
			memcpy((char *)&rt.rt_dst, (char *)&trg, sizeof(struct sockaddr));

			if (ioctl(s, SIOCDELRT, &rt) < 0) {
				if (logging)
					syslog(LOG_ERR, "SIOCDELRT: %m");
			}
		}
	}

	for (route = first_route; route != NULL; route = route->next) {
		if (route->action == NEW_ROUTE) {
			memset((char *)&rt, 0, sizeof(rt));

			trg.sin_family = AF_INET;
			trg.sin_addr   = route->addr;
			trg.sin_port   = 0;
			memcpy((char *)&rt.rt_dst, (char *)&trg, sizeof(struct sockaddr));

			rt.rt_flags = RTF_UP | RTF_GATEWAY | RTF_DYNAMIC;

			if (route->bits == 32) {
				rt.rt_flags |= RTF_HOST;
			} else {
				netmask = bits2mask(route->bits);
			
				trg.sin_family = AF_INET;
				memcpy((char *)&trg.sin_addr, (char *)&netmask, sizeof(struct in_addr));
				trg.sin_port   = 0;
				memcpy((char *)&rt.rt_genmask, (char *)&trg, sizeof(struct sockaddr));
			}

			rt.rt_metric = route->metric + 1;

			trg.sin_family = AF_INET;
			trg.sin_addr   = rem_addr.sin_addr;
			trg.sin_port   = 0;
			memcpy((char *)&rt.rt_gateway, (char *)&trg, sizeof(struct sockaddr));

			if (ioctl(s, SIOCADDRT, &rt) < 0) {
				if (logging)
					syslog(LOG_ERR, "SIOCADDRT: %m");
			}
		}
	}
}
