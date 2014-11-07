/*
 *  This is my version of axl.c, written for the LBBS code to make it
 *    compatable with the kernel AX25 driver.  It appears to work, with
 *    my setup, so it'll probably not work else where :-).
 *
 *  This was inspired by the example code written by Alan Cox (GW4PTS)
 *    axl.c in AX25USER.TGZ from sunacm.swan.ac.uk.
 *
 *
 *  Copyright (C) 1995, 1996 by Darryl L. Miles, G7LED.
 *  Copyright (C) 1996 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *
 *  Just a quickie Feb 1995.
 *  It *was* just a quickie (at the time) but you'd know how these things
 *   just... Apr 1995.
 *
 *  If your AX25/NETROM system is relying on this code for
 *    securetty/firewalling then please be aware this has been coded
 *    with the intent on striving on through system/(mis)configuration
 *    errors in the hope that at worst it will run with a degraded
 *    service.  Rather than leave your system providing no service at
 *    all, if opinions require the old behavior back when let me know
 *    and I'll #ifdef it in.
 *
 *
 *  History:
 *
 *	1.0  Feb 1995	Basic AX25 listening daemon, Multi-port, Call
 *			  matching, etc...
 *
 *	1.1  Feb 1995	Moved entry scanning before fork().
 *			Added setgroups() to plug security hole.
 *			Minor fixes + Improved handling.
 *
 *	1.2  Apr 1995   NETROM support added from developing AX25
 *			  028b/029.
 *			Added 'defaults' port setting.
 *			Added FLAG_NODIGIS.
 *
 *	1.3  Jul 1995	Make it a little more intelligent about what to
 *			  do with errors.
 *			Added exec and argv[0] as two different fields,
 *			  much like inetd uses.
 *
 *	1.4  Aug 1995	Confirmed support for AX25 030 (1.3.20 + hacks),
 *			  it appears to work.
 *			It will now bootup even if initial config errors
 *			  occur when setting up and binding (e.g. port(s)
 *			  down), it will skip the port(s) with a problem
 *			  and listen out on those which are left standing.
 *
 *	1.5  Aug 1995	Updated old (buggy) libax25.a function copies in axl.
 *			  Causing all sorts of problems.
 *
 *	1.6  Aug 1995	Reset the 'defaults' entry's when we start parsing
 *			  a new interface.
 *
 *	1.7  Dec 1995	Added BROKEN_NETROM_KERNEL define for setsockopt.	
 *
 *	1.8  Jan 1996	Added support for AX25_BIND_ANY_DEVICE, specify just
 *			  [CALL-X VIA *].
 *			Better param parsing, T1 and T2 now using the real
 *			  time in seconds as params, and not kernel units.
 *			Connection loggin added, either via it's own logfile
 *			  or syslog.
 *			Modified 'defaults' to 'parameters'.
 *
 *	1.9  Jun 1996	Reworked config file parsing to use port names instead
 *			  of callsigns. Reformated source code.
 *
 *	Under alpha:
 *			BPQ like clever mode called for.... also a mode that
 *			  requires a packet to kick application open.
 *			A flag/mode which will cause a call to initgroups()
 *			  based on uid.
 *			Callsign validation check.
 *			Logging of errors.
 *			Handling of AX25.IP mode VC connections - dl9sau
 *				recommended settings in ax25d.conf:
 *				  parameters_extAX25 VC-wait-login VC-disc-on-linkfailure-msg VC-log-connections
				or
 *				  parameters_extAX25 VC-reject-login VC-send-failure-msg VC-log-connections
 *
 *
 *  TODO:
 *         The timing of the 'accept()' might be changed, defered to the
 *           child, then that child fork() itself, to stop race conditions
 *           around 'accept()'.
 *         Add a config file to allow/disallow connections/services at
 *           different times of the day to restrict access say.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#include <errno.h>

#include <config.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netrom/netrom.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/nrconfig.h>
#include <netax25/rsconfig.h>
#include <netax25/daemon.h>


#include "../pathnames.h"

/* Maximum number of command line arguments for the application we run */
#define MAX_ARGS	32

#define	FLAG_VALIDCALL	0x01	/* NOTUSED */
#define	FLAG_NOLOGGING	0x02	/* Don't log this connection */
#define	FLAG_CHKNRN	0x04	/* Check NetRom Neighbour - NOTUSED */
#define	FLAG_NODIGIS	0x08	/* Disallow digipeated uplinks */
#define	FLAG_LOCKOUT	0x10	/* Disallow connection */


struct axlist {		/* Have used same struct for quickness */
	struct axlist *next;	/* Port list */

	char *port;		/* Port call, only set across the port list */
	int fd;			/* The listening socket fd */

	int af_type;		/* AF_AX25, AF_NETROM or AF_ROSE port */

	struct axlist *ents;	/* Exec line entries */
	char *call;		/* Call in listing entries */
	char *node;		/* Node call in listing entries */
	uid_t uid;		/* UID to run program as */
	gid_t gid;		/* GID to run program as */
	char *exec;		/* Real exec */
	char *shell;		/* Command line. With possible escapes. */

	unsigned int window;	/* Set window to... */
	unsigned long t1;	/* Set T1 to... (Retrans timer) */
	unsigned long t2;	/* Set T2 to... (Ack delay) */
	unsigned long t3;	/* Set T3 to... (Idle Poll timer) */
	unsigned long idle;	/* Set T4 to... (Link Drop timer) */
	unsigned long n2;	/* Set N2 to... (Retries) */
	unsigned long flags;	/* FLAG_ values ORed... */

