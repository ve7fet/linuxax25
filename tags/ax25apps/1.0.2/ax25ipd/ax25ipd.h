/* ax25ipd.h    general configuration info
 *
 * Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.
 * This software may be freely used, distributed, or modified, providing
 * this header is not removed.
 * 
 */

/* 
 * Modifications added for dual port kiss TNC 
 * by Michael Durrant VE3PNX and D. Jeff Dionne Feb 4, 1995
 */

/*
 * cleaned up and prototyped for inclusion into the standard linux ax25
 * toolset in january 1997 by rob mayfield, vk5xxx/vk5zeu
 */

/*
 * added route flags, it's a little ugly, but is extensible fairly easily.
 * provided a mechanism for handling broadcast traffic
 * Terry Dawson, VK2KTJ, July 1997.
 *
 * Minor bug fixes.
 * Terry Dawson, VK2KTJ, September 2001.
 */

/*
  Version 1.0.5
  added provision for dynamic dns hosts
  Steve Fraser vk5asf, June 2005
*/

/*
  Version 1.0.6


*/

/*
  Version 1.1.0
  added support for baudrates 57600 and 115200
  Lee Woldanski ve7fet, Jan 2011
*/

/* Define the current version number
 *
 * The first digit represents the major release (0 is a prototype release)
 *
 * The second represents major changes that might affect configuration
 * file formats or compilation sequences, or anything that may make
 * existing setups change.
 *
 * The last digit(s) marks simple bug fixes.
 *
 */

#define VERS2 "Version 1.1.0"

#define IPPROTO_AX25 93
#define DEFAULT_UDP_PORT 10093

/* local includes */
#include	"../pathnames.h"

/* system includes */
#include	<ctype.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<memory.h>
#include	<netdb.h>
#include	<setjmp.h>
#include	<signal.h>
#include	<stdio.h>
#define __USE_XOPEN
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<limits.h>
#include	<arpa/inet.h>
#include	<netinet/in.h>
#include	<netinet/in_systm.h>
#include	<netinet/ip.h>
#include	<netinet/ip_icmp.h>
#include	<sys/ioctl.h>
#include	<sys/socket.h>
#include	<termio.h>
#include	<sys/termios.h>
#include	<sys/time.h>
#include	<sys/types.h>
#include	<netax25/daemon.h>

int udp_mode;                   /* true if we need a UDP socket */
int ip_mode;                    /* true if we need the raw IP socket */
unsigned short my_udp;          /* the UDP port to use (network byte order) */
char ttydevice[PATH_MAX];       /* the tty device for serial comms */
int ttyspeed;                   /* The baud rate on the tty device */
unsigned char mycallsign[7];    /* My callsign, shifted ASCII with SSID */
unsigned char mycallsign2[7];   /* My seconds port callsign, shifted ASCII with SSID */
unsigned char myalias[7];       /* An alias to use */
unsigned char myalias2[7];      /* An alias for second port */
char bc_text[128];              /* The text for beacon messages */
int bc_interval;                /* The interval, in seconds, between beacons */
int bc_every;                   /* true=every, false=after */
int digi;                       /* True if we are connected to a TNC */
int loglevel;                   /* Verbosity level */
/* addition for dual port flag */
int dual_port;

struct {
	int kiss_in;            /* # packets received */
	int kiss_toobig;        /* packet too large */
	int kiss_badtype;       /* control byte non-zero */
	int kiss_out;           /* # packets sent */
	int kiss_beacon_outs;   /* # of beacons sent */
	int kiss_tooshort;      /* packet too short to be a valid frame */
	int kiss_not_for_me;    /* packet not for me (in digi mode) */
	int kiss_i_am_dest;     /* I am destination (in digi mode) */
	int kiss_no_ip_addr;    /* Couldn't find an IP addr for this call */
	int udp_in;             /* # packets received */
	int udp_out;            /* # packets sent */
	int ip_in;              /* # packets received */
	int ip_out;             /* # packets sent */
	int ip_failed_crc;      /* from ip, but failed CRC check */
	int ip_tooshort;        /* packet too short to be a valid frame */
	int ip_not_for_me;      /* packet not for me (in digi mode) */
	int ip_i_am_dest;       /* I am destination (in digi mode) */
} stats;

#define MAX_FRAME 2048

#define LOGL1(arg...) if(loglevel>0)syslog(LOG_DAEMON | LOG_WARNING, ##arg)
#define LOGL2(arg...) if(loglevel>1)syslog(LOG_DAEMON | LOG_WARNING, ##arg)
#define LOGL3(arg...) if(loglevel>2)syslog(LOG_DAEMON | LOG_WARNING, ##arg)
#define LOGL4(arg...) if(loglevel>3)(void)syslog(LOG_DAEMON | LOG_DEBUG, ##arg)

#define	AXRT_BCAST	1
#define	AXRT_DEFAULT	2
#define	AXRT_PERMANENT	4
#define	AXRT_LEARN	8	


#define IPSTORAGESIZE 6 /* Bytes needed for call_to_ip */

/* start external prototypes */
/* end external prototypes */

/* kiss.c */
void kiss_init(void);
void assemble_kiss(unsigned char *, int);
void send_kiss(unsigned char, unsigned char *, int);
void param_add(int, int);
void dump_params(void);
void send_params(void);
/* void do_beacon(void);  not here it isnt !! xxx */

/* routing.c */
void route_init(void);
void route_add(char *, unsigned char *, unsigned char *, int, unsigned int);
int route_ipmatch(struct sockaddr_in *, unsigned char *);
unsigned char *retrieveip(unsigned char *, unsigned char *);
void route_process(struct sockaddr_in *, unsigned char *);
void bcast_add(unsigned char *);
unsigned char *call_to_ip(unsigned char *, unsigned char *);
int is_call_bcast(unsigned char *);
void send_broadcast(unsigned char *, int);
void dump_routes(void);
void update_dns(unsigned);

/* config.c */
void config_init(void);
void config_read(char *);
int parse_line(char *);
int a_to_call(char *, unsigned char *);
char *call_to_a(unsigned char *);
void dump_config(void);

/* process.c */
void process_init(void);
void from_kiss(unsigned char *, int);
void from_ip(unsigned char *, int, struct sockaddr_in *);
/* void do_broadcast(void);  where did this go ?? xxx */
void do_beacon(void);
int addrmatch(unsigned char *, unsigned char *);
int addrcompare(unsigned char *, unsigned char *);
unsigned char *next_addr(unsigned char *);
unsigned char *from_addr(unsigned char *);
void add_crc(unsigned char *, int);
void dump_ax25frame(char *, unsigned char *, int);

/* io.c */
void io_init(void);
void io_open(void);
void io_start(void);
void send_ip(unsigned char *, int, unsigned char *);
void send_tty(unsigned char *, int);
int io_error(int, unsigned char *, int, int, int, int);

/* crc.c */
unsigned short int compute_crc(unsigned char *, int);
unsigned short int pppfcs(unsigned short, unsigned char *, int);
unsigned short int compute_crc(unsigned char *, int);
int ok_crc(unsigned char *, int);

/* ax25ipd.c */
int main(int, char **);
void greet_world(void);
void do_stats(void);
void hupper(int);
void usr1_handler(int);
void int_handler(int);
void term_handler(int);

/* io.c */
extern int ttyfd_bpq;

/* bpqether.c */
int send_bpq(unsigned char *buf, int len);
int receive_bpq(unsigned char *buf, int l);
int open_ethertap(char *ifname);
int set_bpq_dev_call_and_up(char *ethertap_name);

/*
 * end
 */
