/* AX25 header tracing
 * Copyright 1991 Phil Karn, KA9Q
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "listen.h"

#define	LAPB_UNKNOWN	0
#define	LAPB_COMMAND	1
#define	LAPB_RESPONSE	2

#define	SEG_FIRST	0x80
#define	SEG_REM		0x7F

#define	PID_SEGMENT	0x08
#define	PID_ARP		0xCD
#define	PID_NETROM	0xCF
#define	PID_IP		0xCC
#define	PID_X25		0x01
#define	PID_TEXNET	0xC3
#define	PID_FLEXNET	0xCE
#define	PID_OPENTRAC	0x77
#define	PID_NO_L3	0xF0

#define	I		0x00
#define	S		0x01
#define	RR		0x01
#define	RNR		0x05
#define	REJ		0x09
#define	U		0x03
#define	SABM		0x2F
#define	SABME		0x6F
#define	DISC		0x43
#define	DM		0x0F
#define	UA		0x63
#define	FRMR		0x87
#define	UI		0x03
#define	PF		0x10
#define	EPF		0x01

#define	MMASK		7

#define	HDLCAEB		0x01
#define	SSID		0x1E
#define	REPEATED	0x80
#define	C		0x80
#define	SSSID_SPARE	0x40
#define	ESSID_SPARE	0x20

#define	ALEN		6
#define	AXLEN		7

#define	W		1
#define	X		2
#define	Y		4
#define	Z		8

static int ftype(unsigned char *, int *, int *, int *, int *, int);
static char *decode_type(int);

#define NDAMA_STRING ""
#define DAMA_STRING " [DAMA]"


/* FlexNet header compression display by Thomas Sailer t.sailer@alumni.ethz.ch */

