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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#ifdef __GLIBC__
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif
#include <net/if_arp.h>

#include <config.h>
#include <netax25/ax25.h>
#include <netrose/rose.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include "../pathnames.h"
#include "ax25rtd.h"

ax25_address *asc2ax(char *call)
{
	static ax25_address callsign;

	ax25_aton_entry(call, (char *) &callsign);

	return &callsign;
}

static long asc2ip(char *s)
{
	struct in_addr addr;

	if (!inet_aton(s, &addr))
		return 0;

	return addr.s_addr;
}


char *prepare_cmdline(char *buf)
{
	char *p;
	for (p = buf; *p; p++) {
		if (*p == '\t')
			*p = ' ';
		*p = tolower(*p);

		if (*p == '\n') {
			*p = '\0';
			break;
		}

		if (*p == '#') {
			*p = '\0';
			break;
		}
	}

	return buf;
}

char *get_next_arg(char **p)
{
	char *p2;

	if (p == NULL || *p == NULL)
		return NULL;

	p2 = *p;
	for (; *p2 && *p2 == ' '; p2++);
	if (!*p2)
		return NULL;

	*p = strchr(p2, ' ');
	if (*p != NULL) {
		**p = '\0';
		(*p)++;
	}

	return p2;
}

static inline void invalid_arg(char *c, char *p)
{
	fprintf(stderr, "%s: invalid argument %s\n", c, p);
}

static inline void missing_arg(char *c)
{
	fprintf(stderr, "%s: argument missing\n", c);
}

static int yesno(char *arg)
{
	if (!arg)
		return 0;
	if (!strcmp(arg, "yes") || !strcmp(arg, "1"))
		return 1;
	if (!strcmp(arg, "no") || !strcmp(arg, "0"))
		return 0;
	return -1;
}

static ax25_address *get_mycall(char *port)
{
	char *addr;

	if ((addr = ax25_config_get_addr(port)) == NULL)
		return NULL;

	return asc2ax(addr);
}


void load_ports(void)
{
	config *config, *cfg, *ncfg;
	char buf[1024];
	struct ifconf ifc;
	struct ifreq ifr, *ifrp;
	int k, fd;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "Unable to open socket\n");
		exit(1);
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
		fprintf(stderr, "SIOCGIFCONF: %s\n", strerror(errno));
		return;
	}

	ifrp = ifc.ifc_req;
	for (k = ifc.ifc_len / sizeof(struct ifreq); k > 0; k--, ifrp++) {
		strcpy(ifr.ifr_name, ifrp->ifr_name);
		if (strcmp(ifr.ifr_name, "lo") == 0)
			continue;

		if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
			fprintf(stderr, "SIOCGIFFLAGS: %s\n",
				strerror(errno));
			exit(1);
		}

		if (!(ifr.ifr_flags & IFF_UP))
			continue;

		ioctl(fd, SIOCGIFHWADDR, &ifr);

		if (ifr.ifr_hwaddr.sa_family != ARPHRD_AX25)
			continue;

		for (config = Config; config; config = config->next)
			if (!memcmp
			    (&config->mycalls[0], ifr.ifr_hwaddr.sa_data,
			     AXLEN) && !*config->dev) {
				strcpy(config->dev, ifr.ifr_name);
				ioctl(fd, SIOCGIFADDR, &ifr);
				config->ip =
				    ((struct sockaddr_in *) &ifr.
				     ifr_addr)->sin_addr.s_addr;
				strcpy(ifr.ifr_name, config->dev);
				ioctl(fd, SIOCGIFNETMASK, &ifr);
				config->netmask =
				    ((struct sockaddr_in *) &ifr.
				     ifr_netmask)->sin_addr.s_addr;
				break;
			}
	}
	close(fd);

	config = cfg = Config;

	while (config) {
		if (!*config->dev) {
			if (config == Config) {
				Config = config->next;
				cfg = Config;
			} else
				cfg->next = config->next;
			ncfg = config->next;
			free(config);
			config = ncfg;
		} else {
			cfg = config;
			config = config->next;
		}
	}
}

