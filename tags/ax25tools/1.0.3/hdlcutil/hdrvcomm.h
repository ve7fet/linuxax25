/*****************************************************************************/

/*
 *	hdrvcomm.h  -- HDLC driver communications.
 *
 *	Copyright (C) 1996  Thomas Sailer (t.sailer@alumni.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

/*****************************************************************************/

#ifndef _HDRVCOMM_H
#define _HDRVCOMM_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */

#define HDRVC_KERNEL 1

/* ---------------------------------------------------------------------- */

#ifdef HDRVC_KERNEL
#include <linux/hdlcdrv.h>
#include "soundmodem.h"
#include <linux/baycom.h>
#endif /* HDRVC_KERNEL */

/* ---------------------------------------------------------------------- */

struct hdrvc_channel_params {
        int tx_delay;  /* the transmitter keyup delay in 10ms units */
        int tx_tail;   /* the transmitter keyoff delay in 10ms units */
        int slottime;  /* the slottime in 10ms; usually 10 = 100ms */
        int ppersist;  /* the p-persistence 0..255 */
        int fulldup;   /* some driver do not support full duplex, setting */
                       /* this just makes them send even if DCD is on */
};

struct hdrvc_channel_state {
        int ptt;
        int dcd;
        int ptt_keyed;
        unsigned long tx_packets;
        unsigned long tx_errors;
        unsigned long rx_packets;
        unsigned long rx_errors;
};

/* ---------------------------------------------------------------------- */

/*
 * diagnose modes
 */
#define HDRVC_DIAGMODE_OFF            0
#define HDRVC_DIAGMODE_INPUT          1
#define HDRVC_DIAGMODE_DEMOD          2
#define HDRVC_DIAGMODE_CONSTELLATION  3

/*
 * diagnose flags
 */
#define HDRVC_DIAGFLAG_DCDGATE    (1<<0)


/* ---------------------------------------------------------------------- */

extern int hdrvc_recvpacket(char *pkt, int maxlen);
extern int hdrvc_getfd(void);
extern const char *hdrvc_ifname(void);
extern void hdrvc_args(int *argc, char *argv[], const char *def_if);
extern void hdrvc_init(void);
extern int hdrvc_get_samples(void);
extern int hdrvc_get_bits(void);
extern int hdrvc_get_channel_access_param(struct hdrvc_channel_params *par);
extern int hdrvc_set_channel_access_param(struct hdrvc_channel_params par);
extern int hdrvc_calibrate(int calib);
extern int hdrvc_get_channel_state(struct hdrvc_channel_state *st);
extern unsigned int hdrvc_get_ifflags(void);
extern int hdrvc_diag2(unsigned int mode, unsigned int flags, short *data, unsigned int maxdatalen,
		       unsigned int *samplesperbit);
extern int hdrvc_get_driver_name(char *buf, int bufsz);
extern int hdrvc_get_mode_name(char *buf, int bufsz);

#ifdef HDRVC_KERNEL
extern int hdrvc_hdlcdrv_ioctl(int cmd, struct hdlcdrv_ioctl *par);
extern int hdrvc_sm_ioctl(int cmd, struct sm_ioctl *par);
extern int hdrvc_baycom_ioctl(int cmd, struct baycom_ioctl *par);
extern int hdrvc_diag(struct sm_diag_data *diag);
#endif /* HDRVC_KERNEL */

/* ---------------------------------------------------------------------- */
#ifdef __cplusplus
	   }
#endif
#endif /* _HDRVCOMM_H */
