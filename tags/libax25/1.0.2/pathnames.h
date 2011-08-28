/*
 * pathnames.h : Paths that are used by the library
 */

/*
 * AX25_SYSCONFDIR is defined in the Makefile!!
 */
#define	CONF_AXPORTS_FILE	AX25_SYSCONFDIR"axports"
#define	CONF_NRPORTS_FILE	AX25_SYSCONFDIR"nrports"
#define	CONF_RSPORTS_FILE	AX25_SYSCONFDIR"rsports"

#define	PROC_AX25_FILE		"/proc/net/ax25"
#define	PROC_AX25_ROUTE_FILE	"/proc/net/ax25_route"
#define	PROC_AX25_CALLS_FILE	"/proc/net/ax25_calls"

#define	PROC_NR_FILE		"/proc/net/nr"
#define	PROC_NR_NEIGH_FILE	"/proc/net/nr_neigh"
#define	PROC_NR_NODES_FILE	"/proc/net/nr_nodes"

#define	PROC_RS_FILE		"/proc/net/rose"
#define	PROC_RS_NEIGH_FILE	"/proc/net/rose_neigh"
#define	PROC_RS_NODES_FILE	"/proc/net/rose_nodes"
#define	PROC_RS_ROUTES_FILE	"/proc/net/rose_routes"

#define	LOCK_SERIAL_DIR		"/var/lock"
