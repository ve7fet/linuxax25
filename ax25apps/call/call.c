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

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include <sys/types.h>
#include <utime.h>
#include <limits.h>
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
#include <wchar.h>
#include <wctype.h>
#include <ncursesw/ncurses.h>
#include <locale.h>
#include <iconv.h>
#include <sys/ioctl.h>

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
static int window;
static int be_silent;
static char *port;
static char *mycall;

static int stdin_is_tty = 1;
static iconv_t ibm850toutf8, wcharttoibm850, wcharttoutf8, utf8towchart;

volatile int interrupted = FALSE;
static int sigwinchsignal;
int paclen;
int fd;
static int curson = 1;

static int wait_for_remote_disconnect = FALSE;
static struct timespec inactivity_timeout;
static int inactivity_timeout_is_set = FALSE;
static int remote_commands_enabled = TRUE;

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
	wchar_t string[MAX_BUFLEN];
	unsigned long bytes;
	int curs_pos;
} t_win;

#define TALKMODE	001	/* two windows (outgoing and incoming) with menu */
#define SLAVEMODE	002	/* Menu mode */
#define RAWMODE		004	/* mode used by earlier versions */

#define ORIGINALENCODING	0
#define UTF8ENCODING		1

#define SCROLLBACKSIZE 5000

typedef struct {
	char *str;
	int len;
}scrollbackstruct;

static scrollbackstruct scrollback[SCROLLBACKSIZE];
static int topscroll, lastscroll, scrolledup, eatchar;
static char inbuf[MAX_BUFLEN];
static int inbuflen, inbufwid;
static char incharbuf[6];
static int incharbuflen;

static void addscrollline(char *s, int len)
{
	scrollback[lastscroll].str = malloc(len);
	memcpy(scrollback[lastscroll].str, s, len);
	scrollback[lastscroll].len = len;
	if (++lastscroll >= SCROLLBACKSIZE)
		lastscroll = 0;
	if (lastscroll == topscroll) {
		free(scrollback[topscroll].str);
		if (++topscroll >= SCROLLBACKSIZE)
			topscroll = 0;
	}
}

/*
 * Notes: Japanese, Chinese and Korean characters mostly take up two  characters
 * in width.  These characters return 2 via wcwidth to let the code know that
 * they require two spaces.  Mono-space fonts/ncurses seems to work correctly
 * with East Asian characters.
 *
 * There are also some characters that return a wcwidth of 0. I'm not sure
 * about all of them, but some of the 0 width characters add a mark above or
 * below a character.  In order for these marks to appear correctly, the
 * previous character and the overstrike must be drawn together.  using wmove
 * and drawing the accent doesn't work.
 *
 * There are some other characters that have functions, and these are not
 * supported using a print to the screen. South Asian fonts are complicated.  I
 * don't believe south asian fonts work correctly from the Linux command line
 * (as of late 2009). They print but don't combine.  The result is distorted.
 *
 * Many characters, as of 12/09, are too wide for a single space in the font,
 * although they return a wcwidth of 1.  I suspect this is due to these
 * characters not being part of the mono-spaced font and are instead pulled
 * from an alternate font.  These characters also glitch in xterm, vim and
 * other ncurses software. I suspect this is actually a font bug, and any
 * character that returns wcwidth=1 in a monospaced font should be monospaced.
 */

static int widthchar(char *s, size_t bytes, int xpos)
{
	wchar_t c;
	int width;
	char *outbuf=(char *) &c;
	size_t outsize = sizeof(wchar_t);

	/*
	 * Note:  Actually need to check if bad UTF8 characters show as ?
	 */
	if (iconv(utf8towchart, &s, &bytes, &outbuf, &outsize) == (size_t) -1)
		return 0;
	if (c == 9) {
		return 8 - (xpos & 7);
	}
	width = wcwidth(c);
	if (width < 0)
		return 0;
	return width;
}

static int completecharlen(char *s)
{
	unsigned ut = (unsigned char)s[0];
	int clen;
	if (ut <= 0x80)
		clen = 1;
	else if ((ut >= 192) && (ut < 192+32))
		clen = 2;
	else if ((ut >= 224) && (ut < 224+16))
		clen = 3;
	else if ((ut >= 240) && (ut < 240+8))
		clen = 4;
	else if ((ut >= 248) && (ut < 248+4))
		clen = 5;
	else if ((ut >= 252) && (ut < 252+2))
		clen = 6;
	else
		clen = 1;		/* bad  */
	return clen;
}

/*
 * Must check for COLS while redrawing from history.  Or otherwise the text
 * wraps around and does strange things.
 */
static int waddnstrcolcheck(WINDOW *win, char *s, int len, int twidth)
{
	int n;
	for (twidth = 0,n=0;n<len;) {
		int cwidth = completecharlen(&s[n]), width;
		if (n + cwidth > len)	/* Error condition  */
			return twidth;
		width = widthchar(&s[n], cwidth, twidth);
		if (twidth+width > COLS)
			return twidth;
		waddnstr(win, &s[n], cwidth);
		n += cwidth;
		twidth += width;
	}
	return twidth;
}

/*
 * Update a line on the screen from the backscroll buffer.
 */
static void updateline(int screeny, int yfrom, t_win *win_in, int mode, t_win *win_out)
{
	wmove(win_in->ptr, screeny, 0);
	if (yfrom == lastscroll) {
		int twidth = 0;
		if (inbuflen > 0)
			twidth = waddnstrcolcheck(win_in->ptr, inbuf, inbuflen, 0);
		if (mode == SLAVEMODE) {
			char obuf[MAX_BUFLEN];
			char *inbuf = (char *)win_out->string, *outbuf = obuf;
			size_t insize = win_out->bytes * sizeof(wchar_t), outsize = MAX_BUFLEN;
			iconv(wcharttoutf8, &inbuf, &insize, &outbuf, &outsize);
			waddnstrcolcheck(win_in->ptr, obuf, MAX_BUFLEN - outsize, twidth);
			win_out->curs_pos = win_out->bytes;
		}
	}
	else {
		waddnstrcolcheck(win_in->ptr, scrollback[yfrom].str, scrollback[yfrom].len, 0);
	}
}

