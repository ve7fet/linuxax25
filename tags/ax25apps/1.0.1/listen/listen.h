
#define T_ERROR		1
#define T_PORT		2
#define T_KISS		3
#define T_BPQ		4
#define T_DATA		5
#define T_PROTOCOL	6
#define T_AXHDR		7
#define T_ADDR		8
#define T_IPHDR		9
#define T_TCPHDR	10
#define T_ROSEHDR	11
#define T_TIMESTAMP	12
#define T_FLEXNET       13
#define T_OPENTRAC	14

/* In utils.c */
extern int color;			/* Colorized mode */
extern int sevenbit;			/* Are we on a 7-bit terminal? */
extern int ibmhack;			/* IBM mapping? */

extern int timestamp;

void display_timestamp(void);
	
void lprintf(int dtype, char *fmt, ...);
int  initcolor(void);
char *servname(int port, char *proto);

/* In listen.c */
void data_dump(unsigned char *, int, int);
int  get16(unsigned char *);
int  get32(unsigned char *);

/* In kissdump.c */
void ki_dump(unsigned char *, int, int);

/* ax25dump.c */
void ax25_dump(unsigned char *, int, int);
char *pax25(char *, unsigned char *);

/* In nrdump.c */
void netrom_dump(unsigned char *, int, int, int);

/* In arpdump.c */
void arp_dump(unsigned char *, int);

/* In ipdump.c */
void ip_dump(unsigned char *, int, int);

/* In icmpdump.c */
void icmp_dump(unsigned char *, int, int);

/* In udpdump.c */
void udp_dump(unsigned char *, int, int);

/* In tcpdump.c */
void tcp_dump(unsigned char *, int, int);

/* In rspfdump.c */
void rspf_dump(unsigned char *, int);

/* In ripdump.c */
void rip_dump(unsigned char *, int);

/* In rosedump.c */
void rose_dump(unsigned char *, int, int);

/* In flexnetdump.c */
void flexnet_dump(unsigned char *, int, int);

/* In opentracdump.c */
void opentrac_dump(unsigned char *, int, int);