	int checkVC;		/* check for AX25.IP mode VC users? */
	int VCdiscOnLinkfailureMsg;	/* action when mode VC user and checkVC == 1 */
	int VCsendFailureMsg;	/* action when mode VC user and on checkVC > 1 */
	int VCloginEnable;		/* trigger login when mode VC user and checkVC == 1 and one line is read */
	int LoggingVC;		/* extra log stuff */
};

static struct axlist *AXL	= NULL;
static char *ConfigFile		= CONF_AX25D_FILE;
static char User[10];				/* Room for 'GB9ZZZ-15\0' */
static char Node[10];				/* Room for 'GB9ZZZ-15\0' */
static char myAX25Name[10];			/* Room for 'GB9ZZZ-15\0' */
static char *Port;
static int Logging		= FALSE;

static void SignalHUP(int);
static void SignalTERM(int);
static void WorkoutArgs(int, char *, int *, char **);
static void SetupOptions(int, struct axlist *);
static int ReadConfig(void);
static unsigned long ParseFlags(const char *, int);
static struct axlist *ClearList(struct axlist *);
static char *stripssid(const char *);

static fd_set fdread;
static int maxfd = -1;

/*--------------------------------------------------------------------------*/

void update_maxfd(void) {
	struct axlist *paxl;

	FD_ZERO(&fdread);

	for (maxfd = -1, paxl = AXL; paxl != NULL && paxl->fd >= 0; paxl = paxl->next) {
		FD_SET(paxl->fd, &fdread);

		if (paxl->fd > maxfd)
			maxfd = paxl->fd;
	}
}

/*--------------------------------------------------------------------------*/

void err_config(void) {
	if (AXL == NULL) {
		if (Logging)
			syslog(LOG_ERR, "config file reload error, exiting");
		exit(1);
	} else {
		if (Logging)
			syslog(LOG_INFO, "config file reload error, continuing with original");
	}
}		

/*--------------------------------------------------------------------------*/

void _ReadConfig(int dummy)
{
	ReadConfig();
}

/*--------------------------------------------------------------------------*/

void reload_timer(int sec) {
	struct itimerval itv;

	itv.it_value.tv_sec = sec;
	itv.it_value.tv_usec = 0;
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &itv, 0);
	signal(SIGALRM, _ReadConfig);
}

