/*
 * kissnetd.c : Simple kiss broadcast daemon between several
 *		pseudo-tty devices.
 *		Each kiss frame received on one pty is broadcasted
 *		to each other one.
 *
 * ATEPRA FPAC/Linux Project
 *
 * F1OAT 960804 - Frederic RIBLE
 */
 
#include <stdio.h>
#define __USE_XOPEN
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <limits.h>

static char *Version = "1.5";
static int VerboseMode = 0;
static int MaxFrameSize = 512;

#define REOPEN_TIMEOUT	30	/* try tio reopen every 10 s */

struct PortDescriptor {
	char		Name[PATH_MAX];
	int		Fd;
	unsigned char	*FrameBuffer;
	int		BufferIndex;
	time_t		TimeLastOpen;
	char		namepts[PATH_MAX];  /* name of the unix98 pts slaves, which
				       * the client has to use */
	int		is_active;
};

static struct PortDescriptor *PortList[FD_SETSIZE];

static int NbPort = 0;

static void Usage(void)
{
	fprintf(stderr, "\nUsage : kissnetd [-v] [-f size] [-p num | /dev/pty?? [/dev/pty??]* ]\n");
	fprintf(stderr, " -v       : Verbose mode, trace on stdout\n");
	fprintf(stderr, " -f size  : Set max frame size to size bytes (default 512)\n");
	fprintf(stderr, " -p num   : Number of /dev/ptmx-master-devices has to open\n");
	exit(1);
} 

static void Banner(int Small)
{
	if (Small) {
		printf("kissnetd V %s by Frederic RIBLE F1OAT - ATEPRA FPAC/Linux Project\n", Version);
	}
	else {
		printf("****************************************\n");
		printf("* Network broadcast between kiss ports *\n");
		printf("*      ATEPRA FPAC/Linux Project       *\n");
		printf("****************************************\n");
		printf("*         kissnetd Version %-4s        *\n", Version); 
		printf("*        by Frederic RIBLE F1OAT       *\n");
		printf("****************************************\n");
	}
}

static void NewPort(char *Name)
{
	struct PortDescriptor *MyPort;
	
	if (VerboseMode) {
		printf("Opening port %s\n", Name);
	}
	
	if (NbPort == FD_SETSIZE) {
		fprintf(stderr, "Cannot handle %s : too many ports\n", Name);
		exit(1);
	}
	
	MyPort = calloc(sizeof(struct PortDescriptor), 1);
	if (MyPort) MyPort->FrameBuffer = calloc(sizeof (unsigned char), MaxFrameSize);
	if (!MyPort || !MyPort->FrameBuffer) {
		perror("cannot allocate port descriptor");
		exit(1);
	}
	
	strncpy(MyPort->Name, Name, PATH_MAX-1);
	MyPort->Name[PATH_MAX-1] = '\0';
	MyPort->Fd = -1;
	MyPort->FrameBuffer[0] = 0xC0;
	MyPort->BufferIndex = 1;
	MyPort->namepts [0] = '\0';
	MyPort->is_active = 0;
	PortList[NbPort++] = MyPort;
}

