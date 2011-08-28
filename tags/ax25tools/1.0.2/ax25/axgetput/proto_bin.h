/* @(#) $Id: proto_bin.h,v 1.1 2006/12/10 19:12:59 dl9sau Exp $ */

/*
 * (c) 2002 Thomas Osterried  DL9SAU <thomas@x-berg.in-berlin.de>
 *   License: GPL. See http://www.fsf.org/
 *   Sources: http://x-berg.in-berlin.de/cgi-bin/viewcvs.cgi/ampr/axgetput/
 */

#ifndef	PROTO_BIN_H
#define	PROTO_BIN_H

extern int bget(void);
extern int bput(void);

static const char bin_s_on_ok[] = "#OK#\r";
static const int  bin_len_ok = sizeof(bin_s_on_ok)-1;
static const char bin_s_on_no[] = "#NO#\r";
static const int  bin_len_no = sizeof(bin_s_on_no)-1;
static const char bin_s_on_abort[] = "#ABORT#\r";
static const int  bin_len_abort = sizeof(bin_s_on_abort)-1;

#define bin_send_no_on_sig "\r#NO#\r"
#define bin_send_abort_on_sig "\r#ABORT#\r"

#define	IS_BIN_NO(s, len) \
  len == bin_len_no && !memcmp(s, bin_s_on_no, bin_len_no)

#define	IS_BIN_OK(s, len) \
  len == bin_len_ok && !memcmp(s, bin_s_on_ok, bin_len_ok)

#define	IS_BIN_ABORT(s, len) \
  len == bin_len_abort && !memcmp(s, bin_s_on_abort, bin_len_abort)

#endif /* PROTO_BIN_H */
