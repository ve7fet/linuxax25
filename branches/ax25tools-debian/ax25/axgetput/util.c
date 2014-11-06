/*
 * (c) 2002 Thomas Osterried  DL9SAU <thomas@x-berg.in-berlin.de>
 *   License: GPL. See http://www.fsf.org/
 *   Sources: http://x-berg.in-berlin.de/cgi-bin/viewcvs.cgi/ampr/axgetput/
 */

#include "includes.h"

#include "axgetput.h"

/* Use private function because some platforms are broken, eg 386BSD */

static int Xtolower(int c)
{
  return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
}

/*---------------------------------------------------------------------------*/

char *strlwc(char *s)
{
  char *p;

  for (p = s; (*p = Xtolower(*p)); p++) ;
  return s;
}

/*---------------------------------------------------------------------------*/

char *strtrim(char *s)
{
  char *p;

  for (p = s; *p; p++) ;
  while (--p >= s && isspace(*p & 0xff)) ;
  p[1] = 0;
  return s;
}

/*---------------------------------------------------------------------------*/

int my_read(int fd, char *s, int len_max, int *eof, char *p_break)
{

  struct timeval timeout;
  fd_set errfds;
  int len_got;

  *eof = 0;

  for (len_got = 0; len_got < len_max; ) {
    int len;
    char *s_curr = s + len_got;

    if ((len = read(fd, s_curr, (p_break ? 1 : len_max))) < 1) {
      if (len == -1 && errno == EAGAIN) {
        sleep(10);
        continue;
      }
      *eof = 1;
      /*
       * len = 0: normal eof. if we're looking for a string, return -1 since
       * we have'nt found
       */
      return (len == 0 && p_break ? -1 : (len_got ? len_got : len));
    }
    len_got += len;

    /* read once? or char in p_break found?  */
    if (!p_break || strchr(p_break, *s_curr))
      break;
  }
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  FD_ZERO(&errfds);
  FD_SET(fdout, &errfds);
  if (select(fd+1, 0, 0, &errfds, &timeout) && FD_ISSET(fd, &errfds))
    *eof = 1;

  return len_got;
}
    
/*---------------------------------------------------------------------------*/

int secure_write(int fd, char *s, int len_write) {
  int len_write_left_curr = len_write;

  while (len_write_left_curr) {
    int len;
    if ((len = write(fd, s, len_write_left_curr)) < 0) {
      if (len == -1 && errno == EAGAIN) {
        sleep(10);
        continue;
      }
      return -1;
    }
    if (len != len_write_left_curr) {
      /* queue busy..  */
      sleep(10);
    }
    s += len;
    len_write_left_curr -= len;
  }
  return len_write;
}

/*---------------------------------------------------------------------------*/

char *get_fixed_filename(char *line, long size, unsigned int msg_crc, int generate_filename) {

  static char filename[1024];
  char *p, *q, *r;

  *filename = 0;

  if ((p = strchr(line, '\"'))) {
    p++;
  }

  /* security.. */

  if ((q = strrchr((p ? p : line), '/'))) {
    p = ++q;
  }
  if ((q = strrchr((p ? p : line), '\\'))) {
    p = ++q;
  }
  if ((q = strrchr((p ? p : line), ':'))) {
    p = ++q;
  }
  if (!p) {
    p = line;
  }
  while (*p && isspace(*p & 0xff))
    p++;
  for (r = filename, q = p; *q; q++) {
    if (*q == '"' || *q == ';') {
      break;
    }
    *r++ = *q;
  }
  *r = 0;
  strtrim(filename);
  if ((q = strrchr(filename, '.'))) {
    if (!*(q+1)) {
      /* remove trailing dots */
      *q = 0;
    } else {
      /* make suffix lowercase */
      strlwc(q);
    }
  }
  if (!*filename && generate_filename) {
    sprintf(filename, "unknown-%ld%d%ld.bin", size, msg_crc, time(0));
  }
  return filename;
}

/*---------------------------------------------------------------------------*/

/* Convert a MS-DOS time/date pair to a UNIX date (seconds since 1 1 70). */

/* Linear day numbers of the respective 1sts in non-leap years. */
static int day_n[] = { 0,31,59,90,120,151,181,212,243,273,304,334,0,0,0,0 };
                  /* JanFebMarApr May Jun Jul Aug Sep Oct Nov Dec */

/*---------------------------------------------------------------------------*/

long date_dos2unix(unsigned short time,unsigned short date)
{
        int month,year;
        long secs;

        month = ((date >> 5) & 15)-1;
        year = date >> 9;
        secs = (time & 31)*2+60*((time >> 5) & 63)+(time >> 11)*3600+86400*
            ((date & 31)-1+day_n[month]+(year/4)+year*365-((year & 3) == 0 &&
            month < 2 ? 1 : 0)+3653);
                        /* days since 1.1.70 plus 80's leap day */
        return secs;
}

/*---------------------------------------------------------------------------*/

/* Convert linear UNIX date to a MS-DOS time/date pair. */

void date_unix2dos(int unix_date,unsigned short *time, unsigned short *date)
{
        int day,year,nl_day,month;

        *time = (unix_date % 60)/2+(((unix_date/60) % 60) << 5)+
            (((unix_date/3600) % 24) << 11);
        day = unix_date/86400-3652;
        year = day/365;
        if ((year+3)/4+365*year > day) year--;
        day -= (year+3)/4+365*year;
        if (day == 59 && !(year & 3)) {
                nl_day = day;
                month = 2;
        }
        else {
                nl_day = (year & 3) || day <= 59 ? day : day-1;
                for (month = 0; month < 12; month++)
                        if (day_n[month] > nl_day) break;
        }
        *date = nl_day-day_n[month-1]+1+(month << 5)+(year << 9);
}

