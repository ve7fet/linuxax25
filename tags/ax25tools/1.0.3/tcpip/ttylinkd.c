/*
 * ttylinkd:  A ttylink daemon using the ntalkd protocol.
 * Copyright (C) 1996,1197,2000 Craig Small (csmall@small.dropbear.id.au)
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have recieved a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 675 Mass Ave, Cambridge. MA 02139, USA.
 */
 /*
  * Versions:
  *  13/01/96   cs  Original Version
  *  29/01/96   cs  Added AX.25 proper support, thanks Tomi OH2BNS!!
  *  27/01/97   cs  Added support for stdin/stdout.
  *  04/03/97   cs  Added config file, no more defines.
  *  09/03/97   tjd Enhanced to allow specifying user@hostname.wherever.
  *                 rather than be confined to localhost.
  *  04/01/00   cs  Minor stuff with email addresses
  */
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <config.h>

#include <protocols/talkd.h>

#include <syslog.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <netax25/ax25.h>
#include <netrose/rose.h>

#include "../pathnames.h"


static char version[] = "ttylink daemon (Version 0.05, 9 March 1997) ready.\n";

#define SYSOP_USER "root"

/* TTY control characters from the sysop's talk screen */
char erasec;
char killc;
char werasec;

/* Users address family */
int userfamily;

#define ADDR_SIZE 256
char sysop_addr[ADDR_SIZE];
char *sysop_user=NULL, *sysop_host=NULL;
char config_file[128];

int send_control(int skt, struct in_addr addr, CTL_MSG msg, CTL_RESPONSE *resp);
void alarm_handle(int i);
void do_talk(int skt);
void read_config_file(int dummy);

/*static char *Commands[] = {
	"leaving invitation",
	"look-up",
	"delete",
	"announce" };
*/	
static char *Responses[] = {
	"success",
	"sysop not logged on",
	"failed for unknown reason",
	"sysop's machine unknown",
	"permission denied",
	"unknown request",
	"bad version",
	"bad address",
	"bad control address" };


