/*
 * Copyright (c) 1996 Joerg Reuter (jreuter@poboxes.com)
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
 *
 */

 /* TODO: Should add partial path to ax25_route if we are one of the
  *       digipeaters.
  */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#include <sys/time.h>
#include <netinet/in.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <netax25/ax25.h>

#include "../pathnames.h"
#include "ax25rtd.h"

#include <stdlib.h>


/* FIXME */
static unsigned long get_from_arp(unsigned char *data, int size)
{
	if (size < 16)
		return 0;

	return ntohl(0);
}

static unsigned long get_from_ip(unsigned char *data, int size)
{
	unsigned long adr;

	if ((*data & 0xf) * 4 < IPLEN)
		return 0;

	adr = data[12] << 24;	/* NETWORK byte order */
	adr += data[13] << 16;
	adr += data[14] << 8;
	adr += data[15];

	return ntohl(adr);	/* HOST byte order */
}

int call_is_mycall(config * config, ax25_address * call)
{
	int k;

	for (k = 0; k < config->nmycalls; k++)
		if (!memcmp(call, &config->mycalls[k], AXLEN))
			return 1;
	return 0;
}

/*
 * This catches *all* invalid callsigns, unlike ax25validate.
 */

static int check_ax25_addr(unsigned char *buf)
{
	int k;
	unsigned char c;

	/* must start with at least one capital letter or digit */
	for (k = 0; k < 6; k++) {
		c = buf[k] >> 1;
		if (c == ' ')
			break;
		if ((c < 'A' || c > 'Z') && (c < '0' || c > '9'))
			return 1;
	}

	/* NULL address is invalid */
	if (k == 0)
		return 1;

	/* remaining fields must consist of spaces only */
	for (k++; k < 6; k++)
		if (buf[k] >> 1 != ' ')
			return 1;

	return 0;
}

static inline void invert_digipeater_path(ax25_address * digipeater,
					  int ndigi)
{
	int k, m;
	ax25_address fdigi;

	if (ndigi == 0)
		return;

	for (m = 0, k = ndigi - 1; k > m; k--, m++) {
		memcpy(&fdigi, &digipeater[m], AXLEN);
		memcpy(&digipeater[m], &digipeater[k], AXLEN);
		memcpy(&digipeater[k], &fdigi, AXLEN);
	}
}

int set_arp(config * config, long ip, ax25_address * call)
{
	struct sockaddr_in *isa;
	struct sockaddr_ax25 *asa;
	struct arpreq arp;
	int fds;

	if (!config->ip_add_arp)
		return 0;

	fds = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&arp, 0, sizeof(arp));

	isa = (struct sockaddr_in *) &arp.arp_pa;
	asa = (struct sockaddr_ax25 *) &arp.arp_ha;

	isa->sin_family = AF_INET;
	isa->sin_port = 0;
	isa->sin_addr.s_addr = ip;

	asa->sax25_family = AF_AX25;
	asa->sax25_ndigis = 0;
	asa->sax25_call = *call;

	arp.arp_flags = ATF_PERM | ATF_COM;
	strcpy(arp.arp_dev, config->dev);

	if (ioctl(fds, SIOCSARP, &arp) < 0) {
		invalidate_ip_route(ip);
		perror("routspy: SIOCSARP");
		close(fds);
		return 1;
	}
	close(fds);
	return 0;
}

/* dl9sau: use iproute2 for advanced routing. 
 * Anyone likes to implement this directly, without system()?
 */
#define	RT_DEL		0
#define	RT_ADD		1
int iproute2(long ip, char *dev, int what)
{
	char buffer[256];
	char ipa[32];
	int ret;

	sprintf(ipa, "%d.%d.%d.%d",
		(int) (ip & 0x000000FF),
		(int) ((ip & 0x0000FF00) >> 8),
                (int) ((ip & 0x00FF0000) >> 16),
		(int) ((ip & 0xFF000000) >> 24));
	
	/* ip rule add  table 44 */
	sprintf(buffer, "/sbin/ip route %s %s dev %s table %s proto ax25rtd", (what ? "add" : "del"), ipa, dev, iproute2_table);

	ret = system(buffer);
	return ret;
}

