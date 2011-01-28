/* @(#) $Id: util.h,v 1.1 2006/12/10 19:12:59 dl9sau Exp $ */

/*
 * (c) 2002 Thomas Osterried  DL9SAU <thomas@x-berg.in-berlin.de>
 *   License: GPL. See http://www.fsf.org/
 *   Sources: http://x-berg.in-berlin.de/cgi-bin/viewcvs.cgi/ampr/axgetput/
 */

#ifndef UTIL_H
#define	UTIL_H

extern char *strlwc(char *s);
extern char *strtrim(char *s);
extern int my_read(int fd, char *s, int len_max, int *eof, char *p_break);
extern int secure_write(int fd, char *s, int len_write_left);
extern char *get_fixed_filename(char *line, long size, unsigned int msg_crc, int generate_filename);
extern long date_dos2unix(unsigned short time,unsigned short date);
extern void date_unix2dos(int unix_date,unsigned short *time, unsigned short *date);

#endif /* UTIL_H */
