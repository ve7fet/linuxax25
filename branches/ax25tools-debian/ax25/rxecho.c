/*
 * rxecho.c - Copies AX.25 packets from an interface to another interface.
 *            Reads CONFIGFILE (see below) and uses that information to
 *            decide what packets should be copied and where.
 *
 *            CONFIGFILE format is:
 *
 *              # this is a comment
 *              144	kiss0	oh2bns-1,oh2bns-2
 *              kiss0	144	*
 *
 *            This means that packets received on port 144 are copied to port
 *            kiss0 if they are destined to oh2bns-1 or oh2bns-2. Packets
 *            from port kiss0 are all copied to port 144.
 *
 *            There may be empty lines and an arbirary amount of white
 *            space around the tokens but the callsign field must not
 *            have any spaces in it. There can be up to MAXCALLS call-
 *            signs in the callsign field (see below).
 *
 * Copyright (C) 1996 by Tomi Manninen, OH2BNS, <oh2bns@sral.fi>.
 *
 * *** Modified 9/9/96 by Heikki Hannikainen, OH7LZB, <oh7lzb@sral.fi>:
 *
 *            One port can actually be echoed to multiple ports (with a
 *            different recipient callsign, of course). The old behaviour was
 *            to give up on the first matching port, even if the recipient
 *            callsign didn't match (and the frame wasn't echoed anywhere).
 *
 * *** 20021206 dl9sau:
 * 	      - fixed a bug preventing echo to multible ports; it may also
 *              lead to retransmission on the interface where it came from
 *            - fixed problem that frames via sendto(...,alen) had a wrong
 *              protocol (because alen became larger than the size of
 *              struct sockaddr).
 *            - sockaddr_pkt is the right struct for recvfrom/sendto on
 *              type SOCK_PACKET family AF_INET sockets.
 *	      - added support for new PF_PACKET family with sockaddr_ll
 *
 * ***
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>

#include <config.h>

#include <sys/socket.h>

/*
 * dl9sau:
 * uncomment this if you have problems with sockaddr_pkt.
 * sockaddr_pkt is the right way for type SOCK_PACKET on family AF_INET sockets.
 * especially because the "sockaddr" on recvfrom is truncated (internaly
 * it's sockaddr_pkt) -- and because we use this sockaddr for retransmitting,
 * it's really better to use the sockaddr_spkt.
 * default is to use SOCKADDR_SPKT
 */
#define	USE_SOCKADDR_SPKT	1

/* dl9sau: since linux 2.2.x, SOCK_PACKET is obsolete  */
#define	USE_SOCKADDR_SLL	1

#ifdef	USE_SOCKADDR_SLL
#undef	USE_SOCKADDR_SPKT
#endif

#include <features.h>    /* for the glibc version number */
#if __GLIBC__ >= 2
#ifdef	USE_SOCKADDR_SPKT
#include <net/if_packet.h>
#endif
#ifdef	USE_SOCKADDR_SLL
#include <sys/ioctl.h>
#include <net/if.h>
#include <netpacket/packet.h>
#endif
#include <net/ethernet.h>
#else
#if defined(USE_SOCKADDR_SPKT) || defined(USE_SOCKADDR_SLL)
#include <asm/types.h>
#include <linux/if_packet.h>
#endif
#include <linux/if_ether.h>
#endif

#include <netinet/in.h>

#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/daemon.h>

#include "../pathnames.h"

#define MAXCALLS	8

struct config {
	char		from[14];	/* sockaddr.sa_data is 14 bytes	*/
	int		from_idx;
	char		to[14];
	int		to_idx;
	ax25_address	calls[MAXCALLS];/* list of calls to echo	*/
	int		ncalls;		/* number of calls to echo	*/

	struct config	*next;
};

static int logging = FALSE;

static void terminate(int sig)
{
	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}
	
	exit(0);
}

/*
 * Read string "call1,call2,call3,..." into p.
 */
static int read_calls(struct config *p, char *s)
{
	char *cp, *cp1;

	if (p == NULL || s == NULL)
		return -1;

	p->ncalls = 0;

	if (strcmp(s, "*") == 0)
		return 0;

	cp = s;

	while ((cp1 = strchr(cp, ',')) != NULL && p->ncalls < MAXCALLS) {
		*cp1 = 0;

		if (ax25_aton_entry(cp, p->calls[p->ncalls].ax25_call) == -1)
			return -1;

		p->ncalls++;
		cp = ++cp1;
	}

	if (p->ncalls < MAXCALLS) {
		if (ax25_aton_entry(cp, p->calls[p->ncalls].ax25_call) == -1)
			return -1;

		p->ncalls++;
	}

	return p->ncalls;
}

static struct config *readconfig(void)
{
	FILE *fp;
	char line[80], *cp, *dev;
	struct config *p, *list = NULL;

