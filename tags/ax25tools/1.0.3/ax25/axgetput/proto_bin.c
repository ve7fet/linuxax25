/*
 * (c) 2002 Thomas Osterried  DL9SAU <thomas@x-berg.in-berlin.de>
 *   License: GPL. See http://www.fsf.org/
 *   Sources: http://x-berg.in-berlin.de/cgi-bin/viewcvs.cgi/ampr/axgetput/
 */

#include "includes.h"

#include "proto_bin.h"
#include "axgetput.h"
#include "util.h"

static int crctab[256];
static int bittab[8] = { 128,64,32,16,8,4,2,1 };
static int crcbit[8] = {
  0x9188,0x48c4,0x2462,0x1231,0x8108,0x4084,0x2042,0x1021
};

/*---------------------------------------------------------------------------*/

static int init_crc(void)
{
  int i,j;
  
  for (i = 0; i < 256; i++) {
    crctab[i] = 0;
    for (j = 0; j < 8; j++) {
      if ((bittab[j] & i) != 0) {
        crctab[i] = crctab[i] ^ crcbit[j];
      }
    }
  }
  return 0;
}

/*---------------------------------------------------------------------------*/

static int do_crc(char b, int n, unsigned int crc)
{
  crc = (crctab[(crc >> 8)] ^ ((crc << 8) | (b & 0xff))) & 0xffff;
  return (crc);
}

/*---------------------------------------------------------------------------*/

static long parse_sfbin_date_to_unix(const char *s)
{

  unsigned long x;

  sscanf(s, "%lX", &x);

  return date_dos2unix(((x << 16) >> 16), (x >> 16));
}

/*---------------------------------------------------------------------------*/

static char * unix_to_sfbin_date_string(long gmt)
{

  static char buf[9];
  unsigned short s_time, s_date;

  date_unix2dos(((gmt == -1) ? time(0) : gmt), &s_time, &s_date);
  sprintf(buf, "%X", ((s_date << 16) + s_time));
  return buf;
}

/*---------------------------------------------------------------------------*/

int bput(void)
{
  struct stat statbuf;
  char buf[1024];  /* < signed int */
  char filename_given[PATH_MAX];
  unsigned long len_read_expected = 0L;
  unsigned long len_read_left;
  time_t file_time = 0L;
  unsigned int msg_crc = 0;
  unsigned int crc = 0;
  char *term_line = 0;
  int last_line_had_CR = 0;
  int len_termline = 0;
  int len = 0;
  int fddata = fdout;
  int is_eof;
  int i;
  char *p;
  char *p_buf;

#define save_close(x) { \
      if (!fdout_is_pipe) \
	close(x); \
}


  for (;;) {
    len = my_read(fdin, buf, sizeof(buf), &is_eof, "\r\n");
    if (is_eof || len < 1) {
      sprintf(err_msg, "error: read failed (%s)\n", strerror(errno));
      return 1;
    }
    if (buf[len-1] == '\n') {
#if 0
      p = "warning: <LF> received. not 8bit clean?\r";
      secure_write(fdout, p, strlen(p));
#endif
      sprintf(err_msg, "bad EOL: <LF>\n");
      return 1;
    }
    if (IS_BIN_ABORT(buf, len)) {
      sprintf(err_msg, "Aborted by user request\n");
      return 1;
    }
    if (buf[len-1] == '\r' && len > 5 && !memcmp(buf, "#BIN#", 5)) {
        break;
    }
    if (len == sizeof(buf)) {
      sprintf(err_msg, "line to long\n");
      return 1;
    }
  }
  buf[len-1] = 0; /* without trailing \r. and: string termination */

  send_on_signal = bin_send_no_on_sig;

  /* parse #BIN arguments */
  *filename_given = 0;

  p_buf = buf;
  for (i = 0; (p = strsep(&p_buf, "#")); i++) {
    switch(i) {
    case 0:
    case 1:
      break;
    case 2:
      if (*p)
	len_read_expected = (unsigned long ) atol(p);
      break;
    default:
      if (*p == '|') {
        msg_crc = (unsigned int ) atoi(p+1);
      } else if (*p == '$') {
        file_time = parse_sfbin_date_to_unix(p+1);
      } else {
        strncpy(filename_given, p, sizeof(filename_given)-1);
        filename_given[sizeof(filename_given)-1] = 0;
      }
    }
  }

  if (!fdout_is_pipe) {
    /* normal mode: store in given filename  */
    if (!*filename) {
      p = get_fixed_filename(filename_given, len_read_expected, msg_crc, 1);
      strncpy(filename, p, sizeof(filename)-1);
      filename[sizeof(filename)-1] = 0;
    }
    if (!stat(filename, &statbuf)) {
      /* file exist  */
      if (unlink(filename)) {
        sprintf(err_msg, "error: cannot unlink %s (%s)\n", filename, strerror(errno));
        goto abort;
      }
    }
    if ((fddata = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0640)) < 0) {
      sprintf(err_msg, "error: cannot open %s (%s)\n", filename, strerror(errno));
      write(fderr, "\r#NO#\r", 6);
      return 1;
    }
  }

  if (!len_read_expected) {
    term_line = "***END\r";
    len_termline = strlen(term_line);
  }

  /* say helo  */
  send_on_signal = bin_send_abort_on_sig;
  write(fderr, "\r#OK#\r", 6);

  len_read_left = len_read_expected;

  /* #bin# chechsum initialization  */
  init_crc();

  for (;;) {

    if ((len = my_read(fdin, buf, ((term_line || len_read_left > sizeof(buf)) ? sizeof(buf) : len_read_left), &is_eof, "\r")) < 1) {
      save_close(fddata);
      sprintf(err_msg, "error: read failed (%s)\n", strerror(errno));
      goto abort;
    }
    if (!len) {
      save_close(fddata); 
      if (!term_line) {
        sprintf(err_msg, "error: unexpected end of file during read: %s\n", strerror(errno));
	return 1;
      }
      return 0;
    }

    if (msg_crc) {
      int i;
      for (i = 0; i < len; i++)
        crc = do_crc((int ) buf[i], 1, crc);
    }

    if (buf[len-1] == '\r') {
      if (last_line_had_CR) {
	if (IS_BIN_ABORT(buf, len)) {
	  /* "\r#ABORT#\r" was sent  */
          if (!fdout_is_pipe) {
	    close(fddata);
	    /* clean up  */
	    unlink(filename);
	  }
	  return 1;
	}
	if (term_line && len == len_termline && !memcmp(buf, term_line, len_termline)) {
          /* sucessfully read until termination string  */
          break;
	}
      }
      last_line_had_CR = 1;
    } else {
      last_line_had_CR = 0;
    }
    if (!term_line)
      len_read_left -= len;

    if (secure_write(fddata, buf, len) == -1) {
      save_close(fddata);
      sprintf(err_msg, "error: write failed (%s)\n", strerror(errno));
      goto abort;
    }

    /* nothing left?  */
    if (!term_line && len_read_left == 0L)
      break;
    if (is_eof) {
      if (!term_line && len_read_left) {
	save_close(fddata);
        goto abort;
      }
      break;
    }
  }
  if (crc != msg_crc) {
    sprintf(err_msg, "Invalid crc: computed %d, expected %d.\n", crc, msg_crc);
    /* don't unlink  */
    save_close(fddata);
    return 1;
  }
  if (!fdout_is_pipe) {
    close(fddata);
    if (file_time != 0L) {
      struct utimbuf utb;
      utb.modtime = file_time;
      utb.actime = time(0);
      utime(filename, &utb);
    }
  }

  send_on_signal = 0;
  return 0;

