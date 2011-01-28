/*****************************************************************************/

/*
 *	smmixer.c  -- kernel soundcard radio modem driver mixer utility.
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
 *   0.1  26.06.1996  Started
 *   0.2  11.05.1997  introduced hdrvcomm.h
 *   0.3  05.01.2000  glibc compile fixes
 */

/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include "hdrvcomm.h"

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

static void display_mixer_ad1848(void)
{
	static const char *src[4] = { "Line", "Aux1", "Mic", "Dac" };
	unsigned char data;
	
	data = get_mixer_reg(0);
	printf("Left input:  Source: %-4s Gain: %3ddB\n", src[(data>>6)&3],
	       (((data & 0xe0) == 0xa0) ? 20 : 0) + (data & 0xf) * 3 / 2);
	data = get_mixer_reg(1);
	printf("Right input: Source: %-4s Gain: %3ddB\n", src[(data>>6)&3],
	       (((data & 0xe0) == 0xa0) ? 20 : 0) + (data & 0xf) * 3 / 2);
	data = get_mixer_reg(2);
	if (!(data & 0x80))
		printf("Left Aux1 mixing:  Gain: %3ddB\n", 
		       (8 - (int)(data & 0x1f)) * 3 / 2);
	data = get_mixer_reg(3);
	if (!(data & 0x80))
		printf("Right Aux1 mixing: Gain: %3ddB\n", 
		       (8 - (int)(data & 0x1f)) * 3 / 2);
	data = get_mixer_reg(4);
	if (!(data & 0x80))
		printf("Left Aux2 mixing:  Gain: %3ddB\n", 
		       (8 - (int)(data & 0x1f)) * 3 / 2);
	data = get_mixer_reg(5);
	if (!(data & 0x80))
		printf("Right Aux2 mixing: Gain: %3ddB\n", 
		       (8 - (int)(data & 0x1f)) * 3 / 2);
	data = get_mixer_reg(6);
	if (data & 0x80) 
		printf("Left output:  muted\n");
	else 
		printf("Left output:  Gain: %3ddB\n", 
		       ((int)(data & 0x3f)) * (-3) / 2);
	data = get_mixer_reg(7);
	if (data & 0x80) 
		printf("Right output: muted\n");
	else 
		printf("Right output: Gain: %3ddB\n", 
		       ((int)(data & 0x3f)) * (-3) / 2);
	data = get_mixer_reg(13);
	if (data & 1)
		printf("Digital mix: Gain: %3ddB\n",
		       ((int)((data >> 2) & 0x3f)) * (-3) / 2);
}

/* ---------------------------------------------------------------------- */

static void display_mixer_cs423x(void)
{
	unsigned char data;

	display_mixer_ad1848();
	data = get_mixer_reg(26);
	printf("Mono: %s%s%s Gain: %3ddB\n", 
	       (data & 0x80) ? "input muted, " : "",
	       (data & 0x40) ? "output muted, " : "",
	       (data & 0x20) ? "bypass, " : "",
	       (int)(data & 0xf) * (-3));
       	data = get_mixer_reg(27);
	if (data & 0x80) 
		printf("Left output:  muted\n");
	else 
		printf("Left output:  Gain: %3ddB\n", 
		       ((int)(data & 0xf)) * (-2));
	data = get_mixer_reg(29);
	if (data & 0x80) 
		printf("Right output: muted\n");
	else 
		printf("Right output: Gain: %3ddB\n", 
		       ((int)(data & 0xf)) * (-2));
	
}

/* ---------------------------------------------------------------------- */

