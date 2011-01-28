/*****************************************************************************/

/*
 *	sethdlc.c  -- kernel HDLC radio modem driver setup utility.
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
 *   0.1  16.10.1996  Adapted from setbaycom.c and setsm.c
 *   0.2  20.11.1996  New mode set/query code
 *   0.5  11.05.1997  introduced hdrvcomm.h
 *   0.6  05.01.2000  glibc compile fixes
 */

/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include "hdrvcomm.h"

/* ---------------------------------------------------------------------- */

static unsigned char trace_stat = 0;
static char *progname;

/* ---------------------------------------------------------------------- */

static void display_packet(unsigned char *bp, unsigned int len) 
{
	unsigned char v1=1,cmd=0;
	unsigned char i,j;

	if (!bp || !len) 
		return;
	if (len < 8) 
		return;
	if (bp[1] & 1) {
		/*
		 * FlexNet Header Compression
		 */
		v1 = 0;
		cmd = (bp[1] & 2) != 0;
		printf("fm ? to ");
		i = (bp[2] >> 2) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = ((bp[2] << 4) | ((bp[3] >> 4) & 0xf)) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = ((bp[3] << 2) | ((bp[4] >> 6) & 3)) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = bp[4] & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = (bp[5] >> 2) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		i = ((bp[5] << 4) | ((bp[6] >> 4) & 0xf)) & 0x3f;
		if (i) 
			printf("%c",i+0x20);
		printf("-%u QSO Nr %u", bp[6] & 0xf, (bp[0] << 6) | 
		       (bp[1] >> 2));
		bp += 7;
		len -= 7;
	} else {
		/*
		 * normal header
		 */
		if (len < 15) 
			return;
		if ((bp[6] & 0x80) != (bp[13] & 0x80)) {
			v1 = 0;
			cmd = (bp[6] & 0x80);
		}
		printf("fm ");
		for(i = 7; i < 13; i++) 
			if ((bp[i] &0xfe) != 0x40) 
				printf("%c",bp[i] >> 1);
		printf("-%u to ",(bp[13] >> 1) & 0xf);
		for(i = 0; i < 6; i++) 
			if ((bp[i] &0xfe) != 0x40) 
				printf("%c", bp[i] >> 1);
		printf("-%u", (bp[6] >> 1) & 0xf);
		bp += 14;
		len -= 14;
		if ((!(bp[-1] & 1)) && (len >= 7)) 
			printf(" via ");
		while ((!(bp[-1] & 1)) && (len >= 7)) {
			for(i = 0; i < 6; i++) 
				if ((bp[i] &0xfe) != 0x40) 
					printf("%c",bp[i] >> 1);
			printf("-%u",(bp[6] >> 1) & 0xf);
			bp += 7;
			len -= 7;
			if ((!(bp[-1] & 1)) && (len >= 7)) 
				printf(",");
		}
	}
	if(!len) 
		return;
	i = *bp++;
	len--;
	j = v1 ? ((i & 0x10) ? '!' : ' ') : 
		((i & 0x10) ? (cmd ? '+' : '-') : (cmd ? '^' : 'v'));
	if (!(i & 1)) {
		/*
		 * Info frame
		 */
		printf(" I%u%u%c",(i >> 5) & 7,(i >> 1) & 7,j);
	} else if (i & 2) {
		/*
		 * U frame
		 */
		switch (i & (~0x10)) {
		case 0x03:
			printf(" UI%c",j);
			break;
		case 0x2f:
			printf(" SABM%c",j);
			break;
		case 0x43:
			printf(" DISC%c",j);
			break;
		case 0x0f:
			printf(" DM%c",j);
			break;
		case 0x63:
			printf(" UA%c",j);
			break;
		case 0x87:
			printf(" FRMR%c",j);
			break;
		default:
			printf(" unknown U (0x%x)%c",i & (~0x10),j);
			break;
		}
	} else {
		/*
		 * supervisory
		 */
		switch (i & 0xf) {
		case 0x1:
			printf(" RR%u%c",(i >> 5) & 7,j);
			break;
		case 0x5:
			printf(" RNR%u%c",(i >> 5) & 7,j);
			break;
		case 0x9:
			printf(" REJ%u%c",(i >> 5) & 7,j);
			break;
		default:
			printf(" unknown S (0x%x)%u%c", i & 0xf, 
			       (i >> 5) & 7, j);
			break;
		}
	}
	if (!len) {
		printf("\n");
		return;
	}
	printf(" pid=%02X\n", *bp++);
	len--;
	j = 0;
	while (len) {
		i = *bp++;
		if ((i >= 32) && (i < 128)) 
			printf("%c",i);
		else if (i == 13) {
			if (j) 
				printf("\n");
			j = 0;
		} else 
			printf(".");
		if (i >= 32) 
			j = 1;
		len--;
	}
	if (j) 
		printf("\n");
}