abort:
    sleep(1);
    write(fderr, "\r#ABORT#\r", 9);
    return 1;
#undef	save_close
}

/*---------------------------------------------------------------------------*/

int bget(void) {
  struct strlist {
    struct strlist *next;
    size_t len;
    char data[1]; /* actually a the address of char * pointer */
  };

  struct strlist *stored_file = 0;
  struct strlist  *sl_tail = 0;
  struct strlist *sl;
  struct timeval timeout;
  struct stat statbuf;

  unsigned int crc = 0;

  char buf[1024];
  fd_set readfds;
  int fddata = fdin;
  int len;
  unsigned long file_size = 0;
  unsigned long len_remains;
  int is_eof;
  time_t file_time = 0L;

#define save_close(x) { \
      if (!fdin_is_pipe) \
	close(x); \
}

#define	store_line(s, len) { \
        if (!(sl = (struct strlist *) malloc(sizeof(struct strlist *) + sizeof(size_t) + len))) \
	  return 1; \
	sl->next = 0; \
	sl->len = len; \
	memcpy(sl->data, s, len); \
	if (!stored_file) { \
          stored_file = sl; \
	} else { \
	  sl_tail->next = sl; \
	} \
	sl_tail = sl; \
}

  if (BLOCKSIZ < 1 || BLOCKSIZ > sizeof(buf))
    BLOCKSIZ = BLOCKSIZ_DEFAULT;

  init_crc();

  if (!fdin_is_pipe && *filename) {
    if ((fddata = open(filename, O_RDONLY)) == -1) {
      sprintf(err_msg, "error: cannot open %s (%s)\n", filename, strerror(errno));
      return 1;
    }
    if (!fstat(fddata, &statbuf))
      file_time =  statbuf.st_mtime;
    else
      file_time = time(0);
    
    /* compute crc  */
    while ((len = read(fddata, buf, BLOCKSIZ)) > 0) {
      int i;
      for (i = 0; i < len; i++)
        crc = do_crc((int ) buf[i], 1, crc);
      file_size += len;
    }
    if (len < 0) {
      sprintf(err_msg, "error: read failed (%s)\n", strerror(errno));
      close(fddata);
      return 1;
    }
    /* rewind  */
    if (lseek(fddata, 0L, SEEK_SET) != 0L) {
      sprintf(err_msg, "error: file io failed on lseek() (%s)\n", strerror(errno));
      close(fddata);
      return 1;
    }
    sprintf(buf, "\r#BIN#%ld#|%d#$%s#%s\r", file_size, crc, unix_to_sfbin_date_string(file_time), get_fixed_filename(filename, file_size, crc, 1));
  } else {
    file_time = time(0);
    if (!is_stream || do_crc_only) {
      sprintf(err_msg, "error: not enough memory\n");
      while ((len = read(fddata, buf, sizeof(buf))) > 0) {
        int i;
        for (i = 0; i < len; i++)
          crc = do_crc((int ) buf[i], 1, crc);
        file_size += len;
	if (!do_crc_only)
	  store_line(buf, len);
      }
      if (len < 0) {
        sprintf(err_msg, "error: read failed (%s)\n", strerror(errno));
        close(fddata);
        return 1;
      }
      *err_msg = 0;
      sprintf(buf, "\r#BIN#%ld#|%d#$%s#%s\r", file_size, crc, unix_to_sfbin_date_string(file_time), get_fixed_filename(filename, file_size, crc, 1));
    } else {
      sprintf(buf, "\r#BIN###$%s#%s\r", unix_to_sfbin_date_string(file_time), get_fixed_filename(filename, 0, 0, 1));
    }
    /*
     * hack: check for #ABORT# from fdout (fd 1), because fddata (fd 0) is
     * our pipe we read the data from, which we actually tx.
     * believe me, it does work.
     */
    fdin = fdout;
  }

  if (do_crc_only) {
    printf("File information for %s:\n", get_fixed_filename(filename, file_size, crc, 1));
    printf("  size %ld bytes, crc %d, date %s (%ld)\n", file_size, crc, unix_to_sfbin_date_string(file_time), file_time);
    return 0;
  }

  send_on_signal = bin_send_abort_on_sig;
  if (secure_write(fdout, buf, strlen(buf)) == -1) {
    sprintf(err_msg, "error: write failed (%s)\n", strerror(errno));
    save_close(fddata);
    return 1;
  }

    /* wait for answer  */
  for (;;) {
                 /*
		  * make sure we do not read from a pipe. fdout is also
		  * assigned to the tty
		  */
    len = my_read(fdout, buf, sizeof(buf), &is_eof, "\r\n");
    if (is_eof || len < 1) {
      sprintf(err_msg, "error: read failed (%s)\n", strerror(errno));
      save_close(fddata);
      return 1;
    }
    if (buf[len-1] == '\n') {
#if 0
      char *p = "warning: <LF> received. not 8bit clean?\r";
      secure_write(fdout, p, strlen(p));
#endif
      sprintf(err_msg, "bad EOL: <LF>\n");
      goto abort;
    } else if (buf[len-1] != '\r') {
      sprintf(err_msg, "line to long\n");
      continue;
    }
    if (IS_BIN_OK(buf, len))
      break;
    if (IS_BIN_NO(buf, len)) {
      save_close(fddata);
      return 0;
    }
    if (IS_BIN_ABORT(buf, len)) {
      sprintf(err_msg, "Aborted by user request\n");
      save_close(fddata);
      return 1;
    }
  }

  len_remains = file_size;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  for (;;) {
    char *p_buf;

    /* check for user \r#ABORT#\r on tty stream  */
    FD_ZERO(&readfds);
    FD_SET(fdin, &readfds);
    if (select(fdin+1, &readfds, 0, 0, &timeout) && FD_ISSET(fdin, &readfds)) {
      if ((len = read(fdin, buf, sizeof(buf))) < 0) {
        sprintf(err_msg, "read from tty failed (%s)\n", strerror(errno));
          save_close(fddata);
          goto abort;
      }
      if (IS_BIN_ABORT(buf, len)) {
        sprintf(err_msg, "Aborted by user request\n");
        save_close(fddata);
        return 1;
      }
    }
    /* read data  */
    if (!fdin_is_pipe || is_stream) {
      p_buf = buf;
      if ((len = my_read(fddata, buf, ((len_remains > BLOCKSIZ || is_stream) ? BLOCKSIZ : len_remains), &is_eof, 0)) < 1) {
        save_close(fddata);
        if (len < 0) {
          sprintf(err_msg, "error: read failed (%s)\n", strerror(errno));
	  goto abort;
        }
        break;
      }
      len_remains -= len;
    } else {
      p_buf = stored_file->data;
      len = stored_file->len;
    }
    /* write to client  */
    if (secure_write(fdout, p_buf, len) == -1) {
      sprintf(err_msg, "error: write failed (%s)\n", strerror(errno));
      save_close(fddata);
      goto abort;
    }
    if (fdin_is_pipe && !is_stream) {
      sl = stored_file;
      if (!(stored_file = stored_file->next))
	is_eof = 1;
      free(sl);
    }
    if (!fdin_is_pipe && !len_remains) {
      if (read(fddata, buf, 1) == 1) {
	sprintf(err_msg, "Warning: file has grown in the meantime\n");
      }
      is_eof = 1;
      break;
    }
    /*
     * need this because my_read may returned lenth != 0 (data to be written)
     * but also has detected EOF.
     */
    if (is_eof)
      break;
  }

  sleep(10);

  return 0;

abort:
    sleep(1);
    write(fderr, "\r#ABORT#\r", 9);
    return 1;
#undef	save_close
}