/*
 * Cursor in SLAVE mode while scrolling looks broken.
 * Cursor in TALK mode is always good, because it's on the bottom window
 */
static void checkcursor(int mode)
{
	int newcursor;
	if ((mode == SLAVEMODE) && scrolledup)
		newcursor = 0;
	else newcursor = 1;
	if (curson != newcursor) {
		curs_set(newcursor);
		curson = newcursor;
	}
}

/* For CJK, it's important to keep the cursor always on the input
 * window.  Otherwise the display is confused */
static void restorecursor(int mode, t_win *win_out)
{
	checkcursor(mode);
	if (mode != RAWMODE) {
		int x,y;
		getyx(win_out->ptr, y, x);	/* Must restore input cursor */
		wmove(win_out->ptr,y, x);	/* location.		     */
		wrefresh(win_out->ptr);
	}
}

static void redrawscreen(t_win *win_in, int mode, t_win *win_out)
{
	int y, storedlines;
	if (lastscroll >= topscroll)
		storedlines = lastscroll - topscroll;
	else storedlines = lastscroll + SCROLLBACKSIZE - topscroll;
	/*
	 * Note it's stored lines + 1 extra line for text input.
	 */
	for (y=0;(y<=win_in->max_y) && (y <= storedlines);y++) {
		int linefrom;
		if (storedlines <= win_in->max_y) {
			/*
			 * This is a little confusing.
			 * The screen scrolls top down at start
			 */
			linefrom = topscroll + y;
		}
		else linefrom = lastscroll -scrolledup - (win_in->max_y) + y;
		while (linefrom < 0)
			linefrom += SCROLLBACKSIZE;
		while (linefrom >= SCROLLBACKSIZE)
			linefrom -= SCROLLBACKSIZE;
		updateline(y,linefrom , win_in, mode, win_out);
	}
	checkcursor(mode);
}

