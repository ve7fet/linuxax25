/*
 * mkiss.c
 * Fake out AX.25 code into supporting dual port TNCS by routing serial
 * port data to/from two pseudo ttys.
 *
 * Version 1.03
 *
 * N0BEL
 * Kevin Uhlir
 * kevinu@flochart.com
 *
 * 1.01 12/30/95 Ron Curry - Fixed FD_STATE bug where FD_STATE was being used for
 * three state machines causing spurious writes to wrong TNC port. FD_STATE is
 * now used for real serial port, FD0_STATE for first psuedo tty, and FD1_STATE
 * for second psuedo tty. This was an easy fix but a MAJOR bug.
 *
 * 1.02 3/1/96 Jonathan Naylor - Make hardware handshaking default to off.
 * Allowed for true multiplexing of the data from the two pseudo ttys.
 * Numerous formatting changes.
 *
 * 1.03 4/20/96 Tomi Manninen - Rewrote KISS en/decapsulation (much of the
 * code taken from linux/drivers/net/slip.c). Added support for all the 16
 * possible kiss ports and G8BPQ-style checksumming on ttyinterface. Now
 * mkiss also sends a statistic report to stderr if a SIGUSR1 is sent to it.
 *
 * 1.04 25/5/96 Jonathan Naylor - Added check for UUCP style tty lock files.
 * As they are now supported by kissattach as well.
 *
 * 1.05 20/8/96 Jonathan Naylor - Convert to becoming a daemon and added
 * system logging.
 *
 * 1.06 23/11/96 Tomi Manninen - Added simple support for polled kiss.
 *
 * 1.07 12/24/97 Deti Fliegl - Added Flexnet/BayCom CRC mode with commandline
 * parameter -f    
 *
 * 1.08 xx/xx/99 Tom Mazouch - Adjustable poll interval
 */

#include <stdio.h>
#define __USE_XOPEN
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <syslog.h>
#include <limits.h>

#include <netax25/ttyutils.h>
#include <netax25/daemon.h>

#include <config.h>

#define FLEX_CRC	2
#define G8BPQ_CRC	1

#define	SIZE		4096

#define FEND		0300	/* Frame End			(0xC0)	*/
#define FESC		0333	/* Frame Escape			(0xDB)	*/
#define TFEND		0334	/* Transposed Frame End		(0xDC)	*/
#define TFESC		0335	/* Transposed Frame Escape	(0xDD)	*/

#define ACKREQ		0x0C
#define POLL		0x0E

/*
 * Keep these off the stack.
 */
static unsigned char ibuf[SIZE];	/* buffer for input operations	*/
static unsigned char obuf[SIZE];	/* buffer for kiss_tx()		*/

static int crc_errors		= 0;
static int invalid_ports	= 0;
static int return_polls		= 0;

static char *usage_string	= "usage: mkiss [-p interval] [-c] [-f] [-h] [-l] [-s speed] [-v] [-x <num_ptmx_devices>] ttyinterface pty ..\n";

static int dump_report		= FALSE;

static int logging              = FALSE;
static int crcflag		= FALSE;
static int hwflag		= FALSE;
static int pollspeed		= 0;

/* CRC-stuff */
typedef unsigned short int u16;
#define CRCTYP 0x20
static u16 crctab[256];

struct iface
{
	char		*name;		/* Interface name (/dev/???)	*/
	int		fd;		/* File descriptor		*/
	int		escaped;	/* FESC received?		*/
	u16		crc;		/* Incoming frame crc		*/
	unsigned char	obuf[SIZE];	/* TX buffer			*/
	unsigned char	*optr;		/* Next byte to transmit	*/
	unsigned int	errors;		/* KISS protocol error count	*/
	unsigned int	nondata;	/* Non-data frames rx count	*/
	unsigned int	rxpackets;	/* RX frames count		*/
	unsigned int	txpackets;	/* TX frames count		*/
	unsigned long	rxbytes;	/* RX bytes count		*/
	unsigned long	txbytes;	/* TX bytes count		*/
	char		namepts[PATH_MAX];  /* name of the unix98 pts slaves, which
				       * the client has to use */
};

static struct iface *tty	= NULL;
static struct iface *pty[16]	= { NULL };
static int numptys		= 0;