int main(int argc, char *argv[])
{
	struct sockaddr_in ctl_sin;
	struct in_addr my_addr, rem_addr;
	int ctl_skt, skt, new_skt;
	CTL_RESPONSE resp;
	CTL_MSG msg;
	struct protoent *ppe;
	struct hostent *phe, *rhe=NULL;
	char hostname[256];
	int local_id, remote_id;
	char buf[256];
	char user[NAME_SIZE];
	struct sockaddr sa, msg_sa;
	struct sockaddr_in *peer_sin=NULL, *msg_sin;
	struct sockaddr_ax25 *peer_sax;
	struct sockaddr_rose *peer_srose;
	socklen_t sa_len, length;
	int i;
		
	/* Open up the system logger */
	openlog(argv[0], LOG_PID, LOG_DAEMON);
	
	write(STDOUT_FILENO, version, strlen(version));
	
	/* Work out who is calling us */
	userfamily = AF_UNSPEC;
	memset(user, 0, NAME_SIZE);
	strcpy(sysop_addr, SYSOP_USER);
	strcpy(config_file, CONF_TTYLINKD_FILE);
	for(i=1 ; i < argc ; i++)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
			case 'v':
				return 0;
				break;
			case 'h':
				printf("ttylinkd comes with ABSOLUTELY NO WARRANY, see the file COPYING\n");
				printf("This is free software, and you are welcome to redistribute it\n");
				printf("under the terms of the GNU General Public License.\n\n");
				printf("Usage: %s [-hv] [-c <callsign>] [-f <config_file>]\n", argv[0]);
				return 0;
				break;
			case 'f':
				if (i+2 > argc)
				{
					printf("The -f flag needs a parameter.\n");
					return 0;
					break;
				}
				strncpy(config_file, argv[++i], 127);
				break;
			case 'c':
				if ( i+2 > argc)
				{
					printf("The -c flag need a parameter.\n");
					return 0;
					break;
				}
				strncpy(user, argv[++i], NAME_SIZE);
				break;
			default:
				fprintf(stderr, "%s: Unknown flag, type %s -h for help\n", argv[0], argv[0]);
				return -1;
				break;
			} /*switch */
		} /* - */
	} /* for */ 
	if (user[0] == '\0')
	{
		sa_len = sizeof(sa);
		if (getpeername(STDOUT_FILENO, &sa, &sa_len) < 0)		
		{
			fprintf(stderr, "%s: getpeername() failed, you must specify a callsign in stdin mode.\n", argv[0]);
			syslog(LOG_CRIT | LOG_DAEMON, "main(): getpeername() failed.");
			return 0;
		} else {
			userfamily = sa.sa_family;
			switch(sa.sa_family) {
				case AF_INET:
					peer_sin = (struct sockaddr_in*)&sa;
					write(STDOUT_FILENO, buf, strlen(buf));			
					sprintf(buf, "Please enter your callsign: ");
					write(STDOUT_FILENO, buf, strlen(buf));
					fflush(stdout);
					if (fgets(user, NAME_SIZE-1, stdin) == NULL)
						return 0;
					for (i = 0; user[i] != '\0' && user[i] != '\n' && user[i] != '\r'; i++)
						;
					user[i] = '\0';
					if (strlen(user) < 1)
						return 0;
					userfamily = AF_INET;
					break;
				case AF_AX25:
				case AF_NETROM:
					peer_sax = (struct sockaddr_ax25*)&sa;
					for(i=0 ; i < 6 ; i++)
					{
						user[i] = tolower(((peer_sax->sax25_call.ax25_call[i]) >>1)&0x7f);
						if (user[i] == ' ')
						break;
					}
					user[i] = '\0';
					break;
				case AF_ROSE:
					peer_srose = (struct sockaddr_rose*)&sa;
					for(i=0 ; i < 6 ; i++)
					{
						user[i] = tolower(((peer_srose->srose_call.ax25_call[i]) >>1)&0x7f);
						if (user[i] == ' ')
						break;
					}
					user[i] = '\0';
					break;
				default:
					syslog(LOG_DAEMON | LOG_CRIT, "Unsupported address family.");
					exit(1);
			}
			
		}													
	} /* argc */

	/* Read the configuration file to find the System Operator. */
	read_config_file(0);
	/* ... and parse it into user@host */
	sysop_user=strtok(sysop_addr,"@");
	sysop_host=strtok(NULL,"@");

	if ((ppe = getprotobyname("udp")) == 0)
	{
		syslog(LOG_DAEMON | LOG_CRIT, "Cannot find udp protocol entry.");
		exit(1);
	}

	/* Obtain our local hostname */
	gethostname(hostname,256);

	/* Get the remote address, or use ours if remote isn't specified */
	if (sysop_host)
	{
		if ((rhe = gethostbyname(sysop_host)) == NULL)
		{
			syslog(LOG_DAEMON | LOG_CRIT, "main(): gethostbyname failed.");
			exit(1);
		}
	}
	else
	{
		if ((rhe = gethostbyname(hostname)) == NULL)
		{
			syslog(LOG_DAEMON | LOG_CRIT, "main(): gethostbyname failed.");
			exit(1);
		}
	}
	memcpy((char*)&rem_addr, rhe->h_addr, rhe->h_length);

	/* Get our local address */
	if ((phe = gethostbyname(hostname)) == NULL)
	{
		syslog(LOG_DAEMON | LOG_CRIT, "main(): gethostbyname failed.");
		exit(1);
	}
	memcpy((char*)&my_addr, phe->h_addr, phe->h_length);

	/* Create local data socket */
	memset((char*)&msg_sa, 0, sizeof(msg_sa));

	msg_sa.sa_family = AF_INET;
        msg_sin = (struct sockaddr_in*)&msg_sa;
	msg_sin->sin_port = htons(0);
	memcpy((char*)&(msg_sin->sin_addr), phe->h_addr, phe->h_length);
	
	if ((skt = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		syslog(LOG_DAEMON | LOG_CRIT, "main(): socket() failed.");
		exit(1);
	}
	
	if (bind( skt, &msg_sa, sizeof(msg_sa)) != 0)
	{
		syslog(LOG_DAEMON | LOG_CRIT, "main(): bind() failed.");
		exit(1);
	}
	
	length = sizeof(msg_sa);
	if (getsockname(skt, &msg_sa, &length) < 0)
	{
		syslog(LOG_DAEMON | LOG_CRIT, "main(): getsockname() failed.");
		exit(1);
	}

	/* Create local control socket */
	memset((char*)&ctl_sin, 0, sizeof(ctl_sin));

	ctl_sin.sin_family = AF_INET;
	memcpy((char*)&ctl_sin.sin_addr, phe->h_addr, phe->h_length);
	ctl_sin.sin_port = htons(0);
	
	if ((ctl_skt = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		syslog(LOG_DAEMON | LOG_CRIT, "main(): socket() while attempting to create control socket.");
		close(skt);
		exit(1);
	}
	
	if (bind(ctl_skt, (struct sockaddr*)&ctl_sin, sizeof(ctl_sin)) != 0)
	{
		syslog(LOG_DAEMON | LOG_CRIT, "main(): Error when trying to bind() control socket.");
		close(skt);
		exit(1);
	}
	
	length = sizeof(ctl_sin);
	if (getsockname(ctl_skt, (struct sockaddr*)&ctl_sin, &length) < 0)
	{
		syslog(LOG_DAEMON | LOG_CRIT,"main(): Error when trying to getsockname() for control socket.");
		close(skt);
		close(ctl_skt);
		exit(1);
	}
	
	/* Start talking to the talk daemon */
	memset((char*)&msg, 0, sizeof(msg));
	msg.vers = TALK_VERSION;
	msg.id_num = htonl(0);
	msg.addr.sa_family = ntohs(AF_INET);
        memcpy(&(msg.ctl_addr), &msg_sa, sizeof(struct osockaddr));
	/*msg.ctl_addr = *(struct sockaddr*)&ctl_sin;
	msg.ctl_addr = *(struct sockaddr*)&ctl_sin;*/
	msg.ctl_addr.sa_family = ntohs(AF_INET);
	msg.pid = htonl(getpid());
	strncpy(msg.l_name, user, NAME_SIZE-1);
	strncpy(msg.r_name, sysop_user, NAME_SIZE-1);
	
	
	/* Now look for an invite */
	msg.type = LOOK_UP;
	(void) send_control(ctl_skt, rem_addr, msg, &resp);

	/* The person not there? Send an announce and wake him up */
	msg.type = ANNOUNCE;
	memcpy((char*)&(msg.addr), (char*)&msg_sa, sizeof(struct osockaddr));
	msg.addr.sa_family = htons(AF_INET);
	msg.id_num = -1;
	i = send_control(ctl_skt, rem_addr, msg, &resp);
	if ( i != SUCCESS)
	{
		if (i > BADCTLADDR)
			printf("Cannot talk to sysop errno=%d.\n",i);
		else
			printf("Cannot talk to sysop, reason: %s.\n",Responses[i]);
		
		close(skt);
		close(ctl_skt);
		return 0;
	}
	/* Update the ID number so that we both know what we're talking about. */
	remote_id = resp.id_num;

	/* Get the TCP port ready for a connection */
	if (listen(skt, 5) != 0)
	{
		syslog(LOG_DAEMON | LOG_CRIT, "main(): Error when trying to listen() on socket.");
		exit(1);
	}
			
	/* Now we have to make an invitation for the other user */
	msg.type = LEAVE_INVITE;
	if (send_control(ctl_skt, my_addr, msg, &resp) != SUCCESS)
	{
		printf("Problem with leaving an invitation\n");
		syslog(LOG_DAEMON | LOG_CRIT, "main(): Cannot leave invititation.");
		close(skt);
		close(ctl_skt);
		return 0;
	}
	local_id = resp.id_num;
	
	sprintf(buf, "Paging sysop.\n");
	write(STDOUT_FILENO, buf, strlen(buf));
	
	/* Wait for the sysop to connect to us */
	signal(SIGALRM, alarm_handle);
	alarm(30);
	
	if ((new_skt = accept(skt, 0, 0)) < 0)
	{
		if (errno == EINTR)
		{
			/* Delete invitations from both daemons */
			msg.type = DELETE;
			msg.id_num = htonl(local_id);
			send_control(ctl_skt, my_addr, msg, &resp);
			msg.id_num = htonl(remote_id);
			send_control(ctl_skt, rem_addr, msg, &resp);

			close(skt);
			close(ctl_skt);
			return 0;
		}
		syslog(LOG_DAEMON | LOG_WARNING, "main(): accept() failed. (%m)");
	}
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	
	close(skt);
	skt = new_skt;
	
	/* Delete invitations from both daemons */
	msg.type = DELETE;
	msg.id_num = htonl(local_id);
	send_control(ctl_skt, my_addr, msg, &resp);
	msg.id_num = htonl(remote_id);
	send_control(ctl_skt, rem_addr, msg, &resp);
	
	sprintf(buf, "Sysop has responded.\n");		
	write(STDOUT_FILENO, buf, strlen(buf));
	
	/* 
	 * A little thing that they don't mention anywhere is the fact that the
	 * first three characters on a connection are used to work out to erase,
	 * kill and word erase characters. Nice to know eh?
	 */
	/* We send ours first, but then again we get data in cooked form, so
	 * we don't use this. */
	buf[0] = '\0';
	buf[1] = '\0';
	buf[2] = '\0';
	if (write(skt, buf, 3) != 3)
	{
		printf("Connnection lost\n");
		close(skt);
		close(ctl_skt);
		return 0;
	}
	/* Get their character stuff */
	if (read(skt, buf, 3) != 3)
	{
		printf("Connection lost\n");
		close(skt);
		close(ctl_skt);
		return 0;
	}
	erasec = buf[0];
	killc = buf[1];
	werasec = buf[2];
		
	/* Tell the sysop who this person is */
	if (sa.sa_family == AF_AX25)
	{
		sprintf(buf, "Incoming ttylink from %s.\n", user);
		write(skt, buf, strlen(buf));
	}
	if (sa.sa_family == AF_INET)
	{
		sprintf(buf, "Incoming ttylink from %s@%s.\n", user, inet_ntoa(peer_sin->sin_addr));
		write(skt, buf, strlen(buf));
	}
				
	
	do_talk(skt);

	close(skt);
	close(ctl_skt);					
	return 0;
}

/*
 * Used to send control messages to our friendly local talk daemon
 */ 
int send_control(int skt, struct in_addr addr, CTL_MSG msg, CTL_RESPONSE *resp)
{
	fd_set fdvar;
	struct timeval timeout;
	struct sockaddr_in sin;
	static int talk_port = 0;
	struct servent *pse;
	
	/* Look up talk port once only */
	if (talk_port == 0)
	{
		if ((pse = getservbyname("ntalk", "udp")) == NULL)
		{
			perror("getservbyname, assuming 518");
			talk_port = 518;
		} else {
			talk_port = pse->s_port;
		}
	}

	/* Create the socket address */
	memset((char*)&sin, 0, sizeof(sin));
	sin.sin_addr = addr;
	sin.sin_family = AF_INET;
	sin.sin_port = talk_port;
	
	if (sendto(skt, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&sin, sizeof(sin)) != sizeof(msg))
	{
		syslog(LOG_DAEMON | LOG_ERR, "send_control(): sendto failed (%m).");
		return -1;
	}

	/* Wait for reply */
	FD_ZERO(&fdvar);
	FD_SET(skt, &fdvar);
	timeout.tv_sec = RING_WAIT;
	timeout.tv_usec = 0;
	
	if (select(32, &fdvar, NULL, NULL, &timeout) < 0)
		syslog(LOG_DAEMON | LOG_ERR, "send_control(): select failed. (%m)");
	
	/*
	 * The server is ignoring us, see ya later
	 */			
	if (!(FD_ISSET(skt, &fdvar)))
	{
		printf("Talk server not responding after %d seconds, aborting.\n", RING_WAIT);
		return -1;
	}
	
	/* Get the message */
	if(recv(skt, resp, sizeof(resp),0) ==0)	
		syslog(LOG_DAEMON | LOG_ERR, "send_control(): recv failed. (%m)");
		
	return resp->answer;
}

/* Used to process the data from the sysop */
int send_sysop_data(char *buf, int len)
{
	static char outbuf[82];
	static char *bptr = outbuf;
	int i;
	
	for(i = 0; i < len; i++)
	{
		/* Check for erase character */
		if (buf[i] == erasec)
		{
			if (bptr > outbuf)
				bptr--;
			continue;
		}
		
		/* Check for kill character */
		if (buf[i] == killc)
		{
			bptr = outbuf;
			continue;
		}
		
		/* Check for word-erase character */
		if (buf[i] == werasec)
		{
			while( (bptr > outbuf) && (*bptr != ' ') )
				bptr--;
			continue;
		}

		/* Check for newline character */
		if (buf[i] == '\n')
		{
			if ( (userfamily == AF_AX25) || (userfamily == AF_NETROM) || (userfamily == AF_ROSE) )
			{
				*bptr = '\r';
				bptr++;
			} else {
				*bptr = '\r';
				bptr++;
				*bptr = '\n';
				bptr++;
			}
		} else {
			*bptr = buf[i];
			bptr++;
		}
				
		/* Check for carriage return, which means send it */
		/* We also send if we have more than 80 characters */
		if (buf[i] == '\n' || (bptr - outbuf) > 80 )
		{
			if (write(STDOUT_FILENO, outbuf, (bptr - outbuf) ) != (bptr - outbuf) )
			{
				return -1;
			}
			bptr = outbuf;
		}
	} /* for */
	return 0;
}
		
/* Used to process the data from the user - len must not exceed 256 */
int send_user_data(int skt, char *buf, int len)
{
	char outbuf[256];
	char *bptr = outbuf;
	int i;

	for(i = 0; i < len; i++)
	{
		if (buf[i] == '\r')
		{
			if (userfamily == AF_INET)
			{
				*bptr = '\n';
				bptr++;
				/*
				 * check if this is a <CR><LF> or a <CR><NUL>
				 * and skip the <LF> or <NUL>.
				 */
				if (buf[i + 1] == '\n' || buf[i + 1] == '\0')
				{
					i++;
				}
			} else
			{
				*bptr = '\n';
				bptr++;
			}
			continue;
		}
		*bptr = buf[i];
		bptr++;
	} /* for */

	if (write(skt, outbuf, bptr - outbuf) != bptr - outbuf)
	{
		return -1;
	}
	return 0;
}


/* The main talking loop */
void do_talk(int skt)
{
	fd_set fdvar;
	char inbuf[256], outbuf[256];
	struct timeval timeout;
	int i;
	
	
	while(1)
	{
		FD_ZERO(&fdvar);
		FD_SET(skt, &fdvar);
		FD_SET(STDIN_FILENO, &fdvar);
		
		timeout.tv_sec = 30;
		timeout.tv_usec = 0;
		if (select(32, &fdvar, NULL, NULL, &timeout) == 0)
		{
			if (ioctl(STDIN_FILENO, FIONREAD, (struct sgttyb *)&i) < 0)
				return;
			if (ioctl(skt, FIONREAD, (struct sgttyb*)&i) < 0)
				return;
		}
		
		if (FD_ISSET(skt, &fdvar))
		{
			if ((i = read(skt, inbuf, 256)) <= 0)
			{
				strcpy(inbuf,"Sysop closed connection.\n");
				write(STDOUT_FILENO, inbuf, strlen(inbuf));
				return;
			}
			if (send_sysop_data(inbuf, i) < 0)
			{
				strcpy(outbuf, "User closed connection.\n");
				(void) write(skt, outbuf, strlen(outbuf));
				return;
			}
		}
		if (FD_ISSET(STDIN_FILENO, &fdvar))
		{
			if ((i = read(STDIN_FILENO, outbuf, 256)) <= 0)
			{
				strcpy(outbuf, "User closed connection.\n");
				(void) write(skt, outbuf, strlen(outbuf));
				return;
			}
			if (send_user_data(skt, outbuf, i) < 0)
				{
					strcpy(inbuf,"Sysop closed connection.\n");
					write(STDOUT_FILENO, inbuf, strlen(inbuf));
					return;
			}
		}
	}
}			

void alarm_handle(int i)
{
	char buf[256];
	
	strcpy(buf, "Sysop not responding.\n");
	write(STDOUT_FILENO, buf, strlen(buf));
}

void read_config_file(int dummy)
{
	char buf[128];
	char param[20], value[108];
	FILE *fp;
	
	if ( (fp = fopen(config_file, "r")) == NULL) {
		syslog(LOG_DAEMON | LOG_ERR, "Cannot find configuration file: %s (%m)\n",config_file); 
		return;
	}
	/* Reset any variables here */
	
	while ( fgets(buf, 128, fp) != NULL) {
		if ( buf[0] == '#')
			continue;
		if (sscanf(buf, "%[^=]%*c%[^\n]", param, value) == 2) {
			/* A big wierdo switch */
			if (strcasecmp(param, "sysop") == 0) {
				strncpy(sysop_addr, value, ADDR_SIZE);
			}
		}
	} /* while */
	fclose(fp);
	signal(SIGHUP, read_config_file);
}
