/* yapp.c
 *
 * Copyright (C) 1994 by Jonathan Naylor
 *
 * This module implements the YAPP file transfer protocol as defined by Jeff
 * Jacobsen WA7MBL in the files yappxfer.doc and yappxfer.pas.
 *
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the license, or (at your option) any later version.
 */

/*
 * Yapp C and Resume support added by S N Henson.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <linux/ax25.h>
#include <sys/stat.h>

#include "call.h"

#define	TIMEOUT		300	/* 5 Minutes */

#define	NUL		0x00
#define	SOH		0x01
#define	STX		0x02
#define	ETX		0x03
#define	EOT		0x04
#define	ENQ		0x05
#define	ACK		0x06
#define	DLE		0x10
#define	NAK		0x15
#define	CAN		0x18

#define	STATE_S		0
#define	STATE_SH	1
#define	STATE_SD	2
#define	STATE_SE	3
#define	STATE_ST	4
#define	STATE_R		5
#define	STATE_RH	6
#define	STATE_RD	7

static int state;
static int total = 0;

static int readlen = 0;
static int outlen = 0;
static int outbufptr = 0;
static unsigned char outbuffer[512];
static char yappc;		/* Nonzero if using YAPP C */

static void Write_Status(char *s)
{
	fprintf(stdout, "State: %-60s\r", s);
	fflush(stdout);
}

static void Send_RR(void)
{
	char buffer[2];

	buffer[0] = ACK;
	buffer[1] = 0x01;

	write(fd, buffer, 2);
}

static void Send_RF(void)
{
	char buffer[2];

	buffer[0] = ACK;
	buffer[1] = 0x02;

	write(fd, buffer, 2);
}

static void Send_RT(void)
{
	char buffer[2];

	buffer[0] = ACK;
	buffer[1] = ACK;

	write(fd, buffer, 2);
}

static void Send_AF(void)
{
	char buffer[2];

	buffer[0] = ACK;
	buffer[1] = 0x03;

	write(fd, buffer, 2);
}

static void Send_AT(void)
{
	char buffer[2];

	buffer[0] = ACK;
	buffer[1] = 0x04;

	write(fd, buffer, 2);
}

static void Send_NR(char *reason)
{
	char buffer[257];
	int length;

	if ((length = strlen(reason)) > 255)
		length = 255;

	buffer[0] = NAK;
	buffer[1] = length;
	memcpy(buffer + 2, reason, length);

	write(fd, buffer, length + 2);
}

/* Send a Resume Sequence */

static void Send_RS(int length)
{
	char buffer[256];
	int len;

	buffer[0] = NAK;
	buffer[2] = 'R';
	buffer[3] = 0;

	len = sprintf(buffer + 4, "%d", length) + 5;

	buffer[len] = 'C';
	buffer[len + 1] = 0;
	buffer[1] = len;

	write(fd, buffer, len + 2);
}

static void Send_SI(void)
{
	char buffer[2];

	buffer[0] = ENQ;
	buffer[1] = 0x01;

	write(fd, buffer, 2);
}

static void Send_CN(char *reason)
{
	char buffer[257];
	int length;

	if ((length = strlen(reason)) > 255)
		length = 255;

	buffer[0] = CAN;
	buffer[1] = length;
	memcpy(buffer + 2, reason, length);

	write(fd, buffer, length + 2);
}

static void Send_HD(char *filename, long length)
{
	char buffer[257];
	char size_buffer[10];
	int len_filename;
	int len_size;
	int len;
	struct stat sb;

	sprintf(size_buffer, "%ld", length);

	len_filename = strlen(filename) + 1;	/* Include the NUL */
	len_size = strlen(size_buffer) + 1;	/* Include the NUL */

	len = len_filename + len_size;

	if (!stat(filename, &sb)) {
		unix2yapp(sb.st_mtime, buffer + len + 2);
		len += 9;
	}

	buffer[0] = SOH;
	buffer[1] = len;

	memcpy(buffer + 2, filename, len_filename);
	memcpy(buffer + len_filename + 2, size_buffer, len_size);

	write(fd, buffer, len + 2);
}

static void Send_ET(void)
{
	char buffer[2];

	buffer[0] = EOT;
	buffer[1] = 0x01;

	write(fd, buffer, 2);
}

static void Send_DT(int length)
{
	char buffer[2];

	if (length > 255)
		length = 0;

	buffer[0] = STX;
	buffer[1] = length;

	write(fd, buffer, 2);
}

static void Send_EF(void)
{
	char buffer[2];

	buffer[0] = ETX;
	buffer[1] = 0x01;

	write(fd, buffer, 2);
}

