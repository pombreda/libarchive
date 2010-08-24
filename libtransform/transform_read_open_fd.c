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
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_open_fd.c 201103 2009-12-28 03:13:49Z kientzle $");

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
#include "transform_read_private.h"

struct read_fd_data {
	int	 fd;
	size_t	 block_size;
	char	 use_lseek;
	void	*buffer;
};

static int	file_close(struct transform *, void *);
static ssize_t	file_read(struct transform *, void *, struct transform_read_filter *,
	const void **buff);
static int64_t	file_skip(struct transform *, void *, int64_t request);
static int      file_visit_fds(struct transform *, const void *,
	transform_fd_visitor *visitor, const void *visitor_data);

int
transform_read_open_fd(struct transform *a, int fd, size_t block_size)
{
	struct stat st;
	struct read_fd_data *mine;
	void *b;

	transform_clear_error(a);
	if (fstat(fd, &st) != 0) {
		transform_set_error(a, errno, "Can't stat fd %d", fd);
		return (TRANSFORM_FATAL);
	}

	mine = (struct read_fd_data *)calloc(1, sizeof(*mine));
	b = malloc(block_size);
	if (mine == NULL || b == NULL) {
		transform_set_error(a, ENOMEM, "No memory");
		free(mine);
		free(b);
		return (TRANSFORM_FATAL);
	}
	mine->block_size = block_size;
	mine->buffer = b;
	mine->fd = fd;
	/*
	 * Skip support is a performance optimization for anything
	 * that supports lseek().  On FreeBSD, only regular files and
	 * raw disk devices support lseek() and there's no portable
	 * way to determine if a device is a raw disk device, so we
	 * only enable this optimization for regular files.
	 */
	if (S_ISREG(st.st_mode)) {
		mine->use_lseek = 1;
	}
#if defined(__CYGWIN__) || defined(_WIN32)
	setmode(mine->fd, O_BINARY);
#endif

	return (transform_read_open2(a, mine,
		NULL, file_read, file_skip, file_close, file_visit_fds));
}

static ssize_t
file_read(struct transform *t, void *client_data, struct transform_read_filter *upstream,
	const void **buff)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;
	ssize_t bytes_read;

	*buff = mine->buffer;
	for (;;) {
		bytes_read = read(mine->fd, mine->buffer, mine->block_size);
		if (bytes_read < 0) {
			if (errno == EINTR)
				continue;
			transform_set_error(t, errno, "Error reading fd %d", mine->fd);
		}
		return (bytes_read);
	}
}

static int64_t
file_skip(struct transform *t, void *client_data, int64_t request)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;
	off_t old_offset, new_offset;

	if (!mine->use_lseek)
		return (0);

	/* Reduce request to the next smallest multiple of block_size */
	request = (request / mine->block_size) * mine->block_size;
	if (request == 0)
		return (0);

	if (((old_offset = lseek(mine->fd, 0, SEEK_CUR)) >= 0) &&
	    ((new_offset = lseek(mine->fd, request, SEEK_CUR)) >= 0))
		return (new_offset - old_offset);

	/* If seek failed once, it will probably fail again. */
	mine->use_lseek = 0;

	/* Let libtransform recover with read+discard. */
	if (errno == ESPIPE)
		return (0);

	/*
	 * There's been an error other than ESPIPE. This is most
	 * likely caused by a programmer error (too large request)
	 * or a corrupted transform file.
	 */
	transform_set_error(t, errno, "Error seeking");
	return (-1);
}

static int
file_close(struct transform *t, void *client_data)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;

	(void)t; /* UNUSED */
	free(mine->buffer);
	free(mine);
	return (TRANSFORM_OK);
}

static int
file_visit_fds(struct transform *transform, const void *_data,
	transform_fd_visitor *visitor, const void *visitor_data)
{
	struct read_fd_data *mine = (struct read_fd_data *)_data;
	return visitor(transform, mine->fd, (void *)visitor_data);
}
