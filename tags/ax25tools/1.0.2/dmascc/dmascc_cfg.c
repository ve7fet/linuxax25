/*
 * $Id: dmascc_cfg.c,v 1.4 2009/05/27 14:26:06 dl9sau Exp $
 *
 * Configuration utility for dmascc driver
 * Copyright (C) 1997,2000 Klaus Kudielka
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#ifndef _SC_CLK_TCK
#include <sys/param.h>
#endif
#ifndef _SC_CLK_TCK
#include <asm/param.h>
#endif


/* Ioctls */
#define SIOCGSCCPARAM SIOCDEVPRIVATE
#define SIOCSSCCPARAM (SIOCDEVPRIVATE+1)

/* Frequency of timer 0 */
#define TMR_0_HZ      25600

/* Configurable parameters */
struct scc_param {
  int pclk_hz;    /* frequency of BRG input (don't change) */
  int brg_tc;     /* BRG terminal count; BRG disabled if < 0 */
  int nrzi;       /* 0 (nrz), 1 (nrzi) */
  int clocks;     /* see dmascc_cfg documentation */
  int txdelay;    /* [1/TMR_0_HZ] */
  int txtimeout;  /* [1/HZ] */
  int txtail;     /* [1/TMR_0_HZ] */
  int waittime;   /* [1/TMR_0_HZ] */
  int slottime;   /* [1/TMR_0_HZ] */
  int persist;    /* 1 ... 256 */
  int dma;        /* -1 (disable), 0, 1, 3 */
  int txpause;    /* [1/TMR_0_HZ] */
  int rtsoff;     /* [1/TMR_0_HZ] */
  int dcdon;      /* [1/TMR_0_HZ] */
  int dcdoff;     /* [1/TMR_0_HZ] */
  int reserved[35];
};

void usage(void)
{
  fprintf(stderr,
	  "usage:   dmascc_cfg <interface> [ options ... ]\n\n"
	  "options: --show         show updated configuration\n"
	  "         --frequency f  frequency of baud rate generator in Hz\n"
	  "         --nrzi n       NRZ (0) or NRZI (1) encoding\n"
	  "         --clocks n     clock mode (see manual page)\n"
	  "         --txdelay t    transmit delay in ms\n"
	  "         --txpause t    inter-packet delay in ms\n"
	  "         --txtimeout t  stop transmitting packets after t ms\n"
	  "         --txtail t     transmit tail in ms\n"
	  "         --rtsoff t     DCD settling time in ms (after RTS off)\n"
	  "         --dcdon t      DCD settling time in ms (after DCD on)\n"
	  "         --dcdoff t     DCD settling time in ms (after DCD off)\n"
	  "         --slottime t   slot time in ms\n"
	  "         --persist n    persistence parameter (1..256)\n"
	  "         --waittime t   wait time after transmit in ms\n"
	  "         --dma n        "
	  "DMA channel: -1 (no DMA), 0 (S5SCC/DMA only), 1, 3\n"
	  );
}

int main(int argc, char *argv[])
{
  int sk, show = 0, set = 0, old = 0, secondary;
  struct ifreq ifr;
  struct scc_param param;
  char **option, *end, *error = NULL;
  long hz = -1L;

  if (argc < 2) {
    usage();
    return 1;
  }

  if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    return 2;
  }

  memset(&param, 0, sizeof(param));
  param.txpause = -1;

  if (strncmp(argv[1], "dmascc", 6)) {
    fprintf(stderr, "invalid interface name.\n");
    return 5;
  }

  strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);
  ifr.ifr_data = (caddr_t) &param;
  if (ioctl(sk, SIOCGSCCPARAM, &ifr) < 0) {
    perror("ioctl");
    close(sk);
    return 3;
  }

  if (param.txpause == -1) {
    param.txpause = 0;
    old = 1;
  }

#ifdef	_SC_CLK_TCKxx
  hz = sysconf(_SC_CLK_TCK);
  if (hz == -1)
	perror("sysconf(_SC_CLK_TCK)");
