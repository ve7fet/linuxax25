#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <config.h>

#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>
#include <netax25/mheard.h>

#include "../pathnames.h"

struct PortRecord {
	struct mheard_struct entry;
	struct PortRecord *Next;
};

static char *types[] = {
	"SABM",
	"SABME",
	"DISC",
	"UA",
	"DM",
	"RR",
	"RNR",
	"REJ",
	"FRMR",
	"I",
	"UI",
	"????"};

static struct PortRecord *PortList = NULL;

static void PrintHeader(int data)
{
	switch (data) {
		case 0:
			printf("Callsign  Port Packets   Last Heard\n");
			break;
		case 1:
			printf("Callsign                                                              Port\n");
			break;
		case 2:
			printf("Callsign  Port      #I    #S    #U  First Heard          Last Heard\n");
			break;
		case 3:
			printf("Callsign  Port Packets  Type  PIDs\n");
			break;
	}
}

static void PrintPortEntry(struct PortRecord *pr, int data)
{
	char lh[30], fh[30], *call, *s;
	char buffer[80];
	int i;

	switch (data) {
		case 0:
			strcpy(lh, ctime(&pr->entry.last_heard));
			lh[19] = 0;
			call =  ax25_ntoa(&pr->entry.from_call);
			if ((s = strstr(call, "-0")) != NULL)
				*s = '\0';
			printf("%-10s %-5s %5d   %s\n",
				call, pr->entry.portname, pr->entry.count, lh);
			break;
		case 1:
			buffer[0] = '\0';
			call = ax25_ntoa(&pr->entry.from_call);
			if ((s = strstr(call, "-0")) != NULL)
				*s = '\0';
			strcat(buffer, call);
			call = ax25_ntoa(&pr->entry.to_call);
			if ((s = strstr(call, "-0")) != NULL)
				*s = '\0';
			strcat(buffer, ">");
			strcat(buffer, call);
			for (i = 0; i < pr->entry.ndigis && i < 4; i++) {
				strcat(buffer, ",");
				call = ax25_ntoa(&pr->entry.digis[i]);
				if ((s = strstr(call, "-0")) != NULL)
					*s = '\0';
				strcat(buffer, call);
			}
			if (pr->entry.ndigis >= 4)
				strcat(buffer, ",...");
			printf("%-70s %-5s\n",
				buffer, pr->entry.portname);
			break;
		case 2:
			strcpy(lh, ctime(&pr->entry.last_heard));
			lh[19] = 0;
			strcpy(fh, ctime(&pr->entry.first_heard));
			fh[19] = 0;
			call = ax25_ntoa(&pr->entry.from_call);
			if ((s = strstr(call, "-0")) != NULL)
				*s = '\0';
			printf("%-10s %-5s %5d %5d %5d  %s  %s\n",
				call, pr->entry.portname, pr->entry.iframes, pr->entry.sframes, pr->entry.uframes, fh, lh);
			break;
		case 3:
			call = ax25_ntoa(&pr->entry.from_call);
			if ((s = strstr(call, "-0")) != NULL)
				*s = '\0';
			printf("%-10s %-5s %5d %5s ",
				call, pr->entry.portname, pr->entry.count, types[pr->entry.type]);
			if (pr->entry.mode & MHEARD_MODE_ARP)
				printf(" ARP");
			if (pr->entry.mode & MHEARD_MODE_FLEXNET)
				printf(" FlexNet");
			if (pr->entry.mode & MHEARD_MODE_IP_DG)
				printf(" IP-DG");
			if (pr->entry.mode & MHEARD_MODE_IP_VC)
				printf(" IP-VC");
			if (pr->entry.mode & MHEARD_MODE_NETROM)
				printf(" NET/ROM");
			if (pr->entry.mode & MHEARD_MODE_ROSE)
				printf(" Rose");
			if (pr->entry.mode & MHEARD_MODE_SEGMENT)
				printf(" Segment");
			if (pr->entry.mode & MHEARD_MODE_TEXNET)
				printf(" TexNet");
			if (pr->entry.mode & MHEARD_MODE_TEXT)
				printf(" Text");
			if (pr->entry.mode & MHEARD_MODE_PSATFT)
				printf(" PacsatFT");
			if (pr->entry.mode & MHEARD_MODE_PSATPB)
				printf(" PacsatPB");
			if (pr->entry.mode & MHEARD_MODE_UNKNOWN)
				printf(" Unknown");
			printf("\n");
			break;
	}
}

static void ListAllPorts(int data)
{
	struct PortRecord *pr;

	for (pr = PortList; pr != NULL; pr = pr->Next)
		PrintPortEntry(pr, data);
}

static void ListOnlyPort(char *name, int data)
{
	struct PortRecord *pr;

	for (pr = PortList; pr != NULL; pr = pr->Next)
		if (strcmp(pr->entry.portname, name) == 0)
			PrintPortEntry(pr, data);
}

