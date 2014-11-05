/* 03.04.1995 add binary download "#BIN#-Protocol"  Alexander Tietzel  */
/* 01.05.1995 talkmode Alexander Tietzel (DG6XA) */
/* 15.07.1995 Pull-Down-Menus Alexander Tietzel (DG6XA) */
/* 17.07.1995 auto7+ newer #BIN#-Protocol Alexander Tietzel(DG6XA) */
/* 18.07.1995 Remote commands Alexander Tietzel (DG6XA) */
/* 19.07.1995 statusline Alexander Tietzel (DG6XA) */
/* 25.07.1995 some bug-fixes Alexander Tietzel (DG6XA) */
/* 14.08.1995 merged with mainstream call.c code Jonathan Naylor (G4KLX) */
/* 01.03.1996 support for different screen sizes, fixed 7plus download (DL1BKE)
              */
/* 19.08.1996 fixed enter key handling (G4KLX) */
/* 27.08.1996 added Rose support (G4KLX) */
/* 30.11.1996 added the called user in the call windows and set talk mode as
              default (IW0FBB) */
/* 07.12.1996 updated status line to cope with callsign, bits and status
              message (VK2KTJ) */
/* 02.02.1997 removed NETROM_PACLEN setting to match Jonathon removing it
              from kernel (VK2KTJ) */

#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <curses.h>

#include <netax25/ax25.h>
#include <netrom/netrom.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/nrconfig.h>
#include <netax25/rsconfig.h>

#include <config.h>

#include "../pathnames.h"

#include "call.h"
#include "crc.h"
#include "menu.h"

#define	CTRL_C		0x03

#define	MAX_PACKETLEN	512
#define	MAX_BUFLEN	2*MAX_PACKETLEN
#define	MAX_CMPSTRLEN	MAX_PACKETLEN

#define	STD_DWN_DIR	"/var/ax25/"

#define	FLAG_RECONNECT	0x01

#define	STATW_BITS	12
#define	STATW_STAT	20

static int backoff = -1;
static int ax25mode = -1;

static int debug = FALSE;
static int af_mode = AF_AX25;
static int window = 0;
static int be_silent = 0;
static char *port = NULL;
static char *mycall = NULL;

static int stdin_is_tty = 1;

int interrupted = FALSE;
int paclen = 0;
int fd;

int wait_for_remote_disconnect = FALSE;
static struct timeval inactivity_timeout;
int inactivity_timeout_is_set = FALSE;
int remote_commands_enabled = TRUE;

#define GP_FILENAME_SIZE	255
typedef struct {
	char file_name[GP_FILENAME_SIZE];
	unsigned long dwn_cnt;
	int dwn_file;
	int file_crc;
	int calc_crc;
	struct utimbuf ut;
	int new_header;
} t_gp;

typedef struct {
	WINDOW *ptr;
	int max_y;
	int max_x;
	char string[MAX_BUFLEN];
	unsigned long bytes;
	int curs_pos;
} t_win;

#define TALKMODE	001	/* two windows (outgoing and incoming) with menu */
#define SLAVEMODE	002	/* Menu mode */
#define RAWMODE         004	/* mode used by earlier versions */

void usage(void)
{
	fprintf(stderr, "usage: call [-b l|e] [-d] [-h] [-m s|e] [-p paclen] [-r] [-R]\n");
	fprintf(stderr, "            [-s mycall] [-t] [-T timeout] [-v] [-w window] [-W]\n");
	fprintf(stderr, "            port callsign [[via] digipeaters...]\n");
	exit(1);
}

WINDOW *win;
const char *key_words[] = { "//",
	"#BIN#",
	" go_7+. ",
	" stop_7+. ",
	"\0"
};
const char *rkey_words[] = {
	// actually restricted keywords are very restrictive
	"\0"
};
#define MAXCMDLEN 10

void convert_cr_lf(char *buf, int len)
{
	while (len--) {
		if (*buf == '\r')
			*buf = '\n';
		buf++;
	}
}

void convert_lf_cr(char *buf, int len)
{
	while (len--) {
		if (*buf == '\n')
			*buf = '\r';
		buf++;
	}
}

void convert_upper_lower(char *buf, int len)
{
	while (len--) {
		*buf = tolower(*buf);
		buf++;
	}
}


/* Convert linear UNIX date to a MS-DOS time/date pair. */

static char * unix_to_sfbin_date_string(time_t gmt)
{

  static char buf[9];
  unsigned short s_time, s_date;

  date_unix2dos(((gmt == -1) ? time(0) : gmt), &s_time, &s_date);
  sprintf(buf, "%X", ((s_date << 16) + s_time));
  return buf;
}

static int nr_convert_call(char *address, struct full_sockaddr_ax25 *addr)
{
	char buffer[100], *call, *alias;
	FILE *fp;
	int addrlen;

	for (call = address; *call != '\0'; call++)
		*call = toupper(*call);

	if ((fp = fopen(PROC_NR_NODES_FILE, "r")) == NULL) {
		fprintf(stderr,
			"call: NET/ROM not included in the kernel\n");
		return -1;
	}
	fgets(buffer, 100, fp);

	while (fgets(buffer, 100, fp) != NULL) {
		call = strtok(buffer, " \t\n\r");
		alias = strtok(NULL, " \t\n\r");

		if (strcmp(address, call) == 0
		    || strcmp(address, alias) == 0) {
			addrlen = ax25_aton(call, addr);
			fclose(fp);
			return (addrlen ==
				-1) ? -1 : sizeof(struct sockaddr_ax25);
		}
	}

	fclose(fp);

	fprintf(stderr, "call: NET/ROM callsign or alias not found\n");

	return -1;
}