/* Dump an AX.25 packet header */
void ax25_dump(unsigned char *data, int length, int hexdump)
{
	char tmp[15];
	int ctlen, nr, ns, pf, pid, seg, type, end, cmdrsp, extseq;
	char *dama;

	/* check for FlexNet compressed header first; FlexNet header compressed packets are at least 8 bytes long */
	if (length < 8) {
		/* Something wrong with the header */
		lprintf(T_ERROR, "AX25: bad header!\n");
		return;
	}
	if (data[1] & HDLCAEB) {
		/* this is a FlexNet compressed header */
		lprintf(T_PROTOCOL, " ");
		tmp[6] = tmp[7] = extseq = 0;
		tmp[0] = ' ' + (data[2] >> 2);
		tmp[1] = ' ' + ((data[2] << 4) & 0x30) + (data[3] >> 4);
		tmp[2] = ' ' + ((data[3] << 2) & 0x3c) + (data[4] >> 6);
		tmp[3] = ' ' + (data[4] & 0x3f);
		tmp[4] = ' ' + (data[5] >> 2);
		tmp[5] = ' ' + ((data[5] << 4) & 0x30) + (data[6] >> 4);
		if (data[6] & 0xf)
			sprintf(tmp + 7, "-%d", data[6] & 0xf);
		lprintf(T_ADDR, "%d->%s%s",
			(data[0] << 6) | ((data[1] >> 2) & 0x3f),
			strtok(tmp, " "), tmp + 7);
		cmdrsp = (data[1] & 2) ? LAPB_COMMAND : LAPB_RESPONSE;
		dama = NDAMA_STRING;
		data += 7;
		length -= 7;
	} else {
		/* Extract the address header */
		if (length < (AXLEN + AXLEN + 1)) {
			/* Something wrong with the header */
			lprintf(T_ERROR, "AX25: bad header!\n");
			return;
		}

		if ((data[AXLEN + ALEN] & SSSID_SPARE) == SSSID_SPARE) {
			extseq = 0;
/*                      lprintf(T_PROTOCOL, " ");  */
		} else {
			extseq = 1;
			lprintf(T_PROTOCOL, "EAX25: ");
		}

		if ((data[AXLEN + ALEN] & ESSID_SPARE) == ESSID_SPARE)
			dama = NDAMA_STRING;
		else
			dama = DAMA_STRING;

		lprintf(T_AXHDR, "fm ");
		lprintf(T_ADDR, "%s", pax25(tmp, data + AXLEN));
		lprintf(T_AXHDR, " to ");
		lprintf(T_ADDR, "%s", pax25(tmp, data));

		cmdrsp = LAPB_UNKNOWN;

		if ((data[ALEN] & C) && !(data[AXLEN + ALEN] & C))
			cmdrsp = LAPB_COMMAND;

		if ((data[AXLEN + ALEN] & C) && !(data[ALEN] & C))
			cmdrsp = LAPB_RESPONSE;

		end = (data[AXLEN + ALEN] & HDLCAEB);

		data += (AXLEN + AXLEN);
		length -= (AXLEN + AXLEN);

		if (!end) {
			lprintf(T_AXHDR, " via");

			while (!end) {
				/* Print digi string */
				lprintf(T_ADDR, " %s%s", pax25(tmp, data),
					(data[ALEN] & REPEATED) ? "*" :
					"");

				end = (data[ALEN] & HDLCAEB);

				data += AXLEN;
				length -= AXLEN;
			}
		}
	}

	if (length == 0)
		return;

	ctlen = ftype(data, &type, &ns, &nr, &pf, extseq);

	data += ctlen;
	length -= ctlen;

	lprintf(T_AXHDR, " ctl %s", decode_type(type));

	if ((type & 0x3) != U)	/* I or S frame? */
		lprintf(T_AXHDR, "%d", nr);

	if (type == I)
		lprintf(T_AXHDR, "%d", ns);

	switch (cmdrsp) {
	case LAPB_COMMAND:
		if (pf)
			lprintf(T_AXHDR, "+");
		else
			lprintf(T_AXHDR, "^");
		break;
	case LAPB_RESPONSE:
		if (pf)
			lprintf(T_AXHDR, "-");
		else
			lprintf(T_AXHDR, "v");
		break;
	default:
		break;
	}

	if (type == I || type == UI) {
		/* Decode I field */
		if (length > 0) {	/* Get pid */
			pid = *data++;
			length--;

			lprintf(T_AXHDR, " pid=%X", pid);

			switch (pid) {
			case PID_SEGMENT:
				lprintf(T_AXHDR, "(segment)");
				break;
			case PID_ARP:
				lprintf(T_AXHDR, "(ARP)");
				break;
			case PID_NETROM:
				lprintf(T_AXHDR, "(NET/ROM)");
				break;
			case PID_IP:
				lprintf(T_AXHDR, "(IP)");
				break;
			case PID_X25:
				lprintf(T_AXHDR, "(X.25)");
				break;
			case PID_TEXNET:
				lprintf(T_AXHDR, "(TEXNET)");
				break;
			case PID_FLEXNET:
				lprintf(T_AXHDR, "(FLEXNET)");
				break;
			case PID_OPENTRAC:
				lprintf(T_AXHDR, "(OPENTRAC)");
				break;
			case PID_NO_L3:
				lprintf(T_AXHDR, "(Text)");
				break;
			}
			lprintf(T_AXHDR, "%s len %d ", dama, length);

			if (timestamp)
				display_timestamp();

			if (pid == PID_SEGMENT) {
				seg = *data++;
				length--;
				lprintf(T_AXHDR, "%s remain %u",
					seg & SEG_FIRST ? " First seg;" :
					"", seg & SEG_REM);

				if (seg & SEG_FIRST) {
					pid = *data++;
					length--;
				}
			}
			lprintf(T_AXHDR, "\n");

			switch (pid) {
				case PID_SEGMENT:
					data_dump(data, length, hexdump);
					break;
				case PID_ARP:
					arp_dump(data, length);
					break;
				case PID_NETROM:
					netrom_dump(data, length, hexdump, type);
					break;
				case PID_IP:
					ip_dump(data, length, hexdump);
					break;
				case PID_X25:
					rose_dump(data, length, hexdump);
					break;
				case PID_TEXNET:
					data_dump(data, length, hexdump);
					break;
				case PID_FLEXNET:
					flexnet_dump(data, length, hexdump);
					break;
				case PID_OPENTRAC:
					opentrac_dump(data, length, hexdump);
					break;
				case PID_NO_L3:
					data_dump(data, length, hexdump);
					break;
				default:
					data_dump(data, length, hexdump);
					break;
			}
		}
	} else if (type == FRMR && length >= 3) {
		/* FIX ME XXX
		   lprintf(T_AXHDR, ": %s", decode_type(ftype(data[0])));
		 */
		lprintf(T_AXHDR, ": %02X", data[0]);
		lprintf(T_AXHDR, " Vr = %d Vs = %d",
			(data[1] >> 5) & MMASK, (data[1] >> 1) & MMASK);
		if (data[2] & W)
			lprintf(T_ERROR, " Invalid control field");
		if (data[2] & X)
			lprintf(T_ERROR, " Illegal I-field");
		if (data[2] & Y)
			lprintf(T_ERROR, " Too-long I-field");
		if (data[2] & Z)
			lprintf(T_ERROR, " Invalid seq number");
		lprintf(T_AXHDR, "%s ", dama);

		if (timestamp)
			display_timestamp();
		lprintf(T_AXHDR, "\n");
	} else if ((type == SABM || type == UA) && length >= 2) {
		/* FlexNet transmits the QSO "handle" for header
		 * compression in SABM and UA frame data fields 
		 */
		lprintf(T_AXHDR, " [%d]%s ", (data[0] << 8) | data[1],
			dama);
		if (timestamp)
			display_timestamp();
		lprintf(T_AXHDR, "\n");
	} else {
		lprintf(T_AXHDR, "%s ", dama);
		if (timestamp)
			display_timestamp();
		lprintf(T_AXHDR, "\n");
	}
}

