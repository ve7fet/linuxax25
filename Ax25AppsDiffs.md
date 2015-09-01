# Introduction #

When you align the source trees of each package and run a diff, these are the results as of November 2014.


# Details #

The biggest change in this package is to the way ax25ipd operates. This
version supports dynamic DNS hosts, so there are a lot of changes to the
code to support that.

There are a couple of bug fixes that are also noted.

We reverted a patch made to ax25ipd.c (commented it out). This change isn't needed when using Unix98 pty's, and in fact breaks logging when ax25ipd is run as a daemon.

```
diff -rub ./ve7fet/ax25apps/ax25ipd/ax25ipd.c ./official/ax25-apps/ax25ipd/ax25ipd.c
--- ./ve7fet/ax25apps/ax25ipd/ax25ipd.c 2014-11-09 08:44:29.662524562 -0800
+++ ./official/ax25-apps/ax25ipd/ax25ipd.c      2014-11-09 08:42:37.924975170 -0800
@@ -155,16 +155,14 @@
        /* we need to close stdin, stdout, stderr: because otherwise
         * scripting like ttyname=$(ax25ipd | tail -1) does not work
         */
-/* Commenting this out (with notes this time) as it isn't needed for Unix98
-*  pty's and breaks logging when ax25ipd is run as a daemon - VE7FET
-*      if (!isatty(1)) {
-*              fflush(stdout);
-*              fflush(stderr);
-*              close(0);
-*              close(1);
-*              close(2);
-*      }
-*/
+       if (!isatty(1)) {
+               fflush(stdout);
+               fflush(stderr);
+               close(0);
+               close(1);
+               close(2);
+       }
+
        /* and let the games begin */
        io_start();
```

There are some significant changes to ax25ipd.h (which affect the ax25ipd daemon).
Our version has support for dynamic dns hosts that was added by Steve Fraser in 2005.
We also left the changelog intact to show this.

```
diff -rub ./ve7fet/ax25apps/ax25ipd/ax25ipd.h ./official/ax25-apps/ax25ipd/ax25ipd.h
--- ./ve7fet/ax25apps/ax25ipd/ax25ipd.h 2014-11-09 08:44:29.658524362 -0800
+++ ./official/ax25-apps/ax25ipd/ax25ipd.h      2014-11-09 08:42:37.924975170 -0800
@@ -25,29 +25,6 @@
  * Terry Dawson, VK2KTJ, September 2001.
  */

-/*
-  Version 1.0.5
-  added provision for dynamic dns hosts
-  Steve Fraser vk5asf, June 2005
-*/
-
-/*
-  Version 1.0.6
-
-
-*/
-
-/*
-  Version 1.1.0
-  added support for baudrates 57600 and 115200
-  Lee Woldanski ve7fet, Jan 2011
-*/
-
-/*
-  Version 1.1.1
-  added syslog changes from upstream source per Ralf Baechle
-  Lee Woldanski ve7fet, Nov 2014
-
 /* Define the current version number
  *
  * The first digit represents the major release (0 is a prototype release)
@@ -60,7 +37,7 @@
  *
  */

-#define VERS2 "Version 1.1.1"
+#define VERS2 "Version 1.0.2"

 #define IPPROTO_AX25 93
 #define DEFAULT_UDP_PORT 10093
@@ -143,11 +120,6 @@

 #define        AXRT_BCAST      1
 #define        AXRT_DEFAULT    2
-#define        AXRT_PERMANENT  4
-#define        AXRT_LEARN      8
-
-
-#define IPSTORAGESIZE 6 /* Bytes needed for call_to_ip */

 /* start external prototypes */
 /* end external prototypes */
@@ -163,16 +135,12 @@

 /* routing.c */
 void route_init(void);
-void route_add(char *, unsigned char *, unsigned char *, int, unsigned int);
-int route_ipmatch(struct sockaddr_in *, unsigned char *);
-unsigned char *retrieveip(unsigned char *, unsigned char *);
-void route_process(struct sockaddr_in *, unsigned char *);
+void route_add(unsigned char *, unsigned char *, int, unsigned int);
 void bcast_add(unsigned char *);
-unsigned char *call_to_ip(unsigned char *, unsigned char *);
+unsigned char *call_to_ip(unsigned char *);
 int is_call_bcast(unsigned char *);
 void send_broadcast(unsigned char *, int);
 void dump_routes(void);
-void update_dns(unsigned);

 /* config.c */
 void config_init(void);
@@ -185,13 +153,11 @@
 /* process.c */
 void process_init(void);
 void from_kiss(unsigned char *, int);
-void from_ip(unsigned char *, int, struct sockaddr_in *);
+void from_ip(unsigned char *, int);
 /* void do_broadcast(void);  where did this go ?? xxx */
 void do_beacon(void);
 int addrmatch(unsigned char *, unsigned char *);
-int addrcompare(unsigned char *, unsigned char *);
 unsigned char *next_addr(unsigned char *);
-unsigned char *from_addr(unsigned char *);
 void add_crc(unsigned char *, int);
 void dump_ax25frame(char *, unsigned char *, int);
```

