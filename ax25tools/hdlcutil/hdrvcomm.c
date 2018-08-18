/*****************************************************************************/

/*
 *	hdrvcomm.c  -- HDLC driver communications.
 *
 *	Copyright (C) 1996-1998  Thomas Sailer (t.sailer@alumni.ethz.ch)
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
 *
 * History:
 *   0.1  10.05.97  Started
 *   0.2  14.04.98  Tried to implement AF_PACKET
 */

/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <endian.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <net/if.h>
#include "hdrvcomm.h"
#include "usersmdiag.h"
#include <net/if_arp.h>

#include <net/ethernet.h>

#include <linux/if_packet.h>
#ifndef SOL_PACKET
#define SOL_PACKET  263
#endif

/* ---------------------------------------------------------------------- */

static const char *if_name = "bcsf0";
static char *prg_name;
static int fd = -1;
static struct ifreq ifr_h;
static int promisc;
static int afpacket = 1;
static int msqid = -1;

/* ---------------------------------------------------------------------- */

static void terminate(void)
{
	if (ioctl(fd, SIOCSIFFLAGS, &ifr_h) < 0) {
		perror("ioctl (SIOCSIFFLAGS)");
		exit(1);
	}
	exit(0);
}

/* ---------------------------------------------------------------------- */

static void terminate_sig(int signal)
{
	printf("signal %i caught\n", signal);
	terminate();
}

/* ---------------------------------------------------------------------- */

int hdrvc_recvpacket(char *pkt, int maxlen)
{
	struct ifreq ifr_new;
	struct sockaddr_ll from;
	socklen_t from_len = sizeof(from);

	if (!promisc) {
		if (afpacket) {
			struct sockaddr_ll sll;
			struct packet_mreq mr;

			memset(&sll, 0, sizeof(sll));
			sll.sll_family = AF_PACKET;
			sll.sll_ifindex = ifr_h.ifr_ifindex;
			sll.sll_protocol = htons(ETH_P_AX25);
			if (bind(fd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
				fprintf(stderr,
					"%s: Error %s (%i) bind failed\n",
					prg_name, strerror(errno), errno);
				exit(-2);
			}
			memset(&mr, 0, sizeof(mr));
			mr.mr_ifindex = sll.sll_ifindex;
			mr.mr_type = PACKET_MR_PROMISC;
			if (setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, (char *)&mr, sizeof(mr)) < 0) {
				fprintf(stderr,
					"%s: Error %s (%i) setsockopt SOL_PACKET, PACKET_ADD_MEMBERSHIP failed\n",
					prg_name, strerror(errno), errno);
				exit(-2);
			}
		} else {
			struct sockaddr sa;

			strcpy(sa.sa_data, if_name);
			sa.sa_family = AF_INET;
			if (bind(fd, &sa, sizeof(sa)) < 0) {
				fprintf(stderr,
					"%s: Error %s (%i) bind failed\n",
					prg_name, strerror(errno), errno);
				exit(-2);
			}
			ifr_new = ifr_h;
			ifr_new.ifr_flags |= IFF_PROMISC;
			if (ioctl(fd, SIOCSIFFLAGS, &ifr_new) < 0) {
				perror("ioctl (SIOCSIFFLAGS)");
				exit(1);
			}
			signal(SIGTERM, terminate_sig);
			signal(SIGQUIT, terminate_sig);
			if (atexit((void (*)(void))terminate)) {
				perror("atexit");
				terminate();
			}
		}
		promisc = 1;
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
	}
	if (!pkt || maxlen < 2)
		return 0;
	return recvfrom(fd, pkt, maxlen, 0, (struct sockaddr *)&from, &from_len);
	return -1;
}

/* ---------------------------------------------------------------------- */

int hdrvc_getfd(void)
{
	return fd;
}

/* ---------------------------------------------------------------------- */

const char *hdrvc_ifname(void)
{
	return if_name;
}

/* ---------------------------------------------------------------------- */

void hdrvc_args(int *argc, char *argv[], const char *def_if)
{
	int ac, i;

	if (def_if)
		if_name = def_if;
	if (!argc || !argv || (ac = *argc) < 1)
		return;
	prg_name = argv[0];
	for (i = 1; i < ac-1; i++) {
		if (!strcmp(argv[i], "-i")) {
			if_name = argv[i+1];
			ac -= 2;
			if (i < ac)
				memmove(argv+i, argv+i+2, (ac-i) * sizeof(void *));
			i--;
		}
	}
	*argc = ac;
}