static int connect_to(char *address[])
{
	int fd = 0;
	int addrlen = 0;
	union {
		struct full_sockaddr_ax25 ax25;
		struct sockaddr_rose rose;
	} sockaddr;
	char *digi;
	int one = debug;

	switch (af_mode) {
	case AF_ROSE:
		if (address[0] == NULL || address[1] == NULL) {
			fprintf(stderr,
				"call: too few arguments for Rose\n");
			return (-1);
		}
		if ((fd = socket(AF_ROSE, SOCK_SEQPACKET, 0)) < 0) {
			perror("socket");
			return (-1);
		}
		break;

	case AF_NETROM:
		if (address[0] == NULL) {
			fprintf(stderr,
				"call: too few arguments for NET/ROM\n");
			return (-1);
		}
		if ((fd = socket(AF_NETROM, SOCK_SEQPACKET, 0)) < 0) {
			perror("socket");
			return (-1);
		}
		ax25_aton(nr_config_get_addr(port), &sockaddr.ax25);
		sockaddr.ax25.fsa_ax25.sax25_family = AF_NETROM;
		addrlen = sizeof(struct full_sockaddr_ax25);
		break;

	case AF_AX25:
		if (address[0] == NULL) {
			fprintf(stderr,
				"call: too few arguments for AX.25\n");
			return (-1);
		}
		if ((fd = socket(AF_AX25, SOCK_SEQPACKET, 0)) < 0) {
			perror("socket");
			return (-1);
		}
		ax25_aton(ax25_config_get_addr(port), &sockaddr.ax25);
		if (sockaddr.ax25.fsa_ax25.sax25_ndigis == 0) {
			ax25_aton_entry(ax25_config_get_addr(port),
					sockaddr.ax25.fsa_digipeater[0].
					ax25_call);
			sockaddr.ax25.fsa_ax25.sax25_ndigis = 1;
		}
		sockaddr.ax25.fsa_ax25.sax25_family = AF_AX25;
		if (mycall)
			ax25_aton_entry(mycall, sockaddr.ax25.fsa_ax25.sax25_call.ax25_call);
		addrlen = sizeof(struct full_sockaddr_ax25);

		if (setsockopt
		    (fd, SOL_AX25, AX25_WINDOW, &window,
		     sizeof(window)) == -1) {
			perror("AX25_WINDOW");
			close(fd);
			fd = -1;
			return (-1);
		}
		if (setsockopt
		    (fd, SOL_AX25, AX25_PACLEN, &paclen,
		     sizeof(paclen)) == -1) {
			perror("AX25_PACLEN");
			close(fd);
			fd = -1;
			return (-1);
		}
		if (backoff != -1) {
			if (setsockopt
			    (fd, SOL_AX25, AX25_BACKOFF, &backoff,
			     sizeof(backoff)) == -1) {
				perror("AX25_BACKOFF");
				close(fd);
				fd = -1;
				return (-1);
			}
		}
		if (ax25mode != -1) {
			if (setsockopt
			    (fd, SOL_AX25, AX25_EXTSEQ, &ax25mode,
			     sizeof(ax25mode)) == -1) {
				perror("AX25_EXTSEQ");
				close(fd);
				fd = -1;
				return (-1);
			}
		}
		break;
	}

	if (debug
	    && setsockopt(fd, SOL_SOCKET, SO_DEBUG, &one,
			  sizeof(one)) == -1) {
		perror("SO_DEBUG");
		close(fd);
		fd = -1;
		return (-1);
	}
	if (af_mode != AF_ROSE) {	/* Let Rose autobind */
		if (bind(fd, (struct sockaddr *) &sockaddr, addrlen) == -1) {
			perror("bind");
			close(fd);
			fd = -1;
			return (-1);
		}
	}
	switch (af_mode) {
	case AF_ROSE:
		memset(&sockaddr.rose, 0x00, sizeof(struct sockaddr_rose));

		if (ax25_aton_entry
		    (address[0],
		     sockaddr.rose.srose_call.ax25_call) == -1) {
			close(fd);
			fd = -1;
			return (-1);
		}
		if (rose_aton
		    (address[1],
		     sockaddr.rose.srose_addr.rose_addr) == -1) {
			close(fd);
			fd = -1;
			return (-1);
		}
		if (address[2] != NULL) {
			digi = address[2];
			if (strcasecmp(address[2], "VIA") == 0) {
				if (address[3] == NULL) {
					fprintf(stderr,
						"call: callsign must follow 'via'\n");
					close(fd);
					fd = -1;
					return (-1);
				}
				digi = address[3];
			}
			if (ax25_aton_entry
			    (digi,
			     sockaddr.rose.srose_digi.ax25_call) == -1) {
				close(fd);
				fd = -1;
				return (-1);
			}
			sockaddr.rose.srose_ndigis = 1;
		}
		sockaddr.rose.srose_family = AF_ROSE;
		addrlen = sizeof(struct sockaddr_rose);
		break;

	case AF_NETROM:
		if (nr_convert_call(address[0], &sockaddr.ax25) == -1) {
			close(fd);
			fd = -1;
			return (-1);
		}
		sockaddr.rose.srose_family = AF_NETROM;
		addrlen = sizeof(struct sockaddr_ax25);
		break;

	case AF_AX25:
		if (ax25_aton_arglist
		    ((const char **) address, &sockaddr.ax25) == -1) {
			close(fd);
			fd = -1;
			return (-1);
		}
		sockaddr.rose.srose_family = AF_AX25;
		addrlen = sizeof(struct full_sockaddr_ax25);
		break;
	}

	if (!be_silent) {
		printf("Trying...\n");
		fflush(stdout);
	}

	if (connect(fd, (struct sockaddr *) &sockaddr, addrlen)) {
		printf("\n");
		fflush(stdout);
		perror("connect");
		close(fd);
		fd = -1;
		return (-1);
	}
	if (!be_silent) {
		printf("*** Connected to %s\n", address[0]);
		fflush(stdout);
	}

	return (fd);
}

void cmd_intr(int sig)
{
	signal(SIGQUIT, cmd_intr);
	interrupted = TRUE;
}

void statline(int mode, char *s)
{
	static int oldlen = 0;
	int l, cnt;

	if (*s == '\0') {
		if (mode == RAWMODE)
			return;
		if (oldlen > 0) {
			move(0, STATW_STAT);
			attron(A_REVERSE);
			for (cnt = 0; cnt < oldlen; cnt++)
				addch(' ');
			oldlen = 0;
			attroff(A_REVERSE);
			refresh();
		}
		return;
	}
	if (mode == RAWMODE) {
		printf(">>%s\n", s);
		fflush(stdout);
		return;
	}
	if (strlen(s) > 80 - STATW_STAT)
		s[80 - STATW_STAT] = '\0';

	move(0, STATW_STAT);

	attron(A_REVERSE);
	addstr(s);

	if (oldlen > strlen(s)) {
		l = oldlen - strlen(s);
		for (cnt = 0; cnt < l; cnt++)
			addch(' ');
	}
	attroff(A_REVERSE);
	oldlen = strlen(s);
	refresh();
}

WINDOW *opnstatw(int mode, wint * wintab, char *s, int lines, int cols)
{
	WINDOW *win;

	if (mode == RAWMODE) {
		printf(">>%s\n", s);
		fflush(stdout);
		return NULL;
	}
	win =
	    winopen(wintab, lines, cols, ((LINES - 1) - lines) / 2,
		    ((COLS) - cols) / 2, TRUE);
	mvwaddstr(win, 1, 1 + (cols - strlen(s)) / 2, s);
	wmove(win, 3, 2);

	return win;
}

void wrdstatw(WINDOW * win, char s[])
{
	int y;

	if (win == NULL) {
		printf("  %s\n", s);
		fflush(stdout);
		return;
	}
	waddstr(win, s);
	y = getcury(win);
	wmove(win, y + 1, 2);
	wrefresh(win);
}

void dupdstatw(WINDOW * win, char *s, int add)
{
	static char infostr[80];
	static int y, x;
	static int oldlen;
	int l, cnt;

	if (add) {
		oldlen = 0;
		memcpy(infostr, s, sizeof(infostr)-1);
		infostr[sizeof(infostr)-1] = 0;

		if (win == NULL) {
			printf("  %s", s);
			fflush(stdout);
			return;
		}
		waddstr(win, s);
		getyx(win, y, x);
		wrefresh(win);

		return;
	}
	if (win == NULL) {
		printf("\r  %s%s", infostr, s);
		fflush(stdout);
	} else {
		mvwaddstr(win, y, x, s);
	}

	if (oldlen > strlen(s)) {
		l = oldlen - strlen(s);
		for (cnt = 0; cnt < l; cnt++)
			if (win == NULL) {
				printf(" ");
				fflush(stdout);
			} else
				waddch(win, ' ');
	}
	if (win == NULL) {
		fflush(stdout);
	} else {
		wrefresh(win);
	}

	oldlen = strlen(s);
}

