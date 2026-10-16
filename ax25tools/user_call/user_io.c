#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

#include "config.h"
#include "user_io.h"

#define BUFLEN	8192

int compression;
int paclen_in		= 256;
int paclen_out		= 256;
/* -T: seconds allowed to connect(), 0 leaves it to the protocol timers */
int connect_seconds;

/* This is for select_loop() */
static unsigned char buf[BUFLEN];

#ifdef HAVE_ZLIB_H
#include <zlib.h>

/* Error in (de)compression happened? */
static int compression_error;

/* These are for the (de)compressor */
static unsigned char input_buffer[BUFLEN];
static unsigned char output_buffer[BUFLEN];

static z_stream incoming_stream;
static z_stream outgoing_stream;
#endif

#define min(a,b)	((a) < (b) ? (a) : (b))

void err(char *message)
{
	user_write(STDOUT_FILENO, message, strlen(message));
	exit(1);
}

void init_compress(void)
{
#ifdef HAVE_ZLIB_H
	inflateInit(&incoming_stream);
	deflateInit(&outgoing_stream, 9);

	incoming_stream.next_in = input_buffer;
#else
	err("*** Compression support not available!!!\r\n");
#endif
}

void end_compress(void)
{
#ifdef HAVE_ZLIB_H
	inflateEnd(&incoming_stream);
	deflateEnd(&outgoing_stream);
#endif
}

static const char *timeout_message;
static size_t timeout_message_len;

/*
 * The timeout ends the connection attempt itself. Whether the interrupted
 * connect() comes back with EINTR or is restarted depends on the platform
 * and on what serves the socket: Linux restarts an interrupted connect()
 * when SA_RESTART is set, and the userspace AX.25 daemon wrappers (WAMPES,
 * AGWPE) retry a restarted wait on their own. So instead of waiting for
 * EINTR, the handler writes the message and leaves: _exit() ends the
 * process no matter what the interrupted call would have done.
 * (Thomas Osterried DL9SAU, unix-ax25-stack/ax25-tools fed61b8.)
 */
static void alarm_handler(int sig)
{
	ssize_t n;

	(void) sig;
	n = write(STDOUT_FILENO, timeout_message, timeout_message_len);
	(void) n;
	_exit(1);
}

/*
 * Arms the timeout of connect(), if -T was given. The handler is installed
 * without SA_RESTART: with signal(), glibc restarts the interrupted
 * connect(), which on an AX.25, NET/ROM or ROSE socket starts a new
 * connection attempt. The message is written from the signal handler, so
 * it is put in place before the alarm is armed; its length is taken here
 * because strlen() is not signal safe. With no -T, the timers of the
 * protocol decide alone.
 */
void connect_timeout_handler_init(unsigned int seconds, const char *message)
{
	struct sigaction sa;

	if (seconds == 0)
		return;

	timeout_message = message;
	timeout_message_len = strlen(message);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = alarm_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);

	alarm(seconds);
}

/*
 * Both ends are non blocking (see select_loop()). When the far side is slow
 * - a 1200 baud radio link fed by a BBS listing - the send buffer of the
 * socket fills and write() fails with EAGAIN: wait until there is room and
 * try again, instead of dropping the data. A stream (TCP, pipe, pty) may
 * also take only part of the buffer: write the rest.
 */
static int write_frame(int fd, const unsigned char *byte, int len)
{
	struct pollfd pfd;
	int written = 0;
	int n;

	while (written < len) {
		n = write(fd, byte + written, len - written);
		if (n >= 0) {
			written += n;
			continue;
		}
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			return -1;

		pfd.fd = fd;
		pfd.events = POLLOUT;
		pfd.revents = 0;
		if (poll(&pfd, 1, -1) < 0 && errno != EINTR)
			return -1;
	}

	return 0;
}

