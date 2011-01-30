/* ICMP header tracing
 * Copyright 1991 Phil Karn, KA9Q
 */
#include <stdio.h>
#include "listen.h"

#define	ICMP_DEST_UNREACH	3
#define	ICMP_REDIRECT		5
#define	ICMP_TIME_EXCEED	11
#define	ICMP_PARAM_PROB		12
#define	ICMP_ECHO		8
#define	ICMP_ECHO_REPLY		0
#define	ICMP_INFO_RQST		15
#define	ICMP_INFO_REPLY		16
#define	ICMP_TIMESTAMP		13
#define	ICMP_TIME_REPLY		14
#define	ICMP_QUENCH		4

#define	ICMPLEN			8

/* Dump an ICMP header */
void icmp_dump(unsigned char *data, int length, int hexdump)
{
	int type;
	int code;
	int pointer;
	unsigned char *address;
	int id, seq;

	type = data[0];
	code = data[1];
	pointer = data[4];
	address = data + 4;
	id = get16(data + 4);
	seq = get16(data + 6);

	data += ICMPLEN;
	length -= ICMPLEN;

	lprintf(T_IPHDR, "ICMP: type ");

	switch (type) {
	case ICMP_DEST_UNREACH:
		lprintf(T_ERROR, "Unreachable ");
		lprintf(T_IPHDR, "code ");

		switch (code) {
		case 0:
			lprintf(T_ERROR, "Network");
			break;
		case 1:
			lprintf(T_ERROR, "Host");
			break;
		case 2:
			lprintf(T_ERROR, "Protocol");
			break;
		case 3:
			lprintf(T_ERROR, "Port");
			break;
		case 4:
			lprintf(T_ERROR, "Fragmentation");
			break;
		case 5:
			lprintf(T_ERROR, "Source route");
			break;
		case 6:
			lprintf(T_ERROR, "Dest net unknown");
			break;
		case 7:
			lprintf(T_ERROR, "Dest host unknown");
			break;
		case 8:
			lprintf(T_ERROR, "Source host isolated");
			break;
		case 9:
			lprintf(T_ERROR, "Net prohibited");
			break;
		case 10:
			lprintf(T_ERROR, "Host prohibited");
			break;
		case 11:
			lprintf(T_ERROR, "Net TOS");
			break;
		case 12:
			lprintf(T_ERROR, "Host TOS");
			break;
		case 13:
			lprintf(T_ERROR, "Administratively Prohibited");
			break;
		default:
			lprintf(T_ERROR, "%d", code);
			break;
		}
		lprintf(T_IPHDR, "\nReturned ");
		ip_dump(data, length, hexdump);
		break;

	case ICMP_REDIRECT:
		lprintf(T_ERROR, "Redirect ");
		lprintf(T_IPHDR, "code ");

		switch (code) {
		case 0:
			lprintf(T_ERROR, "Network");
			break;
		case 1:
			lprintf(T_ERROR, "Host");
			break;
		case 2:
			lprintf(T_ERROR, "TOS & Network");
			break;
		case 3:
			lprintf(T_ERROR, "TOS & Host");
			break;
		default:
			lprintf(T_ERROR, "%d", code);
			break;
		}
		lprintf(T_IPHDR, " new gateway %d.%d.%d.%d",
			address[0], address[1], address[2], address[3]);
		lprintf(T_IPHDR, "\nReturned ");
		ip_dump(data, length, hexdump);
		break;

	case ICMP_TIME_EXCEED:
		lprintf(T_ERROR, "Time Exceeded code ");
		lprintf(T_ERROR, "Time Exceeded ");
		lprintf(T_IPHDR, "code ");

		switch (code) {
		case 0:
			lprintf(T_ERROR, "Time-to-live");
			break;
		case 1:
			lprintf(T_ERROR, "Fragment reassembly");
			break;
		default:
			lprintf(T_ERROR, "%d", code);
			break;
		}
		lprintf(T_IPHDR, "\nReturned ");
		ip_dump(data, length, hexdump);
		break;

	case ICMP_PARAM_PROB:
		lprintf(T_ERROR, "Parameter Problem pointer %d", pointer);
		lprintf(T_IPHDR, "\nReturned ");
		ip_dump(data, length, hexdump);
		break;

	case ICMP_QUENCH:
		lprintf(T_ERROR, "Source Quench");
		lprintf(T_IPHDR, "\nReturned ");
		ip_dump(data, length, hexdump);
		break;

	case ICMP_ECHO:
		lprintf(T_IPHDR, "Echo Request id %d seq %d\n", id, seq);
		data_dump(data, length, hexdump);
		break;

	case ICMP_ECHO_REPLY:
		lprintf(T_IPHDR, "Echo Reply id %d seq %d\n", id, seq);
		data_dump(data, length, hexdump);
		break;

	case ICMP_INFO_RQST:
		lprintf(T_IPHDR, "Information Request id %d seq %d\n", id,
			seq);
		data_dump(data, length, hexdump);
		break;

	case ICMP_INFO_REPLY:
		lprintf(T_IPHDR, "Information Reply id %d seq %d\n", id,
			seq);
		data_dump(data, length, hexdump);
		break;

	case ICMP_TIMESTAMP:
		lprintf(T_IPHDR, "Timestamp Request id %d seq %d\n", id,
			seq);
		data_dump(data, length, hexdump);
		break;

	case ICMP_TIME_REPLY:
		lprintf(T_IPHDR, "Timestamp Reply id %d seq %d\n", id,
			seq);
		data_dump(data, length, hexdump);
		break;

	default:
		lprintf(T_IPHDR, "%d\n", type);
		data_dump(data, length, hexdump);
		break;
	}
}
