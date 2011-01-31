#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <sys/socket.h>

#ifdef __GLIBC__
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif

#include <netinet/in.h>

#include <config.h>
#include <netax25/ax25.h>
#include <netrose/rose.h>


#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/daemon.h>
#include <netax25/mheard.h>

#include <config.h>

#include "../pathnames.h"

#define	KISS_MASK	0x0F
#define	KISS_DATA	0x00

#define	PID_SEGMENT	0x08
#define	PID_ARP		0xCD
#define	PID_NETROM	0xCF
#define	PID_IP		0xCC
#define	PID_ROSE	0x01
#define	PID_TEXNET	0xC3
#define	PID_FLEXNET	0xCE
#define	PID_TEXT	0xF0
#define	PID_PSATFT	0xBB
#define	PID_PSATPB	0xBD

#define	I		0x00
#define	S		0x01
#define	RR		0x01
#define	RNR		0x05
#define	REJ		0x09
#define	U		0x03
#define	SABM		0x2F
#define	SABME		0x6F
#define	DISC		0x43
#define	DM		0x0F
#define	UA		0x63
#define	FRMR		0x87
#define	UI		0x03

#define	PF		0x10
#define	EPF		0x01

#define	MMASK		7

#define	HDLCAEB		0x01
#define	SSID		0x1E
#define	SSSID_SPARE	0x40
#define	ESSID_SPARE	0x20

#define	ALEN		6
#define	AXLEN		7

struct mheard_list_struct {
	int in_use;
	struct mheard_struct entry;
	long position;
};

static struct mheard_list_struct *mheard_list;
#define	MHEARD_LIST_SIZE 1000
static int    mheard_list_size = MHEARD_LIST_SIZE/10;
static int    logging = FALSE;

static int ftype(unsigned char *, int *, int);
static struct mheard_list_struct *findentry(ax25_address *, char *);

static void terminate(int sig)
{
	if (logging) {
		syslog(LOG_INFO, "terminating on SIGTERM\n");
		closelog();
	}

	exit(0);
}