	if ((fp = fopen(CONF_RXECHO_FILE, "r")) == NULL) {
		fprintf(stderr, "rxecho: cannot open config file\n");
		return NULL;
	}

	while (fgets(line, 80, fp) != NULL) {
		cp = strtok(line, " \t\r\n");

		if (cp == NULL || cp[0] == '#')
			continue;

		if ((p = calloc(1, sizeof(struct config))) == NULL) {
			perror("rxecho: malloc");
			return NULL;
		}

		if ((dev = ax25_config_get_dev(cp)) == NULL) {
			fprintf(stderr, "rxecho: invalid port name - %s\n", cp);
			return NULL;
		}

		strcpy(p->from, dev);
		p->from_idx = -1;

		if ((cp = strtok(NULL, " \t\r\n")) == NULL) {
			fprintf(stderr, "rxecho: config file error.\n");
			return NULL;
		}

		if ((dev = ax25_config_get_dev(cp)) == NULL) {
			fprintf(stderr, "rxecho: invalid port name - %s\n", cp);
			return NULL;
		}

		strcpy(p->to, dev);
		p->to_idx = -1;

		if (read_calls(p, strtok(NULL, " \t\r\n")) == -1) {
			fprintf(stderr, "rxecho: config file error.\n");
			return NULL;
		}

		p->next = list;
		list = p;
	}

	fclose(fp);

	if (list == NULL)
		fprintf(stderr, "rxecho: Empty config file!\n");

	return list;
}

/*
 *	Slightly modified from linux/include/net/ax25.h and 
 *	linux/net/ax25/ax25_subr.c:
 */

#if 0
#define C_COMMAND	1
#define C_RESPONSE	2
#define LAPB_C		0x80
#endif
#define LAPB_E		0x01
#define AX25_ADDR_LEN	7
#define AX25_REPEATED	0x80

typedef struct {
	ax25_address calls[AX25_MAX_DIGIS];
	unsigned char repeated[AX25_MAX_DIGIS];
	char ndigi;
	char lastrepeat;
} ax25_digi;

/*
 *	Given an AX.25 address pull of to, from, digi list, and the start of data.
 */
static unsigned char *ax25_parse_addr(unsigned char *buf, int len, ax25_address *src, ax25_address *dest, ax25_digi *digi)
{
	int d = 0;
	
	if (len < 14) return NULL;
		
#if 0
	if (flags != NULL) {
		*flags = 0;
	
		if (buf[6] & LAPB_C) {
			*flags = C_COMMAND;
		}

		if (buf[13] & LAPB_C) {
			*flags = C_RESPONSE;
		}
	}
		
	if (dama != NULL) 
		*dama = ~buf[13] & DAMA_FLAG;
#endif
		
	/* Copy to, from */
	if (dest != NULL) 
		memcpy(dest, buf + 0, AX25_ADDR_LEN);

	if (src != NULL)  
		memcpy(src,  buf + 7, AX25_ADDR_LEN);

	buf += 2 * AX25_ADDR_LEN;
	len -= 2 * AX25_ADDR_LEN;

	digi->lastrepeat = -1;
	digi->ndigi      = 0;
	
	while (!(buf[-1] & LAPB_E)) {
		if (d >= AX25_MAX_DIGIS)  return NULL;	/* Max of 6 digis */
		if (len < 7) return NULL;		/* Short packet */

		if (digi != NULL) {
			memcpy(&digi->calls[d], buf, AX25_ADDR_LEN);
			digi->ndigi = d + 1;

			if (buf[6] & AX25_REPEATED) {
				digi->repeated[d] = 1;
				digi->lastrepeat  = d;
			} else {
				digi->repeated[d] = 0;
			}
		}

		buf += AX25_ADDR_LEN;
		len -= AX25_ADDR_LEN;
		d++;
	}

	return buf;
}

/*
 * Check if frame should be echoed. Return 0 if it should and -1 if not.
 */
static int check_calls(struct config *cfg, unsigned char *buf, int len)
{
	ax25_address dest;
	ax25_digi digi;
	ax25_address *axp;
	int i;

	if ((buf[0] & 0x0F) != 0)
		return -1;			/* don't echo non-data	*/

	if (cfg->ncalls == 0)
		return 0;			/* copy everything	*/

	if (ax25_parse_addr(++buf, --len, NULL, &dest, &digi) == NULL)
		return -1;			/* invalid ax.25 header	*/

	/*
	 * If there are no digis or all digis are already repeated
	 * use destination address. Else use first non-repeated digi.
	 */
	if (digi.ndigi == 0 || digi.ndigi == digi.lastrepeat + 1)
		axp = &dest;
	else
		axp = &digi.calls[digi.lastrepeat + 1];

	for (i = 0; i < cfg->ncalls; i++)
		if (ax25_cmp(&cfg->calls[i], axp) == 0)
			return 0;

	return -1;
}