static void ReopenPort(int PortNumber)
{
	char MyString[80];
	PortList[PortNumber]->TimeLastOpen = time(NULL);
		
	if (VerboseMode) {
		printf("Reopening port %d\n", PortNumber);
	}
	
	if (PortList[PortNumber]->namepts[0] == '\0') {
		
		syslog(LOG_WARNING, "kissnetd : Opening port %s\n", PortList[PortNumber]->Name);
		PortList[PortNumber]->Fd = open(PortList[PortNumber]->Name, O_RDWR | O_NONBLOCK);
		if (PortList[PortNumber]->Fd < 0) {
			syslog(LOG_WARNING, "kissnetd : Error opening port %s : %s\n", 
				PortList[PortNumber]->Name, strerror(errno));
			if (VerboseMode) {
				sprintf(MyString, "cannot reopen %s", PortList[PortNumber]->Name);
				perror(MyString);
			}
			return;
		}
		PortList[PortNumber]->is_active = 1;
		if (!strcmp(PortList[PortNumber]->Name, "/dev/ptmx")) {
			char *npts;
			/* get name of pts-device */
			if ((npts = ptsname(PortList[PortNumber]->Fd)) == NULL) {
				sprintf(MyString, "Cannot get name of pts-device.\n");
				syslog(LOG_WARNING, "kissnetd : Cannot get name of pts-device\n"); 
				exit(1);
			}
			strncpy(PortList[PortNumber]->namepts, npts, PATH_MAX-1);
			PortList[PortNumber]->namepts[PATH_MAX-1] = '\0';

			/* unlock pts-device */
			if (unlockpt(PortList[PortNumber]->Fd) == -1) {
				sprintf(MyString, "Cannot unlock pts-device %s\n", PortList[PortNumber]->namepts);
				syslog(LOG_WARNING, "kissnetd : Cannot unlock pts-device %s\n", PortList[PortNumber]->namepts);
				exit(1);
			}
			syslog(LOG_WARNING, "kissnetd : Using /dev/ptmx with slave pty %s\n", PortList[PortNumber]->namepts);
		}
	} else {
		if (PortList[PortNumber]->Fd == -1) {
			syslog(LOG_WARNING, "kissnetd : Cannot reopen port ptmx (slave %s) : not supported by ptmx-device\n", 
		       	PortList[PortNumber]->namepts);
			if (VerboseMode) {
				sprintf(MyString, "cannot reopen ptmx (slave %s).", PortList[PortNumber]->namepts);
				perror(MyString);
			}
			return;
		}
		syslog(LOG_WARNING, "kissnetd : Trying to poll port ptmx (slave %s).\n", 
		       	PortList[PortNumber]->namepts);
		PortList[PortNumber]->is_active = 1;
	}
}

static void TickReopen(void)
{
	int i;
	static int wrote_info = 0;
	time_t CurrentTime = time(NULL);
	
	for (i=0; i<NbPort; i++) {
		if (PortList[i]->Fd >= 0 &&  PortList[i]->is_active == 1) continue;
		if ( (CurrentTime - PortList[i]->TimeLastOpen) > REOPEN_TIMEOUT ) ReopenPort(i);
	}
	
	if (!wrote_info) {
		for (i=0; i<NbPort; i++) {
			if (PortList[i]->namepts[0] != '\0') {
				if (wrote_info == 0)
					printf("\nAwaiting client connects on:\n");
				else
					printf(" ");
				printf("%s", PortList[i]->namepts);
				wrote_info = 1;
			}
		}
		if (wrote_info > 0) {
			printf("\n");
			if (!VerboseMode) {
				fflush(stdout);
				fflush(stderr);
				close(0);
				close(1);
				close(2);
			}
		}
	}
}

static void Broadcast(int InputPort)
{
	int i;
	int rc;
	
	/* Broadcast only info frames */
	
	if (PortList[InputPort]->FrameBuffer[1] != 0x00 && \
	    PortList[InputPort]->FrameBuffer[1] != 0x20 && \
	    PortList[InputPort]->FrameBuffer[1] != 0x80)
		return;
	
	for (i=0; i<NbPort; i++) {
		int offset = 0;
		if (i == InputPort) continue;
		if (PortList[i]->Fd < 0 || PortList[i]->is_active == 0) continue;
again:
		rc = write(PortList[i]->Fd, 
			   PortList[InputPort]->FrameBuffer+offset, 
			   PortList[InputPort]->BufferIndex-offset);
		if (rc < 0) {
			if (errno == EAGAIN) {
				if (PortList[i]->namepts[0] == '\0')
					syslog(LOG_WARNING, "kissnetd : write buffer full on port %s. dropping frame. %s", 
						PortList[i]->Name, strerror(errno));
				else
					syslog(LOG_WARNING, "kissnetd : write buffer full on ptmx port %s. dropping frame. %s", 
						PortList[i]->namepts, strerror(errno));
				continue;
			}
			if (PortList[i]->namepts[0] == '\0')
				syslog(LOG_WARNING, "kissnetd : Error writing to port %s : %s\n", 
					PortList[i]->Name, strerror(errno));
			else
				syslog(LOG_WARNING, "kissnetd : Error writing to port ptmx (slave %s) : %s\n", 
					PortList[i]->namepts, strerror(errno));
			if (VerboseMode) perror("write");
			PortList[i]->is_active = 0;
			if (PortList[i]->namepts[0] == '\0') {
				close(PortList[i]->Fd);
				PortList[i]->Fd = -1;
			}
			continue;
		}
		if (VerboseMode) {
			printf("Sending %d bytes on port %d : rc=%d\n",
				PortList[InputPort]->BufferIndex,
				i, rc);
		}	   
		if (rc < PortList[InputPort]->BufferIndex-offset) {
			offset += rc;
			goto again;
		}
	}
}

