
/* tnt: Hostmode Terminal for TNC
   Copyright (C) 1993 by Mark Wahl
   For license details see documentation
   Procedures for autobin-checksum (crc.c)
   created: Mark Wahl DL4YBG 94/01/17
   updated: Mark Wahl DL4YBG 94/01/17
*/

static int crcbit[8] = {
	0x9188, 0x48c4, 0x2462, 0x1231, 0x8108, 0x4084, 0x2042, 0x1021
};

static int bittab[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };

static int crctab[256];

void init_crc(void)
{
	int i, j;

	for (i = 0; i < 256; i++) {
		crctab[i] = 0;
		for (j = 0; j < 8; j++) {
			if ((bittab[j] & i) != 0) {
				crctab[i] = crctab[i] ^ crcbit[j];
			}
		}
	}
}

/* calculate checksum for autobin-protocol */
unsigned int calc_crc(unsigned char *buf, int n, unsigned crc)
{
	while (--n >= 0)
		crc =
		    (crctab[(crc >> 8)] ^ ((crc << 8) | *buf++)) & 0xffff;
	return crc;
}
