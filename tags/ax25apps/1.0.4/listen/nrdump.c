/* NET/ROM header tracing routines
 * Copyright 1991 Phil Karn, KA9Q
 *
 * Added decoding of INP route information frames, Jeroen (PE1RXQ)
 */

#include <stdio.h>
#include <string.h>
#include "listen.h"

#define	AXLEN		7
#define	ALEN		6

#define	NRPROTO_IP	0x0C

#define	NR4OPCODE	0x0F
#define	NR4OPPID	0
#define	NR4OPCONRQ	1
#define	NR4OPCONAK	2
#define	NR4OPDISRQ	3
#define	NR4OPDISAK	4
#define	NR4OPINFO	5
#define	NR4OPACK	6
#define	NR4MORE		0x20
#define	NR4NAK		0x40
#define	NR4CHOKE	0x80

#define	NRDESTPERPACK	11
#define	NR3NODESIG	0xFF
#define	NR3POLLSIG	0xFE

#define UI		0x03

static void netrom_flags(int);

/* Display INP route information frames */
static void netrom_inp_dump(unsigned char *data, int length)
{
	char node[10];
	char alias[7];
	int hops;
	int tt;
	int alen;
	int i;
	
	if (data[0]==0xff) {
		lprintf(T_AXHDR, "INP Route Information Frame:\n");
		i=1;
		while (i<length-10) {
			pax25(node, data+i);
			i+=7;
			hops=data[i++];
			tt=data[i++]*256;
			tt+=data[i++];
			alias[0]=0;
			while (i<length-data[i] && data[i]) {
				if (data[i+1]==0x00) {
					alen=data[i]-2;
					alen=alen < 7 ? alen : 6;
					memcpy(alias, data+i+2, alen);
					alias[alen]=0;
				}
				i+=data[i];
			}
			i++;
			lprintf(T_DATA, "        %12s  %-6s   %6u %6u\n", node, alias, hops, tt);
		}
	}
}

/* Display NET/ROM network and transport headers */
void netrom_dump(unsigned char *data, int length, int hexdump, int type)
{
	char tmp[15];
	int i;

	/* See if it is a routing broadcast */
	if (data[0] == NR3NODESIG) {
		/* Filter out INP routing frames */
		if (type!=UI) {
			netrom_inp_dump(data, length);
			return;
		}
		memcpy(tmp, data + 1, ALEN);
		tmp[ALEN] = '\0';
		lprintf(T_AXHDR, "NET/ROM Routing: %s\n", tmp);

		data += (ALEN + 1);
		length -= (ALEN + 1);

		for (i = 0; i < NRDESTPERPACK; i++) {
			if (length < AXLEN)
				break;
			lprintf(T_DATA, "        %12s", pax25(tmp, data));

			memcpy(tmp, data + AXLEN, ALEN);
			tmp[ALEN] = '\0';
			lprintf(T_DATA, "%8s", tmp);

			lprintf(T_DATA, "    %12s",
				pax25(tmp, data + AXLEN + ALEN));
			lprintf(T_DATA, "    %3u\n",
				data[AXLEN + ALEN + AXLEN]);

			data += (AXLEN + ALEN + AXLEN + 1);
			length -= (AXLEN + ALEN + AXLEN + 1);
		}

		return;
	}

	/* See if it is a JNOS style node poll */
	if (data[0] == NR3POLLSIG) {
		memcpy(tmp, data + 1, ALEN);
		tmp[ALEN] = '\0';
		lprintf(T_AXHDR, "NET/ROM Poll: %s\n", tmp);
		return;
	}

	/* Decode network layer */
	lprintf(T_AXHDR, "NET/ROM: ");
	lprintf(T_ADDR, "%s->", pax25(tmp, data));
	lprintf(T_ADDR, "%s", pax25(tmp, data + AXLEN));
	lprintf(T_AXHDR, " ttl %d\n", data[AXLEN + AXLEN]);

	data += (AXLEN + AXLEN + 1);
	length -= (AXLEN + AXLEN + 1);

	switch (data[4] & NR4OPCODE) {
	case NR4OPPID:		/* network PID extension */
		if (data[0] == NRPROTO_IP && data[1] == NRPROTO_IP) {
			ip_dump(data + 5, length - 5, hexdump);
			return;
		} else {
			lprintf(T_AXHDR,
				"         protocol family %x, proto %x",
				data[0], data[1]);
		}
		break;

	case NR4OPCONRQ:	/* Connect request */
		lprintf(T_AXHDR, "         conn rqst: my ckt %02X/%02X",
			data[0], data[1]);
		lprintf(T_AXHDR, " wnd %d", data[5]);
		lprintf(T_ADDR, " %s", pax25(tmp, data + 6));
		lprintf(T_ADDR, "@%s", pax25(tmp, data + 6 + AXLEN));
		data += AXLEN + AXLEN + 6;
		length -= AXLEN + AXLEN + 6;
		if (length > 0)
			lprintf(T_AXHDR, " timeout %d",
				data[1] * 256 + data[0]);
		lprintf(T_AXHDR, "\n");
		break;

	case NR4OPCONAK:	/* Connect acknowledgement */
		lprintf(T_AXHDR,
			"         conn ack: ur ckt %02X/%02X my ckt %02X/%02X",
			data[0], data[1], data[2], data[3]);
		lprintf(T_AXHDR, " wnd %d", data[5]);
		netrom_flags(data[4]);
		break;

	case NR4OPDISRQ:	/* Disconnect request */
		lprintf(T_AXHDR, "         disc: ur ckt %02X/%02X\n",
			data[0], data[1]);
		break;

	case NR4OPDISAK:	/* Disconnect acknowledgement */
		lprintf(T_AXHDR, "         disc ack: ur ckt %02X/%02X\n",
			data[0], data[1]);
		break;

	case NR4OPINFO:	/* Information (data) */
		lprintf(T_AXHDR, "         info: ur ckt %02X/%02X",
			data[0], data[1]);
		lprintf(T_AXHDR, " txseq %d rxseq %d", data[2], data[3]);
		netrom_flags(data[4]);
		data_dump(data + 5, length - 5, hexdump);
		break;

	case NR4OPACK:		/* Information acknowledgement */
		lprintf(T_AXHDR, "         info ack: ur ckt %02X/%02X",
			data[0], data[1]);
		lprintf(T_AXHDR, " rxseq %d", data[3]);
		netrom_flags(data[4]);
		break;

	default:
		lprintf(T_AXHDR, "         unknown transport type %d\n",
			data[4] & 0x0f);
		break;
	}
}


static void netrom_flags(int flags)
{
	if (flags & NR4CHOKE)
		lprintf(T_AXHDR, " CHOKE");

	if (flags & NR4NAK)
		lprintf(T_AXHDR, " NAK");

	if (flags & NR4MORE)
		lprintf(T_AXHDR, " MORE");

	lprintf(T_AXHDR, "\n");
}
