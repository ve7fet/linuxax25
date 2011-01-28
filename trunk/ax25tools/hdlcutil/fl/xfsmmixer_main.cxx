/*****************************************************************************/

/*
 *	xfsmmixer_main.c  -- kernel soundcard radio modem driver mixer utility.
 *
 *	Copyright (C) 1996,1997,2000  Thomas Sailer (t.sailer@alumni.ethz.ch)
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
 *   0.1  14.12.1996  Started
 *   0.2  11.05.1997  introduced hdrvcomm.h
 *   0.3  05.01.2000  upgraded to fltk 1.0.7
 */

/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
//#include <forms.h>
#include "hdrvcomm.h"
#include "xfsmmixer.h"

/* ---------------------------------------------------------------------- */

static char *progname;
static unsigned int mixdevice;

/* ---------------------------------------------------------------------- */

static int do_mix_ioctl(int cmd, struct sm_mixer_data *mixdat)
{
        struct sm_ioctl par;
	int i;
	
	par.cmd = cmd;
	par.data.mix = *mixdat;
	i = hdrvc_sm_ioctl(cmd, &par);
	*mixdat = par.data.mix;
	return i;
}

/* ---------------------------------------------------------------------- */

static unsigned char get_mixer_reg(unsigned char addr)
{
	int i;
	struct sm_mixer_data mixdat;

	mixdat.reg = addr;
	if ((i = do_mix_ioctl(SMCTL_GETMIXER, &mixdat)) < 0) {
		perror("do_mix_ioctl: SMCTL_GETMIXER");
		exit(1);
	}
	if (!i)
		fprintf(stderr, "warning: mixer device %u register %u not "
			"accessible for reading!\n", mixdat.mixer_type,
			addr);
	if (mixdat.mixer_type != mixdevice) {
		fprintf(stderr, "error: mixer type changed !?\n");
		exit(2);
	}
	return mixdat.data;
}

/* ---------------------------------------------------------------------- */

static void set_mixer_reg(unsigned char addr, unsigned char data)
{
	struct sm_mixer_data mixdat;

	mixdat.reg = addr;
	mixdat.data = data;
	mixdat.mixer_type = mixdevice;
	if (do_mix_ioctl(SMCTL_SETMIXER, &mixdat) < 0) {
		perror("do_mix_ioctl: SMCTL_SETMIXER");
		exit(1);
	}
}

/* ---------------------------------------------------------------------- */

void update_ad1848(Fl_Widget *widget, void *udata)
{
	unsigned char mdata;
	float mval;

	mdata = 0;
	if (ad1848_srcl_line->value())
		mdata = 0x00;
	else if (ad1848_srcl_aux1->value())
		mdata = 0x40;
	else if (ad1848_srcl_mic->value())
		mdata = 0x80;
	else if (ad1848_srcl_dac->value())
		mdata = 0xc0;
	mval = ad1848_inl->value();
	if (mdata == 0x80 && mval >= 20) {
		mval -= 20;
		mdata |= 0x20;
	}
	mval *= 0.666666;
	if (mval > 15) 
		mval = 15;
	mdata |= (int)mval;
	set_mixer_reg(0x00, mdata);
	mdata = 0;
	if (ad1848_srcr_line->value())
		mdata = 0x00;
	else if (ad1848_srcr_aux1->value())
		mdata = 0x40;
	else if (ad1848_srcr_mic->value())
		mdata = 0x80;
	else if (ad1848_srcr_dac->value())
		mdata = 0xc0;
	mval = ad1848_inr->value();
	if (mdata == 0x80 && mval >= 20) {
		mval -= 20;
		mdata |= 0x20;
	}
	mval *= 0.666666;
	if (mval > 15) 
		mval = 15;
	mdata |= (int)mval;
	set_mixer_reg(0x01, mdata);
	set_mixer_reg(0x02, 0x80);
	set_mixer_reg(0x03, 0x80);
	set_mixer_reg(0x04, 0x80);
	set_mixer_reg(0x05, 0x80);
	mval = ad1848_outl->value();
	if (mval < -95)
		set_mixer_reg(0x06, 0x80);
	else 
		set_mixer_reg(0x06, (unsigned char)(mval * (-0.66666666)));
	mval = ad1848_outr->value();
	if (mval < -95)
		set_mixer_reg(0x07, 0x80);
	else 
		set_mixer_reg(0x07, (unsigned char)(mval * (-0.66666666)));
	set_mixer_reg(0x0d, 0x00);
}

