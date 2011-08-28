#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "listen.h"

void rspf_dump(unsigned char *data, int length)
{
	int bptr, nodes, links, adjs;

	lprintf(T_IPHDR, "RSPF: version %u ", data[0]);

	switch (data[1]) {
	case 3:		/* RRH */
		lprintf(T_IPHDR, "type RRH seq %#04x flags %d\n",
			ntohs(*((u_short *) (&data[8]))), data[10]);
		bptr = 11;
		while (bptr < length)
			lprintf(T_IPHDR, "%c", data[bptr++]);
		lprintf(T_IPHDR, "\n");
		break;
	case 1:		/*Routing update */
		lprintf(T_IPHDR, "type ROUTING UPDATE ");
		lprintf(T_IPHDR,
			"fragment %u frag total %u sync %u #nodes %u env_id %u\n",
			data[2], data[3], data[6], data[7],
			ntohs(*((u_short *) (&data[8]))));

		bptr = data[6] + 6;
		nodes = data[7];
		while (nodes-- && (length - bptr) > 7) {
			lprintf(T_DATA,
				"     Reporting Router: %s Seq %u Subseq %u #links %u\n",
				inet_ntoa(*
					  ((struct in_addr
					    *) (&data[bptr]))),
				ntohs(*((u_short *) (&data[bptr + 4]))),
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
						inet_ntoa(*
							  ((struct in_addr
							    *) (&data[bptr
								      +
								      1]))),
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