The changes to config.c also relate to the dynamic DNS host addressing, I believe.

```
diff -rub ./ve7fet/ax25apps/ax25ipd/config.c ./official/ax25-apps/ax25ipd/config.c
--- ./ve7fet/ax25apps/ax25ipd/config.c  2014-11-09 08:44:29.658524362 -0800
+++ ./official/ax25-apps/ax25ipd/config.c       2014-11-09 08:42:37.924975170 -0800
@@ -292,8 +292,6 @@
                return 0;

        } else if (strcmp(p, "route") == 0) {
-               char *thost;
-
                uport = 0;
                flags = 0;

@@ -307,24 +305,14 @@
                q = strtok(NULL, " \t\n\r");
                if (q == NULL)
                        return -1;
-               thost = strdup(q);
-               j = inet_addr(q);
-               if (j != -1){ /* maybe user specified a numeric addr? */
-                       flags |= AXRT_PERMANENT; /* Prevent useless dns check on dotted ip */
-                       memcpy(tip, (char *) &j, 4);
-                       if (!j){/* IP of 0.0.0.0. Learn from incoming packets */
-                               flags |= AXRT_LEARN;
-                       }
-               }
-               else {
                        he = gethostbyname(q);
                        if (he != NULL) {
                                memcpy(tip, he->h_addr_list[0], 4);
-                       } else {
-                               j = inet_addr("0.0.0.0");/* try to rescue this idea of checking later.*/
-                               memcpy(tip,  (char *) &j, 4);
-                               fprintf(stderr,"ax25ipd: %s host IP address unknown - will probe it again l
ater\n",thost);
-                       }
+               } else {        /* maybe user specified a numeric addr? */
+                       j = inet_addr(q);
+                       if (j == -1)
+                               return -5;      /* if -1, bad deal! */
+                       memcpy(tip, &j, 4);
                }

                 if (my_udp)
@@ -359,19 +347,9 @@
                                if (strchr(q, 'd')) {
                                        flags |= AXRT_DEFAULT;
                                }
-
-                               /* Test for "permanent" flag */
-                               if (strchr(q, 'p')) {
-                                       flags |= AXRT_PERMANENT;
-                               }
-                               /* Test for "learn" flag */
-                               if (strchr(q, 'l')) {
-                                       flags |= AXRT_LEARN;
-                               }
                        }
                }
-               route_add(thost, tip, tcall, uport, flags);
-               free(thost);
+               route_add(tip, tcall, uport, flags);
                return 0;

        } else if (strcmp(p, "broadcast") == 0) {
```

The changes in io.c are also due to dynamic DNS, as well as adding support for higher serial baud rates.

