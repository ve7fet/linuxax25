/* process.c     Handle processing and routing of AX25 frames
 *
 * Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.
 * This software may be freely used, distributed, or modified, providing
 * this header is not removed.
 *
 * This is the only module that knows about the internal structure of
 * AX25 frames.
 */

/*
 * Dual port additions by M.Durrant VE3PNX and D.J.Dionne Feb 4, 1995
 */

#include "ax25ipd.h"
#include <stdio.h>
#include <syslog.h>
/* if dual port the upper nibble will have a value of 1 (not 0) */
#define FROM_PORT2(p)   (((*(p+1))&0x10)!=0)
#define FOR_PORT2(p)    (addrmatch(p,mycallsign2) || addrmatch(p,myalias2))
/* ve3djf and ve3pnx addition above                             */
#define IS_LAST(p)      (((*(p+6))&0x01)!=0)
#define NOT_LAST(p)     (((*(p+6))&0x01)==0)
#define REPEATED(p)     (((*(p+6))&0x80)!=0)
#define NOTREPEATED(p)  (((*(p+6))&0x80)==0)
#define IS_ME(p)        (addrmatch(p,mycallsign) || addrmatch(p,myalias) || addrmatch(p,mycallsign2) || addrmatch(p,myalias2) )
#define NOT_ME(p)       (!(addrmatch(p,mycallsign) || addrmatch(p,myalias) || addrmatch(p,mycallsign2) || addrmatch(p,myalias2) ) )
#define ARE_DIGIS(f)    (((*(f+13))&0x01)==0)
#define NO_DIGIS(f)     (((*(f+13))&0x01)!=0)
#define SETREPEATED(p)  (*(p+6))|=0x80
#define SETLAST(p)      (*(p+6))|=0x01

unsigned char bcbuf[256];	/* Must be larger than bc_text!!! */
int bclen;			/* The size of bcbuf */

/*
 * Initialize the process variables
 */

void process_init(void)
{
	bclen = -1;		/* flag that we need to rebuild the bctext */
}

/*
 * handle a frame given us by the kiss routines.  The buf variable is
 * a pointer to an AX25 frame.  Note that the AX25 frame from kiss does
 * not include the CRC bytes.  These are computed by this routine, and
 * it is expected that the buffer we have has room for the CRC bytes.
 * We will either dump this frame, or send it via the IP interface.
 * 
 * If we are in digi mode, we validate in several ways:
 *   a) we must be the next digi in line to pick up the packet
 *   b) the next site to get the packet (the next listed digi, or
 *      the destination if we are the last digi) must be known to
 *      us via the route table.
 * If we pass validation, we then set the digipeated bit for our entry
 * in the packet, compute the CRC, and send the packet to the IP
 * interface.
 *
 * If we are in tnc mode, we have less work to do.
 *   a) the next site to get the packet (the next listed digi, or
 *      the destination) must be known to us via the route table.
 * If we pass validation, we compute the CRC, and send the packet to
 * the IP interface.
 */

void from_kiss(unsigned char *buf, int l)
{
	unsigned char *a, *ipaddr;

	if (l < 15) {
		LOGL2("from_kiss: dumped - length wrong!\n");
		stats.kiss_tooshort++;
		return;
	}

	if (loglevel > 2)
		dump_ax25frame("from_kiss: ", buf, l);

	if (digi) {		/* if we are in digi mode */
		a = next_addr(buf);
		if (NOT_ME(a)) {
			stats.kiss_not_for_me++;
			LOGL4("from_kiss: (digi) dumped - not for me\n");
			return;
		}
		if (a == buf) {	/* must be a digi */
			stats.kiss_i_am_dest++;
			LOGL2
			    ("from_kiss: (digi) dumped - I am destination!\n");
			return;
		}
		SETREPEATED(a);
		a = next_addr(buf);	/* find who gets it after us */
	} else {		/* must be tnc mode */
		a = next_addr(buf);
#ifdef TNC_FILTER
		if (IS_ME(a)) {
			LOGL2
			    ("from_kiss: (tnc) dumped - addressed to self!\n");
			return;
		}
#endif
	}			/* end of tnc mode */

	/* Lookup the IP address for this route */
	ipaddr = call_to_ip(a);

	if (ipaddr == NULL) {
		if (is_call_bcast(a)) {
			/* Warning - assuming buffer has room for 2 bytes */
			add_crc(buf, l);
			l += 2;
			send_broadcast(buf, l);
		} else {
			stats.kiss_no_ip_addr++;
			LOGL2
			    ("from_kiss: dumped - cannot figure out where to send this!\n");
		}
		return;
	} else {
		/* Warning - assuming buffer has room for 2 bytes */
		add_crc(buf, l);
		l += 2;
		send_ip(buf, l, ipaddr);
		if (is_call_bcast(a)) {
			send_broadcast(buf, l);
		}
	}
}

/*
 * handle a frame given us by the IP routines.  The buf variable is
 * a pointer to an AX25 frame.
 * Note that the frame includes the CRC bytes, which we dump ASAP.
 * We will either dump this frame, or send it via the KISS interface.
 * 
 * If we are in digi mode, we only validate that:
 *   a) we must be the next digi in line to pick up the packet
 * If we pass validation, we then set the digipeated bit for our entry
 * in the packet, and send the packet to the KISS send routine.
 *
 * If we are in tnc mode, we validate pretty well nothing, just like a
 * real TNC...  #define FILTER_TNC will change this.
 * We simply send the packet to the KISS send routine.
 */

