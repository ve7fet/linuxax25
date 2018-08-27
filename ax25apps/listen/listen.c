#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <curses.h>
#include <signal.h>
#include <errno.h>

#include <sys/socket.h>
#include <net/if.h>
#ifdef __GLIBC__
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif
#include <netax25/ax25.h>
#include <netax25/axconfig.h>

#include <config.h>
#include "listen.h"

static struct timeval t_recv;
static int tflag = 0;
static int32_t thiszone;	/* seconds offset from gmt to local time */
static int sigint;
static int sock;

static void display_port(char *dev)
{
	char *port;

	port = ax25_config_get_name(dev);
	if (port == NULL)
		port = dev;

	lprintf(T_PORT, "%s: ", port);
}

/* from tcpdump util.c */

/*
 * Format the timestamp
 */
static char * ts_format(unsigned int sec, unsigned int usec)
{
	static char buf[sizeof("00:00:00.000000")];
	unsigned int hours, minutes, seconds;

	hours = sec / 3600;
	minutes = (sec % 3600) / 60;
	seconds  = sec % 60;

	/*
	 * The real purpose of these checks is to let GCC figure out the
	 * value range of all variables thus avoid bogus warnings.  For any
	 * halfway modern GCC the checks will be optimized away.
	 */
	if (hours >= 60)
		unreachable();
	if (minutes >= 60)
		unreachable();
	if (seconds >= 60)
		unreachable();
	if (usec >= 1000000)
		unreachable();

	snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%06u",
		 hours, minutes, seconds, usec);

	return buf;
}

/*
 * Print the timestamp
 */
static void ts_print(const struct timeval *tvp)
{
        int s;
        struct tm *tm;
        time_t Time;
        static unsigned b_sec;
        static unsigned b_usec;
        int d_usec;
        int d_sec;

        switch (tflag) {

        case 0: /* Default */
                s = (tvp->tv_sec + thiszone) % 86400;
                (void)lprintf(T_TIMESTAMP, "%s ", ts_format(s, tvp->tv_usec));
                break;

        case 1: /* No time stamp */
                break;

        case 2: /* Unix timeval style */
                (void)lprintf(T_TIMESTAMP, "%u.%06u ",
                             (unsigned)tvp->tv_sec,
                             (unsigned)tvp->tv_usec);
                break;

        case 3: /* Microseconds since previous packet */
        case 5: /* Microseconds since first packet */
                if (b_sec == 0) {
                        /* init timestamp for first packet */
                        b_usec = tvp->tv_usec;
                        b_sec = tvp->tv_sec;
                }

                d_usec = tvp->tv_usec - b_usec;
                d_sec = tvp->tv_sec - b_sec;

                while (d_usec < 0) {
                    d_usec += 1000000;
                    d_sec--;
                }

                (void)lprintf(T_TIMESTAMP, "%s ", ts_format(d_sec, d_usec));

                if (tflag == 3) { /* set timestamp for last packet */
                    b_sec = tvp->tv_sec;
                    b_usec = tvp->tv_usec;
                }
                break;

        case 4: /* Default + Date*/
                s = (tvp->tv_sec + thiszone) % 86400;
                Time = (tvp->tv_sec + thiszone) - s;
                tm = gmtime (&Time);
                if (!tm)
                        lprintf(T_TIMESTAMP, "Date fail  ");
                else
                        lprintf(T_TIMESTAMP, "%04d-%02d-%02d %s ",
                               tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
                               ts_format(s, tvp->tv_usec));
                break;
        }
}

void display_timestamp(void)
{
	ts_print(&t_recv);
}

/* from tcpdump gmtlocal.c */

static int32_t gmt2local(time_t t)
{
        int dt, dir;
        struct tm *gmt, *loc;
        struct tm sgmt;

        if (t == 0)
                t = time(NULL);
        gmt = &sgmt;
        *gmt = *gmtime(&t);
        loc = localtime(&t);
        dt = (loc->tm_hour - gmt->tm_hour) * 60 * 60 +
            (loc->tm_min - gmt->tm_min) * 60;

        /*
         * If the year or julian day is different, we span 00:00 GMT
         * and must add or subtract a day. Check the year first to
         * avoid problems when the julian day wraps.
         */
        dir = loc->tm_year - gmt->tm_year;
        if (dir == 0)
                dir = loc->tm_yday - gmt->tm_yday;
        dt += dir * 24 * 60 * 60;

        return dt;
}

