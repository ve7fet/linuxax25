#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "listen.h"

/* dotted quad from 4 bytes of the packet (they need not be aligned) */
static char *ipaddr(unsigned char *p)
{
	static char buf[16];

	snprintf(buf, sizeof(buf), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
	return buf;
}

void rspf_dump(unsigned char *data, int length)
{
	int bptr, nodes, links, adjs;

	if (length < 2) {
		lprintf(T_ERROR, "RSPF: bad packet\n");
		return;
	}

	lprintf(T_IPHDR, "RSPF: version %u ", data[0]);

	switch (data[1]) {
	case 3:		/* RRH */
		if (length < 11) {
			lprintf(T_ERROR, "type RRH: bad packet\n");
			break;
		}
		lprintf(T_IPHDR, "type RRH seq %#04x flags %d\n",
			get16(data + 8), data[10]);
		bptr = 11;
		while (bptr < length)
			lprintf(T_IPHDR, "%c", data[bptr++]);
		lprintf(T_IPHDR, "\n");
		break;
	case 1:		/*Routing update */
		lprintf(T_IPHDR, "type ROUTING UPDATE ");
		if (length < 10) {
			lprintf(T_ERROR, "bad packet\n");
			break;
		}
		lprintf(T_IPHDR,
			"fragment %u frag total %u sync %u #nodes %u env_id %u\n",
			data[2], data[3], data[6], data[7], get16(data + 8));

		bptr = data[6] + 6;
		nodes = data[7];
		while (nodes-- && (length - bptr) > 7) {
			lprintf(T_DATA,
				"     Reporting Router: %s Seq %u Subseq %u #links %u\n",
				ipaddr(&data[bptr]), get16(&data[bptr + 4]),
				data[bptr + 6], data[bptr + 7]);
			links = data[bptr + 7];
			bptr += 8;
			while (links-- && (length - bptr) > 4) {
				lprintf(T_DATA,
					"          horizon %u ERP factor %u cost %u #adjacencies %u\n",
					data[bptr], data[bptr + 1],
					data[bptr + 2], data[bptr + 3]);
				adjs = data[bptr + 3];
				bptr += 4;
				while (adjs-- && (length - bptr) > 4) {
					lprintf(T_DATA,
						"               %s/%d \n",
						ipaddr(&data[bptr + 1]),
						data[bptr] & 0x3f);
					bptr += 5;
				}
			}
		}
		break;
	default:
		lprintf(T_ERROR, "Unknown packet type %d\n", data[1]);
		break;
	}
}
