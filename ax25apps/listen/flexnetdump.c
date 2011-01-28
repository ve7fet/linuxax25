/*
 * FlexNet internode communication dump
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "listen.h"

/*
 * This was hacked by Thomas Sailer, t.sailer@alumni.ethz.ch, HB9JNX/AE4WA, with
 * some bits stolen from Dieter Deyke (Wampes), such as these defines :-)
 */

#define FLEX_INIT       '0'	/* Link initialization */
#define FLEX_RPRT       '1'	/* Poll answer */
#define FLEX_POLL       '2'	/* Poll */
#define FLEX_ROUT       '3'	/* Routing information */
#define FLEX_QURY       '6'	/* Path query */
#define FLEX_RSLT       '7'	/* Query result */

static int flx_get_number(unsigned char **data, int *length)
{
	int l = *length;
	unsigned char *d = *data;
	int res = 0;

	if (l <= 0 || *d < '0' || *d > '9')
		return -1;
	while (l > 0 && *d >= '0' && *d <= '9') {
		res = res * 10 + (*d++) - '0';
		l--;
	}
	*data = d;
	*length = l;
	return res;
}

static void dump_end(unsigned char *data, int length)
{
	if (length <= 0)
		return;
	for (; length > 0; length--, data++)
		lprintf(T_FLEXNET, " %02x", *data);
	lprintf(T_FLEXNET, "\n");
}

void flexnet_dump(unsigned char *data, int length, int hexdump)
{
	int i;
	int hopcnt, qsonr;
	unsigned char *cp;

	lprintf(T_PROTOCOL, "FlexNet:");

	if (length < 1) {
		lprintf(T_ERROR, " bad packet\n");
		return;
	}
	switch (data[0]) {
	default:
		lprintf(T_ERROR, " unknown packet type\n");
		data_dump(data, length, hexdump);
		return;

	case FLEX_INIT:
		if (length < 2) {
			lprintf(T_ERROR, " bad packet\n");
			dump_end(data + 1, length - 1);
			return;
		}
		lprintf(T_FLEXNET, " Link setup - max SSID %d ",
			data[1] & 0xf);
		dump_end(data + 1, length - 1);
		return;

	case FLEX_RPRT:
		lprintf(T_FLEXNET, " Poll response -");
		data++;
		length--;
		i = flx_get_number(&data, &length);
		data++;
		length--;
		if (i < 0)
			goto too_short;
		lprintf(T_FLEXNET, " delay: %d\n", i);
		dump_end(data, length);
		return;

	case FLEX_POLL:
		lprintf(T_FLEXNET, " Poll\n");
		return;

	case FLEX_ROUT:
		data++;
		length--;
		lprintf(T_FLEXNET, " Routing\n");
		while (length > 0) {
			if (isdigit(*data) || isupper(*data)) {
				if (length < 10)
					goto too_short;
				lprintf(T_FLEXNET, "  %.6s %2d-%2d ", data,
					data[6] - '0', data[7] - '0');
				data += 8;
				length -= 8;
				i = flx_get_number(&data, &length);
				data++;
				length--;
				if (!i)
					lprintf(T_FLEXNET, "link down\n");
				else
					lprintf(T_FLEXNET, "delay: %d\n",
						i);
			} else {
				if (*data == '+')
					lprintf(T_FLEXNET,
						"  Request token\n");
				else if (*data == '-')
					lprintf(T_FLEXNET,
						"  Release token\n");
				else if (*data != '\r')
					lprintf(T_FLEXNET,
						"  invalid char: %02x\n",
						*data);
				data++;
				length--;
			}
		}
		return;

	case FLEX_QURY:
	case FLEX_RSLT:
		lprintf(T_FLEXNET, " Route query");
		if (*data == FLEX_RSLT)
			lprintf(T_FLEXNET, " response");
		if (length < 2)
			goto too_short;
		hopcnt = data[1] - ' ';
		data += 2;
		length -= 2;
		qsonr = flx_get_number(&data, &length);
		data++;
		length--;
		lprintf(T_FLEXNET, " - hop count: %d QSO number: %d\n",
			hopcnt, qsonr);
		cp = memchr(data, '\r', length);
		if (cp)
			*cp = 0;
		lprintf(T_FLEXNET, "  data\n");
		return;
	}

      too_short:
	lprintf(T_ERROR, " packet too short\n");
	dump_end(data, length);
}