static void handle_sigint(int signal)
{
	sigint++;
	close(sock);	/* disturb blocking recvfrom  */
	sock = -1;
}

#define ASCII		0
#define HEX 		1
#define READABLE	2

#define BUFSIZE		1500

int main(int argc, char **argv)
{
	unsigned char buffer[BUFSIZE];
	int dumpstyle = ASCII;
	int size;
	int s;
	char *port = NULL, *dev = NULL;
	struct sockaddr sa;
	socklen_t asize = sizeof(sa);
	struct ifreq ifr;
	int proto = ETH_P_AX25;
	int exit_code = EXIT_SUCCESS;

	while ((s = getopt(argc, argv, "8achip:rtv")) != -1) {
		switch (s) {
		case '8':
			sevenbit = 0;
			break;
		case 'a':
			proto = ETH_P_ALL;
			break;
		case 'c':
			color = 1;
			break;
		case 'h':
			dumpstyle = HEX;
			break;
		case 'i':
			ibmhack = 1;
			break;
		case 'p':
			port = optarg;
			break;
		case 'r':
			dumpstyle = READABLE;
			break;
		case 't':
			tflag++;
			break;
		case 'v':
			printf("listen: %s\n", VERSION);
			return 0;
		case ':':
			fprintf(stderr,
				"listen: option -p needs a port name\n");
			return 1;
		case '?':
			fprintf(stderr,
				"Usage: listen [-8] [-a] [-c] [-h] [-i] [-p port] [-r] [-t..] [-v]\n");
			return 1;
		}
	}

	switch (tflag) {
	case 0: /* Default */
	case 4: /* Default + Date*/
		thiszone = gmt2local(0);
		break;
	case 1: /* No time stamp */
	case 2: /* Unix timeval style */
	case 3: /* Microseconds since previous packet */
	case 5: /* Microseconds since first packet */
		break;
	default: /* Not supported */
		fprintf(stderr, "listen: only -t, -tt, -ttt, -tttt and -ttttt are supported\n");
		return 1;
        }

	if (ax25_config_load_ports() == 0)
		fprintf(stderr, "listen: no AX.25 port data configured\n");

	if (port != NULL) {
		dev = ax25_config_get_dev(port);
		if (dev == NULL) {
			fprintf(stderr, "listen: invalid port name - %s\n",
				port);
			return 1;
		}
	}

	sock = socket(PF_PACKET, SOCK_PACKET, htons(proto));
	if (sock == -1) {
		perror("socket");
		return 1;
	}

	if (color) {
		color = initcolor();	/* Initialize color support */
		if (!color)
			fprintf(stderr, "listen: Could not initialize color support.\n");
	}

	setservent(1);

	while (!sigint) {
		asize = sizeof(sa);

		signal(SIGINT, handle_sigint);
		signal(SIGTERM, handle_sigint);
		size = recvfrom(sock, buffer, sizeof(buffer), 0, &sa, &asize);
		if (size == -1) {
			/*
			 * Signals are cared for by the handler, and we
			 * don't want to abort on SIGWINCH
			 */
			if (errno == EINTR) {
				refresh();
				continue;
			} else if (!(errno == EBADF && sigint)) {
				perror("recv");
				exit_code = errno;
			}
			break;
		}
		gettimeofday(&t_recv, NULL);
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		if (sock == -1 || sigint)
			break;

		if (dev != NULL && strcmp(dev, sa.sa_data) != 0)
			continue;

		if (proto == ETH_P_ALL) {
			strcpy(ifr.ifr_name, sa.sa_data);
			signal(SIGINT, handle_sigint);
			signal(SIGTERM, handle_sigint);
			if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
				if (!(errno == EBADF && sigint)) {
					perror("SIOCGIFHWADDR");
					exit_code = errno;
					break;
				}
			}
			signal(SIGINT, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			if (sock == -1 || sigint)
				break;
			if (ifr.ifr_hwaddr.sa_family == AF_AX25) {
                                if (size > 2 && *buffer == 0xcc) {
                                        /* IP packets from the ax25 de-segmenter
                                           are seen on socket "PF_PACKET,
                                           SOCK_PACKET, ETH_P_ALL" without
                                           AX.25 header (just the IP-frame),
                                           prefixed by 0xcc (AX25_P_IP).
                                           It's unclear why in the kernel code
                                           this happens (unsegmentet AX25 PID
                                           AX25_P_IP have not this behavior).
                                           We have already displayed all the
                                           segments and like to ignore this
                                           data.
                                           AX.25 packets start with a kiss
                                           byte (buffer[0]); ax25_dump()
                                           looks for it.
                                           There's no kiss command 0xcc
                                           defined; kiss bytes are checked
                                           against & 0xf (= 0x0c), which is
                                           also not defined.
                                           Kiss commands may have one argument.
                                           => We can make safely make the
                                           assumption for first byte == 0xcc
                                           and length > 2, that we safeley can
                                           detect those IP frames, and then
                                           ignore it.
                                         */
                                        continue;
                                }
				display_port(sa.sa_data);
				ki_dump(buffer, size, dumpstyle);
/*				lprintf(T_DATA, "\n");  */
			}
		} else {
			display_port(sa.sa_data);
			ki_dump(buffer, size, dumpstyle);
/*                      lprintf(T_DATA, "\n");  */
		}
		if (color)
			refresh();
	}
	if (color)
		endwin();

	return exit_code;
}