int start_ab_download(int mode, WINDOW ** swin, wint * wintab,
		      char parms[], int parmsbytes, char buf[], int bytes,
		      t_gp * gp, char *address[])
{
	int crcst;		/* startposition crc-field  */
	int datest = 0;		/* startposition date-field */
	int namest = 0;		/* startposition name-field */
	int cnt;
	int date = 0;
	struct tm ft;
	char s[80];
	int time_set = 0;

	for (crcst = 2; crcst < parmsbytes - 1 &&
		(!(parms[crcst - 2] == '#' && parms[crcst - 1] == '|')); crcst++);

	if (crcst < parmsbytes - 1) {
		gp->file_crc = atoi(parms + crcst);

		for (datest = crcst; datest < parmsbytes - 1 &&
			(!(parms[datest - 2] == '#' && parms[datest - 1] == '$'));
		     datest++);

		if (datest < parmsbytes -1) {
		  date = (int) strtol(parms + datest, NULL, 16);
		  ft.tm_sec = (date & 0x1F) * 2;
		  date >>= 5;
		  ft.tm_min = date & 0x3F;
		  date >>= 6;
		  ft.tm_hour = date & 0x1F;
		  date >>= 5;
		  ft.tm_mday = date & 0x1F;
		  date >>= 5;
		  ft.tm_mon = (date & 0x0F) -1;
		  date >>= 4;
		  ft.tm_year = (date & 0x7F) + 80;
		  ft.tm_isdst = -1;
		  ft.tm_yday = -1;
		  ft.tm_wday = -1;
		  gp->ut.actime = mktime(&ft);
		  gp->ut.modtime = gp->ut.actime;
		  time_set = 1;
		} else {
		  datest = crcst + 1;
		}

		for (namest = datest; namest < parmsbytes - 1 &&
			(parms[namest - 1] != '#'); namest++);
	}
	
	if (!time_set) {
		time_t t = time(0);
		memcpy(&ft, localtime(&t), sizeof(struct tm));
		gp->ut.actime = mktime(localtime(&t));
		gp->ut.modtime = gp->ut.actime;
	}

	gp->dwn_cnt = (unsigned long ) atol(parms);
	strncpy(gp->file_name, STD_DWN_DIR, GP_FILENAME_SIZE-1);
	gp->file_name[GP_FILENAME_SIZE-1] = 0;


	if (crcst == parmsbytes - 1 || datest - crcst > 7
	    || namest - datest > 10) {
		*swin =
		    opnstatw(mode, wintab,
			     "Remote starts AutoBin transfer", 6, 52);
		gp->new_header = FALSE;
		wrdstatw(*swin, "old styled Header (no filename)");
		if (strlen(gp->file_name) + strlen(address[0]) < GP_FILENAME_SIZE -1)
		  strcat(gp->file_name, address[0]);
		else
		  return -1;
		if (strlen(gp->file_name) + strlen(".dwnfile") < GP_FILENAME_SIZE -1)
		  strcat(gp->file_name, ".dwnfile");
		else
		  return -1;
	} else {
		int len;
		gp->new_header = TRUE;
		for (cnt = parmsbytes - namest;
		     cnt > 0 && !(parms[cnt + namest - 1] == '\\'
		       || parms[cnt + namest - 1] == '/');
		     cnt--);
		len = parmsbytes - namest - cnt;
		if (len < 1)
			return -1;
		if (len > sizeof(s) -1)
			len = sizeof(s) -1;
		strncpy(s, &parms[namest + cnt], len);
		s[len] = 0;
		if (*s == '\r')
			return -1;
		/* convert_upper_lower(s, len); */
		if (strlen(gp->file_name) + len < GP_FILENAME_SIZE - 1)
		  strcat(gp->file_name, s);
		else
		  return -1;

		*swin =
		    opnstatw(mode, wintab,
			     "Remote starts AutoBin transfer", 10, 52);

		sprintf(s, "size of file    : %lu",
			(unsigned long) gp->dwn_cnt);
		wrdstatw(*swin, s);
		sprintf(s, "filename        : %s", gp->file_name);
		wrdstatw(*swin, s);
		sprintf(s, "last mod. date  : %02i.%02i.%04i", ft.tm_mday,
			ft.tm_mon+1 , ft.tm_year + 1900);
		wrdstatw(*swin, s);
		sprintf(s, "last mod. time  : %02i:%02i:%02i", ft.tm_hour,
			ft.tm_min, ft.tm_sec);
		wrdstatw(*swin, s);
	}

	dupdstatw(*swin, "Bytes to receive: ", TRUE);

	if ((gp->dwn_file =
	     open(gp->file_name, O_RDWR | O_CREAT, 0666)) == -1) {
		sprintf(s, "Unable to open %s", gp->file_name);
		statline(mode, s);
		if (write(fd, "#ABORT#\r", 8) == -1) {
			perror("write");
		}
		gp->dwn_cnt = 0;
		gp->file_name[0] = '\0';
		return -1;
	}
	if (bytes == 1) {
		if (write(fd, "#OK#\r", 5) == -1) {
			perror("write");
			close(gp->dwn_file);
			gp->dwn_file = -1;
			gp->dwn_cnt = 0;
			gp->file_name[0] = '\0';
			return -1;
		}
		gp->calc_crc = 0;
	} else {
		unsigned long offset = 0L;
		while (offset != bytes) {
			int ret = write(gp->dwn_file, buf+offset, bytes-offset);
			if (ret == -1) {
				perror("write");
			    	if (errno == EWOULDBLOCK || errno == EAGAIN) {
		 			usleep(100000);
					continue;
				}
				close(gp->dwn_file);
				gp->dwn_file = -1;
				gp->dwn_cnt = 0;
				gp->file_name[0] = '\0';
				return -1;
			}
			if (ret == 0) {
				close(gp->dwn_file);
				gp->dwn_file = -1;
				gp->dwn_cnt = 0;
				gp->file_name[0] = '\0';
				return -1;
				break;
			}
		  	gp->calc_crc = calc_crc((unsigned char *) buf, ret, 0);
		  	gp->dwn_cnt -= ret;
		  	offset += ret;
		}
	}

	return 0;
}

int ab_down(int mode, WINDOW * swin, wint * wintab, char buf[], int *bytes,
	    t_gp * gp)
{
	unsigned long extrach = 0;
	char s[80];

	if (*bytes == 8 && strncmp(buf, "#ABORT#\r", 8) == 0) {
		gp->dwn_cnt = 0;
		close(gp->dwn_file);
		gp->dwn_file = -1;
		statline(mode, "Remote aborts AutoBin transfer!");
		gp->file_name[0] = '\0';
		*bytes = 0;
		if (mode != RAWMODE) {
			delwin(swin);
			winclose(wintab);
		} else {
			printf("\n");
			fflush(stdout);
		}

		return 0;
	}
	if (gp->dwn_cnt < *bytes) {
		extrach = *bytes - gp->dwn_cnt;
		*bytes = gp->dwn_cnt;
	}
	if (write(gp->dwn_file, buf, *bytes) != *bytes) {
		close(gp->dwn_file);
		gp->dwn_file = -1;
		gp->dwn_cnt = 0;
		gp->file_name[0] = '\0';
		statline(mode,
			 "Error while writing download file. Download aborted.");

		if (mode != RAWMODE) {
			delwin(swin);
			winclose(wintab);
		} else {
			printf("\n");
			fflush(stdout);
		}
	} else {
		gp->calc_crc = calc_crc((unsigned char *) buf, *bytes, gp->calc_crc);
		gp->dwn_cnt -= *bytes;

		if (gp->dwn_cnt == 0) {
			if (mode != RAWMODE) {
				delwin(swin);
				winclose(wintab);
			} else {
				printf("\n");
				fflush(stdout);
			}

			strcpy(s, "AutoBin download finished ");
			if (gp->new_header)
				if (gp->calc_crc == gp->file_crc)
					strcat(s, "CRC check ok");
				else {
					strcat(s, "CRC check failed!");
			} else {
				sprintf(s + strlen(s), "CRC=%u",
					gp->calc_crc);
			}
			statline(mode, s);
			close(gp->dwn_file);
			gp->dwn_file = -1;
			utime(gp->file_name, &gp->ut);
		        gp->file_name[0] = '\0';
			if (extrach != 0) {
				memmove(buf, buf + *bytes, extrach);
				*bytes = extrach;
			} else
				*bytes = 0;
		} else {
			sprintf(s, "%u", (unsigned int) gp->dwn_cnt);
			dupdstatw(swin, s, FALSE);
			*bytes = 0;
		}
	}
	return 0;
}

int start_screen(char *call[])
{
	int cnt;
	char idString[12];
	sprintf(idString, " %9.9s ", call[0]);

	if ((win = initscr()) == NULL)
		return -1;

	attron(A_REVERSE);
	move(0, 0);
	addstr(idString);
	addch(ACS_VLINE);
	addstr("--------");
	addch(ACS_VLINE);
	for (cnt = STATW_STAT; cnt <= 80; cnt++)
		addch(' ');
	attroff(A_REVERSE);

	noecho();
	raw();
	nodelay(win, TRUE);
	keypad(win, TRUE);
	refresh();

	return 0;
}

int start_slave_mode(wint * wintab, t_win * win_in, t_win * win_out)
{
	win_in->max_y = LINES - 2;
	win_in->max_x = COLS;
	win_in->ptr =
	    winopen(wintab, win_in->max_y + 1, win_in->max_x, 1, 0, FALSE);
	win_out->ptr = win_in->ptr;

	scrollok(win_in->ptr, TRUE);

	wclear(win_out->ptr);
	wrefresh(win_out->ptr);

	win_out->bytes = 0;
	win_out->curs_pos = 0;
	win_in->bytes = 0;
	win_in->curs_pos = 0;

	return 0;
}

