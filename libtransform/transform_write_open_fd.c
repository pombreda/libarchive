/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "transform_platform.h"
__FBSDID("$FreeBSD: head/lib/libtransform/transform_write_open_fd.c 201093 2009-12-28 02:28:44Z kientzle $");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "transform.h"
#include "transform_write_private.h"

struct write_fd_data {
	int		fd;
};

static int	file_close(struct transform *, void *);
static int	file_open(struct transform *, void *);
static ssize_t	file_write(struct transform *, void *, const void *buff, size_t);
static int      file_visit_fds(struct transform_write_filter *t, int position,
    transform_fd_visitor *visitor, const void *visitor_data);

int
transform_write_open_fd(struct transform *a, int fd)
{
	struct write_fd_data *mine;

	mine = (struct write_fd_data *)malloc(sizeof(*mine));
	if (mine == NULL) {
		transform_set_error(a, ENOMEM, "No memory");
		return (TRANSFORM_FATAL);
	}
	mine->fd = fd;
#if defined(__CYGWIN__) || defined(_WIN32)
	setmode(mine->fd, O_BINARY);
#endif
	return (transform_write_open2(a, mine,
		    file_open, file_write, file_close, file_visit_fds));
}

static int
file_open(struct transform *t, void *client_data)
{
	struct write_fd_data *mine;
	struct stat st;

	mine = (struct write_fd_data *)client_data;

	if (fstat(mine->fd, &st) != 0) {
		transform_set_error(t, errno, "Couldn't stat fd %d", mine->fd);
		return (TRANSFORM_FATAL);
	}

	/*
	 * If client hasn't explicitly set the last block handling,
	 * then set it here.
	 */
	if (transform_write_get_bytes_in_last_block(t) < 0) {
		/* If the output is a block or character device, fifo,
		 * or stdout, pad the last block, otherwise leave it
		 * unpadded. */
		if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) ||
		    S_ISFIFO(st.st_mode) || (mine->fd == 1))
			/* Last block will be fully padded. */
			transform_write_set_bytes_in_last_block(t, 0);
		else
			transform_write_set_bytes_in_last_block(t, 1);
	}

	return (TRANSFORM_OK);
}

static ssize_t
file_write(struct transform *t, void *client_data, const void *buff, size_t length)
{
	struct write_fd_data	*mine;
	ssize_t	bytesWritten;

	mine = (struct write_fd_data *)client_data;
	for (;;) {
		bytesWritten = write(mine->fd, buff, length);
		if (bytesWritten <= 0) {
			if (errno == EINTR)
				continue;
			transform_set_error(t, errno, "Write error");
			return (-1);
		}
		return (bytesWritten);
	}
}

static int
file_close(struct transform *a, void *client_data)
{
	struct write_fd_data	*mine = (struct write_fd_data *)client_data;

	(void)a; /* UNUSED */
	free(mine);
	return (TRANSFORM_OK);
}

static int
file_visit_fds(struct transform_write_filter *f, int position,
    transform_fd_visitor *visitor, const void *visitor_data)
{
	struct write_fd_data *mine = (struct write_fd_data *)f->data;
	return visitor((struct transform *)f->transform,
		position, mine->fd, (void *)visitor_data);
}