/*--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	struct axlist *axltmp, *paxl, *raxl;
	union {
		struct full_sockaddr_ax25 ax25;
		struct sockaddr_rose rose;
	} sockaddr;
	struct sigaction act, oact;
	socklen_t addrlen;
	int cnt;
	char buf[1024];
	char *p;
	char *mesg;
   
	while ((cnt = getopt(argc, argv, "c:lv")) != EOF) {
		switch (cnt) {
			case 'c':
				ConfigFile = optarg;
				break;

			case 'l':
				Logging = TRUE;
				break;

			case 'v':
				printf("ax25d: %s\n", VERSION);
				return 1;

			default:
				fprintf(stderr, "Usage: ax25d [-v] [-c altfile] [-l]\n");
				return 1;
		}
	}

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "ax25d: no AX.25 port data configured\n");
		return 1;
	}

	nr_config_load_ports();

	rs_config_load_ports();

	if (!daemon_start(TRUE)) {
		fprintf(stderr, "ax25d: cannot become a daemon\n");
		return 1;
	}

	if (Logging) {
		openlog("ax25d", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "starting");
	}

	act.sa_handler = SignalHUP;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGHUP, &act, &oact);

	act.sa_handler = SignalTERM;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGTERM, &act, &oact);

	ReadConfig();

	for (;;) {

		update_maxfd();
		if (maxfd < 0) {
			sleep(10);
			continue;
		}

		if (select(maxfd + 1, &fdread, NULL, NULL, NULL) <= 0)
			continue;

		for (paxl = AXL; paxl != NULL; paxl = paxl->next) {
			if (paxl->fd > 0 && FD_ISSET(paxl->fd, &fdread)) {
				pid_t pid;
				gid_t grps[2];
				char *argv[MAX_ARGS];
				int argc;
				int new;
				int i;
            
				/*
				 * Setting up a non-blocking accept() so is does not hang up
				 *  - I am not sure at this time why I didn't/don't assign
				 *  the socket non-blocking to start with.
				 */
				/*
				 * We really need to setup the netrom window option here so
				 *  that it's negotiated correctly on accepting the connection.
				 */
				/*
				 * It would be very useful if recvmsg/sendmsg were supported
				 *  then we can move the call checking up here.
				 */
				i = TRUE;
				ioctl(paxl->fd, FIONBIO, &i);

				addrlen = sizeof(struct full_sockaddr_ax25);
				new = accept(paxl->fd, (struct sockaddr *)&sockaddr, &addrlen);

				i = FALSE;
				ioctl(paxl->fd, FIONBIO, &i);
 
				if (new < 0) {
					if (errno == EWOULDBLOCK)
						continue;	/* It's gone ??? */

					if (Logging)
						syslog(LOG_ERR, "accept error %m, closing socket on port %s", paxl->port);
					close(paxl->fd);
					paxl->fd = -1;
					reload_timer(10);
					continue; 
				}

				switch (paxl->af_type) {
					case AF_AX25:
						strcpy(User, ax25_ntoa(&sockaddr.ax25.fsa_ax25.sax25_call));
						strcpy(Node, "");
						break;
					case AF_NETROM:
						strcpy(User, ax25_ntoa(&sockaddr.ax25.fsa_ax25.sax25_call));
						strcpy(Node, ax25_ntoa(&sockaddr.ax25.fsa_digipeater[0]));
						break;
					case AF_ROSE:
						strcpy(User, ax25_ntoa(&sockaddr.rose.srose_call));
						strcpy(Node, rose_ntoa(&sockaddr.rose.srose_addr));
						break;
				}

				for (raxl = paxl->ents; raxl != NULL; raxl = raxl->ents) {
					if (paxl->af_type == AF_NETROM && raxl->node != NULL && Node[0] != '\0') {
						if (strchr(raxl->node, '-') == NULL) {
							if (strcasecmp(raxl->node, stripssid(Node)) != 0)
								continue;	/* Found no match (for any SSID) */
						} else {
							if (strcasecmp(raxl->node, Node) != 0)
								continue;	/* Found no match */
						}
					}

					if (raxl->call == NULL)	/* default */
						break;

					if (strchr(raxl->call, '-') == NULL) {
						if (strcasecmp(raxl->call, stripssid(User)) == 0)
							break;	/* Found a match (for any SSID) */
					} else {
						if (strcasecmp(raxl->call, User) == 0)
							break;	/* Found a match */
					}
				}

				addrlen = sizeof(struct full_sockaddr_ax25);
				getsockname(new, (struct sockaddr *)&sockaddr, &addrlen);

				switch (paxl->af_type) {
					case AF_AX25:
						Port = ax25_config_get_port(&sockaddr.ax25.fsa_digipeater[0]);
						break;
					case AF_NETROM:
						Port = nr_config_get_port(&sockaddr.ax25.fsa_ax25.sax25_call);
						break;
					case AF_ROSE:
						Port = rs_config_get_port(&sockaddr.rose.srose_addr);
						break;
					default:
						Port = "???";
						break;
				}

				if (raxl == NULL) {
					/* No default */
					if (Logging && !(paxl->flags & FLAG_NOLOGGING)) {
						switch (paxl->af_type) {
							case AF_AX25:
								syslog(LOG_INFO, "AX.25 %s (%s) rejected - no default", User, Port);
								break;
							case AF_NETROM:
								syslog(LOG_INFO, "NET/ROM %s@%s (%s) rejected - no default", User, Node, Port);
								break;
							case AF_ROSE:
								syslog(LOG_INFO, "Rose %s@%s (%s) rejected - no default", User, Node, Port);
								break;
						}
					}
					close(new);
					continue;
				}

				if (raxl->flags & FLAG_LOCKOUT) {
					/* Not allowed to connect */
					if (Logging && !(paxl->flags & FLAG_NOLOGGING)) {
						switch (raxl->af_type) {
							case AF_AX25:
								syslog(LOG_INFO, "AX.25 %s (%s) rejected - port locked", User, Port);
								break;
							case AF_NETROM:
								syslog(LOG_INFO, "NET/ROM %s@%s (%s) rejected - port locked", User, Node, Port);
								break;
							case AF_ROSE:
								syslog(LOG_INFO, "Rose %s@%s (%s) rejected - port locked", User, Node, Port);
								break;
						}
					}
					close(new);
					continue;
				}

				if (raxl->af_type == AF_AX25 && (raxl->flags & FLAG_NODIGIS) && sockaddr.ax25.fsa_ax25.sax25_ndigis != 0) {
					/* Not allowed to uplink via digi's */
					if (Logging && !(paxl->flags & FLAG_NOLOGGING))
						syslog(LOG_INFO, "AX.25 %s (%s) rejected - digipeaters", User, Port);
					close(new);
					continue;
				}

				pid = fork();

				switch (pid) {
					case -1:
						if (Logging)
							syslog(LOG_ERR, "fork error %m");
						/*
						 * I don't think AX25 at the moment will hold the
						 * connection open, if the above does not make it
						 * through first time.
						 */
						close(new);
						break;			/* Oh well... */

					case 0:

						if (raxl->af_type == AF_AX25 && raxl->checkVC) {
							/* to which of my own addresses has the user connected? */
							getsockname(new, (struct sockaddr *)&sockaddr, &addrlen);
							strcpy(myAX25Name, ax25_ntoa(&sockaddr.ax25.fsa_ax25.sax25_call));

							sprintf(buf, "/usr/sbin/ax25rtctl -l ip | grep -i ' %s ' | /bin/grep 'v ' >/dev/null", User);

							/* he's not a VC user? */
							if (system(buf) != 0)
								goto login;

							if (raxl->checkVC > 1) {
								/* setproctitle("ax25d [%d]: rejecting, User);  */
							} else {
								/* setproctitle("ax25d [%d]: %s, User, (VCloginEnable ? "waiting" : "discarding")); */
							}

							/* is a VC user. checkVC > 0? then reject now, or fork and wait for pid=textdata.. */
							if (raxl->VCsendFailureMsg) {
								sprintf(buf, "*** %s: NO MODE VC with %s\r", myAX25Name, myAX25Name);
								write(new, buf, strlen(buf));
								if (raxl->checkVC > 1)
									sleep(1);
							}

							if (raxl->checkVC > 1) {
								if (Logging && (!(paxl->flags & FLAG_NOLOGGING) || raxl->LoggingVC))
									syslog((raxl->LoggingVC ? LOG_NOTICE : LOG_INFO), "AX.25 %s (%s:%s) rejected - AX25.IP-VC entry found", User, Port, myAX25Name);
								goto close_link;
							}

							/* checkVC == 1: wait for data */
							if (Logging && (!(paxl->flags & FLAG_NOLOGGING) || raxl->LoggingVC)) {
								syslog((raxl->LoggingVC ? LOG_NOTICE : LOG_INFO), "AX.25 %s (%s:%s) AX25.IP-VC host connected. %s", 
									User, Port, myAX25Name,
									(raxl->VCloginEnable ? "waiting for first PID=text packet for triggered login" : "discarding PID=text packets"));
							}

							*buf = 0;
							mesg = 0;
							while ((i = read(new, buf, (sizeof(buf)-1))) > 0) {
								buf[i] = 0;
								if (Logging && raxl->LoggingVC > 1) {
									/* debug */
									syslog((LOG_DEBUG), "DEBUG: AX.25 %s (%s:%s) AX25.IP-VC host said: >%s<", User, Port, myAX25Name, buf); 
								}
							        /* skip leading mess */
								for (p = buf; *p && (isspace(*p & 0xff) || *p == '\r'); p++) ;
								if (raxl->VCdiscOnLinkfailureMsg && !strncmp(p, "***", 3)) {
									/* format received line for debug purposes */
									/* 1. skip "***" */
									for (p += 3; *p && isspace(*p & 0xff); p++ ) ;
									mesg = p;
									/* 2. get rid off EOL delimiters */
									while (*p) {
										if (*p == '\r' || *p == '\n') {
											*p = 0;
											break;
										}
										p++;
									}
									/* end read loop */
									break;
								}
								/* per default, we discard all messages,
							 	 * because there's no useful combination
							 	 * using VC and pidText togegther
							 	 */
								if (raxl->VCloginEnable)
									goto login;
							}

							if (Logging && (!(paxl->flags & FLAG_NOLOGGING) || raxl->LoggingVC)) {
								syslog((raxl->LoggingVC ? LOG_NOTICE : LOG_INFO),
										"AX.25 %s (%s:%s) AX25.IP-VC host disconnected%s%s",
										User,
										Port,
										myAX25Name,
										(mesg ? ": " : "."),
										(mesg ? mesg : ""));
							}
							/* point of no return */
close_link:
							/* close link */
							/* setproctitle("ax25d [%s]: disconnecting", User); */
							close(new);
							exit(0);
						}
login:
						/* setproctitle("ax25d [%s]: login", User); */

						SetupOptions(new, raxl);
						WorkoutArgs(raxl->af_type, raxl->shell, &argc, argv);

						if (Logging && !(paxl->flags & FLAG_NOLOGGING)) {
							switch (paxl->af_type) {
								case AF_AX25:
									syslog(LOG_INFO, "AX.25 %s (%s) %s", User, Port, argv[0]);
									break;
								case AF_NETROM:
									syslog(LOG_INFO, "NET/ROM %s@%s (%s) %s", User, Node, Port, argv[0]);
									break;
								case AF_ROSE:
									syslog(LOG_INFO, "Rose %s@%s (%s) %s", User, Node, Port, argv[0]);
									break;
							}
						}

						dup2(new, STDIN_FILENO);
						dup2(new, STDOUT_FILENO);
						close(new);

						/*
						 * Might be more efficient if we just went down AXL,
						 * we cleaned up our parents left overs on bootup.
						 */
						for (axltmp = AXL; axltmp != NULL; axltmp = axltmp->next)
							close(axltmp->fd);

						if (Logging)
							closelog();

						/* Make root secure, before we exec() */
						/* Strip any supplementary gid's */
						if (setgroups(0, grps) == -1)
							exit(1);
						if (setgid(raxl->gid) == -1)
							exit(1);
						if (setuid(raxl->uid) == -1)
							exit(1);
						execve(raxl->exec, argv, NULL);
						exit(1);

					default:
						close(new);
						break;
				}
			}
		}
	}

	/* NOT REACHED */
	return 0;
}