static void display_mixer_ct1335(void)
{
	unsigned char data;
	
	data = get_mixer_reg(0x2);
	printf("Master volume: %3ddB\n", 
	       (((int)((data >> 1) & 7)) - 7) * 46 / 7);
	data = get_mixer_reg(0xa);
	printf("Voice volume:  %3ddB\n", 
	       (((int)((data >> 1) & 3)) - 3) * 46 / 3);
	data = get_mixer_reg(0x6);
	printf("MIDI volume:   %3ddB\n", 
	       (((int)((data >> 1) & 7)) - 7) * 46 / 7);
	data = get_mixer_reg(0x8);
	printf("CD volume:     %3ddB\n", 
	       (((int)((data >> 1) & 7)) - 7) * 46 / 7);
}

/* ---------------------------------------------------------------------- */

static void display_mixer_ct1345(void)
{	
	static const char *src[4] = { "Mic", "CD", "Mic", "Line" };
	unsigned char data, data2;
	
	data = get_mixer_reg(0xc);
	data2 = get_mixer_reg(0xe);
	printf("Input source: %s\n", src[(data >> 1) & 3]);
	if (!(data & data2 & 0x20)) { 
		printf("Filter: Low pass %s kHz: ", (data & 0x8) ? 
		       "8.8" : "3.2");
		if (data & 0x20)
			printf("output\n");
		else 
			printf("input%s\n", (data2 & 0x20) ? "" : ", output");
	}
	if (data2 & 2)
		printf("stereo\n");
	
	data = get_mixer_reg(0x22);
	printf("Master volume: Left: %3ddB  Right: %3ddB\n", 
	       (((int)((data >> 5) & 7)) - 7) * 46 / 7,
	       (((int)((data >> 1) & 7)) - 7) * 46 / 7);
	data = get_mixer_reg(0x4);
	printf("Voice volume:  Left: %3ddB  Right: %3ddB\n", 
	       (((int)((data >> 5) & 7)) - 7) * 46 / 7,
	       (((int)((data >> 1) & 7)) - 7) * 46 / 7);
	data = get_mixer_reg(0x26);
	printf("MIDI volume:   Left: %3ddB  Right: %3ddB\n", 
	       (((int)((data >> 5) & 7)) - 7) * 46 / 7,
	       (((int)((data >> 1) & 7)) - 7) * 46 / 7);
	data = get_mixer_reg(0x28);
	printf("CD volume:     Left: %3ddB  Right: %3ddB\n", 
	       (((int)((data >> 5) & 7)) - 7) * 46 / 7,
	       (((int)((data >> 1) & 7)) - 7) * 46 / 7);
	data = get_mixer_reg(0x2e);
	printf("Line volume:   Left: %3ddB  Right: %3ddB\n", 
	       (((int)((data >> 5) & 7)) - 7) * 46 / 7,
	       (((int)((data >> 1) & 7)) - 7) * 46 / 7);
	data = get_mixer_reg(0x0a);
	printf("Mic mixing volume:   %3ddB\n", 
	       (((int)((data >> 1) & 3)) - 3) * 46 / 3);
}

/* ---------------------------------------------------------------------- */