void load_listeners(void)
{
	config *config;
	char buf[1024], device[14], call[10], dcall[10];
	char dummy[1024];
	int k;
	FILE *fp;
	ax25_address *axcall;


	fp = fopen(PROC_AX25_FILE, "r");

	if (fp == NULL) {
		fprintf(stderr, "No AX.25 in kernel. Tss, tss...\n");
		exit(1);
	}

	while (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
		k = sscanf(buf, "%s %s %s %s", dummy, device, call, dcall);
		if (k == 4 && !strcmp(dcall, "*")) {
			axcall = asc2ax(call);
			if (!strcmp("*", device)) {
				for (config = Config; config;
				     config = config->next) {
					if (call_is_mycall(config, axcall)
					    || config->nmycalls >
					    AX25_MAXCALLS)
						continue;
					memcpy(&config->
					       mycalls[config->nmycalls++],
					       axcall, AXLEN);
				}
			} else {
				config = dev_get_config(device);
				if (config == NULL
				    || call_is_mycall(config, axcall)
				    || config->nmycalls > AX25_MAXCALLS)
					continue;

				memcpy(&config->
				       mycalls[config->nmycalls++], axcall,
				       AXLEN);
			}
		}
	}
	fclose(fp);
}

void load_config(void)
{
	FILE *fp;
	char buf[1024], *p, *cmd, *arg;
	config *config, *cfg;
	ax25_address *axcall;

	config = NULL;

	fp = fopen(CONF_AX25ROUTED_FILE, "r");

	if (fp == NULL) {
		fprintf(stderr, "config file %s not found\n",
			CONF_AX25ROUTED_FILE);
		exit(1);
	}

	while (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
		p = prepare_cmdline(buf);
		if (!*p)
			continue;

		cmd = get_next_arg(&p);
		if (cmd == NULL)
			continue;
		arg = get_next_arg(&p);

		if (*cmd == '[') {
			cmd++;
			p = strrchr(cmd, ']');
			if (p == NULL) {
				fprintf(stderr, "syntax error: [%s\n",
					cmd);
				continue;
			}
			*p = '\0';

			axcall = get_mycall(cmd);
			if (axcall == NULL)
				continue;

			cfg =
			    (struct config_ *)
			    malloc(sizeof(struct config_));
			if (cfg == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(1);
			}
			memset(cfg, 0, sizeof(struct config_));

			if (config)
				config->next = cfg;
			else
				Config = cfg;

			cfg->next = NULL;
			config = cfg;
			strcpy(config->port, cmd);
			memcpy(&config->mycalls[0], axcall, AXLEN);
			config->nmycalls = 1;
		} else if (config && !strcmp(cmd, "ax25-learn-routes")) {
			/* ax25-learn-routes no|yes: learn digipeater path */
			if (arg) {
				int k = yesno(arg);

				if (k == -1) {
					invalid_arg(cmd, arg);
					continue;
				} else {
					config->ax25_add_route = k;
				}
			} else
				missing_arg(cmd);
		} else if (config && !strcmp(cmd, "ax25-learn-only-mine")) {
			/* ax25-learn-only-mine no|yes: learn only if addressed to me */
			if (arg) {
				int k = yesno(arg);

				if (k == -1) {
					invalid_arg(cmd, arg);
					continue;
				} else {
					config->ax25_for_me = k;
				}
			} else
				missing_arg(cmd);
		} else if (config && !strcmp(cmd, "ax25-add-path")) {
			/* ax25-add-path [no|<digis>]: add digis if digi-less frame rvd */
			int k = 0;
			if (!arg || (arg && yesno(arg) == 0))
				continue;

			config->ax25_add_default = 1;
			while (arg && k < AX25_MAX_DIGIS) {
				memcpy(&config->ax25_default_path.
				       fsa_digipeater[k], asc2ax(arg),
				       AXLEN);
				arg = get_next_arg(&p);
				k++;
			}
			config->ax25_default_path.fsa_ax25.sax25_ndigis =
			    k;
		} else if (config && !strcmp(cmd, "ax25-more-mycalls")) {
			/* ax25-more-mycalls call1 call2: frames to this calls are for "me", too. */
			if (arg) {
				while (arg
				       && config->nmycalls <
				       AX25_MAXCALLS) {
					axcall = asc2ax(arg);
					if (call_is_mycall(config, axcall)) {
						arg = get_next_arg(&p);
						continue;
					}
					memcpy(&config->
					       mycalls[config->nmycalls++],
					       axcall, AXLEN);
					arg = get_next_arg(&p);
				}
			} else
				missing_arg(cmd);
		} else if (config && !strcmp(cmd, "ip-learn-routes")) {
			/* ip-learn-routes no|yes: learn ip routes */
			if (arg) {
				int k = yesno(arg);

				if (k == -1) {
					invalid_arg(cmd, arg);
					continue;
				} else {
					config->ip_add_route = k;
				}
			} else
				missing_arg(cmd);
		} else if (config && !strcmp(cmd, "irtt")) {
			/* irtt <irtt>: new routes will get this IRTT in msec */
			if (arg) {
				int k = atoi(arg);

				if (k == 0) {
					invalid_arg(cmd, arg);
					continue;
				} else {
					config->tcp_irtt = k;
				}
			} else
				missing_arg(cmd);
		} else if (config && !strcmp(cmd, "dg-mtu")) {
			/* dg-mtu <mtu>: MTU for datagram mode routes (unused) */
			if (arg) {
				int k = atoi(arg);

				if (k == 0) {
					invalid_arg(cmd, arg);
					continue;
				} else {
					config->dg_mtu = k;
				}
			} else
				missing_arg(cmd);
		} else if (config && !strcmp(cmd, "vc-mtu")) {
			/* vc-mtu <mtu>: MTU for virtual connect mode routes (unused) */
			if (arg) {
				int k = atoi(arg);

				if (k == 0) {
					invalid_arg(cmd, arg);
					continue;
				} else {
					config->vc_mtu = k;
				}
			} else
				missing_arg(cmd);
		} else if (config && !strcmp(cmd, "ip-adjust-mode")) {
			/* ip-adjust-mode no|yes: adjust ip-mode? (dg or vc) */
			if (arg) {
				int k = yesno(arg);

				if (k == -1) {
					invalid_arg(cmd, arg);
					continue;
				} else {
					config->ip_adjust_mode = k;
				}
			} else
				missing_arg(cmd);
		} else if (config && !strcmp(cmd, "arp-add")) {
			/* arp-add no|yes: adjust arp table? */
			if (arg) {
				int k = yesno(arg);

				if (k == -1) {
					invalid_arg(cmd, arg);
					continue;
				} else {
					config->ip_add_arp = k;
				}
			} else
				missing_arg(cmd);
		} else if (!strcmp(cmd, "ip-encaps-dev")) {
			if (arg)
				strcpy(ip_encaps_dev, arg);
			else
				missing_arg(cmd);
		} else if (!strcmp(cmd, "ax25-maxroutes")) {
			if (arg)
				ax25_maxroutes = atoi(arg);
			else
				missing_arg(cmd);
		} else if (!strcmp(cmd, "ip-maxroutes")) {
			if (arg)
				ip_maxroutes = atoi(arg);
			else
				missing_arg(cmd);
		} else if (!strcmp(cmd, "iproute2-table")) {
			if (arg)
				strcpy(iproute2_table, arg);
			else
				missing_arg(cmd);
		} else
			fprintf(stderr, "invalid command %s\n", cmd);
	}
	fclose(fp);

	load_ports();
	load_listeners();

	reload = 0;
}

