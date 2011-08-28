/*****************************************************************************/

/*
 *	xfhdlcchpar_main.C  -- kernel hdlc radio modem driver channel parameter setting utility.
 *
 *	Copyright (C) 1996,1997,2000  Thomas Sailer (t.sailer@alumni.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 *
 * History:
 *   0.1  14.12.1996  Started
 *   0.2  11.05.1997  introduced hdrvcomm.h
 *   0.3  05.01.2000  upgraded to fltk 1.0.7
 */

/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <net/if.h>
#include "hdrvcomm.h"
#include "xfhdlcchpar.h"

/* ---------------------------------------------------------------------- */

static char *progname;

/* ---------------------------------------------------------------------- */

void cb_update(Fl_Widget *widget, void *udata)
{
	int ret;
	struct hdrvc_channel_params cp;

	cp.tx_delay = (int)(0.1*txdelay->value());
	cp.tx_tail = (int)(0.1*txtail->value());
	cp.slottime = (int)(0.1*slottime->value());
	cp.ppersist = (int)(ppersist->value());
	cp.fulldup = !!fulldup->value();
	ret = hdrvc_set_channel_access_param(cp);
	if (ret < 0) 
		perror("hdrvc_set_channel_access_param");
}

/* ---------------------------------------------------------------------- */

void cb_quit(Fl_Button *, long)
{
	exit(0);
}

/* ---------------------------------------------------------------------- */

static const char *usage_str = 
"[-i smif]\n"
"  -i: specify the name of the baycom kernel driver interface\n\n";

/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
        int c, i;
	int ret;
	struct hdrvc_channel_params cp;

	progname = *argv;
	printf("%s: Version 0.3; (C) 1996,1997,2000 by Thomas Sailer HB9JNX/AE4WA\n", progname);
	hdrvc_args(&argc, argv, "bc0");
	for (i = 1; i < argc; ) {
		c = i;
		Fl::arg(argc, argv, c);
		if (c <= 0) {
			i++;
			continue;
		}
		memmove(&argv[i], &argv[i+c], (argc-i) * sizeof(char *));
		argc -= c;
	}
#if 0
	while ((ret = getopt(argc, argv, "")) != -1) {
		switch (ret) {
		default:
			printf("usage: %s %s", progname, usage_str);
			exit(-1);
		}
	}
#endif
	hdrvc_init();

	create_the_forms();
	ifname->value(hdrvc_ifname());
	/*
	 * set channel params
	 */
	ret = hdrvc_get_channel_access_param(&cp);
	if (ret < 0) {
		perror("hdrvc_get_channel_access_param");
		exit(1);
	}
	txdelay->step(10);
	txdelay->bounds(0, 2550);
	txdelay->value(cp.tx_delay*10);
	txtail->step(10);
	txtail->bounds(0, 2550);
	txtail->value(cp.tx_tail*10);
	slottime->step(10);
	slottime->bounds(0, 2550);
	slottime->value(cp.slottime*10);
	ppersist->step(1);
	ppersist->bounds(0, 255);
	ppersist->value(cp.ppersist);
	fulldup->value(!!cp.fulldup);
	chpar->show();
	Fl::run();
	exit(0);
}
