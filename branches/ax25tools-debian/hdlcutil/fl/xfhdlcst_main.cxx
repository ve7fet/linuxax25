/*****************************************************************************/

/*
 *	xfhdlcst_main.C  -- kernel hdlc radio modem driver status display utility.
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
#include "xfhdlcst.h"

/* ---------------------------------------------------------------------- */

static char *progname;
static bool update = true;

/* ---------------------------------------------------------------------- */

void cb_quit(Fl_Button *, long)
{
	exit(0);
}

/* ---------------------------------------------------------------------- */

void cb_timer(void *)
{
	update = true;
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
	struct hdrvc_channel_state cs;
#ifdef HDRVC_KERNEL
	struct sm_ioctl smi;
#endif /* HDRVC_KERNEL */
	char buf[32];
	char name[64];

	progname = *argv;
	printf("%s: Version 0.3; (C) 1996,1997,2000 by Thomas Sailer HB9JNX/AE4WA\n", *argv);
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
			printf("usage: %s %s", *argv, usage_str);
			exit(-1);
		}
	}
#endif
	hdrvc_init();

	create_the_forms();
	Fl::add_timeout(0.1, cb_timer);
	/*
	 * set driver and modem name
	 */
	ret = hdrvc_get_mode_name(name, sizeof(name));
	if (ret < 0) {
		perror("hdrvc_get_mode_name");
		modename->hide();
	} else 
		modename->value(name);
	ret = hdrvc_get_driver_name(name, sizeof(name));
	if (ret < 0) {
		perror("hdrvc_get_driver_name");
		drivername->hide();
	} else 
		drivername->value(name);
	/*
	 * check for soundmodem driver
	 */
#ifdef HDRVC_KERNEL
	ret = hdrvc_sm_ioctl(SMCTL_GETDEBUG, &smi);
	if (ret < 0) {
#endif /* HDRVC_KERNEL */
		tdemodcyc->hide();
		tmodcyc->hide();
		tintfreq->hide();
		tdmares->hide();
		demodcyc->hide();
		modcyc->hide();
		intfreq->hide();
		dmares->hide();
#ifdef HDRVC_KERNEL
	}
#endif /* HDRVC_KERNEL */
	hdlcst->show();
	for (;;) {
		Fl::wait();
		if (!update)
			continue;
		update = false;
		Fl::add_timeout(0.5, cb_timer);
		/*
		 * display state
		 */
		ret = hdrvc_get_channel_state(&cs);
		if (ret >= 0) {
			ptt->value(cs.ptt);
			dcd->value(cs.dcd);
			sprintf(buf, "%ld", cs.tx_packets);
			txpacket->value(buf);
			sprintf(buf, "%ld", cs.tx_errors);
			txerror->value(buf);
			sprintf(buf, "%ld", cs.rx_packets);
			rxpacket->value(buf);
			sprintf(buf, "%ld", cs.rx_errors);
			rxerror->value(buf);
		}
		ret = hdrvc_sm_ioctl(SMCTL_GETDEBUG, &smi);
		if (ret >= 0) {
			sprintf(buf, "%d", smi.data.dbg.int_rate);
			intfreq->value(buf);
			sprintf(buf, "%d", smi.data.dbg.mod_cycles);
			modcyc->value(buf);
			sprintf(buf, "%d", smi.data.dbg.demod_cycles);
			demodcyc->value(buf);
			sprintf(buf, "%d", smi.data.dbg.dma_residue);
			dmares->value(buf);
		} 
	}
	exit (0);
}