static void LoadPortData(void)
{
	FILE *fp;
	struct PortRecord *pr;
	struct mheard_struct mheard;

	if ((fp = fopen(DATA_MHEARD_FILE, "r")) == NULL) {
		fprintf(stderr, "mheard: cannot open mheard data file\n");
		exit(1);
	}

	while (fread(&mheard, sizeof(struct mheard_struct), 1, fp) == 1) {
		pr = malloc(sizeof(struct PortRecord));
		pr->entry = mheard;
		pr->Next  = PortList;
		PortList  = pr;
	}

	fclose(fp);
}

static void SortByTime(void)
{
	struct PortRecord *p = PortList;
	struct PortRecord *n;
	PortList = NULL;

	while (p != NULL) {
		struct PortRecord *w = PortList;

		n = p->Next;

		if (w == NULL || p->entry.last_heard > w->entry.last_heard) {
			p->Next  = w;
			PortList = p;
			p = n;
			continue;
		}

		while (w->Next != NULL && p->entry.last_heard <= w->Next->entry.last_heard)
			w = w->Next;

		p->Next = w->Next;
		w->Next = p;
		p = n;
	}
}


static void SortByPort(void)
{
	struct PortRecord *p = PortList;
	struct PortRecord *n;
	PortList = NULL;

	while (p != NULL) {
		struct PortRecord *w = PortList;

		n = p->Next;

		if (w == NULL || strcmp(p->entry.portname, w->entry.portname) < 0) {
			p->Next  = w;
			PortList = p;
			p = n;
			continue;
		}

		while (w->Next != NULL && strcmp(p->entry.portname, w->Next->entry.portname) >= 0)
			w = w->Next;

		p->Next = w->Next;
		w->Next = p;
		p = n;
	}
}

static void SortByCall(void)
{
	struct PortRecord *p = PortList;
	struct PortRecord *n;
	PortList = NULL;

	while (p != NULL) {
		struct PortRecord *w = PortList;

		n = p->Next;

		if (w == NULL || memcmp(&p->entry.from_call, &w->entry.from_call, sizeof(ax25_address)) < 0) {
			p->Next  = w;
			PortList = p;
			p = n;
			continue;
		}

		while (w->Next != NULL && memcmp(&p->entry.from_call, &w->Next->entry.from_call, sizeof(ax25_address)) >= 0)
			w = w->Next;

		p->Next = w->Next;
		w->Next = p;
		p = n;
	}
}

static void SortByFrame(void)
{
	struct PortRecord *p = PortList;
	struct PortRecord *n;
	PortList = NULL;

	while (p != NULL) {
		struct PortRecord *w = PortList;

		n = p->Next;

		if (w == NULL || p->entry.count > w->entry.count) {
			p->Next  = w;
			PortList = p;
			p = n;
			continue;
		}

		while (w->Next != NULL && p->entry.count <= w->Next->entry.count)
			w = w->Next;

		p->Next = w->Next;
		w->Next = p;
		p = n;
	}
}
		
int main(int argc, char *argv[])
{
	int headers = TRUE;
	int mode = 0;
	int data = 0;
	int c;

	while ((c = getopt(argc, argv, "d:no:v")) != -1) {
		switch (c) {
			case 'd':
				switch (*optarg) {
					case 'c':
						data = 1;
						break;
					case 'm':
						data = 3;
						break;
					case 'n':
						data = 0;
						break;
					case 's':
						data = 2;
						break;
					default:
						fprintf(stderr, "mheard: invalid display type '%s'\n", optarg);
						return 1;
				}
				break;
			case 'n':
				headers = FALSE;
				break;
			case 'o':
				switch (*optarg) {
					case 'c':
						mode = 2;
						break;
					case 'f':
						mode = 3;
						break;
					case 'p':
						mode = 1;
						break;
					case 't':
						mode = 0;
						break;
					default:
						fprintf(stderr, "mheard: invalid ordering type '%s'\n", optarg);
						return 1;
				}
				break;
			case 'v':
				printf("mheard: %s\n", VERSION);
				return 0;
			case '?':
			case ':':
				fprintf(stderr, "Usage: %s [-d cmns] [-n] [-o cfpt] [-v] [port ...]\n", argv[0]);
				return 1;
		}
	}
	
	LoadPortData();

	switch (mode) {
		case 0: SortByTime();  break;
		case 1: SortByPort();  break;
		case 2: SortByCall();  break;
		case 3: SortByFrame(); break;
	}

	if (argc == optind) {
		if (headers)
			PrintHeader(data);
		ListAllPorts(data);
	} else {
		while (argv[optind] != NULL) {
			if (headers) {
				printf("Port %s:\n", argv[optind]);
				PrintHeader(data);
			}
			ListOnlyPort(argv[optind], data);
			optind++;
		}
	}

	return 0;
}