static void init_crc(void)
{
	short int i, j;
	u16 accum, data;

	for (i = 0; i < 256; i++) {	/* fill table with CRC of values... */
		accum = 0xffff;
		data = i;
		for (j = 0; j < 8; ++j) {
			if ((data^accum) & 0x0001)
				/* if msb of data^accum is TRUE */
				/* then shift and subtract poly */
				accum = (accum >> 1) ^ 0x8408;
			else
				/* otherwise: transparent shift */
				accum >>= 1;
			data >>= 1;	/* move up next bit for XOR     */
		}
		crctab[i] = accum;
	}
}

static int poll(int fd, int ports)
{
	char buffer[3];
	static int port = 0;

	buffer[0] = FEND;
	buffer[1] = POLL | (port << 4);
	buffer[2] = FEND;
	if (write(fd, buffer, 3) == -1) {
		perror("mkiss: poll: write");
		return 1;
	}
	if (++port >= ports)
		port = 0;
	return 0;
}

static int put_ubyte(unsigned char* s, u16* crc, unsigned char c, int usecrc)
{ 
  	int len = 1;

  	if (c == FEND) { 
		*s++ = FESC;
		*s++ = TFEND;
		len++;
  	} else { 
		*s++ = c;
		if (c == FESC) {
			*s++ = TFESC;
			len++;
		}
	}

	switch (usecrc) {
	case G8BPQ_CRC:
		*crc ^= c;	/* Adjust checksum */
		break;
	case FLEX_CRC:
		*crc = (*crc<<8)^crctab[(*crc>>8)^((u16)((c)&255))];
		break;
	}

	return len;
}

static int kiss_tx(int fd, int port, unsigned char *s, int len, int usecrc)
{
	unsigned char *ptr = obuf;
	unsigned char c, cmd;
	u16 crc = 0;
	int i;

	cmd = s[0] & 0x0F;

	/* Non-data frames don't get a checksum byte */
	if (usecrc == G8BPQ_CRC && cmd != 0 && cmd != ACKREQ)
		usecrc = FALSE;

	/*
	 * Send an initial FEND character to flush out any
	 * data that may have accumulated in the receiver
	 * due to line noise.
	 */
	*ptr++ = FEND;

    	if (usecrc == FLEX_CRC) {
		crc = 0xffff;
		ptr += put_ubyte(ptr, &crc, CRCTYP, usecrc);
		c = *s++;
	} else {
		c = *s++;
		c = (c & 0x0F) | (port << 4);
		ptr += put_ubyte(ptr, &crc, c, usecrc);
	}
	
	/*
	 * For each byte in the packet, send the appropriate
	 * character sequence, according to the SLIP protocol.
	 */
	for(i = 0; i < len - 1; i++)
		ptr += put_ubyte(ptr, &crc, s[i], usecrc);

	/*
	 * Now the checksum...
	 */
	switch (usecrc) {
	case G8BPQ_CRC:
		c = crc & 0xFF;
		ptr += put_ubyte(ptr, &crc, c, usecrc);
		break;
	case FLEX_CRC:
		{
			u16 u = crc;
			ptr += put_ubyte(ptr, &crc, u / 256, usecrc);
			ptr += put_ubyte(ptr, &crc, u & 255, usecrc);
		}
		break;
	}
	
	*ptr++ = FEND;
	return write(fd, obuf, ptr - obuf);
}