/* ---------------------------------------------------------------------- */

void hdrvc_init(void)
{
	key_t k;
	static struct ifreq ifr;

	strcpy(ifr_h.ifr_name, if_name);
		/* first try to use AF_PACKET */
	if ((fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_AX25))) < 0
	    || ioctl(fd, SIOCGIFINDEX, &ifr_h) < 0) {
		if (fd >= 0)
			close(fd);
		afpacket = 0;
		fd = socket(PF_INET, SOCK_PACKET, htons(ETH_P_AX25));
		if (fd < 0) {
			fprintf(stderr, "%s: Error %s (%i), cannot open %s\n",
				prg_name,
				strerror(errno), errno, if_name);
			exit(-1);
		}
		if (ioctl(fd, SIOCGIFFLAGS, &ifr_h) < 0) {
			fprintf(stderr, "%s: Error %s (%i), cannot ioctl SIOCGIFFLAGS %s\n",
				prg_name,
				strerror(errno), errno, if_name);
			exit(-1);
		}
	}
	strcpy(ifr.ifr_name, if_name);
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0 ) {
		fprintf(stderr, "%s: Error %s (%i), cannot ioctl SIOCGIFHWADDR %s\n",
			prg_name,
			strerror(errno), errno, if_name);
		exit(-1);
	}
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_AX25) {
		fprintf(stderr, "%s: Error, interface %s not AX25 (%i)\n",
			prg_name,
			if_name, ifr.ifr_hwaddr.sa_family);
		exit(-1);
	}
	return;
	k = ftok(if_name, USERSM_KEY_PROJ);
	if (k == (key_t)-1) {
		fprintf(stderr, "%s: Error %s (%i), cannot ftok on %s\n", prg_name,
			strerror(errno), errno, if_name);
		exit(-1);
	}
	msqid = msgget(k, 0700);
	if (msqid < 0) {
		fprintf(stderr, "%s: Error %s (%i), cannot msgget %d\n", prg_name,
			strerror(errno), errno, k);
		exit(-1);
	}
}

/* ---------------------------------------------------------------------- */

static void hdrvc_sendmsg(struct usersmmsg *msg, int len)
{
	if (msgsnd(msqid, (struct msgbuf *)msg, len+sizeof(msg->hdr)-sizeof(long), 0) < 0) {
		perror("msgsnd");
		exit(1);
	}
}

static int hdrvc_recvmsg(struct usersmmsg *msg, int maxlen, long type)
{
	int len;

	len = msgrcv(msqid, (struct msgbuf *)msg, maxlen - sizeof(long), type,
		     MSG_NOERROR);
	if (len < 0) {
		perror("msgrcv");
		exit(1);
	}
#if 0
	for (;;) {
		if ((len = msgrcv(msqid, (struct msgbuf *)msg, maxlen-sizeof(long), type, IPC_NOWAIT|MSG_NOERROR)) >= 0)
			return len+sizeof(long);
		if (errno != ENOMSG) {
			perror("msgrcv");
			exit(1);
		}
		usleep(250000);
	}
#endif
	return len+sizeof(long);
}

/* ---------------------------------------------------------------------- */


int hdrvc_hdlcdrv_ioctl(int cmd, struct hdlcdrv_ioctl *par)
{
	struct ifreq ifr = ifr_h;

	errno = EINVAL;
	return -1;
	ifr.ifr_data = (caddr_t)par;
	par->cmd = cmd;
	return ioctl(fd, SIOCDEVPRIVATE, &ifr);
}

/* ---------------------------------------------------------------------- */

int hdrvc_sm_ioctl(int cmd, struct sm_ioctl *par)
{
	struct ifreq ifr = ifr_h;

	errno = EINVAL;
	return -1;
	ifr.ifr_data = (caddr_t)par;
	par->cmd = cmd;
	return ioctl(fd, SIOCDEVPRIVATE, &ifr);
}

/* ---------------------------------------------------------------------- */

