#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>
#include <syslog.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <netax25/daemon.h>
#include <netax25/ttyutils.h>

#include <config.h>

#include "../pathnames.h"

static int kissfd, nrsfd;
static int logging = FALSE;
static int debugging = FALSE;
static int flowcontrol = FALSE;
static char *kissdev, *nrsdev;

#define	NUL		000
#define	STX		002
#define	ETX		003
#define	DLE		020

#define	NRS_WAIT	0
#define	NRS_DATA	1
#define	NRS_ESCAPE	2
#define	NRS_CKSUM	3
static int nrs_state = NRS_WAIT;

static unsigned char nrs_cksum = 0;

static unsigned char nrs_rxbuffer[512];
static int nrs_rxcount = 0;

#define	FEND		0300
#define	FESC		0333
#define	FESCEND		0334
#define	FESCESC		0335

#define	KISS_WAIT	0
#define	KISS_CTRL	1
#define	KISS_DATA	2
#define	KISS_ESCAPE	3
static int kiss_state = KISS_WAIT;

static unsigned char kiss_rxbuffer[512];
static int kiss_rxcount = 0;

static void terminate(int sig)
{
	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}

	tty_unlock(kissdev);
	tty_unlock(nrsdev);

	exit(0);
}

static void key_rts(int fd)
{
	int status;

	if (!flowcontrol)
		return;

	/* Wait for CTS to be low */
	while (1) {
		/* Get CTS status */
                if (ioctl(fd, TIOCMGET, &status) < 0) {
			syslog(LOG_INFO|LOG_ERR, "TIOCMGET failed: flowcontrol disabled (%m)\n");
			flowcontrol = 0;
                        return;
                }
		if (status & TIOCM_CTS) {
			if (debugging) {
				fprintf(stderr,"CTS high: waiting\n");
			}
                	ioctl(fd, TIOCMIWAIT, &status);
		} else {
			break;
		}
	}

	if (debugging) {
		fprintf(stderr,"CTS low: keying RTS\n");
	}
	status |= TIOCM_RTS | TIOCM_DTR;
        if (ioctl(fd, TIOCMSET, &status) < 0) {
		syslog(LOG_INFO|LOG_ERR, "TIOCMGET failed: flowcontrol disabled (%m)\n");
		flowcontrol = 0;
        }
}

static void unkey_rts(int fd)
{
	int status;

	if (!flowcontrol)
		return;

	if (debugging) {
		fprintf(stderr,"Transmission finished: unkeying RTS\n");
	}
        ioctl(fd, TIOCMGET, &status);
	status &= ~TIOCM_RTS;
        status |= TIOCM_DTR;
        if (ioctl(fd, TIOCMSET, &status) < 0) {
		syslog(LOG_INFO|LOG_ERR, "TIOCMGET failed: flowcontrol disabled (%m)\n");
		flowcontrol = 0;
        }
}

static void nrs_esc(unsigned char *s, int len)
{
	static unsigned char buffer[512];
	unsigned char *ptr = buffer;
	unsigned char csum = 0;
	unsigned char c;

	*ptr++ = STX;

	while (len-- > 0) {
		switch (c = *s++) {
			case STX:
			case ETX:
			case DLE:
				*ptr++ = DLE;
				/* Fall through */
			default:
				*ptr++ = c;
				break;
		}
		
		csum += c;
	}

	*ptr++ = ETX;
	*ptr++ = csum;
	*ptr++ = NUL;
	*ptr++ = NUL;

	key_rts(nrsfd);
	write(nrsfd, buffer, ptr - buffer);
	unkey_rts(nrsfd);
}

static void kiss_esc(unsigned char *s, int len)
{
	static unsigned char buffer[512];
	unsigned char *ptr = buffer;
	unsigned char c;

	*ptr++ = FEND;
	*ptr++ = 0x00;		/* KISS DATA */

	while (len-- > 0) {
		switch (c = *s++) {
			case FESC:
				*ptr++ = FESC;
				*ptr++ = FESCESC;
				break;
			case FEND:
				*ptr++ = FESC;
				*ptr++ = FESCEND;
				break;
			default:
				*ptr++ = c;
				break;
		}
	}

	*ptr++ = FEND;

	write(kissfd, buffer, ptr - buffer);
}

static void nrs_unesc(unsigned char *buffer, int len)
{
	int i;
	
	for (i = 0; i < len; i++) {
		switch (nrs_state) {
			case NRS_WAIT:
				if (buffer[i] == STX) {
					nrs_state   = NRS_DATA;
					nrs_rxcount = 0;
					nrs_cksum   = 0;
				}
				break;

			case NRS_DATA:
				switch (buffer[i]) {	
					case STX:	/* !! */
						nrs_rxcount = 0;
						nrs_cksum   = 0;
						break;
					case DLE:
						nrs_state = NRS_ESCAPE;
						break;
					case ETX:
						nrs_state = NRS_CKSUM;
						break;
					default:
						if (nrs_rxcount < 512) {
							nrs_cksum += buffer[i];
							nrs_rxbuffer[nrs_rxcount++] = buffer[i];
						}
						break;
				}
				break;

			case NRS_ESCAPE:
				nrs_state = NRS_DATA; 
				if (nrs_rxcount < 512) {
					nrs_cksum += buffer[i];
					nrs_rxbuffer[nrs_rxcount++] = buffer[i];
				}
				break;

			case NRS_CKSUM:
				if (buffer[i] == nrs_cksum)
					kiss_esc(nrs_rxbuffer, nrs_rxcount);
				nrs_state   = NRS_WAIT;
				nrs_cksum   = 0;
				nrs_rxcount = 0;
				break;
		}
	}
}