/* ---------------------------------------------------------------------- */

void update_ct1335(Fl_Widget *widget, void *udata)
{
	float mval;

	mval = ct1335_out->value() * (7.0 / 46.0);
	set_mixer_reg(0x02, ((((int)(mval)) + 7) << 1));
	set_mixer_reg(0x06, 0x00);
	set_mixer_reg(0x08, 0x00);
	set_mixer_reg(0x0a, 0x06);
}

/* ---------------------------------------------------------------------- */

void update_ct1345(Fl_Widget *widget, void *udata)
{
	unsigned char mdata;
	double mval;

	set_mixer_reg(0x04, 0xee);
	set_mixer_reg(0x0a, 0x00);
	mdata = 0x20;
	if (ct1345_src_mic->value())
		mdata = 0x20;
	else if (ct1345_src_cd->value())
		mdata = 0x22;
	else if (ct1345_src_line->value())
		mdata = 0x26;
	set_mixer_reg(0x0c, mdata);
	set_mixer_reg(0x0e, 0x20);
	mval = ct1345_outl->value() * (7.0 / 46.0);
	mdata = (((int)(mval))+7) << 5;
	mval = ct1345_outr->value() * (7.0 / 46.0);
	mdata |= (((int)(mval))+7) << 1;
	set_mixer_reg(0x22, mdata);
	set_mixer_reg(0x26, 0x00);
	set_mixer_reg(0x28, 0x00);
	set_mixer_reg(0x2e, 0x00);
}

/* ---------------------------------------------------------------------- */

void update_ct1745(Fl_Widget *widget, void *udata)
{
	unsigned char mdata;
	float mval;

	set_mixer_reg(0x3c, 0); /* output src: only voice and pcspk */
	mval = ct1745_treble->value();
	set_mixer_reg(0x44, ((int)mval)<<4); /* treble.l */
	set_mixer_reg(0x45, ((int)mval)<<4); /* treble.r */
	mval = ct1745_bass->value();
	set_mixer_reg(0x46, ((int)mval)<<4); /* bass.l */
	set_mixer_reg(0x47, ((int)mval)<<4); /* bass.r */
	set_mixer_reg(0x3b, 0); /* PC speaker vol -18dB */
	set_mixer_reg(0x43, 1); /* mic: agc off, fixed 20dB gain */
	mval = ct1745_outl->value();
	mdata = 0;
	while (mval > 0) {
		mval -= 6;
		mdata += 0x40;
	}
	set_mixer_reg(0x41, mdata); /* output gain */
	set_mixer_reg(0x30, 0xf8); /* master vol */
	set_mixer_reg(0x32, ((int)(mval * 0.5) + 31) << 3); /* voice vol */
	mval = ct1745_outr->value();
	mdata = 0;
	while (mval > 0) {
		mval -= 6;
		mdata += 0x40;
	}
	set_mixer_reg(0x42, mdata); /* output gain */
	set_mixer_reg(0x31, 0xf8); /* master vol */
	set_mixer_reg(0x33, ((int)(mval * 0.5) + 31) << 3); /* voice vol */
	mval = ct1745_inl->value();
	mdata = 0;
	while (mval > 0) {
		mval -= 6;
		mdata += 0x40;
	}
	set_mixer_reg(0x3f, mdata); /* input gain */
	mdata = ((int)(mval * 0.5) + 31) << 3;
	set_mixer_reg(0x34, mdata); /* midi vol */
	set_mixer_reg(0x36, mdata); /* cd vol */
	set_mixer_reg(0x38, mdata); /* line vol */
	set_mixer_reg(0x3a, mdata); /* mic vol */
	mval = ct1745_inr->value();
	mdata = 0;
	while (mval > 0) {
		mval -= 6;
		mdata += 0x40;
	}
	set_mixer_reg(0x40, mdata); /* input gain */
	mdata = ((int)(mval * 0.5) + 31) << 3;
	set_mixer_reg(0x35, mdata); /* midi vol */
	set_mixer_reg(0x37, mdata); /* cd vol */
	set_mixer_reg(0x39, mdata); /* line vol */
	mdata = 0;
	if (ct1745_srcl_mic->value())
		mdata |= 0x01;
	if (ct1745_srcl_cdl->value())
		mdata |= 0x04;
	if (ct1745_srcl_cdr->value())
		mdata |= 0x02;
	if (ct1745_srcl_linel->value())
		mdata |= 0x10;
	if (ct1745_srcl_liner->value())
		mdata |= 0x08;
	if (ct1745_srcl_midil->value())
		mdata |= 0x40;
	if (ct1745_srcl_midir->value())
		mdata |= 0x20;				
	set_mixer_reg(0x3d, mdata); /* input sources left */
	mdata = 0;
	if (ct1745_srcr_mic->value())
		mdata |= 0x01;
	if (ct1745_srcr_cdl->value())
		mdata |= 0x04;
	if (ct1745_srcr_cdr->value())
		mdata |= 0x02;
	if (ct1745_srcr_linel->value())
		mdata |= 0x10;
	if (ct1745_srcr_liner->value())
		mdata |= 0x08;
	if (ct1745_srcr_midil->value())
		mdata |= 0x40;
	if (ct1745_srcr_midir->value())
		mdata |= 0x20;				
	set_mixer_reg(0x3e, mdata); /* input sources right*/
}

