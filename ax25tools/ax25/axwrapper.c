/*
 * axwrapper.c - Convert end-of-line sequences for non-AX.25 aware programs -
 * version 1.1
 *
 * Copyright (C) 1996-2001 Tomi Manninen, OH2BNS, <tomi.manninen@hut.fi>.
 *
 * Compile with: gcc -Wall -O6 -o axwrapper axwrapper.c
 *
 * Usage: axwrapper [-p <paclen>] <filename> <argv[0]> ...
 *
 * Axwrapper first creates a pipe and then forks and execs the program
 * <filename> with arguments given at the axwrapper command line.
 * The parent process then sits and waits for any I/O to and from the
 * user and converts any <CR>'s from user to <NL>'s and any <NL>'s from
 * the program to <CR>'s. This is useful when starting non-AX.25 aware
 * programs from ax25d.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#define FLUSHTIMEOUT	500000		/* 0.5 sec */

#define	PERROR(s)	fprintf(stderr, "*** %s: %s\r", (s), strerror(errno))
#define	USAGE()		fputs("Usage: axwrapper [-p <paclen>] <filename> <argv[0]> ...\r", stderr)

static void convert_cr_lf(unsigned char *buf, int len)
{
	while (len-- > 0) {
		if (*buf == '\r') *buf = '\n';
		buf++;
	}
}

static void convert_lf_cr(unsigned char *buf, int len)
{
	while (len-- > 0) {
		if (*buf == '\n') *buf = '\r';
		buf++;
	}
}

int main(int argc, char **argv)
{
	unsigned char buf[4096];
	char *stdoutbuf;
	int pipe_in[2];
	int pipe_out[2];
	int pipe_err[2];
	int len;
	int pid;
	int paclen = 256;
	fd_set fdset;
	struct timeval tv;

	while ((len = getopt(argc, argv, "p:")) != -1) {
		switch (len) {
		case 'p':
			paclen = atoi(optarg);
			break;
		case ':':
		case '?':
			USAGE();
			exit(1);
		}
	}

	if (argc - optind < 2) {
		USAGE();
		exit(1);
	}

	stdoutbuf = malloc(paclen);
	if (stdoutbuf == NULL) {
		PERROR("axwrapper: malloc");
		exit(1);
	}

	setvbuf(stdout, stdoutbuf, _IOFBF, paclen);

	if (pipe(pipe_in) == -1) {
		PERROR("axwrapper: pipe");
		exit(1);
	}
	if (pipe(pipe_out) == -1) {
		PERROR("axwrapper: pipe");
		exit(1);
	}
	if (pipe(pipe_err) == -1) {
		PERROR("axwrapper: pipe");
		exit(1);
	}

	signal(SIGCHLD, SIG_IGN);

	pid = fork();

	if (pid == -1) {
		/* fork error */
		PERROR("axwrapper: fork");
		exit(1);
	}

	if (pid == 0) {
		/* child */
		dup2(pipe_in[0], STDIN_FILENO);
		close(pipe_in[1]);

		dup2(pipe_out[1], STDOUT_FILENO);
		close(pipe_out[0]);

		dup2(pipe_err[1], STDERR_FILENO);
		close(pipe_err[0]);

		execve(argv[optind], argv + optind + 1, NULL);

		/* execve() should not return */
		perror("axwrapper: execve");
		exit(1);
	}

	/* parent */

	close(pipe_in[0]);
	close(pipe_out[1]);
	close(pipe_err[1]);

	if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1) {
		perror("axwrapper: fcntl");
		exit(1);
	}
	if (fcntl(pipe_out[0], F_SETFL, O_NONBLOCK) == -1) {
		perror("axwrapper: fcntl");
		exit(1);
	}
	if (fcntl(pipe_err[0], F_SETFL, O_NONBLOCK) == -1) {
		perror("axwrapper: fcntl");
		exit(1);
	}

	while (1) {
		FD_ZERO(&fdset);
		int maxfd = -1;

		FD_SET(STDIN_FILENO, &fdset);
		if (STDIN_FILENO > maxfd)
			maxfd = STDIN_FILENO + 1;
		FD_SET(pipe_out[0], &fdset);
		if (pipe_out[0] > maxfd)
			maxfd = pipe_out[0] + 1;
		FD_SET(pipe_err[0], &fdset);
		if (pipe_err[0] > maxfd)
			maxfd = pipe_err[0] + 1;

		tv.tv_sec = 0;
		tv.tv_usec = FLUSHTIMEOUT;

		len = select(maxfd, &fdset, NULL, NULL, &tv);
		if (len == -1) {
			if (errno == EINTR)
				continue;

			perror("axwrapper: select");
			exit(1);
		}
		if (len == 0) {
			fflush(stdout);
		}

		if (FD_ISSET(STDIN_FILENO, &fdset)) {
			len = read(STDIN_FILENO, buf, sizeof(buf));
			if (len < 0 && errno != EAGAIN) {
				perror("axwrapper: read");
				break;
			}
			if (len == 0)
				break;
			convert_cr_lf(buf, len);
			write(pipe_in[1], buf, len);
		}
		if (FD_ISSET(pipe_out[0], &fdset)) {
			len = read(pipe_out[0], buf, sizeof(buf));
			if (len < 0 && errno != EAGAIN) {
				perror("axwrapper: read");
				break;
			}
			if (len == 0)
				break;
			convert_lf_cr(buf, len);
			fwrite(buf, 1, len, stdout);
		}
		if (FD_ISSET(pipe_err[0], &fdset)) {
			len = read(pipe_err[0], buf, sizeof(buf));
			if (len < 0 && errno != EAGAIN) {
				perror("axwrapper: read");
				break;
			}
			if (len == 0)
				break;
			convert_lf_cr(buf, len);
			fwrite(buf, 1, len, stderr);
		}
	}

	kill(pid, SIGTERM);
	close(pipe_in[1]);
	close(pipe_out[0]);
	close(pipe_err[0]);
	return 0;
}