int set_route(config * config, long ip)
{
	struct rtentry rt;
	struct sockaddr_in *isa;
	char origdev[16], buf[1024];
/* modif f5lct */
	long gwr;
/* fin modif f5lct */
	long ipr;
	int fds;
	FILE *fp;

	fp = fopen(PROC_IP_ROUTE_FILE, "r");
	if (fp == NULL) {
		invalidate_ip_route(ip);
		return 1;
	}

	fgets(buf, sizeof(buf) - 1, fp);	/* discard header */
	while (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
/* modif f5lct */
		/*      sscanf(buf, "%s %lx", origdev, &ipr); */
		sscanf(buf, "%s %lx %lx", origdev, &ipr, &gwr);
		if (ipr == ip && gwr == 00000000)
/* fin modif f5lct */
		{
			if (dev_get_config(origdev) == NULL) {
				invalidate_ip_route(ip);
				fclose(fp);
				return 1;
			} else {
				del_kernel_ip_route(origdev, ip);
			}
		}

	}
	fclose(fp);

	if (!config->ip_add_route)
		return 0;

	if (*iproute2_table)
		return iproute2(ip, config->dev, RT_ADD);

	fds = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&rt, 0, sizeof(rt));

	isa = (struct sockaddr_in *) &rt.rt_dst;

	isa->sin_family = AF_INET;
	isa->sin_port = 0;
	isa->sin_addr.s_addr = ip;

	rt.rt_flags = RTF_UP | RTF_HOST;
	rt.rt_dev = config->dev;

	if (config->tcp_irtt != 0) {
		rt.rt_irtt = config->tcp_irtt;
		rt.rt_flags |= RTF_IRTT;
	}

	isa = (struct sockaddr_in *) &rt.rt_genmask;
	isa->sin_addr.s_addr = 0xffffffff;

	if (ioctl(fds, SIOCADDRT, &rt) < 0) {
		invalidate_ip_route(ip);
		perror("ax25rtd: IP SIOCADDRT");
		close(fds);
		return 1;
	}
	close(fds);

	return 0;
}

int del_kernel_ip_route(char *dev, long ip)
{
	int fds;
	struct rtentry rt;
	struct sockaddr_in *isa;
	config *config;

	config = dev_get_config(dev);
	if (config == NULL || !config->ip_add_route)
		return 0;

	if (*iproute2_table)
		return iproute2(ip, dev, RT_DEL);

	fds = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&rt, 0, sizeof(struct rtentry));

	isa = (struct sockaddr_in *) &rt.rt_dst;

	isa->sin_family = AF_INET;
	isa->sin_addr.s_addr = ip;

	rt.rt_flags = RTF_UP | RTF_HOST;
	rt.rt_dev = dev;

	if (ioctl(fds, SIOCDELRT, &rt) < 0) {
		perror("ax25rtd: IP SIOCDELRT");
		close(fds);
		return 1;
	}
	close(fds);

	return 0;
}

int set_ax25_route(config * config, ax25_rt_entry * rt)
{
	struct ax25_routes_struct ax25_route;
	int fds, k;

	if (!config->ax25_add_route)
		return 0;

	ax25_route.port_addr = config->mycalls[0];
	ax25_route.dest_addr = rt->call;
	ax25_route.digi_count = rt->ndigi;

	for (k = 0; k < rt->ndigi; k++)
		ax25_route.digi_addr[k] = rt->digipeater[k];

	fds = socket(AF_AX25, SOCK_SEQPACKET, 0);

	if (ioctl(fds, SIOCADDRT, &ax25_route) < 0) {
		perror("ax25rtd: AX.25 SIOCADDRT");
		close(fds);
		return 1;
	}

	close(fds);
	return 0;
}

int del_kernel_ax25_route(char *dev, ax25_address * call)
{
	struct ax25_routes_struct ax25_route;
	int fds;
	config *config;

	config = dev_get_config(dev);
	if (config == NULL || !config->ax25_add_route)
		return 0;

	ax25_route.port_addr = config->mycalls[0];
	ax25_route.dest_addr = *call;

	fds = socket(AF_AX25, SOCK_SEQPACKET, 0);

	if (ioctl(fds, SIOCDELRT, &ax25_route) < 0) {
		perror("ax25rtd: AX.25 SIOCDELRT");
		close(fds);
		return 1;
	}

	close(fds);
	return 0;
}

int set_ipmode(config * config, ax25_address * call, int ipmode)
{
	struct ax25_route_opt_struct ax25_opt;
	int fds;

	if (!config->ip_adjust_mode)
		return 0;

	ax25_opt.port_addr = config->mycalls[0];
	ax25_opt.dest_addr = *call;
	ax25_opt.cmd = AX25_SET_RT_IPMODE;
	ax25_opt.arg = ipmode ? 'V' : 'D';

	fds = socket(AF_AX25, SOCK_SEQPACKET, 0);

	if (ioctl(fds, SIOCAX25OPTRT, &ax25_opt) < 0) {
		perror("ax25rtd: SIOCAX25OPTRT");
		close(fds);
		return 1;
	}

	close(fds);
	return 0;

}

/* Yes, the code *IS* ugly... */

