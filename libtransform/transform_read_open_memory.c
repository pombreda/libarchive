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
__FBSDID("$FreeBSD: src/lib/libtransform/transform_read_open_memory.c,v 1.6 2007/07/06 15:51:59 kientzle Exp $");

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "transform.h"
#include "transform_read_private.h"

/*
 * Glue to read an transform from a block of memory.
 *
 * This is mostly a huge help in building test harnesses;
 * test programs can build transforms in memory and read them
 * back again without having to mess with files on disk.
 */

struct read_memory_data {
	unsigned char	*buffer;
	unsigned char	*end;
	ssize_t	 read_size;
};

static int	memory_read_close(struct transform *, void *);
static int64_t	memory_read_skip(struct transform *, void *,
	struct transform_read_filter *upstream, int64_t request);
static int	memory_read(struct transform *, void *, struct transform_read_filter *,
	const void **buff, size_t *bytes_read);

int
transform_read_open_memory(struct transform *a, void *buff, size_t size)
{
	return transform_read_open_memory2(a, buff, size, size);
}

/*
 * Don't use _open_memory2() in production code; the transform_read_open_memory()
 * version is the one you really want.  This is just here so that
 * test harnesses can exercise block operations inside the library.
 */
int
transform_read_open_memory2(struct transform *a, void *buff,
    size_t size, size_t read_size)
{
	struct read_memory_data *mine;
	struct transform_read_filter *source;

	mine = (struct read_memory_data *)calloc(1, sizeof(*mine));
	if (mine == NULL) {
		transform_set_error(a, ENOMEM, "No memory");
		return (TRANSFORM_FATAL);
	}

	mine->buffer = (unsigned char *)buff;
	mine->end = mine->buffer + size;
	mine->read_size = read_size;

	source = transform_read_filter_new(mine, "source:memory",
		TRANSFORM_FILTER_NONE, memory_read, memory_read_skip,
		memory_read_close, NULL, TRANSFORM_FILTER_SOURCE | TRANSFORM_FILTER_SELF_BUFFERING);

	if (!source) {
		transform_set_error(a, ENOMEM, "failed allocating filter");
		return TRANSFORM_FATAL;
	}

	return transform_read_open_source(a, source);
}

/*
 * This is scary simple:  Just advance a pointer.  Limiting
 * to read_size is not technically necessary, but it exercises
 * more of the internal logic when used with a small block size
 * in a test harness.  Production use should not specify a block
 * size; then this is much faster.
 */
static int
memory_read(struct transform *t, void *client_data,
	struct transform_read_filter *f, const void **buff, size_t *bytes_read)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;

	(void)t; /* UNUSED */
	*buff = mine->buffer;
	*bytes_read = mine->end - mine->buffer;
	if (*bytes_read > mine->read_size) {
		*bytes_read = mine->read_size;
	}
	mine->buffer += *bytes_read;

	return (mine->buffer == mine->end ? TRANSFORM_EOF : TRANSFORM_OK);
}

/*
 * Advancing is just as simple.  Again, this is doing more than
 * necessary in order to better exercise internal code when used
 * as a test harness.
 */
static int64_t
memory_read_skip(struct transform *t, void *client_data,
	struct transform_read_filter *upstream, int64_t skip)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;

	(void)t; /* UNUSED */
	(void)upstream;
	
	if ((int64_t)skip > (int64_t)(mine->end - mine->buffer))
		skip = mine->end - mine->buffer;
	/* Round down to block size. */
	skip /= mine->read_size;
	skip *= mine->read_size;
	mine->buffer += skip;
	return (skip);
}

/*
 * Close is just cleaning up our one small bit of data.
 */
static int
memory_read_close(struct transform *t, void *client_data)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;
	(void)t; /* UNUSED */
	free(mine);
	return (TRANSFORM_OK);
}
