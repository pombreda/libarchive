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

__FBSDID("$FreeBSD: head/lib/libtransform/transform_write_set_compression_bzip2.c 201091 2009-12-28 02:22:41Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif

#include "transform.h"
#include "transform_private.h"
#include "transform_write_private.h"

#if !defined(HAVE_BZLIB_H) || !defined(BZ_CONFIG_ERROR)
int
transform_write_add_filter_bzip2(struct transform *a)
{
	transform_set_error(a, TRANSFORM_ERRNO_MISC,
	    "bzip2 compression not supported on this platform");
	return (TRANSFORM_FATAL);
}
#else
/* Don't compile this if we don't have bzlib. */

struct private_data {
	int		 compression_level;
	bz_stream	 stream;
	int64_t		 total_in;
	char		*compressed;
	size_t		 compressed_buffer_size;
};

/*
 * Yuck.  bzlib.h is not const-correct, so I need this one bit
 * of ugly hackery to convert a const * pointer to a non-const pointer.
 */
#define	SET_NEXT_IN(st,src)					\
	(st)->stream.next_in = (char *)(uintptr_t)(const void *)(src)

static int transform_compressor_bzip2_close(struct transform_write_filter *);
static int transform_compressor_bzip2_free(struct transform *, void *);
static int transform_compressor_bzip2_open(struct transform_write_filter *);
static int transform_compressor_bzip2_options(struct transform_write_filter *,
		    const char *, const char *);
static int transform_compressor_bzip2_write(struct transform_write_filter *,
	const void *, const void *, size_t);
static int drive_compressor(struct transform_write_filter *,
		    struct private_data *, int finishing);

/*
 * Add a bzip2 compression filter to this write handle.
 */
int
transform_write_add_filter_bzip2(struct transform *t)
{
	struct private_data *data;

	transform_check_magic(t, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_add_filter_bzip2");

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		transform_set_error(t, ENOMEM, "Out of memory");
		return (TRANSFORM_FATAL);
	}
	data->compression_level = 9; /* default */

	if (TRANSFORM_OK != transform_write_add_filter(t, data, "bzip2",
		TRANSFORM_FILTER_BZIP2,
		transform_compressor_bzip2_options,
		transform_compressor_bzip2_open,
		transform_compressor_bzip2_write,
		transform_compressor_bzip2_close,
		transform_compressor_bzip2_free,
		0)) {
		return (TRANSFORM_FATAL);
	}
	return (TRANSFORM_OK);
}

/*
 * Setup callback.
 */
static int
transform_compressor_bzip2_open(struct transform_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->base.data;
	int ret;

	ret = __transform_write_open_filter(f->base.upstream.write);
	if (ret != 0)
		return (ret);

	// TODO: Find a better way to size this.  (Maybe look at the
	// block size expected by the following filter?)
	if (data->compressed == NULL) {
		data->compressed_buffer_size = 65536;
		data->compressed
		    = (char *)malloc(data->compressed_buffer_size);
		if (data->compressed == NULL) {
			transform_set_error(f->base.transform, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (TRANSFORM_FATAL);
		}
	}

	memset(&data->stream, 0, sizeof(data->stream));
	data->stream.next_out = data->compressed;
	data->stream.avail_out = data->compressed_buffer_size;

	/* Initialize compression library */
	ret = BZ2_bzCompressInit(&(data->stream),
	    data->compression_level, 0, 30);
	if (ret == BZ_OK) {
		f->base.data = data;
		return (TRANSFORM_OK);
	}

	/* Library setup failed: clean up. */
	transform_set_error(f->base.transform, TRANSFORM_ERRNO_MISC,
	    "Internal error initializing compression library");

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case BZ_PARAM_ERROR:
		transform_set_error(f->base.transform, TRANSFORM_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid setup parameter");
		break;
	case BZ_MEM_ERROR:
		transform_set_error(f->base.transform, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	case BZ_CONFIG_ERROR:
		transform_set_error(f->base.transform, TRANSFORM_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "mis-compiled library");
		break;
	}

	return (TRANSFORM_FATAL);

}

/*
 * Set write options.
 */
static int
transform_compressor_bzip2_options(struct transform_write_filter *f,
    const char *key, const char *value)
{
	struct private_data *data = (struct private_data *)f->base.data;

	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0')
			return (TRANSFORM_WARN);
		data->compression_level = value[0] - '0';
		/* Make '0' be a synonym for '1'. */
		/* This way, bzip2 compressor supports the same 0..9
		 * range of levels as gzip. */
		if (data->compression_level < 1)
			data->compression_level = 1;
		return (TRANSFORM_OK);
	}

	return (TRANSFORM_WARN);
}

/*
 * Write data to the compressed stream.
 *
 * Returns TRANSFORM_OK if all data written, error otherwise.
 */
static int
transform_compressor_bzip2_write(struct transform_write_filter *f,
	const void *filter_data, const void *buff, size_t length)
{
	struct private_data *data = (struct private_data *)filter_data;

	/* Update statistics */
	data->total_in += length;

	/* Compress input data to output buffer */
	SET_NEXT_IN(data, buff);
	data->stream.avail_in = length;
	if (drive_compressor(f, data, 0))
		return (TRANSFORM_FATAL);
	return (TRANSFORM_OK);
}


/*
 * Finish the compression.
 */
static int
transform_compressor_bzip2_close(struct transform_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->base.data;
	int ret, r1;

	/* Finish compression cycle. */
	ret = drive_compressor(f, data, 1);
	if (ret == TRANSFORM_OK) {
		/* Write the last block */
		ret = __transform_write_filter(f->base.upstream.write,
		    data->compressed,
		    data->compressed_buffer_size - data->stream.avail_out);
	}

	switch (BZ2_bzCompressEnd(&(data->stream))) {
	case BZ_OK:
		break;
	default:
		transform_set_error(f->base.transform, TRANSFORM_ERRNO_PROGRAMMER,
		    "Failed to clean up compressor");
		ret = TRANSFORM_FATAL;
	}

	r1 = __transform_write_close_filter(f->base.upstream.write);
	return (r1 < ret ? r1 : ret);
}

static int
transform_compressor_bzip2_free(struct transform *t, void *_data)
{
	struct private_data *data = (struct private_data *)_data;
	free(data->compressed);
	free(data);
	return (TRANSFORM_OK);
}

/*
 * Utility function to push input data through compressor, writing
 * full output blocks as necessary.
 *
 * Note that this handles both the regular write case (finishing ==
 * false) and the end-of-transform case (finishing == true).
 */
static int
drive_compressor(struct transform_write_filter *f,
    struct private_data *data, int finishing)
{
	ssize_t	bytes_written;
	int ret;

	for (;;) {
		if (data->stream.avail_out == 0) {
			bytes_written = __transform_write_filter(f->base.upstream.write,
			    data->compressed,
			    data->compressed_buffer_size);
			if (bytes_written <= 0) {
				/* TODO: Handle this write failure */
				return (TRANSFORM_FATAL);
			} else if ((size_t)bytes_written < data->compressed_buffer_size) {
				/* Short write: Move remainder to
				 * front and keep filling */
				memmove(data->compressed,
				    data->compressed + bytes_written,
				    data->compressed_buffer_size - bytes_written);
			}

			data->stream.next_out = data->compressed +
			    data->compressed_buffer_size - bytes_written;
			data->stream.avail_out = bytes_written;
		}

		/* If there's nothing to do, we're done. */
		if (!finishing && data->stream.avail_in == 0)
			return (TRANSFORM_OK);

		ret = BZ2_bzCompress(&(data->stream),
		    finishing ? BZ_FINISH : BZ_RUN);

		switch (ret) {
		case BZ_RUN_OK:
			/* In non-finishing case, did compressor
			 * consume everything? */
			if (!finishing && data->stream.avail_in == 0)
				return (TRANSFORM_OK);
			break;
		case BZ_FINISH_OK:  /* Finishing: There's more work to do */
			break;
		case BZ_STREAM_END: /* Finishing: all done */
			/* Only occurs in finishing case */
			return (TRANSFORM_OK);
		default:
			/* Any other return value indicates an error */
			transform_set_error(f->base.transform,
			    TRANSFORM_ERRNO_PROGRAMMER,
			    "Bzip2 compression failed;"
			    " BZ2_bzCompress() returned %d",
			    ret);
			return (TRANSFORM_FATAL);
		}
	}
}

#endif /* HAVE_BZLIB_H && BZ_CONFIG_ERROR */