int main(int argc, char **argv)
{
	struct mheard_list_struct *mheard;
	unsigned char buffer[1500], *data;
	int size, s;
	char *port = NULL;
	struct sockaddr sa;
	socklen_t asize;
	long position;
	int ctlen, type, end, extseq, flush = FALSE;
	FILE *fp;
	char *p;
	char ports[1024];
	int ports_excl = 0;

	*ports = 0;
	while ((s = getopt(argc, argv, "fln:p:v")) != -1) {
		switch (s) {
			case 'l':
				logging = TRUE;
				break;
			case 'f':
				flush = TRUE;
				break;
			case 'n':
				mheard_list_size = atoi(optarg);
				if (mheard_list_size < 10 || mheard_list_size > MHEARD_LIST_SIZE) {
					fprintf(stderr, "mheardd: list size must be between 10 and %d\n", MHEARD_LIST_SIZE);
					return 1;
				}
				break;
			case 'p':
				if (strlen(optarg) > sizeof(ports)-4) {
					fprintf(stderr, "mheardd: too many ports specified.");
					return 1;
				}
				if (*optarg == '!') {
					ports_excl = 1;
					optarg++;
				}
				sprintf(ports, "|%s|", optarg);
				for (p = ports; *p; p++) {
					if (*p == ' ' || *p == ',')
						*p = '|';
				}
				break;
			case 'v':
				printf("mheardd: %s\n", VERSION);
				return 0;
			case ':':
				fprintf(stderr, "mheardd: option -n needs an argument\n");
				return 1;
			case '?':
				fprintf(stderr, "Usage: mheardd [-f] [-l] [-n number] [-p [!]port1[,port2,..]] [-v]\n");
				return 1;
		}
	}

	signal(SIGTERM, terminate);

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "mheardd: no AX.25 port data configured\n");
		return 1;
	}

	if ((mheard_list = calloc(mheard_list_size, sizeof(struct mheard_list_struct))) == NULL) {
		fprintf(stderr, "mheardd: cannot allocate memory\n");
		return 1;
	}

	if (flush)
		unlink(DATA_MHEARD_FILE);

	/* Load an existing heard list */
	if ((fp = fopen(DATA_MHEARD_FILE, "r")) != NULL) {
		s = 0;
		position = ftell(fp);

		while (fread(buffer, sizeof(struct mheard_struct), 1, fp) == 1 && s < mheard_list_size) {
			memcpy(&mheard_list[s].entry, buffer, sizeof(struct mheard_struct));
			mheard_list[s].in_use   = TRUE;
			mheard_list[s].position = position;
			position = ftell(fp);
			s++;
		}
		
		fclose(fp);
	} else {
		if ((fp = fopen(DATA_MHEARD_FILE, "w")) != NULL)
			fclose(fp);
	}

	if ((s = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_AX25))) == -1) {
		perror("mheardd: socket");
		return 1;
	}
	
	if (!daemon_start(FALSE)) {
		fprintf(stderr, "mheardd: cannot become a daemon\n");
		return 1;
	}

	/* Use syslog for error messages rather than perror/fprintf */
	if (logging) {
		openlog("mheardd", LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "starting");
	}

	for (;;) {
		asize = sizeof(sa);

		if ((size = recvfrom(s, buffer, sizeof(buffer), 0, &sa, &asize)) == -1) {
			if (logging) {
				syslog(LOG_ERR, "recv: %m");
				closelog();
			}
			return 1;
		}
		
		if ((port = ax25_config_get_name(sa.sa_data)) == NULL) {
			if (logging)
				syslog(LOG_WARNING, "unknown port '%s'\n", sa.sa_data);
			continue;
		}
		if (*ports) {
			char testport[sizeof(sa.sa_data)+2];
			sprintf(testport, "|%s|", sa.sa_data);
			if (ports_excl) {
				if (strstr(ports, testport)) {
					continue;
				}
			} else {
				if (!strstr(ports, testport)) {
					continue;
				}
			}
		}

		data = buffer;

		if ((*data & KISS_MASK) != KISS_DATA)
			continue;

		data++;
		size--;

		if (size < (AXLEN + AXLEN + 1)) {
			if (logging)
				syslog(LOG_WARNING, "packet too short\n");
			continue;
		}

		mheard = findentry((ax25_address *)(data + AXLEN), port);

		if (!ax25_validate(data + 0) || !ax25_validate(data + AXLEN)) {
			if (logging)
				syslog(LOG_WARNING, "invalid callsign on port %s\n", port);
			continue;
		}

		memcpy(&mheard->entry.from_call, data + AXLEN, sizeof(ax25_address));
		memcpy(&mheard->entry.to_call,   data + 0,     sizeof(ax25_address));
		strcpy(mheard->entry.portname,   port);
		mheard->entry.ndigis = 0;

		extseq = ((data[AXLEN + ALEN] & SSSID_SPARE) != SSSID_SPARE);
		end    = (data[AXLEN + ALEN] & HDLCAEB);

		data += (AXLEN + AXLEN);
		size -= (AXLEN + AXLEN);

		while (!end) {
			memcpy(&mheard->entry.digis[mheard->entry.ndigis], data, sizeof(ax25_address));
			mheard->entry.ndigis++;
			
			end = (data[ALEN] & HDLCAEB);
			
			data += AXLEN;
			size -= AXLEN;
		}

		if (size == 0) {
			if (logging)
				syslog(LOG_WARNING, "packet too short\n");
			continue;
		}

		ctlen = ftype(data, &type, extseq);

		mheard->entry.count++;

		switch (type) {
			case SABM:
				mheard->entry.type = MHEARD_TYPE_SABM;
				mheard->entry.uframes++;
				break;
			case SABME:
				mheard->entry.type = MHEARD_TYPE_SABME;
				mheard->entry.uframes++;
				break;
			case DISC:
				mheard->entry.type = MHEARD_TYPE_DISC;
				mheard->entry.uframes++;
				break;
			case UA:
				mheard->entry.type = MHEARD_TYPE_UA;
				mheard->entry.uframes++;
				break;
			case DM:
				mheard->entry.type = MHEARD_TYPE_DM;
				mheard->entry.uframes++;
				break;
			case RR:
				mheard->entry.type = MHEARD_TYPE_RR;
				mheard->entry.sframes++;
				break;
			case RNR:
				mheard->entry.type = MHEARD_TYPE_RNR;
				mheard->entry.sframes++;
				break;
			case REJ:
				mheard->entry.type = MHEARD_TYPE_REJ;
				mheard->entry.sframes++;
				break;
			case FRMR:
				mheard->entry.type = MHEARD_TYPE_FRMR;
				mheard->entry.uframes++;
				break;
			case I:
				mheard->entry.type = MHEARD_TYPE_I;
				mheard->entry.iframes++;
				break;
			case UI:
				mheard->entry.type = MHEARD_TYPE_UI;
				mheard->entry.uframes++;
				break;
			default:
				if (logging)
					syslog(LOG_WARNING, "unknown packet type %02X\n", *data);
				mheard->entry.type = MHEARD_TYPE_UNKNOWN;
				break;
		}

		data += ctlen;
		size -= ctlen;

		if (type == I || type == UI) {
			switch (*data) {
				case PID_TEXT:
					mheard->entry.mode |= MHEARD_MODE_TEXT;
					break;
				case PID_SEGMENT:
					mheard->entry.mode |= MHEARD_MODE_SEGMENT;
					break;
				case PID_ARP:
					mheard->entry.mode |= MHEARD_MODE_ARP;
					break;
				case PID_NETROM:
					mheard->entry.mode |= MHEARD_MODE_NETROM;
					break;
				case PID_IP:
					mheard->entry.mode |= (type == I) ? MHEARD_MODE_IP_VC : MHEARD_MODE_IP_DG;
					break;
				case PID_ROSE:
					mheard->entry.mode |= MHEARD_MODE_ROSE;
					break;
				case PID_TEXNET:
					mheard->entry.mode |= MHEARD_MODE_TEXNET;
					break;
				case PID_FLEXNET:
					mheard->entry.mode |= MHEARD_MODE_FLEXNET;
					break;
				case PID_PSATPB:
					mheard->entry.mode |= MHEARD_MODE_PSATPB;
					break;
				case PID_PSATFT:
					mheard->entry.mode |= MHEARD_MODE_PSATFT;
					break;
				default:
					if (logging)
						syslog(LOG_WARNING, "unknown PID %02X\n", *data);
					mheard->entry.mode |= MHEARD_MODE_UNKNOWN;
					break;
			}
		}
		
		if (mheard->entry.first_heard == 0)
			time(&mheard->entry.first_heard);
			
		time(&mheard->entry.last_heard);

		if ((fp = fopen(DATA_MHEARD_FILE, "r+")) == NULL) {
			if (logging)
				syslog(LOG_ERR, "cannot open mheard data file\n");
			continue;
		}
		
		if (mheard->position == 0xFFFFFF) {
			fseek(fp, 0L, SEEK_END);
			mheard->position = ftell(fp);
		}

		fseek(fp, mheard->position, SEEK_SET);
		
		fwrite(&mheard->entry, sizeof(struct mheard_struct), 1, fp);
		
		fclose(fp);
	}
}

