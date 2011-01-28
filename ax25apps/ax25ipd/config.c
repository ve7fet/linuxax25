/* config.c    config file manipulation routines
 *
 * Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.
 * This software may be freely used, distributed, or modified, providing
 * this header is not removed.
 *
 */

/*
 * Modifications made for dual port TNC's
 * by Michael Durrant VE3PNX and D. Jeff Dionne  February 4, 1995
 */

#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <memory.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include "ax25ipd.h"

/* Initialize the config table */
void config_init(void)
{
	int i;

	*ttydevice = '\0';
	for (i = 0; i < 7; i++)
		mycallsign[i] = '\0';
	for (i = 0; i < 7; i++)
		myalias[i] = '\0';
	for (i = 0; i < 7; i++)
		mycallsign2[i] = '\0';
	for (i = 0; i < 7; i++)
		myalias2[i] = '\0';
	digi = 1;
	ttyspeed = 9600;
	loglevel = 0;
	bc_interval = 0;
	bc_text[0] = '\0';
	bc_every = 0;
	my_udp = htons(0);
	udp_mode = 0;
	ip_mode = 0;
	dual_port = 0;

	stats.kiss_in = 0;
	stats.kiss_toobig = 0;
	stats.kiss_badtype = 0;
	stats.kiss_tooshort = 0;
	stats.kiss_not_for_me = 0;
	stats.kiss_i_am_dest = 0;
	stats.kiss_no_ip_addr = 0;
	stats.kiss_out = 0;
	stats.kiss_beacon_outs = 0;
	stats.udp_in = 0;
	stats.udp_out = 0;
	stats.ip_in = 0;
	stats.ip_out = 0;
	stats.ip_failed_crc = 0;
	stats.ip_tooshort = 0;
	stats.ip_not_for_me = 0;
	stats.ip_i_am_dest = 0;
}

/* Open and read the config file */

void config_read(char *f)
{
	FILE *cf;
	char buf[256], cbuf[256];
	int errflag, e, lineno;
	char *fname;

	if (f==NULL || strlen(f) == 0)
		fname=CONF_AX25IPD_FILE;
	else
		fname=f;

	if ((cf = fopen(fname, "r")) == NULL) {
		fprintf(stderr,
			"Config file %s not found or could not be opened\n",
			fname);
		exit(1);
	}

	errflag = 0;
	lineno = 0;
	while (fgets(buf, 255, cf) != NULL) {
		strcpy(cbuf, buf);
		lineno++;
		if ((e = parse_line(buf)) < 0) {
			fprintf(stderr, "Config error at line %d: ",
				lineno);
			if (e == -1)
				fprintf(stderr, "Missing argument\n");
			else if (e == -2)
				fprintf(stderr, "Bad callsign format\n");
			else if (e == -3)
				fprintf(stderr, "Bad option - on/off\n");
			else if (e == -4)
				fprintf(stderr, "Bad option - tnc/digi\n");
			else if (e == -5)
				fprintf(stderr, "Host not known\n");
			else if (e == -6)
				fprintf(stderr, "Unknown command\n");
			else if (e == -7)
				fprintf(stderr, "Text string too long\n");
			else if (e == -8)
				fprintf(stderr,
					"Bad option - every/after\n");
			else if (e == -9)
				fprintf(stderr, "Bad option - ip/udp\n");
			else
				fprintf(stderr, "Unknown error\n");
			fprintf(stderr, "%s", cbuf);
			errflag++;
		}
	}
	if (errflag)
		exit(1);

	if (strlen(ttydevice) == 0) {
		fprintf(stderr, "No device specified in config file\n");
		exit(1);
	}

	if ((udp_mode == 0) && (ip_mode == 0)) {
		fprintf(stderr, "Must specify ip and/or udp sockets\n");
		exit(1);
	}

	if (digi) {
		if (mycallsign[0] == '\0') {
			fprintf(stderr, "No mycall line in config file\n");
			exit(1);
		}
	}
	if ((digi) && (dual_port)) {
		if (mycallsign2[0] == '\0') {
			fprintf(stderr,
				"No mycall2 line in config file\n");
			exit(1);
		}
	}
	fclose(cf);
}