/* ---------------------------------------------------------------------- */

static void print_bits(int (*bitproc)(void))
{
	int ret;
	int i;
	char str[9];
	char *cp;
	
	for(;;) {
		ret = bitproc();
		if (ret < 0) {
			if (errno == EAGAIN)
				return;
			fprintf(stderr, "%s: Error %s (%i), cannot ioctl\n",
			        progname, strerror(errno), errno);
			return;
		}
		strcpy(cp = str, "00000000");
		for(i = 0; i < 8; i++, cp++, ret >>= 1) 
			*cp += (ret & 1);
		printf("%s", str);
	}
}

/* ---------------------------------------------------------------------- */

#ifdef HDRVC_KERNEL

static void do_set_params(int argc, char **argv) 
{
	struct hdlcdrv_ioctl drvname, curm, newm, mlist, curp, newp, mpmask;
	char set = 0;
	int ret;
	int mask;

	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_DRIVERNAME, &drvname);
	if (ret < 0) {
		perror("ioctl (HDLCDRVCTL_DRIVERNAME)");
		exit(1);
	}
	printf("driver name: %s\n", drvname.data.drivername);

	mask = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_MODEMPARMASK, &mpmask);
	if (mask < 0)
		mask = ~0;

	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_GETMODEMPAR, &curp);
	if (ret < 0) {
		perror("ioctl (HDLCDRVCTL_GETMODEMPAR)");
		exit(1);
	}
	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_GETMODE, &curm);
	if (ret < 0) {
		perror("ioctl (HDLCDRVCTL_GETMODE)");
		exit(1);
	}
	newm = curm;
	newp = curp;
	while (argc >= 2) {
		if (!strcasecmp(argv[0], "mode")) {
			strncpy(newm.data.modename, argv[1], 
				sizeof(newm.data.modename));
			set |= 1;
		} else if (!strcasecmp(argv[0], "io") && mask & HDLCDRV_PARMASK_IOBASE) {
			newp.data.mp.iobase = strtoul(argv[1], NULL, 0);
			set |= 2;
		} else if (!strcasecmp(argv[0], "irq") && mask & HDLCDRV_PARMASK_IRQ) {
			newp.data.mp.irq = strtoul(argv[1], NULL, 0);
			set |= 2;
		} else if (!strcasecmp(argv[0], "dma") && mask & HDLCDRV_PARMASK_DMA) {
			newp.data.mp.dma = strtoul(argv[1], NULL, 0);
			set |= 2;
		} else if (!strcasecmp(argv[0], "dma2") && mask & HDLCDRV_PARMASK_DMA2) {
			newp.data.mp.dma2 = strtoul(argv[1], NULL, 0);
			set |= 2;
		} else if (!strcasecmp(argv[0], "serio") && mask & HDLCDRV_PARMASK_SERIOBASE) {
			newp.data.mp.seriobase = strtol(argv[1], NULL, 0);
			set |= 2;
		} else if (!strcasecmp(argv[0], "pario") && mask & HDLCDRV_PARMASK_PARIOBASE) {
			newp.data.mp.pariobase = strtol(argv[1], NULL, 0);
			set |= 2;
		} else if (!strcasecmp(argv[0], "midiio") && mask & HDLCDRV_PARMASK_MIDIIOBASE) {
			newp.data.mp.midiiobase = strtol(argv[1], NULL, 0);
			set |= 2;       		
		} else {
			fprintf(stderr, "%s: invalid parameter type '%s', "
				"valid: mode%s%s%s%s%s%s%s\n",
				progname, argv[0], 
				(mask & HDLCDRV_PARMASK_IOBASE) ? " io" : "", 
				(mask & HDLCDRV_PARMASK_IRQ) ? " irq" : "", 
				(mask & HDLCDRV_PARMASK_DMA) ? " dma" : "", 
				(mask & HDLCDRV_PARMASK_DMA2) ? " dma2" : "", 
				(mask & HDLCDRV_PARMASK_SERIOBASE) ? " serio" : "", 
				(mask & HDLCDRV_PARMASK_PARIOBASE) ? " pario" : "", 
				(mask & HDLCDRV_PARMASK_MIDIIOBASE) ? " midiio" : "");
		}
		argv += 2;
		argc -= 2;
	}
	if (argc >= 1)
		fprintf(stderr, "%s: orphaned parameter type '%s', no data\n",
			progname, argv[0]);
	printf("current parameters: mode %s", curm.data.modename);
	if (mask & HDLCDRV_PARMASK_IOBASE)
		printf(" io 0x%x", curp.data.mp.iobase);
	if (mask & HDLCDRV_PARMASK_IRQ)
		printf(" irq %u", curp.data.mp.irq);
	if (mask & HDLCDRV_PARMASK_DMA)
		printf(" dma %u", curp.data.mp.dma);
	if (mask & HDLCDRV_PARMASK_DMA2)
		printf(" dma2 %u", curp.data.mp.dma2);
	if (mask & HDLCDRV_PARMASK_SERIOBASE)
		printf(" serio 0x%x", curp.data.mp.seriobase);
	if (mask & HDLCDRV_PARMASK_PARIOBASE)
		printf(" pario 0x%x", curp.data.mp.pariobase);
	if (mask & HDLCDRV_PARMASK_MIDIIOBASE)
		printf(" midiio 0x%x", curp.data.mp.midiiobase);
	printf("\n");

	if (set & 1) 
		ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_SETMODE, &newm);
		if (ret < 0) {
			perror("ioctl (HDLCDRVCTL_SETMODE)");
			ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_MODELIST, &mlist);
			if (ret < 0) {
				perror("ioctl (HDLCDRVCTL_MODELIST)");
				exit(1);
			}
			printf("driver supported modes: %s\n", 
			       mlist.data.modename);
			exit(1);
		}
	if (set & 2) {
		ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_SETMODEMPAR, &newp);
		if (ret < 0) {
			perror("ioctl (HDLCDRVCTL_SETMODEMPAR)");
			fprintf(stderr, "%s: trying to restore old "
				"parameters\n", progname);
			ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_SETMODEMPAR, &curp);
			if (ret < 0) 
				perror("ioctl (HDLCDRVCTL_SETMODEMPAR)");
			exit(1);
		}
	}
	if (set & 3) {
		printf("new parameters:     mode %s", newm.data.modename);
		if (mask & HDLCDRV_PARMASK_IOBASE)
			printf(" io 0x%x", newp.data.mp.iobase);
		if (mask & HDLCDRV_PARMASK_IRQ)
			printf(" irq %u", newp.data.mp.irq);
		if (mask & HDLCDRV_PARMASK_DMA)
			printf(" dma %u", newp.data.mp.dma);
		if (mask & HDLCDRV_PARMASK_DMA2)
			printf(" dma2 %u", newp.data.mp.dma2);
		if (mask & HDLCDRV_PARMASK_SERIOBASE)
			printf(" serio 0x%x", newp.data.mp.seriobase);
		if (mask & HDLCDRV_PARMASK_PARIOBASE)
			printf(" pario 0x%x", newp.data.mp.pariobase);
		if (mask & HDLCDRV_PARMASK_MIDIIOBASE)
			printf(" midiio 0x%x", newp.data.mp.midiiobase);
		printf("\n");
	}
	exit(0);
}

