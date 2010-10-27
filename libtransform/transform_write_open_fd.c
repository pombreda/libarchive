/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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
__FBSDID("$FreeBSD: head/lib/libtransform/transform_write_open_filename.c 191165 2009-04-17 00:39:35Z kientzle $");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
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

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct write_file_data {
	int		fd;
	int 	close_fd;
	char	filename[1];
};

static int  common_open_write_fd(struct transform *t, int fd, const char *filename);
static int	file_close(struct transform *, void *, struct transform_write_filter *);
static int	file_open(struct transform *, void *);
static ssize_t	file_write(struct transform *, void *, const void *buff, size_t,
	struct transform_write_filter *);

int
transform_write_open_filename(struct transform *t, const char *filename)
{
	if (!filename) {
		transform_set_error(t, TRANSFORM_ERRNO_PROGRAMMER,
			"null filename passed in");
		return (TRANSFORM_FATAL);
	}

	return (common_open_write_fd(t, -1, NULL));
}

int
transform_write_open_fd(struct transform *t, int fd)
{
	if (fd < 0) {
		transform_set_error(t, EINVAL,
			"transform_write_open_fd called with an invalid fd- %d",
			fd);
		return (TRANSFORM_FATAL);
	}
	return (common_open_write_fd(t, fd, NULL));
}

static int
common_open_write_fd(struct transform *t, int fd, const char *filename)
{
	struct write_file_data *mine;
	size_t filename_len = 0;

	if (filename) {
		filename_len = strlen(filename);
	}

	mine = (struct write_file_data *)calloc(1, sizeof(*mine) + filename_len);
	if (!mine) {
		transform_set_error(t, ENOMEM, "No memory");
		return (TRANSFORM_FATAL);
	}
	if (filename) {
		strcpy(mine->filename, filename);
	}
	mine->fd = fd;
	mine->close_fd = (fd < 0);
	return (transform_write_open(t, mine,
		file_open, file_write, file_close));
}

static int
file_open(struct transform *t, void *client_data)
{
	int flags;
	struct write_file_data *mine;
	struct stat st;

	mine = (struct write_file_data *)client_data;

	/*
	 * Open the file if needed
	 */
	if (mine->fd < 0) {
		flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
		mine->fd = open(mine->filename, flags, 0666);
		if (mine->fd < 0) {
			transform_set_error(t, errno, "Failed to open '%s'",
				mine->filename);
			return (TRANSFORM_FATAL);
		}
	}

	if (fstat(mine->fd, &st) != 0) {
		transform_set_error(t, errno, "Couldn't stat '%s'",
			mine->filename);
		return (TRANSFORM_FATAL);
	}

	/*
	 * Set up default last block handling.
	 */
	if (transform_write_get_bytes_in_last_block(t) < 0) {
		if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) ||
			S_ISFIFO(st.st_mode))
			/* Pad last block when writing to device or FIFO. */
			transform_write_set_bytes_in_last_block(t, 0);
		else
			/* Don't pad last block otherwise. */
			transform_write_set_bytes_in_last_block(t, 1);
	}

	return (TRANSFORM_OK);
}

static ssize_t
file_write(struct transform *t, void *client_data, const void *buff, size_t length,
	struct transform_write_filter *upstream)
{
	struct write_file_data	*mine;
	ssize_t	bytesWritten;

	(void)upstream;

	mine = (struct write_file_data *)client_data;
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
file_close(struct transform *t, void *client_data, struct transform_write_filter *upstream)
{
	struct write_file_data	*mine = (struct write_file_data *)client_data;

	(void)t; /* UNUSED */
	(void)upstream;
	if (mine->close_fd) {
		close(mine->fd);
	}
	free(mine);
	return (TRANSFORM_OK);
}