static void SignalHUP(int code)
{
	ReadConfig();
}

static void SignalTERM(int code)
{
	if (Logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}
	
	exit(0);
}

static void WorkoutArgs(int af_type, char *shell, int *argc, char **argv)
{
	char buffer[1024];	/* Maximum arg size */
	char *sp, *cp;
	int cnt  = 0;
	int args = 0;
   
	for (cp = shell; *cp != '\0'; cp++) {
		if (isspace(*cp) && cnt != 0) {
			buffer[cnt]  = '\0';
			argv[args++] = strdup(buffer);
			cnt          = 0;

			if (args == MAX_ARGS - 1) {
				argv[args] = NULL;
				*argc = args;
				return;
			}

			continue;
		} else if (isspace(*cp))	/* && !cnt */
			continue;

		if (*cp == '%') {
			cp++;

			switch(*cp) {
				case 'd':	/* portname */
					for (sp = Port; *sp != '\0' && *sp != '-'; sp++)
						buffer[cnt++] = *sp;
					break;

				case 'U':	/* username in UPPER */
					for (sp = User; *sp != '\0' && *sp != '-'; sp++)
						buffer[cnt++] = toupper(*sp);
					break;

				case 'u':	/* USERNAME IN lower */
					for (sp = User; *sp != '\0' && *sp != '-'; sp++)
						buffer[cnt++] = tolower(*sp);
					break;

				case 'S':	/* username in UPPER (with SSID) */
					for (sp = User; *sp != '\0'; sp++)
						buffer[cnt++] = toupper(*sp);
					break;

				case 's':	/* USERNAME IN lower (with SSID) */
					for (sp = User; *sp != '\0'; sp++)
						buffer[cnt++] = tolower(*sp);
					break;

				case 'P':	/* nodename in UPPER */
					if (af_type == AF_NETROM) {
						for (sp = Node; *sp != '\0' && *sp != '-'; sp++)
							buffer[cnt++] = toupper(*sp);
					} else {
						buffer[cnt++] = '%';
					}
					break;

				case 'p':	/* NODENAME IN lower */
					if (af_type == AF_NETROM) {
						for(sp = Node; *sp != '\0' && *sp != '-'; sp++)
							buffer[cnt++] = tolower(*sp);
					} else {
						buffer[cnt++] = '%';
					}
					break;

				case 'R':	/* nodename in UPPER (with SSID) */
					if (af_type == AF_NETROM) {
						for (sp = Node; *sp != '\0'; sp++)
							buffer[cnt++] = toupper(*sp);
					} else {
						buffer[cnt++] = '%';
					}
					break;

				case 'r':	/* NODENAME IN lower (with SSID) */
					if (af_type == AF_NETROM) {
						for (sp = Node; *sp != '\0'; sp++)
							buffer[cnt++] = tolower(*sp);
					} else {
						buffer[cnt++] = '%';
					}
					break;

				case '\0':
				case '%':
				default:
					buffer[cnt++] = '%';
					break;
			}
		} else {
			buffer[cnt++] = *cp;
		}
	}

	if (cnt != 0) {
		buffer[cnt]  = '\0';
		argv[args++] = strdup(buffer);
	}

	argv[args] = NULL;
	*argc      = args;
}

