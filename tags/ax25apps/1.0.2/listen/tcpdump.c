/* TCP header tracing routines
 * Copyright 1991 Phil Karn, KA9Q
 */
#include <stdio.h>
#include <string.h>
#include "listen.h"

#define	FIN	0x01
#define	SYN	0x02
#define	RST	0x04
#define	PSH	0x08
#define	ACK	0x10
#define	URG	0x20
#define	CE	0x40

/* TCP options */
#define EOL_KIND	0
#define NOOP_KIND	1
#define MSS_KIND	2
#define MSS_LENGTH	4

#define	TCPLEN	20

#define max(a,b)  ((a) > (b) ? (a) : (b))

/* Dump a TCP segment header. Assumed to be in network byte order */
void tcp_dump(unsigned char *data, int length, int hexdump)
{
	int source, dest;
	int seq;
	int ack;
	int flags;
	int wnd;
	int up;
	int hdrlen;
	int mss = 0;

	source = get16(data + 0);
	dest = get16(data + 2);
	seq = get32(data + 4);
	ack = get32(data + 8);
	hdrlen = (data[12] & 0xF0) >> 2;
	flags = data[13];
	wnd = get16(data + 14);
	up = get16(data + 18);

	lprintf(T_PROTOCOL, "TCP:");
	lprintf(T_TCPHDR, " %s->", servname(source, "tcp"));
	lprintf(T_TCPHDR, "%s Seq x%x", servname(dest, "tcp"), seq);

	if (flags & ACK)
		lprintf(T_TCPHDR, " Ack x%x", ack);

	if (flags & CE)
		lprintf(T_TCPHDR, " CE");

	if (flags & URG)
		lprintf(T_TCPHDR, " URG");

	if (flags & ACK)
		lprintf(T_TCPHDR, " ACK");

	if (flags & PSH)
		lprintf(T_TCPHDR, " PSH");

	if (flags & RST)
		lprintf(T_TCPHDR, " RST");

	if (flags & SYN)
		lprintf(T_TCPHDR, " SYN");

	if (flags & FIN)
		lprintf(T_TCPHDR, " FIN");

	lprintf(T_TCPHDR, " Wnd %d", wnd);

	if (flags & URG)
		lprintf(T_TCPHDR, " UP x%x", up);

	/* Process options, if any */
	if (hdrlen > TCPLEN && length >= hdrlen) {
		unsigned char *cp = data + TCPLEN;
		int i = hdrlen - TCPLEN;
		int kind, optlen;

		while (i > 0) {
			kind = *cp++;

			/* Process single-byte options */
			switch (kind) {
			case EOL_KIND:
				i--;
				cp++;
				break;
			case NOOP_KIND:
				i--;
				cp++;
				continue;
			}

			/* All other options have a length field */
			optlen = *cp++;

			/* Process valid multi-byte options */
			switch (kind) {
			case MSS_KIND:
				if (optlen == MSS_LENGTH)
					mss = get16(cp);
				break;
			}

			optlen = max(2, optlen);	/* Enforce legal minimum */
			i -= optlen;
			cp += optlen - 2;
		}
	}

	if (mss != 0)
		lprintf(T_TCPHDR, " MSS %d", mss);

	length -= hdrlen;
	data += hdrlen;

	if (length > 0) {
		lprintf(T_TCPHDR, " Data %d\n", length);
		data_dump(data, length, hexdump);
		return;
	}

	lprintf(T_TCPHDR, "\n");
}
