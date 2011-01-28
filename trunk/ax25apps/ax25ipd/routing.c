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
#include <string.h>
#include <pthread.h>

/* The routing table structure is not visible outside this module. */

struct route_table_entry {
	unsigned char callsign[7];	/* the callsign and ssid */
	unsigned char padcall;	/* always set to zero */
	unsigned char ip_addr[4];	/* the IP address */
	unsigned short udp_port;	/* the port number if udp */
	unsigned char pad1;
	unsigned char pad2;
	unsigned int flags;	/* route flags */
	char *hostnm; /* host name */
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

struct callsign_lookup_entry {
	unsigned char callsign[7];
	struct route_table_entry *route;
	struct callsign_lookup_entry *prev, *next;
};

struct callsign_lookup_entry *callsign_lookup;

time_t last_dns_time;
volatile int threadrunning;
pthread_mutex_t dnsmutex = PTHREAD_MUTEX_INITIALIZER;

/* Initialize the routing module */
void route_init(void)
{
	route_tbl = NULL;
	default_route = NULL;
	bcast_tbl = NULL;
	callsign_lookup = NULL;
	last_dns_time = time(NULL);
	threadrunning = 0;
}

/* Add a new route entry */
void route_add(char *host, unsigned char *ip, unsigned char *call, int udpport,
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
	rn->hostnm = strdup(host);
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

/* For route rn, a different IP is being used.  Trigger a DNS check. 
 * as long as DNS hasn't been checked within 5 minutes (300 seconds)
 * */
void route_updatedyndns(struct sockaddr_in *ip_addr, struct route_table_entry *rn)
{
	if (rn->flags & AXRT_LEARN){
		pthread_mutex_lock(&dnsmutex);
		* (unsigned *) rn->ip_addr = (unsigned) ip_addr->sin_addr.s_addr;
		pthread_mutex_unlock(&dnsmutex);
	}
	if (!(rn->flags & AXRT_PERMANENT)){
		LOGL4("received changed ip: %s", call_to_a(rn->callsign));
		update_dns(300);
	}
}

/* Save the calls in a binary tree.  This code links new callsigns 
 * back to an existing route.  No new routes are created. */
void route_updatereturnpath(unsigned char *mycall, struct route_table_entry *rn)
{
	struct callsign_lookup_entry **clookupp = &callsign_lookup;
	struct callsign_lookup_entry *clookup = callsign_lookup;
	for (;;){
		int chk;
		if (!clookup){
			clookup = *clookupp = calloc(sizeof(struct callsign_lookup_entry), 1);
			memcpy(clookup->callsign, mycall, 7);
			LOGL4("added return route: %s %s %s %d\n",
	      			call_to_a(mycall),
				(char *) inet_ntoa(*(struct in_addr *) rn->ip_addr),
	      			rn->udp_port ? "udp" : "ip", ntohs(rn->udp_port));

			clookup->route = rn;
			return;
		}
		chk = addrcompare(mycall, clookup->callsign);
		if (chk > 0){
			clookupp = &clookup->next;
			clookup = *clookupp;
		}
		else if (chk < 0){
			clookupp = &clookup->prev;
			clookup = *clookupp;
			}
		else{
			
			if (clookup->route != rn){
				clookup->route = rn;
			}
			return;
		}	
	}


}


/* Compare ip_addr to the format used by route_table_entry */
int route_ipmatch(struct sockaddr_in *ip_addr, unsigned char *routeip)
{
	unsigned char ipstorage[IPSTORAGESIZE];
	return (unsigned) ip_addr->sin_addr.s_addr == * (unsigned *) retrieveip(routeip, ipstorage);
}


/* Process calls and ip addresses for routing.  The idea behind
 * this code that for the routes listed in the ax25ipd.conf file,
 * only DNS or the given IP's should be used.  But for calls
 * that are not referenced in ax25ipd.conf that arrive using
 * a known gateway IP, the code links them back to that gateway.
 *
 * No new routes are created.  If a callsign comes in from an  
 * unknown gateway, it will not create an entry.
 *
 * if the Callsign for a known route is received, and it does
 * not match the current ip for that route, the update_dns code
 * is triggered, which should fix the IP based on DNS.  
 *
 * */
void route_process(struct sockaddr_in *ip_addr, unsigned char *call)
{
	struct route_table_entry *rp;
	unsigned char mycall[7];
	int i;
	int isroute;

	if (call == NULL)
		return;

	for (i = 0; i < 6; i++)
		mycall[i] = call[i] & 0xfe;

	mycall[6] = (call[6] & 0x1e) | 0x60;

	rp = route_tbl;
	isroute = 0;
	while (rp) {
		if (addrmatch(mycall, rp->callsign)) {
			if (!route_ipmatch(ip_addr, rp->ip_addr)) {
				route_updatedyndns(ip_addr, rp);
				}
			isroute = 1;
		}
		rp = rp->next;
	}				

	// Don't use any of the ip lookup code for the routes.
	// Also, do not set return paths for broadcast callsigns. 
	// Not sure if this ever happens.  But it's no good if it does.
	if (!isroute && !is_call_bcast(call)){
		rp = route_tbl;
		while (rp) {
			// Only do this once. 
			if (route_ipmatch(ip_addr, rp->ip_addr)) {
				route_updatereturnpath(mycall, rp);
				break;
			}
			rp = rp->next;
		}
	}

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
 * It's possible that the thread will update the IP in mid
 * memcpy of an ip address, so I changed all the references to
 * route_table_entry->ip_addr to use this routine
 
 * Note that the printfs don't check this
 */

unsigned char *retrieveip(unsigned char *ip, unsigned char *ipstorage)
{
	if (!ipstorage)return ip;
	pthread_mutex_lock(&dnsmutex);
	memcpy((void *) ipstorage, (void *) ip, IPSTORAGESIZE);
	pthread_mutex_unlock(&dnsmutex);
	return ipstorage;
}

/*
 * Return an IP address and port number given a callsign.
 * We return a pointer to the address; the port number can be found
 * immediately following the IP address. (UGLY coding; to be fixed later!)
 */
 
unsigned char *call_to_ip(unsigned char *call, unsigned char *ipstorage)
{
	struct route_table_entry *rp;
	unsigned char mycall[7];
	int i;
	struct callsign_lookup_entry *clookup;
	
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
			return retrieveip(rp->ip_addr, ipstorage);
		}
		rp = rp->next;
	}

	/* Check for callsigns that have been heard on known routes */
	clookup = callsign_lookup;
	for (;;){
		int chk;
		if (!clookup){
			break;
		}
		chk = addrcompare(mycall, clookup->callsign);
		if (chk > 0){
			clookup = clookup->next;
		}
		else if (chk < 0){
			clookup = clookup->prev;
		}
		else{
			LOGL4("found cached ip addr %s\n",
			      (char *) inet_ntoa(*(struct in_addr *)
						 clookup->route->ip_addr));
	
			return retrieveip(clookup->route->ip_addr, ipstorage);
		}	
	}


	/*
	 * No match found in the routing table, use the default route if
	 * we have one defined.
	 */
	if (default_route) {
		LOGL4("failed, using default ip addr %s\n",
		      (char *) inet_ntoa(*(struct in_addr *)
					 default_route->ip_addr));
		return retrieveip(default_route->ip_addr, ipstorage);
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
			unsigned char ipstorage[IPSTORAGESIZE];

			send_ip(buf, l, retrieveip(rp->ip_addr, ipstorage));
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
		LOGL1("  %s %s %s %d %d\n",
		      call_to_a(rp->callsign),
		      (char *) inet_ntoa(*(struct in_addr *) rp->ip_addr),
		      rp->udp_port ? "udp" : "ip",
		      ntohs(rp->udp_port), rp->flags);
		rp = rp->next;
	}
	fflush(stdout);
}

/* Update the IPs of DNS entries for all routes */
void *update_dnsthread(void *arg)
{
	struct hostent *he;
	struct route_table_entry *rp;

	rp = route_tbl;
	while (rp) {
		// If IP is 0,  DNS lookup failed at startup.
		// So check DNS even though it's permanent.
		// I think checking the lock on this ip_addr reference
		// isn't needed since the main code doesn't change
		// ip_addr after startup, and worst case is that it
		// does one extra dns check
		if ((rp->hostnm[0])  && 
			  (!(rp->flags & AXRT_PERMANENT) || ( * (unsigned *) rp->ip_addr == 0) )){
			LOGL4("Checking DNS for %s\n", rp->hostnm);
			he = gethostbyname(rp->hostnm);
			if (he != NULL) {
				pthread_mutex_lock(&dnsmutex);
				* (unsigned *) rp->ip_addr = * (unsigned *) he->h_addr_list[0];  
				pthread_mutex_unlock(&dnsmutex);
				LOGL4("DNS returned IP=%s\n", (char *) inet_ntoa(*(struct in_addr *) rp->ip_addr)); 
			}
		}
		rp = rp->next;
	}
	threadrunning = 0;
	pthread_exit(0);
	return 0;
}

/* check DNS for route IPs.  Packets from system whose ip 
 * has changed  trigger a DNS lookup. The 
 * the timer is needed when both systems are on dynamic
 * IPs and they both reset at about the same time.  Also
 * when the IP changes immediately, but DNS lags.  
 */
void update_dns(unsigned wait)
{
	pthread_t dnspthread;
	int rc;

	if (threadrunning)return;// Don't start a thread when one is going already.
	if (wait && (time(NULL) < last_dns_time + wait))return;
	threadrunning = 1;
	LOGL4("Starting DNS thread\n");
	last_dns_time = time(NULL);
	rc = pthread_create(&dnspthread, NULL, update_dnsthread,  (void *) 0);
	if (rc){
		LOGL1("    Thread err%d\n", rc); // Should we exit here?
		threadrunning = 0;
	}	
		
}