void from_ip(unsigned char *buf, int l)
{
	int port = 0;
	unsigned char *a;

	if (!ok_crc(buf, l)) {
		stats.ip_failed_crc++;
		LOGL2("from_ip: dumped - CRC incorrect!\n");
		return;
	}
	l = l - 2;		/* dump the blasted CRC */

	if (l < 15) {
		stats.ip_tooshort++;
		LOGL2("from_ip: dumped - length wrong!\n");
		return;
	}

	if (loglevel > 2)
		dump_ax25frame("from_ip: ", buf, l);

	if (digi) {		/* if we are in digi mode */
		a = next_addr(buf);
		if (NOT_ME(a)) {
			stats.ip_not_for_me++;
			LOGL2("from_ip: (digi) dumped - not for me!\n");
			return;
		}
		if (a == buf) {	/* must be a digi */
			stats.ip_i_am_dest++;
			LOGL2
			    ("from_ip: (digi) dumped - I am destination!\n");
			return;
		}
		if (dual_port == 1 && FOR_PORT2(a)) {
			port = 0x10;
		}
		SETREPEATED(a);
	} else {		/* must be tnc mode */
		a = next_addr(buf);
#ifdef TNC_FILTER
		if (NOT_ME(a)) {
			LOGL2
			    ("from_ip: (tnc) dumped - I am not destination!\n");
			return;
		}
#endif
	}			/* end of tnc mode */
	if (!ttyfd_bpq)
		send_kiss(port, buf, l);
	else {
		send_bpq(buf, l);
	}
}

/*
 * Send an ID frame out the KISS port.
 */

void do_beacon(void)
{
	int i;
	unsigned char *p;

	if (bclen == 0)
		return;		/* nothing to do! */

	if (bclen < 0) {	/* build the id string */
		p = bcbuf;
		*p++ = ('I' << 1);
		*p++ = ('D' << 1);
		*p++ = (' ' << 1);
		*p++ = (' ' << 1);
		*p++ = (' ' << 1);
		*p++ = (' ' << 1);
		*p++ = '\0' | 0x60;	/* SSID, set reserved bits */

		for (i = 0; i < 6; i++)
			*p++ = mycallsign[i];
		*p++ = mycallsign[6] | 0x60;	/* ensure reserved bits are set */
		SETLAST(bcbuf + 7);	/* Set the E bit -- last address */

		*p++ = 0x03;	/* Control field -- UI frame */

		*p++ = 0xf0;	/* Protocol ID -- 0xf0 is no protocol */

		strcpy(p, bc_text);	/* add the text field */

		bclen = 16 + strlen(bc_text);	/* adjust the length nicely */
	}

	if (loglevel > 2)
		dump_ax25frame("do_beacon: ", bcbuf, bclen);
	stats.kiss_beacon_outs++;
	if (!ttyfd_bpq)
		send_kiss(0, bcbuf, bclen);
	else {
		send_bpq(bcbuf, bclen);
	}
}

/*
 * return true if the addresses supplied match
 * modified for wildcarding by vk5xxx
 */
int addrmatch(unsigned char *a, unsigned char *b)
{
	if ((*a == '\0') || (*b == '\0'))
		return 0;

	if ((*a++ ^ *b++) & 0xfe)
		return 0;	/* "K" */
	if ((*a++ ^ *b++) & 0xfe)
		return 0;	/* "A" */
	if ((*a++ ^ *b++) & 0xfe)
		return 0;	/* "9" */
	if ((*a++ ^ *b++) & 0xfe)
		return 0;	/* "W" */
	if ((*a++ ^ *b++) & 0xfe)
		return 0;	/* "S" */
	if ((*a++ ^ *b++) & 0xfe)
		return 0;	/* "B" */
	if (((*b) & 0x1e) == 0)
		return 1;	/* ssid 0 matches all ssid's */
	if ((*a++ ^ *b) & 0x1e)
		return 0;	/* ssid */
/*	if((*a++^*b++)&0x1e)return 0;      ssid (how it was ...) */
	return 1;
}

/*
 * return pointer to the next station to get this packet
 */
unsigned char *next_addr(unsigned char *f)
{
	unsigned char *a;

/* If no digis, return the destination address */
	if (NO_DIGIS(f))
		return f;

/* check each digi field.  The first one that hasn't seen it is the one */
	a = f + 7;
	do {
		a += 7;
		if (NOTREPEATED(a))
			return a;
	}
	while (NOT_LAST(a));

/* all the digis have seen it.  return the destination address */
	return f;
}

/*
 * tack on the CRC for the frame.  Note we assume the buffer is long
 * enough to have the two bytes tacked on.
 */
void add_crc(unsigned char *buf, int l)
{
	unsigned short int u;

	u = compute_crc(buf, l);
	buf[l] = u & 0xff;	/* lsb first */
	buf[l + 1] = (u >> 8) & 0xff;	/* msb next */
}

/*
 * Dump AX25 frame.
 */
void dump_ax25frame(char *t, unsigned char *buf, int l)
{
#ifdef DEBUG
	int i;
#endif
	unsigned char *a;

	printf("%s AX25: (l=%3d)   ", t, l);

	if (l < 15) {
		printf("Bogus size...\n");
		return;
	}

	printf("%s -> ", call_to_a(buf + 7));
	printf("%s", call_to_a(buf));

	if (ARE_DIGIS(buf)) {
		printf(" v");
		a = buf + 7;
		do {
			a += 7;
			printf(" %s", call_to_a(a));
			if (REPEATED(a))
				printf("*");
		}
		while (NOT_LAST(a));
	}

	printf("\n");

#ifdef DEBUG
	for (i = 0; i < l; i++)
		printf("%02x ", buf[i]);
	printf("\n");
#endif

	fflush(stdout);
}
