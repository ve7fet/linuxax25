/*
 * This is axgetput
 *
 * This shell utility is for up/downloading files via your ax25 unix login
 * session which is managed by axspawn.
 * you need a //BIN compatible axspawn, which has the functionality of making
 * the stream 8bit compatible (normally the EOL-conversion <LF> <-> <CR>
 * prevents this). see:
 *   http://x-berg.in-berlin.de/cgi-bin/viewcvs.cgi/ampr/patches/linux-ax25/
 *
 * (c) 2002 Thomas Osterried  DL9SAU <thomas@x-berg.in-berlin.de>
 * License: GPL. See http://www.fsf.org/
 * Sources: http://x-berg.in-berlin.de/cgi-bin/viewcvs.cgi/ampr/axgetput/
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "includes.h"

#include "axgetput.h"
#include "util.h"
#include "proto_bin.h"

int fdin = 0;
int fdout = 1;
int fderr = 2;
int fdout_is_pipe = 0;
int fdin_is_pipe = 0;

int is_stream = 0;
int mode = 0;
int do_crc_only = 0;

char c_eol = '\n';

unsigned int BLOCKSIZ = BLOCKSIZ_DEFAULT;

char *send_on_signal = 0;

static struct termios prev_termios;
static int prev_termios_stored = 0;
static mode_t mode_tty = 0;

#ifndef	MYNAME
#define MYNAME "axgetput"
#endif

/*---------------------------------------------------------------------------*/

static void set_tty_flags(void)
{
  struct termios termios;
  struct stat statbuf;

  if (fdin_is_pipe)
    return;
  /* mesg no  */
  if (!fstat(fdin, &statbuf)) {
    /* save old mode  */
    mode_tty = statbuf.st_mode;
    fchmod(fdin, 0600);
  }
  /* make tty 8bit clean  */
  if (tcgetattr(fdin, &prev_termios) != -1)
    prev_termios_stored = 1;
  memset((char *) &termios, 0, sizeof(termios));
  termios.c_iflag = IGNBRK | IGNPAR;
  termios.c_oflag = 0;
  termios.c_cflag = CBAUD | CS8 | CREAD | CLOCAL;
  termios.c_cflag = ~(CSTOPB|PARENB|PARODD|HUPCL);
  termios.c_lflag = 0;
  termios.c_cc[VMIN] = 1;
  termios.c_cc[VTIME] = 0;
  termios.c_cc[VSTART] = -1;
  termios.c_cc[VSTOP] = -1;
  tcsetattr(fdin, TCSANOW, &termios);
}

/*---------------------------------------------------------------------------*/

static void restore_tty_flags(void)
{
  if (fdin_is_pipe)
    return;
  if (prev_termios_stored)
    tcsetattr(fdin, TCSANOW, &prev_termios);
  if (mode_tty)
    fchmod(fdin, mode_tty);
}

/*---------------------------------------------------------------------------*/

static void eol_convention(int state_should)
{
  /* need patched axspawn */
#define	BIN_ON	"//BIN ON\r"
#define	BIN_OFF	"//BIN OFF\r"
  static int state_is = 0;

  /* already in correct state?  */
  if ((state_is && state_should) || (!state_is && !state_should))
	return;

  sleep(1);

  if (state_should) {
    write(fderr, BIN_ON, strlen(BIN_ON));
    c_eol = '\r';
  } else {
    write(fderr, BIN_OFF, strlen(BIN_OFF));
    c_eol = '\n';
  }
  state_is = state_should;

  sleep(1);
}

/*---------------------------------------------------------------------------*/

static void restore_defaults(void)
{
  eol_convention(0);
  restore_tty_flags();
}

/*---------------------------------------------------------------------------*/

static void signal_handler(int sig)
{
  if (send_on_signal)
    secure_write(fdout, send_on_signal, strlen(send_on_signal));
  restore_defaults();
  if (*err_msg) {
    fputs(err_msg, stderr);
  }
  fprintf(stderr, "Died by signal %d.\n", sig);
  exit(sig);
}

/*---------------------------------------------------------------------------*/

