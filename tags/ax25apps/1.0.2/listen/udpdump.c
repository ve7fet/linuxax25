/* UDP packet tracing
 * Copyright 1991 Phil Karn, KA9Q
 */
#include <stdio.h>
#include "listen.h"

#define	RIP_PORT	520

#define	UDPHDR		8

/* Dump a UDP header */
void udp_dump(unsigned char *data, int length, int hexdump)
{
	int hdr_length;
	int source;
	int dest;

	hdr_length = get16(data + 4);
	source = get16(data + 0);
	dest = get16(data + 2);

	lprintf(T_PROTOCOL, "UDP:");

	lprintf(T_TCPHDR, " len %d %s->", hdr_length,
		servname(source, "udp"));
	lprintf(T_TCPHDR, "%s", servname(dest, "udp"));

	if (hdr_length > UDPHDR) {
		length -= UDPHDR;
		data += UDPHDR;

		switch (dest) {
		case RIP_PORT:
			lprintf(T_TCPHDR, "\n");
			rip_dump(data, length);
			break;
		default:
			lprintf(T_TCPHDR, " Data %d\n", length);
			data_dump(data, length, hexdump);
			break;
		}
	}
}
