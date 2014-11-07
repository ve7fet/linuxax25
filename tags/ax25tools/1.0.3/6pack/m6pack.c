/* Hey Emacs! this is -*- linux-c -*- 
 * from /usr/src/linux/Documentation/CodingStyle
 *
 * m6pack.c
 *
 * Fake out AX.25 code into supporting 6pack TNC rings by routing serial
 * port data to/from pseudo ttys.
 *
 * Author(s):
 *
 *	Iñaki Arenaza (EB2EBU) <iarenaza@escomposlinux.org>
 *
 *********************************************************************
 *
 * Quite a lot of stuff "stolen" from mkiss.c, written by
 *
 * 		Kevin Uhlir
 * 		Ron Curry
 *		Jonathan Naylor
 *		Tomi Manninen
 *
 * Copyright (C) 1999 Iñaki Arenaza
 * mkiss.c was GPLed, so I guess this one is GPLed too ;-)
 *
 *********************************************************************
 *
 * REMARK:
 *
 * See document '6pack.ps' by Matthias Welwarsky (DG2FEF), translated
 * by Thomas Sailer (HB9JNX) found in ax25-doc-1.0.tgz for details about
 * 6pack protocol specifications.
 *
 *********************************************************************
 */

#include <stdio.h>
#define __USE_XOPEN
#include <string.h>
#include <errno.h>
#include <stdlib.h>
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

typedef unsigned char __u8;
typedef enum {data, command} frame_t;

#define SIXP_MAX_ADDR   ((__u8) 8)
#define	SIZE		4096
#define MAX_PTYS	SIXP_MAX_ADDR  /* Max number of TNCs in a 6PACK ring */

/*
 * Keep these off the stack.
 */
static __u8 ibuf[SIZE];	/* buffer for input operations */
static __u8 obuf[SIZE];	/* buffer for sixpack_tx() */

static int invalid_ports = 0;

static char *usage_string = "usage: m6pack [-l] [-s speed] [-x num_ptmx_devices] [-v] tyinterface pty ..\n";

static int dump_report = FALSE;
static int logging = FALSE;

static struct iface *pty[MAX_PTYS];
static struct iface *tty;
static int numptys;

struct iface
{
	char		*name;		/* Interface name (/dev/???)	*/
	int		fd;		/* File descriptor		*/
	int		pty_id;		/* 6pack address asigned to pty */
	__u8		seof;		/* leading SEOF of this frame	*/
	__u8		databuf[SIZE];	/* TX buffer (data frames)	*/
	__u8		cmdbuf[1];	/* TX buffer (command frames)	*/
	__u8		*optr;		/* Next byte to transmit	*/
	unsigned int	sixp_cnt;	/* 6pack-coded octets count	*/
	unsigned int	decod_cnt;	/* 6pack-decoded octets cont	*/
	unsigned int	errors;		/* 6PACK frame error count	*/
	unsigned int	rxpackets;	/* RX frames count		*/
	unsigned int	txpackets;	/* TX frames count		*/
	unsigned long	rxbytes;	/* RX bytes count		*/
	unsigned long	txbytes;	/* TX bytes count		*/
	char		namepts[PATH_MAX];  /* name of the unix98 pts slaves, which
				       * the client has to use */
};

#define PTY_ID_TTY (-1)

/* 6PACK "TNC Address" commands are special in that the address they
 * carry is not the address of the destination TNC but the initial
 * address for the firt TNC in the ring (when sent PC->TNC Ring), or
 * the last TNC address+1 (when received TNC ring -> PC).  So we can't
 * use the address in the command to select a pty to pass the
 * command, and we have to keep track of what ptys have sent "TNC
 * Address" commands to sent them back the response (until we modify
 * the kernel 6pack driver to support multiple TNC's (i.e., network
 * devices) on one serial port). We deliver the responses with a FIFO
 * policy.
 */
static int cmd_addr_fifo[SIXP_MAX_ADDR];
static int head; /* We retrieve from head position */
static int tail; /* We insert at tail position */

/* Masks to tell apart 6pack command and data frames
 */
#define SIXP_IS_DATA(x) ((((__u8)(x)) & (__u8)0xC0) == (__u8)0)
#define SIXP_IS_CMD(x) !(SIXP_IS_DATA(x))

/* Masks to split commands in opcode and TNC address
 */
#define SIXP_ADDR_MASK  ((__u8) (SIXP_MAX_ADDR-1))
#define SIXP_CMD_MASK ((__u8) ~SIXP_ADDR_MASK)
#define SIXP_CMD(x) (((__u8)x) & SIXP_CMD_MASK)
#define SIXP_ADDR(x) (((__u8)x) & SIXP_ADDR_MASK)

/* Macro to build a 6pack TNC command from opcode and TNC address
 */