void do_version(void)
{
        fprintf(stderr, MYNAME " " VERSION "\n");
	fprintf(stderr, "  (c) 2002 Thomas Osterried <thomas@x-berg.in-berlin.de>\n");
	fprintf(stderr, "  License: GPL. See http://www.fsf.org/\n");
	fprintf(stderr, "  Sources: http://x-berg.in-berlin.de/cgi-bin/viewcvs.cgi/ampr/axgetput/\n");
}

/*---------------------------------------------------------------------------*/

static void usage(int all) {
  fprintf(stderr, "usage: %s ", myname);
  if (mode % 2) {
    fprintf(stderr, "[-ivh] [filename]\n");
  } else {
    fprintf(stderr, "[-isvh] [-b <blocksize>] [filename]\n");
  }
  fprintf(stderr, "  -h prints detailed help\n");
  fprintf(stderr, "  -i computes checksum only\n");
  fprintf(stderr, "  -v prints version and exits\n");
  if (!all)
    return;
  if (mode % 2) {
    fprintf(stderr, "  filename is usually got from the remote side by the protocol\n");
    fprintf(stderr, "  but can be forced if you like to ignore this.\n");
    fprintf(stderr, "  filename should be ommitted if output is sent to a pipe\n.");
  } else {
    fprintf(stderr, "  -b value is the blocksize (framelen) of the transmitted data\n");
    fprintf(stderr, "     default %d, which is a useful choice for ampr ax25.\n", BLOCKSIZ_DEFAULT);
    fprintf(stderr, "  -s indicates a stream with unknown size.\n");
    fprintf(stderr, "     otherwise, the data will be read to memory until EOF.\n");
    fprintf(stderr, "  -s is only available if stdin is a pipe\n");
    fprintf(stderr, "  if filename specified in filter, the given name will be suggested instead.\n");
    fprintf(stderr, "  filename may be ommited if used as filter.\n");
  }
  fputc('\n', stderr);
  fprintf(stderr, "Tips: - compressed download:\n");
  fprintf(stderr, "        gzip -c foo.txt | bget foo.txt.gz\n");
  fprintf(stderr, "        tar cjf - ~/foo ~/bar/ | bget my_data.tar.bz2\n");
  fputc('\n', stderr);
  fprintf(stderr, "Other protocols:\n");
  fprintf(stderr, "  bget / bput: receive / send with #BIN# protocol\n");
  fprintf(stderr, "  yget / yput: receive / send with yapp\n");
  fprintf(stderr, "  rget / rput: receive / send with didadit\n");
  fprintf(stderr, "These are (sym)links to one program: " MYNAME "\n");
}


/*---------------------------------------------------------------------------*/

/* not implemented */
static int yput(void) { strcpy(err_msg, "yapp: not implementet yet\n"); return 1; }
static int yget(void) { strcpy(err_msg, "yapp: not implementet yet\n"); return 1; }
static int rget(void) { strcpy(err_msg, "yapp: not implementet yet\n"); return 1; }
static int rput(void) { strcpy(err_msg, "yapp: not implementet yet\n"); return 1; }