int start_talk_mode(wint * wintab, t_win * win_in, t_win * win_out)
{
	int cnt;
	WINDOW *win;

	win_out->max_y = 4;	/* TXLINES */
	win_out->max_x = COLS;
	win_in->max_y = (LINES - 4) - win_out->max_y;
	win_in->max_x = COLS;

	win_out->ptr =
	    winopen(wintab, win_out->max_y + 1, win_out->max_x,
		    (win_in->max_y + 3), 0, FALSE);
	win_in->ptr =
	    winopen(wintab, win_in->max_y + 1, win_in->max_x, 1, 0, FALSE);
	win =
	    winopen(wintab, 1, win_out->max_x, win_in->max_y + 2, 0,
		    FALSE);

	for (cnt = 0; cnt < COLS; cnt++)
		waddch(win, '-');
	wrefresh(win);

	scrollok(win_in->ptr, TRUE);
	scrollok(win_out->ptr, TRUE);

	wclear(win_out->ptr);
	wrefresh(win_out->ptr);
	wclear(win_in->ptr);
	wrefresh(win_in->ptr);

	win_out->bytes = 0;
	win_out->curs_pos = 0;
	win_in->bytes = 0;
	win_out->curs_pos = 0;

	return 0;
}

int change_mode(int oldmode, int newmode, wint * wintab, t_win * win_in,
		t_win * win_out, char *call[])
{
	switch (oldmode) {
	case RAWMODE:
		if (newmode == TALKMODE) {
			start_screen(call);
			start_talk_mode(wintab, win_in, win_out);
		}
		if (newmode == SLAVEMODE) {
			start_screen(call);
			start_slave_mode(wintab, win_in, win_out);
		}
		break;

	case TALKMODE:
		if (newmode == RAWMODE) {
			wclear(win_out->ptr);
			wrefresh(win_out->ptr);
			wclear(win_in->ptr);
			wrefresh(win_in->ptr);
			wintab->next = 0;
			endwin();
		}
		if (newmode == SLAVEMODE) {
			delwin(win_out->ptr);
			delwin(win_in->ptr);
			wintab->next = 0;
			start_slave_mode(wintab, win_in, win_out);
		}
		break;

	case SLAVEMODE:
		if (newmode == RAWMODE) {
			wclear(win_out->ptr);
			wrefresh(win_out->ptr);
			wintab->next = 0;
			endwin();
		}
		if (newmode == TALKMODE) {
			delwin(win_out->ptr);
			wintab->next = 0;
			start_talk_mode(wintab, win_in, win_out);
		}
		break;
	}

	return newmode;
}

void writeincom(int mode, t_win * win_in, unsigned char buf[], int bytes)
{
	int cnt;

	if (mode & RAWMODE) {
		while (write(STDOUT_FILENO, buf, bytes) == -1) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				usleep(100000);
				continue;
			}
			exit(1);
		}
		return;
	}
	for (cnt = 0; cnt < bytes; cnt++) {
		switch (buf[cnt]) {
		case 201:
		case 218:
			waddch(win_in->ptr, ACS_ULCORNER);
			break;
		case 187:
		case 191:
			waddch(win_in->ptr, ACS_URCORNER);
			break;
		case 200:
		case 192:
			waddch(win_in->ptr, ACS_LLCORNER);
			break;
		case 188:
		case 217:
			waddch(win_in->ptr, ACS_LRCORNER);
			break;
		case 204:
		case 195:
			waddch(win_in->ptr, ACS_LTEE);
			break;
		case 185:
		case 180:
			waddch(win_in->ptr, ACS_RTEE);
			break;
		case 203:
		case 194:
			waddch(win_in->ptr, ACS_TTEE);
			break;
		case 202:
		case 193:
			waddch(win_in->ptr, ACS_BTEE);
			break;
		case 205:
		case 196:
			waddch(win_in->ptr, ACS_HLINE);
			break;
		case 186:
		case 179:
			waddch(win_in->ptr, ACS_VLINE);
			break;
		case 129:
			waddch(win_in->ptr, 252);	/*u umlaut */
			break;
		case 132:
			waddch(win_in->ptr, 228);	/*a umlaut */
			break;
		case 142:
			waddch(win_in->ptr, 196);	/*A umlaut */
			break;
		case 148:
			waddch(win_in->ptr, 246);	/*o umlaut */
			break;
		case 153:
			waddch(win_in->ptr, 214);	/*O umlaut */
			break;
		case 154:
			waddch(win_in->ptr, 220);	/*U umlaut */
			break;
		case 225:
			waddch(win_in->ptr, 223);	/*sz */
			break;
		default:
			{
				if (buf[cnt] > 127)
					waddch(win_in->ptr, '.');
				else
					waddch(win_in->ptr, buf[cnt]);
			}
		}
	}

/*      waddnstr(win_in->ptr, buf, bytes); */
	wrefresh(win_in->ptr);

	return;
}

int getstring(wint * wintab, char text[], char buf[])
{
	int c;
	int ypos = 0, xpos = 0;
	int bytes = 0;

	WINDOW *win = winopen(wintab, 3, COLS, 10, 0, TRUE);

	wmove(win, 1, 2);
	waddstr(win, text);
	wrefresh(win);

	do {
		c = getch();
		if (c != ERR) {
			switch (c) {
			case KEY_BACKSPACE:
			case 127:
				{
					getyx(win, ypos, xpos);
					if (xpos > 0 && bytes > 0) {
						wmove(win, ypos, --xpos);
						waddch(win, ' ');
						wmove(win, ypos, xpos);
						bytes--;
					}
				}
				break;
			case (int) '\n':
			case (int) '\r':
			case KEY_ENTER:
				{
					waddch(win, '\n');
					buf[bytes++] = (char) '\n';
					wrefresh(win);
					buf[bytes] = 0;
				}
				break;
			default:
				{
					waddch(win, (char) c);
					buf[bytes++] = (char) c;
				}
			}
			wrefresh(win);
		}
	}
	while (c != '\n' && c != '\r' && c != KEY_ENTER);
	delwin(win);
	winclose(wintab);
	return 0;
}

int readoutg(t_win * win_out, wint * wintab, menuitem * top, char buf[],
	     int keyesc)
{
	int out_cnt;
	int c;
	int ypos = 0, xpos = 0;
	int value;

	c = getch();
	if (c == ERR)
		return 0;

	if (c == keyesc) {
		if ((value = top_menu(wintab, top, 1)) == 0)
			return 0;
		buf[0] = '~';
		switch (value) {
		case 0x01:
			{
				buf[1] = 'r';
				return 2;
			}
		case 0x02:
			{
				buf[1] = '.';
				return 2;
			}
		case 0x11:
			{
				buf[1] = 'o';
				getstring(wintab,
					  "Please enter filename: ",
					  &buf[2]);
				return strlen(buf);
			}
		case 0x12:
			{
				buf[1] = 'c';
				return 2;
			}
		case 0x13:
		case 0x14:
		case 0x15:
			{
				switch (value) {
				case 0x13:
					buf[1] = 'u';
					break;
				case 0x14:
					buf[1] = 'b';
					break;
				case 0x15:
					buf[1] = 'a';
				}
				getstring(wintab,
					  "Please enter filename: ",
					  buf + 2);
				return strlen(buf);
			}
		case 0x21:
			{
				buf[1] = '1';
				return 2;
			}
		case 0x22:
			{
				buf[1] = '2';
				return 2;
			}
		case 0x23:
			{
				buf[1] = '0';
				return 2;
			}
		case 0x31:
			return -1;
		}
		wrefresh(win_out->ptr);
		return 2;
	}
	switch (c) {
	case KEY_BACKSPACE:
	case 127:
		{
			getyx(win_out->ptr, ypos, xpos);
			if (win_out->bytes > 0) {
				if (win_out->curs_pos < win_out->bytes) {
					mvwaddnstr(win_out->ptr, ypos,
						   --xpos,
						   &win_out->
						   string[win_out->
							  curs_pos],
						   win_out->bytes -
						   win_out->curs_pos);
					waddch(win_out->ptr, ' ');
					memmove(&win_out->
						string[win_out->curs_pos -
						       1],
						&win_out->string[win_out->
								 curs_pos],
						win_out->bytes -
						win_out->curs_pos);
				} else
					mvwaddch(win_out->ptr, ypos,
						 --xpos, ' ');

				wmove(win_out->ptr, ypos, xpos);
				win_out->bytes--;
				win_out->curs_pos--;
			}
		}
		break;
	case KEY_LEFT:
		if (win_out->curs_pos > 0) {
			win_out->curs_pos--;
			getyx(win_out->ptr, ypos, xpos);
			wmove(win_out->ptr, ypos, xpos - 1);
		}
		break;
	case KEY_RIGHT:
		if (win_out->curs_pos < win_out->bytes) {
			win_out->curs_pos++;
			getyx(win_out->ptr, ypos, xpos);
			wmove(win_out->ptr, ypos, xpos + 1);
		}
		break;
	case KEY_ENTER:
	case (int) '\n':
	case (int) '\r':
		{
			if (win_out->curs_pos < win_out->bytes) {
				getyx(win_out->ptr, ypos, xpos);
				wmove(win_out->ptr, ypos,
				      xpos + win_out->bytes -
				      win_out->curs_pos);
			}
			waddch(win_out->ptr, '\n');
			win_out->string[win_out->bytes++] = (char) '\n';
			wrefresh(win_out->ptr);
			strncpy(buf, win_out->string, win_out->bytes);
			wrefresh(win_out->ptr);
			out_cnt = win_out->bytes;
			win_out->bytes = 0;
			win_out->curs_pos = 0;
			return out_cnt;
		}
		break;
	default:
		{
			waddch(win_out->ptr, (char) c);
			if (win_out->curs_pos < win_out->bytes) {
				getyx(win_out->ptr, ypos, xpos);
				waddnstr(win_out->ptr,
					 &win_out->string[win_out->
							  curs_pos],
					 win_out->bytes -
					 win_out->curs_pos);
				memmove(&win_out->
					string[win_out->curs_pos + 1],
					&win_out->string[win_out->
							 curs_pos],
					win_out->bytes -
					win_out->curs_pos);
				win_out->string[win_out->curs_pos] =
				    (char) c;
				wmove(win_out->ptr, ypos, xpos);
			} else
				win_out->string[win_out->bytes] = (char) c;

			win_out->bytes++;
			win_out->curs_pos++;
		}
	}
	wrefresh(win_out->ptr);
	return 0;
}
void writemsg(char fname[], char caller[])
{
	char text_row[255];
	char *text_ptr;
	char buf[255];
	FILE *f = fopen(fname, "r");

	if (f == NULL) {
		perror(fname);
		return;
	}
	do {
		if (fgets(text_row, 255, f) != 0) {
			text_row[strlen(text_row) - 1] = '\r';
			text_ptr = strchr(text_row, '$');
			if (text_ptr != NULL) {
				strcpy(buf, text_ptr + 2);
				switch (*(text_ptr + 1)) {
				case 'c':
					{
						strcpy(text_ptr, caller);
						strcat(text_ptr, buf);
					}
				}
			}
			write(fd, text_row, strlen(text_row));
		}
	}
	while (!feof(f));
}