#define SIXP_MAKE_CMD(opcode,addr) (((__u8)opcode) | ((__u8)addr))

/* 6pack protocol commands (relevant bits only)
 */
#define SIXP_CMD_SEOF       ((__u8) 0x40)	/* start/end of 6pack frame */
#define SIXP_CMD_TX_ORUN    ((__u8) 0x48)	/* transmit overrun */
#define SIXP_CMD_RX_ORUN    ((__u8) 0x50)	/* receive overrun */
#define SIXP_CMD_RX_BUF_OVL ((__u8) 0x58)	/* receive buffer overflow */
#define SIXP_CMD_LED	    ((__u8) 0x60)	/* set LED status */
#define SIXP_CMD_TX_1	    ((__u8) 0xA0)	/* TX counter + 1 */
#define SIXP_CMD_RX_1	    ((__u8) 0x90)	/* RX counter + 1 */
#define SIXP_CMD_DCD	    ((__u8) 0x88)	/* DCD state */
#define SIXP_CMD_CAL	    ((__u8) 0xE0)	/* send calibration pattern */
#define SIXP_CMD_ADDR	    ((__u8) 0xE8)	/* set TNC address */

/* Valid checksum of a 6pack frame
 */
#define SIXP_CHKSUM ((__u8) 0xFF)

static int sixpack_rx(struct iface *ifp, __u8 c, __u8 *tnc_addr, frame_t *type)
{
	int i, len;
	__u8 checksum;
	
	/* Is it a data octect?
	 */
	if (SIXP_IS_DATA(c)) {
		/* Decode the 6PACK octect and store the resultant
		 * bits in the output buffer.
		 */
		switch (ifp->sixp_cnt % 4) {
		case 0:
			*ifp->optr = (c & 0x3F);
			break;
		case 1:
			*ifp->optr++ |= (c & 0x30) << 2; 
			*ifp->optr = (c & 0x0F);
			break;
		case 2:
			*ifp->optr++ |= (c & 0x3C) << 2; 
			*ifp->optr = (c & 0x03);
			break;
		default:
			*ifp->optr++ |= (c & 0x3F) << 2; 
		}
		ifp->sixp_cnt++;
		return 0;
	}
	
	/* Nope, it's a command octect. See which kind of command.
	 * Anything but a SEOF command is a one-octect command, so
	 * process it immediately and return.
	 */
	if (SIXP_CMD(c) != SIXP_CMD_SEOF) {
		if (SIXP_CMD(c) == SIXP_CMD_ADDR) {
			/* 6PACK "TNC Address" commans are
			 * "special". The address field must be
			 * rewritten.
			 */
			if (PTY_ID_TTY == ifp->pty_id) {
				/* TNC Ring -> pty
				 * Dequeue a TNC Address request.
				 */
				*ifp->cmdbuf = SIXP_MAKE_CMD(SIXP_CMD(c),1);
				*tnc_addr = cmd_addr_fifo[head];
				head = (head + 1) % SIXP_MAX_ADDR;
			}
			else {
				/* pty -> TNC Ring
				 * Enqueue a TNC Address request.
				 */
				*ifp->cmdbuf = SIXP_MAKE_CMD(SIXP_CMD(c), 0);
				*tnc_addr = 0;
				cmd_addr_fifo[tail] = ifp->pty_id;
				tail = (tail + 1) % SIXP_MAX_ADDR;
			}
		}
		else {
			/* We need a separate buffer here for 6PACK commands
			 * as the protocol allows the transmission of command
			 * frames in the middle of data frames. Don't touch
			 * anything else! A data frame may be "en route".
			 */
			*ifp->cmdbuf = SIXP_CMD(c);
			*tnc_addr = SIXP_ADDR(c);
		}
		/* Update statistics and return.
		 */
		ifp->rxpackets++;
		ifp->rxbytes++;
		*type = command;
		return 1;
	}
	
	/* We're dealing with a SEOF command.
	 */
	len = ifp->optr - ifp->databuf;
	if (len > 0) {
		if (len < 3) {
			/* Data frames have at least 3 octets:
			 *    TxDelay, Datum, CheckSum.
			 * Signal error and reset state.
			 */
			goto error_reset_state;
		}
		
		/* Now that we've decoded 6PACK octects,
		 * check the checksum of the frame. 
		 */
		checksum = 0;
		for (i = 0; i < len; i++) {
			checksum += ifp->databuf[i];
		}
		/* Address of this frame must be taken from the
		 * leading SEOF.
		 */
		*tnc_addr = SIXP_ADDR(ifp->seof);
		checksum += *tnc_addr;
		
		if (checksum != SIXP_CHKSUM) {
			/* Signal error and reset state.
			 */
			goto error_reset_state;
		}
		
		/* Now remove checksum from the frame (this makes
		 * sixpack_tx easier).
		 */
		len--;/* Minus Checksum */

		/* Finally update statistics and return all
		 * needed parameters
		 */
		ifp->rxpackets++;
		ifp->rxbytes += len;
		*type = data;
	}

	ifp->optr = ifp->databuf;
	ifp->seof = c;
	ifp->sixp_cnt = 0;
	ifp->decod_cnt = 0;
	return len;  
	
 error_reset_state:
	ifp->errors++;
	ifp->optr = ifp->databuf;
	ifp->sixp_cnt = 0;
	ifp->decod_cnt = 0;
	return 0;
}

