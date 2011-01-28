/*****************************************************************************/

/*
 *    yaminit.c  -- YAM radio modem driver setup utility.
 *
 *  Copyright (C) 1998  Jean-Paul ROUBELAT (jpr@f6fbb.org)
 *  Adapted from sethdlc.c written by Thomas Sailer (t.sailer@alumni.ethz.ch)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 *
 * History:
 *   0.0  29.07.98  First draft
 */

/*****************************************************************************/

/* ---------------------------------------------------------------------- */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#ifdef __GLIBC__
#include <netinet/if_ether.h>
#else
#include <linux/if_ether.h>
#endif
#include <endian.h>
#include <netinet/in.h>

#include "yam.h"

static unsigned char bitswap (unsigned char c)
{
	unsigned char r = 0;
	int i;

	for (i = 0; i < 8; i++)
	{
		r <<= 1;
		if (c & 1)
			r |= 1;
		c >>= 1;
	}
	return r;
}

static int in2hex (char *ptr)
{
	char str[3];
	int val;

	memcpy (str, ptr, 2);
	str[2] = '\0';

	sscanf (str, "%x", &val);

	return val;
}

static int set_mcs (char *name, struct ifreq *ifr, int sock, int bitrate, char *filename)
{
	struct yamdrv_ioctl_mcs yammcs;
	FILE *fptr;
	int nb, i, type, pos;
	char buf[256];

	yammcs.bitrate = bitrate;

	fptr = fopen (filename, "r");
	if (fptr == NULL)
	{
		fprintf (stderr, "%s: file %s not found\n", name, filename);
		return -1;
	}

	pos = 0;

	while (fgets (buf, sizeof (buf), fptr))
	{
		nb = in2hex (buf + 1);
		type = in2hex (buf + 7);

		if (type != 0)
			continue;

		for (i = 0; i < nb; i++)
		{
			if (pos == YAM_FPGA_SIZE)
			{
				fclose (fptr);
				printf ("fpga bytes are more than %d in %s mcs file\n", YAM_FPGA_SIZE, filename);
				return -1;
			}
			yammcs.bits[pos++] = bitswap (in2hex (buf + 9 + i * 2));
		}
	}

	fclose (fptr);

	if (pos != YAM_FPGA_SIZE)
	{
		printf ("fpga bytes are not %d in %s mcs file\n", YAM_FPGA_SIZE, filename);
		return -1;
	}

	ifr->ifr_data = (caddr_t) & yammcs;
	yammcs.cmd = SIOCYAMSMCS;

	if (ioctl (sock, SIOCDEVPRIVATE, ifr) == -1)
	{
		fprintf (stderr, "%s: Error %s (%i), cannot ioctl SIOCYAMSMCS on %s\n",
				 name, strerror (errno), errno, ifr->ifr_name);
		return -1;
	}

	printf ("mcs file %s loaded for bitrate %d\n\n", filename, yammcs.bitrate);

	return 0;
}

static int set_params (char *name, struct ifreq *ifr, int sock, int ac, char **av, unsigned *pmask)
{
	int len;
	int bitrate = 0;
	char *filename = NULL;
	struct yamdrv_ioctl_cfg yamctl;

	printf ("\n");

	yamctl.cfg.mask = 0;

	while (ac >= 2)
	{
		len = strlen (av[0]);
		if (!strncasecmp (av[0], "iobase", len))
		{
			yamctl.cfg.iobase = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_IOBASE;
		}
		else if (!strncasecmp (av[0], "irq", len))
		{
			yamctl.cfg.irq = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_IRQ;
		}
		else if (!strncasecmp (av[0], "bitrate", len))
		{
			yamctl.cfg.bitrate = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_BITRATE;
		}
		else if (!strncasecmp (av[0], "baudrate", len))
		{
			yamctl.cfg.baudrate = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_BAUDRATE;
		}
		else if (!strncasecmp (av[0], "duplex", len))
		{
			yamctl.cfg.mode = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_MODE;
		}
		else if (!strncasecmp (av[0], "hold", len))
		{
			yamctl.cfg.holddly = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_HOLDDLY;
		}
		else if (!strncasecmp (av[0], "txdelay", len))
		{
			yamctl.cfg.txdelay = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_TXDELAY;
		}
		else if (!strncasecmp (av[0], "txtail", len))
		{
			yamctl.cfg.txtail = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_TXTAIL;
		}
		else if (!strncasecmp (av[0], "slottime", len))
		{
			yamctl.cfg.slottime = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_SLOTTIME;
		}
		else if (!strncasecmp (av[0], "persist", len))
		{
			yamctl.cfg.persist = strtoul (av[1], NULL, 0);
			yamctl.cfg.mask |= YAM_PERSIST;
		}
		else if (!strncasecmp (av[0], "load", len))
		{
			if (ac < 3)
				break;
			bitrate = strtoul (av[1], NULL, 0);
			filename = av[2];
			av += 1;
			ac -= 1;
		}
		else
		{
			fprintf (stderr, "%s: invalid parameter type '%s'\n",
					 name, av[0]);
		}
		av += 2;
		ac -= 2;
	}
	if (ac > 0)
		fprintf (stderr, "%s: orphaned parameter type '%s', no data\n",
				 name, av[0]);

	if (yamctl.cfg.mask)
	{
		ifr->ifr_data = (caddr_t) & yamctl;
		yamctl.cmd = SIOCYAMSCFG;
		*pmask = yamctl.cfg.mask;

		if (ioctl (sock, SIOCDEVPRIVATE, ifr) == -1)
		{
			fprintf (stderr, "%s: Error %s (%i), cannot ioctl SIOCYAMSCFG on %s\n",
					 name, strerror (errno), errno, ifr->ifr_name);
			return -1;
		}
	}

	if (filename)
		return set_mcs (name, ifr, sock, bitrate, filename);

	return 0;
}