#endif /* HDRVC_KERNEL */

/* ---------------------------------------------------------------------- */

static void display_channel_params(const struct hdrvc_channel_params *par) 
{
	printf("TX delay %ums, TX tail %ums, slottime %ums, p-persistence "
	       " %u/256, %s duplex\n", 10*par->tx_delay, 10*par->tx_tail,
	       10*par->slottime, par->ppersist, 
	       par->fulldup ? "Full" : "Half");
}

/* ---------------------------------------------------------------------- */

static void do_set_channel_params(int argc, char **argv) 
{
	struct hdrvc_channel_params par1, par2;
	char set = 0;
	int ret;

	ret = hdrvc_get_channel_access_param(&par1);
	if (ret < 0) {
		perror("hdrvc_get_channel_access_param");
		exit(1);
	}
	par2 = par1;
	while (argc > 0) {
		if (argc >= 2 && !strcasecmp(argv[0], "txd")) {
			par2.tx_delay = strtoul(argv[1], NULL, 0) / 10;
			set = 1;
			argv += 2;
			argc -= 2;
		} else if (argc >= 2 && !strcasecmp(argv[0], "txtail")) {
			par2.tx_tail = strtoul(argv[1], NULL, 0) / 10;
			set = 1;
			argv += 2;
			argc -= 2;
		} else if (argc >= 2 && !strcasecmp(argv[0], "slot")) {
			par2.slottime = strtoul(argv[1], NULL, 0) / 10;
			set = 1;
			argv += 2;
			argc -= 2;
		} else if (argc >= 2 && !strcasecmp(argv[0], "ppersist")) {
			par2.ppersist = strtoul(argv[1], NULL, 0);
			set = 1;
			argv += 2;
			argc -= 2;
		} else if (!strcasecmp(argv[0], "half")) {
			par2.fulldup = 0;
			set = 1;
			argv += 1;
			argc -= 1;
		} else if (!strcasecmp(argv[0], "full")) {
			par2.fulldup = 1;
			set = 1;
			argv += 1;
			argc -= 1;
		} else {

			argv += 1;
			argc -= 1;
		}
	}
	printf("current parameters: ");
	display_channel_params(&par1);
	if (set) {
		ret = hdrvc_set_channel_access_param(par2);
		if (ret < 0) {
			perror("hdrvc_set_channel_access_param");
			exit(1);
		}
		printf("new parameters:     ");
		display_channel_params(&par2);
	}
	exit(0);
}

