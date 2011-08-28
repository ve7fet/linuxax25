/* RIP packet tracing
 * Copyright 1991 Phil Karn, KA9Q
 *
 *  Changes Copyright (c) 1993 Jeff White - N0POY, All Rights Reserved.
 *  Permission granted for non-commercial copying and use, provided
 *  this notice is retained.
 *
 * Rehack for RIP-2 (RFC1388) by N0POY 4/1993
 *
 * Beta release 11/10/93 V0.91
 *
 * 2/19/94 release V1.0
 *
 */

#include <stdio.h>

#include "listen.h"

#define  RIP_VERSION_2           2
#define  RIP_VERSION_98		 98

#define  RIP_HEADER              4
#define  RIP_ENTRY               20

#define  RIP_AF_INET             2	/* IP Family */

#define  RIPCMD_REQUEST          1	/* want info */
#define  RIPCMD_RESPONSE         2	/* responding to request */

#define  RIP98_ENTRY             6

static struct mask_struct {
	unsigned int mask;
	unsigned int width;
} mask_table[] = {
	{
	0xFFFFFFFF, 32}, {
	0xFFFFFFFE, 31}, {
	0xFFFFFFFC, 30}, {
	0xFFFFFFF8, 29}, {
	0xFFFFFFF0, 28}, {
	0xFFFFFFE0, 27}, {
	0xFFFFFFC0, 26}, {
	0xFFFFFF80, 25}, {
	0xFFFFFF00, 24}, {
	0xFFFFFE00, 23}, {
	0xFFFFFC00, 22}, {
	0xFFFFF800, 21}, {
	0xFFFFF000, 20}, {
	0xFFFFE000, 19}, {
	0xFFFFC000, 18}, {
	0xFFFF8000, 17}, {
	0xFFFF0000, 16}, {
	0xFFFE0000, 15}, {
	0xFFFC0000, 14}, {
	0xFFF80000, 13}, {
	0xFFF00000, 12}, {
	0xFFE00000, 11}, {
	0xFFC00000, 10}, {
	0xFF800000, 9}, {
	0xFF000000, 8}, {
	0xFE000000, 7}, {
	0xFC000000, 6}, {
	0xF8000000, 5}, {
	0xF0000000, 4}, {
	0xE0000000, 3}, {
	0xC0000000, 2}, {
	0x80000000, 1}, {
0x00000000, 0},};

static unsigned int mask2width(unsigned int mask)
{
	struct mask_struct *t;

	for (t = mask_table; t->mask != 0; t++)
		if (mask == t->mask)
			return t->width;

	return 0;
}

void rip_dump(unsigned char *data, int length)
{
	int i;
	int cmd;
	int version;
	int domain;
	char ipaddmask[25];

	lprintf(T_PROTOCOL, "RIP: ");

	cmd = data[0];
	version = data[1];
	domain = get16(data + 2);

	length -= RIP_HEADER;
	data += RIP_HEADER;

	switch (cmd) {
	case RIPCMD_REQUEST:
		lprintf(T_IPHDR, "REQUEST");
		break;

	case RIPCMD_RESPONSE:
		lprintf(T_IPHDR, "RESPONSE");
		break;

	default:
		lprintf(T_IPHDR, " cmd %u", cmd);
		break;
	}

	switch (version) {
	case RIP_VERSION_98:	/* IPGATE * */
		lprintf(T_IPHDR, " vers %u entries %u\n", version,
			length / RIP98_ENTRY);

		i = 0;
		while (length >= RIP98_ENTRY) {
			sprintf(ipaddmask, "%u.%u.%u.%u/%-2u", data[0],
				data[1], data[2], data[3], data[4]);
			lprintf(T_ADDR, "%-16s %-3u ", ipaddmask, data[5]);

			if ((++i % 3) == 0)
				lprintf(T_IPHDR, "\n");

			length -= RIP98_ENTRY;
			data += RIP98_ENTRY;
		}

		if ((i % 3) != 0)
			lprintf(T_IPHDR, "\n");
		break;

	default:
		lprintf(T_IPHDR, " vers %u entries %u domain %u:\n",
			version, length / RIP_ENTRY, domain);

		i = 0;
		while (length >= RIP_ENTRY) {
			if (get16(data + 0) != RIP_AF_INET) {
				/* Skip non-IP addresses */
				length -= RIP_ENTRY;
				data += RIP_ENTRY;
				continue;
			}

			if (version >= RIP_VERSION_2) {
				sprintf(ipaddmask, "%u.%u.%u.%u/%-4d",
					data[4], data[5], data[6], data[7],
					mask2width(get32(data + 8)));
			} else {
				sprintf(ipaddmask, "%u.%u.%u.%u/??",
					data[4], data[5], data[6],
					data[7]);
			}

			lprintf(T_ADDR, "%-20s%-3u", ipaddmask,
				get32(data + 16));

			if ((++i % 3) == 0)
				lprintf(T_IPHDR, "\n");

			length -= RIP_ENTRY;
			data += RIP_ENTRY;
		}

		if ((i % 3) != 0)
			lprintf(T_IPHDR, "\n");
		break;
	}
}