int hdrvc_baycom_ioctl(int cmd, struct baycom_ioctl *par)
{
	struct ifreq ifr = ifr_h;

	errno = EINVAL;
	return -1;
	ifr.ifr_data = (caddr_t)par;
	par->cmd = cmd;
	return ioctl(fd, SIOCDEVPRIVATE, &ifr);
}

/* ---------------------------------------------------------------------- */

unsigned int hdrvc_get_ifflags(void)
{
	struct ifreq ifr;

	memcpy(&ifr, &ifr_h, sizeof(ifr));
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		perror("ioctl: SIOCGIFFLAGS");
		exit(-1);
	}
	return ifr.ifr_flags;
	return IFF_UP | IFF_RUNNING;
}

/* ---------------------------------------------------------------------- */

int hdrvc_diag(struct sm_diag_data *diag)
{
	struct sm_ioctl smi;
	int ret;
	struct usersmmsg msg;

	if (!diag) {
		errno = EINVAL;
		return -1;
	}
	memcpy(&smi.data.diag, diag, sizeof(smi.data.diag));
	ret = hdrvc_sm_ioctl(SMCTL_DIAGNOSE, &smi);
	memcpy(diag, &smi.data.diag, sizeof(smi.data.diag));
	return ret;
	msg.hdr.type = USERSM_CMD_REQ_DIAG;
	msg.hdr.channel = 0;
	msg.data.diag.mode = diag->mode;
	msg.data.diag.flags = diag->flags;
	msg.data.diag.samplesperbit = diag->samplesperbit;
	msg.data.diag.datalen = diag->datalen;
	hdrvc_sendmsg(&msg, sizeof(msg.data.diag));
	ret = hdrvc_recvmsg(&msg, sizeof(msg), USERSM_CMD_ACK_DIAG);
	if (ret < 0)
		return ret;
	if (ret < sizeof(msg.data.diag) || ret < sizeof(msg.data.diag)+msg.data.diag.datalen*sizeof(short)) {
		errno = EIO;
		return -1;
	}
	diag->mode = msg.data.diag.mode;
	diag->flags = msg.data.diag.flags;
	diag->samplesperbit = msg.data.diag.samplesperbit;
	diag->datalen = msg.data.diag.datalen;
	memcpy(diag->data, msg.data.diag_out.samples, msg.data.diag.datalen*sizeof(short));
	return 0;
}


/* ---------------------------------------------------------------------- */

int hdrvc_get_samples(void)
{
	int ret;
	struct hdlcdrv_ioctl bi;

	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_GETSAMPLES, &bi);
	if (ret < 0)
		return ret;
	return bi.data.bits & 0xff;
	errno = EAGAIN;
	return -1;
}

/* ---------------------------------------------------------------------- */

int hdrvc_get_bits(void)
{
	int ret;
	struct hdlcdrv_ioctl bi;

	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_GETBITS, &bi);
	if (ret < 0)
		return ret;
	return bi.data.bits & 0xff;
	errno = EAGAIN;
	return -1;
}

/* ---------------------------------------------------------------------- */

int hdrvc_get_channel_access_param(struct hdrvc_channel_params *par)
{
	struct hdlcdrv_ioctl hi;
	int ret;
	struct usersmmsg msg;

	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_GETCHANNELPAR, &hi);
	if (ret < 0)
		return ret;
	if (!par) {
		errno = EINVAL;
		return -1;
	}
	par->tx_delay = hi.data.cp.tx_delay;
	par->tx_tail = hi.data.cp.tx_tail;
	par->slottime = hi.data.cp.slottime;
	par->ppersist = hi.data.cp.ppersist;
	par->fulldup = hi.data.cp.fulldup;
	return 0;
	msg.hdr.type = USERSM_CMD_REQ_CHACCESS_PAR;
	msg.hdr.channel = 0;
	hdrvc_sendmsg(&msg, 0);
	ret = hdrvc_recvmsg(&msg, sizeof(msg), USERSM_CMD_ACK_CHACCESS_PAR);
	if (ret < 0)
		return ret;
	if (ret < sizeof(msg.data.cp)) {
		errno = EIO;
		return -1;
	}
	par->tx_delay = msg.data.cp.tx_delay;
	par->tx_tail = msg.data.cp.tx_tail;
	par->slottime = msg.data.cp.slottime;
	par->ppersist = msg.data.cp.ppersist;
	par->fulldup = msg.data.cp.fulldup;
	return 0;
}