/* Process each line from the config file.  The return value is encoded. */
int parse_line(char *buf)
{
	char *p, *q;
	unsigned char tcall[7], tip[4];
	struct hostent *he;
	int i, j, uport;
	unsigned int flags;

	p = strtok(buf, " \t\n\r");

	if (p == NULL)
		return 0;
	if (*p == '#')
		return 0;

	if (strcmp(p, "mycall") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		if (a_to_call(q, mycallsign) != 0)
			return -2;
		return 0;

	} else if (strcmp(p, "mycall2") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		if (a_to_call(q, mycallsign2) != 0)
			return -2;
		return 0;

	} else if (strcmp(p, "myalias") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		if (a_to_call(q, myalias) != 0)
			return -2;
		dual_port = 1;
		if (mycallsign2[0] == '\0') {
			dual_port = 0;
		}
		return 0;

	} else if (strcmp(p, "myalias2") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		if (a_to_call(q, myalias2) != 0)
			return -2;
		return 0;

	} else if (strcmp(p, "device") == 0) {
		/* already set? i.e. with commandline option, which overwrites
		 * the preconfigured setting. useful for systems with unix98
		 * style pty's
		 */
		if (*ttydevice)
			return 0;
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		strncpy(ttydevice, q, sizeof(ttydevice)-1);
		ttydevice[sizeof(ttydevice)-1] = 0;
		return 0;

	} else if (strcmp(p, "mode") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		if (strcmp(q, "digi") == 0)
			digi = 1;
		else if (strcmp(q, "tnc") == 0)
			digi = 0;
		else
			return -4;
		return 0;

	} else if (strcmp(p, "speed") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		ttyspeed = atoi(q);
		return 0;

	} else if (strcmp(p, "socket") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		if (strcmp(q, "ip") == 0) {
			ip_mode = 1;
		} else if (strcmp(q, "udp") == 0) {
			udp_mode = 1;
			my_udp = htons(DEFAULT_UDP_PORT);
			q = strtok(NULL, " \t\n\r");
			if (q != NULL) {
				i = atoi(q);
				if (i > 0)
					my_udp = htons(i);
			}
		} else
			return -9;
		return 0;

	} else if (strcmp(p, "beacon") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;

		if (strcmp(q, "every") == 0)
			bc_every = 1;
		else if (strcmp(q, "after") == 0)
			bc_every = 0;
		else
			return -8;

		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		bc_interval = atoi(q);
		return 0;

/* This next one is a hack!!!!!! watch out!!!! */
	} else if (strcmp(p, "btext") == 0) {
		q = p + strlen(p) + 1;
		if (strlen(q) < 2)
			return -1;	/* line ends with a \n */
		if (strlen(q) > sizeof(bc_text))
			return -7;
		q[strlen(q) - 1] = '\0';
		strncpy(bc_text, q, sizeof(bc_text)-1);
		bc_text[sizeof(bc_text)-1] = 0;
		return 0;

	} else if (strcmp(p, "loglevel") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		loglevel = atoi(q);
		return 0;

	} else if (strcmp(p, "route") == 0) {
		uport = 0;
		flags = 0;

		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;

		if (a_to_call(q, tcall) != 0)
			return -2;

		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		he = gethostbyname(q);
		if (he != NULL) {
			memcpy(tip, he->h_addr_list[0], 4);
		} else {	/* maybe user specified a numeric addr? */
			j = inet_addr(q);
			if (j == -1)
				return -5;	/* if -1, bad deal! */
			memcpy(tip, (char *) &j, 4);
		}

		while ((q = strtok(NULL, " \t\n\r")) != NULL) {
			if (strcmp(q, "udp") == 0) {
				uport = DEFAULT_UDP_PORT;
				q = strtok(NULL, " \t\n\r");
				if (q != NULL) {
					i = atoi(q);
					if (i > 0)
						uport = i;
				}
			} else {
				/* Test for broadcast flag */
				if (strchr(q, 'b')) {
					flags |= AXRT_BCAST;
				}

				/* Test for Default flag */
				if (strchr(q, 'd')) {
					flags |= AXRT_DEFAULT;
				}
			}
		}
		route_add(tip, tcall, uport, flags);
		return 0;

	} else if (strcmp(p, "broadcast") == 0) {

		while ((q = strtok(NULL, " \t\n\r")) != NULL) {
			if (a_to_call(q, tcall) != 0)
				return -2;
			bcast_add(tcall);
		}
		return 0;

	} else if (strcmp(p, "param") == 0) {
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		i = atoi(q);
		q = strtok(NULL, " \t\n\r");
		if (q == NULL)
			return -1;
		j = atoi(q);
		param_add(i, j);
		return 0;
	}
	return -999;
}

/* Convert ascii callsign to internal format */
int a_to_call(char *text, unsigned char *tcall)
{
	int i;
	int ssid;
	unsigned char c;

	if (strlen(text) == 0)
		return -1;

	ssid = 0;
	for (i = 0; i < 6; i++) {
		tcall[i] = (' ' << 1);
	}
	tcall[6] = '\0';

	for (i = 0; i < strlen(text); i++) {
		c = text[i];
		if (c == '-') {
			ssid = atoi(&text[i + 1]);
			if (ssid > 15)
				return -1;
			tcall[6] = (ssid << 1);
			return 0;
		}
		if (islower(c))
			c = toupper(c);
		if (i > 5)
			return -1;
		tcall[i] = (c << 1);
	}
	return 0;
}

/* Convert internal callsign to printable format */
char *call_to_a(unsigned char *tcall)
{
	int i;
	int ssid;
	char *tptr;
	static char t[10];

	for (i = 0, tptr = t; i < 6; i++) {
		if (tcall[i] == (' ' << 1))
			break;
		*tptr = tcall[i] >> 1;
		tptr++;
	}

	ssid = (tcall[6] >> 1) & 0x0f;
	if (ssid > 0) {
		*tptr = '-';
		tptr++;
		if (ssid > 9) {
			*tptr = '1';
			tptr++;
			ssid -= 10;
		}
		*tptr = '0' + ssid;
		tptr++;
	}

	*tptr = '\0';
	return &t[0];
}

/* print the configuration data out */
void dump_config(void)
{
	LOGL1("\nCurrent configuration:\n");
	if (ip_mode)
		LOGL1("  socket     ip\n");
	if (udp_mode)
		LOGL1("  socket     udp on port %d\n", ntohs(my_udp));
	LOGL1("  mode       %s\n", digi ? "digi" : "tnc");
	LOGL1("  device     %s\n", ttydevice);
	LOGL1("  speed      %d\n", ttyspeed);
	if (digi)
		LOGL1("  mycall     %s\n", call_to_a(mycallsign));
	if (digi && myalias[0])
		LOGL1("  myalias    %s\n", call_to_a(myalias));
	if (bc_interval > 0) {
		LOGL1("  beacon     %s %d\n", bc_every ? "every" : "after",
		      bc_interval);
		LOGL1("  btext      %s\n", bc_text);
	}
	LOGL1("  loglevel   %d\n", loglevel);
	(void) fflush(stdout);
}