static void kiss_unesc(unsigned char *buffer, int len)
{
	int i;
	
	for (i = 0; i < len; i++) {
		switch (kiss_state) {
			case KISS_WAIT:
				if (buffer[i] == FEND) {
					kiss_state   = KISS_CTRL;
					kiss_rxcount = 0;
				}
				break;

			case KISS_CTRL:
				if ((buffer[i] & 0x0F) == 0x00) {
					kiss_state   = KISS_DATA;
					kiss_rxcount = 0;
				} else {
					kiss_state   = KISS_WAIT;
					kiss_rxcount = 0;
				}
				break;

			case KISS_DATA:
				switch (buffer[i]) {	
					case FEND:
						if (kiss_rxcount > 2)
							nrs_esc(kiss_rxbuffer, kiss_rxcount);
						kiss_state   = KISS_WAIT;
						kiss_rxcount = 0;
						break;
					case FESC:
						kiss_state = KISS_ESCAPE;
						break;
					default:
						if (kiss_rxcount < 512)
							kiss_rxbuffer[kiss_rxcount++] = buffer[i];
						break;
				}
				break;

			case KISS_ESCAPE:
				kiss_state = KISS_DATA;
				switch (buffer[i]) {
					case FESCESC:
						if (kiss_rxcount < 512)
							kiss_rxbuffer[kiss_rxcount++] = FESC;
						break;
					case FESCEND:
						if (kiss_rxcount < 512)
							kiss_rxbuffer[kiss_rxcount++] = FEND;
						break;
				}
				break;
		}
	}
}

int main(int argc, char *argv[])
{
	static char buffer[512];
	unsigned int speed = 0;
	fd_set read_fd;
	int c, n;

	while ((c = getopt(argc, argv, "dfls:v")) != -1) {
		switch (c) {
			case 'd':
				debugging = TRUE;
				break;
			case 'f':
				flowcontrol = TRUE;
				break;
			case 'l':
				logging = TRUE;
				break;
			case 's':
				if ((speed = atoi(optarg)) <= 0) {
					fprintf(stderr, "nrsdrv: invalid speed %s\n", optarg);
					return 1;
				}
				break;
			case 'v':
				printf("kissattach: %s\n", VERSION);
				return 0;
			case ':':
			case '?':
				fprintf(stderr, "usage: nrsdrv [-f] [-l] [-s speed] [-v] kisstty nrstty\n");
				return 1;
		}
	}

	if (debugging) {
		fprintf(stderr,"Flow control %s\n", 
			flowcontrol ? "enabled" : "disabled");
	}

	if ((argc - optind) != 2) {
		fprintf(stderr, "usage: nrsdrv [-f] [-l] [-s speed] [-v] kisstty nrstty\n");
		return 1;
	}

	kissdev = argv[optind + 0];
	nrsdev  = argv[optind + 1];

	if (tty_is_locked(kissdev)) {
		fprintf(stderr, "nrsdrv: device %s already in use\n", argv[optind]);
		return 1;
	}

	if (tty_is_locked(nrsdev)) {
		fprintf(stderr, "nrsdrv: device %s already in use\n", argv[optind + 1]);
		return 1;
	}

	if ((kissfd = open(kissdev, O_RDWR)) == -1) {
		perror("nrsdrv: open kiss device");
		return 1;
	}

	if ((nrsfd = open(nrsdev, O_RDWR)) == -1) {
		perror("nrsdrv: open nrs device");
		return 1;
	}

	tty_lock(kissdev);
	tty_lock(nrsdev);

	if (!tty_raw(kissfd, FALSE)) {
		tty_unlock(kissdev);
		tty_unlock(nrsdev);
		return 1;
	}

	if (!tty_raw(nrsfd, FALSE)) {
		tty_unlock(kissdev);
		tty_unlock(nrsdev);
		return 1;
	}

	if (speed != 0 && !tty_speed(kissfd, speed)) {
		tty_unlock(kissdev);
		tty_unlock(nrsdev);
		return 1;
	}

	if (speed != 0 && !tty_speed(nrsfd, speed)) {
		tty_unlock(kissdev);
		tty_unlock(nrsdev);
		return 1;
	}

	if (logging) {
		openlog("nrsdrv", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "KISS device %s connected to NRS device %s\n", argv[optind + 0], argv[optind + 1]);
	}
		
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, terminate);

	/*
	 * Become a daemon if we can.
	 */
	if (!daemon_start(FALSE)) {
		fprintf(stderr, "nrsdrv: cannot become a daemon\n");
		tty_unlock(kissdev);
		tty_unlock(nrsdev);
		return 1;
	}

	if (flowcontrol) {
		unkey_rts(nrsfd);
	}

	c = ((kissfd > nrsfd) ? kissfd : nrsfd) + 1;

	for (;;) {
		FD_ZERO(&read_fd);
		
		FD_SET(kissfd, &read_fd);
		FD_SET(nrsfd, &read_fd);

		n = select(c, &read_fd, NULL, NULL, NULL);

		if (FD_ISSET(kissfd, &read_fd)) {
			if ((n = read(kissfd, buffer, 512)) <= 0) {
				if (logging) {
					syslog(LOG_INFO, "terminating on KISS device closure\n");
					closelog();
				}
				break;
			}
			kiss_unesc(buffer, n);
		}
		
		if (FD_ISSET(nrsfd, &read_fd)) {
			if ((n = read(nrsfd, buffer, 512)) <= 0) {
				if (logging) {
					syslog(LOG_INFO, "terminating on NRS device closure\n");
					closelog();
				}
				break;
			}
			nrs_unesc(buffer, n);
		}
	}

	tty_unlock(kissdev);
	tty_unlock(nrsdev);

	return 0;
}