void reload_config(void)
{
	config *cfg, *config = Config;

	while (config) {
		cfg = config->next;
		free(config);
		config = cfg;
	}
	Config = NULL;

	load_config();
}


/* commands:
   ---------
   
   add ax25 <callsign> <dev> <time> [<digipeater>]	# Add an AX.25 route
   add ip   <ip> <dev> <time> <call> <mode>		# Add an IP route & mode
   del ax25 <callsign> <dev>				# Remove an AX.25 route (from cache)
   del ip   <ip>					# Remove an IP route (from cache)
   list [ax25|ip]					# List cache entries
   reload						# Reload config
   save							# Save cache
   expire <minutes>					# Expire cache entries
   shutdown						# Save cache and exit
   
   There's a difference between 'list heard' and 'heard':
   
   The 'list' commands will output the symbolic port names as defined in 
   /usr/local/etc/axports (i.e. 9k6 for scc3), while 'heard' shows the 
   'real' network device name (i.e. scc3). All commands accept either the
   port or the network device name. The expample
   
   add ax25 dl0tha scc3 0 db0pra
   
   is equivalent to
   
   add ax25 dl0tha 9k6 0 dbpra
   
   Note that in conflicting cases the network device name has precedence
   over the port name.
*/


void interpret_command(int fd, unsigned char *buf)
{
	char *p, *cmd, *arg, *arg2, *dev, *time;
	ax25_address digipeater[AX25_MAX_DIGIS];
	ax25_rt_entry *ax25rt;
	int ndigi, ipmode, action;
	time_t stamp;
	config *config;
	long ip;

	p = prepare_cmdline(buf);

	if (!*p)
		return;

	cmd = get_next_arg(&p);
	arg = get_next_arg(&p);

	if (!strcmp(cmd, "add")) {
		if (arg == NULL)
			return;
		if ((arg2 = get_next_arg(&p)) == NULL)
			return;
		if ((dev = get_next_arg(&p)) == NULL)
			return;
		if ((time = get_next_arg(&p)) == NULL)
			return;
		if ((config = dev_get_config(dev)) == NULL)
			return;

		sscanf(time, "%lx", &stamp);

		if (!strcmp(arg, "ax25")) {
			ndigi = 0;
			arg = get_next_arg(&p);

			while (arg != NULL) {
				memcpy(&digipeater[ndigi++], asc2ax(arg),
				       AXLEN);
				if (ndigi == AX25_MAX_DIGIS)
					break;
				arg = get_next_arg(&p);
			}

			ax25rt =
			    update_ax25_route(config, asc2ax(arg2), ndigi,
					      digipeater, stamp);
			if (ax25rt != NULL)
				set_ax25_route(config, ax25rt);
		} else if (!strcmp(arg, "ip")) {
			ip = asc2ip(arg2);

			if ((arg2 = get_next_arg(&p)) == NULL)
				return;

			if ((arg = get_next_arg(&p)) == NULL)
				return;

			if (*arg == 'x')
				return;

			ipmode = (*arg == 'v');

			if (*ip_encaps_dev && (config =
			     dev_get_config(ip_encaps_dev)) == NULL) {
				printf("no config for %s\n",
				       ip_encaps_dev);
				return;
			}

			action =
			    update_ip_route(config, ip, ipmode,
					    asc2ax(arg2), stamp);

			if (action & NEW_ROUTE)
				if (set_route(config, ip))
					return;

			if (action & NEW_ARP)
				if (set_arp(config, ip, asc2ax(arg2)))
					return;

			if (action & NEW_IPMODE)
				if (set_ipmode
				    (config, asc2ax(arg2), ipmode))
					return;
		}
	} else if (!strcmp(cmd, "del")) {
		if (arg == NULL)
			return;
		arg2 = get_next_arg(&p);
		if (arg2 == NULL)
			return;

		if (!strcmp(arg, "ax25")) {
			arg = get_next_arg(&p);
			if (arg == NULL
			    || (config = dev_get_config(arg)) == NULL)
				return;
			del_ax25_route(config, asc2ax(arg2));
		} else if (!strcmp(arg, "ip")) {
			del_ip_route(asc2ip(arg2));
		}
	} else if (!strcmp(cmd, "expire")) {
		if (arg == NULL)
			return;
		sscanf(arg, "%ld", &stamp);
		if (stamp != 0) {
			stamp *= 60;
			expire_ax25_route(stamp);
			expire_ip_route(stamp);
		}
	} else if (!strcmp(cmd, "reload")) {
		reload_config();
	} else if (!strcmp(cmd, "list")) {
		if (arg == NULL)
			return;

		if (!strcmp(arg, "ax25"))
			dump_ax25_routes(fd, 0);
		else if (!strcmp(arg, "ip"))
			dump_ip_routes(fd, 0);
	} else if (!strcmp(cmd, "shutdown")) {
		save_cache();
		daemon_shutdown(0);
	} else if (!strcmp(cmd, "save")) {
		save_cache();
	} else if (!strcmp(cmd, "version")) {
		char buf[] = "ax25rtd version " VERSION "\n";
		write(fd, buf, strlen(buf));
	} else if (!strcmp(cmd, "quit")) {
		close(fd);
	}
}

void load_cache(void)
{
	FILE *fp;
	char buf[512];

	fp = fopen(DATA_AX25ROUTED_AXRT_FILE, "r");
	if (fp != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL)
			interpret_command(2, buf);
		fclose(fp);
	} else
		perror("open AX.25 route cache file");

	fp = fopen(DATA_AX25ROUTED_IPRT_FILE, "r");
	if (fp != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL)
			interpret_command(2, buf);
		fclose(fp);
	} else
		perror("open IP route cache file");
}

void save_cache(void)
{
	int fd;

	fd = creat(DATA_AX25ROUTED_AXRT_FILE, 0664);
	dump_ax25_routes(fd, 1);
	close(fd);

	fd = creat(DATA_AX25ROUTED_IPRT_FILE, 0664);
	dump_ip_routes(fd, 1);
	close(fd);

}
