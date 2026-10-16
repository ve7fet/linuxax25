/*
 * Copyright 1996 Jonathan Naylor G4KLX
 *
 * Added ROSE new facilities : Jean-Paul ROUBELAT 04-1998
 * Added ROSE multi-digi     : Jean-Paul ROUBELAT 05-1998
 * Added ROSE PID transport  : Jean-Paul ROUBELAT 07-1998
 */

#include <stdio.h>
#include <string.h>
#include "listen.h"

#define	PID_SEGMENT	0x08
#define	PID_ARP		0xCD
#define	PID_NETROM	0xCF
#define	PID_IP		0xCC
#define	PID_X25		0x01
#define	PID_TEXNET	0xC3
#define	PID_FLEXNET	0xCE
#define	PID_NO_L3	0xF0

#define	ROSE_ADDR_LEN			5

#define	CALL_REQUEST			0x0B
#define	CALL_ACCEPTED			0x0F
#define	CLEAR_REQUEST			0x13
#define	CLEAR_CONFIRMATION		0x17
#define	INTERRUPT			0x23
#define	INTERRUPT_CONFIRMATION		0x27
#define	RESET_REQUEST			0x1B
#define	RESET_CONFIRMATION		0x1F
#define	RESTART_REQUEST			0xFB
#define	RESTART_CONFIRMATION		0xFF
#define	REGISTRATION_REQUEST		0xF3
#define	REGISTRATION_CONFIRMATION	0xF7
#define	DIAGNOSTIC			0xF1
#define	RR				0x01
#define	RNR				0x05
#define	REJ				0x09
#define	DATA				0x00

#define	QBIT				0x80
#define	DBIT				0x40
#define	MBIT				0x10

#define	AX25_HBIT			0x80

static char *dump_x25_addr(unsigned char *);
static char *clear_code(unsigned char);
static char *reset_code(unsigned char);
static char *restart_code(unsigned char);
static void facility(unsigned char *, int len);