static void statline(int mode, char *s)
{
	static int oldlen = 0;
	int l, cnt;

	if (*s == '\0') {
		if (mode == RAWMODE)
			return;
		if (oldlen > 0) {
			move(0, STATW_STAT);
			attron(A_REVERSE);
			for (cnt = STATW_STAT; cnt < COLS; cnt++)
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
	if (COLS <= STATW_STAT)
		return;
	l = strlen(s);
	if (l > COLS - STATW_STAT)
		l = COLS-STATW_STAT;

	move(0, STATW_STAT);

	attron(A_REVERSE);
	addnstr(s, l);
	for (cnt = STATW_STAT+l;cnt < COLS;cnt++)
		addch(' ');
	attroff(A_REVERSE);
	oldlen = l;
	refresh();
}

static void scrolltext(t_win *win_in, int lines, int mode, t_win *win_out)
{
	int topline, storedlines;
	int y;
	int wasscrolledup;
	if (scrolledup + lines < 0) {
		lines = -scrolledup;
	}
	/*
	 * storedlines = Lines stored in buffer.
	 */
	if (lastscroll >= topscroll)
		storedlines = lastscroll - topscroll;
	else storedlines = lastscroll + SCROLLBACKSIZE - topscroll;
	/*
	 * The max scrolling  we can do is the # of lines stored - the
	 * screen size.
	 */
	topline = storedlines - win_in->max_y;
	if (topline < 0)
		topline = 0;
	if (scrolledup + lines > topline) {
		lines = topline - scrolledup;
	}
	if (!lines)
		return;
	wasscrolledup = scrolledup;
	scrolledup += lines;
	wscrl(win_in->ptr, -lines);
	scrollok(win_in->ptr, FALSE);
	if (lines > 0) {
		for (y=0;y<lines;y++) {
			int linefrom = lastscroll -scrolledup - (win_in->max_y) + y;
			while (linefrom < 0)
				linefrom += SCROLLBACKSIZE;
			while (linefrom >= SCROLLBACKSIZE)
				linefrom -= SCROLLBACKSIZE;
			updateline(y,linefrom , win_in, mode, win_out);
		}
	}
	else  {
		for (y=-lines-1;y>=0;y--) {
			int linefrom = lastscroll -scrolledup -  y;
			while (linefrom < 0)
				linefrom += SCROLLBACKSIZE;
			while (linefrom >= SCROLLBACKSIZE)
				linefrom -= SCROLLBACKSIZE;
			updateline(win_in->max_y  - y,linefrom , win_in, mode, win_out);
		}

	}
	scrollok(win_in->ptr, TRUE);
	checkcursor(mode);
	wrefresh(win_in->ptr);
	if (wasscrolledup && !scrolledup) {
		statline(mode, "");
	}
	else if (!wasscrolledup && scrolledup) {
		statline(mode, "Viewing Scrollback");
	}
}

static void usage(void)
{
	fprintf(stderr, "usage: call [-8] [-b l|e] [-d] [-h] [-i] [-m s|e] [-p paclen] [-r] [-R]\n");
	fprintf(stderr, "            [-s mycall] [-S] [-t] [-T timeout] [-v] [-w window] [-W]\n");
	fprintf(stderr, "            port callsign [[via] digipeaters...]\n");
	exit(1);
}

static WINDOW *win;
static const char *key_words[] = { "//",
	"#BIN#",
	" go_7+. ",
	" stop_7+. ",
	"\0"
};

static const char *rkey_words[] = {
	/*
	 * actually restricted keywords are very restrictive
	 */
	"\0"
};

static void convert_cr_lf(char *buf, int len)
{
	while (len--) {
		if (*buf == '\r')
			*buf = '\n';
		buf++;
	}
}

static void convert_lf_cr(char *buf, int len)
{
	while (len--) {
		if (*buf == '\n')
			*buf = '\r';
		buf++;
	}
}

static void convert_upper_lower(char *buf, int len)
{
	while (len--) {
		*buf = tolower(*buf);
		buf++;
	}
}

/* Return the with of this character in character blocks.  (Normal = 1, CJK=2)
 * Also for control chracters, return the width of the replacement string.
 * */
static int wcwidthcontrol(wchar_t c)
{
	int width;
	cchar_t cc = {0};
	wchar_t *str;
	cc.chars[0] = c;
	str = wunctrl(&cc);
	if (!str)
		return 0;
	width = wcswidth(str, wcslen(str));
	return width;
}

/* Return a string to print for a wchar_t.  Expand control characters.
 * Strings returned by wunctrl don't like to be freed.  It seems. */
static wchar_t *wunctrlwchar(wchar_t c)
{
	cchar_t cc = {0};
	wchar_t *str;
	cc.chars[0] = c;
	str = wunctrl(&cc);
	return str;
}

/*
 * For some reason wins_nwstr fails on fedora 12 on some characters but
 * waddnstr works.  Draw the entire input buffer when adding text.
 * The fonts that do overstrike fail when written one char at a time.
 */
static void drawinbuf(WINDOW *w, wchar_t *string, int bytes, int cur_pos)
{
	int n, x, cursorx, xpos, ypos, width;

	/*
	 * Assume cursor to be at position of current char to draw.
	 */
	getyx(w, ypos, xpos);
	x = xpos;

	cursorx = xpos;
	// cur_pos-1 = the chracter that was just added.
	for (n=cur_pos-2;n>=0;n--) {
		/*
		 * Move x position to start of string or 0
		 */
		width = wcwidthcontrol(string[n]);
		if (x >= width)
			x -= width;
		else x = 0;
	}
	wmove(w, ypos, x);
	for (n=0;n<bytes;n++) {
		char obuf[MAX_BUFLEN];
		char *inbuf, *outbuf = obuf;
		size_t insize, outsize = MAX_BUFLEN;
		int len, width;
		wchar_t *str;

		if (n == cur_pos) {
			cursorx = x;
		}
		str = wunctrlwchar(string[n]);
		if (!str)
			continue;
		inbuf = (char *) str;
		len = wcslen(str);
		insize = len * sizeof(wchar_t);
		width = wcswidth(str, len);
		iconv(wcharttoutf8, &inbuf, &insize, &outbuf, &outsize);
		waddnstr(w, obuf, MAX_BUFLEN-outsize);
		x += width;
	}
	if (cur_pos < bytes) {
		wmove(w,ypos, cursorx);
	}
}

/* Convert linear UNIX date to a MS-DOS time/date pair. */

static char * unix_to_sfbin_date_string(time_t gmt)
{

  static char buf[9];
  unsigned short s_time, s_date;

  date_unix2dos(((gmt == -1) ? time(NULL) : gmt), &s_time, &s_date);
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

	fp = fopen(PROC_NR_NODES_FILE, "r");
	if (fp == NULL) {
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
			return -1;
		}
		fd = socket(AF_ROSE, SOCK_SEQPACKET, 0);
		if (fd < 0) {
			perror("socket");
			return -1;
		}
		break;

	case AF_NETROM:
		if (address[0] == NULL) {
			fprintf(stderr,
				"call: too few arguments for NET/ROM\n");
			return -1;
		}
		fd = socket(AF_NETROM, SOCK_SEQPACKET, 0);
		if (fd < 0) {
			perror("socket");
			return -1;
		}
		ax25_aton(nr_config_get_addr(port), &sockaddr.ax25);
		sockaddr.ax25.fsa_ax25.sax25_family = AF_NETROM;
		addrlen = sizeof(struct full_sockaddr_ax25);
		break;

	case AF_AX25:
		if (address[0] == NULL) {
			fprintf(stderr,
				"call: too few arguments for AX.25\n");
			return -1;
		}
		fd = socket(AF_AX25, SOCK_SEQPACKET, 0);
		if (fd < 0) {
			perror("socket");
			return -1;
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
			return -1;
		}
		if (setsockopt
		    (fd, SOL_AX25, AX25_PACLEN, &paclen,
		     sizeof(paclen)) == -1) {
			perror("AX25_PACLEN");
			close(fd);
			fd = -1;
			return -1;
		}
		if (backoff != -1) {
			if (setsockopt
			    (fd, SOL_AX25, AX25_BACKOFF, &backoff,
			     sizeof(backoff)) == -1) {
				perror("AX25_BACKOFF");
				close(fd);
				fd = -1;
				return -1;
			}
		}
		if (ax25mode != -1) {
			if (setsockopt
			    (fd, SOL_AX25, AX25_EXTSEQ, &ax25mode,
			     sizeof(ax25mode)) == -1) {
				perror("AX25_EXTSEQ");
				close(fd);
				fd = -1;
				return -1;
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
		return -1;
	}
	if (af_mode != AF_ROSE) {	/* Let Rose autobind */
		if (bind(fd, (struct sockaddr *) &sockaddr, addrlen) == -1) {
			perror("bind");
			close(fd);
			fd = -1;
			return -1;
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
			return -1;
		}
		if (rose_aton
		    (address[1],
		     sockaddr.rose.srose_addr.rose_addr) == -1) {
			close(fd);
			fd = -1;
			return -1;
		}
		if (address[2] != NULL) {
			digi = address[2];
			if (strcasecmp(address[2], "VIA") == 0) {
				if (address[3] == NULL) {
					fprintf(stderr,
						"call: callsign must follow 'via'\n");
					close(fd);
					fd = -1;
					return -1;
				}
				digi = address[3];
			}
			if (ax25_aton_entry
			    (digi,
			     sockaddr.rose.srose_digi.ax25_call) == -1) {
				close(fd);
				fd = -1;
				return -1;
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
			return -1;
		}
		sockaddr.rose.srose_family = AF_NETROM;
		addrlen = sizeof(struct sockaddr_ax25);
		break;

	case AF_AX25:
		if (ax25_aton_arglist
		    ((const char **) address, &sockaddr.ax25) == -1) {
			close(fd);
			fd = -1;
			return -1;
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
		return -1;
	}
	if (!be_silent) {
		printf("*** Connected to %s\n", address[0]);
		fflush(stdout);
	}

	return fd;
}

static void cmd_sigwinch(int sig)
{
	signal(SIGWINCH, cmd_sigwinch);
	sigwinchsignal = TRUE;
}

static void cmd_intr(int sig)
{
	interrupted = TRUE;
}

static WINDOW *opnstatw(int mode, wint * wintab, char *s, int lines, int cols)
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

static void wrdstatw(WINDOW * win, char s[])
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

static void dupdstatw(WINDOW * win, char *s, int add)
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

static int start_ab_download(int mode, WINDOW ** swin, wint * wintab,
		      char parms[], int parmsbytes, char buf[], int bytes,
		      t_gp * gp, char *address[])
{
	int crcst;		/* startposition crc-field  */
	int datest = 0;		/* startposition date-field */
	int namest = 0;		/* startposition name-field */
	int cnt;
	int date = 0;
	struct tm ft;
	char s[GP_FILENAME_SIZE + 18];
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
		time_t t = time(NULL);
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
		snprintf(s, sizeof(s), "filename        : %s", gp->file_name);
		wrdstatw(*swin, s);
		sprintf(s, "last mod. date  : %02i.%02i.%04i", ft.tm_mday,
			ft.tm_mon+1 , ft.tm_year + 1900);
		wrdstatw(*swin, s);
		sprintf(s, "last mod. time  : %02i:%02i:%02i", ft.tm_hour,
			ft.tm_min, ft.tm_sec);
		wrdstatw(*swin, s);
	}

	dupdstatw(*swin, "Bytes to receive: ", TRUE);

	gp->dwn_file = open(gp->file_name, O_RDWR | O_CREAT, 0666);
	if (gp->dwn_file == -1) {
		snprintf(s, sizeof(s), "Unable to open %s", gp->file_name);
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

static int ab_down(int mode, WINDOW * swin, wint * wintab, char buf[],
	int *bytes, t_gp * gp)
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

static int start_screen(char *call[])
{
	int cnt;
	char idString[12];
	char *p;
	struct winsize winsz = {0};

	if ((ioctl(0, TIOCGWINSZ, &winsz) >= 0) && winsz.ws_row && winsz.ws_col)
		resizeterm(winsz.ws_row, winsz.ws_col);
	sprintf(idString, " %9.9s ", call[0]);
	for (p = idString; *p; p++)
		if (islower(*p)) *p = toupper(*p);

	win = initscr();
	if (win == NULL)
		return -1;

	attron(A_REVERSE);
	move(0, 0);
	addstr(idString);
	addch(ACS_VLINE);
	addstr("--------");
	addch(ACS_VLINE);
	move(0, STATW_STAT);
	for (cnt = STATW_STAT; cnt < COLS; cnt++)
		addch(' ');
	attroff(A_REVERSE);

	noecho();
	raw();
	nodelay(win, TRUE);
	keypad(win, TRUE);
	curson = 1;
	refresh();
	return 0;
}

static int start_slave_mode(wint * wintab, t_win * win_in, t_win * win_out)
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
	redrawscreen(win_in, SLAVEMODE, win_out);
	wrefresh(win_in->ptr);
	return 0;
}

static int start_talk_mode(wint * wintab, t_win * win_in, t_win * win_out)
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
	wclear(win_in->ptr);

	win_out->bytes = 0;
	win_out->curs_pos = 0;
	win_in->bytes = 0;
	win_out->curs_pos = 0;
	redrawscreen(win_in, TALKMODE, win_out);
	restorecursor(TALKMODE, win_out);
	wrefresh(win_in->ptr);
	wrefresh(win_out->ptr);
	return 0;
}

static int change_mode(int oldmode, int newmode, wint * wintab,
	t_win * win_in, t_win * win_out, char *call[])
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
			wintab->next = NULL;
			endwin();
		}
		if (newmode == SLAVEMODE) {
			delwin(win_out->ptr);
			delwin(win_in->ptr);
			wintab->next = NULL;
			start_slave_mode(wintab, win_in, win_out);
		}
		break;

	case SLAVEMODE:
		if (newmode == RAWMODE) {
			wclear(win_out->ptr);
			wrefresh(win_out->ptr);
			wintab->next = NULL;
			endwin();
		}
		if (newmode == TALKMODE) {
			delwin(win_out->ptr);
			wintab->next = NULL;
			start_talk_mode(wintab, win_in, win_out);
		}
		break;
	}

	scrolledup = 0;
	return newmode;
}

static void reinit_mode(int mode, wint * wintab, t_win * win_in, t_win * win_out, char *call[])
{
	switch (mode) {
	case RAWMODE:
		break;
	case TALKMODE:	/* Clear the screen and re-init. Which looks awful.  */
		wclear(win_out->ptr);
		wrefresh(win_out->ptr);
		wclear(win_in->ptr);
		/* wrefresh(win_in->ptr);  */
		wintab->next = NULL;
		endwin();
		start_screen(call);
		start_talk_mode(wintab, win_in, win_out);
		restorecursor(mode, win_out);
		break;
		case SLAVEMODE:	/* Also fix me.  */
		wclear(win_out->ptr);
		/* wrefresh(win_out->ptr);  */
		wintab->next = NULL;
		endwin();
		start_screen(call);
		start_slave_mode(wintab, win_in, win_out);
		restorecursor(mode, win_out);
		break;
	}
}

static void waddnstrcrcheck(t_win *win_in, char *buf, int bytes, int draw, int mode, t_win *win_out)
{
	int n;
	for (n=0;n<bytes;n++) {
		int width;
		incharbuf[incharbuflen++] = buf[n];
		if (completecharlen(incharbuf) > incharbuflen)
			continue;
		if (eatchar && (incharbuf[0] == '\n')) {
			eatchar = 0;
			incharbuflen = 0;
			continue;
			}
		width = widthchar(incharbuf, incharbuflen, inbufwid);
		eatchar = 0;
		if (draw) {
			if (win_in)
				waddnstr(win_in->ptr, incharbuf, incharbuflen);
			else write(STDOUT_FILENO, incharbuf, incharbuflen);
		}
		if (incharbuf[0] == '\n')
			incharbuflen = 0;
		else if (width + inbufwid <= COLS) {
			if (inbuflen + incharbuflen <= MAX_BUFLEN) {
				memcpy(&inbuf[inbuflen], incharbuf, incharbuflen);
				inbuflen += incharbuflen;
			}
			incharbuflen = 0;
			inbufwid += width;
			if (inbufwid >= COLS)
				eatchar = 1;
			continue; /* Skip to next line when width goes over. */
		}
		addscrollline(inbuf, inbuflen);
		if (incharbuflen) {
			memcpy(&inbuf[0], incharbuf, incharbuflen);
			inbuflen = incharbuflen;
			incharbuflen = 0;
			inbufwid = width;
		}
		else {
			inbuflen = 0;
			inbufwid = 0;
		}
		if (scrolledup && win_in && win_out) {
			/*
			 * scrolledup is relative to bottom line
			 */
			scrolledup++;
			scrolltext(win_in, 0, mode, win_out);
		}
	}
	if (draw && win_in)
		wrefresh(win_in->ptr);
}

static void writeincom(int mode, int encoding, t_win * win_in, unsigned char buf[], int bytes, t_win *win_out)
{
	if (mode & RAWMODE) {
		while (write(STDOUT_FILENO, buf, bytes) == -1) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				usleep(100000);
				continue;
			}
			exit(1);
		}
		return;
	} else if (encoding == UTF8ENCODING)
		waddnstrcrcheck(win_in, (char *)buf, bytes,scrolledup == 0, mode, win_out);
	else {
		char *inbuf = (char *) buf, out[MAX_BUFLEN], *outbuf=out;
		size_t insize = bytes, outsize = MAX_BUFLEN;
		iconv(ibm850toutf8, &inbuf, &insize, &outbuf, &outsize);
		waddnstrcrcheck(win_in, out, MAX_BUFLEN-outsize,scrolledup == 0, mode, win_out);
	}
	return;
}

