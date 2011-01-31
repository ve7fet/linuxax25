#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <config.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#ifdef __GLIBC__ 
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif

#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/axconfig.h>

#define	PARAM_TXDELAY	1
#define	PARAM_PERSIST	2
#define	PARAM_SLOTTIME	3
#define	PARAM_TXTAIL	4
#define	PARAM_FULLDUP	5
#define	PARAM_HARDWARE	6
#define	PARAM_FECLEVEL	8
#define	PARAM_RETURN	255

#define USAGE "usage: kissparms [-c crc-type] -p <port> [-f y|n] [-h hw] [-l txtail]\n                 [-r pers ] [-s slot] [-t txd] [-e feclevel] [-v] [-x] [-X raw]\n"

int main(int argc, char *argv[])
{
	unsigned char buffer[256];
	struct sockaddr sa;
	int proto = ETH_P_AX25;
	int txdelay  = -1;
	int txtail  = -1;
	int persist  = -1;
	int slottime = -1;
	int fulldup  = -1;
	int hardware = -1;
	int feclevel = -1;
	int crcmode = -1;
	int kissoff  = 0;
	int buflen = 0;
	int s;
	int X = 0;
	char *port   = NULL;

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "kissparms: no AX.25 ports configured\n");
		return 1;
	}

	while ((s = getopt(argc, argv, "c:e:f:h:l:p:r:s:t:X:vx")) != -1) {
		switch (s) {
			case 'c':
				crcmode  = atoi(optarg);
				break;
			case 'e':
				feclevel = atoi(optarg);
				if (feclevel < 0 || feclevel > 3) {
					fprintf(stderr, "kissparms: invalid FEC level value\n");
					return 1;
				}
				break;

			case 'f':
				if (*optarg != 'y' && *optarg != 'n') {
					fprintf(stderr, "kissparms: invalid full duplex setting\n");
					return 1;
				}
				fulldup = *optarg == 'y';
				break;

			case 'l':
				txtail = atoi(optarg) / 10;
				if (txtail < 0 || txtail > 255) {
					fprintf(stderr, "kissparms: invalid txtail value\n");
					return 1;
				}
				break;


			case 'h':
				hardware = atoi(optarg);
				if (hardware < 0 || hardware > 255) {
					fprintf(stderr, "kissparms: invalid hardware value\n");
					return 1;
				}
				break;

			case 'p':
				port = optarg;
				if (ax25_config_get_addr(port) == NULL) {
					fprintf(stderr, "kissparms: invalid port name - %s\n", port);
					return 1;
				}
				break;

			case 'r':
				persist = atoi(optarg);
				if (persist < 0 || persist > 255) {
					fprintf(stderr, "kissparms: invalid persist value\n");
					return 1;
				}
				break;

			case 's':
				slottime = atoi(optarg) / 10;
				if (slottime < 0 || slottime > 255) {
					fprintf(stderr, "kissparms: invalid slottime value\n");
					return 1;
				}
				break;

			case 't':
				txdelay = atoi(optarg) / 10;
				if (txdelay < 0 || txdelay > 255) {
					fprintf(stderr, "kissparms: invalid txdelay value\n");
					return 1;
				}
				break;

			case 'v':
				printf("kissparms: %s\n", VERSION);
				return 0;

			case 'x':
				kissoff = 1;
				break;

			case 'X':
				do {
					buffer[buflen++] = atoi(optarg);
					while (*optarg && isalnum(*optarg & 0xff))
						optarg++;
					while (*optarg && isspace(*optarg & 0xff))
						optarg++;
				} while (*optarg);
				X = 1;
				break;
			case ':':
			case '?':
				fprintf(stderr, USAGE);
				return 1;
		}
	}

	if (port == NULL) {
		fprintf(stderr, USAGE);
		return 1;
	}

	if ((s = socket(PF_PACKET, SOCK_PACKET, htons(proto))) < 0) {
		perror("kissparms: socket");
		return 1;
	}

	strcpy(sa.sa_data, ax25_config_get_dev(port));

	if (X && buflen)
		goto rawsend;
	if (kissoff) {
		buffer[0] = PARAM_RETURN;
		buflen    = 1;
rawsend:
		if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
			perror("kissparms: sendto");
			return 1;
		}
	} else {
		if (txdelay != -1) {
			buffer[0] = PARAM_TXDELAY;
			buffer[1] = txdelay;
			buflen    = 2;
			if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
				perror("kissparms: sendto");
				return 1;
			}
		}
		if (txtail != -1) {
			buffer[0] = PARAM_TXTAIL;
			buffer[1] = txtail;
			buflen    = 2;
			if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
				perror("kissparms: sendto");
				return 1;
			}
		}
		if (persist != -1) {
			buffer[0] = PARAM_PERSIST;
			buffer[1] = persist;
			buflen    = 2;
			if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
				perror("kissparms: sendto");
				return 1;
			}
		}
		if (slottime != -1) {
			buffer[0] = PARAM_SLOTTIME;
			buffer[1] = slottime;
			buflen    = 2;
			if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
				perror("kissparms: sendto");
				return 1;
			}
		}
		if (fulldup != -1) {
			buffer[0] = PARAM_FULLDUP;
			buffer[1] = fulldup;
			buflen    = 2;
			if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
				perror("kissparms: sendto");
				return 1;
			}
		}
		if (hardware != -1) {
			buffer[0] = PARAM_HARDWARE;
			buffer[1] = hardware;
			buflen    = 2;
			if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
				perror("kissparms: sendto");
				return 1;
			}
		}

		if (feclevel != -1) {
			buffer[0] = PARAM_FECLEVEL;
			buffer[1] = feclevel;

			buflen    = 2;
			if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
				perror("kissparms: sendto");
				return 1;
			}
		}

		if (crcmode != -1) {
			buffer[0] = 0x85;
			buffer[1] = crcmode;
			buflen    = 2;
			if (sendto(s, buffer, buflen, 0, &sa, sizeof(struct sockaddr)) == -1) {
				perror("kissparms: sendto");
				return 1;
			}
		}
	}
	
	close(s);
	
	return 0;
}
