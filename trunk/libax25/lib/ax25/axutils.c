#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/socket.h>
#include <config.h>

#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netrose/rose.h>

ax25_address null_ax25_address = {{0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00}};

/*
 *	Library routine for callsign conversion.
 */
 
int ax25_aton_entry(const char *name, char *buf)
{
	int ct   = 0;
	int ssid = 0;
	const char *p = name;
	char c;

	while (ct < 6) {
		c = toupper(*p);

		if (c == '-' || c == '\0')
			break;

		if (!isalnum(c)) {
			printf("axutils: invalid symbol in callsign '%s'\n", name);
			return -1;
		}

		buf[ct] = c << 1;

		p++;
		ct++;
	}

	while (ct < 6) {
		buf[ct] = ' ' << 1;
		ct++;
	}

	if (*p != '\0') {
		p++;

		if (sscanf(p, "%d", &ssid) != 1 || ssid < 0 || ssid > 15) {
			printf("axutils: SSID must follow '-' and be numeric in the range 0-15 - '%s'\n", name);
			return -1;
		}
	}

	buf[6] = ((ssid + '0') << 1) & 0x1E;

	return 0;
}

int ax25_aton(const char *call, struct full_sockaddr_ax25 *sax)
{
	char *bp, *np;
	char *addrp;
	int n = 0;
	char *tmp = strdup(call);
	
	if (tmp == NULL)
		return -1;
		
	bp = tmp;
	
	addrp = sax->fsa_ax25.sax25_call.ax25_call;

	do {
		/* Fetch one callsign token */
		while (*bp != '\0' && isspace(*bp))
			bp++;

		np = bp;

		while (*np != '\0' && !isspace(*np))
			np++;

		if (*np != '\0')
			*np++ = '\0';
	
		/* Check for the optional 'via' syntax */
		if (n == 1 && (strcasecmp(bp, "V") == 0 || strcasecmp(bp, "VIA") == 0)) {
			bp = np;
			continue;
		}
		
		/* Process the token */
		if (ax25_aton_entry(bp, addrp) == -1) {
			free(tmp);
			return -1;
		}
			
		/* Move along */
		bp = np;
		n++;

		if (n == 1)
			addrp  = sax->fsa_digipeater[0].ax25_call;	/* First digipeater address */
		else
			addrp += sizeof(ax25_address);

	} while (n <= AX25_MAX_DIGIS && *bp);

	free(tmp);

	/* Tidy up */
	sax->fsa_ax25.sax25_ndigis = n - 1;
	sax->fsa_ax25.sax25_family = AF_AX25;	

	return sizeof(struct full_sockaddr_ax25);
}

int ax25_aton_arglist(const char *call[], struct full_sockaddr_ax25 *sax)
{
	const char *bp;
	char *addrp;
	int n = 0;
	int argp = 0;

	addrp = sax->fsa_ax25.sax25_call.ax25_call;

	do {
		/* Fetch one callsign token */
		if ((bp = call[argp++]) == NULL)
			break;
	
		/* Check for the optional 'via' syntax */
		if (n == 1 && (strcasecmp(bp, "V") == 0 || strcasecmp(bp, "VIA") == 0))
			continue;
		
		/* Process the token */
		if (ax25_aton_entry(bp, addrp) == -1)
			return -1;
			
		n++;

		if (n == 1)
			addrp  = sax->fsa_digipeater[0].ax25_call;	/* First digipeater address */
		else
			addrp += sizeof(ax25_address);

	} while (n <= AX25_MAX_DIGIS && call[argp] != NULL);

	/* Tidy up */
	sax->fsa_ax25.sax25_ndigis = n - 1;
	sax->fsa_ax25.sax25_family = AF_AX25;	

	return sizeof(struct full_sockaddr_ax25);
}

/*
 *	Library routine for Rose address conversion.
 */
 
int rose_aton(const char *addr, char *buf)
{
	int i, n;

	if (strlen(addr) != 10) {
		printf("axutils: invalid rose address '%s' length = %d\n", addr, strlen(addr));
		return -1;
	}

	if (strspn(addr, "0123456789") != 10) {
		printf("axutils: invalid characters in address\n");
		return -1;
	}

	for (n = 0, i = 0; i < 5; i++, n += 2) {
		buf[i]  = (addr[n + 0] - '0') << 4;
		buf[i] |= (addr[n + 1] - '0') << 0;
	}

	return 0;
}

/*
 *	ax25 -> ascii conversion
 */
char *ax25_ntoa(const ax25_address *a)
{
	static char buf[11];
	char c, *s;
	int n;

	for (n = 0, s = buf; n < 6; n++) {
		c = (a->ax25_call[n] >> 1) & 0x7F;

		if (c != ' ') *s++ = c;
	}
	
	*s++ = '-';

	if ((n = ((a->ax25_call[6] >> 1) & 0x0F)) > 9) {
		*s++ = '1';
		n -= 10;
	}
	
	*s++ = n + '0';
	*s++ = '\0';

	return buf;
}

/*
 *	rose -> ascii conversion
 */
char *rose_ntoa(const rose_address *a)
{
	static char buf[11];

	sprintf(buf, "%02X%02X%02X%02X%02X", a->rose_addr[0] & 0xFF,
					     a->rose_addr[1] & 0xFF,
					     a->rose_addr[2] & 0xFF,
					     a->rose_addr[3] & 0xFF,
					     a->rose_addr[4] & 0xFF);

	return buf;
}

/*
 *	Compare two ax.25 addresses
 */
int ax25_cmp(const ax25_address *a, const ax25_address *b)
{
	if ((a->ax25_call[0] & 0xFE) != (b->ax25_call[0] & 0xFE))
		return 1;

	if ((a->ax25_call[1] & 0xFE) != (b->ax25_call[1] & 0xFE))
		return 1;

	if ((a->ax25_call[2] & 0xFE) != (b->ax25_call[2] & 0xFE))
		return 1;

	if ((a->ax25_call[3] & 0xFE) != (b->ax25_call[3] & 0xFE))
		return 1;

	if ((a->ax25_call[4] & 0xFE) != (b->ax25_call[4] & 0xFE))
		return 1;

	if ((a->ax25_call[5] & 0xFE) != (b->ax25_call[5] & 0xFE))
		return 1;

 	if ((a->ax25_call[6] & 0x1E) != (b->ax25_call[6] & 0x1E))	/* SSID without control bit */
 		return 2;

 	return 0;
}

/*
 *	Compare two Rose addresses
 */
int rose_cmp(const rose_address *a, const rose_address *b)
{
	int i;

	for (i = 0; i < 5; i++)
		if (a->rose_addr[i] != b->rose_addr[i])
			return 1;
			
	return 0;
}

/*
 * Validate an AX.25 callsign.
 */
int ax25_validate(const char *call)
{
	char s[7];
	int n;

	for (n = 0; n < 6; n++)
		s[n] = (call[n] >> 1) & 0x7F;
	s[6] = '\0';

	if (strspn(s, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ") == 6)
		return TRUE;

	return FALSE;
}

/*
 *	Convert a string to upper case
 */
char *strupr(char *s)
{
	char *p;

	if (s == NULL)
		return NULL;
	
	for (p = s; *p != '\0'; p++)
		*p = toupper(*p);
		
	return s;
}

/*
 *	Convert a string to lower case
 */
char *strlwr(char *s)
{
	char *p;

	if (s == NULL)
		return NULL;
	
	for (p = s; *p != '\0'; p++)
		*p = tolower(*p);
		
	return s;
}