void rose_dump(unsigned char *data, int length, int hexdump)
{
	unsigned int len, hlen;
	unsigned int lci;
	unsigned char hdr[5];

	if (length < 3) {
		lprintf(T_ERROR, "X.25: bad packet!\n");
		return;
	}
	/* The cause and diagnostic bytes of a short packet read as 0
	 * instead of past the end of the frame. */
	memset(hdr, 0, sizeof(hdr));
	memcpy(hdr, data, length < 5 ? length : 5);

	lci = ((unsigned) (data[0] & 0x0F) << 8) + data[1];
	lprintf(T_ROSEHDR, "X.25: LCI %3.3X : ", lci);

	switch (data[2]) {
	case CALL_REQUEST:
		len = 4;
		hlen = (((hdr[3] >> 4) & 0x0F) + 1) / 2;
		hlen += (((hdr[3] >> 0) & 0x0F) + 1) / 2;
		len += hlen;
		if (length < (int) len) {
			lprintf(T_ERROR, "CALL REQUEST - bad length\n");
			return;
		}
		data += len;
		length -= len;
		lprintf(T_ROSEHDR, "CALL REQUEST - ");
		if (length > 0)
			facility(data, length);
		else
			lprintf(T_ROSEHDR, "\n");
		return;

	case CALL_ACCEPTED:
		lprintf(T_ROSEHDR, "CALL ACCEPTED\n");
		return;

	case CLEAR_REQUEST:
		lprintf(T_ROSEHDR, "CLEAR REQUEST - Cause %s - Diag %d\n",
			clear_code(hdr[3]), hdr[4]);
		return;

	case CLEAR_CONFIRMATION:
		lprintf(T_ROSEHDR, "CLEAR CONFIRMATION\n");
		return;

	case DIAGNOSTIC:
		lprintf(T_ROSEHDR, "DIAGNOSTIC - Diag %d\n", hdr[3]);
		return;

	case INTERRUPT:
		lprintf(T_ROSEHDR, "INTERRUPT\n");
		data_dump(data + 3, length - 3, hexdump);
		return;

	case INTERRUPT_CONFIRMATION:
		lprintf(T_ROSEHDR, "INTERRUPT CONFIRMATION\n");
		return;

	case RESET_REQUEST:
		lprintf(T_ROSEHDR, "RESET REQUEST - Cause %s - Diag %d\n",
			reset_code(hdr[3]), hdr[4]);
		return;

	case RESET_CONFIRMATION:
		lprintf(T_ROSEHDR, "RESET CONFIRMATION\n");
		return;

	case RESTART_REQUEST:
		lprintf(T_ROSEHDR,
			"RESTART REQUEST - Cause %s - Diag %d\n",
			restart_code(hdr[3]), hdr[4]);
		return;

	case RESTART_CONFIRMATION:
		lprintf(T_ROSEHDR, "RESTART CONFIRMATION\n");
		return;

	case REGISTRATION_REQUEST:
		lprintf(T_ROSEHDR, "REGISTRATION REQUEST\n");
		return;

	case REGISTRATION_CONFIRMATION:
		lprintf(T_ROSEHDR, "REGISTRATION CONFIRMATION\n");
		return;
	}

	if ((data[2] & 0x01) == DATA) {
		lprintf(T_ROSEHDR, "DATA R%d S%d %s%s%s",
			(data[2] >> 5) & 0x07, (data[2] >> 1) & 0x07,
			(data[0] & QBIT) ? "Q" : "",
			(data[0] & DBIT) ? "D" : "",
			(data[2] & MBIT) ? "M" : "");
		if ((length >= 5) && (data[0] & QBIT) && (data[3] == 0x7f)) {
			/* pid transport */
			int pid = data[4];
			data += 5;
			length -= 5;
			switch (pid) {
				case PID_SEGMENT:
					lprintf(T_ROSEHDR," len %d\n", length - 5);
					data_dump(data, length, hexdump);
					break;
				case PID_ARP:
					lprintf(T_ROSEHDR," pid=ARP len %d\n", length - 5);
					arp_dump(data, length);
					break;
				case PID_NETROM:
					lprintf(T_ROSEHDR," pid=NET/ROM len %d\n", length - 5);
					netrom_dump(data, length, hexdump, 0);
					break;
				case PID_IP:
					lprintf(T_ROSEHDR," pid=IP len %d\n", length - 5);
					ip_dump(data, length, hexdump);
					break;
				case PID_X25:
					lprintf(T_ROSEHDR, " pid=X.25 len %d\n", length - 5);
					rose_dump(data, length, hexdump);
					break;
				case PID_TEXNET:
					lprintf(T_ROSEHDR, " pid=TEXNET len %d\n", length - 5);
					data_dump(data, length, hexdump);
					break;
				case PID_FLEXNET:
					lprintf(T_ROSEHDR, " pid=FLEXNET len %d\n", length - 5);
					flexnet_dump(data, length, hexdump);
					break;
				case PID_NO_L3:
					lprintf(T_ROSEHDR, " pid=Text len %d\n", length - 5);
					data_dump(data, length, hexdump);
					break;
				default:
					lprintf(T_ROSEHDR, " pid=0x%x len %d\n", pid, length - 5);
					data_dump(data, length, hexdump);
					break;
			}
		} else {
			lprintf(T_ROSEHDR, " len %d\n", length - 3);
			data_dump(data + 3, length - 3, hexdump);
		}
		return;
	}

	switch (data[2] & 0x1F) {
	case RR:
		lprintf(T_ROSEHDR, "RR R%d\n", (data[2] >> 5) & 0x07);
		return;
	case RNR:
		lprintf(T_ROSEHDR, "RNR R%d\n", (data[2] >> 5) & 0x07);
		return;
	case REJ:
		lprintf(T_ROSEHDR, "REJ R%d\n", (data[2] >> 5) & 0x07);
		return;
	}

	lprintf(T_ROSEHDR, "UNKNOWN\n");
	data_dump(data, length, 1);
}