/* ---------------------------------------------------------------------- */

void cb_quit(Fl_Button *, long)
{
	exit (0);
}

/* ---------------------------------------------------------------------- */

static const char *usage_str = 
"[-i smif]\n"
"  -i: specify the name of the soundmodem kernel driver interface\n\n";

/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
        int c, i;
	struct sm_mixer_data mixdat;
	unsigned char mdata;
	
	progname = *argv;
	printf("%s: Version 0.3; (C) 1996,1997,2000 by Thomas Sailer HB9JNX/AE4WA\n", progname);
	hdrvc_args(&argc, argv, "sm0");
	for (i = 1; i < argc; ) {
		c = i;
		Fl::arg(argc, argv, c);
		if (c <= 0) {
			i++;
			continue;
		}
		memmove(&argv[i], &argv[i+c], (argc-i) * sizeof(char *));
		argc -= c;
	}
#if 0
	while ((ret = getopt(argc, argv, "")) != -1) {
		switch (ret) {
		default:
			printf("usage: %s %s", progname, usage_str);
			exit(-1);
		}
	}
#endif
	hdrvc_init();
	/*
	 * set mixer
	 */
	mixdat.reg = 0;
	if (do_mix_ioctl(SMCTL_GETMIXER, &mixdat) < 0) {
		perror("do_mix_ioctl: SMCTL_GETMIXER");
		mixdevice = SM_MIXER_INVALID;
	} else
		mixdevice = mixdat.mixer_type;
	switch (mixdevice) {
	case SM_MIXER_INVALID:
		printf("invalid mixer device\n");
		exit(1);

	case SM_MIXER_AD1848:
	case SM_MIXER_CRYSTAL:
		printf("Mixer device: %s\n", mixdevice == SM_MIXER_CRYSTAL ? 
		       "CS423x" : "AD1848");
		create_form_ad1848();
		mdata = get_mixer_reg(0);
		ad1848_inl->step(1.5);
		ad1848_inl->bounds(42.5, 0);
		ad1848_inl->value((((mdata & 0xe0) == 0xa0) ? 20 : 0) + (mdata & 0xf) * 1.5);
		ad1848_srcl_line->value((mdata & 0xc0) == 0x00);
		ad1848_srcl_aux1->value((mdata & 0xc0) == 0x40);
		ad1848_srcl_mic->value((mdata & 0xc0) == 0x80);
		ad1848_srcl_dac->value((mdata & 0xc0) == 0xc0);
		mdata = get_mixer_reg(1);
		ad1848_inr->step(1.5);
		ad1848_inr->bounds(42.5, 0);
		ad1848_inr->value((((mdata & 0xe0) == 0xa0) ? 20 : 0) + (mdata & 0xf) * 1.5);
		ad1848_srcr_line->value((mdata & 0xc0) == 0x00);
		ad1848_srcr_aux1->value((mdata & 0xc0) == 0x40);
		ad1848_srcr_mic->value((mdata & 0xc0) == 0x80);
		ad1848_srcr_dac->value((mdata & 0xc0) == 0xc0);
		mdata = get_mixer_reg(6);
		ad1848_outl->step(1.5);
		ad1848_outl->bounds(0, -100);
		ad1848_outl->value((mdata & 0x80) ? -100 : (mdata & 0x3f) * -1.5);
		mdata = get_mixer_reg(7);
		ad1848_outr->step(1.5);
		ad1848_outr->bounds(0, -100);
		ad1848_outr->value((mdata & 0x80) ? -100 : (mdata & 0x3f) * -1.5);
		mixer_ad1848->show();
		break;

	case SM_MIXER_CT1335:
		printf("Mixer device: CT1335\n");
		create_form_ct1335();
		mdata = get_mixer_reg(0x2);
		ct1335_out->step(46.0/7.0);
		ct1335_out->bounds(0, -46);
		ct1335_out->value((((int)((mdata >> 1) & 7)) - 7) * (46.0/7.0));
		mixer_ct1335->show();
		break;

	case SM_MIXER_CT1345:
		printf("Mixer device: CT1345\n");
		create_form_ct1345();
		mdata = get_mixer_reg(0x22);
		ct1345_outl->step(46.0/7.0);
		ct1345_outl->bounds(0, -46);
		ct1345_outl->value((((int)((mdata >> 5) & 7)) - 7) * (46.0/7.0));
		ct1345_outr->step(46.0/7.0);
		ct1345_outr->bounds(0, -46);
		ct1345_outr->value((((int)((mdata >> 1) & 7)) - 7) * (46.0/7.0));
		mdata = get_mixer_reg(0xc);
		ct1345_src_mic->value((mdata & 6) == 0 || (mdata & 6) == 4);
		ct1345_src_cd->value((mdata & 6) == 2);
		ct1345_src_line->value((mdata & 6) == 6);
		mixer_ct1345->show();
		break;

	case SM_MIXER_CT1745:
		printf("Mixer device: CT1745\n");
		create_form_ct1745();
		ct1745_outl->step(2);
		ct1745_outl->bounds(0, -62);
		ct1745_outl->value(((int)((get_mixer_reg(0x32) >> 3) & 0x1f) - 31) * 2 +
				   ((get_mixer_reg(0x41) >> 6) & 0x3) * 6);
		ct1745_outr->step(2);
		ct1745_outr->bounds(0, -62);
		ct1745_outr->value(((int)((get_mixer_reg(0x33) >> 3) & 0x1f) - 31) * 2 +
				   ((get_mixer_reg(0x42) >> 6) & 0x3) * 6);
		ct1745_inl->step(2);
		ct1745_inl->bounds(0, -62);
		ct1745_inl->value(((int)((get_mixer_reg(0x38) >> 3) & 0x1f) - 31) * 2 +
				  ((get_mixer_reg(0x3f) >> 6) & 0x3) * 6);
		ct1745_inr->step(2);
		ct1745_inr->bounds(0, -62);
		ct1745_inr->value(((int)((get_mixer_reg(0x39) >> 3) & 0x1f) - 31) * 2 +
				  ((get_mixer_reg(0x40) >> 6) & 0x3) * 6);
		mdata = get_mixer_reg(0x3d);
		ct1745_srcl_mic->value(!!(mdata & 1));
		ct1745_srcl_cdl->value(!!(mdata & 4));
		ct1745_srcl_cdr->value(!!(mdata & 2));
		ct1745_srcl_linel->value(!!(mdata & 0x10));
		ct1745_srcl_liner->value(!!(mdata & 8));
		ct1745_srcl_midil->value(!!(mdata & 0x40));
		ct1745_srcl_midir->value(!!(mdata & 0x20));
		mdata = get_mixer_reg(0x3e);
		ct1745_srcr_mic->value(!!(mdata & 1));
		ct1745_srcr_cdl->value(!!(mdata & 4));
		ct1745_srcr_cdr->value(!!(mdata & 2));
		ct1745_srcr_linel->value(!!(mdata & 0x10));
		ct1745_srcr_liner->value(!!(mdata & 8));
		ct1745_srcr_midil->value(!!(mdata & 0x40));
		ct1745_srcr_midir->value(!!(mdata & 0x20));
		ct1745_treble->step(1);
		ct1745_treble->bounds(7, -8);
		ct1745_treble->value((get_mixer_reg(0x44) >> 4) & 0xf);
		ct1745_bass->step(1);
		ct1745_bass->bounds(7, -8);
		ct1745_bass->value((get_mixer_reg(0x46) >> 4) & 0xf);
		mixer_ct1745->show();
		break;

	default:
		printf("unknown mixer device %d\n", mixdevice);
		exit(1);
	}
	Fl::run();
	exit(0);
}
