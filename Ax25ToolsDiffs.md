# Introduction #

When you align the source trees of each package and run a diff, these are the results as of November 2014.


# Details #

Only two differences in the ax25tools package. First one is to mheard
to clean up its output. The second was just cosmetic to remove some
extra stuff from pathnames.h.

Bernard patched mheard on Aug 6, 2013. His comments: "mheard ax25tools was displaying empty data. Lets skip zero packet records".

```
diff -rub ./ve7fet/ax25tools/ax25/mheard.c ./official/ax25-tools/ax25/mheard.c
--- ./ve7fet/ax25tools/ax25/mheard.c    2014-11-09 08:44:37.958936587 -0800
+++ ./official/ax25-tools/ax25/mheard.c 2014-11-09 08:42:48.125481771 -0800
@@ -68,7 +68,6 @@
                        call =  ax25_ntoa(&pr->entry.from_call);
                        if ((s = strstr(call, "-0")) != NULL)
                                *s = '\0';
-                       if (pr->entry.count != 0)
                                printf("%-10s %-5s %5d   %s\n",
                                call, pr->entry.portname, pr->entry.count, lh);
                        break;
@@ -92,7 +91,6 @@
                        }
                        if (pr->entry.ndigis >= 4)
                                strcat(buffer, ",...");
-                       if (pr->entry.count != 0)
                                printf("%-70s %-5s\n",
                                buffer, pr->entry.portname);
                        break;
@@ -104,7 +102,6 @@
                        call = ax25_ntoa(&pr->entry.from_call);
                        if ((s = strstr(call, "-0")) != NULL)
                                *s = '\0';
-                       if (pr->entry.count != 0)
                                printf("%-10s %-5s %5d %5d %5d  %s  %s\n",
                                call, pr->entry.portname, pr->entry.iframes, pr->entry.sframes, pr->entry.u
frames, fh, lh);
                        break;
@@ -112,7 +109,6 @@
                        call = ax25_ntoa(&pr->entry.from_call);
                        if ((s = strstr(call, "-0")) != NULL)
                                *s = '\0';
-                       if (pr->entry.count != 0) {
                                printf("%-10s %-5s %5d %5s ",
                                call, pr->entry.portname, pr->entry.count, types[pr->entry.type]);
                        if (pr->entry.mode & MHEARD_MODE_ARP)
@@ -140,7 +136,6 @@
                        if (pr->entry.mode & MHEARD_MODE_UNKNOWN)
                                printf(" Unknown");
                        printf("\n");
-                       }
                        break;
        }
 }
```

We took out some extra stuff that wasn't needed in pathnames.h.

```
diff -rub ./ve7fet/ax25tools/pathnames.h ./official/ax25-tools/pathnames.h
--- ./ve7fet/ax25tools/pathnames.h      2014-11-09 08:44:38.954986045 -0800
+++ ./official/ax25-tools/pathnames.h   2014-11-09 08:42:48.137482367 -0800
@@ -13,7 +13,9 @@
 #define        DATA_MHEARD_FILE        AX25_LOCALSTATEDIR"mheard/mheard.dat"

 #define        LOCK_AXSPAWN_FILE       "/var/lock/axspawn"
+#define        LOCK_SERIAL_DIR         "/var/lock"

+#define        PROC_AX25_FILE          "/proc/net/ax25"
 #define        PROC_AX25_ROUTE_FILE    "/proc/net/ax25_route"
 #define        PROC_IP_ROUTE_FILE      "/proc/net/route"
 #define        PROC_NR_SYSCTL_DIR      "/proc/sys/net/netrom"
@@ -22,3 +24,5 @@
 #define PROC_AX25_CALLS_FILE   "/proc/net/ax25_calls"
 #define PROC_NR_NEIGH_FILE     "/proc/net/nr_neigh"
 #define PROC_NR_NODES_FILE     "/proc/net/nr_nodes"
+
+
```