static unsigned char checksum(unsigned char *buf, int len)
{
	int i;
	unsigned char sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];

	return sum;
}

static int yapp_download_data(int *filefd, unsigned char *buffer)
{
	int length, file_time;
	char Message[50];

	if (buffer[0] == CAN || buffer[0] == NAK) {
		Write_Status("RcdABORT");
		return (FALSE);
	}

	switch (state) {
	case STATE_R:
		if (buffer[0] == ENQ && buffer[1] == 0x01) {
			Send_RR();
			Write_Status("RcvHeader");
			state = STATE_RH;
			break;
		}

		Send_CN("Unknown code");
		Write_Status("SndABORT");
		return (FALSE);

	case STATE_RH:
		if (buffer[0] == SOH) {
			/* Parse header: 3 fields == YAPP C */
			char *hptr, *hfield[3];
			if ((length = buffer[1]) == 0)
				length = 256;
			hptr = buffer + 2;
			while (length > 0) {
				int hlen;
				hlen = strlen(hptr) + 1;
				hfield[(int) yappc++] = hptr;
				hptr += hlen;
				length -= hlen;
			}

			if (yappc < 3) {
				yappc = 0;
			} else {
				file_time = yapp2unix(hfield[2]);
				yappc = 1;
			}

			if (*filefd == -1) {
				if ((*filefd =
				     open(hfield[0],
					  O_RDWR | O_APPEND | O_CREAT,
					  0666)) == -1) {
					printf("\n[Unable to open %s]\n",
					       hfield[0]);
					Send_NR("Invalid filename");
					return (FALSE);
				}
			}

			printf("Receiving %s %s %s", hfield[0], hfield[1],
			       yappc ? ctime((time_t *) & file_time) :
			       " ");

			if (yappc) {
				struct stat sb;

				if (!fstat(*filefd, &sb) && sb.st_size)
					Send_RS(sb.st_size);
				else
					Send_RT();
			} else {
				Send_RF();
			}

			state = STATE_RD;
			break;
		}

		if (buffer[0] == ENQ && buffer[1] == 0x01) {
			break;
		}

		if (buffer[0] == EOT && buffer[1] == 0x01) {
			Send_AT();
			Write_Status("RcvEOT");
			return (FALSE);
		}

		Send_CN("Unknown code");
		Write_Status("SndABORT");
		return (FALSE);

	case STATE_RD:
		if (buffer[0] == STX) {
			if ((length = buffer[1]) == 0)
				length = 256;
			total += length;
			sprintf(Message, "RcvData  %5d bytes received",
				total);
			Write_Status(Message);

			if (yappc) {
				int i;
				unsigned char checksum = 0;

				for (i = 0; i < length; i++)
					checksum += buffer[i + 2];

				if (checksum != buffer[length + 2]) {
					Send_CN("Bad Checksum");
					Write_Status
					    ("SndABORT: Bad Checksum");
					return (FALSE);
				}
			}

			write(*filefd, buffer + 2, length);
			break;
		}

		if (buffer[0] == ETX && buffer[1] == 0x01) {
			Send_AF();
			Write_Status("RcvEof");
			state = STATE_RH;
			close(*filefd);
			*filefd = -1;
			break;
		}

		Send_CN("Unknown code");
		Write_Status("SndABORT");
		return (FALSE);
	}

	return (TRUE);
}

static void yapp_download(int filefd)
{
	struct timeval timeval;
	fd_set sock_read;
	int n;
	int buflen = 0;
	int length;
	int used;
	unsigned char buffer[1024];

	Write_Status("RcvWait");

	state = STATE_R;
	total = 0;
	yappc = 0;

	while (TRUE) {
		FD_ZERO(&sock_read);
		FD_SET(STDIN_FILENO, &sock_read);
		FD_SET(fd, &sock_read);

		timeval.tv_usec = 0;
		timeval.tv_sec = TIMEOUT;

		n = select(fd + 1, &sock_read, NULL, NULL, &timeval);

		if (n == -1) {
			if (!interrupted && errno == EAGAIN)
				continue;
			if (!interrupted)
				perror("select");
			Send_CN("Internal error");
			Write_Status("SndABORT");
			return;
		}

		if (n == 0) {	/* Timeout */
			Send_CN("Timeout");
			Write_Status("SndABORT");
			return;
		}

		if (FD_ISSET(STDIN_FILENO, &sock_read)) {
			Send_CN("Cancelled by user");
			Write_Status("SndABORT");
			return;
		}

		if (FD_ISSET(fd, &sock_read)) {
			if ((length = read(fd, buffer + buflen, 511)) > 0) {
				buflen += length;

				do {
					used = FALSE;

					switch (buffer[0]) {
					case ACK:
					case ENQ:
					case ETX:
					case EOT:
						if (buflen >= 2) {
							if (!yapp_download_data(&filefd, buffer))
								return;
							buflen -= 2;
							memcpy(buffer,
							       buffer + 2,
							       buflen);
							used = TRUE;
						}
						break;
					default:
						if ((length =
						     buffer[1]) == 0)
							length = 256;
						if (buffer[0] == STX)
							length += yappc;
						if (buflen >= (length + 2)) {
							if (!yapp_download_data(&filefd, buffer))
								return;
							buflen -=
							    length + 2;
							memcpy(buffer,
							       buffer +
							       length + 2,
							       buflen);
							used = TRUE;
						}
						break;
					}
				}
				while (used);
			}
		}
	}
}

