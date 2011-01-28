/* AX25IO - Library for io manuipulation for AX.25 programs
 * Copyright (C) 1998 Tomi Manninen
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef _AX25IO_H
#define _AX25IO_H

#define AXBUFLEN	4096

typedef struct ax25io_s {
	int ifd;		/* stdin socket index			*/
	int ofd;		/* stdout socket index			*/
	char eol[4];		/* end-of-line sequence                 */
	int eolmode;		/* end-of-line translation on/off       */
	int telnetmode;		/* telnet option negotiation on/off     */
	int tn_echo;		/* will/wont echo                       */
	int tn_linemode;	/* will/wont linemode                   */
	int size;		/* size of the packet in input buffer   */
	int paclen;		/* paclen                               */
	unsigned char ibuf[AXBUFLEN];	/* input buffer			*/
	unsigned char obuf[AXBUFLEN];	/* output buffer		*/
	unsigned char gbuf[AXBUFLEN];	/* getline buffer		*/
	int gbuf_usage;		/* getline buffer usage			*/
	int iptr;		/* input pointer                        */
	int optr;		/* output pointer                       */
	void *zptr;		/* pointer to the compression struct	*/

        struct ax25io_s *next;	/* linked list pointer			*/
} ax25io;

#define EOLMODE_TEXT	0
#define EOLMODE_BINARY	1
#define EOLMODE_GW	2

#define AX25_EOL	"\r"
#define NETROM_EOL	AX25_EOL
#define	ROSE_EOL	AX25_EOL
#define INET_EOL	"\r\n"
#define UNSPEC_EOL	"\n"
#define INTERNAL_EOL	021271

#define ZERR_STREAM_END		1024
#define ZERR_STREAM_ERROR	1025
#define ZERR_DATA_ERROR		1026
#define ZERR_MEM_ERROR		1027
#define ZERR_BUF_ERROR		1028
#define ZERR_UNKNOWN		1029

extern ax25io *axio_init(int, int, int, char *);
extern void axio_end(ax25io *);
extern void axio_end_all(void);

extern int axio_compr(ax25io *, int);
extern int axio_paclen(ax25io *, int);
extern int axio_eolmode(ax25io *, int);
extern int axio_cmpeol(ax25io *, ax25io *);
extern int axio_tnmode(ax25io *, int);
extern int axio_flush(ax25io *);

extern int axio_getc(ax25io *);
extern int axio_putc(int, ax25io *);

extern char *axio_getline(ax25io *);
extern int axio_gets(char *, int, ax25io *);
extern int axio_puts(const char *, ax25io *);

extern int axio_printf(ax25io *, const char *, ...);

extern int axio_tn_do_linemode(ax25io *);
extern int axio_tn_will_echo(ax25io *);
extern int axio_tn_wont_echo(ax25io *);

#endif		/* _AX25IO_H */