```
diff -rub ./ve7fet/ax25apps/ax25ipd/io.c ./official/ax25-apps/ax25ipd/io.c
--- ./ve7fet/ax25apps/ax25ipd/io.c      2014-11-09 08:44:29.662524562 -0800
+++ ./official/ax25-apps/ax25ipd/io.c   2014-11-09 08:42:37.924975170 -0800
@@ -281,14 +281,6 @@
                baudrate = EXTB;
 #endif /* EXTB */
 #endif /* B38400 */
-#ifdef B57600
-       else if (ttyspeed == 57600)
-               baudrate = B57600;
-#endif /* B57600 */
-#ifdef B115200
-       else if (ttyspeed == 115200)
-               baudrate = B115200;
-#endif /* B115200 */
        else
                baudrate = B9600;

@@ -386,11 +378,10 @@
                if (nb == 0) {
                        fflush(stdout);
                        fflush(stderr);
-                       update_dns(60*60); // Once/hour
                        /* just so we go back to the top of the loop! */
                        continue;
                }
-               update_dns(60*60); // once/hour
+
                if (FD_ISSET(ttyfd, &readfds)) {
                        do {
                                n = read(ttyfd, buf, MAX_FRAME);
@@ -429,7 +420,7 @@
                                      ntohs(from.  sin_port), n);
                                stats.udp_in++;
                                if (n > 0)
-                                       from_ip(buf, n, &from);
+                                       from_ip(buf, n);
                        }
                }
                /* if udp_mode */
@@ -446,7 +437,7 @@
                                      inet_ntoa(from.  sin_addr), n, hdr_len);
                                stats.ip_in++;
                                if (n > hdr_len)
-                                       from_ip(buf + hdr_len, n - hdr_len, &from);
+                                       from_ip(buf + hdr_len, n - hdr_len);
                        }
 #ifdef USE_ICMP
                        if (FD_ISSET(icmpsock, &readfds)) {
@@ -474,9 +465,6 @@

        if (l <= 0)
                return;
-       if (* (unsigned *) targetip == 0){ /* If the ip is set to 0 don't send anything. I'm not sure what
sending to 0 does, but I don't like the idea. */
-               return;
-       }
        memcpy(&to.sin_addr, targetip, 4);
        memcpy(&to.sin_port, &targetip[4], 2);
        LOGL4("sendipdata to=%s %s %d l=%d\n", inet_ntoa(to.  sin_addr),
```