static void sixpack_tx(int fd, __u8 tnc_addr, __u8 *ibuf, int len, frame_t type)
{
	__u8 *ptr = obuf;
	__u8 checksum;
	int count, written;

	if (type == command) {
		if (SIXP_CMD(*ibuf) == SIXP_CMD_ADDR) {
			/* 6PACK "TNC Address" commans are
			 * "special". We must send them untouched here.
			 */
			*ptr++ = *ibuf;
		}
		else {
			/* Commands, except SEOF, are 1 byte long.
			 */
			*ptr++ = SIXP_MAKE_CMD(SIXP_CMD(*ibuf), tnc_addr);
		}
	}
	else {
		*ptr++ = SIXP_MAKE_CMD(SIXP_CMD_SEOF, tnc_addr);
		checksum = 0;
		for (count = 0; count < len; count++) {
			switch (count % 3) {
			case 0:
				*ptr++ = (ibuf[count] & 0x3F);
				*ptr = (ibuf[count] & 0xC0) >> 2;
				break;
			case 1:
				*ptr++ |= (ibuf[count] & 0x0F);
				*ptr = (ibuf[count] & 0xF0) >> 2;
				break;
			default:
				*ptr++ |= (ibuf[count] & 0x03);
				*ptr++ = (ibuf[count] & 0xFC) >> 2;
				break;
			}
			checksum += ibuf[count];
		}

		checksum += tnc_addr;
		checksum = 0xFF - checksum;
		switch (count % 3) {
		case 0:
			*ptr++ = (checksum & 0x3F);
			*ptr++ = (checksum & 0xC0) >> 2;
			break;
		case 1:
			*ptr++ |= (checksum & 0x0F);
			*ptr++ = (checksum & 0xF0) >> 2;
			break;
		default:
			*ptr++ |= (checksum & 0x03);
			*ptr++ = (checksum & 0xFC) >> 2;
			break;
		}

		/* Trailing SEOF doesn't carry TNC address
		 */
		*ptr++ = SIXP_MAKE_CMD(SIXP_CMD_SEOF, 0);
	}

	count = ptr - obuf;
	written = 0;
	ptr = obuf; 
	while (count > 0) {
		written = write (fd, ptr, count);
		count -= written;
		ptr += written;
	}
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

static void report(struct iface *tty, struct iface **pty, int numptys)
{
	int i;
	long t;

	time(&t);
	syslog(LOG_INFO, "version %s.", VERSION);
	syslog(LOG_INFO, "Status report at %s", ctime(&t));
	syslog(LOG_INFO, "ttyinterface is %s (fd=%d)", tty->name, tty->fd);
	for (i = 0; i < numptys; i++)
		syslog(LOG_INFO, "pty%d is %s (fd=%d)", i, pty[i]->name,
			pty[i]->fd);
	syslog(LOG_INFO, "Invalid ports: %d", invalid_ports);
	syslog(LOG_INFO, "Interface   TX frames TX bytes  RX frames RX "
	       "bytes  Errors");
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
	__u8 *icp, tnc_addr;
	int topfd;
	fd_set readfd;
	int retval, i, size, len;
	int speed	= -1;
	int ptmxdevices = 0;
	char *npts;
	int wrote_info = 0;
	frame_t type;

	head = tail = 0;
	while ((size = getopt(argc, argv, "ls:vx:")) != -1) {
		switch (size) {
		case 'l':
			logging = TRUE;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'x':
			ptmxdevices = atoi(optarg);
			if (ptmxdevices < 1 || ptmxdevices > MAX_PTYS) {
				fprintf(stderr, "m6pack: too %s devices\n", ptmxdevices < 1 ? "few" : "many");
				return 1;
			}
			break;
		case 'v':
			printf("m6pack: %s\n", VERSION);
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

	if (numptys + ptmxdevices > MAX_PTYS) {
		fprintf(stderr, "m6pack: max %d pty interfaces allowed.\n",
			MAX_PTYS);
		return 1;
	}

	/*
	 * Check for lock files before opening any TTYs
	 */
	if (tty_is_locked(argv[optind])) {
		fprintf(stderr, "m6pack: tty %s is locked by another process\n", argv[optind]);
		return 1;
	}
	for (i = 0; i < numptys; i++) {
		if (!strcmp("/dev/ptmx", argv[optind + i + 1]))
			continue;
		if (tty_is_locked(argv[optind + i + 1])) {
			fprintf(stderr, "m6pack: pty %s is locked by another process\n", argv[optind + i + 1]);
			return 1;
		}
	}

	/*
	 * Open and configure the tty interface. Open() is
	 * non-blocking so it won't block regardless of the modem
	 * status lines.
	 */
	if ((tty = calloc(1, sizeof(struct iface))) == NULL) {
		perror("m6pack: malloc");
		return 1;
	}

	if ((tty->fd = open(argv[optind], O_RDWR | O_NDELAY)) == -1) {
		perror("m6pack: open");
		return 1;
	}

	tty->name = argv[optind];
	if (speed != -1) tty_speed(tty->fd, speed);
	tty_raw(tty->fd, FALSE);
	tty->pty_id = PTY_ID_TTY;
	tty->optr = tty->databuf;
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
			perror("m6pack: malloc");
			return 1;
		}
		if ((pty[i]->fd = open(pty_name, O_RDWR)) == -1) {
			perror("m6pack: open");
			return 1;
		}
		pty[i]->name = pty_name;
		tty_raw(pty[i]->fd, FALSE);
		pty[i]->pty_id = i;
		pty[i]->optr = pty[i]->databuf;
		topfd = (pty[i]->fd > topfd) ? pty[i]->fd : topfd;
		pty[i]->namepts[0] = '\0';
		if (!strcmp(pty[i]->name, "/dev/ptmx")) {
			/* get name of pts-device */
			if ((npts = ptsname(pty[i]->fd)) == NULL) {
				fprintf(stderr, "m6pack: Cannot get name of pts-device.\n");
				return 1;
			}
			strncpy(pty[i]->namepts, npts, PATH_MAX-1);
			pty[i]->namepts[PATH_MAX-1] = '\0';

			/* unlock pts-device */
			if (unlockpt(pty[i]->fd) == -1) {
				fprintf(stderr, "m6pack: Cannot unlock pts-device %s\n", pty[i]->namepts);
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
		openlog("m6pack", LOG_PID, LOG_DAEMON);
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
		fprintf(stderr, "m6pack: cannot become a daemon\n");
		return 1;
	}

	/*
	 * Loop until an error occurs on a read.
	 */
	while (TRUE) {
		FD_ZERO(&readfd);
		FD_SET(tty->fd, &readfd);
		for (i = 0; i < numptys; i++)
			FD_SET(pty[i]->fd, &readfd);

		errno = 0;
		retval = select(topfd + 1, &readfd, NULL, NULL, NULL);

		if (retval == -1) {
			if (dump_report) {
				if (logging)
					report(tty, pty, numptys);
				dump_report = FALSE;
				continue;
			} else {
				perror("m6pack: select");
				continue;
			}
		}


		/*
		 * A character has arrived on the ttyinterface.
		 */
		if (FD_ISSET(tty->fd, &readfd)) {
			if ((size = read(tty->fd, ibuf, SIZE)) < 0 
			    && errno != EINTR) {
				if (logging)
					syslog(LOG_ERR, "tty->fd: %m");
				break;
			}

			for (icp = ibuf; size > 0; size--, icp++) {
				if ((len = sixpack_rx(tty,*icp,&tnc_addr,
						      &type)) != 0) {
					if (tnc_addr <= numptys) {
						sixpack_tx(pty[tnc_addr]->fd,
							   0,
							   (type == data) ? 
							   tty->databuf : 
							   tty->cmdbuf,
							   len,
							   type);
						pty[tnc_addr]->txpackets++;
						pty[tnc_addr]->txbytes += len;
					} else
						invalid_ports++;
				}
			}
		}

		for (i = 0; i < numptys; i++) {
			/*
			 * A character has arrived on pty[i].
			 */
			if (FD_ISSET(pty[i]->fd, &readfd)) {
				if ((size = read(pty[i]->fd, ibuf, SIZE)) < 0
				    && errno != EINTR) {
					if (logging)
						syslog(LOG_ERR,
						       "pty[%d]->fd: %m\n",i);
					goto end;
				}

				for (icp = ibuf; size > 0; size--, icp++) {
					if ((len = sixpack_rx(pty[i],
							      *icp,
							      &tnc_addr,
							      &type)) != 0) {
						sixpack_tx(tty->fd, i,
							   (type == data) ?
							   pty[i]->databuf :
							   pty[i]->cmdbuf,
							   len, type);
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
		tty_unlock(pty[i]->name);
		close(pty[i]->fd);
		free(pty[i]);
	}

	return 1;
}