static int yapp_upload_data(int filefd, char *filename, int filelength,
			    unsigned char *buffer)
{
	char Message[80];

	if (buffer[0] == CAN || buffer[0] == NAK) {
		Write_Status("RcvABORT");
		return (FALSE);
	}

	switch (state) {
	case STATE_S:
		if (buffer[0] == ACK && buffer[1] == 0x01) {
			Write_Status("SendHeader");
			Send_HD(filename, filelength);
			state = STATE_SH;
			break;
		}

		if (buffer[0] == ACK && buffer[1] == 0x02) {
			sprintf(Message, "SendData  %5d bytes transmitted",
				total);
			Write_Status(Message);
			outlen = read(filefd, outbuffer, readlen);
			outbufptr = 0;

			if (outlen)
				Send_DT(outlen);

			if (yappc) {
				outbuffer[outlen] =
				    checksum(outbuffer, outlen);
				outlen++;
			}

			state = STATE_SD;
			break;
		}

		Send_CN("Unknown code");
		Write_Status("SndABORT");
		return (FALSE);

	case STATE_SH:
		/* Could get three replies here:
		 * ACK 02 : normal acknowledge.
		 * ACK ACK: yappc acknowledge.
		 * NAK ...: resume request.
		 */
		if (buffer[0] == NAK && buffer[2] == 'R') {
			int len;
			off_t rpos;

			len = buffer[1];
			if (buffer[len] == 'C')
				yappc = 1;
			rpos = atol(buffer + 4);
			lseek(filefd, rpos, SEEK_SET);
			buffer[0] = ACK;
			buffer[1] = yappc ? ACK : 0x02;
		}

		if (buffer[0] == ACK &&
		    (buffer[1] == 0x02 || buffer[1] == ACK)) {
			if (buffer[1] == ACK)
				yappc = 1;

			sprintf(Message, "SendData  %5d bytes transmitted",
				total);
			Write_Status(Message);
			outlen = read(filefd, outbuffer, readlen);
			outbufptr = 0;
			if (outlen)
				Send_DT(outlen);
			state = STATE_SD;

			if (yappc) {
				outbuffer[outlen] =
				    checksum(outbuffer, outlen);
				outlen++;
			}
			break;
		}

		Send_CN("Unknown code");
		Write_Status("SndABORT");
		return (FALSE);

	case STATE_SD:
		Send_CN("Unknown code");
		Write_Status("SndABORT");
		return (FALSE);

	case STATE_SE:
		if (buffer[0] == ACK && buffer[1] == 0x03) {
			Write_Status("SendEOT");
			Send_ET();
			state = STATE_ST;
			break;
		}

		Send_CN("Unknown code");
		Write_Status("SndABORT");
		return (FALSE);

	case STATE_ST:
		if (buffer[0] == ACK && buffer[1] == 0x04) {
			return (FALSE);
		}

		Send_CN("Unknown code");
		Write_Status("SndABORT");
		return (FALSE);
	}

	return (TRUE);
}