This patch to kiss.c should allow ax25ipd to better handle control
bytes per Debian [Bug #606338](https://code.google.com/p/linuxax25/issues/detail?id=606338).

```
diff -rub ./ve7fet/ax25apps/ax25ipd/kiss.c ./official/ax25-apps/ax25ipd/kiss.c
--- ./ve7fet/ax25apps/ax25ipd/kiss.c    2014-11-09 08:44:29.662524562 -0800
+++ ./official/ax25-apps/ax25ipd/kiss.c 2014-11-09 08:42:37.924975170 -0800
@@ -70,7 +70,7 @@
                if (c == FEND) {
                        if (ifcount > 0) {
                                /* Make sure that the control byte is zero */
-                               if ((*iframe & 0xf) == '\0') {
+                               if (*iframe == '\0' || *iframe == 0x10) {
                                        /* Room for CRC in buffer? */
                                        if (ifcount < (MAX_FRAME - 2)) {
                                                stats.kiss_in++;
```

More changes to process.c in relation to dynamic DNS handling. Also a couple of patches to make logging less verbose and noisy.

```
diff -rub ./ve7fet/ax25apps/ax25ipd/process.c ./official/ax25-apps/ax25ipd/process.c
--- ./ve7fet/ax25apps/ax25ipd/process.c 2014-11-09 08:44:29.658524362 -0800
+++ ./official/ax25-apps/ax25ipd/process.c      2014-11-09 08:42:37.924975170 -0800
@@ -68,7 +68,6 @@
 void from_kiss(unsigned char *buf, int l)
 {
        unsigned char *a, *ipaddr;
-        unsigned char ipstorage[IPSTORAGESIZE];// Provide storage for the IP.  To keep all the locking stu
ff centralized.

        if (l < 15) {
                LOGL2("from_kiss: dumped - length wrong!\n");
@@ -76,7 +75,7 @@
                return;
        }

-       if (loglevel > 3)
+       if (loglevel > 2)
                dump_ax25frame("from_kiss: ", buf, l);

        if (digi) {             /* if we are in digi mode */
@@ -106,7 +105,7 @@
        }                       /* end of tnc mode */

        /* Lookup the IP address for this route */
-       ipaddr = call_to_ip(a, ipstorage);
+       ipaddr = call_to_ip(a);

        if (ipaddr == NULL) {
                if (is_call_bcast(a)) {
@@ -147,11 +146,10 @@
  * We simply send the packet to the KISS send routine.
  */

-void from_ip(unsigned char *buf, int l, struct sockaddr_in *ip_addr)
+void from_ip(unsigned char *buf, int l)
 {
        int port = 0;
        unsigned char *a;
-       unsigned char *f;

        if (!ok_crc(buf, l)) {
                stats.ip_failed_crc++;
@@ -166,7 +164,7 @@
                return;
        }

-       if (loglevel > 3)
+       if (loglevel > 2)
                dump_ax25frame("from_ip: ", buf, l);

        if (digi) {             /* if we are in digi mode */
@@ -196,10 +194,6 @@
                }
 #endif
        }                       /* end of tnc mode */
-
-       f = from_addr(buf);
-       route_process(ip_addr, f);
-
        if (!ttyfd_bpq)
                send_kiss(port, buf, l);
        else {
@@ -283,31 +277,6 @@
 }

 /*
- * return 0 if the addresses supplied match
- * return a positive or negative value otherwise
- */
-int addrcompare(unsigned char *a, unsigned char *b)
-{
-       signed char diff;
-
-       if ((diff = (signed char )((*a++ - *b++) & 0xfe)))
-               return diff;    /* "K" */
-       if ((diff = (signed char )((*a++ - *b++) & 0xfe)))
-               return diff;    /* "A" */
-       if ((diff = (signed char )((*a++ - *b++) & 0xfe)))
-               return diff;    /* "9" */
-       if ((diff = (signed char )((*a++ - *b++) & 0xfe)))
-               return diff;    /* "W" */
-       if ((diff = (signed char )((*a++ - *b++) & 0xfe)))
-               return diff;    /* "S" */
-       if ((diff = (signed char )((*a++ - *b++) & 0xfe)))
-               return diff;    /* "B" */
-       if ((diff = (signed char )((*a++ - *b++) & 0xfe)))
-               return diff;    /* ssid */
-       return 0;
-}
-
-/*
  * return pointer to the next station to get this packet
  */
 unsigned char *next_addr(unsigned char *f)
@@ -332,32 +301,6 @@
 }

 /*
- * return pointer to the last station this packet came from
- */
-unsigned char *from_addr(unsigned char *f)
-{
-       unsigned char *a;
-
-       a = f + 7;
-/* If no digis, return the source address */
-       if (NO_DIGIS(f))
-               return a;
-
-/* check each digi field. Go to last that has seen it */
-       do
-               a += 7;
-       while (NOT_LAST(a) && REPEATED(a));
-
-/* in DIGI mode: we have set REPEATED already, so the one before is it */
-       if (digi || !REPEATED(a))
-               a -= 7;
-
-/* in TNC mode: always the last is it */
-       return a;
-}
-
-
-/*
  * tack on the CRC for the frame.  Note we assume the buffer is long
  * enough to have the two bytes tacked on.
  */
```

Changes to routing.c to support dynamic DNS.

```
diff -rub ./ve7fet/ax25apps/ax25ipd/routing.c ./official/ax25-apps/ax25ipd/routing.c
--- ./ve7fet/ax25apps/ax25ipd/routing.c 2014-11-09 08:44:29.658524362 -0800
+++ ./official/ax25-apps/ax25ipd/routing.c      2014-11-09 08:42:37.924975170 -0800
@@ -12,8 +12,6 @@
 #include <netinet/in.h>
 #include <memory.h>
 #include <syslog.h>
-#include <string.h>
-#include <pthread.h>

 /* The routing table structure is not visible outside this module. */

@@ -28,7 +26,6 @@
        unsigned char pad1;
        unsigned char pad2;
        unsigned int flags;     /* route flags */
-       char *hostnm; /* host name */
        struct route_table_entry *next;
 };

@@ -44,31 +41,16 @@

 struct bcast_table_entry *bcast_tbl;

-struct callsign_lookup_entry {
-       unsigned char callsign[7];
-       struct route_table_entry *route;
-       struct callsign_lookup_entry *prev, *next;
-};
-
-struct callsign_lookup_entry *callsign_lookup;
-
-time_t last_dns_time;
-volatile int threadrunning;
-pthread_mutex_t dnsmutex = PTHREAD_MUTEX_INITIALIZER;
-
 /* Initialize the routing module */
 void route_init(void)
 {
        route_tbl = NULL;
        default_route = NULL;
        bcast_tbl = NULL;
-       callsign_lookup = NULL;
-       last_dns_time = time(NULL);
-       threadrunning = 0;
 }

 /* Add a new route entry */
-void route_add(char *host, unsigned char *ip, unsigned char *call, int udpport,
+void route_add(unsigned char *ip, unsigned char *call, int udpport,
        unsigned int flags)
 {
        struct route_table_entry *rl, *rn;
@@ -96,7 +78,6 @@
                rn->callsign[i] = call[i] & 0xfe;
        rn->callsign[6] = (call[6] & 0x1e) | 0x60;
        rn->padcall = 0;
-       rn->hostnm = strdup(host);
        memcpy(rn->ip_addr, ip, 4);
        rn->udp_port = htons(udpport);
        rn->pad1 = 0;
@@ -122,130 +103,6 @@
        return;
 }

-/* For route rn, a different IP is being used.  Trigger a DNS check.
- * as long as DNS hasn't been checked within 5 minutes (300 seconds)
- * */
-void route_updatedyndns(struct sockaddr_in *ip_addr, struct route_table_entry *rn)
-{
-       if (rn->flags & AXRT_LEARN){
-               pthread_mutex_lock(&dnsmutex);
-               * (unsigned *) rn->ip_addr = (unsigned) ip_addr->sin_addr.s_addr;
-               pthread_mutex_unlock(&dnsmutex);
-       }
-       if (!(rn->flags & AXRT_PERMANENT)){
-               LOGL4("received changed ip: %s", call_to_a(rn->callsign));
-               update_dns(300);
-       }
-}
-
-/* Save the calls in a binary tree.  This code links new callsigns
- * back to an existing route.  No new routes are created. */
-void route_updatereturnpath(unsigned char *mycall, struct route_table_entry *rn)
-{
-       struct callsign_lookup_entry **clookupp = &callsign_lookup;
-       struct callsign_lookup_entry *clookup = callsign_lookup;
-       for (;;){
-               int chk;
-               if (!clookup){
-                       clookup = *clookupp = calloc(sizeof(struct callsign_lookup_entry), 1);
-                       memcpy(clookup->callsign, mycall, 7);
-                       LOGL4("added return route: %s %s %s %d\n",
-                               call_to_a(mycall),
-                               inet_ntoa(rn->ip_addr_in),
-                               rn->udp_port ? "udp" : "ip", ntohs(rn->udp_port));
-
-                       clookup->route = rn;
-                       return;
-               }
-               chk = addrcompare(mycall, clookup->callsign);
-               if (chk > 0){
-                       clookupp = &clookup->next;
-                       clookup = *clookupp;
-               }
-               else if (chk < 0){
-                       clookupp = &clookup->prev;
-                       clookup = *clookupp;
-                       }
-               else{
-
-                       if (clookup->route != rn){
-                               clookup->route = rn;
-                       }
-                       return;
-               }
-       }
-
-
-}
-
-
-/* Compare ip_addr to the format used by route_table_entry */
-int route_ipmatch(struct sockaddr_in *ip_addr, unsigned char *routeip)
-{
-       unsigned char ipstorage[IPSTORAGESIZE];
-       return (unsigned) ip_addr->sin_addr.s_addr == * (unsigned *) retrieveip(routeip, ipstorage);
-}
-
-
-/* Process calls and ip addresses for routing.  The idea behind
- * this code that for the routes listed in the ax25ipd.conf file,
- * only DNS or the given IP's should be used.  But for calls
- * that are not referenced in ax25ipd.conf that arrive using
- * a known gateway IP, the code links them back to that gateway.
- *
- * No new routes are created.  If a callsign comes in from an
- * unknown gateway, it will not create an entry.
- *
- * if the Callsign for a known route is received, and it does
- * not match the current ip for that route, the update_dns code
- * is triggered, which should fix the IP based on DNS.
- *
- * */
-void route_process(struct sockaddr_in *ip_addr, unsigned char *call)
-{
-       struct route_table_entry *rp;
-       unsigned char mycall[7];
-       int i;
-       int isroute;
-
-       if (call == NULL)
-               return;
-
-       for (i = 0; i < 6; i++)
-               mycall[i] = call[i] & 0xfe;
-
-       mycall[6] = (call[6] & 0x1e) | 0x60;
-
-       rp = route_tbl;
-       isroute = 0;
-       while (rp) {
-               if (addrmatch(mycall, rp->callsign)) {
-                       if (!route_ipmatch(ip_addr, rp->ip_addr)) {
-                               route_updatedyndns(ip_addr, rp);
-                               }
-                       isroute = 1;
-               }
-               rp = rp->next;
-       }
-
-       // Don't use any of the ip lookup code for the routes.
-       // Also, do not set return paths for broadcast callsigns.
-       // Not sure if this ever happens.  But it's no good if it does.
-       if (!isroute && !is_call_bcast(call)){
-               rp = route_tbl;
-               while (rp) {
-                       // Only do this once.
-                       if (route_ipmatch(ip_addr, rp->ip_addr)) {
-                               route_updatereturnpath(mycall, rp);
-                               break;
-                       }
-                       rp = rp->next;
-               }
-       }
-
-}
-
-
 /* Add a new broadcast address entry */
 void bcast_add(unsigned char *call)
 {
@@ -282,34 +139,16 @@
 }

 /*
- * It's possible that the thread will update the IP in mid
- * memcpy of an ip address, so I changed all the references to
- * route_table_entry->ip_addr to use this routine
-
- * Note that the printfs don't check this
- */
-
-unsigned char *retrieveip(unsigned char *ip, unsigned char *ipstorage)
-{
-       if (!ipstorage)return ip;
-       pthread_mutex_lock(&dnsmutex);
-       memcpy((void *) ipstorage, (void *) ip, IPSTORAGESIZE);
-       pthread_mutex_unlock(&dnsmutex);
-       return ipstorage;
-}
-
-/*
  * Return an IP address and port number given a callsign.
  * We return a pointer to the address; the port number can be found
  * immediately following the IP address. (UGLY coding; to be fixed later!)
  */

-unsigned char *call_to_ip(unsigned char *call, unsigned char *ipstorage)
+unsigned char *call_to_ip(unsigned char *call)
 {
        struct route_table_entry *rp;
        unsigned char mycall[7];
        int i;
-       struct callsign_lookup_entry *clookup;

        if (call == NULL)
                return NULL;
@@ -326,34 +165,11 @@
                if (addrmatch(mycall, rp->callsign)) {
                        LOGL4("found ip addr %s\n",
                              inet_ntoa(rp->ip_addr_in));
-                       return retrieveip(rp->ip_addr, ipstorage);
+                       return rp->ip_addr;
                }
                rp = rp->next;
        }

-       /* Check for callsigns that have been heard on known routes */
-       clookup = callsign_lookup;
-       for (;;){
-               int chk;
-               if (!clookup){
-                       break;
-               }
-               chk = addrcompare(mycall, clookup->callsign);
-               if (chk > 0){
-                       clookup = clookup->next;
-               }
-               else if (chk < 0){
-                       clookup = clookup->prev;
-               }
-               else{
-                       LOGL4("found cached ip addr %s\n",
-                             inet_ntoa(clookup->route->ip_addr_in));
-
-                       return retrieveip(clookup->route->ip_addr, ipstorage);
-               }
-       }
-
-
        /*
         * No match found in the routing table, use the default route if
         * we have one defined.
@@ -361,7 +177,7 @@
        if (default_route) {
                LOGL4("failed, using default ip addr %s\n",
                      inet_ntoa(default_route->ip_addr_in));
-               return retrieveip(default_route->ip_addr, ipstorage);
+               return default_route->ip_addr;
        }

        LOGL4("failed.\n");
@@ -408,9 +224,7 @@
        rp = route_tbl;
        while (rp) {
                if (rp->flags & AXRT_BCAST) {
-                       unsigned char ipstorage[IPSTORAGESIZE];
-
-                       send_ip(buf, l, retrieveip(rp->ip_addr, ipstorage));
+                       send_ip(buf, l, rp->ip_addr);
                }
                rp = rp->next;
        }
@@ -429,7 +243,7 @@

        rp = route_tbl;
        while (rp) {
-               LOGL1("  %s %s %s %d %d\n",
+               LOGL1("  %s\t%s\t%s\t%d\t%d\n",
                      call_to_a(rp->callsign),
                      inet_ntoa(rp->ip_addr_in),
                      rp->udp_port ? "udp" : "ip",
@@ -438,60 +252,3 @@
        }
        fflush(stdout);
 }
-
-/* Update the IPs of DNS entries for all routes */
-void *update_dnsthread(void *arg)
-{
-       struct hostent *he;
-       struct route_table_entry *rp;
-
-       rp = route_tbl;
-       while (rp) {
-               // If IP is 0,  DNS lookup failed at startup.
-               // So check DNS even though it's permanent.
-               // I think checking the lock on this ip_addr reference
-               // isn't needed since the main code doesn't change
-               // ip_addr after startup, and worst case is that it
-               // does one extra dns check
-               if ((rp->hostnm[0])  &&
-                         (!(rp->flags & AXRT_PERMANENT) || ( * (unsigned *) rp->ip_addr == 0) )){
-                       LOGL4("Checking DNS for %s\n", rp->hostnm);
-                       he = gethostbyname(rp->hostnm);
-                       if (he != NULL) {
-                               pthread_mutex_lock(&dnsmutex);
-                               * (unsigned *) rp->ip_addr = * (unsigned *) he->h_addr_list[0];
-                               pthread_mutex_unlock(&dnsmutex);
-                               LOGL4("DNS returned IP=%s\n", inet_ntoa(rp->ip_addr_in));
-                       }
-               }
-               rp = rp->next;
-       }
-       threadrunning = 0;
-       pthread_exit(0);
-       return 0;
-}
-
-/* check DNS for route IPs.  Packets from system whose ip
- * has changed  trigger a DNS lookup. The
- * the timer is needed when both systems are on dynamic
- * IPs and they both reset at about the same time.  Also
- * when the IP changes immediately, but DNS lags.
- */
-void update_dns(unsigned wait)
-{
-       pthread_t dnspthread;
-       int rc;
-
-       if (threadrunning)return;// Don't start a thread when one is going already.
-       if (wait && (time(NULL) < last_dns_time + wait))return;
-       threadrunning = 1;
-       LOGL4("Starting DNS thread\n");
-       last_dns_time = time(NULL);
-       rc = pthread_create(&dnspthread, NULL, update_dnsthread,  (void *) 0);
-       if (rc){
-               LOGL1("    Thread err%d\n", rc); // Should we exit here?
-               threadrunning = 0;
-       }
-
-}
-
```

Patch listen to remove unnecessary data dump for ROSE CALL\_REQUEST and
CLEAR\_REQUEST frames, per F6BVP.

```
diff -rub ./ve7fet/ax25apps/listen/rosedump.c ./official/ax25-apps/listen/rosedump.c
--- ./ve7fet/ax25apps/listen/rosedump.c 2014-11-09 08:44:29.378510434 -0800
+++ ./official/ax25-apps/listen/rosedump.c      2014-11-09 08:42:37.932975567 -0800
@@ -71,6 +71,8 @@
                        facility(data, length);
                        length -= flen;
                        data += flen;
+                       if (length > 0)
+                               data_dump(data, length, 1);
                } else {
                        lprintf(T_ROSEHDR, "\n");
                }
@@ -83,6 +85,9 @@
        case CLEAR_REQUEST:
                lprintf(T_ROSEHDR, "CLEAR REQUEST - Cause %s - Diag %d\n",
                        clear_code(data[3]), data[4]);
+               if (length > 6) {
+                       facility(data + 6, length - 6);
+               }
                return;

        case CLEAR_CONFIRMATION:
```

Patch listen to fix a segfault when listen is called with the -c (color) option. This changes a patch made in the official source that was done to fix a compiler warning. Also fix listen to print the warning to stderr when we ask for color and it isn't available

```
diff -rub ./ve7fet/ax25apps/listen/utils.c ./official/ax25-apps/listen/utils.c
--- ./ve7fet/ax25apps/listen/utils.c    2014-12-01 13:28:17.886361647 -0800
+++ ./official/ax25-apps/listen/utils.c 2014-12-01 13:31:12.436455802 -0800
@@ -81,12 +81,9 @@

 int initcolor(void)
 {
-       initscr();
-       if (has_colors() == FALSE )
-       {
-               endwin();
+       if (!has_colors())
                return 0;
-       }
+       initscr();              /* Start ncurses */
        start_color();          /* Initialize color support */
        refresh();              /* Clear screen */
        noecho();               /* Don't echo */

```


```
diff -rub ./ve7fet/ax25apps/listen/listen.c ./official/ax25-apps/listen/listen.c
--- ./ve7fet/ax25apps/listen/listen.c   2014-12-01 13:28:17.886361647 -0800
+++ ./official/ax25-apps/listen/listen.c        2014-12-01 13:31:12.436455802 -0800
@@ -138,7 +138,7 @@
        if (color) {
                color = initcolor();    /* Initialize color support */
                if (!color)
-                       fprintf(stderr, "listen: Could not initialize color support.\n");
+                       printf("Could not initialize color support.\n");
        }

        setservent(1);
```

Remove the unnecessary stuff from pathnames.h

```
diff -rub ./ve7fet/ax25apps/pathnames.h ./official/ax25-apps/pathnames.h
--- ./ve7fet/ax25apps/pathnames.h       2014-11-09 08:44:29.662524562 -0800
+++ ./official/ax25-apps/pathnames.h    2014-11-09 08:42:37.932975567 -0800
@@ -1,11 +1,37 @@
+
 #define CONF_AX25IPD_FILE              AX25_SYSCONFDIR"/ax25ipd.conf"

 #define CONF_AX25ROUTED_FILE           AX25_SYSCONFDIR"/ax25rtd.conf"
 #define DATA_AX25ROUTED_CTL_SOCK       AX25_LOCALSTATEDIR"/ax25rtd/control"
+#define        PROC_AX25_FILE          "/proc/net/ax25"
+#define PROC_AX25PORTS_FILE    "/proc/net/ax25_ports"
 #define DATA_AX25ROUTED_AXRT_FILE      AX25_LOCALSTATEDIR"/ax25rtd/ax25_route"
 #define DATA_AX25ROUTED_IPRT_FILE      AX25_LOCALSTATEDIR"/ax25rtd/ip_route"

-#define PROC_AX25_FILE                 "/proc/net/ax25"
 #define PROC_IP_ROUTE_FILE             "/proc/net/route"
+
 #define PROC_NR_NODES_FILE             "/proc/net/nr_nodes"

+#define        CONF_NODE_MOTD_FILE     AX25_SYSCONFDIR"/node.motd"
+#define        CONF_NODE_PERMS_FILE    AX25_SYSCONFDIR"/node.perms"
+#define        CONF_NODE_FILE          AX25_SYSCONFDIR"/node.conf"
+
+#define        DATA_MHEARD_FILE        AX25_LOCALSTATEDIR"/mheard/mheard.dat"
+#define        DATA_NODE_HELP_DIR      "/usr/lib/ax25/node/help/"
+#define        CONF_NODE_INFO_FILE     AX25_SYSCONFDIR"/node.info"
+
+#define        DATA_NODE_LOGIN_FILE    AX25_LOCALSTATEDIR"/node/loggedin"
+
+#define        PROC_AX25_CALLS_FILE    "/proc/net/ax25_calls"
+
+#define        MY_TALK                 "/usr/sbin/ttylinkd"
+
+#define        MAIL_DELIVERY_AGENT     "/usr/sbin/sendmail %s"
+
+#define        DATA_PMS_LOGIN_FILE     AX25_LOCALSTATEDIR"/pms/loggedin"
+
+#define        PROC_AX25_ROUTE_FILE    "/proc/net/ax25_route"
+
+#define        CONF_PMS_MOTD_FILE      AX25_SYSCONFDIR"/pms.motd"
+
+#define        CONF_PMS_INFO_FILE      AX25_SYSCONFDIR"/pms.info"
```