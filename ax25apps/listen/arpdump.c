/* ARP packet tracing routines
 * Copyright 1991 Phil Karn, KA9Q
 */
#include <stdio.h>
#include "listen.h"

#define	ARP_REQUEST	1
#define	ARP_REPLY	2
#define	REVARP_REQUEST	3
#define	REVARP_REPLY	4

#define	ARP_AX25	3

#define	AXALEN		7

#define	PID_IP		0xCC

void arp_dump(unsigned char *data, int length)
{
	int is_ip = 0;
	int hardware;
	int protocol;
	int hwlen;
	int pralen;
	int operation;
	unsigned char *shwaddr, *sprotaddr;
	unsigned char *thwaddr, *tprotaddr;
	char tmp[25];

	lprintf(T_PROTOCOL, "ARP: ");
	lprintf(T_IPHDR, "len %d", length);
	if (length < 16) {
		lprintf(T_ERROR, " bad packet\n");
		return;
	}

	hardware = get16(data + 0);
	protocol = get16(data + 2);
	hwlen = data[4];
	pralen = data[5];
	operation = get16(data + 6);

	if (hardware != ARP_AX25) {
		lprintf(T_IPHDR, " non-AX25 ARP packet\n");
		return;
	}

	lprintf(T_IPHDR, " hwtype AX25");

	/* Print hardware length only if it doesn't match
	 * the length in the known types table
	 */
	if (hwlen != AXALEN)
		lprintf(T_IPHDR, " hwlen %d", hwlen);

	if (protocol == PID_IP) {
		lprintf(T_IPHDR, " prot IP");
		is_ip = 1;
	} else {
		lprintf(T_IPHDR, " prot 0x%x prlen %d", protocol, pralen);
	}

	switch (operation) {
	case ARP_REQUEST:
		lprintf(T_IPHDR, " op REQUEST");
		break;
	case ARP_REPLY:
		lprintf(T_IPHDR, " op REPLY");
		break;
	case REVARP_REQUEST:
		lprintf(T_IPHDR, " op REVERSE REQUEST");
		break;
	case REVARP_REPLY:
		lprintf(T_IPHDR, " op REVERSE REPLY");
		break;
	default:
		lprintf(T_IPHDR, " op %d", operation);
		break;
	}

	shwaddr = data + 8;
	sprotaddr = shwaddr + hwlen;

	thwaddr = sprotaddr + pralen;
	tprotaddr = thwaddr + hwlen;

	lprintf(T_IPHDR, "\nsender");
	if (is_ip)
		lprintf(T_ADDR, " IPaddr %d.%d.%d.%d",
			sprotaddr[0], sprotaddr[1],
			sprotaddr[2], sprotaddr[3]);
	lprintf(T_IPHDR, " hwaddr %s\n", pax25(tmp, shwaddr));

	lprintf(T_IPHDR, "target");
	if (is_ip)
		lprintf(T_ADDR, " IPaddr %d.%d.%d.%d",
			tprotaddr[0], tprotaddr[1],
			tprotaddr[2], tprotaddr[3]);
	if (*thwaddr != 0)
		lprintf(T_ADDR, " hwaddr %s", pax25(tmp, thwaddr));
	lprintf(T_IPHDR, "\n");
}
