/* kiss.c    KISS assembly and byte-stuffing stuff
 *
 * Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.
 * This software may be freely used, distributed, or modified, providing
 * this header is not removed.
 *
 * This is the only module that knows about the internal structure of
 * KISS frames.
 */

/*
 * dual port changes Feb 95 ve3pnx & ve3djf
 */

#include <stdio.h>
#include <syslog.h>
#include "ax25ipd.h"

#define FEND  0xc0
#define FESC  0xdb
#define TFEND 0xdc
#define TFESC 0xdd

unsigned char iframe[MAX_FRAME];
unsigned char *ifptr;
int ifcount;
int iescaped;

unsigned char oframe[MAX_FRAME];
unsigned char *ofptr;
int ofcount;

#define PTABLE_SIZE 10

struct param_table_entry {
	unsigned char parameter;
	unsigned char value;
} param_tbl[PTABLE_SIZE];

int param_tbl_top;


/*
 * Initialize the KISS variables
 */

void kiss_init(void)
{
	ifptr = iframe;
	ifcount = 0;
	iescaped = 0;
	ofptr = oframe;
	ofcount = 0;
	param_tbl_top = 0;
}

/*
 * Assemble a kiss frame from random hunks of incoming data
 * Calls the "from_kiss" routine with the kiss frame when a
 * frame has been assembled.
 */

void assemble_kiss(unsigned char *buf, int l)
{
	int i;
	unsigned char c;

	for (i = 0; i < l; i++, buf++) {
		c = *buf;
		if (c == FEND) {
			if (ifcount > 0) {
				/* Make sure that the control byte is zero */
				if (*iframe == '\0' || *iframe == 0x10) {
					/* Room for CRC in buffer? */
					if (ifcount < (MAX_FRAME - 2)) {
						stats.kiss_in++;
						from_kiss(iframe +
							  1, ifcount - 1);
					} else {
						stats.kiss_toobig++;
						LOGL2
						    ("assemble_kiss: dumped - frame too large\n");
					}
				} else {
					stats.kiss_badtype++;
					LOGL2
					    ("assemble_kiss: dumped - control byte non-zero\n");
				}
			}
			ifcount = 0;
			iescaped = 0;
			ifptr = iframe;
			continue;
		}
		if (c == FESC) {
			iescaped = 1;
			continue;
		}
		if (iescaped) {
			if (c == TFEND)
				c = FEND;
			if (c == TFESC)
				c = FESC;
			iescaped = 0;
		}
		if (ifcount < MAX_FRAME) {
			*ifptr = c;
			ifptr++;
			ifcount++;
		}
	}			/* for every character in the buffer */
}

/* convert a standard AX25 frame into a kiss frame */
void send_kiss(unsigned char type, unsigned char *buf, int l)
{
#define KISSEMIT(x) if(ofcount<MAX_FRAME){*ofptr=(x);ofptr++;ofcount++;}

	int i;

	ofptr = oframe;
	ofcount = 0;

	KISSEMIT(FEND);

	if (type == FEND) {
		KISSEMIT(FESC);
		KISSEMIT(TFEND);
	} else if (type == FESC) {
		KISSEMIT(FESC);
		KISSEMIT(TFESC);
	} else {
		KISSEMIT(type);
	}

	for (i = 0; i < l; i++, buf++) {
		if (*buf == FEND) {
			KISSEMIT(FESC);
			KISSEMIT(TFEND);
		} else if (*buf == FESC) {
			KISSEMIT(FESC);
			KISSEMIT(TFESC);
		} else {
			KISSEMIT(*buf);
		}
	}			/* for each character in the incoming AX25 frame */

	KISSEMIT(FEND);

	send_tty(oframe, ofcount);
}

/* Add an entry to the parameter table */
void param_add(int p, int v)
{
	if (param_tbl_top >= PTABLE_SIZE) {
		fprintf(stderr, "param table is full; entry ignored.\n");
	}
	param_tbl[param_tbl_top].parameter = p & 0xff;
	param_tbl[param_tbl_top].value = v & 0xff;
	LOGL4("added param: %d\t%d\n",
	      param_tbl[param_tbl_top].parameter,
	      param_tbl[param_tbl_top].value);
	param_tbl_top++;
	return;
}

/* dump the contents of the parameter table */
void dump_params(void)
{
	int i;

	LOGL1("\n%d parameters\n", param_tbl_top);
	for (i = 0; i < param_tbl_top; i++) {
		LOGL1("  %d\t%d\n",
		      param_tbl[i].parameter, param_tbl[i].value);
	}
	fflush(stdout);
}

/* send the parameters to the TNC */
void send_params(void)
{
	int i;
	unsigned char p, v;

	for (i = 0; i < param_tbl_top; i++) {
		p = param_tbl[i].parameter;
		v = param_tbl[i].value;
		send_kiss(p, &v, 1);
		LOGL2("send_params: param %d %d\n", p, v);
	}
}
