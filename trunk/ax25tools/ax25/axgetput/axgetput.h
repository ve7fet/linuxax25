/*
 * (c) 2002 Thomas Osterried  DL9SAU <thomas@x-berg.in-berlin.de>
 *   License: GPL. See http://www.fsf.org/
 *   Sources: http://x-berg.in-berlin.de/cgi-bin/viewcvs.cgi/ampr/axgetput/
 */

#ifndef	AXGETPUT_H
#define AXGETPUT_H

extern int fdin;
extern int fdout;
extern int fderr;

extern int fdin_is_pipe;
extern int fdout_is_pipe;

char myname[PATH_MAX+1];
char filename[PATH_MAX+1];
char err_msg[2048];

extern int is_stream;
extern int mode;
extern int do_crc_only;

extern char c_eol;
extern char *send_on_signal;

/* modes */
#define RECV_BIN        1	/*   #BIN# protocol: receive */
#define SEND_BIN        2	/*   #BIN# protocol: send */
#define RECV_YAPP       3	/*    yapp protocol: receive */
#define SEND_YAPP       4	/*    yapp protocol: send */
#define RECV_DIDADIT    5	/* didadit protocol: receive */
#define SEND_DIDADIT    6	/* didadit protocol: send */


/* block sizes */

extern unsigned int BLOCKSIZ;

#define BLOCKSIZ_MIN	   1	/* not suggested */
#define BLOCKSIZ_DEFAULT 256    /* useful, because it fits in an ax25 frame */
#define BLOCKSIZ_MAX	1024	/* max. our buffer relies on it */

#endif /* AXGETPUT_H */