static void SetupOptions(int new, struct axlist *axl)
{
	switch (axl->af_type) {
	case AF_AX25:
		if (axl->window != 0)
			setsockopt(new, SOL_AX25, AX25_WINDOW, &axl->window, sizeof(axl->window));
		if (axl->t1 != 0)
			setsockopt(new, SOL_AX25, AX25_T1, &axl->t1, sizeof(axl->t1));
		if (axl->n2 != 0)
			setsockopt(new, SOL_AX25, AX25_N2, &axl->n2, sizeof(axl->n2));
		if (axl->t2 != 0)
			setsockopt(new, SOL_AX25, AX25_T2, &axl->t2, sizeof(axl->t2));
		if (axl->t3 != 0)
			setsockopt(new, SOL_AX25, AX25_T3, &axl->t3, sizeof(axl->t3));
		if (axl->idle != 0)
			setsockopt(new, SOL_AX25, AX25_IDLE, &axl->idle, sizeof(axl->idle));
		break;
	case AF_NETROM:
		if (axl->t1 != 0)
			setsockopt(new, SOL_NETROM, NETROM_T1, &axl->t1, sizeof(axl->t1));
		if (axl->n2 != 0)
			setsockopt(new, SOL_NETROM, NETROM_N2, &axl->n2, sizeof(axl->n2));
		if (axl->t2 != 0)
			setsockopt(new, SOL_NETROM, NETROM_T2, &axl->t2, sizeof(axl->t2));
		break;
	case AF_ROSE:
		if (axl->idle != 0)
			setsockopt(new, SOL_ROSE, ROSE_IDLE, &axl->idle, sizeof(axl->idle));
		break;
	}
}

/**************************** CONFIGURATION STUFF ***************************/