static int ftype(unsigned char *data, int *type, int extseq)
{
	if (extseq) {
		if ((*data & 0x01) == 0) {	/* An I frame is an I-frame ... */
			*type = I;
			return 2;
		}
		if (*data & 0x02) {
			*type = *data & ~PF;
			return 1;
		} else {
			*type = *data;
			return 2;
		}
	} else {
		if ((*data & 0x01) == 0) {	/* An I frame is an I-frame ... */
			*type = I;
			return 1;
		}
		if (*data & 0x02) {		/* U-frames use all except P/F bit for type */
			*type = *data & ~PF;
			return 1;
		} else {			/* S-frames use low order 4 bits for type */
			*type = *data & 0x0F;
			return 1;
		}
	}
}

static struct mheard_list_struct *findentry(ax25_address *callsign, char *port)
{
	struct mheard_list_struct *oldest = NULL;
	int i;
	
	for (i = 0; i < mheard_list_size; i++)
		if (mheard_list[i].in_use &&
		    ax25_cmp(&mheard_list[i].entry.from_call, callsign) == 0 &&
		    strcmp(mheard_list[i].entry.portname, port) == 0)
			return mheard_list + i;

	for (i = 0; i < mheard_list_size; i++) {
		if (!mheard_list[i].in_use) {
			mheard_list[i].in_use   = TRUE;
			mheard_list[i].position = 0xFFFFFF;
			return mheard_list + i;
		}
	}

	for (i = 0; i < mheard_list_size; i++) {
		if (mheard_list[i].in_use) {
			if (oldest == NULL) {
				oldest = mheard_list + i;
			} else {
				if (mheard_list[i].entry.last_heard < oldest->entry.last_heard)
					oldest = mheard_list + i;
			}
		}
	}

	memset(&oldest->entry, 0x00, sizeof(struct mheard_struct));

	return oldest;
}