/* ---------------------------------------------------------------------- */

int hdrvc_set_channel_access_param(struct hdrvc_channel_params par)
{
	int ret;
	struct usersmmsg msg;

	struct hdlcdrv_ioctl hi;

	hi.data.cp.tx_delay = par.tx_delay;
	hi.data.cp.tx_tail = par.tx_tail;
	hi.data.cp.slottime = par.slottime;
	hi.data.cp.ppersist = par.ppersist;
	hi.data.cp.fulldup = par.fulldup;
	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_SETCHANNELPAR, &hi);
	if (ret < 0)
		return ret;
	return 0;
	msg.hdr.type = USERSM_CMD_SET_CHACCESS_PAR;
	msg.hdr.channel = 0;
	msg.data.cp.tx_delay = par.tx_delay;
	msg.data.cp.tx_tail = par.tx_tail;
	msg.data.cp.slottime = par.slottime;
	msg.data.cp.ppersist = par.ppersist;
	msg.data.cp.fulldup = par.fulldup;
	hdrvc_sendmsg(&msg, sizeof(msg.data.cp));
	return 0;
}

/* ---------------------------------------------------------------------- */

int hdrvc_calibrate(int calib)
{
	struct hdlcdrv_ioctl bhi;
	struct usersmmsg msg;

	bhi.data.calibrate = calib;
	return hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_CALIBRATE, &bhi);
	msg.hdr.type = USERSM_CMD_CALIBRATE;
	msg.hdr.channel = 0;
	msg.data.calib = calib;
	hdrvc_sendmsg(&msg, sizeof(msg.data.calib));
	return 0;
}

/* ---------------------------------------------------------------------- */

int hdrvc_get_channel_state(struct hdrvc_channel_state *st)
{
	struct hdlcdrv_ioctl bhi;
	int ret;
	struct usersmmsg msg;

	if (!st) {
		errno = EINVAL;
		return -1;
	}
	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_GETSTAT, &bhi);
	if (ret >= 0) {
		st->ptt = bhi.data.cs.ptt;
		st->dcd = bhi.data.cs.dcd;
		st->ptt_keyed = bhi.data.cs.ptt_keyed;
		st->tx_packets = bhi.data.cs.tx_packets;
		st->tx_errors = bhi.data.cs.tx_errors;
		st->rx_packets = bhi.data.cs.rx_packets;
		st->rx_errors = bhi.data.cs.rx_errors;
	}
	return ret;
	msg.hdr.type = USERSM_CMD_REQ_CHANNELSTATE;
	msg.hdr.channel = 0;
	hdrvc_sendmsg(&msg, 0);
	ret = hdrvc_recvmsg(&msg, sizeof(msg), USERSM_CMD_ACK_CHANNELSTATE);
	if (ret < 0)
		return ret;
	if (ret < sizeof(msg.data.cs)) {
		errno = EIO;
		return -1;
	}
	st->ptt = msg.data.cs.ptt;
	st->dcd = msg.data.cs.dcd;
	st->ptt_keyed = msg.data.cs.ptt_keyed;
	st->tx_packets = msg.data.cs.tx_packets;
	st->tx_errors = msg.data.cs.tx_errors;
	st->rx_packets = msg.data.cs.rx_packets;
	st->rx_errors = msg.data.cs.rx_errors;
	return 0;
}

/* ---------------------------------------------------------------------- */