static void ascii_dump(unsigned char *data, int length)
{
	char c;
	int i, j;
	char buf[100];

	for (i = 0; length > 0; i += 64) {
		sprintf(buf, "%04X  ", i);

		for (j = 0; j < 64 && length > 0; j++) {
			c = *data++;
			length--;

			if ((c != '\0') && (c != '\n'))
				strncat(buf, &c, 1);
			else
				strcat(buf, ".");
		}

		lprintf(T_DATA, "%s\n", buf);
	}
}

static void readable_dump(unsigned char *data, int length)
{
	unsigned char c;
	int i;
	int cr = 1;
	char buf[BUFSIZE];

	for (i = 0; length > 0; i++) {

		c = *data++;
		length--;

		switch (c) {
		case 0x00:
			buf[i] = ' ';
		case 0x0A:	/* hum... */
		case 0x0D:
			if (cr)
				buf[i] = '\n';
			else
				i--;
			break;
		default:
			buf[i] = c;
		}
		cr = (buf[i] != '\n');
	}
	if (cr)
		buf[i++] = '\n';
	buf[i] = '\0';
	lprintf(T_DATA, "%s", buf);
}

static void hex_dump(unsigned char *data, int length)
{
	unsigned char *data2;
	int i, j, length2;
	unsigned char c;

	char buf[4], hexd[49], ascd[17];

	length2 = length;
	data2 = data;

	for (i = 0; length > 0; i += 16) {

		hexd[0] = '\0';
		for (j = 0; j < 16; j++) {
			c = *data2++;
			length2--;

			if (length2 >= 0)
				sprintf(buf, "%2.2X ", c);
			else
				strcpy(buf, "   ");
			strcat(hexd, buf);
		}

		ascd[0] = '\0';
		for (j = 0; j < 16 && length > 0; j++) {
			c = *data++;
			length--;

			sprintf(buf, "%c",
				((c != '\0') && (c != '\n')) ? c : '.');
			strcat(ascd, buf);
		}

		lprintf(T_DATA, "%04X  %s | %s\n", i, hexd, ascd);
	}
}

void data_dump(void *data, int length, int dumpstyle)
{
	switch (dumpstyle) {

	case READABLE:
		readable_dump(data, length);
		break;
	case HEX:
		hex_dump(data, length);
		break;
	default:
		ascii_dump(data, length);
	}
}

int get16(unsigned char *cp)
{
	int x;

	x = *cp++;
	x <<= 8;
	x |= *cp++;

	return x;
}

int get32(unsigned char *cp)
{
	int x;

	x = *cp++;
	x <<= 8;
	x |= *cp++;
	x <<= 8;
	x |= *cp++;
	x <<= 8;
	x |= *cp;

	return x;
}

