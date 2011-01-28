#ifndef	TRUE
#define	TRUE	1
#endif
#ifndef	FALSE
#define	FALSE	0
#endif

#define	RIP_PORT	520

#define	RIPCMD_REQUEST	1
#define	RIPCMD_RESPONSE	2

#define	RIP98_INFINITY	16

#define	RIP_VERSION_98	98

#define	RIP_AF_INET	2

#define	RIP98_HEADER	4
#define	RIP98_ENTRY	6

#define	RIP98_MAX_FRAME	30

struct route_struct {
	struct route_struct *next;
	struct in_addr addr;
	int bits;
	int metric;
#define	ORIG_ROUTE	0
#define	FIXED_ROUTE	1
#define	NEW_ROUTE	2
#define	DEL_ROUTE	3
	int action;
};

extern struct route_struct *first_route;

struct dest_struct {
	struct in_addr dest_addr;
};

extern struct dest_struct dest_list[];

extern int dest_count;

extern int debug;
extern int restrict;
extern int logging;

/* In rip98d.c */
extern unsigned int mask2bits(unsigned long int);
extern unsigned long int bits2mask(unsigned int);

/* In rip98t.c */
extern void transmit_routes(int);

/* In rip98r.c */
extern void receive_routes(int);
