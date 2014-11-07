#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"
#include "user_io.h"

#define BUFLEN	8192

int compression		= 0;
int paclen_in		= 256;
int paclen_out		= 256;

/* This is for select_loop() */
static unsigned char buf[BUFLEN];

#ifdef HAVE_ZLIB_H
#include <zlib.h>

/* Error in (de)compression happened? */
static int compression_error = 0;

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

static int flush_output(int fd, const void *buf, size_t count)
{
	int cnt = count;

	while (cnt > 0) {
		if (write(fd, buf, min(paclen_out, cnt)) < 0)
			return -1;
		buf += paclen_out;
		cnt -= paclen_out;
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

		if ((len = read(fd, input_buffer, BUFLEN)) <= 0)
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
		
		select(s + 1, &read_fd, NULL, NULL, NULL);

		if (FD_ISSET(s, &read_fd)) {
			while ((n = user_read(s, buf, BUFLEN)) > 0)
				user_write(STDOUT_FILENO, buf, n);
			if (n == 0 || (n < 0 && errno != EAGAIN)) {
				close(s);
				break;
			}
		}

		if (FD_ISSET(STDIN_FILENO, &read_fd)) {
			while ((n = user_read(STDIN_FILENO, buf, BUFLEN)) > 0)
				user_write(s, buf, n);
			if (n == 0 || (n < 0 && errno != EAGAIN)) {
				close(s);
				break;
			}
		}
	}

	return 0;
}