static int kiss_rx(struct iface *ifp, unsigned char c, int usecrc)
{
	int len;

	switch (c) {
	case FEND:
		len = ifp->optr - ifp->obuf;

		if (len != 0 && ifp->escaped) {	/* protocol error...	*/
			len = 0;		/* ...drop frame	*/
			ifp->errors++;
		}
		
		if (len != 0) {
			switch (usecrc) {
			case G8BPQ_CRC:
				if ((ifp->obuf[0] & 0x0F) != 0) {
					/*
					 * Non-data frames don't have checksum.
					 */
					usecrc = 0;
					if ((ifp->obuf[0] & 0x0F) == POLL) {
						/* drop returned polls	*/
						len = 0;
						return_polls++;
					} else
						ifp->nondata++;
				} else {
					if ((ifp->crc & 0xFF) != 0) {
						/* checksum failed...	*/
						/* ...drop frame	*/
						len = 0;
						crc_errors++;
					} else
						/* delete checksum byte	*/
						len--;
				}
				break;
			case FLEX_CRC:
				if (len > 14 && ifp->crc == 0x7070) {
					len -= 2;
					*ifp->obuf = 0;
				} else {
					len = 0;
					crc_errors++;
				}
				break;
			}
		}

		if (len != 0) {
			ifp->rxpackets++;
			ifp->rxbytes += len;
		}

		/*
		 * Clean up.
		 */
		ifp->optr = ifp->obuf;
		if (usecrc == FLEX_CRC)
			ifp->crc = 0xffff;
		else
			ifp->crc = 0;
		ifp->escaped = FALSE;
		return len;
	case FESC:
		ifp->escaped = TRUE;
		return 0;
	case TFESC:
		if (ifp->escaped) {
			ifp->escaped = FALSE;
			c = FESC;
		}
		break;
	case TFEND:
		if (ifp->escaped) {
			ifp->escaped = FALSE;
			c = FEND;
		}
		break;
	default:
		if (ifp->escaped) {		/* protocol error...	*/
			ifp->escaped = FALSE;
			ifp->errors++;
		}
		break;
	}

	*ifp->optr++ = c;

	switch (usecrc) {
	case G8BPQ_CRC:	
		ifp->crc ^= c;
		break;
	case FLEX_CRC:
		ifp->crc = (ifp->crc << 8) ^ crctab[(ifp->crc >> 8) ^ c];
		break;
	default:
		break;
	}

	return 0;
}

static void sigterm_handler(int sig)
{
	int i;

	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}

	tty_unlock(tty->name);
	close(tty->fd);
	free(tty);

	for (i = 0; i < numptys; i++) {
		if (pty[i]->fd == -1)
			continue;
		if (pty[i]->namepts[0] != '\0')
			continue;
		tty_unlock(pty[i]->name);
		close(pty[i]->fd);
		free(pty[i]);
	}

	exit(0);
}

static void sigusr1_handler(int sig)
{
	signal(SIGUSR1, sigusr1_handler);
	dump_report = TRUE;
}

static void report(void)
{
	int i;
	long t;

	time(&t);
	syslog(LOG_INFO, "version %s.", VERSION);
	syslog(LOG_INFO, "Status report at %s", ctime(&t));
	syslog(LOG_INFO, "Hardware handshaking %sabled.",
	       hwflag  ? "en" : "dis");
	syslog(LOG_INFO, "G8BPQ checksumming %sabled.",
	       crcflag == G8BPQ_CRC ? "en" : "dis");
	syslog(LOG_INFO, "FLEX checksumming %sabled.",
	       crcflag == FLEX_CRC ? "en" : "dis");
	       
	syslog(LOG_INFO, "polling %sabled.",
	       pollspeed ? "en" : "dis");
	if (pollspeed)
		syslog(LOG_INFO, "Poll interval %d00ms", pollspeed);
	syslog(LOG_INFO, "ttyinterface is %s (fd=%d)", tty->name, tty->fd);
	for (i = 0; i < numptys; i++)
		syslog(LOG_INFO, "pty%d is %s (fd=%d)", i, pty[i]->name,
			pty[i]->fd);
	syslog(LOG_INFO, "Checksum errors: %d", crc_errors);
	syslog(LOG_INFO, "Invalid ports: %d", invalid_ports);
	syslog(LOG_INFO, "Returned polls: %d", return_polls);
	syslog(LOG_INFO, "Interface   TX frames TX bytes  RX frames RX bytes  Errors");
	syslog(LOG_INFO, "%-11s %-9u %-9lu %-9u %-9lu %u",
	       tty->name,
	       tty->txpackets, tty->txbytes,
	       tty->rxpackets, tty->rxbytes,
	       tty->errors);
	for (i = 0; i < numptys; i++) {
		syslog(LOG_INFO, "%-11s %-9u %-9lu %-9u %-9lu %u",
		       pty[i]->name,
		       pty[i]->txpackets, pty[i]->txbytes,
		       pty[i]->rxpackets, pty[i]->rxbytes,
		       pty[i]->errors);
	}
	return;
}