int hdrvc_diag2(unsigned int mode, unsigned int flags, short *data,
		unsigned int maxdatalen, unsigned int *samplesperbit)
{
	int ret;
	struct usersmmsg msg;
	static unsigned int modeconvusersm[4] = {
		USERSM_DIAGMODE_OFF, USERSM_DIAGMODE_INPUT, USERSM_DIAGMODE_DEMOD,
		USERSM_DIAGMODE_CONSTELLATION
	};

	if (mode > HDRVC_DIAGMODE_CONSTELLATION) {
		errno = EINVAL;
		return -1;
	}
	struct sm_ioctl smi;
	static unsigned int modeconvsm[4] = {
		SM_DIAGMODE_OFF, SM_DIAGMODE_INPUT, SM_DIAGMODE_DEMOD,
	SM_DIAGMODE_CONSTELLATION
	};

	smi.data.diag.mode = modeconvsm[mode];
	smi.data.diag.flags = (flags & HDRVC_DIAGFLAG_DCDGATE) ? SM_DIAGFLAG_DCDGATE : 0;
	smi.data.diag.samplesperbit = 0;
	smi.data.diag.datalen = maxdatalen;
	smi.data.diag.data = data;
	ret = hdrvc_sm_ioctl(SMCTL_DIAGNOSE, &smi);
	if (ret < 0)
		return ret;
	if (samplesperbit)
		*samplesperbit = smi.data.diag.samplesperbit;
	if (smi.data.diag.mode != modeconvsm[mode] || !(smi.data.diag.flags & SM_DIAGFLAG_VALID))
		return 0;
	return smi.data.diag.datalen;
	msg.hdr.type = USERSM_CMD_REQ_DIAG;
	msg.hdr.channel = 0;
	msg.data.diag.mode = modeconvusersm[mode];
	msg.data.diag.flags = (flags & HDRVC_DIAGFLAG_DCDGATE) ? USERSM_DIAGFLAG_DCDGATE : 0;
	msg.data.diag.samplesperbit = 0;
	msg.data.diag.datalen = maxdatalen;
	hdrvc_sendmsg(&msg, sizeof(msg.data.diag));
	ret = hdrvc_recvmsg(&msg, sizeof(msg), USERSM_CMD_ACK_DIAG);
	if (ret < 0)
		return ret;
	if (ret < sizeof(msg.data.diag) || ret < sizeof(msg.data.diag)+msg.data.diag.datalen*sizeof(short)) {
		errno = EIO;
		return -1;
	}
	if (samplesperbit)
		*samplesperbit = msg.data.diag.samplesperbit;
	if (msg.data.diag.mode != modeconvusersm[mode] || !(msg.data.diag.flags & USERSM_DIAGFLAG_VALID))
		return 0;
	if (!data || msg.data.diag.datalen <= 0)
		return 0;
	memcpy(data, msg.data.diag_out.samples, msg.data.diag.datalen*sizeof(short));
	return msg.data.diag.datalen;
}

/* ---------------------------------------------------------------------- */

int hdrvc_get_driver_name(char *buf, int bufsz)
{
	int ret;
	struct usersmmsg msg;

	struct hdlcdrv_ioctl bhi;

	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_DRIVERNAME, &bhi);
	if (ret < 0)
		return ret;
	strncpy(buf, bhi.data.modename, bufsz);
	return 0;
	msg.hdr.type = USERSM_CMD_REQ_DRVNAME;
	msg.hdr.channel = 0;
	hdrvc_sendmsg(&msg, 0);
	ret = hdrvc_recvmsg(&msg, sizeof(msg), USERSM_CMD_ACK_DRVNAME);
	if (ret < 0)
		return ret;
	if (ret < 1) {
		errno = EIO;
		return -1;
	}
	if (bufsz < ret)
		ret = bufsz;
	strncpy(buf, msg.data.by, ret);
	buf[ret-1] = 0;
	return 0;
}

/* ---------------------------------------------------------------------- */

int hdrvc_get_mode_name(char *buf, int bufsz)
{
	int ret;
	struct usersmmsg msg;

	struct hdlcdrv_ioctl bhi;

	ret = hdrvc_hdlcdrv_ioctl(HDLCDRVCTL_GETMODE, &bhi);
	if (ret < 0)
		return ret;
	strncpy(buf, bhi.data.modename, bufsz);
	return 0;
	msg.hdr.type = USERSM_CMD_REQ_DRVMODE;
	msg.hdr.channel = 0;
	hdrvc_sendmsg(&msg, 0);
	ret = hdrvc_recvmsg(&msg, sizeof(msg), USERSM_CMD_ACK_DRVMODE);
	if (ret < 0)
		return ret;
	if (ret < 1) {
		errno = EIO;
		return -1;
	}
	if (bufsz < ret)
		ret = bufsz;
	strncpy(buf, msg.data.by, ret);
	buf[ret-1] = 0;
	return 0;
}

/* ---------------------------------------------------------------------- */