static int flush_output(int fd, const void *buf, size_t count)
{
	const unsigned char *byte = buf;
	int cnt = count;
	/* -i is the frame length of the incoming connection (our stdout),
	 * -o the one of the connection we made */
	int paclen = (fd == STDOUT_FILENO) ? paclen_in : paclen_out;

	while (cnt > 0) {
		if (write_frame(fd, byte, min(paclen, cnt)) < 0)
			return -1;
		byte += paclen;
		cnt -= paclen;
	}

	return count;
}

int user_write(int fd, const void *buf, size_t count)
{
#ifdef HAVE_ZLIB_H
	int status;
#endif

	if (count == 0)
		return 0;

#ifndef HAVE_ZLIB_H
	return flush_output(fd, buf, count);
#else
	/* Only output to stdout can be compressed */
	if (fd != STDOUT_FILENO || !compression)
		return flush_output(fd, buf, count);

	if (compression_error) {
		errno = 0;
		return -1;
	}

	/* Input is the contents of the input buffer. */
	outgoing_stream.next_in = (unsigned char *)buf;
	outgoing_stream.avail_in = count;

	/* Loop compressing until deflate() returns with avail_out != 0. */
	do {
		/* Set up fixed-size output buffer. */
		outgoing_stream.next_out = output_buffer;
		outgoing_stream.avail_out = BUFLEN;

		/* Compress as much data into the buffer as possible. */
		status = deflate(&outgoing_stream, Z_PARTIAL_FLUSH);

		if (status != Z_OK) {
			compression_error = status;
			errno = 0;
			return -1;
		}

		/* Now send the compressed data */
		if (flush_output(fd, output_buffer, BUFLEN - outgoing_stream.avail_out) < 0)
			return -1;
	} while (outgoing_stream.avail_out == 0);

	return count;
#endif
}

int user_read(int fd, void *buf, size_t count)
{
#ifdef HAVE_ZLIB_H
	int status, len;
#endif

	if (count == 0)
		return 0;

#ifndef HAVE_ZLIB_H
	return read(fd, buf, count);
#else
	/* Only input from stdin can be compressed */
	if (fd != STDIN_FILENO || !compression)
		return read(fd, buf, count);

	if (compression_error) {
		errno = 0;
		return -1;
	}

	incoming_stream.next_out = buf;
	incoming_stream.avail_out = count;

	for (;;) {
		status = inflate(&incoming_stream, Z_SYNC_FLUSH);

		if (count - incoming_stream.avail_out > 0)
			return count - incoming_stream.avail_out;

		if (status != Z_OK && status != Z_BUF_ERROR) {
			compression_error = status;
			errno = 0;
			return -1;
		}

		incoming_stream.next_in = input_buffer;
		incoming_stream.avail_in = 0;

		len = read(fd, input_buffer, BUFLEN);
		if (len <= 0)
			return len;

		incoming_stream.avail_in = len;
	}

	return 0;
#endif
}

int select_loop(int s)
{
	fd_set read_fd;
	int n;

	if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
		close(s);
		return -1;
	}
	if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) < 0) {
		close(s);
		return -1;
	}

	/*
	 * Loop until one end of the connection goes away.
	 */
	for (;;) {
		FD_ZERO(&read_fd);
		FD_SET(STDIN_FILENO, &read_fd);
		FD_SET(s, &read_fd);

		if (select(s + 1, &read_fd, NULL, NULL, NULL) < 0) {
			if (errno == EINTR)
				continue;
			close(s);
			break;
		}

		if (FD_ISSET(s, &read_fd)) {
			while ((n = user_read(s, buf, BUFLEN)) > 0) {
				if (user_write(STDOUT_FILENO, buf, n) < 0) {
					close(s);
					return -1;
				}
			}
			if (n == 0 || (n < 0 && errno != EAGAIN)) {
				close(s);
				break;
			}
		}

		if (FD_ISSET(STDIN_FILENO, &read_fd)) {
			while ((n = user_read(STDIN_FILENO, buf, BUFLEN)) > 0) {
				if (user_write(s, buf, n) < 0) {
					close(s);
					return -1;
				}
			}
			if (n == 0 || (n < 0 && errno != EAGAIN)) {
				close(s);
				break;
			}
		}
	}

	return 0;
}
