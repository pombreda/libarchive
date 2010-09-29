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
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_open_file.c 201093 2009-12-28 02:28:44Z kientzle $");

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

struct read_FILE_data {
	FILE    *f;
	char	 can_skip;
};

static int	file_close(struct transform *, void *);
static int	file_read(struct transform *, void *,
	struct transform_read_filter *, const void **buff, size_t *bytes_read);
static int64_t	file_skip(struct transform *, void *, 
	struct transform_read_filter *, int64_t request);
static int      file_visit_fds(struct transform *, const void *,
    transform_fd_visitor *visitor, const void *visitor_data);

int
transform_read_open_FILE(struct transform *t, FILE *f)
{
	struct stat st;
	struct transform_read_filter *source;
	struct read_FILE_data *mine;

	transform_clear_error(t);
	mine = (struct read_FILE_data *)malloc(sizeof(*mine));
	if (mine == NULL) {
		transform_set_error(t, ENOMEM, "No memory");
		free(mine);
		return (TRANSFORM_FATAL);
	}
	mine->f = f;
	/*
	 * If we can't fstat() the file, it may just be that it's not
	 * a file.  (FILE * objects can wrap many kinds of I/O
	 * streams, some of which don't support fileno()).)
	 */
	if (fstat(fileno(mine->f), &st) == 0 && S_ISREG(st.st_mode)) {
		/* Enable the seek optimization only for regular files. */
		mine->can_skip = 1;
	} else
		mine->can_skip = 0;

#if defined(__CYGWIN__) || defined(_WIN32)
	setmode(fileno(mine->f), O_BINARY);
#endif

	source = transform_read_filter_new(mine, "source:FILE",
		TRANSFORM_FILTER_NONE, file_read, mine->can_skip ? file_skip : NULL,
		file_close, file_visit_fds, TRANSFORM_FILTER_SOURCE);

	if (!source) {
		transform_set_error(t, ENOMEM, "failed allocating filter");
		return (TRANSFORM_FATAL);
	}

	/* we ignore error output here; filter isn't initialized (meaning no alloc),
	 * no error possible */
	transform_read_filter_set_buffering(source, DEFAULT_FILE_BLOCK_SIZE);

	return transform_read_open_source(t, source);
}

static int
file_read(struct transform *t, void *client_data,
	struct transform_read_filter *f, const void **buff, size_t *bytes_read)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;

	*bytes_read = fread((void *)*buff, 1, *bytes_read, mine->f);
	if (*bytes_read < 0) {
		transform_set_error(t, errno, "Error reading file");
		return (TRANSFORM_FATAL);
	}
	return (*bytes_read == 0 ? TRANSFORM_EOF : TRANSFORM_OK);
}

static int64_t
file_skip(struct transform *t, void *client_data,
	struct transform_read_filter *upstream, int64_t request)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;

	(void)t; /* UNUSED */
	(void)upstream;

	/*
	 * If we can't skip, return 0 as the amount we did step and
	 * the caller will work around by reading and discarding.
	 */
	if (!mine->can_skip)
		return (0);
	if (request == 0)
		return (0);

#if HAVE_FSEEKO
	if (fseeko(mine->f, request, SEEK_CUR) != 0)
#else
	if (fseek(mine->f, request, SEEK_CUR) != 0)
#endif
	{
		mine->can_skip = 0;
		return (0);
	}
	return (request);
}

static int
file_close(struct transform *t, void *client_data)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;

	(void)t; /* UNUSED */
	free(mine);
	return (TRANSFORM_OK);
}

static int
file_visit_fds(struct transform *transform, const void *_data,
    transform_fd_visitor *visitor, const void *visitor_data)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)_data;
	struct stat st;
	int fd = fileno(mine->f);
	if(-1 == fd) {
		transform_set_error((struct transform *)transform,
			errno, "fstating failed");
		return (TRANSFORM_FATAL);
	}
	/* 
	 * do a fstat to ensure it's actually a file of some sort.
	 * note we're mainly just matching behaviour from above...
	 */
	if(0 == fstat(fd, &st)) {
		return visitor(transform, fd, (void *)visitor_data);
	}
	return (TRANSFORM_OK);
}