int main(int argc, char *argv[])
{
	unsigned char *icp;
	int topfd;
	fd_set readfd;
	struct timeval timeout, pollinterval;
	int retval, i, size, len;
	int speed	= -1;
	int ptmxdevices = 0;
	char *npts;
	int wrote_info = 0;

	while ((size = getopt(argc, argv, "cfhlp:s:vx:")) != -1) {
		switch (size) {
		case 'c':
			crcflag = G8BPQ_CRC;
			break;
		case 'f':
			crcflag = FLEX_CRC;
			break;
		case 'h':
			hwflag = TRUE;
			break;
		case 'l':
		        logging = TRUE;
		        break;
		case 'p':
			pollspeed = atoi(optarg);
			pollinterval.tv_sec = pollspeed / 10;
			pollinterval.tv_usec = (pollspeed % 10) * 100000L;
		        break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'x':
			ptmxdevices = atoi(optarg);
			if (ptmxdevices < 1 || ptmxdevices > 16) {
				fprintf(stderr, "mkiss: too %s devices\n", ptmxdevices < 1 ? "few" : "many");
				return 1;
			}
			break;
		case 'v':
			printf("mkiss: %s\n", VERSION);
			return 1;
		case ':':
		case '?':
			fprintf(stderr, usage_string);
			return 1;
		}
	}

	if ((argc - optind) < 2 && ptmxdevices == 0) {
		fprintf(stderr, usage_string);
		return 1;
	}

	if ((argc - optind) < 1 && ptmxdevices > 0) {
		fprintf(stderr, usage_string);
		return 1;
	}

        numptys = argc - optind - 1;
	if ((numptys + ptmxdevices) > 16) {
		fprintf(stderr, "mkiss: max 16 pty interfaces allowed.\n");
		return 1;
	}

	/*
	 * Check for lock files before opening any TTYs
	 */
	if (tty_is_locked(argv[optind])) {
		fprintf(stderr, "mkiss: tty %s is locked by another process\n", argv[optind]);
		return 1;
	}
	for (i = 0; i < numptys; i++) {
		if (!strcmp("/dev/ptmx", argv[optind + i + 1]))
			continue;
		if (!strcmp("none", argv[optind + i + 1]))
			continue;
		if (tty_is_locked(argv[optind + i + 1])) {
			fprintf(stderr, "mkiss: pty %s is locked by another process\n", argv[optind + i + 1]);
			return 1;
		}
	}

	/*
	 * Open and configure the tty interface. Open() is
	 * non-blocking so it won't block regardless of the modem
	 * status lines.
	 */
	if ((tty = calloc(1, sizeof(struct iface))) == NULL) {
		perror("mkiss: malloc");
		return 1;
	}

	if ((tty->fd = open(argv[optind], O_RDWR | O_NDELAY)) == -1) {
		perror("mkiss: open");
		return 1;
	}

	tty->name = argv[optind];
	tty_raw(tty->fd, hwflag);
	if (speed != -1 && !tty_speed(tty->fd, speed)) {
		close(tty->fd);
		return 1;
	}
	tty->optr = tty->obuf;
	topfd = tty->fd;
	tty->namepts[0] = '\0';

	/*
	 * Make it block again...
	 */
	fcntl(tty->fd, F_SETFL, 0);

	/*
	 * Open and configure the pty interfaces
	 */
	for (i = 0; i < numptys+ptmxdevices; i++) {
		static char name_ptmx[] = "/dev/ptmx";
		char *pty_name = (i < numptys ? argv[optind+i+1] : name_ptmx);

		if ((pty[i] = calloc(1, sizeof(struct iface))) == NULL) {
			perror("mkiss: malloc");
			return 1;
		}
		if (!strcmp(pty_name, "none")) {
			pty[i]->fd = -1;
			strcpy(pty[i]->namepts, "none");
		} else {
			if ((pty[i]->fd = open(pty_name, O_RDWR)) == -1) {
				perror("mkiss: open");
				free(pty[i]);
				pty[i] = 0;
				return 1;
			}
			tty_raw(pty[i]->fd, FALSE);
			topfd = (pty[i]->fd > topfd) ? pty[i]->fd : topfd;
			pty[i]->namepts[0] = '\0';
		}
		pty[i]->name = pty_name;
		pty[i]->optr = pty[i]->obuf;
		if (!strcmp(pty[i]->name, "/dev/ptmx")) {
			/* get name of pts-device */
			if ((npts = ptsname(pty[i]->fd)) == NULL) {
				fprintf(stderr, "mkiss: Cannot get name of pts-device.\n");
				free(pty[i]);
				pty[i] = 0;
				return 1;
			}
			strncpy(pty[i]->namepts, npts, PATH_MAX-1);
			pty[i]->namepts[PATH_MAX-1] = '\0';

			/* unlock pts-device */
			if (unlockpt(pty[i]->fd) == -1) {
				fprintf(stderr, "mkiss: Cannot unlock pts-device %s\n", pty[i]->namepts);
				free(pty[i]);
				pty[i] = 0;
				return 1;
			}
			if (wrote_info == 0)
				printf("\nAwaiting client connects on:\n");
			else
				printf(" ");
			printf("%s", pty[i]->namepts);
			wrote_info = 1;
		}
	}

	if (wrote_info > 0)
		printf("\n");

	numptys=numptys+ptmxdevices;

	/*
	 * Now all the ports are open, lock them.
	 */
	tty_lock(tty->name);
	for (i = 0; i < numptys; i++) {
		if (pty[i]->namepts[0] == '\0')
			tty_lock(pty[i]->name);
	}

	if (logging) {
		openlog("mkiss", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "starting");
	}

	if (wrote_info > 0) {
		fflush(stdout);
		fflush(stderr);
		close(0);
		close(1);
		close(2);
	}

	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, sigusr1_handler);
	signal(SIGTERM, sigterm_handler);

	if (!daemon_start(FALSE)) {
		fprintf(stderr, "mkiss: cannot become a daemon\n");
		return 1;
	}

	init_crc();
	/*
	 * Loop until an error occurs on a read.
	 */
	while (TRUE) {
		FD_ZERO(&readfd);
		FD_SET(tty->fd, &readfd);
		for (i = 0; i < numptys; i++)
			if (pty[i]->fd != -1)
				FD_SET(pty[i]->fd, &readfd);

		if (pollspeed)
			timeout = pollinterval;

		errno = 0;
		retval = select(topfd + 1, &readfd, NULL, NULL, pollspeed ? &timeout : NULL);

		if (retval == -1) {
			if (dump_report) {
				if (logging)
					report();
				dump_report = FALSE;
				continue;
			} else {
				perror("mkiss: select");
				continue;
			}
		}

		/*
		 * Timer expired - let's poll...
		 */
		if (retval == 0 && pollspeed) {
			poll(tty->fd, numptys);
			continue;
		}

		/*
		 * A character has arrived on the ttyinterface.
		 */
		if (tty->fd > -1 && FD_ISSET(tty->fd, &readfd)) {
			if ((size = read(tty->fd, ibuf, SIZE)) < 0 && errno != EINTR) {
				if (logging)
					syslog(LOG_ERR, "tty->fd: %m");
				break;
			}
			for (icp = ibuf; size > 0; size--, icp++) {
				if ((len = kiss_rx(tty, *icp, crcflag)) != 0) {
					if ((i = (*tty->obuf & 0xF0) >> 4) < numptys) {
						if (pty[i]->fd != -1) {
							kiss_tx(pty[i]->fd, 0, tty->obuf, len, FALSE);
							pty[i]->txpackets++;
							pty[i]->txbytes += len;
						}
					} else
						invalid_ports++;
					if (pollspeed)
						poll(tty->fd, numptys);
				}
			}
		}

		for (i = 0; i < numptys; i++) {
			/*
			 * A character has arrived on pty[i].
			 */
			if (pty[i]->fd > -1 && FD_ISSET(pty[i]->fd, &readfd)) {
				if ((size = read(pty[i]->fd, ibuf, SIZE)) < 0 && errno != EINTR) {
					if (logging)
						syslog(LOG_ERR, "pty[%d]->fd: %m\n", i);
					goto end;
				}
				for (icp = ibuf; size > 0; size--, icp++) {
					if ((len = kiss_rx(pty[i], *icp, FALSE)) != 0) {
						kiss_tx(tty->fd, i, pty[i]->obuf, len, crcflag);
						tty->txpackets++;
						tty->txbytes += len;
					}
				}
			}
		}
	}

end:
	if (logging)
		closelog();

	tty_unlock(tty->name);
	close(tty->fd);
	free(tty);

	for (i = 0; i < numptys; i++) {
		if (pty[i]->fd == -1)
			continue;
		if (pty[i]->namepts[0] != '\0')
			continue;
		tty_unlock(pty[i]->name);
		close(pty[i]->fd);
		free(pty[i]);
	}

	return 1;
}
