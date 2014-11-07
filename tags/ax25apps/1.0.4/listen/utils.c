/*
 * Copyright 1996, 1997 Heikki Hannikainen OH7LZB <oh7lzb@sral.fi>
 *
 * Portions and ideas (like the ibm character mapping) from
 *	Tomi Manninen OH2BNS <oh2bns@sral.fi>
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <curses.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

#include "listen.h"

int color = 0;			/* Colorized? */
int sevenbit = 1;		/* Are we on a 7-bit terminal? */
int ibmhack = 0;		/* IBM mapping? */

/* mapping of IBM codepage 437 chars 128-159 to ISO latin1 equivalents
 * (158 and 159 are mapped to space)
 */

unsigned char ibm_map[32] = {
	199, 252, 233, 226, 228, 224, 229, 231,
	234, 235, 232, 239, 238, 236, 196, 197,
	201, 230, 198, 244, 246, 242, 251, 249,
	255, 214, 220, 162, 163, 165, 32, 32
};

/*
 *	Printf in Technicolor (TM) (available in selected theatres only)
 */

void lprintf(int dtype, char *fmt, ...)
{
	va_list args;
	char str[1024];
	unsigned char *p;
	chtype ch;

	va_start(args, fmt);
	vsnprintf(str, 1024, fmt, args);
	va_end(args);

	if (color) {
		for (p = str; *p != '\0'; p++) {
			ch = *p;

			if (sevenbit && ch > 127)
				ch = '.';

			if ((ch > 127 && ch < 160) && ibmhack)
				ch = ibm_map[ch - 128] | A_BOLD;
			else if ((ch < 32) && (ch != '\n'))
				ch = (ch + 64) | A_REVERSE;

			if ((dtype == T_ADDR) || (dtype == T_PROTOCOL)
			    || (dtype == T_AXHDR) || (dtype == T_IPHDR)
			    || (dtype == T_ROSEHDR) || (dtype == T_PORT)
			    || (dtype == T_TIMESTAMP))
				ch |= A_BOLD;

			ch |= COLOR_PAIR(dtype);

			addch(ch);
		}
	} else {
		for (p = str; *p != '\0'; p++)
			if ((*p < 32 && *p != '\n')
			    || (*p > 126 && *p < 160 && sevenbit))
				*p = '.';
		if (fputs(str, stdout) == -1)
			exit(1);
		if (fflush(stdout) == -1)
			exit(1);
	}
}

int initcolor(void)
{
	if (!has_colors())
		return 0;
	initscr();		/* Start ncurses */
	start_color();		/* Initialize color support */
	refresh();		/* Clear screen */
	noecho();		/* Don't echo */
	wattrset(stdscr, 0);	/* Clear attributes */
	scrollok(stdscr, TRUE);	/* Like a scrolling Stone... */
	leaveok(stdscr, TRUE);	/* Cursor position doesn't really matter */
	idlok(stdscr, TRUE);	/* Use hardware ins/del of the terminal */
	nodelay(stdscr, TRUE);	/* Make getch() nonblocking */

	/* Pick colors for each type */
	init_pair(T_PORT, COLOR_GREEN, COLOR_BLACK);
	init_pair(T_DATA, COLOR_WHITE, COLOR_BLACK);
	init_pair(T_ERROR, COLOR_RED, COLOR_BLACK);
	init_pair(T_PROTOCOL, COLOR_CYAN, COLOR_BLACK);
	init_pair(T_AXHDR, COLOR_WHITE, COLOR_BLACK);
	init_pair(T_IPHDR, COLOR_WHITE, COLOR_BLACK);
	init_pair(T_ADDR, COLOR_GREEN, COLOR_BLACK);
	init_pair(T_ROSEHDR, COLOR_WHITE, COLOR_BLACK);
	init_pair(T_TIMESTAMP, COLOR_YELLOW, COLOR_BLACK);
	init_pair(T_KISS, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(T_BPQ, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(T_TCPHDR, COLOR_BLUE, COLOR_BLACK);
	init_pair(T_FLEXNET, COLOR_BLUE, COLOR_BLACK);
	init_pair(T_OPENTRAC, COLOR_YELLOW, COLOR_BLACK);


	return 1;
}

char *servname(int port, char *proto)
{
	struct servent *serv;
	static char str[16];

	if ((serv = getservbyport(htons(port), proto)))
		strncpy(str, serv->s_name, 16);
	else
		snprintf(str, 16, "%i", port);

	return str;
}
