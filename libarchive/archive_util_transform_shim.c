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


#include "archive.h"
#include "archive_platform.h"

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


struct archive_shim {
	struct archive *archive;
	archive_open_callback *opener;
	archive_close_callback *closer;
	archive_skip_callback *seeker;
	archive_write_callback *writer;
	archive_read_callback *reader;
	void *client_data;
};

void *
__archive_shim_new(struct archive *a,
	void *client_data,
	archive_open_callback *opener,
	archive_close_callback *closer,
	archive_write_callback *writer,
	archive_read_callback *reader,
	archive_skip_callback *seeker)
{
	struct archive_shim *mine;

	mine = (struct archive_shim *)malloc(sizeof(*mine));
	if(NULL == mine)
		return (NULL);
	mine->archive = a;
	mine->client_data = client_data;
	mine->opener = opener;
	mine->closer = closer;
	mine->seeker = seeker;
	mine->writer = writer;
	mine->reader = reader;
	return (mine);
}


static int
__archive_error_to_transform(struct archive *a, struct transform *t, int ret)
{
    switch(ret) {
    case ARCHIVE_OK:
        return TRANSFORM_OK;
	case ARCHIVE_EOF:
		return TRANSFORM_EOF;
    case ARCHIVE_FAILED:
    	return TRANSFORM_FATAL;
    case ARCHIVE_WARN:
        return TRANSFORM_WARN;
    default:
        transform_set_error(t, ARCHIVE_ERRNO_MISC,
            "unhandled return from archive_shim- errno %d, error %s",
            archive_errno(a),
            archive_error_string(a));
        return TRANSFORM_FATAL;
    }
}	

int
__archive_shim_open(struct transform *t, void *data)
{
    struct archive_shim *mine = (struct archive_shim *)data;
	if (mine->opener) {
		return (__archive_error_to_transform(mine->archive, t,
			(mine->opener)(mine->archive, mine->client_data)));
	}
	return (TRANSFORM_OK);
}

int
__archive_shim_close(struct transform *t, void *data)
{
	int ret = TRANSFORM_OK;
    struct archive_shim *mine = (struct archive_shim *)data;
	if (mine->closer) {
		ret = (__archive_error_to_transform(mine->archive, t,
			(mine->closer)(mine->archive, mine->client_data)));
	}
	free(mine);
	return (ret);
}

#if ARCHIVE_VERSION_NUMBER < 3000000
/* Libarchive 2.0 used off_t here, but that is a bad idea on Linux and a
 * few other platforms where off_t varies with build settings. */
off_t
__archive_shim_skip(struct transform *t, void *data,
	struct transform_read_filter *upstream, off_t request)
#else
int64_t
__archive_shim_skip(struct transform *t, void *data, 
	struct transform_read_filter *upstream, int64_t request)
#endif
{
	struct archive_shim *mine = (struct archive_shim *)data;
	if (mine->seeker) {
		return ((mine->seeker)(mine->archive, mine->client_data, request));
	}
	return (0);
}

ssize_t
__archive_shim_write(struct transform *t, void *data, const void *buff,
    size_t length)
{
	struct archive_shim *mine = (struct archive_shim *)data;
	if (mine->writer) {
		return ((mine->writer)(mine->archive, mine->client_data, buff, length));
	}
	transform_set_error(t, ARCHIVE_ERRNO_PROGRAMMER, "unset writer");
	return (-1);
}

int
__archive_shim_read(struct transform *t, void *data, struct transform_read_filter *upstream,
	const void **buff, size_t *bytes_read)
{
	struct archive_shim *mine = (struct archive_shim *)data;
	ssize_t ret;

	if (mine->reader) {
		ret = ((mine->reader)(mine->archive, mine->client_data, buff));
		if (ARCHIVE_EOF == ret || 0 == ret) {
			*bytes_read = 0;
			return (TRANSFORM_EOF);
		} else if (ret < 0) {
			return __archive_error_to_transform(mine->archive, t, ret);
		}
		*bytes_read = ret;
		return (TRANSFORM_OK);
	}

	transform_set_error(t, ARCHIVE_ERRNO_PROGRAMMER, "unset reader");
	return (TRANSFORM_FATAL);
}
