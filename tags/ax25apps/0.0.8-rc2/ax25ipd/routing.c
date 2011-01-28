/* routing.c    Routing table manipulation routines
 *
 * Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.
 * This software may be freely used, distributed, or modified, providing
 * this header is not removed.
 *
 */

#include <stdio.h>
#include "ax25ipd.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <memory.h>
#include <syslog.h>

/* The routing table structure is not visible outside this module. */

struct route_table_entry {
	unsigned char callsign[7];	/* the callsign and ssid */
	unsigned char padcall;	/* always set to zero */
	unsigned char ip_addr[4];	/* the IP address */
	unsigned short udp_port;	/* the port number if udp */
	unsigned char pad1;
	unsigned char pad2;
	unsigned int flags;	/* route flags */
	struct route_table_entry *next;
};

struct route_table_entry *route_tbl;
struct route_table_entry *default_route;

/* The Broadcast address structure is not visible outside this module either */

struct bcast_table_entry {
	unsigned char callsign[7];	/* The broadcast address */
	struct bcast_table_entry *next;
};

struct bcast_table_entry *bcast_tbl;

/* Initialize the routing module */
void route_init(void)
{
	route_tbl = NULL;
	default_route = NULL;
	bcast_tbl = NULL;
}

/* Add a new route entry */
void route_add(unsigned char *ip, unsigned char *call, int udpport,
	unsigned int flags)
{
	struct route_table_entry *rl, *rn;
	int i;

	/* Check we have an IP address */
	if (ip == NULL)
		return;

	/* Check we have a callsign */
	if (call == NULL)
		return;

	/* Find the last entry in the list */
	rl = route_tbl;
	if (route_tbl)
		while (rl->next)
			rl = rl->next;

	rn = (struct route_table_entry *)
	    malloc(sizeof(struct route_table_entry));

	/* Build this entry ... */
	for (i = 0; i < 6; i++)
		rn->callsign[i] = call[i] & 0xfe;
	rn->callsign[6] = (call[6] & 0x1e) | 0x60;
	rn->padcall = 0;
	memcpy(rn->ip_addr, ip, 4);
	rn->udp_port = htons(udpport);
	rn->pad1 = 0;
	rn->pad2 = 0;
	rn->flags = flags;
	rn->next = NULL;

	/* Update the default_route pointer if this is a default route */
	if (flags & AXRT_DEFAULT)
		default_route = rn;

	if (rl)			/* ... the list is already started add the new route */
		rl->next = rn;
	else			/* ... start the list off */
		route_tbl = rn;

	/* Log this entry ... */
	LOGL4("added route: %s %s %s %d %d\n",
	      call_to_a(rn->callsign),
	      (char *) inet_ntoa(*(struct in_addr *) rn->ip_addr),
	      rn->udp_port ? "udp" : "ip", ntohs(rn->udp_port), flags);

	return;
}

/* Add a new broadcast address entry */
void bcast_add(unsigned char *call)
{
	struct bcast_table_entry *bl, *bn;
	int i;

	/* Check we have a callsign */
	if (call == NULL)
		return;

	/* Find the last entry in the list */
	bl = bcast_tbl;
	if (bcast_tbl)
		while (bl->next)
			bl = bl->next;

	bn = (struct bcast_table_entry *)
	    malloc(sizeof(struct bcast_table_entry));

	/* Build this entry ... */
	for (i = 0; i < 6; i++)
		bn->callsign[i] = call[i] & 0xfe;
	bn->callsign[6] = (call[6] & 0x1e) | 0x60;

	bn->next = NULL;

	if (bl)			/* ... the list is already started add the new route */
		bl->next = bn;
	else			/* ... start the list off */
		bcast_tbl = bn;

	/* Log this entry ... */
	LOGL4("added broadcast address: %s\n", call_to_a(bn->callsign));
}

/*
 * Return an IP address and port number given a callsign.
 * We return a pointer to the address; the port number can be found
 * immediately following the IP address. (UGLY coding; to be fixed later!)
 */

unsigned char *call_to_ip(unsigned char *call)
{
	struct route_table_entry *rp;
	unsigned char mycall[7];
	int i;

	if (call == NULL)
		return NULL;

	for (i = 0; i < 6; i++)
		mycall[i] = call[i] & 0xfe;

	mycall[6] = (call[6] & 0x1e) | 0x60;

	LOGL4("lookup call %s ", call_to_a(mycall));

	rp = route_tbl;
	while (rp) {
		if (addrmatch(mycall, rp->callsign)) {
			LOGL4("found ip addr %s\n",
			      (char *) inet_ntoa(*(struct in_addr *)
						 rp->ip_addr));
			return rp->ip_addr;
		}
		rp = rp->next;
	}

	/*
	 * No match found in the routing table, use the default route if
	 * we have one defined.
	 */
	if (default_route) {
		LOGL4("failed, using default ip addr %s\n",
		      (char *) inet_ntoa(*(struct in_addr *)
					 default_route->ip_addr));
		return default_route->ip_addr;
	}

	LOGL4("failed.\n");
	return NULL;
}

/*
 * Accept a callsign and return true if it is a broadcast address, or false
 * if it is not found on the list
 */
int is_call_bcast(unsigned char *call)
{
	struct bcast_table_entry *bp;
	unsigned char bccall[7];
	int i;

	if (call == NULL)
		return (FALSE);

	for (i = 0; i < 6; i++)
		bccall[i] = call[i] & 0xfe;

	bccall[6] = (call[6] & 0x1e) | 0x60;

	LOGL4("lookup broadcast %s ", call_to_a(bccall));

	bp = bcast_tbl;
	while (bp) {
		if (addrmatch(bccall, bp->callsign)) {
			LOGL4("found broadcast %s\n",
			      call_to_a(bp->callsign));
			return (TRUE);
		}
		bp = bp->next;
	}
	return (FALSE);
}

/* Traverse the routing table, transmitting the packet to each bcast route */
void send_broadcast(unsigned char *buf, int l)
{
	struct route_table_entry *rp;

	rp = route_tbl;
	while (rp) {
		if (rp->flags & AXRT_BCAST) {
			send_ip(buf, l, rp->ip_addr);
		}
		rp = rp->next;
	}
}

/* print out the list of routes */
void dump_routes(void)
{
	struct route_table_entry *rp;
	int i;

	for (rp = route_tbl, i = 0; rp; rp = rp->next)
		i++;

	LOGL1("\n%d active routes.\n", i);

	rp = route_tbl;
	while (rp) {
		LOGL1("  %s\t%s\t%s\t%d\t%d\n",
		      call_to_a(rp->callsign),
		      (char *) inet_ntoa(*(struct in_addr *) rp->ip_addr),
		      rp->udp_port ? "udp" : "ip",
		      ntohs(rp->udp_port), rp->flags);
		rp = rp->next;
	}
	fflush(stdout);
}