/*---------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{

  int len;
  int ret = 0;
  int c;
  char *p;

  /* determine what to so in the way how we are called  */
  if ((p = strrchr(argv[0], '/')))
    p++; /* skip '/'  */
  else
    p = argv[0];
  len = strlen(p);
  if (len < 0 || len > sizeof(myname)-1)
    len = sizeof(myname)-1;
  strncpy(myname, p, len);
  myname[len] = 0;
  strlwc(myname);

  fdin_is_pipe = (isatty(fdin) ? 0 : 1);
  fdout_is_pipe = (isatty(fdout) ? 0 : 1);

  if (fdin_is_pipe && fdout_is_pipe) { 
    fprintf(stderr, "error: cannot work in between two pipes\n");
    exit(1);
  }

  *filename = 0;
  *err_msg = 0;

  if (!strcmp(myname, "bput"))
    mode = RECV_BIN;
  else if (!strcmp(myname, "bget"))
    mode = SEND_BIN;
  else if (!strcmp(myname, "yput"))
    mode = RECV_YAPP;
  else if (!strcmp(myname, "yget"))
    mode = SEND_YAPP;
  else if (!strcmp(myname, "rput"))
    mode = RECV_DIDADIT;
  else if (!strcmp(myname, "rget"))
    mode = SEND_DIDADIT;

  if (mode % 2) {
    if (fdin_is_pipe) {
      fprintf(stderr, "error: error: stdin must be a tty\n");
      exit(1);
    }
  } else {
    if (fdout_is_pipe) {
      fprintf(stderr, "error: stdout must be a tty\n");
      exit(1);
    }
  }

  signal(SIGHUP, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

/* for difference betreen "bput -f file" and "bget file"  */
#define	get_filename(f) { \
      if (!strcmp(f, "-")) { \
	if (mode % 2) \
          fdin_is_pipe = 1; \
        else \
          fdout_is_pipe = 1; \
      } else { \
        strncpy(filename, f, sizeof(filename)-1); \
        filename[sizeof(filename)-1] = 0; \
	if (mode % 2) { \
	  if (fdin_is_pipe) \
	    fdin_is_pipe = 0; \
	} else { \
          if (fdout_is_pipe) \
	    fdout_is_pipe = 0; \
	} \
      } \
}

  while ((c = getopt(argc, argv, (mode % 2) ? "ivh?" : "b:isvh?")) != EOF) {
    switch(c) {
    case 'b':
      if (((BLOCKSIZ = (unsigned ) atoi(optarg)) < BLOCKSIZ_MIN) || BLOCKSIZ > BLOCKSIZ_MAX) {
	fprintf(stderr, "error: invalid blocksize: %d\n", BLOCKSIZ);
	fprintf(stderr, "       blocksize must be in the range %d <= x <= %d. a value\n", BLOCKSIZ_MIN, BLOCKSIZ_MAX);
	fprintf(stderr, "       of %d (default) is suggested, because it fits in an ax25 frame.\n", BLOCKSIZ_DEFAULT);
	exit(1);
      }
      break;
    case 'i':
      do_crc_only = 1;
      break;
    case 's':
      is_stream = 1;
      break;
    case 'v':
      do_version();
      exit(0);
      break;
    case 'h':
    case '?':
      usage((c == 'h'));
      exit(0);
      break;
    }
  }

  if (mode == 0) {
    usage(1);
    exit(0);
  }

  if (optind < argc) {
    get_filename(argv[optind]);
    optind++;
  }
  if (optind < argc) {
    usage(0);
    exit(1);
  }

  if (is_stream && !fdin_is_pipe) {
    fprintf(stderr, "error: -s is only available in a pipe\n");
    exit(1);
  }

  if (do_crc_only)
    goto skiped_crc_only_tag_1;

  if (mode % 2) {
    if (fdin_is_pipe) {
      fprintf(stderr, "error: error: stdin must be a tty.\n");
      exit(1);
    }
    if (fdout_is_pipe && *filename) {
      fprintf(stderr, "error: filename in a pipe does not make sense.\n");
      exit(1);
    }
  } else {
    if (fdout_is_pipe) {
      fprintf(stderr, "error: stdout must be a tty.\n");
      exit(1);
    }
    if (!fdin_is_pipe && !*filename) {
      fprintf(stderr, "error: no file to send.\n");
      exit(1);
    }
  }

  signal(SIGQUIT, signal_handler);

  set_tty_flags();
  eol_convention(1);

skiped_crc_only_tag_1:

  switch (mode) {
  case RECV_BIN:
    if (do_crc_only)
      ret = bget();
    else
      ret = bput();
    break;
  case SEND_BIN:
    ret = bget();
    break;
  case RECV_YAPP:
    ret = yput();
    break;
  case SEND_YAPP:
    ret = yget();
    break;
  case RECV_DIDADIT:
    ret = rput();
    break;
  case SEND_DIDADIT:
    ret = rget();
    break;
  }

  restore_defaults();
  if (*err_msg) {
    fputs(err_msg, stderr);
  }
  exit(ret);

  return 0;

}