static void writeincomstr(int mode, int encoding, t_win * win_in, char buf[], t_win *win_out)
{
	int len;
	len = strlen(buf);
	writeincom(mode, encoding, win_in, (unsigned char *)buf, len, win_out);
}

static int outstring(char *buf, wchar_t *string, int bytes, int encoding)
{
	char *inbuf = (char *) string, *outbuf = buf;
	size_t insize = bytes * sizeof(wchar_t), outsize = MAX_BUFLEN-1;
	if (encoding == UTF8ENCODING) {
		iconv(wcharttoutf8, &inbuf, &insize, &outbuf, &outsize);
	}
	else {
		iconv(wcharttoibm850, &inbuf, &insize, &outbuf, &outsize);

	}
	buf[(MAX_BUFLEN-1)-outsize] = '\0';
	return (MAX_BUFLEN-1)-outsize;
}

static int getstring(wint * wintab, char text[], char buf[])
{
	wchar_t c;
	int ypos = 0, xpos = 0;
	int bytes = 0;
	wchar_t wbuf[MAX_BUFLEN];
	WINDOW *win = winopen(wintab, 3, COLS, 10, 0, TRUE);
	int done = 0;

	wmove(win, 1, 2);
	waddstr(win, text);
	wrefresh(win);

	do {
		int r;
		wint_t ci;
		r = get_wch(&ci);
		if (r != ERR) {
			c = (wchar_t) ci;
			if  (((r == KEY_CODE_YES) && (c == KEY_BACKSPACE))||
				    ((r == OK) && ((c==127)|| (c==8)))) {
				getyx(win, ypos, xpos);
				if (bytes > 0) {
					int width, j;
					width = wcwidthcontrol(wbuf[bytes-1]);
					for (j=0;j<width;j++)
						mvwdelch(win, ypos, xpos-width);
					xpos -= width;
					wmove(win, ypos, xpos);
					bytes--;
				}
			}
			else if (( (r==KEY_CODE_YES) && (c == KEY_ENTER))||
				   ( (r == OK) && ((c=='\n') || (c=='\r')))) {
				wbuf[bytes++] = (wchar_t) '\n';
				outstring(buf, wbuf, bytes, UTF8ENCODING);
				done = 1;
			}
			else if (r == KEY_CODE_YES);
				/*
				 * Put in code for other KEYCODES here
				 */
			else if (bytes+2 < MAX_BUFLEN) {
#if 0
				int width;
				width = wins_nwchrmy(win, c);
				getyx(win, ypos, xpos);
				wmove(win, ypos, xpos+width);
#endif
				wbuf[bytes++] = c;
				drawinbuf(win, wbuf, bytes, bytes);
			}
			wrefresh(win);
		}
	} while (!done);
	delwin(win);
	winclose(wintab);

	return 0;
}