static int ReadConfig(void)
{
	struct axlist axl_defaults;
	struct axlist *axl_build = NULL;
	struct axlist *axl_port  = NULL;
	struct axlist *axl_ent, *axltmp;
	union {
		struct full_sockaddr_ax25 ax25;
		struct sockaddr_rose rose;
	} sockaddr;
	struct passwd *pwd;
	FILE *fp;
	char buffer[2048];
	char *s, *port, *call, *node, *addr = NULL;
	unsigned long val;
	socklen_t addrlen;
	int af_type = 0;	/* Keep GCC happy */
	int line = 0;
	int hunt = TRUE, error = FALSE;
	int iamdigi = FALSE;
	int parameters = 0;

	signal(SIGALRM, SIG_IGN);
	memset(&axl_defaults, 0, sizeof(axl_defaults));

	if ((fp = fopen(ConfigFile, "r")) == NULL)
		return -1;

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		line++;

		if ((s = strchr(buffer, '\n')) != NULL)
			*s = '\0';
		if ((s = strchr(buffer, '\r')) != NULL)
			*s = '\0';

		if (buffer[0] == '#')
			continue;
         
		switch (buffer[0]) {
			case '[':		/* AX25 port call */
				af_type = AF_AX25;
				hunt    = TRUE;
				error   = FALSE;
				iamdigi = FALSE;
				break;
   
			case '<':		/* NETROM iface call */
				af_type = AF_NETROM;
				hunt    = TRUE;
				error   = FALSE;
				iamdigi = FALSE;
				break;

			case '{':		/* ROSE iface call */
				af_type = AF_ROSE;
				hunt    = TRUE;
				error   = FALSE;
				iamdigi = FALSE;
				break;
   
			default:
				if (hunt && !error)
					goto BadLine;
				break;
		}

		if (hunt) {	/* We've found a Iface entry */
			/* Reset 'defaults' entry on the interface */
			memset(&axl_defaults, 0, sizeof(axl_defaults));

			switch (af_type) {
				case AF_AX25:
					if ((s = strchr(buffer, ']')) == NULL)
						goto BadLine;
					*s = '\0';
					if ((s = strtok(buffer + 1, " \t")) == NULL)
						goto BadLine;
					port = s;
					call = NULL;
					if ((s = strtok(NULL, " \t")) != NULL) {
						if (strcasecmp(s, "VIA") == 0 || strcasecmp(s, "V") == 0) {
							if ((s = strtok(NULL, " \t")) == NULL)
								goto BadLine;
						}

						call = port;
						port = s;

						if ((s = strchr(call, '*')) != NULL) {
							iamdigi = TRUE;
							*s = '\0';
						}
					}
					if (strcmp(port, "*") == 0 && call == NULL) {
						fprintf(stderr, "ax25d: invalid port name\n");
						continue;
					}
					if (strcmp(port, "*") != 0) {
						if ((addr = ax25_config_get_addr(port)) == NULL) {
							fprintf(stderr, "ax25d: invalid AX.25 port '%s'\n", port);
							continue;
						}
					}
					if (call == NULL) {
						sockaddr.ax25.fsa_ax25.sax25_family = AF_AX25;
						sockaddr.ax25.fsa_ax25.sax25_ndigis = 0;
						ax25_aton_entry(addr, sockaddr.ax25.fsa_ax25.sax25_call.ax25_call);
					} else {
						sockaddr.ax25.fsa_ax25.sax25_family = AF_AX25;
						sockaddr.ax25.fsa_ax25.sax25_ndigis = 1;
						ax25_aton_entry(call, sockaddr.ax25.fsa_ax25.sax25_call.ax25_call);
						if (strcmp(port, "*") != 0)
							ax25_aton_entry(addr, sockaddr.ax25.fsa_digipeater[0].ax25_call);
						else
							sockaddr.ax25.fsa_digipeater[0] = null_ax25_address;
					}
					addrlen = sizeof(struct full_sockaddr_ax25);
					break;

				case AF_NETROM:
					if ((s = strchr(buffer, '>')) == NULL)
						goto BadLine;
					*s = '\0';
					port = buffer + 1;
					if ((addr = nr_config_get_addr(port)) == NULL) {
						fprintf(stderr, "ax25d: invalid NET/ROM port '%s'\n", port);
						continue;
					}
					sockaddr.ax25.fsa_ax25.sax25_family = AF_NETROM;
					sockaddr.ax25.fsa_ax25.sax25_ndigis = 0;
					ax25_aton_entry(addr, sockaddr.ax25.fsa_ax25.sax25_call.ax25_call);
					addrlen = sizeof(struct full_sockaddr_ax25);
					break;

				case AF_ROSE:
					if ((s = strchr(buffer, '}')) == NULL)
						goto BadLine;
					*s = '\0';
					if ((s = strtok(buffer + 1, " \t")) == NULL)
						goto BadLine;
					call = s;
					if ((s = strtok(NULL, " \t")) == NULL)
						goto BadLine;
					if (strcasecmp(s, "VIA") == 0 || strcasecmp(s, "V") == 0) {
						if ((s = strtok(NULL, " \t")) == NULL)
							goto BadLine;
					}
					port = s;
					if ((addr = rs_config_get_addr(port)) == NULL) {
						fprintf(stderr, "ax25d: invalid Rose port '%s'\n", port);
						continue;
					}
					if (strcmp(call, "*") == 0) {
						sockaddr.rose.srose_family = AF_ROSE;
						sockaddr.rose.srose_ndigis = 0;
						rose_aton(addr, sockaddr.rose.srose_addr.rose_addr);
						sockaddr.rose.srose_call   = null_ax25_address;
					} else {
						sockaddr.rose.srose_family = AF_ROSE;
						sockaddr.rose.srose_ndigis = 0;
						rose_aton(addr, sockaddr.rose.srose_addr.rose_addr);
						ax25_aton_entry(call,   sockaddr.rose.srose_call.ax25_call);
					}
					addrlen = sizeof(struct sockaddr_rose);
					break;

				default:
					fprintf(stderr, "ax25d: unknown af_type=%d\n", af_type);
					exit(1);
			}

			if ((axl_port = calloc(1, sizeof(*axl_port))) == NULL) {
				fprintf(stderr, "ax25d: out of memory\n");
				goto Error;
			}

			axl_port->port    = strdup(port);
			axl_port->af_type = af_type;

			if ((axl_port->fd = socket(axl_port->af_type, SOCK_SEQPACKET, 0)) < 0) {
				fprintf(stderr, "ax25d: socket: %s\n", strerror(errno));
				free(axl_port->port);
				free(axl_port);
				error = TRUE;
				reload_timer(60);
				continue;
			}
                        /* xlz - have to nuke this as this option is gone
                         * what should be here?
			if (iamdigi) {
				yes = 1;
				setsockopt(axl_port->fd, SOL_AX25, AX25_IAMDIGI, &yes, sizeof(yes));
			}
                        */

			if (bind(axl_port->fd, (struct sockaddr *)&sockaddr, addrlen) < 0) {
				fprintf(stderr, "ax25d: bind: %s on port %s\n", strerror(errno), axl_port->port);
				close(axl_port->fd);
				free(axl_port->port);
				free(axl_port);
				error = TRUE;
				reload_timer(60);
				continue;
			}

			if (listen(axl_port->fd, SOMAXCONN) < 0) {
				fprintf(stderr, "ax25d: listen: %s\n", strerror(errno));
				close(axl_port->fd);
				free(axl_port->port);
				free(axl_port);
				error = TRUE;
				reload_timer(60);
				continue;
			}

			/* Add it to the head of the list we are building */
			if (axl_build == NULL) {
				axl_build = axl_port;
			} else {
				for (axltmp = axl_build; axltmp->next != NULL; axltmp = axltmp->next);
				axltmp->next = axl_port;
			}

			hunt = FALSE;	/* Next lines will be entries */
		} else {		/* This is an entry */
			if ((axl_ent = calloc(1, sizeof(*axl_ent))) == NULL) {
				fprintf(stderr, "ax25d: out of memory\n");
				goto Error;
			}

			axl_ent->af_type = axl_port->af_type;	/* Inherit this */

			if ((call = strtok(buffer, " \t")) == NULL) {
				free(axl_ent);
				continue;
			}

			strupr(call);

			if (axl_ent->af_type == AF_NETROM) {
				if ((s = strchr(call, '@')) != NULL) {
					node = s + 1;
					*s = '\0';

					if (*node == '\0') {
						free(axl_ent);
						continue;
					}

					axl_ent->node = strdup(node);

					if (*call == '\0')
						call = "default";	/* @NODE means default@NODE */
				}
			}

			if (axl_ent->af_type == AF_AX25) {
				if ((strcasecmp("parameters_extAX25", call) == 0)) {
					axl_defaults.checkVC = axl_defaults.VCdiscOnLinkfailureMsg = axl_defaults.VCsendFailureMsg = axl_defaults.VCloginEnable = axl_defaults.LoggingVC = 0;
					while ((s = strtok(NULL, "  \t")) != NULL) {
						if (strncmp(s, "VC", 2))
							goto ignore;
						if (!strncmp(s, "VC", 2)) {
							if (!strcasecmp(s, "VC-reject-login"))
								axl_defaults.checkVC += 2;
							else if (!strcasecmp(s, "VC-wait-login"))
								axl_defaults.checkVC += 1;
							else if (!strcasecmp(s, "VC-disc-on-linkfailure-msg"))
								axl_defaults.VCdiscOnLinkfailureMsg = 1;
							else if (!strcasecmp(s, "VC-send-failure-msg"))
								axl_defaults.VCsendFailureMsg = 1;
							else if (!strcasecmp(s, "VC-login-ok"))
								axl_defaults.VCloginEnable = 1;
							else if (!strcasecmp(s, "VC-log-connections"))
								axl_defaults.LoggingVC += 1;
							else if (!strcasecmp(s, "VC-debug"))
								axl_defaults.LoggingVC += 2;
							else goto ignore;
						}
						continue;
ignore:
						fprintf(stderr, "ax25d: bad config entry on line %d: %s", line, s);
						fprintf(stderr, "  valid parametrs for parameters_extAX25 are:\n[VC-reject-login ||\nVC-wait-login [VC-login-ok] [VC-disc-on-linkfailure-msg]||\nVC-send-failure-msg || VC-log-connections]\n");
					}
					if (axl_defaults.checkVC) {
						if (axl_defaults.checkVC > 2) {
							fprintf(stderr, "warning: line %d: VC-reject-login and VC-wait-login are exclusive. using VC-reject-login", line);
							axl_defaults.checkVC = 2;
						}
					}
					continue;
				}
				if (axl_defaults.checkVC != 0) {
					axl_ent->checkVC = axl_defaults.checkVC;
					axl_ent->VCdiscOnLinkfailureMsg = axl_defaults.VCdiscOnLinkfailureMsg;
					axl_ent->VCsendFailureMsg = axl_defaults.VCsendFailureMsg;
					axl_ent->VCloginEnable = axl_defaults.VCloginEnable;
					axl_ent->LoggingVC = axl_defaults.LoggingVC;
				}
			}

			parameters = FALSE;

			if (strcasecmp("parameters", call) == 0)
				parameters = TRUE;
			else if (strcasecmp("default", call) != 0)
				axl_ent->call = strdup(call);

			/* Window */
			if ((s = strtok(NULL, " \t")) == NULL)
				goto BadArgsFree;

			if (!parameters) {
				if (strcmp(s, "*") != 0)
					axl_ent->window = atoi(s);
				else
					axl_ent->window = axl_defaults.window;
			} else {
				if (strcmp(s, "*") != 0)
					axl_defaults.window = atoi(s);
			}

			/* T1 */
			if ((s = strtok(NULL, " \t")) == NULL)
				goto BadArgsFree;

			if (!parameters) {
				if (strcmp(s, "*") != 0) {
					val = (unsigned long)(atof(s) / 0.1);

					if (val == 0 || val > 65535)
						axl_ent->t1 = axl_defaults.t1;
					else
						axl_ent->t1 = val;
				} else {
					axl_ent->t1 = axl_defaults.t1;
				}
			} else {
				if (strcmp(s, "*") != 0) {
					val = (unsigned long)(atof(s) / 0.1);

					if (val > 0 && val < 65535)
						axl_defaults.t1 = val;
				}
			}

			/* T2 */
			if ((s = strtok(NULL, " \t")) == NULL)
				goto BadArgsFree;

			if (!parameters) {
				if (strcmp(s, "*") != 0) {
					val = (unsigned long)(atof(s) / 0.1);

					if (val == 0 || val > 65535)
						axl_ent->t2 = axl_defaults.t2;
					else
						axl_ent->t2 = val;
				} else {
					axl_ent->t2 = axl_defaults.t2;
				}
			} else {
				if (strcmp(s, "*") != 0) {
					val = (unsigned long)(atof(s) / 0.1);

					if (val > 0 && val < 65535)
						axl_defaults.t2 = val;
				}
			}

			/* T3 */
			if ((s = strtok(NULL, " \t")) == NULL)
				goto BadArgsFree;

			if (!parameters) {
				if (strcmp(s, "*") != 0)
					axl_ent->t3 = atoi(s);
				else
					axl_ent->t3 = axl_defaults.t3;
			} else {
				if (strcmp(s, "*") != 0)
					axl_defaults.t3 = atoi(s);
			}

			/* Idle */
			if ((s = strtok(NULL, " \t")) == NULL)
				goto BadArgsFree;

			if (!parameters) {
				if (strcmp(s, "*") != 0)
					axl_ent->idle = atoi(s);
				else
					axl_ent->idle = axl_defaults.idle;
			} else {
				if (strcmp(s, "*") != 0)
					axl_defaults.idle = atoi(s);
			}

			/* N2 */
			if ((s = strtok(NULL, " \t")) == NULL)
				goto BadArgsFree;

			if (!parameters) {
				if (strcmp(s, "*") != 0)
					axl_ent->n2 = atoi(s);
				else
					axl_ent->n2 = axl_defaults.n2;
			} else {
				if (strcmp(s, "*") != 0)
					axl_defaults.n2 = atoi(s);
			}

			if (!parameters) {
				/* Flags */
				if ((s = strtok(NULL, " \t")) == NULL)
					goto BadArgsFree;

				axl_ent->flags = ParseFlags(s, line);
            
				if (!(axl_ent->flags & FLAG_LOCKOUT)) {
					/* Get username */
					if ((s = strtok(NULL, " \t")) == NULL)
						goto BadArgsFree;

					if ((pwd = getpwnam(s)) == NULL) {
						fprintf(stderr, "ax25d: UID for user '%s' is unknown, ignoring entry\n", s);
						goto BadUID;
					}

					axl_ent->uid = pwd->pw_uid;
					axl_ent->gid = pwd->pw_gid;

					/* Get exec file */
					if ((s = strtok(NULL, " \t")) == NULL)
						goto BadArgsFree;

					axl_ent->exec = strdup(s);

					/* Get command line */
					if ((s = strtok(NULL, "")) == NULL)
						goto BadArgsFree2;

					axl_ent->shell = strdup(s);
				}

				axl_ent->next  = NULL;

				if (axl_port->ents == NULL) {
					axl_port->ents = axl_ent;
				} else {
					for (axltmp = axl_port->ents; axltmp->ents != NULL; axltmp = axltmp->ents)
						;
					axltmp->ents = axl_ent;
				}
			}
		}

		continue;

BadLine:
		fprintf(stderr, "ax25d: bad config entry on line %d\n", line);
		continue;

BadUID:
		if (axl_ent->call != NULL)
			free(axl_ent->call);
		free(axl_ent);
		continue;

BadArgsFree2:
		if (axl_ent->exec != NULL)
			free(axl_ent->exec);
BadArgsFree:
		if (axl_ent->call != NULL)
			free(axl_ent->call);
		free(axl_ent);

		/* BadArgs: */
		fprintf(stderr, "ax25d: bad config entry on line %d, not enough fields.\n", line);
		continue;
	}

	fclose(fp);

	AXL = ClearList(AXL);
	AXL = axl_build;		/* Assign our built list to AXL */

	if (Logging)
		syslog(LOG_INFO, "new config file loaded successfuly");

	update_maxfd();
	return 0;