int main(int argc, char **argv)
{
#ifdef	USE_SOCKADDR_SLL
	struct sockaddr_ll sll;
	struct sockaddr *psa = (struct sockaddr *)&sll;
	const socklen_t sa_len = sizeof(struct sockaddr_ll);
	int from_idx;
#else
#ifdef	USE_SOCKADDR_SPKT
	struct sockaddr_pkt spkt;
	struct sockaddr *psa = (struct sockaddr *)&spkt;
	const int sa_len = sizeof(struct sockaddr_pkt);
#else
	struct sockaddr sa_generic;
	struct sockaddr *psa = (struct sockaddr *)&sa_generic;
	const int sa_len = sizeof(struct sockaddr);
#endif
	char from_dev_name[sizeof(psa->sa_data)];
#endif
	int s, size;
	socklen_t alen;
	unsigned char buf[1500];
	struct config *p, *list;

	while ((s = getopt(argc, argv, "lv")) != -1) {
		switch (s) {
			case 'l':
				logging = TRUE;
				break;
			case 'v':
				printf("rxecho: %s\n", VERSION);
				return 0;
			default:
				fprintf(stderr, "usage: rxecho [-l] [-v]\n");
				return 1;
		}
	}

	signal(SIGTERM, terminate);

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "rxecho: no AX.25 port data configured\n");
		return 1;
	}

	if ((list = readconfig()) == NULL)
		return 1;

#ifdef	USE_SOCKADDR_SLL
	if ((s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_AX25))) == -1) {
#else
	if ((s = socket(AF_INET, SOCK_PACKET, htons(ETH_P_AX25))) == -1) {
#endif
		perror("rxecho: socket:");
		return 1;
	}

#ifdef	USE_SOCKADDR_SLL
	for (p = list; p != NULL; p = p->next) {
		int i;

		for (i = 0; i < 2; i++) {
			struct config *q;
			struct ifreq ifr;
			char *p_name = (i ? p->to : p->from);
			int *p_idx = (i ? &p->to_idx : &p->from_idx);

			/* already set?  */
			if (*p_idx >= 0)
				continue;
			strncpy(ifr.ifr_name, p_name, sizeof(ifr.ifr_name)-1);
			ifr.ifr_name[sizeof(ifr.ifr_name)-1] = 0;
			if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
				perror("SIOCGIFINDEX");
				return 1;
			}
			*p_idx = ifr.ifr_ifindex;
			for (q = p->next; q != NULL; q = q->next) {
				if (q->from_idx < 0 && !strcmp(q->from, p_name))
					q->from_idx = *p_idx;
				if (q->to_idx < 0 && !strcmp(q->to, p_name))
					q->to_idx = *p_idx;
			}
		}
	}
#endif

	if (!daemon_start(FALSE)) {
		fprintf(stderr, "rxecho: cannot become a daemon\n");
		close(s);
		return 1;
	}

	if (logging) {
		openlog("rxecho", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "starting");
	}

	for (;;) {
 		alen = sa_len;

		if ((size = recvfrom(s, buf, 1500, 0, psa, &alen)) == -1) {
			if (logging) {
				syslog(LOG_ERR, "recvfrom: %m");
				closelog();
			}
			return 1;
		}

#ifdef	USE_SOCKADDR_SLL
		from_idx = sll.sll_ifindex;
#else
		/*
		 * dl9sau: save the names of the iface the frame came from;
		 * we'll overwrite psa->sa_data it for sendto() and need the
		 * name again when multiplexing to more than one iface
		 */
		strncpy(from_dev_name, psa->sa_data, sizeof(from_dev_name)-1);
		from_dev_name[sizeof(from_dev_name)-1] = 0;
#endif

		for (p = list; p != NULL; p = p->next)
#ifdef	USE_SOCKADDR_SLL
 			if (p->from_idx == from_idx && (check_calls(p, buf, size) == 0)) {
 				sll.sll_ifindex = p->to_idx;
#else
 			if ((strcmp(p->from, from_dev_name) == 0) && (check_calls(p, buf, size) == 0)) {
 				strcpy(psa->sa_data, p->to);
#endif
				/*
				 * cave: alen (set by recvfrom()) may > salen
				 *   which means it may point beyound of the
				 *   end of the struct. thats why we use the
				 *   correct len (sa_len). this fixes a bug
				 *   where skpt->protocol on sockaddr structs
				 *   pointed behind the struct, leading to
				 *   funny protocol IDs.
				 *   btw, the linux kernel internaly always
				 *   maps to sockaddr to sockaddr_pkt
				 *   on sockets of type SOCK_PACKET on family
				 *   AF_INET. sockaddr_pkt is len 18, sockaddr
				 *   is 16.
				 */
				if (sendto(s, buf, size, 0, psa, sa_len) == -1) {
					if (logging) {
						syslog(LOG_ERR, "sendto: %m");
						closelog();
					}
				}
			}
	}
}
