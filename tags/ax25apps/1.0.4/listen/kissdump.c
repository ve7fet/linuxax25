/* Tracing routines for KISS TNC
 * Copyright 1991 Phil Karn, KA9Q
 */
#include <stdio.h>
#include "listen.h"

#define	PARAM_DATA	0
#define	PARAM_TXDELAY	1
#define	PARAM_PERSIST	2
#define	PARAM_SLOTTIME	3
#define	PARAM_TXTAIL	4
#define	PARAM_FULLDUP	5
#define	PARAM_HW	6
#define	PARAM_RETURN	15	/* Should be 255, but is ANDed with 0x0F */

void ki_dump(unsigned char *data, int length, int hexdump)
{
	int type;
	int val;

	type = data[0] & 0xf;

	if (type == PARAM_DATA) {
		ax25_dump(data + 1, length - 1, hexdump);
		return;
	}

	val = data[1];

	switch (type) {
	case PARAM_TXDELAY:
		lprintf(T_KISS, "TX Delay: %lu ms\n", val * 10L);
		break;
	case PARAM_PERSIST:
		lprintf(T_KISS, "Persistence: %u/256\n", val + 1);
		break;
	case PARAM_SLOTTIME:
		lprintf(T_KISS, "Slot time: %lu ms\n", val * 10L);
		break;
	case PARAM_TXTAIL:
		lprintf(T_KISS, "TX Tail time: %lu ms\n", val * 10L);
		break;
	case PARAM_FULLDUP:
		lprintf(T_KISS, "Duplex: %s\n",
			val == 0 ? "Half" : "Full");
		break;
	case PARAM_HW:
		lprintf(T_KISS, "Hardware %u\n", val);
		break;
	case PARAM_RETURN:
		lprintf(T_KISS, "RETURN\n");
		break;
	default:
		lprintf(T_KISS, "code %u arg %u\n", type, val);
		break;
	}
}