static char *clear_code(unsigned char code)
{
	static char buffer[25];

	if (code == 0x00 || (code & 0x80) == 0x80)
		return "DTE Originated";
	if (code == 0x01)
		return "Number Busy";
	if (code == 0x09)
		return "Out Of Order";
	if (code == 0x11)
		return "Remote Procedure Error";
	if (code == 0x19)
		return "Reverse Charging Acceptance Not Subscribed";
	if (code == 0x21)
		return "Incompatible Destination";
	if (code == 0x29)
		return "Fast Select Acceptance Not Subscribed";
	if (code == 0x39)
		return "Destination Absent";
	if (code == 0x03)
		return "Invalid Facility Requested";
	if (code == 0x0B)
		return "Access Barred";
	if (code == 0x13)
		return "Local Procedure Error";
	if (code == 0x05)
		return "Network Congestion";
	if (code == 0x0D)
		return "Not Obtainable";
	if (code == 0x15)
		return "RPOA Out Of Order";

	sprintf(buffer, "Unknown %02X", code);

	return buffer;
}

static char *reset_code(unsigned char code)
{
	static char buffer[25];

	if (code == 0x00 || (code & 0x80) == 0x80)
		return "DTE Originated";
	if (code == 0x03)
		return "Remote Procedure Error";
	if (code == 0x11)
		return "Incompatible Destination";
	if (code == 0x05)
		return "Local Procedure Error";
	if (code == 0x07)
		return "Network Congestion";

	sprintf(buffer, "Unknown %02X", code);

	return buffer;
}

static char *restart_code(unsigned char code)
{
	static char buffer[25];

	if (code == 0x00 || (code & 0x80) == 0x80)
		return "DTE Originated";
	if (code == 0x01)
		return "Local Procedure Error";
	if (code == 0x03)
		return "Network Congestion";
	if (code == 0x07)
		return "Network Operational";

	sprintf(buffer, "Unknown %02X", code);

	return buffer;
}

static char *dump_x25_addr(unsigned char *data)
{
	static char buffer[25];

	sprintf(buffer, "%02X%02X,%02X%02X%02X", data[0], data[1], data[2],
		data[3], data[4]);

	return buffer;
}

/* An AX.25 callsign of a facility: 6 shifted characters and an SSID byte.
 * Written into buf (size bytes); a field shorter than 7 bytes gives "?". */
static char *dump_ax25_call(char *buf, int size, unsigned char *data,
			    int l_data)
{
	char call[7];
	char *ptr = call;
	int i;

	if (l_data < 7) {
		snprintf(buf, size, "?");
		return buf;
	}
	for (i = 0; i < 6; i++) {
		*ptr = (data[i] >> 1) & 0x7F;
		if (*ptr != ' ')
			++ptr;
	}
	*ptr = '\0';
	snprintf(buf, size, "%s-%d", call, (data[6] >> 1) & 0x0F);

	return buf;
}

/* Decode the facilities of a CALL REQUEST. The first byte is the length of
 * the facility field. Each facility code gives the size of its parameter
 * by its class (X.25): 00-3F one byte, 40-7F two, 80-BF three, C0-FF a
 * length byte followed by that many bytes. Every field is checked against
 * the facility length and the frame, so nothing is read past them. */
