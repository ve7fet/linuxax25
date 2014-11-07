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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

#include <config.h>
#include <netax25/ax25.h>

#include "ax25rtd.h"

/* Hmm, we really should implement this in C++ */
/* But I HATE it. */

/* (later: haven't I seen this statement elsewere? hmm...) */

int update_ip_route(config * config, unsigned long ip, int ipmode,
		    ax25_address * call, time_t timestamp)
{
	ip_rt_entry *bp = ip_routes;
	ip_rt_entry *bp_prev = ip_routes;
	char *iface;
	int action = 0;

	if (((ip ^ config->ip) & config->netmask) != 0)
		return 0;

	iface = config->dev;

	while (bp) {
		if (bp->ip == ip) {
			if (bp->timestamp == 0 && timestamp != 0)
				return 0;

			if (strcmp(bp->iface, iface)) {
				action |= NEW_ROUTE;
				strcpy(bp->iface, iface);
			}

			if (memcmp(&bp->call, call, AXLEN)) {
				action |= NEW_ARP;
				memcpy(&bp->call, call, AXLEN);
			}

			if (ipmode != bp->ipmode) {
				action |= NEW_IPMODE;
				bp->ipmode = ipmode;
			}

			bp->timestamp = timestamp;

			if (bp != ip_routes) {
				if (bp->next)
					bp->next->prev = bp->prev;
				if (bp->prev)
					bp->prev->next = bp->next;

				bp->next = ip_routes;
				bp->prev = NULL;
				ip_routes->prev = bp;
				ip_routes = bp;
			}

			return action;
		}

		bp_prev = bp;
		bp = bp->next;
	}

	if (ip_routes_cnt >= ip_maxroutes) {
		if (bp_prev == NULL)	/* error */
			return 0;

		bp_prev->prev->next = NULL;
		free(bp_prev);
		ip_routes_cnt--;
	}

	bp = (ip_rt_entry *) malloc(sizeof(ip_rt_entry));
	if (bp == NULL)
		return 0;

	ip_routes_cnt++;
	action = NEW_ROUTE | NEW_ARP | NEW_IPMODE;
	bp->ipmode = ipmode;
	bp->ip = ip;
	bp->invalid = 0;

	bp->timestamp = timestamp;
	strcpy(bp->iface, iface);
	memcpy(&bp->call, call, AXLEN);

	if (ip_routes == NULL) {
		ip_routes = bp;
		bp->next = bp->prev = NULL;
		return action;
	}

	bp->next = ip_routes;
	bp->prev = NULL;
	ip_routes->prev = bp;
	ip_routes = bp;

	return action;
}


ax25_rt_entry *update_ax25_route(config * config, ax25_address * call,
				 int ndigi, ax25_address * digi,
				 time_t timestamp)
{
	ax25_rt_entry *bp = ax25_routes;
	ax25_rt_entry *bp_prev = ax25_routes;
	unsigned char *iface = config->dev;
	int action = 0;

	while (bp) {
		if (!memcmp(call, &bp->call, AXLEN)) {
			if (bp->timestamp == 0 && timestamp != 0)
				return NULL;

			if (strcmp(bp->iface, iface)) {
				del_kernel_ax25_route(bp->iface,
						      &bp->call);
				action |= NEW_ROUTE;
				strcpy(bp->iface, iface);
			}

			if (ndigi != bp->ndigi
			    || memcmp(bp->digipeater, digi,
				      bp->ndigi * AXLEN)) {
				action |= NEW_ROUTE;
				memcpy(bp->digipeater, digi,
				       ndigi * AXLEN);
				bp->ndigi = ndigi;
			}

			bp->timestamp = timestamp;

			if (bp != ax25_routes) {
				if (bp->next)
					bp->next->prev = bp->prev;
				if (bp->prev)
					bp->prev->next = bp->next;

				bp->next = ax25_routes;
				bp->prev = NULL;
				ax25_routes->prev = bp;
				ax25_routes = bp;
			}

			if (action)
				return bp;
			else
				return NULL;
		}

		bp_prev = bp;
		bp = bp->next;
	}

	if (ax25_routes_cnt >= ax25_maxroutes) {
		if (bp_prev == NULL)	/* error */
			return NULL;

		bp_prev->prev->next = NULL;
		free(bp_prev);
		ax25_routes_cnt--;
	}

	bp = (ax25_rt_entry *) malloc(sizeof(ax25_rt_entry));
	if (bp == NULL)
		return NULL;

	ax25_routes_cnt++;

	bp->timestamp = timestamp;
	strcpy(bp->iface, iface);
	bp->call = *call;

	if (ndigi)
		memcpy(bp->digipeater, digi, ndigi * AXLEN);

	bp->ndigi = ndigi;

	if (ax25_routes == NULL) {
		ax25_routes = bp;
		bp->next = bp->prev = NULL;
		return bp;
	}

	bp->next = ax25_routes;
	bp->prev = NULL;
	ax25_routes->prev = bp;
	ax25_routes = bp;

	return bp;
}

ip_rt_entry *remove_ip_route(ip_rt_entry * bp)
{
	ip_rt_entry *bp2;

	bp2 = bp->next;
	if (bp2)
		bp2->prev = bp->prev;
	if (bp->prev)
		bp->prev->next = bp2;
	if (bp == ip_routes)
		ip_routes = bp2;

	del_kernel_ip_route(bp->iface, bp->ip);

	free(bp);
	ip_routes_cnt--;
	return bp2;
}

ax25_rt_entry *remove_ax25_route(ax25_rt_entry * bp)
{
	ax25_rt_entry *bp2;
	ip_rt_entry *iprt;

	bp2 = bp->next;
	if (bp2)
		bp2->prev = bp->prev;
	if (bp->prev)
		bp->prev->next = bp2;
	if (bp == ax25_routes)
		ax25_routes = bp2;

	for (iprt = ip_routes; iprt; iprt = iprt->next)
		if (!memcmp(&iprt->call, &bp->call, AXLEN))
			remove_ip_route(iprt);

	del_kernel_ax25_route(bp->iface, &bp->call);

	free(bp);
	ax25_routes_cnt--;
	return bp2;
}

int del_ip_route(unsigned long ip)
{
	ip_rt_entry *bp;

	if (ip == 0)
		return 1;

	for (bp = ip_routes; bp; bp = bp->next)
		if (bp->ip == ip) {
			remove_ip_route(bp);
			return 0;
		}

	return 1;
}

int invalidate_ip_route(unsigned long ip)
{
	ip_rt_entry *bp;

	for (bp = ip_routes; bp; bp = bp->next)
		if (bp->ip == ip) {
			bp->invalid = 1;
			return 1;
		}

	return 0;
}

int del_ax25_route(config * config, ax25_address * call)
{
	ax25_rt_entry *bp;

	for (bp = ax25_routes; bp; bp = bp->next)
		if (!memcmp(call, &bp->call, AXLEN)
		    && !strcmp(config->dev, bp->iface)) {
			remove_ax25_route(bp);
			return 0;
		}

	return 1;
}


void expire_ax25_route(time_t when)
{
	ax25_rt_entry *bp;
	time_t now = time(NULL);

	for (bp = ax25_routes; bp;)
		if (bp->timestamp != 0 && bp->timestamp + when <= now)
			bp = remove_ax25_route(bp);
		else
			bp = bp->next;
}

void expire_ip_route(time_t when)
{
	ip_rt_entry *bp;
	time_t now = time(NULL);

	for (bp = ip_routes; bp;)
		if (bp->timestamp != 0 && bp->timestamp + when <= now)
			bp = remove_ip_route(bp);
		else
			bp = bp->next;
}