Error:
	axl_build = ClearList(axl_build);
	err_config();

	return -1;
}

static unsigned long ParseFlags(const char *kp, int line)
{
	unsigned long flags = 0UL;

	for (; *kp != '\0'; kp++) {
		switch (*kp) {
			case 'v':
			case 'V':
				flags |= FLAG_VALIDCALL;
				break;

			case 'q':
			case 'Q':
				flags |= FLAG_NOLOGGING;
				break;

			case 'n':
			case 'N':
				flags |= FLAG_CHKNRN;
				break;

			case 'd':
			case 'D':
				flags |= FLAG_NODIGIS;
				break;

			case 'l':
			case 'L':
				flags |= FLAG_LOCKOUT;
				break;

			case '0':
			case '*':
			case '-':		/* Be compatible and allow markers */
				break;

			default:
				fprintf(stderr, "ax25d: config file line %d bad flag option '%c'.\n", line, *kp);
				break;
		}
	}

	return flags;
}

static struct axlist *ClearList(struct axlist *list)
{
	struct axlist *tp1, *tp2, *tmp;
   
	for (tp1 = list; tp1 != NULL; ) {
		for (tp2 = tp1->ents; tp2 != NULL; ) {
			if (tp2->port != NULL)
				free(tp2->port);
			if (tp2->call != NULL)
				free(tp2->call);
			if (tp2->node != NULL)
				free(tp2->node);
			if (tp2->exec != NULL)
				free(tp2->exec);
			if (tp2->shell != NULL)
				free(tp2->shell);

			tmp = tp2->ents;
			free(tp2);
			tp2 = tmp;
		}

		if (tp1->port != NULL)
			free(tp1->port);
		if (tp1->call != NULL)
			free(tp1->call);
		if (tp1->node != NULL)
			free(tp1->node);
		if (tp1->exec != NULL)
			free(tp1->exec);
		if (tp1->shell != NULL)
			free(tp1->shell);

		close(tp1->fd);

		tmp = tp1->next;
		free(tp1);
		tp1 = tmp;
	}

	return NULL;
}

static char *stripssid(const char *call)
{
	static char newcall[10];
	char *s;
	
	strcpy(newcall, call);

	if ((s = strchr(newcall, '-')) != NULL)
		*s = '\0';
	
	return newcall;
}