static int readoutg(t_win * win_out, wint * wintab, menuitem * top, char buf[],
		int keyesc, int mode, int encoding, t_win *win_in)
{
	int out_cnt;
	int r;
	wint_t ci;
	wchar_t c;
	int ypos = 0, xpos = 0;
	int value;

	r = get_wch(&ci);
	if (r == ERR)
		return 0;

	c = (wchar_t) ci;
	if (c == keyesc) {
		value = top_menu(wintab, top, 1);
		if (value == 0) {
			restorecursor(mode, win_out);
			return 0;
		}
		restorecursor(mode, win_out);
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
				restorecursor(mode, win_out);
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
				restorecursor(mode, win_out);
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
	if  (((r == KEY_CODE_YES) && (c == KEY_BACKSPACE))||
			((r == OK) && ((c==127)|| (c==8)))) {
		if ((mode == SLAVEMODE) && scrolledup)
			return 0;
		while(win_out->curs_pos > 0) {
			int width;
			int j;
			getyx(win_out->ptr, ypos, xpos);
			width = wcwidthcontrol(win_out->string[win_out->curs_pos-1]);
			for (j=0;j<width;j++)
				mvwdelch(win_out->ptr, ypos, xpos-width);
			xpos -= width;
			wmove(win_out->ptr, ypos, xpos);
			if (win_out->curs_pos < win_out->bytes) {
				memmove(&win_out->
					string[win_out->curs_pos - 1],
					&win_out->string[win_out-> curs_pos],
					(win_out->bytes -
					win_out->curs_pos) * sizeof(wchar_t));
				}
			win_out->bytes--;
			win_out->curs_pos--;
			if (width)
				break;
		}
	}
	else if (( (r==KEY_CODE_YES) && (c == KEY_ENTER))||
		   ( (r == OK) && ((c=='\n') || (c=='\r')))) {
		if ((mode == SLAVEMODE) && scrolledup)
			return 0;
		while (win_out->curs_pos < win_out->bytes) {
			/* Move to end of the line  */
			int width;
			width = wcwidthcontrol(win_out->string[win_out->curs_pos]);
			win_out->curs_pos++;
			getyx(win_out->ptr, ypos, xpos);
			wmove(win_out->ptr, ypos, xpos + width);
			}
		waddch(win_out->ptr, '\n');
		win_out->string[win_out->bytes++] = (wchar_t) '\n';
		wrefresh(win_out->ptr);
		out_cnt = outstring(buf, win_out->string, win_out->bytes, encoding);
		if (mode == SLAVEMODE) {
			char obuf[MAX_BUFLEN];
			char *inbuf = (char *)win_out->string, *outbuf = obuf;
			size_t insize = win_out->bytes * sizeof(wchar_t), outsize = MAX_BUFLEN;
			iconv(wcharttoutf8, &inbuf, &insize, &outbuf, &outsize);
			waddnstrcrcheck(win_in, obuf, MAX_BUFLEN-outsize, 0, mode, win_out);
		}
		win_out->bytes = 0;
		win_out->curs_pos = 0;
		return out_cnt;
	}
	else if (r == KEY_CODE_YES) {
		switch(c) {
			case KEY_LEFT:	/* Character of 0 width  */
				while (win_out->curs_pos > 0) {
					int width;
					win_out->curs_pos--;
					width = wcwidthcontrol(win_out->string[win_out->curs_pos]);
					getyx(win_out->ptr, ypos, xpos);
					wmove(win_out->ptr, ypos, xpos - width);
					if (width)
						break; /* Skip to non-width */
				}
				break;
			case KEY_RIGHT:
				{
				/* Skip over 0 length characters  */
				int skipped = 0;
				while (win_out->curs_pos < win_out->bytes) {
					int width;
					width = wcwidthcontrol(win_out->string[win_out->curs_pos]);
					if (width) {
						if (skipped)
							break;
						skipped = 1;
					}
					win_out->curs_pos++;
					getyx(win_out->ptr, ypos, xpos);
					wmove(win_out->ptr, ypos, xpos + width);
				}
				break;
			}
			case KEY_UP:
				scrolltext(win_in, 1, mode, win_out);
				break;
			case KEY_DOWN:
				scrolltext(win_in, -1, mode, win_out);
				break;
			case KEY_NPAGE:
				scrolltext(win_in, -win_in->max_y, mode, win_out);
				break;
			case KEY_PPAGE:
				scrolltext(win_in, win_in->max_y, mode, win_out);
				break;
			case KEY_HOME:
				while (win_out->curs_pos > 0) {
					int width;
					win_out->curs_pos--;
					width = wcwidthcontrol(win_out->string[win_out->curs_pos]);
					getyx(win_out->ptr, ypos, xpos);
					wmove(win_out->ptr, ypos, xpos - width);
				}
				break;
			case KEY_END:
				while (win_out->curs_pos < win_out->bytes) {
					/*
					 * Move to end of the line
					 */
					int width;
					width = wcwidthcontrol(win_out->string[win_out->curs_pos]);
					win_out->curs_pos++;
					getyx(win_out->ptr, ypos, xpos);
					wmove(win_out->ptr, ypos, xpos + width);
					}
				break;
			case KEY_DC:{
				int skipped = 0;
				if ((mode == SLAVEMODE) && scrolledup)
					return 0;
				while (win_out->curs_pos < win_out->bytes) {
					int width;
					int j;
					getyx(win_out->ptr, ypos, xpos);
					width = wcwidthcontrol(win_out->string[win_out->curs_pos]);
					if (width) {
						if (skipped)
							break;
						skipped = 1;
					}
					for (j=0;j<width;j++)
						mvwdelch(win_out->ptr, ypos, xpos);
					if (win_out->curs_pos + 1 < win_out->bytes) {
						memmove(&win_out-> string[win_out->curs_pos],
							&win_out->string[win_out-> curs_pos+1],
							(win_out->bytes - (win_out->curs_pos+1)) * sizeof(wchar_t));
					}
					win_out->bytes--;
				}
				break;
			}
			case KEY_RESIZE:
				break;
			case KEY_BACKSPACE:
				break;
			case KEY_ENTER:
				break;
			default:
				break;
		}
	}
	else switch (c) {
	case 8:
	case 127:
	case '\r':
	case '\n':
		break;
	default:
		/*
		 * Don't try to edit while scrolled up in SLAVEmode.
		 */
		if ((mode == SLAVEMODE) && scrolledup)
			return 0;
		/*
		 * It's just not possible because the cursor is off screen
		 */
		if (win_out->bytes < MAX_BUFLEN )	{
			if (win_out->curs_pos < win_out->bytes) {
				memmove(&win_out->
					string[win_out->curs_pos + 1],
					&win_out->string[win_out-> curs_pos],
					(win_out->bytes -
					win_out->curs_pos) * sizeof(wchar_t));
			}
			win_out->string[win_out->curs_pos] =  c;
			win_out->bytes++;
			win_out->curs_pos++;
			drawinbuf(win_out->ptr, win_out->string, win_out->bytes, win_out->curs_pos);
			break;
		}
	}
	wrefresh(win_out->ptr);
	return 0;
}

#if 0
static void remotecommand(char buf[], int bytes)
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
#endif

static int compstr(const char st1[], char st2[], int maxbytes)
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

static int eol(char c)
{

	if (c == '\r' || c == '\n' || c == 0x1A)
		return TRUE;
	else
		return FALSE;
}

static int searche_key_words(char buf[], int *bytes, char *parms, int *parmsbytes,
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
				t = compstr(pkey_words[command], &buf[cnt],
					    *bytes - cnt);
				if (t != -1)
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

static int sevenplname(int mode, WINDOW ** swin, wint * wintab, int *f,
		int *logfile, char parms[], int parmsbytes)
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

static void statbits(int mode, char stat, int m)
{
	if (mode == RAWMODE)
		return;
	move(0, STATW_BITS + m);
	attron(A_REVERSE);
	addch(stat);
	attroff(A_REVERSE);
	refresh();
}

static int cmd_call(char *call[], int mode, int encoding)
{
	menuitem con[] = {
		{"~Reconnect", 'R', M_ITEM, (void *) 0x01},
		{"~Exit", 'E', M_ITEM, (void *) 0x02},
		{"\0", 0, M_END, NULL}
	};

	menuitem fil[] = {
		{"~Open Logfile", 'O', M_ITEM, NULL},
		{"~Close Logfile", 'C', M_ITEM, NULL},
		{"Send ~Textfile", 'T', M_ITEM, NULL},
		{"Send ~Binary", 'B', M_ITEM, NULL},
		{"Send ~AutoBin", 'A', M_ITEM, NULL},
		{"\0", 0, M_END, NULL}
	};

	menuitem mod[] = {
		{"~Slavemode", 'S', M_ITEM, NULL},
		{"~Talkmode", 'T', M_ITEM, NULL},
		{"~Rawmode", 'R', M_ITEM, NULL},
		{"\0", 0, M_END, NULL}
	};

	menuitem win[] = {
		{"~Clear", 'C', M_ITEM, NULL},
		{"~Resize", 'R', M_ITEM, NULL},
		{"\0", 0, M_END, NULL}
	};

	menuitem top[] = {
		{"~Connect", 'C', M_P_DWN, con},
		{"~File", 'F', M_P_DWN, fil},
		{"~Mode", 'M', M_P_DWN, mod},
		{"~Window", 'W', M_P_DWN, win},
		{"\0", 0, M_END, NULL}
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
	t_win win_in = {
		.ptr	= NULL,
	};
	t_win win_out = {
		.ptr	= NULL,
	};
	WINDOW *swin = NULL;
	int cnt;
	int crc = 0;
	char s[80];
	int flags = 0;
	int EOF_on_STDIN = FALSE;
	sigset_t oursigs, oldsigs;

	init_crc();

	if (paclen > sizeof(buf))
		paclen = sizeof(buf);

	gp.dwn_cnt = 0;
	wintab.next = NULL;

	fd = connect_to(call);
	if (fd == -1)
		return FALSE;

	interrupted = FALSE;
	sigwinchsignal = FALSE;
	signal(SIGQUIT, cmd_intr);
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGWINCH, cmd_sigwinch);
	sigemptyset(&oursigs);
	sigaddset(&oursigs, SIGQUIT);

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

	sigprocmask(SIG_BLOCK, &oursigs, &oldsigs);

	while (TRUE) {
		struct timespec tv, *timeout;
		int nfds;
		if (inactivity_timeout_is_set == TRUE && uploadfile == -1 && downloadfile == -1) {
			tv.tv_sec = inactivity_timeout.tv_sec;
			tv.tv_nsec = inactivity_timeout.tv_nsec;
		} else {
			tv.tv_sec = 0;
			tv.tv_nsec = 10000;		/* 10ms		*/
		}
		FD_ZERO(&sock_read);
		if (EOF_on_STDIN == FALSE)
			FD_SET(STDIN_FILENO, &sock_read);
		FD_SET(fd, &sock_read);
		FD_ZERO(&sock_write);

		if (uploadfile != -1)
			FD_SET(fd, &sock_write);

		if (uploadfile == -1 && downloadfile == -1 &&
		    inactivity_timeout_is_set == FALSE)
			timeout = NULL;
		else
			timeout = &tv;
		nfds = pselect(fd + 1, &sock_read, &sock_write, NULL, timeout,
			       &oldsigs);
		if (nfds == -1) {
			if (!interrupted) {
				if (errno == EINTR)
					continue;
				if (errno == EAGAIN) {
					usleep(100000);
					continue;
				}
				if ((errno == EINTR) && sigwinchsignal) {
					/*
					 * Just process screen resize here.
					 */
					reinit_mode(mode, &wintab, &win_in, &win_out, call);
					sigwinchsignal = 0;
					continue;
				}
				perror("select");
			}
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

						writeincom(mode, encoding, &win_in,
							   (unsigned char * ) buf, bytes, &win_out);
						restorecursor(mode, &win_out);
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
#if 0
					/*
					 * FIXME! We should, no: WE MUST be
					 * able to turn off all remote commands
					 * to avoid mail bombs generating
					 * offensive mails with //e while
					 *sucking the BBS
					 */
					{
						remotecommand(parms,
							      parmsbytes);
					}
#endif
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
						sevenplcnt = sevenplname(mode,
									 &swin,
									 &wintab,
									 &downloadfile,
									 &logfile,
									 parms,
									 parmsbytes);
						if (sevenplcnt != -1)
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
			} while (restbytes != 0);
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
					     0x1d, mode, encoding, &win_in);
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
						t = strchr(c, '\n');
						if (t != NULL)
							*t = '\0';
					} else {
						c = getenv("SHELL");
						if (c == NULL)
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
					writeincomstr(mode, encoding, &win_in,"\nTilde escapes:\n", &win_out);
					writeincomstr(mode, encoding, &win_in,".   close\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"~   send ~\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"r   reconnect\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"!   shell\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"Z   suspend program. Resume with \"fg\"\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"s   Stop upload\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"o   Open log\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"c   Close log\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"0   Switch GUI to \"RAW\" (line) mode - already here ;)\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"1   Switch GUI to \"Slave\" mode\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"2   Switch GUI to \"Talk\" (split) mode\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"u   Upload\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"a   Upload (autobin protocol)\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"b   Upload binary data (crlf conversion)\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"yd  YAPP Download\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"yu  YAPP Upload\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"i   IBM850 encoding\n", &win_out);
					writeincomstr(mode, encoding, &win_in,"8   UTF-8 encoding\n", &win_out);
					restorecursor(mode, &win_out);
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
					restorecursor(mode, &win_out);
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
						restorecursor(mode, &win_out);
						continue;
					}
					t = strchr(buf, '\n');
					if (t != NULL)
						*t = '\0';
					t = buf + 2;
					while (*t != '\0' && isspace(*t))
						t++;
					if (*t == '\0') {
						statline(mode,
								"Upload requires a filename");
						restorecursor(mode, &win_out);
						continue;
					}
					uploadfile = open(t, O_RDONLY);
					if (uploadfile == -1) {
						statline(mode,
								"Unable to open upload file");
						restorecursor(mode, &win_out);
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
								file_time = time(NULL);

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
						restorecursor(mode, &win_out);
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
					t = strchr(buf, '\n');
					if (t != NULL)
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
					restorecursor(mode, &win_out);
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
					restorecursor(mode, &win_out);
					continue;
				case 'Y':
				case 'y':
					cmd_yapp(buf + 2, bytes - 2);
					restorecursor(mode, &win_out);
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
				case '8':
					encoding = UTF8ENCODING;
					statline(mode, "UTF-8 encoding");
					restorecursor(mode, &win_out);
					continue;
				case 'i':
				case 'I':
					encoding = ORIGINALENCODING;
					statline(mode, "IBM850 encoding");
					restorecursor(mode, &win_out);
					continue;
				default:
					statline(mode,
						 "Unknown '~' escape. Type ~h for a list");
					restorecursor(mode, &win_out);
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
					restorecursor(mode, &win_out);
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
				restorecursor(mode, &win_out);
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
					restorecursor(mode, &win_out);
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
					restorecursor(mode, &win_out);
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
					restorecursor(mode, &win_out);
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
/*			} */

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

	sigprocmask(SIG_SETMASK, &oldsigs, NULL);
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

static void iconvclose(void)
{
	iconv_close(ibm850toutf8);
	iconv_close(wcharttoibm850);
	iconv_close(wcharttoutf8);
	iconv_close(utf8towchart);
}

int main(int argc, char **argv)
{
	int p;
	int mode = TALKMODE;
	int encoding = UTF8ENCODING;	/* Maybe controversial?  */

	setlocale(LC_ALL, "");
	if (!isatty(STDIN_FILENO))
		stdin_is_tty = 0;

	setlinebuf(stdin);

	while ((p = getopt(argc, argv, "b:dhm:p:rs:RStT:vw:Wi8")) != -1) {
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
			paclen = atoi(optarg);
			if (paclen == 0) {
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
			/*
			 * This is used to format the scrollback buffer,
			 * which is stored in raw
			 */
			COLS = 80;
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
		case 'T': {
				double f = atof(optarg);

				inactivity_timeout.tv_sec = f;
				inactivity_timeout.tv_nsec = (f - (long) f) * 1000000000;

				if (f < 0.001 || f > (double) LONG_MAX) {
					fprintf(stderr, "call: option '-T' must be > 0.001 (1ms) and < %ld seconds\n", LONG_MAX);
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
			window = atoi(optarg);
			if (window == 0) {
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
		case 'i':
			encoding = ORIGINALENCODING;
			break;
		case '8':
			encoding = UTF8ENCODING;
			break;
		case '?':
		case ':':
			usage();
		}
	}

	if (optind == argc || optind == argc - 1) {
		usage();
	}

	ibm850toutf8 = iconv_open("UTF8", "IBM850");
	wcharttoibm850 = iconv_open("IBM850", "WCHAR_T");
	wcharttoutf8 = iconv_open("UTF8", "WCHAR_T");
	utf8towchart = iconv_open("WCHAR_T", "UTF8");
	atexit(iconvclose);

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
		printf("GW4PTS AX.25 Connect %s\n", VERSION);
		fflush(stdout);
	}
	while (cmd_call(argv + optind + 1, mode, encoding)) {
		if (!be_silent) {
			printf("Wait 60 sec before reconnect\n");
			fflush(stdout);
		}
		sleep(60);
	}

	return 0;
}