#define SKIP(o) {data+=(o); size-=(o);}
void ax25_receive(int sock)
{
	unsigned char buf[1500];
	unsigned char *data;
	unsigned long ip;
	struct sockaddr sa;
	ax25_address srccall, destcall, digipeater[AX25_MAX_DIGIS];
	char extseq = 0;
	int size, action, ipmode, ctl, pid, ndigi, kdigi, mine;
	time_t stamp;
	config *config;
	ax25_rt_entry *ax25rt;
	socklen_t asize;

	asize = sizeof(sa);
	if ((size = recvfrom(sock, buf, sizeof(buf), 0, &sa, &asize)) < 0) {
		perror("recvfrom");
		save_cache();
		daemon_shutdown(1);
	}

	stamp = time(NULL);
	ip = 0;
	pid = ctl = 0;

	config = dev_get_config(sa.sa_data);

	if (config == NULL)
		return;

	data = buf;

	/* 
	 * KISS data?
	 */

	if ((*data & 0x0f) != 0)
		return;

	SKIP(1);

	/* valid frame? */

	if (size < (2 * AXLEN + 1))
		return;

	/*
	 * Get destination callsign
	 */

	if (check_ax25_addr(data))
		return;

	memcpy(&destcall, data, AXLEN);
	destcall.ax25_call[6] &= 0x1e;
	SKIP(AXLEN);

	mine = call_is_mycall(config, &destcall);

	/*
	 * Get Source callsign
	 */

	if (check_ax25_addr(data))
		return;

	memcpy(&srccall, data, AXLEN);
	srccall.ax25_call[6] &= 0x1e;
	SKIP(ALEN);

	/* 
	 * How long is our control field?
	 */

	extseq = ~(*data) & SSSID_SPARE;

	/*
	 * Extract digipeaters
	 */

	ndigi = 0;
	while (((*data) & HDLCAEB) != HDLCAEB) {
		SKIP(1);
		if (size <= 0 || check_ax25_addr(data))
			return;

		if (ndigi < AX25_MAX_DIGIS)
			memcpy(&digipeater[ndigi++], data, AXLEN);
		else
			return;


		SKIP(ALEN);
	}

	SKIP(1);
	if (size <= 0)
		return;

	/*
	 * Get type of frame
	 */

	if ((*data & LAPB_S) == LAPB_I)
		ctl = LAPB_I;
	else {
		ctl = *data;
		if (extseq == 0)
			ctl &= ~LAPB_PF;
	}

	/* 
	 * Check if info frame and get PID
	 */

	if (ctl == LAPB_I || ctl == LAPB_UI) {
		SKIP(extseq ? 2 : 1);
		if (size <= 0)
			return;

		/* Get PID */

		pid = *data;

		if (pid == PID_SEGMENT) {
			SKIP(1);
			if (size <= 0)
				return;
			pid = 0;

			if (*data && SEG_FIRST) {
				pid = *data;
				SKIP(1);
				if (size <= 0)
					return;
			}
		}
	}

	/*
	 * See if it is fully digipeated (TODO: or if we are the next digipeater)
	 */

	for (kdigi = 0; kdigi < ndigi; kdigi++) {
		if ((digipeater[kdigi].ax25_call[6] & AX25_REPEATED) !=
		    AX25_REPEATED)
			return;

		digipeater[kdigi].ax25_call[6] &= 0x1e;
	}

	invert_digipeater_path(digipeater, ndigi);

	/*
	 * Are we allowed to add it to our routing table?
	 */

	if (mine || !config->ax25_for_me) {
		if (!mine && ndigi == 0 && config->ax25_add_default) {
			ndigi =
			    config->ax25_default_path.fsa_ax25.
			    sax25_ndigis;
			for (kdigi = 0; kdigi < ndigi; kdigi++)
				if (!memcmp
				    (&srccall,
				     &config->ax25_default_path.
				     fsa_digipeater[kdigi], AXLEN))
					break;

			if (ndigi == kdigi)
				memcpy(digipeater,
				       config->ax25_default_path.
				       fsa_digipeater, ndigi * AXLEN);
			else
				ndigi = 0;
		}

		ax25rt =
		    update_ax25_route(config, &srccall, ndigi, digipeater,
				      stamp);

		if (ax25rt != NULL)
			set_ax25_route(config, ax25rt);
	}

	/*
	 * Now see if it carries IP traffic
	 */

	switch (pid) {
	case PID_ARP:
		SKIP(1);
		if (size > 0)
			ip = get_from_arp(data, size);
		break;
	case PID_IP:
		if (!mine)
			return;

		SKIP(1);
		if (size > 0)
			ip = get_from_ip(data, size);
		break;
	default:
		return;
	}

	/*
	 * And adjust routes/arp/ipmode if we are allowed to...
	 */

	ipmode = (ctl == LAPB_I);

	if (ip != 0) {
		if (*ip_encaps_dev && (config = dev_get_config(ip_encaps_dev)) == NULL)
			return;

		action =
		    update_ip_route(config, ip, ipmode, &srccall, stamp);

		if (action & NEW_ROUTE)
			if (set_route(config, ip))
				return;

		if (action & NEW_ARP)
			if (set_arp(config, ip, &srccall))
				return;

		if (action & NEW_IPMODE)
			if (set_ipmode(config, &srccall, ipmode))
				return;
	}
}