static void yapp_upload(int filefd, char *filename, long filelength)
{
	struct timeval timeval;
	fd_set sock_read;
	fd_set sock_write;
	int n;
	unsigned char buffer[1024];
	int buflen = 0;
	int length;
	int used;
	char Message[80];

	Write_Status("SendInit");

	readlen = (paclen - 2 > 253) ? 253 : paclen - 2;
	state = STATE_S;
	total = 0;
	yappc = 0;

	Send_SI();

	while (TRUE) {
		FD_ZERO(&sock_read);
		FD_ZERO(&sock_write);
		FD_SET(STDIN_FILENO, &sock_read);
		FD_SET(fd, &sock_read);

		if (state == STATE_SD) {
			FD_SET(fd, &sock_write);

			n = select(fd + 1, &sock_read, &sock_write, NULL,
				   NULL);
		} else {
			timeval.tv_usec = 0;
			timeval.tv_sec = TIMEOUT;

			n = select(fd + 1, &sock_read, NULL, NULL,
				   &timeval);
		}

		if (n == -1) {
			if (!interrupted && errno == EAGAIN)
				continue;
			if (!interrupted)
				perror("select");
			Write_Status("SndABORT");
			Send_CN("Internal error");
			return;
		}

		if (n == 0) {	/* Timeout, not STATE_SD */
			Write_Status("SndABORT");
			Send_CN("Timeout");
			return;
		}

		if (FD_ISSET(STDIN_FILENO, &sock_read)) {
			Write_Status("SndABORT");
			Send_CN("Cancelled by user");
			return;
		}

		if (FD_ISSET(fd, &sock_write)) {	/* Writable, only STATE_SD */
			if (outlen > 0) {
				if ((n =
				     write(fd, outbuffer + outbufptr,
					   outlen)) > 0) {
					outbufptr += n;
					outlen -= n;
					total += n;
				}
			}

			if (outlen == 0) {
				total -= yappc;
				if ((outlen =
				     read(filefd, outbuffer,
					  readlen)) > 0) {
					sprintf(Message,
						"SendData  %5d bytes transmitted",
						total);
					Write_Status(Message);

					outbufptr = 0;
					Send_DT(outlen);

					if (yappc) {
						outbuffer[outlen] =
						    checksum(outbuffer,
							     outlen);
						outlen++;
					}
				} else {
					Write_Status("SendEof");
					state = STATE_SE;
					Send_EF();
				}
			}
		}

		if (FD_ISSET(fd, &sock_read)) {
			if ((length = read(fd, buffer + buflen, 511)) > 0) {
				buflen += length;

				do {
					used = FALSE;

					switch (buffer[0]) {
					case ACK:
					case ENQ:
					case ETX:
					case EOT:
						if (buflen >= 2) {
							if (!yapp_upload_data(filefd, filename, filelength, buffer))
								return;
							buflen -= 2;
							memcpy(buffer,
							       buffer + 2,
							       buflen);
							used = TRUE;
						}
						break;
					default:
						if ((length =
						     buffer[1]) == 0)
							length = 256;
						if (buflen >= (length + 2)) {
							if (!yapp_upload_data(filefd, filename, filelength, buffer))
								return;
							buflen -=
							    length + 2;
							memcpy(buffer,
							       buffer +
							       length + 2,
							       buflen);
							used = TRUE;
						}
						break;
					}
				}
				while (used);
			}
		}
	}
}

void cmd_yapp(char *buf, int bytes)
{
	int filefd;
	long size = 0L;
	char *t;

	if (bytes == 0)
		return;

	switch (buf[0]) {
	case 'U':
	case 'u':
		if ((t = strchr(buf, '\n')) != NULL)
			*t = '\0';
		t = buf + 2;
		while (*t != '\0' && isspace(*t))
			t++;
		if (*t == '\0') {
			printf
			    ("\n[YAPP Upload requires a filename - eg ~yu hello.txt]\n");
			Send_NR("No filename");
			return;
		}
		if ((filefd = open(t, O_RDONLY)) == -1) {
			printf("\n[Unable to open upload file]\n");
			Send_NR("Invalid filename");
			return;
		}
		if (lseek(filefd, 0L, SEEK_END) != -1)
			size = lseek(filefd, 0L, SEEK_CUR);
		lseek(filefd, 0L, SEEK_SET);
		if (size != -1)
			printf
			    ("\n[Uploading %ld bytes from %s using YAPP]\n",
			     size, t);
		else
			printf("\n[Uploading from %s using YAPP]\n", t);
		yapp_upload(filefd, t, size);
		close(filefd);
		printf("[Finished YAPP Upload, %d bytes Transmitted]\n",
		       total);
		break;

	case 'D':
	case 'd':
		if ((t = strchr(buf, '\n')) != NULL)
			*t = '\0';
		t = buf + 2;
		while (*t != '\0' && isspace(*t))
			t++;
		if (*t == '\0')
			filefd = -1;
		else if ((filefd =
			  open(t, O_RDWR | O_APPEND | O_CREAT,
			       0666)) == -1) {
			printf("\n[Unable to open %s]\n", buf + 2);
			Send_NR("Invalid filename");
			return;
		}
		printf("\n[Downloading using YAPP]\n");
		yapp_download(filefd);
		close(filefd);
		printf("[Finished YAPP Download, %d bytes received]\n",
		       total);
		break;

	default:
		printf("\nUnknown '~y' escape. Type ~h for a list.\n");
		break;
	}
}