/* ---------------------------------------------------------------------- */

static const char *usage_str = 
"[-b] [-i] [-d] [-i <baycomif>] [-h] [-c <cal>]\n"
"[-p] [hw <hw>] [type <type>] [io <iobase>] [irq <irq>] [dma <dma>]\n"
"    [options <opt>] [serio <serio>] [pario <pario>] [midiio <midiio>]\n"
"[-a] [txd <txdelay>] [txtail <txtail>] [slot <slottime>]\n"
"    [ppersist <ppersistence>] [full] [half]\n"
"  -a: set or display channel access parameters\n" 
"  -b: trace demodulated bits\n"
"  -s: trace sampled input from tcm3105 (ser12 only)\n"
"  -d: trace dcd and ptt status on stdout\n"
"  -i: specify the name of the baycom kernel driver interface\n"
"  -c: calibrate (i.e. send calibration pattern) for cal seconds\n"
#ifdef HDRVC_KERNEL
"  -p: set or display interface parameters\n"
#endif /* HDRVC_KERNEL */
"  -h: this help\n\n";

int main(int argc, char *argv[])
{
	int ret;
	fd_set fds_read;
	fd_set fds_write;
	struct timeval tm;
	char getsetparams = 0;
	char cal = 0;
	int cal_val = 0;
	struct sm_ioctl bsi;
	struct baycom_ioctl bbi;
	struct hdrvc_channel_state chst;
	unsigned char pktbuf[2048];

	progname = *argv;
	printf("%s: Version 0.5; (C) 1996-1997 by Thomas Sailer HB9JNX/AE4WA\n", *argv);
	hdrvc_args(&argc, argv, "bc0");
	while ((ret = getopt(argc, argv, "bsdhpc:a")) != -1) {
		switch (ret) {
		case 'b':
			trace_stat = 2;
			break;
		case 's':
			trace_stat = 3;
			break;
		case 'd':
			trace_stat = 1;
			break;
#ifdef HDRVC_KERNEL
		case 'p':
			getsetparams = 1;
			break;
#endif /* HDRVC_KERNEL */
		case 'a':
			getsetparams = 2;
			break;
		case 'c':
			cal = 1;
			cal_val = strtoul(optarg, NULL, 0);
			break;
		default:
			printf("usage: %s %s", *argv, usage_str);
			exit(-1);
		}
	}
	hdrvc_init();
	if (getsetparams == 1) {
#ifdef HDRVC_KERNEL
		do_set_params(argc - optind, argv+optind);
#endif /* HDRVC_KERNEL */
		exit(0);
	}
	if (getsetparams == 2) {
		do_set_channel_params(argc - optind, argv+optind);
		exit(0);
	}
	if (cal) {
		ret = hdrvc_calibrate(cal_val);
		if (ret < 0) {
			perror("hdrvc_calibrate");
			exit(1);
		}
		fprintf(stdout, "%s: calibrating for %i seconds\n", *argv, 
			cal_val);
		exit(0);
	}
	for (;;) {
		while ((ret = hdrvc_recvpacket(pktbuf, sizeof(pktbuf))) > 1)
			display_packet(pktbuf+1, ret-1);
		switch (trace_stat) {
		case 1:
			ret = hdrvc_get_channel_state(&chst);
			if (ret < 0)
				perror("hdrvc_get_channel_state");
			printf("%c%c rx: %lu  tx: %lu  rxerr: %lu  txerr: %lu", 
			       chst.dcd ? 'D' : '-', chst.ptt ? 'P' : '-',
			       chst.rx_packets, chst.tx_packets, chst.rx_errors, 
			       chst.tx_errors);
#ifdef HDRVC_KERNEL
			if (hdrvc_sm_ioctl(SMCTL_GETDEBUG, &bsi) >= 0) {
				printf("   intrate: %u  modcyc: %u  "
				       "demodcyc: %u  dmares: %u", 
				       bsi.data.dbg.int_rate, 
				       bsi.data.dbg.mod_cycles, 
				       bsi.data.dbg.demod_cycles,
				       bsi.data.dbg.dma_residue);
			} 
			if (hdrvc_baycom_ioctl(BAYCOMCTL_GETDEBUG, &bbi) >= 0) {
				printf("   dbg1: %lu  dbg2: %lu  dbg3: %li", 
				       bbi.data.dbg.debug1, bbi.data.dbg.debug2, 
				       bbi.data.dbg.debug3);
			} 
#endif /* HDRVC_KERNEL */
			printf("\n");
			break;
		case 2:
			print_bits(hdrvc_get_bits);
			printf("\n");
			break;
		case 3:
			print_bits(hdrvc_get_samples);
			printf("\n");
			break;
		default:
			break;
		}
		tm.tv_usec = 500000L;
		tm.tv_sec = 0L;
		if (hdrvc_getfd() >= 0) {
			FD_ZERO(&fds_read);
			FD_ZERO(&fds_write);
			FD_SET(hdrvc_getfd(), &fds_read);
			ret = select(hdrvc_getfd()+1, &fds_read, &fds_write, NULL, &tm);
		} else 
			ret = select(0, NULL, NULL, NULL, &tm);
		if (ret < 0) {
			fprintf(stderr, "%s: Error %s (%i) in select\n", *argv,
				strerror(errno), errno);
			exit(-2);
		}
	}
}

/* ---------------------------------------------------------------------- */