#endif
  if (hz == -1) {
#ifdef	HZ
	hz = HZ;
#else
	hz = 100;
#endif
	fprintf(stderr, "warning: cannot dermine the clock rate HZ on which this system is running.\n");
	fprintf(stderr, "         Assuming %ld, what may be wrong.\n", hz);
  }

  secondary = argv[1][strlen(argv[1])-1]%2;

  option = argv + 2;
  while (!error && *option != NULL) {
    if (!strcmp(*option, "--show")) {
      show = 1;
      option++;
    } else if (!strcmp(*option, "--frequency")) {
      option++;
      if (*option != NULL) {
	double f;
	set = 1;
	f = strtod(*option++, &end);
	if (*end) error = "frequency not a number";
	else {
	  if (f < 0.0) error = "frequency < 0";
	  else if (f == 0.0) param.brg_tc = -1;
	  else {
	    param.brg_tc = param.pclk_hz / (f * 2) - 2;
	    if (param.brg_tc > 0xffff) error = "frequency too low";
	    if (param.brg_tc < 0) error = "frequency too high";
	  }
	}
      } else error = "--frequency requires parameter";
    } else if (!strcmp(*option, "--nrzi")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.nrzi = strtol(*option++, &end, 0);
	if (*end || param.nrzi < 0 || param.nrzi > 1)
	  error = "nrzi must be 0 or 1";
      } else error = "--nrzi requires parameter";
    } else if (!strcmp(*option, "--clocks")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.clocks = strtol(*option++, &end, 0);
	if (*end) error = "clock mode not a number";
	else if ((param.clocks & ~0x7f)) error = "invalid clock mode";
      } else error = "--clocks requires parameter";
    } else if (!strcmp(*option, "--txdelay")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.txdelay = TMR_0_HZ * strtod(*option++, &end) / 1000.0;
	if (*end) error = "txdelay not a number";
	else if (param.txdelay < 0) error = "txdelay < 0";
	else if (param.txdelay > 0xffff) error = "txdelay too large";
      } else error = "--txdelay requires parameter";
    } else if (!strcmp(*option, "--txpause")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.txpause = TMR_0_HZ * strtod(*option++, &end) / 1000.0;
	if (*end) error = "txpause not a number";
	else if (param.txpause < 0) error = "txpause < 0";
	else if (param.txpause > 0xffff) error = "txpause too large";
	if (old && param.txpause != 0)
	  fprintf(stderr, "warning: old driver; txpause not supported.\n");
      } else error = "--txpause requires parameter";
    } else if (!strcmp(*option, "--txtimeout")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.txtimeout = hz * strtod(*option++, &end) / 1000.0;
	if (*end) error = "txtimeout not a number";
	else if (param.txtimeout < 0) error = "txtimeout < 0";
      } else error = "--txtimeout requires parameter";
    } else if (!strcmp(*option, "--txtail")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.txtail = TMR_0_HZ * strtod(*option++, &end) / 1000.0;
	if (*end) error = "txtail not a number";
	else if (param.txtail < 0) error = "txtail < 0";
	else if (param.txtail > 0xffff) error = "txtail too large";
      } else error = "--txtail requires parameter";
    } else if (!strcmp(*option, "--rtsoff")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.rtsoff = TMR_0_HZ * strtod(*option++, &end) / 1000.0;
	if (*end) error = "rtsoff not a number";
	else if (param.rtsoff < 0) error = "rtsoff < 0";
	else if (param.rtsoff > 0xffff) error = "rtsoff too large";
	if (old && param.rtsoff != 0)
	  fprintf(stderr, "warning: old driver; rtsoff not supported.\n");
      } else error = "--rtsoff requires parameter";
    } else if (!strcmp(*option, "--dcdon")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.dcdon = TMR_0_HZ * strtod(*option++, &end) / 1000.0;
	if (*end) error = "dcdon not a number";
	else if (param.dcdon < 0) error = "dcdon < 0";
	else if (param.dcdon > 0xffff) error = "dcdon too large";
	if (old && param.dcdon != 0)
	  fprintf(stderr, "warning: old driver; dcdon not supported.\n");
      } else error = "--dcdon requires parameter";
    } else if (!strcmp(*option, "--dcdoff")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.dcdoff = TMR_0_HZ * strtod(*option++, &end) / 1000.0;
	if (*end) error = "dcdoff not a number";
	else if (param.dcdoff < 0) error = "dcdoff < 0";
	else if (param.dcdoff > 0xffff) error = "dcdoff too large";
	if (old && param.dcdoff != 0)
	  fprintf(stderr, "warning: old driver; dcdoff not supported.\n");
      } else error = "--dcdoff requires parameter";
    } else if (!strcmp(*option, "--slottime")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.slottime = TMR_0_HZ * strtod(*option++, &end) / 1000.0;
	if (*end) error = "slottime not a number";
	else if (param.slottime < 0) error = "slottime < 0";
	else if (param.slottime > 0xffff) error = "slottime too large";
      } else error = "--slottime requires parameter";
    } else if (!strcmp(*option, "--persist")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.persist = strtol(*option++, &end, 0);
	if (*end) error = "persist not a number";
	else if (param.persist < 1) error = "persist < 1";
	else if (param.persist > 256) error = "persist > 256";
      } else error = "--persist requires parameter";
    } else if (!strcmp(*option, "--waittime")) {
      option++;
      if (*option != NULL) {
	set = 1;
	param.waittime = TMR_0_HZ * strtod(*option++, &end) / 1000.0;
	if (*end) error = "waittime not a number";
	else if (param.waittime < 0) error = "waittime < 0";
	else if (param.waittime > 0xffff) error = "waittime too large";
      } else error = "--waittime requires parameter";
    } else if (!strcmp(*option, "--dma")) {
      option++;
      if (*option != NULL) {
	int dma = param.dma;
	set = 1;
	param.dma = strtol(*option++, &end, 0);
	if (*end) error = "DMA channel not a number";
	else if (secondary && param.dma != -1)
	  error = "SCC port B must have DMA disabled";
	else if (param.dma < -1 || param.dma == 2 || param.dma > 3)
	  error = "invalid DMA channel";
	else if (param.pclk_hz != 9830400 && param.dma == 0)
	  error = "only S5SCC/DMA supports DMA channel 0";
	else if (old && param.dma == 0)
	  error = "old driver; DMA channel 0 not supported";
	else if (old && param.dma == -1 && dma > 0)
	  error = "old driver; reload module or reboot to disable DMA";
	else if (old && param.dma == -1 && dma == 0)
	  param.dma = 0;
      } else error = "--dma requires parameter";
    } else error = "invalid option";
  }

  if (error) {
    fprintf(stderr, "usage error: %s.\n", error);
    close(sk);
    return 1;
  }

  if (set) {
    if (ioctl(sk, SIOCSSCCPARAM, &ifr) < 0) {
      perror("ioctl");
      close(sk);
      return 4;
    }
  }

  if (show) {
    double f;
    if (param.brg_tc < 0) f = 0.0;
    else f = ((double) param.pclk_hz) / ( 2 * (param.brg_tc + 2));
    printf("dmascc_cfg %s \\\n--frequency %.2f --nrzi %d --clocks 0x%02X "
	   "--txdelay %.2f \\\n--txpause %.2f --txtimeout %.2f "
	   "--txtail %.2f --rtsoff %.2f \\\n--dcdon %.2f --dcdoff %.2f "
	   "--slottime %.2f --persist %d \\\n--waittime %.2f --dma %d\n",
	   argv[1],
	   f,
	   param.nrzi,
	   param.clocks,
	   param.txdelay * 1000.0 / TMR_0_HZ,
	   param.txpause * 1000.0 / TMR_0_HZ,
	   param.txtimeout * 1000.0 / hz,
	   param.txtail * 1000.0 / TMR_0_HZ,
	   param.rtsoff * 1000.0 / TMR_0_HZ,
	   param.dcdon * 1000.0 / TMR_0_HZ,
	   param.dcdoff * 1000.0 / TMR_0_HZ,
	   param.slottime * 1000.0 / TMR_0_HZ,
	   param.persist,
	   param.waittime * 1000.0 / TMR_0_HZ,
	   (old && param.dma == 0) ? -1 : param.dma);
  }

  close(sk);
  return 0;
}