static void ProcessInput(int PortNumber)
{
	static unsigned char MyBuffer[2048];
	int Length;
	int i;
	struct PortDescriptor *MyPort = PortList[PortNumber];
	
	Length = read(MyPort->Fd, MyBuffer, sizeof(MyBuffer));
	if (VerboseMode) {
		printf("Read port %d : rc=%d\n", PortNumber, Length);
	}
	if (!Length) return;
	if (Length < 0) {
		if (errno == EAGAIN)
			return;
		if (MyPort->namepts[0] == '\0')
			syslog(LOG_WARNING, "kissnetd : Error reading from port %s : %s\n", 
				PortList[PortNumber]->Name, strerror(errno));
		else
			syslog(LOG_WARNING, "kissnetd : Error reading from port ptmx (slave %s) : %s\n", 
				PortList[PortNumber]->namepts, strerror(errno));
		if (VerboseMode) perror("read");
		MyPort->is_active = 0;
		if (MyPort->namepts[0] == '\0') {
			close(MyPort->Fd);
			MyPort->Fd = -1;
		}
		return;
	}
	for (i=0; i<Length; i++) {
		if (MyPort->BufferIndex == MaxFrameSize) {
			if (MyBuffer[i] == 0xC0) {
				if (VerboseMode) printf("Drop frame too long\n");
				MyPort->BufferIndex = 1;
			}
		}
		else {		
			MyPort->FrameBuffer[MyPort->BufferIndex++] = MyBuffer[i];
			if (MyBuffer[i] == 0xC0) {
				Broadcast(PortNumber);
				MyPort->BufferIndex = 1; 
			}
		}
	}
}

static void ProcessPortList(void)
{
	static fd_set MyFdSet;
	int i, rc;
	struct timeval Timeout;
	
	Timeout.tv_sec = 1;
	Timeout.tv_usec = 0;
	
	FD_ZERO(&MyFdSet);
	for (i=0; i<NbPort; i++) {
		if (PortList[i]->Fd >= 0 && PortList[i]->is_active) FD_SET(PortList[i]->Fd, &MyFdSet);
	}
	rc = select(FD_SETSIZE, &MyFdSet, NULL, NULL, &Timeout);
	
	if (VerboseMode) printf("select : rc=%d\n", rc);
	if (!rc ) TickReopen();
	
	if (rc > 0) {
		for (i=0; i<NbPort && rc; i++) {
			if (PortList[i]->Fd < 0) continue;
			if (FD_ISSET(PortList[i]->Fd, &MyFdSet)) {
				ProcessInput(i);
				rc--;
			}
		}
	}	
}

static void ProcessArgv(int argc, char *argv[])
{
	int opt;
	int i=0;
	int ptmxdevices = 0;
	
	while ((opt = getopt(argc, argv, "vf:p:")) != -1) {
		switch (opt) {
		case 'v':
			VerboseMode = 1;
			break;
		case 'f':
			MaxFrameSize = atoi(argv[optind]);
			break;
		case 'p':
			ptmxdevices = atoi(optarg);
			if (ptmxdevices < 1) {
				fprintf(stderr, "error: too many devices\n");
				exit(1);
			}
			for (i=0; i < ptmxdevices; i++)
				NewPort("/dev/ptmx");
			break;
		default:
			fprintf(stderr, "Invalid option %s\n", argv[optind]);
			Usage();
			exit(1);
		}
	}
		
	while (optind < argc)
		NewPort(argv[optind++]);

	if (NbPort < 2) {
		fprintf(stderr, "This multiplexer needs at least two pty's\n");
		exit(1);
	}
}


int main(int argc, char *argv[]) 
{
	if (argc < 2) {
		Banner(0);
		Usage();
	}
	else {
		Banner(1);
	}
	
	ProcessArgv(argc, argv);
	while (1) ProcessPortList();
	return 0;	
}
