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
__FBSDID("$FreeBSD: src/lib/libtransform/transform_write_open_file.c,v 1.19 2007/01/09 08:05:56 kientzle Exp $");

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

struct write_FILE_data {
	FILE		*f;
};

static int	file_close(struct transform *, void *, struct transform_write_filter *);
static int	file_open(struct transform *, void *);
static ssize_t	file_write(struct transform *, void *, const void *buff, size_t,
	struct transform_write_filter *);

int
transform_write_open_FILE(struct transform *a, FILE *f)
{
	struct write_FILE_data *mine;

	mine = (struct write_FILE_data *)malloc(sizeof(*mine));
	if (mine == NULL) {
		transform_set_error(a, ENOMEM, "No memory");
		return (TRANSFORM_FATAL);
	}
	mine->f = f;
	return (transform_write_open(a, mine,
		    file_open, file_write, file_close));
}

static int
file_open(struct transform *t, void *client_data)
{
	(void)t; /* UNUSED */
	(void)client_data; /* UNUSED */

	return (TRANSFORM_OK);
}

static ssize_t
file_write(struct transform *t, void *client_data, const void *buff, size_t length,
	struct transform_write_filter *upstream)
{
	struct write_FILE_data	*mine;
	size_t	bytesWritten;

	(void)upstream;

	mine = client_data;
	for (;;) {
		bytesWritten = fwrite(buff, 1, length, mine->f);
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
	struct write_FILE_data	*mine = client_data;

	(void)t; /* UNUSED */
	(void)upstream;
	free(mine);
	return (TRANSFORM_OK);
}
