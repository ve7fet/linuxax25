/* IP header tracing routines
 * Copyright 1991 Phil Karn, KA9Q
 */
#include <stdio.h>
#include "listen.h"

#define	IPLEN		20

#define	MF		0x2000
#define	DF		0x4000
#define	CE		0x8000

#define	IPIPNEW_PTCL	4
#define	IPIPOLD_PTCL	94
#define	TCP_PTCL	6
#define	UDP_PTCL	17
#define	ICMP_PTCL	1
#define	AX25_PTCL	93
#define RSPF_PTCL	73

void ip_dump(unsigned char *data, int length, int hexdump)
{
	int hdr_length;
	int tos;
	int ip_length;
	int id;
	int fl_offs;
	int flags;
	int offset;
	int ttl;
	int protocol;
	unsigned char *source, *dest;

	lprintf(T_PROTOCOL, "IP:");

	/* Sneak peek at IP header and find length */
	hdr_length = (data[0] & 0xf) << 2;

	if (hdr_length < IPLEN) {
		lprintf(T_ERROR, " bad header\n");
		return;
	}

	tos = data[1];
	ip_length = get16(data + 2);
	id = get16(data + 4);
	fl_offs = get16(data + 6);
	flags = fl_offs & 0xE000;
	offset = (fl_offs & 0x1FFF) << 3;
	ttl = data[8];
	protocol = data[9];
	source = data + 12;
	dest = data + 16;

	lprintf(T_IPHDR, " len %d", ip_length);

	lprintf(T_ADDR, " %d.%d.%d.%d->%d.%d.%d.%d",
		source[0], source[1], source[2], source[3],
		dest[0], dest[1], dest[2], dest[3]);

	lprintf(T_IPHDR, " ihl %d ttl %d", hdr_length, ttl);

	if (tos != 0)
		lprintf(T_IPHDR, " tos %d", tos);

	if (offset != 0 || (flags & MF))
		lprintf(T_IPHDR, " id %d offs %d", id, offset);

	if (flags & DF)
		lprintf(T_IPHDR, " DF");
	if (flags & MF)
		lprintf(T_IPHDR, " MF");
	if (flags & CE)
		lprintf(T_IPHDR, " CE");

	data += hdr_length;
	length -= hdr_length;

	if (offset != 0) {
		lprintf(T_IPHDR, "\n");
		if (length > 0)
			data_dump(data, length, hexdump);
		return;
	}

	switch (protocol) {
	case IPIPOLD_PTCL:
	case IPIPNEW_PTCL:
		lprintf(T_IPHDR, " prot IP\n");
		ip_dump(data, length, hexdump);
		break;
	case TCP_PTCL:
		lprintf(T_IPHDR, " prot TCP\n");
		tcp_dump(data, length, hexdump);
		break;
	case UDP_PTCL:
		lprintf(T_IPHDR, " prot UDP\n");
		udp_dump(data, length, hexdump);
		break;
	case ICMP_PTCL:
		lprintf(T_IPHDR, " prot ICMP\n");
		icmp_dump(data, length, hexdump);
		break;
	case AX25_PTCL:
		lprintf(T_IPHDR, " prot AX25\n");
		ax25_dump(data, length, hexdump);
		break;
	case RSPF_PTCL:
		lprintf(T_IPHDR, " prot RSPF\n");
		rspf_dump(data, length);
		break;
	default:
		lprintf(T_IPHDR, " prot %d\n", protocol);
		data_dump(data, length, hexdump);
		break;
	}
}