#define is_mask(m, n) (m & n) ? '*' : ' '
static void get_params (char *name, struct ifreq *ifr, int sock, unsigned mask)
{
	char *dup;
	struct yamdrv_ioctl_cfg yamctl;

	ifr->ifr_data = (caddr_t) & yamctl;
	yamctl.cmd = SIOCYAMGCFG;

	if (ioctl (sock, SIOCDEVPRIVATE, ifr) == -1)
	{
		fprintf (stderr, "%s: Error %s (%i), cannot ioctl SIOCYAMGCFG on %s\n",
				 name, strerror (errno), errno, ifr->ifr_name);
		return;
	}

	switch (yamctl.cfg.mode)
	{
	case 0:
		dup = "off";
		break;
	case 1:
		dup = "on";
		break;
	case 2:
		dup = "on+delay";
		break;
	default:
		dup = "";
		break;
	}

	printf ("Device    : %s\n", ifr->ifr_name);
	printf ("iobase    :%c%04x\n", is_mask (mask, YAM_IOBASE), yamctl.cfg.iobase);
	printf ("bitrate   :%c%u (bps)\n", is_mask (mask, YAM_BITRATE), yamctl.cfg.bitrate);
	printf ("irq       :%c%u\n", is_mask (mask, YAM_IRQ), yamctl.cfg.irq);
	printf ("baudrate  :%c%u (bps)\n", is_mask (mask, YAM_BAUDRATE), yamctl.cfg.baudrate);
	printf ("fulldup   :%c%s\n", is_mask (mask, YAM_MODE), dup);
	if (yamctl.cfg.mode == 2)
		printf ("holddelay :%c%-3u (sec)\n", is_mask (mask, YAM_HOLDDLY), yamctl.cfg.holddly);
	printf ("txdelay   :%c%-3u (ms)\n", is_mask (mask, YAM_TXDELAY), yamctl.cfg.txdelay);
	printf ("txtail    :%c%-3u (ms)\n", is_mask (mask, YAM_TXTAIL), yamctl.cfg.txtail);
	printf ("slottime  :%c%-3u (ms)\n", is_mask (mask, YAM_SLOTTIME), yamctl.cfg.slottime);
	printf ("persist   :%c%u\n", is_mask (mask, YAM_PERSIST), yamctl.cfg.persist);
	printf ("\n");
}

static const char *usage_str =
"[-h] yamif\n"
"  [iobase <iobase>] [irq <irq>] [bitrate <bitrate>] [baudrate <baudrate>]\n"
"  [duplex <mode>] [hold <delay>] [txdelay <txdelay>]\n"
"  [txtail <txtail>] [slottime <slottime>] [persist <persistence>]\n"
"  [load <bitrate> <filename.mcs>]\n"
"  yamif: the name of the yam kernel driver interface (yam0)\n"
"  -h: this help\n\n";

int main (int argc, char *argv[])
{
	char name[256] = "yam0";
	struct ifreq ifr;
	int sock;
	unsigned int mask;

	printf ("\n%s: Version 0.1 (C) 1998 by Jean-Paul ROUBELAT - F6FBB\n", *argv);

	if ((argc < 2) || (strcasecmp (argv[1], "-h") == 0))
	{
		printf ("usage: %s %s", *argv, usage_str);
		return 1;
	}

	strcpy (name, argv[1]);

	if ((sock = socket (PF_INET, SOCK_PACKET, htons (ETH_P_AX25))) < 0)
	{
		fprintf (stderr, "%s: Error %s (%i), cannot open %s\n",
				 argv[0], strerror (errno), errno, name);
		return 2;
	}

	strcpy (ifr.ifr_name, name);
	if (ioctl (sock, SIOCGIFFLAGS, &ifr) < 0)
	{
		fprintf (stderr, "%s: Error %s (%i), cannot ioctl %s\n",
				 argv[0], strerror (errno), errno, name);
		close (sock);
		return 3;
	}

	mask = 0;

	if (set_params (argv[0], &ifr, sock, argc - 2, argv + 2, &mask) == -1)
	{
		close (sock);
		return 4;
	}

	get_params (argv[0], &ifr, sock, mask);

	close (sock);

	return 0;
}