static char *decode_type(int type)
{
	switch (type) {
	case I:
		return "I";
	case SABM:
		return "SABM";
	case SABME:
		return "SABME";
	case DISC:
		return "DISC";
	case DM:
		return "DM";
	case UA:
		return "UA";
	case RR:
		return "RR";
	case RNR:
		return "RNR";
	case REJ:
		return "REJ";
	case FRMR:
		return "FRMR";
	case UI:
		return "UI";
	default:
		return "[invalid]";
	}
}

char *pax25(char *buf, unsigned char *data)
{
	int i, ssid;
	char *s;
	char c;

	s = buf;

	for (i = 0; i < ALEN; i++) {
		c = (data[i] >> 1) & 0x7F;

		if (!isalnum(c) && c != ' ') {
			strcpy(buf, "[invalid]");
			return buf;
		}

		if (c != ' ')
			*s++ = c;
	}

	if ((ssid = (data[ALEN] & SSID)) != 0)
		sprintf(s, "-%d", ssid >> 1);
	else
		*s = '\0';

	return (buf);
}

static int ftype(unsigned char *data, int *type, int *ns, int *nr, int *pf,
		 int extseq)
{
	*ns = *nr = 0;				/* To avoid warnings  */

	if (extseq) {
		if ((*data & 0x01) == 0) {	/* An I frame is an I-frame ... */
			*type = I;
			*ns = (*data >> 1) & 127;
			data++;
			*nr = (*data >> 1) & 127;
			*pf = *data & EPF;
			return 2;
		}
		if (*data & 0x02) {
			*type = *data & ~PF;
			*pf = *data & PF;
			return 1;
		} else {
			*type = *data;
			data++;
			*nr = (*data >> 1) & 127;
			*pf = *data & EPF;
			return 2;
		}
	} else {
		if ((*data & 0x01) == 0) {	/* An I frame is an I-frame ... */
			*type = I;
			*ns = (*data >> 1) & 7;
			*nr = (*data >> 5) & 7;
			*pf = *data & PF;
			return 1;
		}
		if (*data & 0x02) {	/* U-frames use all except P/F bit for type */
			*type = *data & ~PF;
			*pf = *data & PF;
			return 1;
		} else {	/* S-frames use low order 4 bits for type */
			*type = *data & 0x0F;
			*nr = (*data >> 5) & 7;
			*pf = *data & PF;
			return 1;
		}
	}
}