static void display_mixer_ct1745(void)
{	
	unsigned char data, data2;
	
	printf("Master volume: Left: %3ddB  Right: %3ddB\n",
	       ((int)((get_mixer_reg(0x30) >> 3) & 0x1f) - 31) * 2,
	       ((int)((get_mixer_reg(0x31) >> 3) & 0x1f) - 31) * 2);
	printf("Voice volume:  Left: %3ddB  Right: %3ddB\n",
	       ((int)((get_mixer_reg(0x32) >> 3) & 0x1f) - 31) * 2,
	       ((int)((get_mixer_reg(0x33) >> 3) & 0x1f) - 31) * 2);
	printf("MIDI volume:   Left: %3ddB  Right: %3ddB\n",
	       ((int)((get_mixer_reg(0x34) >> 3) & 0x1f) - 31) * 2,
	       ((int)((get_mixer_reg(0x35) >> 3) & 0x1f) - 31) * 2);
	printf("CD volume:     Left: %3ddB  Right: %3ddB\n",
	       ((int)((get_mixer_reg(0x36) >> 3) & 0x1f) - 31) * 2,
	       ((int)((get_mixer_reg(0x37) >> 3) & 0x1f) - 31) * 2);
	printf("Line volume:   Left: %3ddB  Right: %3ddB\n",
	       ((int)((get_mixer_reg(0x38) >> 3) & 0x1f) - 31) * 2,
	       ((int)((get_mixer_reg(0x39) >> 3) & 0x1f) - 31) * 2);
	printf("Mic volume:          %3ddB\n",
	       ((int)((get_mixer_reg(0x3a) >> 3) & 0x1f) - 31) * 2);
	printf("PC speaker volume:   %3ddB\n",
	       ((int)((get_mixer_reg(0x3b) >> 6) & 0x3) - 3) * 6);
	printf("Mic gain:      %s\n", 
	       (get_mixer_reg(0x43) & 1) ? "fixed 20dB" : "AGC");
	printf("Output gain:   Left: %3ddB  Right: %3ddB\n",
	       ((int)((get_mixer_reg(0x41) >> 6) & 3)) * 6,
	       ((int)((get_mixer_reg(0x42) >> 6) & 3)) * 6);
	printf("Input gain:    Left: %3ddB  Right: %3ddB\n",
	       ((int)((get_mixer_reg(0x3f) >> 6) & 3)) * 6,
	       ((int)((get_mixer_reg(0x40) >> 6) & 3)) * 6);
	data = (get_mixer_reg(0x44) >> 4) & 0xf;
	if (data > 7)
		data--;
	data2 = (get_mixer_reg(0x45) >> 4) & 0xf;
	if (data2 > 7)
		data2--;
	printf("Treble:        Left: %3ddB  Right: %3ddB\n",
	       ((int)data - 7) * 2, ((int)data2 - 7) * 2);
	data = (get_mixer_reg(0x46) >> 4) & 0xf;
	if (data > 7)
		data--;
	data2 = (get_mixer_reg(0x47) >> 4) & 0xf;
	if (data2 > 7)
		data2--;
	printf("Bass:          Left: %3ddB  Right: %3ddB\n",
	       ((int)data - 7) * 2, ((int)data2 - 7) * 2);
	data = get_mixer_reg(0x3c);
	printf("Output sources left:  PCSpeaker Voice.L%s%s%s\n",
	       (data & 1) ? " Mic" : "", (data & 4) ? " CD.L" : "",
	       (data & 0x10) ? " Line.L" : "");
	printf("Output sources right: PCSpeaker Voice.R%s%s%s\n",
	       (data & 1) ? " Mic" : "", (data & 2) ? " CD.R" : "",
	       (data & 8) ? " Line.R" : "");
	data = get_mixer_reg(0x3d);
	printf("Input sources left:  %s%s%s%s%s%s%s\n",
	       (data & 1) ? " Mic" : "", (data & 2) ? " CD.R" : "", 
	       (data & 4) ? " CD.L" : "", (data & 8) ? " Line.R" : "", 
	       (data & 0x10) ? " Line.L" : "", (data & 0x20) ? " Midi.R" : "",
	       (data & 0x40) ? " Midi.L" : "");
	data = get_mixer_reg(0x3e);
	printf("Input sources right: %s%s%s%s%s%s%s\n",
	       (data & 1) ? " Mic" : "", (data & 2) ? " CD.R" : "", 
	       (data & 4) ? " CD.L" : "", (data & 8) ? " Line.R" : "", 
	       (data & 0x10) ? " Line.L" : "", (data & 0x20) ? " Midi.R" : "",
	       (data & 0x40) ? " Midi.L" : "");
}

/* ---------------------------------------------------------------------- */

static int parse_ad_src(const char *cp)
{
        if (!strcasecmp(cp, "line"))
                return 0;
        if (!strcasecmp(cp, "aux1"))
                return 1;
        if (!strcasecmp(cp, "mic"))
                return 2;
        if (!strcasecmp(cp, "dac"))
                return 3;
        return -1;
}

