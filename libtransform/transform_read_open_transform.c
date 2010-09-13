/*-
 * Copyright (c) 2010 Brian Harring
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

struct private_data {
	struct transform *src;
};

static int _transform_read(struct transform *, void *,
	struct transform_read_filter *, const void **, size_t *);
static int64_t _transform_skip(struct transform *, void *,
	struct transform_read_filter *, int64_t);
static int _transform_close(struct transform *, void *);


int
transform_read_open_transform(struct transform *t, struct transform *src,
	int64_t start, int64_t length)
{
	int ret;
	struct private_data *data = (struct private_data *)malloc(
		sizeof(struct private_data));

	if (!data) {
		transform_set_error(t, ENOMEM,
			"failed allocating private data");
		return (TRANSFORM_FATAL);
	}

	data->src = src;

	if (start != 0 || length != -1) {
		ret = transform_read_add_window(t, start, length);
		if (TRANSFORM_OK != ret) {
			free(data);
			return (ret);
		}
	}

	return (transform_read_open2(t, (void *)data, NULL,
		_transform_read, _transform_skip, _transform_close,
		NULL, TRANSFORM_FILTER_NOTIFY_ALL_CONSUME_FLAG));
}

static int
_transform_read(struct transform *t, void *_data,
	struct transform_read_filter *upstream, const void **buff, size_t *bytes_read)
{
	struct private_data *data = (struct private_data *)_data;
	ssize_t avail;

	*buff = transform_read_ahead(data->src, 1, &avail);
	if (avail < 0) {
		transform_set_error(t,
			transform_errno(data->src),
			"%s",
			transform_error_string(data->src));
	}
	*bytes_read = avail;
	return (avail == 0 ? TRANSFORM_EOF : TRANSFORM_OK);
}

static int64_t _transform_skip(struct transform *t, void *_data,
	struct transform_read_filter *upstream, int64_t request)
{
	struct private_data *data = (struct private_data *)_data;

	return (transform_read_consume(data->src, request));
}

static int
_transform_close(struct transform *t, void *_data)
{
	struct private_data *data = (struct private_data *)_data;
	free(data);
	return (TRANSFORM_OK);
}
	
