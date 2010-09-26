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

__FBSDID("$FreeBSD: head/lib/libtransform/transform_write_set_compression_gzip.c 201081 2009-12-28 02:04:42Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "transform.h"
#include "transform_private.h"
#include "transform_write_private.h"

#ifndef HAVE_ZLIB_H
int
transform_write_add_filter_gzip(struct transform *a)
{
	transform_set_error(a, TRANSFORM_ERRNO_MISC,
	    "gzip compression not supported on this platform");
	return (TRANSFORM_FATAL);
}
#else
/* Don't compile this if we don't have zlib. */

struct private_data {
	int		 compression_level;
	z_stream	 stream;
	int64_t		 total_in;
	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
	unsigned long	 crc;
};

/*
 * Yuck.  zlib.h is not const-correct, so I need this one bit
 * of ugly hackery to convert a const * pointer to a non-const pointer.
 */
#define	SET_NEXT_IN(st,src)					\
	(st)->stream.next_in = (Bytef *)(uintptr_t)(const void *)(src)

static int transform_compressor_gzip_options(struct transform *, void *,
		    const char *, const char *);
static int transform_compressor_gzip_open(struct transform *, void **);
static ssize_t transform_compressor_gzip_write(struct transform *,
	void *, const void *, size_t, struct transform_write_filter *);
static int transform_compressor_gzip_close(struct transform *, void *,
	struct transform_write_filter *);
static int transform_compressor_gzip_free(struct transform *, void *);
static int drive_compressor(struct transform *, struct private_data *,
	int finishing, struct transform_write_filter *);


/*
 * Add a gzip compression filter to this write handle.
 */
int
transform_write_add_filter_gzip(struct transform *t)
{
	struct private_data *data;
	transform_check_magic(t, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_add_filter_gzip");

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		transform_set_error(t, ENOMEM, "Out of memory");
		return (TRANSFORM_FATAL);
	}
	data->compression_level = Z_DEFAULT_COMPRESSION;
	
	if (TRANSFORM_OK != transform_write_add_filter(t,
		data, "gzip", TRANSFORM_FILTER_GZIP,
		transform_compressor_gzip_options,
		transform_compressor_gzip_open,
		transform_compressor_gzip_write,
		transform_compressor_gzip_close,
		transform_compressor_gzip_free,
		0)) {
		free(data);
		return (TRANSFORM_FATAL);
	}
	return (TRANSFORM_OK);
}

/*
 * Setup callback.
 */
static int
transform_compressor_gzip_open(struct transform *t, void **_data)
{
	struct private_data *data = (struct private_data *)*_data;
	int ret;
	time_t cur_time;

	if (data->compressed == NULL) {
		data->compressed_buffer_size = 65536;
		data->compressed
		    = (unsigned char *)malloc(data->compressed_buffer_size);
		if (data->compressed == NULL) {
			transform_set_error(t, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (TRANSFORM_FATAL);
		}
	}

	data->crc = crc32(0L, NULL, 0);
	data->stream.next_out = data->compressed;
	data->stream.avail_out = data->compressed_buffer_size;

	/* Prime output buffer with a gzip header. */
	cur_time = time(NULL);
	data->compressed[0] = 0x1f; /* GZip signature bytes */
	data->compressed[1] = 0x8b;
	data->compressed[2] = 0x08; /* "Deflate" compression */
	data->compressed[3] = 0; /* No options */
	data->compressed[4] = (cur_time)&0xff;  /* Timestamp */
	data->compressed[5] = (cur_time>>8)&0xff;
	data->compressed[6] = (cur_time>>16)&0xff;
	data->compressed[7] = (cur_time>>24)&0xff;
	data->compressed[8] = 0; /* No deflate options */
	data->compressed[9] = 3; /* OS=Unix */
	data->stream.next_out += 10;
	data->stream.avail_out -= 10;

	/* Initialize compression library. */
	ret = deflateInit2(&(data->stream),
	    data->compression_level,
	    Z_DEFLATED,
	    -15 /* < 0 to suppress zlib header */,
	    8,
	    Z_DEFAULT_STRATEGY);

	if (ret == Z_OK) {
		return (TRANSFORM_OK);
	}

	/* Library setup failed: clean up. */
	transform_set_error(t, TRANSFORM_ERRNO_MISC, "Internal error "
	    "initializing compression library");

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case Z_STREAM_ERROR:
		transform_set_error(t, TRANSFORM_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		transform_set_error(t, ENOMEM, "Internal error initializing "
		    "compression library");
		break;
	case Z_VERSION_ERROR:
		transform_set_error(t, TRANSFORM_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid library version");
		break;
	}

	return (TRANSFORM_FATAL);
}

/*
 * Set write options.
 */
static int
transform_compressor_gzip_options(struct transform *t, void *_data,
	const char *key, const char *value)
{
	struct private_data *data = (struct private_data *)_data;

	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0')
			return (TRANSFORM_WARN);
		data->compression_level = value[0] - '0';
		return (TRANSFORM_OK);
	}
	return (TRANSFORM_WARN);
}

/*
 * Write data to the compressed stream.
 */
static ssize_t
transform_compressor_gzip_write(struct transform *t,
	void *_data, const void *buff, size_t length,
	struct transform_write_filter *upstream)
{
	struct private_data *data = (struct private_data *)_data;
	int ret;

	/* Update statistics */
	data->crc = crc32(data->crc, (const Bytef *)buff, length);
	data->total_in += length;


	/* Compress input data to output buffer */
	SET_NEXT_IN(data, buff);
	data->stream.avail_in = length;

	ret = drive_compressor(t, data, 0, upstream);

	if (TRANSFORM_OK == ret) {
		return (length);
	}

	return (ret);
}

/*
 * Finish the compression...
 */
static int
transform_compressor_gzip_close(struct transform *t, void *_data,
	struct transform_write_filter *upstream)
{
	unsigned char trailer[8];
	struct private_data *data = (struct private_data *)_data;
	int ret, r1;

	/* Finish compression cycle */
	ret = drive_compressor(t, data, 1, upstream);
	if (ret == TRANSFORM_OK) {
		/* Write the last compressed data. */
		ret = __transform_write_filter(upstream,
		    data->compressed,
		    data->compressed_buffer_size - data->stream.avail_out);
	}
	if (ret == TRANSFORM_OK) {
		/* Build and write out 8-byte trailer. */
		trailer[0] = (data->crc)&0xff;
		trailer[1] = (data->crc >> 8)&0xff;
		trailer[2] = (data->crc >> 16)&0xff;
		trailer[3] = (data->crc >> 24)&0xff;
		trailer[4] = (data->total_in)&0xff;
		trailer[5] = (data->total_in >> 8)&0xff;
		trailer[6] = (data->total_in >> 16)&0xff;
		trailer[7] = (data->total_in >> 24)&0xff;
		ret = __transform_write_filter(upstream, trailer, 8);
	}

	switch (deflateEnd(&(data->stream))) {
	case Z_OK:
		break;
	default:
		transform_set_error(t, TRANSFORM_ERRNO_MISC,
		    "Failed to clean up compressor");
		ret = TRANSFORM_FATAL;
	}
	r1 = __transform_write_close_filter(upstream);
	return (r1 < ret ? r1 : ret);
}

static int
transform_compressor_gzip_free(struct transform *t, void *_data)
{
	struct private_data *data = (struct private_data *)_data;
	free(data->compressed);
	free(data);
	return (TRANSFORM_OK);
}

/*
 * Utility function to push input data through compressor,
 * writing full output blocks as necessary.
 *
 * Note that this handles both the regular write case (finishing ==
 * false) and the end-of-transform case (finishing == true).
 */
static int
drive_compressor(struct transform *t, struct private_data *data, int finishing,
	struct transform_write_filter *upstream)
{
	int ret;

	for (;;) {
		if (data->stream.avail_out == 0) {
			ret = __transform_write_filter(upstream,
			    data->compressed,
			    data->compressed_buffer_size);
			if (ret != TRANSFORM_OK)
				return (TRANSFORM_FATAL);
			data->stream.next_out = data->compressed;
			data->stream.avail_out = data->compressed_buffer_size;
		}

		/* If there's nothing to do, we're done. */
		if (!finishing && data->stream.avail_in == 0)
			return (TRANSFORM_OK);

		ret = deflate(&(data->stream),
		    finishing ? Z_FINISH : Z_NO_FLUSH );

		switch (ret) {
		case Z_OK:
			/* In non-finishing case, check if compressor
			 * consumed everything */
			if (!finishing && data->stream.avail_in == 0)
				return (TRANSFORM_OK);
			/* In finishing case, this return always means
			 * there's more work */
			break;
		case Z_STREAM_END:
			/* This return can only occur in finishing case. */
			return (TRANSFORM_OK);
		default:
			/* Any other return value indicates an error. */
			transform_set_error(t, TRANSFORM_ERRNO_MISC,
			    "GZip compression failed:"
			    " deflate() call returned status %d",
			    ret);
			return (TRANSFORM_FATAL);
		}
	}
}

#endif /* HAVE_ZLIB_H */