void remotecommand(char buf[], int bytes)
{
	int firstchar;
	if (bytes == 0)
		return;

	switch (buf[0]) {
	case 'e':
	case 'E':
		{
			for (firstchar = 0; buf[firstchar] != ' ';
			     firstchar++);
			firstchar++;
			buf[bytes] = '\n';
			convert_lf_cr(buf + firstchar,
				      bytes - firstchar + 1);
			write(fd, buf + firstchar, bytes - firstchar + 1);
		}
		break;
	default:
		write(fd, "Unknown command\r", 16);
	}
}

int compstr(const char st1[], char st2[], int maxbytes)
{
	int cnt;
	for (cnt = 0;
	     st1[cnt] == st2[cnt] && cnt + 1 < maxbytes
	     && st1[cnt + 1] != 0; cnt++);
	if (st1[cnt] != st2[cnt])
		return -1;

	if (st1[cnt + 1] == 0)
		return 0;
	if (cnt == maxbytes - 1)
		return -2;

	return -1;
}

int eol(char c)
{

	if (c == '\r' || c == '\n' || c == 0x1A)
		return TRUE;
	else
		return FALSE;
}

int searche_key_words(char buf[], int *bytes, char *parms, int *parmsbytes,
		      char restbuf[], int *restbytes)
{
	static char cmpstr[MAX_CMPSTRLEN];
	static int cmpstrbyte = 0;
	static int command = -1;
        const char **pkey_words = remote_commands_enabled ? key_words : rkey_words;

	int cmdstpos = 0;
	int cnt = 0;
	int t = 0;


	if (cmpstrbyte != 0) {
		memmove(buf + cmpstrbyte, buf, *bytes);
		*bytes += cmpstrbyte;
		strncpy(buf, cmpstr, cmpstrbyte);
		cmpstrbyte = 0;
		for (cnt = 0; !eol(buf[cnt]) && cnt < *bytes - 1; cnt++);
		if (cnt == *bytes - 1 && !eol(buf[cnt])) {
			printf("Problem!!!\n");
			fflush(stdout);
			command = -1;
			*restbytes = 0;
			*parmsbytes = 0;
			return -1;
		}
	}
	if (command == -1) {
		cnt = 0;
		do {
			command = 0;
			cmdstpos = cnt;

			t = -1;
			while (*pkey_words[command] != '\0') {
				if ((t =
				     compstr(pkey_words[command], &buf[cnt],
					     *bytes - cnt)) != -1)
					break;
				command++;
			}

			for (; !eol(buf[cnt]) && cnt < *bytes - 1; cnt++);

			if (cnt < *bytes - 1)
				cnt++;
			else
				break;
		} while (t == -1);

		if (t < 0)
			command = -1;
	}
	if (t == -2
	    || (command != -1 && cnt == *bytes - 1
		&& !eol(buf[*bytes - 1]))) {
		cmpstrbyte = *bytes - cmdstpos;
		strncpy(cmpstr, &buf[cmdstpos], cmpstrbyte);
		cmpstr[cmpstrbyte] = 0;
		*bytes -= cmpstrbyte;
		*restbytes = 0;
		return -1;
	}
	if (t == -1) {
		command = -1;
		cmpstrbyte = 0;
		*restbytes = 0;
		return -1;
	}
	t = cmdstpos + strlen(pkey_words[command]);
	*restbytes = *bytes - cnt;
	strncpy(parms, &buf[t], cnt - t);
	*parmsbytes = cnt - t;
	strncpy(restbuf, buf + cnt, *restbytes);
	*bytes = cmdstpos;

	t = command;
	command = -1;
	return t;
}

int sevenplname(int mode, WINDOW ** swin, wint * wintab, int *f,
		int *logfile, char parms[], int parmsbytes, char buf[],
		int bytes)
{
	int cnt;
	int part;
	int nrparts;
	int lines;
	char orgn[13];
	char prtn[13+3];
	char strn[PATH_MAX];
	char v[20];
	char s[80];
	if (parmsbytes >= 40)
		if (strcmp(" of ", &parms[3]) == 0
		    || parmsbytes < 41
		    || parms[10] != ' '
		    || parms[23] != ' '
		    || parms[31] != ' '
		    || parms[36] != ' ' || parms[40] != ' ') {
			return -1;
		}
	part = atof(parms);
	lines = (int) strtol(parms + 37, NULL, 16);
	nrparts = (int) strtol(parms + 7, NULL, 10);

	strncpy(orgn, &parms[11], 12);
	convert_upper_lower(orgn, 12);
	for (cnt = 11; orgn[cnt] == ' '; cnt--) {
		if (cnt == 0)
			break;
	}
	orgn[cnt + 1] = 0;
	if (orgn[cnt - 3] == '.') {
		strncpy(prtn, orgn, cnt - 2);
		if (nrparts == 1)
			sprintf(prtn + cnt - 2, "7pl");
		else
			sprintf(prtn + cnt - 2, "p%02x", part);
	} else {
		strcpy(prtn, orgn);
		if (nrparts == 1)
			sprintf(prtn + cnt, ".7pl");
		else
			sprintf(prtn + cnt, ".p%02x", part);
	}

	strcpy(strn, STD_DWN_DIR);
	strcat(strn, prtn);

	for (cnt = 0; cnt + 41 < parmsbytes && parms[cnt + 41] != ')'; cnt++) ;
	if (parms[cnt + 41] != ')') {
		return -1;
	}
	if (cnt > sizeof(v)-2)
		cnt = sizeof(v)-2;
	strncpy(v, &parms[41], cnt + 1);
	v[cnt + 1] = 0;
	*swin =
	    opnstatw(mode, wintab, "Remote starts 7+ Download", 11, 55);
	sprintf(s, "7plus version        : %-.55s", v);
	wrdstatw(*swin, s);
	sprintf(s, "Name of decoded file : %-.55s", orgn);
	wrdstatw(*swin, s);
	sprintf(s, "Storagename          : %-.55s", strn);
	wrdstatw(*swin, s);
	sprintf(s, "Parts                : %i", nrparts);
	wrdstatw(*swin, s);
	sprintf(s, "Number of this Part  : %i", part);
	wrdstatw(*swin, s);
	sprintf(s, "Lines                : %i", lines);
	wrdstatw(*swin, s);
	dupdstatw(*swin, "Outstanding lines    : ", TRUE);

	if (*f != -1) {
		close(*f);
		*f = -1;
	}
	if ((*f = open(strn, O_RDWR | O_APPEND | O_CREAT, 0666)) == -1) {
		sprintf(s, "Unable to open %s", strn);
		statline(mode, s);
		return -1;
	} else if (*logfile != -1) {
		sprintf(s, "*** 7plus download into file: %s ***\n", strn);
		write(*logfile, s, strlen(s));
	}

	write(*f, key_words[2], strlen(key_words[2]));
	convert_cr_lf(parms, parmsbytes);
	write(*f, parms, parmsbytes);

	return lines;
}
void statbits(int mode, char stat, int m)
{
	if (mode == RAWMODE)
		return;
	move(0, STATW_BITS + m);
	attron(A_REVERSE);
	addch(stat);
	attroff(A_REVERSE);
	refresh();
	return;
}


