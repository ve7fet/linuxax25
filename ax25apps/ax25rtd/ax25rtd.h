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
 
/* Defines for defaults */
 
#define IP_MAXROUTES	4096
#define AX25_MAXROUTES	4096
#define AX25_MAXCALLS	32

/* Some AX.25 stuff */

#define NEW_ARP		1
#define NEW_ROUTE	2
#define NEW_IPMODE	4
  
#define SEG_FIRST       0x80
#define SEG_REM         0x7F

#define PID_SEGMENT     0x08
#define PID_ARP         0xCD
#define PID_IP          0xCC
#define PID_NETROM	0xCF
  
#define HDLCAEB         0x01
#define SSSID_SPARE     0x40
#define AX25_REPEATED	0x80

#define LAPB_I          0x00
#define LAPB_S          0x01
#define LAPB_UI		0x03
#define LAPB_PF         0x10

#define ALEN		6
#define AXLEN		7
#define IPLEN		20


/* structs for the caches */

typedef struct ip_rt_entry_ {
	struct ip_rt_entry_	*next, *prev;
	unsigned long		ip;
	unsigned char		iface[14];
	ax25_address		call;
	char			ipmode;
	time_t			timestamp;
	char			invalid;
} ip_rt_entry;

typedef struct ax25_rt_entry_ {
	struct ax25_rt_entry_	*next, *prev;
	unsigned char		iface[14];
	ax25_address		call;
	ax25_address		digipeater[AX25_MAX_DIGIS];
	int			ndigi;
	long			cnt;
	time_t			timestamp;
} ax25_rt_entry;

/* struct for the channel configuration */

typedef struct config_ {
	struct config_ *next;
	char port[128];
	char dev[14];

	char ax25_add_route;
	char ax25_for_me;
	char ax25_add_default;

	char ip_add_route;
	char ip_add_arp;
	char ip_adjust_mode;
	char ip_arp_use_netlink;

	unsigned int  dg_mtu;
	unsigned int  vc_mtu;
	unsigned long tcp_irtt;

	unsigned long netmask;
	unsigned long ip;
	
	int nmycalls;
	ax25_address mycalls[AX25_MAXCALLS];
	
	struct full_sockaddr_ax25 ax25_default_path;
} config;

/* global variables */

extern const char * Version;

extern int reload;

extern config *Config;

extern ip_rt_entry * ip_routes;
extern int ip_routes_cnt;
extern int ip_maxroutes;

extern ax25_rt_entry * ax25_routes;
extern int ax25_routes_cnt;
extern int ax25_maxroutes;

extern char ip_encaps_dev[];
extern char iproute2_table[];

/* config.c */

void load_config(void);
void reload_config(void);
void load_cache(void);
void save_cache(void);
void interpret_command(int fd, unsigned char *buf);

/* listener.c */

int call_is_mycall(config *config, ax25_address *call);
int set_arp(config *config, long ip, ax25_address *call);
int set_route(config *config, long ip);
int set_ax25_route(config *config, ax25_rt_entry *rt);
int set_ipmode(config *config, ax25_address *call, int ipmode);
int del_kernel_ip_route(char *dev, long ip);
int del_kernel_ax25_route(char *dev, ax25_address *call);
void ax25_receive(int sock);

/* ax25rtd.c */

void daemon_shutdown(int reason);
int set_ipmode(config *config, ax25_address *call, int ipmode);
config * dev_get_config(char *dev);
config * port_get_config(char *port);

/* cache_dump.c */

void dump_ip_routes(int fd, int cmd);
void dump_ax25_routes(int fd, int cmd);
void dump_config(int fd);

/* cache_ctl.c */

int update_ip_route(config *config, unsigned long ip, int ipmode, ax25_address *call, time_t timestamp);
ax25_rt_entry * update_ax25_route(config *config, ax25_address *call, int ndigi, ax25_address *digi, time_t timestamp);
int del_ip_route(unsigned long ip);
int invalidate_ip_route(unsigned long ip);
int del_ax25_route(config * config, ax25_address *call);
void expire_ax25_route(time_t when);
void expire_ip_route(time_t when);