/* ---------------------------------------------------------------------- */

static int set_mixer_ad1848(int argc, char *argv[])
{
	unsigned int mask = 0;
	int olvll = 0, olvlr = 0;
	int isrcl = 0, isrcr = 0;
	int ilvll = 0, ilvlr = 0;
	unsigned int data;

	for (; argc >= 1; argc--, argv++) {
		if (!strncasecmp(argv[0], "ol=", 3)) {
			olvll = strtol(argv[0]+3, NULL, 0);
			mask |= 1;
			if (olvll < -46 || olvll > 0) {
				fprintf(stderr, "output level out of range "
					"-100..0dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "or=", 3)) {
			olvlr = strtol(argv[0]+3, NULL, 0);
			mask |= 2;
			if (olvlr < -46 || olvlr > 0) {
				fprintf(stderr, "output level out of range "
					"-100..0dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "o=", 2)) {
			olvll = olvlr = strtol(argv[0]+2, NULL, 0);
			mask |= 3;
			if (olvll < -46 || olvll > 0) {
				fprintf(stderr, "output level out of range "
					"-100..0dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "il=", 3)) {
			ilvll = strtol(argv[0]+3, NULL, 0);
			mask |= 4;
			if (ilvll < 0 || ilvll > 43) {
				fprintf(stderr, "input gain out of range "
					"0..43dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "ir=", 3)) {
			ilvlr = strtol(argv[0]+3, NULL, 0);
			mask |= 8;
			if (ilvll < 0 || ilvll > 43) {
				fprintf(stderr, "input gain out of range "
					"0..43dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "i=", 2)) {
			ilvll = ilvlr = strtol(argv[0]+2, NULL, 0);
			mask |= 12;
			if (ilvll < 0 || ilvll > 43) {
				fprintf(stderr, "input gain out of range "
					"0..43dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "sl=", 3)) {
			mask |= 16;
			if ((isrcl = parse_ad_src(argv[0]+3)) < 0) {
				fprintf(stderr, "invalid input source, must "
					"be either line, aux1, mic or dac\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "sr=", 3)) {
			mask |= 32;
			if ((isrcr = parse_ad_src(argv[0]+3)) < 0) {
				fprintf(stderr, "invalid input source, must "
					"be either line, aux1, mic or dac\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "s=", 2)) {
			mask |= 48;
			if ((isrcl = isrcr = parse_ad_src(argv[0]+2)) < 0) {
				fprintf(stderr, "invalid input source, must "
					"be either line, aux1, mic or dac\n");
				return -1;
			}
		} else {
			fprintf(stderr, "invalid parameter \"%s\"\n", argv[0]);
			return -1;
		}
	}
	data = get_mixer_reg(0x00);
	if (mask & 4) {
		data &= 0xc0;
		if (ilvll > 23) {
			data |= 0x20;
			ilvll -= 20;
		}
		data |= ilvll * 2 / 3;
	}
	if (mask & 16)
		data = (data & 0x3f) | (isrcl << 6);
	if ((data & 0xc0) != 0x80)
		data &= ~0x20;
	set_mixer_reg(0x00, data);
	data = get_mixer_reg(0x01);
	if (mask & 8) {
		data &= 0xc0;
		if (ilvlr > 23) {
			data |= 0x20;
			ilvlr -= 20;
		}
		data |= ilvlr * 2 / 3;
	}
	if (mask & 32)
		data = (data & 0x3f) | (isrcr << 6);
	if ((data & 0xc0) != 0x80)
		data &= ~0x20;
	set_mixer_reg(0x01, data);
	set_mixer_reg(0x02, 0x80);
	set_mixer_reg(0x03, 0x80);
	set_mixer_reg(0x04, 0x80);
	set_mixer_reg(0x05, 0x80);
	if (mask & 1) {
		if (olvll < -95)
			set_mixer_reg(0x06, 0x80);
		else 
			set_mixer_reg(0x06, (olvll * (-2) / 3));
	}
	if (mask & 2) {
		if (olvlr < -95)
			set_mixer_reg(0x07, 0x80);
		else 
			set_mixer_reg(0x07, (olvlr * (-2) / 3));
	}
	set_mixer_reg(0x0d, 0x00);
	return 0;
}

/* ---------------------------------------------------------------------- */

static int set_mixer_ct1335(int argc, char *argv[])
{
	unsigned int mask = 0;
	int olvl = 0;

	for (; argc >= 1; argc--, argv++) {
		if (!strncasecmp(argv[0], "o=", 2)) {
			olvl = strtol(argv[0]+2, NULL, 0);
			mask |= 1;
			if (olvl < -46 || olvl > 0) {
				fprintf(stderr, "output level out of range "
					"-46..0dB\n");
				return -1;
			}
		} else {
			fprintf(stderr, "invalid parameter \"%s\"\n", argv[0]);
			return -1;
		}
	}
	if (mask & 1)
		set_mixer_reg(0x02, (((olvl * 7 / 46) + 7) << 1));
	set_mixer_reg(0x06, 0x00);
	set_mixer_reg(0x08, 0x00);
	set_mixer_reg(0x0a, 0x06);
	return 0;
}

/* ---------------------------------------------------------------------- */

static int set_mixer_ct1345(int argc, char *argv[])
{
	unsigned int mask = 0;
	int olvll = 0, olvlr = 0;
	int isrc = 0;
	unsigned int data;

	for (; argc >= 1; argc--, argv++) {
		if (!strncasecmp(argv[0], "ol=", 3)) {
			olvll = strtol(argv[0]+3, NULL, 0);
			mask |= 1;
			if (olvll < -46 || olvll > 0) {
				fprintf(stderr, "output level out of range "
					"-46..0dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "or=", 3)) {
			olvlr = strtol(argv[0]+3, NULL, 0);
			mask |= 2;
			if (olvlr < -46 || olvlr > 0) {
				fprintf(stderr, "output level out of range "
					"-46..0dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "o=", 2)) {
			olvll = olvlr = strtol(argv[0]+2, NULL, 0);
			mask |= 3;
			if (olvll < -46 || olvll > 0) {
				fprintf(stderr, "output level out of range "
					"-46..0dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "s=", 2)) {
			mask |= 4;
			if (!strcasecmp(argv[0]+2, "mic"))
				isrc = 0;
			else if (!strcasecmp(argv[0]+2, "cd"))
				isrc = 1;
			else if (!strcasecmp(argv[0]+2, "line"))
				isrc = 3;
			else {
				fprintf(stderr, "invalid input source, must "
					"be either mic, cd or line\n");
				return -1;
			}
		} else {
			fprintf(stderr, "invalid parameter \"%s\"\n", argv[0]);
			return -1;
		}
	}
	set_mixer_reg(0x04, 0xee);
	set_mixer_reg(0x0a, 0x00);
	if (mask & 4)
		data = (isrc & 3) << 1;
	else
		data = get_mixer_reg(0x0c) & 0x06;
	set_mixer_reg(0x0c, data | 0x20);
	set_mixer_reg(0x0e, 0x20);
	data = get_mixer_reg(0x22);
	if (mask & 1)
		data = (data & 0x0f) | (((olvll * 7 / 46) + 7) << 5);
	if (mask & 2)
		data = (data & 0xf0) | (((olvlr * 7 / 46) + 7) << 1);
	set_mixer_reg(0x22, data);
	set_mixer_reg(0x26, 0x00);
	set_mixer_reg(0x28, 0x00);
	set_mixer_reg(0x2e, 0x00);
	return 0;
}

/* ---------------------------------------------------------------------- */

static int set_mixer_ct1745(int argc, char *argv[])
{
	unsigned int mask = 0;
	int olvll = 0, olvlr = 0;
	int ilvll = 0, ilvlr = 0;
	unsigned int insrc = 0;
	unsigned int data;

	for (; argc >= 1; argc--, argv++) {
		if (!strncasecmp(argv[0], "ol=", 3)) {
			olvll = strtol(argv[0]+3, NULL, 0);
			mask |= 1;
			if (olvll < -62 || olvll > 18) {
				fprintf(stderr, "output level out of range "
					"-62..18dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "or=", 3)) {
			olvlr = strtol(argv[0]+3, NULL, 0);
			mask |= 2;
			if (olvlr < -62 || olvlr > 18) {
				fprintf(stderr, "output level out of range "
					"-62..18dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "o=", 2)) {
			olvll = olvlr = strtol(argv[0]+2, NULL, 0);
			mask |= 3;
			if (olvll < -62 || olvll > 18) {
				fprintf(stderr, "output level out of range "
					"-62..18dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "il=", 3)) {
			ilvll = strtol(argv[0]+3, NULL, 0);
			mask |= 4;
			if (ilvll < -62 || ilvll > 18) {
				fprintf(stderr, "input gain out of range "
					"-62..18dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "ir=", 3)) {
			ilvlr = strtol(argv[0]+3, NULL, 0);
			mask |= 8;
			if (ilvll < -62 || ilvll > 18) {
				fprintf(stderr, "input gain out of range "
					"-62..18dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "i=", 2)) {
			ilvll = ilvlr = strtol(argv[0]+2, NULL, 0);
			mask |= 12;
			if (ilvll < -62 || ilvll > 18) {
				fprintf(stderr, "input gain out of range "
					"-62..18dB\n");
				return -1;
			}
		} else if (!strncasecmp(argv[0], "s=", 2)) {
			mask |= 16;
			if (!strcasecmp(argv[0]+2, "mic")) 
				insrc |= 1;
			else if (!strcasecmp(argv[0]+2, "cd.r")) 
				insrc |= 2;
			else if (!strcasecmp(argv[0]+2, "cd.l")) 
				insrc |= 4;
			else if (!strcasecmp(argv[0]+2, "cd")) 
				insrc |= 6;
			else if (!strcasecmp(argv[0]+2, "line.r")) 
				insrc |= 0x08;
			else if (!strcasecmp(argv[0]+2, "line.l")) 
				insrc |= 0x10;
			else if (!strcasecmp(argv[0]+2, "line")) 
				insrc |= 0x18;
			else if (!strcasecmp(argv[0]+2, "midi.r")) 
				insrc |= 0x20;
			else if (!strcasecmp(argv[0]+2, "midi.l")) 
				insrc |= 0x40;
			else if (!strcasecmp(argv[0]+2, "midi")) 
				insrc |= 0x60;
			else {
				fprintf(stderr, "invalid input source, must "
					"be either line, cd, mic or midi\n");
				return -1;
			}
		} else {
			fprintf(stderr, "invalid parameter \"%s\"\n", argv[0]);
			return -1;
		}
	}
	if (mask & 3) {
		set_mixer_reg(0x3c, 0); /* output src: only voice and pcspk */
		set_mixer_reg(0x44, 4<<4); /* treble.l: -6dB */
		set_mixer_reg(0x45, 4<<4); /* treble.r: -6dB */
		set_mixer_reg(0x46, 6<<4); /* bass.l: -2dB */
		set_mixer_reg(0x47, 6<<4); /* bass.r: -2dB */
		set_mixer_reg(0x3b, 0); /* PC speaker vol -18dB */
	}
	if (mask & 12)
		set_mixer_reg(0x43, 1); /* mic: agc off, fixed 20dB gain */
	if (mask & 1) {
		data = 0;
		while (olvll > 0) {
			olvll -= 6;
			data += 0x40;
		}
		set_mixer_reg(0x41, data); /* output gain */
		set_mixer_reg(0x30, 0xf8); /* master vol */
		set_mixer_reg(0x32, (olvll / 2 + 31) << 3); /* voice vol */
	}
	if (mask & 2) {
		data = 0;
		while (olvlr > 0) {
			olvlr -= 6;
			data += 0x40;
		}
		set_mixer_reg(0x42, data); /* output gain */
		set_mixer_reg(0x31, 0xf8); /* master vol */
		set_mixer_reg(0x33, (olvlr / 2 + 31) << 3); /* voice vol */
	}
	if (mask & 4) {
		data = 0;
		while (ilvll > 0) {
			ilvll -= 6;
			data += 0x40;
		}
		set_mixer_reg(0x3f, data); /* input gain */
		data = (ilvll / 2 + 31) << 3;
		set_mixer_reg(0x34, data); /* midi vol */
		set_mixer_reg(0x36, data); /* cd vol */
		set_mixer_reg(0x38, data); /* line vol */
		set_mixer_reg(0x3a, data); /* mic vol */
	}
	if (mask & 8) {
		data = 0;
		while (ilvlr > 0) {
			ilvlr -= 6;
			data += 0x40;
		}
		set_mixer_reg(0x40, data); /* input gain */
		data = (ilvlr / 2 + 31) << 3;
		set_mixer_reg(0x35, data); /* midi vol */
		set_mixer_reg(0x37, data); /* cd vol */
		set_mixer_reg(0x39, data); /* line vol */
		set_mixer_reg(0x3a, data); /* mic vol */
	}
	if (mask & 16) {
		set_mixer_reg(0x3d, insrc); /* input sources */
		set_mixer_reg(0x3e, insrc); /* input sources */
	}
	return 0;
}

/* ---------------------------------------------------------------------- */

static const char *usage_str = 
"[-i smif]\n\n";

int main(int argc, char *argv[])
{
	int c;
	struct sm_mixer_data mixdat;

	progname = *argv;
	printf("%s: Version 0.2; (C) 1996-1997 by Thomas Sailer HB9JNX/AE4WA\n", *argv);
	hdrvc_args(&argc, argv, "sm0");
	while ((c = getopt(argc, argv, "")) != -1) {
		switch (c) {
		default:
			printf("usage: %s %s", *argv, usage_str);
			exit(-1);
		}
	}
	hdrvc_init();
	mixdat.reg = 0;
	if (do_mix_ioctl(SMCTL_GETMIXER, &mixdat) < 0) {
		perror("do_mix_ioctl: SMCTL_GETMIXER");
		exit(1);
	}
	mixdevice = mixdat.mixer_type;
	switch (mixdevice) {
	case SM_MIXER_INVALID:
		printf("invalid mixer device\n");
		exit(1);
	case SM_MIXER_AD1848:
		if (optind < argc)
			set_mixer_ad1848(argc - optind, argv + optind);
		display_mixer_ad1848();
		break;
	case SM_MIXER_CRYSTAL:
		if (optind < argc)
			set_mixer_ad1848(argc - optind, argv + optind);
		display_mixer_cs423x();
		break;
	case SM_MIXER_CT1335:
		if (optind < argc)
			set_mixer_ct1335(argc - optind, argv + optind);
		display_mixer_ct1335();
		break;
	case SM_MIXER_CT1345:
		if (optind < argc)
			set_mixer_ct1345(argc - optind, argv + optind);
		display_mixer_ct1345();
		break;
	case SM_MIXER_CT1745:
		if (optind < argc)
			set_mixer_ct1745(argc - optind, argv + optind);
		display_mixer_ct1745();
		break;
	default:
		printf("unknown mixer device %d\n", mixdevice);
		exit(1);
	}
	exit(0);
}

/* ---------------------------------------------------------------------- */