int cmd_call(char *call[], int mode)
{
	menuitem con[] = {
		{"~Reconnect", 'R', M_ITEM, (void *) 0x01},
		{"~Exit", 'E', M_ITEM, (void *) 0x02},
		{"\0", 0, M_END, 0}
	};

	menuitem fil[] = {
		{"~Open Logfile", 'O', M_ITEM, 0},
		{"~Close Logfile", 'C', M_ITEM, 0},
		{"Send ~Textfile", 'T', M_ITEM, 0},
		{"Send ~Binary", 'B', M_ITEM, 0},
		{"Send ~AutoBin", 'A', M_ITEM, 0},
		{"\0", 0, M_END, 0}
	};

	menuitem mod[] = {
		{"~Slavemode", 'S', M_ITEM, 0},
		{"~Talkmode", 'T', M_ITEM, 0},
		{"~Rawmode", 'R', M_ITEM, 0},
		{"\0", 0, M_END, 0}
	};

	menuitem win[] = {
		{"~Clear", 'C', M_ITEM, 0},
		{"~Resize", 'R', M_ITEM, 0},
		{"\0", 0, M_END, 0}
	};

	menuitem top[] = {
		{"~Connect", 'C', M_P_DWN, con},
		{"~File", 'F', M_P_DWN, fil},
		{"~Mode", 'M', M_P_DWN, mod},
		{"~Window", 'W', M_P_DWN, win},
		{"\0", 0, M_END, 0}
	};

	wint wintab;
	fd_set sock_read;
	fd_set sock_write;
	char buf[MAX_BUFLEN];
	/* char restbuf[MAX_PACKETLEN]; */
	char restbuf[MAX_BUFLEN];
	/* char parms[256]; */
	char parms[MAX_BUFLEN];
	int sevenplus = FALSE;
	int sevenplcnt = 0;
	int bytes;
	int restbytes = 0;
	int parmsbytes;
	int com_num;
	int logfile = -1;
	int uploadfile = -1;
	int downloadfile = -1;
	int binup = FALSE;
	unsigned long uplsize = 0;
	unsigned long uplpos = 0;
	char uplbuf[128];	/* Upload buffer */
	int upldp = 0;
	int upllen = 0;
	char *c, *t;
	t_gp gp;
	t_win win_in;
	t_win win_out;
	WINDOW *swin = 0;
	int cnt;
	int crc = 0;
	char s[80];
	int flags = 0;
	int EOF_on_STDIN = FALSE;

	init_crc();

	if (paclen > sizeof(buf))
		paclen = sizeof(buf);

	gp.dwn_cnt = 0;
	wintab.next = 0;

	if ((fd = connect_to(call)) == -1)
		return FALSE;

	interrupted = FALSE;
	signal(SIGQUIT, cmd_intr);
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	fcntl(fd, F_SETFL, O_NONBLOCK);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

	if (mode != RAWMODE)
		start_screen(call);
	switch (mode) {
	case TALKMODE:
		start_talk_mode(&wintab, &win_in, &win_out);
		break;
	case SLAVEMODE:
		start_slave_mode(&wintab, &win_in, &win_out);
		break;
	case RAWMODE:
		if (!be_silent) {
			printf("Rawmode\n");
			fflush(stdout);
		}
	}

	while (TRUE) {
		struct timeval tv;
		if (inactivity_timeout_is_set == TRUE && uploadfile == -1 && downloadfile == -1) {
			tv.tv_sec = inactivity_timeout.tv_sec;
			tv.tv_usec = inactivity_timeout.tv_usec;
		} else {
			tv.tv_sec = 0;
			tv.tv_usec = 10;
		}
		FD_ZERO(&sock_read);
		if (EOF_on_STDIN == FALSE)
			FD_SET(STDIN_FILENO, &sock_read);
		FD_SET(fd, &sock_read);
		FD_ZERO(&sock_write);

		if (uploadfile != -1)
			FD_SET(fd, &sock_write);

		if (select(fd + 1, &sock_read, &sock_write, NULL, (uploadfile == -1 && downloadfile == -1 && inactivity_timeout_is_set == FALSE) ? NULL : &tv) == -1) {
			if (!interrupted && errno == EAGAIN) {
				usleep(100000);
				continue;
			}
			if (!interrupted)
				perror("select");
			break;
		}
		if (inactivity_timeout_is_set == TRUE && !FD_ISSET(fd, &sock_read) && !FD_ISSET(STDIN_FILENO, &sock_read)) {
			if (!be_silent) {
				printf("*** Inactivity timeout reached. Terminating..\n");
				fflush(stdout);
			}
			break;
		}
		if (FD_ISSET(fd, &sock_read)) {
			/* bytes = read(fd, buf, 511); */
			bytes = read(fd, buf, sizeof(buf));
			if (bytes == 0) {
				/* read EOF on connection */
				/* cause program to terminate */
				flags &= ~FLAG_RECONNECT;
				break;
			}
			if (bytes == -1) {
			    	if (errno == EWOULDBLOCK || errno == EAGAIN) {
				 	usleep(100000);
					continue;
				}
				if (errno != ENOTCONN)
			 		perror("read");
				break;
			}
			if (gp.dwn_cnt != 0) {
				ab_down(mode, swin, &wintab, buf, &bytes,
					&gp);
				if (bytes == 0) {
					continue;
				}
			}
			do {
				/* imagine one line ("bar go_7+") was split into
				 * two packets: 1. "...foo\nbar" 2. " go_7+. ..."
				 * then searche_key_words misinterprets " go_7+. "
				 * as start of a line.
				 */
				com_num = searche_key_words(buf, &bytes, parms, &parmsbytes, restbuf, &restbytes);
				if (bytes != 0) {
					convert_cr_lf(buf, bytes);
					if (!sevenplus) {

						writeincom(mode, &win_in,
							   (unsigned char * ) buf, bytes);
					} else {
						for (cnt = 0; cnt < bytes;
						     cnt++)
							if (eol(buf[cnt]))
								sevenplcnt--;
						dupdstatw(swin, s, FALSE);
					}
					if (downloadfile != -1) {
						if (write
						    (downloadfile, buf,
						     bytes) != bytes) {
							close
							    (downloadfile);
							downloadfile = -1;
							statline(mode,
								 "Error while writing file. Downloadfile closed.");
						}
					} else if (logfile != -1) {
						if (write
						    (logfile, buf,
						     bytes) != bytes) {
							close(logfile);
							logfile = -1;
							statline(mode,
								 "Error while writing log. Log closed.");
						}
					}
				}
				switch (com_num) {
				case 0:
					{
#if 0
						/*
						   FIXME! We should, no: WE MUST be able to turn off
						   all remote commands to avoid mail bombs generating
						   offensive mails with //e while sucking the BBS
						 */
						remotecommand(parms,
							      parmsbytes);
#endif
					}
					break;
				case 1:
					{
						start_ab_download(mode,
								  &swin,
								  &wintab,
								  parms,
								  parmsbytes,
								  restbuf,
								  restbytes,
								  &gp,
								  call);
						restbytes = 0;
					}
					break;
				case 2:
					{
						if ((sevenplcnt =
						     sevenplname(mode,
								 &swin,
								 &wintab,
								 &downloadfile,
								 &logfile,
								 parms,
								 parmsbytes,
								 buf,
								 bytes)) !=
						    -1)
							sevenplus = TRUE;
					}
					break;
				case 3:
					{
						if (!sevenplus)
							break;
						write(downloadfile,
						      key_words[3],
						      strlen(key_words
							     [3]));
						convert_cr_lf(parms,
							      parmsbytes);
						write(downloadfile, parms,
						      parmsbytes);
						if (mode != RAWMODE) {
							delwin(swin);
							winclose(&wintab);
						} else {
							printf("\n");
							fflush(stdout);
						}
						statline(mode,
							 "7+ Download finished.");
						sevenplus = FALSE;
						close(downloadfile);
						downloadfile = -1;
					}
					break;
				}

				memcpy(buf, restbuf, restbytes);
				bytes = restbytes;
			}
			while (restbytes != 0);
		}
		if (FD_ISSET(STDIN_FILENO, &sock_read)) {
			if ((mode & RAWMODE) == RAWMODE) {
				/* bytes = read(STDIN_FILENO, buf, 511); */
				bytes = read(STDIN_FILENO, buf, paclen);
				/* bytes == 0? select() indicated that there
				 * is data to read, but read() returned 0
				 * bytes. -> terminate normaly
                                 */
				if (bytes == 0)
					EOF_on_STDIN = TRUE;
				else if (bytes == -1) {
					if (errno == EWOULDBLOCK || errno == EAGAIN) {
						usleep(100);
					} else {
						perror("input");
						EOF_on_STDIN = TRUE;
					}
				}
			} else {
				bytes =
				    readoutg(&win_out, &wintab, top, buf,
					     0x1d);
				if (bytes == -1) {
					wclear(win_in.ptr);
					wrefresh(win_in.ptr);
					wclear(win_out.ptr);
					wrefresh(win_out.ptr);
					bytes = 0;
				}
			}
			if (bytes > 0)
				statline(mode, "");

			if (bytes > 1 && *buf == '~' && stdin_is_tty) {
				buf[bytes] = 0;

				switch (buf[1]) {
				case '.':
					{
						bytes = 0;
						interrupted = TRUE;
					}
					break;
				case '!':
					change_mode(mode, RAWMODE, &wintab,
							&win_in, &win_out,
							call);
					if (buf[2] != '\0'
							&& buf[2] != '\n') {
						c = buf + 2;
						if ((t =
									strchr(c,
										'\n')) != NULL)
							*t = '\0';
					} else {
						if ((c =
									getenv("SHELL")) ==
								NULL)
							c = "/bin/sh";
					}

					fcntl(STDIN_FILENO, F_SETFL, 0);
					printf("\n[Spawning subshell]\n");
					fflush(stdout);
					system(c);
					printf
						("\n[Returned to connect]\n");
					fflush(stdout);
					fcntl(STDIN_FILENO, F_SETFL,
							O_NONBLOCK);
					change_mode(RAWMODE, mode, &wintab,
							&win_in, &win_out,
							call);
					continue;
				case 'z':
				case 'Z':
				case 'Z' - 64:
					fcntl(STDIN_FILENO, F_SETFL, 0);
					kill(getpid(), SIGSTOP);
					statline(mode, "Resumed");
					fcntl(STDIN_FILENO, F_SETFL,
							O_NONBLOCK);
					continue;
				case '?':
				case 'h':
				case 'H':
					printf("\nTilde escapes:\n");
					printf(".   close\n");
					printf("~   send ~\n");
					printf("r   reconnect\n");
					printf("!   shell\n");
					printf("Z   suspend program. Resume with \"fg\"\n");
					printf("s   Stop upload\n");
					printf("o   Open log\n");
					printf("c   Close log\n");
					printf("0   Switch GUI to \"RAW\" (line) mode - already here ;)\n");
					printf("1   Switch GUI to \"Slave\" mode\n");
					printf("2   Switch GUI to \"Talk\" (split) mode\n");
					printf("u   Upload\n");
					printf("a   Upload (autobin protocol)\n");
					printf("b   Upload binary data (crlf conversion)\n");
					printf("yd  YAPP Download\n");
					printf("yu  YAPP Upload\n");
					fflush(stdout);
					continue;
				case 'S':
				case 's':
					if (uploadfile != -1) {
						statline(mode,
								"Upload file closed");
						close(uploadfile);
						uploadfile = -1;
					} else {
						statline(mode,
								"No upload in progress");
					}
					continue;
				case 'A':
				case 'a':
				case 'b':
				case 'B':
				case 'u':
				case 'U':
					if (uploadfile != -1) {
						statline(mode,
								"Already uploading");
						continue;
					}
					if ((t =
								strchr(buf, '\n')) != NULL)
						*t = '\0';
					t = buf + 2;
					while (*t != '\0' && isspace(*t))
						t++;
					if (*t == '\0') {
						statline(mode,
								"Upload requires a filename");
						continue;
					}
					uploadfile = open(t, O_RDONLY);
					if (uploadfile == -1) {
						statline(mode,
								"Unable to open upload file");
						continue;
					}
					if (lseek(uploadfile, 0L, SEEK_END)
							!= -1)
						uplsize =
							lseek(uploadfile, 0L,
									SEEK_CUR);
					else
						uplsize = 0;
					lseek(uploadfile, 0L, SEEK_SET);
					uplpos = 0;
					upldp = -1;
					upllen = 0;
					if (uplsize != -1) {
						sprintf(s,
								"Uploading %ld bytes from %s",
								uplsize, t);
						swin =
							opnstatw(mode, &wintab,
									s, 6, 50);
						dupdstatw(swin,
								"bytes sent   : ",
								TRUE);
					} else {
						sprintf(s,
								"Uploading from %s",
								t);
						swin =
							opnstatw(mode, &wintab,
									s, 6, 50);
						dupdstatw(swin,
								"bytes sent   : ",
								TRUE);
					}
					switch (buf[1]) {
						case 'a':
						case 'A':
							{
								struct stat statbuf;
								time_t file_time = 0L;
								binup = TRUE;
								crc = 0;
								if (!fstat(uploadfile, &statbuf))
									file_time =  statbuf.st_mtime;
								else
									file_time = time(0);


								do {
									upllen =
										read
										(uploadfile,
										 uplbuf,
										 128);

									if (upllen
											==
											-1) {
										close
											(uploadfile);
										uploadfile
											=
											-1;
										delwin
											(swin);
										winclose
											(&wintab);
										sprintf
											(s,
											 "Error reading upload file: upload aborted");
										statline
											(mode,
											 s);
										break;
									}
									crc =
										calc_crc
										((unsigned char *) uplbuf,
										 upllen,
										 crc);
								} while (upllen > 0);
								lseek(uploadfile,
										0L,
										SEEK_SET);
								sprintf(s,
										"#BIN#%ld#|%u#$%s#%s\r",
										uplsize,
										crc,
										unix_to_sfbin_date_string(file_time),
										t);
								if ( write(fd, s,
											strlen(s)) != strlen(s)) {
									perror("write");
									exit(1);
								}
								uplpos = 0;
								upldp = -1;
								upllen = 0;
							}
							break;
						case 'b':
						case 'B':
							binup = TRUE;
							break;
						case 'u':
						case 'U':
							binup = FALSE;
					}
					continue;
				case 'O':
				case 'o':
					if ((t =
								strchr(buf, '\n')) != NULL)
						*t = '\0';
					if (logfile != -1) {
						close(logfile);
						logfile = -1;
					}
					if (downloadfile != -1) {
						close(downloadfile);
						downloadfile = -1;
					}
					t = buf + 2;
					while (*t != '\0' && isspace(*t))
						t++;
					if (*t == '\0')
						t = "logfile.txt";
					if ((logfile =
								open(t,
									O_RDWR | O_APPEND |
									O_CREAT, 0666)) == -1) {
						sprintf(s,
								"Unable to open %s",
								buf + 2);
						statline(mode, s);
					} else
						statbits(mode, 'L', 1);
					continue;
				case 'C':
				case 'c':
					if (logfile != -1) {
						close(logfile);
						logfile = -1;
						statbits(mode, '-', 1);
					} else {
						statline(mode,
								"Log file not open");
					}
					continue;
				case 'Y':
				case 'y':
					cmd_yapp(buf + 2, bytes - 2);
					continue;
				case '~':
					bytes--;
					memmove(buf, buf + 1, strlen(buf));
					break;
				case 'R':
				case 'r':
					flags |= FLAG_RECONNECT;
					bytes = 0;
					interrupted = TRUE;
					continue;
				case '0':
					mode =
					    change_mode(mode, RAWMODE,
							&wintab, &win_in,
							&win_out, call);
					continue;
				case '1':
					mode =
					    change_mode(mode, SLAVEMODE,
							&wintab, &win_in,
							&win_out, call);
					continue;
				case '2':
					mode =
					    change_mode(mode, TALKMODE,
							&wintab, &win_in,
							&win_out, call);
					continue;
				default:
					statline(mode,
						 "Unknown '~' escape. Type ~h for a list");
					continue;
				}
			}

			if (interrupted)
				break;

			if (bytes > 0) {
				unsigned long offset = 0L;
				int err = 0;

				sevenplus = FALSE;
				if (uploadfile != -1) {
					statline(mode,
						 "Ignored. Type ~s to stop upload");
					continue;
				}
				convert_lf_cr(buf, bytes);

				while (offset != bytes) {
					int len = (bytes-offset > paclen) ? paclen : bytes-offset;
					int ret = write(fd, buf+offset, len);
					if (ret == 0)
						break;
					if (ret == -1) {
			    			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				 			usleep(100000);
							continue;
						}
						perror("write");
						err = 1;
						break;
					}
					offset += ret;
				}
				if (err)
					break;
			}

			if (EOF_on_STDIN == TRUE && wait_for_remote_disconnect == FALSE)
				break;
		}
		if (uploadfile != -1) {
			if (uplsize == 0) {
				close(uploadfile);
				uploadfile = -1;
				delwin(swin);
				winclose(&wintab);
				statline(mode,
					 "Upload complete: 0 bytes");
				continue;
			}
			if (upldp == -1) {
				upllen =
				    read(uploadfile, uplbuf, 128);

				if (upllen == 0) {
					close(uploadfile);
					uploadfile = -1;
					delwin(swin);
					winclose(&wintab);
					sprintf(s,
						"Upload complete: %ld bytes",
						uplpos);
					statline(mode, s);
					continue;
				}
				if (upllen == -1) {
					close(uploadfile);
					uploadfile = -1;
					delwin(swin);
					winclose(&wintab);
					sprintf(s,
						"Error reading upload file: upload aborted at %ld bytes",
						uplpos);
					statline(mode, s);
					continue;
				}
				if (!binup)
					convert_lf_cr(uplbuf,
						      upllen);

				upldp = 0;
			}
			bytes =
			    write(fd, uplbuf + upldp,
				  upllen - upldp);

			if (bytes == 0) {
				break;
			}
			if (bytes == -1) {
				if (errno != EWOULDBLOCK && errno != EAGAIN) {
					sprintf(s, "Write error during upload. Connection lost");
					statline(mode, s);
					perror("write");
					break;
			  	}
				usleep(100000);
				continue;
			}
/*                      if (uplpos / 1024 != (uplpos + bytes) / 1024)
                           { */ /*      printf("\r%ld bytes sent    ", uplpos + bytes); */
				sprintf(s, "%ld", uplpos + bytes);
				dupdstatw(swin, s, FALSE);
/*                      } */

			uplpos += bytes;
			upldp += bytes;

			if (upldp >= upllen)
				upldp = -1;
		}
	}

	close(fd);
	fd = -1;

	if (logfile != -1) {
		close(logfile);
		logfile = -1;
	}
	if (downloadfile != -1) {
		close(downloadfile);
		downloadfile = -1;
	}
	if (gp.dwn_cnt != 0) {
		close(gp.dwn_file);
		gp.dwn_file = -1;
		gp.dwn_cnt = 0;
		gp.file_name[0] = '\0';
	}
	fcntl(STDIN_FILENO, F_SETFL, 0);

	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_DFL);

	if (mode != RAWMODE)
		endwin();

	if (!be_silent) {
		printf("*** Cleared\n");
		fflush(stdout);
	}

	if (flags & FLAG_RECONNECT) {
		return TRUE;
	} else {
		return FALSE;
	}
}