static void facility(unsigned char *data, int lgtot)
{
	unsigned char *end;
	int fct = 0, l, l2, lcall;
	char call[16], tmp[16];
	char digis[80], digid[80];
	char indorig[11], inddest[11];
	char addstorig[16], addstdest[16];
	int digi_fac = 0;

	digid[0] = digis[0] = '\0';
	indorig[0] = inddest[0] = '\0';
	addstorig[0] = addstdest[0] = '\0';

	if (lgtot < 1)
		return;
	l = *data++;
	if (l > lgtot - 1) {
		lprintf(T_ERROR, "Facilities longer than the frame\n");
		l = lgtot - 1;
	}
	end = data + l;

	while (data < end) {
		fct = *data++;
		switch (fct & 0xC0) {
		case 0x00:
			l = 1;
			break;
		case 0x40:
			l = 2;
			break;
		case 0x80:
			l = 3;
			break;
		default:
			if (data >= end)
				goto bad;
			l = *data++;
			break;
		}
		if (l > end - data)
			goto bad;

		switch (fct) {
		case 0x00:
			/* Marker: 00 national, 0F CCITT */
			break;
		case 0x3F:
			/* Used if call request via L2 digi instead of L3 */
			lprintf(T_ROSEHDR, "Facility 3F%2.2X\n", data[0]);
			break;
		case 0x7F:
			/* Random number to avoid loops */
			lprintf(T_ROSEHDR, "NbAlea: %2.2X%2.2X\n", data[0],
				data[1]);
			break;
		case 0xE9:
			/* Destination digi (for compatibility) */
			if (!digi_fac)
				dump_ax25_call(digid, sizeof(digid), data, l);
			break;
		case 0xEB:
			/* Origin digi (for compatibility) */
			if (!digi_fac)
				dump_ax25_call(digis, sizeof(digis), data, l);
			break;
		case 0xED:
			/* Out of order : callsign */
			lprintf(T_ROSEHDR, "at %s",
				dump_ax25_call(call, sizeof(call), data, l));
			break;
		case 0xEE:
			/* Out of order : address */
			if (l >= 1 + ROSE_ADDR_LEN)
				lprintf(T_ROSEHDR, " @%s\n",
					dump_x25_addr(data + 1));
			break;
		case 0xEF:
			/* Digipeaters: 7 bytes each, 6 at most */
			digid[0] = digis[0] = '\0';
			for (l2 = 0; l2 + 7 <= l && l2 <= 6 * 7; l2 += 7) {
				dump_ax25_call(tmp, sizeof(tmp), data + l2, 7);
				if (data[l2 + 6] & AX25_HBIT) {
					strcat(digis, tmp);
					strcat(digis, " ");
				} else {
					strcat(digid, tmp);
					strcat(digid, " ");
				}
			}
			digi_fac = 1;
			break;
		case 0xC9:
			/* Address and callsign of the remote node */
		case 0xCB:
			/* Address and callsign of the local node: 4 bytes, the
			 * number of digits, the address, then the callsign */
			if (l < 5 + ROSE_ADDR_LEN)
				goto bad;
			lcall = l - 5 - ROSE_ADDR_LEN;
			if (lcall > 10)
				lcall = 10;
			if (fct == 0xCB) {
				strcpy(addstorig, dump_x25_addr(data + 5));
				memcpy(indorig, data + 5 + ROSE_ADDR_LEN, lcall);
				indorig[lcall] = '\0';
			} else {
				strcpy(addstdest, dump_x25_addr(data + 5));
				memcpy(inddest, data + 5 + ROSE_ADDR_LEN, lcall);
				inddest[lcall] = '\0';
			}
			break;
		default:
			lprintf(T_ROSEHDR, "Unknown Facility Type %2.2X\n",
				fct);
			break;
		}
		data += l;
	}

	if (*indorig && *inddest) {
		/* Build the displayed string */
		lprintf(T_ROSEHDR, "fm %-9s @%s", indorig, addstorig);
		if (*digis)
			lprintf(T_ROSEHDR, " via %s", digis);
		lprintf(T_ROSEHDR, "\n");
		lprintf(T_ROSEHDR, "to %-9s @%s", inddest, addstdest);
		if (*digid)
			lprintf(T_ROSEHDR, " via %s", digid);
		lprintf(T_ROSEHDR, "\n");
	}
	return;

bad:
	lprintf(T_ERROR, "Bad facility %2.2X\n", fct);
}
