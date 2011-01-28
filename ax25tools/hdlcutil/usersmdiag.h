/*****************************************************************************/

/*
 *      usersmdiag.h  --  Diagnostics interface.
 *
 *      Copyright (C) 1996  Thomas Sailer (t.sailer@alumni.ethz.ch)
 *        Swiss Federal Institute of Technology (ETH), Electronics Lab
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  This is the Linux realtime sound output driver
 */

/*****************************************************************************/
      
#ifndef _USERSMDIAG_H
#define _USERSMDIAG_H

/* --------------------------------------------------------------------- */

#define USERSM_KEY_PROJ                '0'

/* --------------------------------------------------------------------- */

#define USERSM_CMD_REQ_CHACCESS_PAR    0x00020
#define USERSM_CMD_ACK_CHACCESS_PAR    0x10021
#define USERSM_CMD_SET_CHACCESS_PAR    0x00022

#define USERSM_CMD_CALIBRATE           0x00030
#define USERSM_CMD_REQ_CHANNELSTATE    0x00031
#define USERSM_CMD_ACK_CHANNELSTATE    0x10032

#define USERSM_CMD_REQ_DIAG            0x00040
#define USERSM_CMD_ACK_DIAG            0x10041

#define USERSM_CMD_REQ_DRVNAME         0x00050
#define USERSM_CMD_ACK_DRVNAME         0x10051
#define USERSM_CMD_REQ_DRVMODE         0x00052
#define USERSM_CMD_ACK_DRVMODE         0x10053

/*
 * diagnose modes; same as in <linux/soundmodem.h>
 */
#define USERSM_DIAGMODE_OFF            0
#define USERSM_DIAGMODE_INPUT          1
#define USERSM_DIAGMODE_DEMOD          2
#define USERSM_DIAGMODE_CONSTELLATION  3

/*
 * diagnose flags; same as in <linux/soundmodem.h>
 */
#define USERSM_DIAGFLAG_DCDGATE    (1<<0)
#define USERSM_DIAGFLAG_VALID      (1<<1)

/* --------------------------------------------------------------------- */

#define DIAGDATALEN 64

/* --------------------------------------------------------------------- */

struct usersmmsg {
	struct usersm_hdr {
		long type;
		unsigned int channel;
	} hdr;
	union {
		struct usersm_chaccess_par {
			int tx_delay;  /* the transmitter keyup delay in 10ms units */
			int tx_tail;   /* the transmitter keyoff delay in 10ms units */
			int slottime;  /* the slottime in 10ms; usually 10 = 100ms */
			int ppersist;  /* the p-persistence 0..255 */
			int fulldup;   /* some driver do not support full duplex, setting */
			/* this just makes them send even if DCD is on */
		} cp;
		
		int calib;

		struct usersm_channel_state {
			int ptt;
			int dcd;
			int ptt_keyed;
			unsigned long tx_packets;
			unsigned long tx_errors;
			unsigned long rx_packets;
			unsigned long rx_errors;
		} cs;
		
		struct usersm_diag {
			unsigned int mode;
			unsigned int flags;
			unsigned int samplesperbit;
			unsigned int datalen;
		} diag;

		struct usersm_diag_out {
			struct usersm_diag diag;
			short samples[DIAGDATALEN];
		} diag_out;

		unsigned char by[0];
	} data;
};

/* --------------------------------------------------------------------- */
#endif /* _USERSMDIAG_H */