int main(int argc, char **argv)
{
	int p;
	int mode = TALKMODE;

	if (!isatty(STDIN_FILENO))
		stdin_is_tty = 0;

 	setlinebuf(stdin);

	while ((p = getopt(argc, argv, "b:dhm:p:rs:RStT:vw:W")) != -1) {
		switch (p) {
		case 'b':
			if (*optarg != 'e' && *optarg != 'l') {
				fprintf(stderr,
					"call: invalid argument for option '-b'\n");
				return 1;
			}
			backoff = *optarg == 'e';
			break;
		case 'd':
			debug = TRUE;
			break;
		case 'h':
			mode = SLAVEMODE;
			break;
		case 'm':
			if (*optarg != 's' && *optarg != 'e') {
				fprintf(stderr,
					"call: invalid argument for option '-m'\n");
				return 1;
			}
			ax25mode = *optarg == 'e';
			break;
		case 'p':
			if ((paclen = atoi(optarg)) == 0) {
				fprintf(stderr,
					"call: option '-p' requires a numeric argument\n");
				return 1;
			}
			if (paclen < 1 || paclen > 500) {
				fprintf(stderr,
					"call: paclen must be between 1 and 500\n");
				return 1;
			}
			break;
		case 'r':
			mode = RAWMODE;
			break;
		case 's':
			mycall = strdup(optarg);
			break;
		case 'R':
			remote_commands_enabled = FALSE;
		case 'S':
			be_silent = 1;
			break;
		case 'T':
			{ double f = atof(optarg);
			  inactivity_timeout.tv_sec = ((time_t) f) & 0x7fffffff;
			  inactivity_timeout.tv_usec = (time_t ) (f - (double ) (time_t ) f);
			  if (f < 0.001 || f > (double) (0x7fffffff) || (inactivity_timeout.tv_sec == 0 && inactivity_timeout.tv_usec == 0)) {
			  	fprintf(stderr, "call: option '-T' must be > 0.001 (1ms) and < 69 years\n");
				return 1;
			  }
			  inactivity_timeout_is_set = TRUE;
			}
			break;
		case 't':
			mode = TALKMODE;
			break;
		case 'v':
			printf("call: %s\n", VERSION);
			return 0;
		case 'w':
			if ((window = atoi(optarg)) == 0) {
				fprintf(stderr,
					"call: option '-w' requires a numeric argument\n");
				return 1;
			}
			if (ax25mode) {
				if (window < 1 || window > 63) {
					fprintf(stderr,
						"call: window must be between 1 and 63 frames\n");
					return 1;
				}
			} else {
				if (window < 1 || window > 7) {
					fprintf(stderr,
						"call: window must be between 1 and 7 frames\n");
					return 1;
				}
			}
			break;
		case 'W':
			wait_for_remote_disconnect = TRUE;
			break;
		case '?':
		case ':':
			usage();
		}
	}

	if (optind == argc || optind == argc - 1) {
		usage();
	}
	port = argv[optind];

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "call: no AX.25 port data configured\n");
		return 1;
	}
	if (ax25_config_get_addr(port) == NULL) {
		nr_config_load_ports();

		if (nr_config_get_addr(port) == NULL) {
			rs_config_load_ports();

			if (rs_config_get_addr(port) == NULL) {
				fprintf(stderr,
					"call: invalid port setting\n");
				return 1;
			} else {
				af_mode = AF_ROSE;
			}
		} else {
			af_mode = AF_NETROM;
		}
	} else {
		af_mode = AF_AX25;
	}

	switch (af_mode) {
	case AF_ROSE:
		paclen = rs_config_get_paclen(port);
		break;

	case AF_NETROM:
		if (paclen == 0)
			paclen = nr_config_get_paclen(port);
		break;
	case AF_AX25:
		if (window == 0)
			window = ax25_config_get_window(port);
		if (paclen == 0)
			paclen = ax25_config_get_paclen(port);
		break;
	}

	
	if (!be_silent) {
		printf("GW4PTS AX.25 Connect v1.11\n");
		fflush(stdout);
	}
	while (cmd_call(argv + optind + 1, mode)) {
		if (!be_silent) {
			printf("Wait 60 sec before reconnect\n");
			fflush(stdout);
		}
		sleep(60);
	}

	return 0;
}
