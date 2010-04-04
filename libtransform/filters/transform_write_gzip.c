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

static int transform_compressor_gzip_options(struct transform_write_filter *,
		    const char *, const char *);
static int transform_compressor_gzip_open(struct transform_write_filter *);
static int transform_compressor_gzip_write(struct transform_write_filter *,
		    const void *, size_t);
static int transform_compressor_gzip_close(struct transform_write_filter *);
static int transform_compressor_gzip_free(struct transform_write_filter *);
static int drive_compressor(struct transform_write_filter *,
		    struct private_data *, int finishing);


/*
 * Add a gzip compression filter to this write handle.
 */
int
transform_write_add_filter_gzip(struct transform *_a)
{
	struct transform_write *a = (struct transform_write *)_a;
	struct transform_write_filter *f = __transform_write_allocate_filter(_a);
	struct private_data *data;
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_add_filter_gzip");

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		transform_set_error(&a->transform, ENOMEM, "Out of memory");
		return (TRANSFORM_FATAL);
	}
	f->data = data;
	data->compression_level = Z_DEFAULT_COMPRESSION;
	f->open = &transform_compressor_gzip_open;
	f->options = &transform_compressor_gzip_options;
	f->close = &transform_compressor_gzip_close;
	f->free = &transform_compressor_gzip_free;
	f->code = TRANSFORM_FILTER_GZIP;
	f->name = "gzip";
	return (TRANSFORM_OK);
}

/*
 * Setup callback.
 */
static int
transform_compressor_gzip_open(struct transform_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret;
	time_t t;

	ret = __transform_write_open_filter(f->next_filter);
	if (ret != TRANSFORM_OK)
		return (ret);

	if (data->compressed == NULL) {
		data->compressed_buffer_size = 65536;
		data->compressed
		    = (unsigned char *)malloc(data->compressed_buffer_size);
		if (data->compressed == NULL) {
			transform_set_error(f->transform, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (TRANSFORM_FATAL);
		}
	}

	data->crc = crc32(0L, NULL, 0);
	data->stream.next_out = data->compressed;
	data->stream.avail_out = data->compressed_buffer_size;

	/* Prime output buffer with a gzip header. */
	t = time(NULL);
	data->compressed[0] = 0x1f; /* GZip signature bytes */
	data->compressed[1] = 0x8b;
	data->compressed[2] = 0x08; /* "Deflate" compression */
	data->compressed[3] = 0; /* No options */
	data->compressed[4] = (t)&0xff;  /* Timestamp */
	data->compressed[5] = (t>>8)&0xff;
	data->compressed[6] = (t>>16)&0xff;
	data->compressed[7] = (t>>24)&0xff;
	data->compressed[8] = 0; /* No deflate options */
	data->compressed[9] = 3; /* OS=Unix */
	data->stream.next_out += 10;
	data->stream.avail_out -= 10;

	f->write = transform_compressor_gzip_write;

	/* Initialize compression library. */
	ret = deflateInit2(&(data->stream),
	    data->compression_level,
	    Z_DEFLATED,
	    -15 /* < 0 to suppress zlib header */,
	    8,
	    Z_DEFAULT_STRATEGY);

	if (ret == Z_OK) {
		f->data = data;
		return (TRANSFORM_OK);
	}

	/* Library setup failed: clean up. */
	transform_set_error(f->transform, TRANSFORM_ERRNO_MISC, "Internal error "
	    "initializing compression library");

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case Z_STREAM_ERROR:
		transform_set_error(f->transform, TRANSFORM_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		transform_set_error(f->transform, ENOMEM, "Internal error initializing "
		    "compression library");
		break;
	case Z_VERSION_ERROR:
		transform_set_error(f->transform, TRANSFORM_ERRNO_MISC,
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
transform_compressor_gzip_options(struct transform_write_filter *f, const char *key,
    const char *value)
{
	struct private_data *data = (struct private_data *)f->data;

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
static int
transform_compressor_gzip_write(struct transform_write_filter *f, const void *buff,
    size_t length)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret;

	/* Update statistics */
	data->crc = crc32(data->crc, (const Bytef *)buff, length);
	data->total_in += length;

	/* Compress input data to output buffer */
	SET_NEXT_IN(data, buff);
	data->stream.avail_in = length;
	if ((ret = drive_compressor(f, data, 0)) != TRANSFORM_OK)
		return (ret);

	return (TRANSFORM_OK);
}

/*
 * Finish the compression...
 */
static int
transform_compressor_gzip_close(struct transform_write_filter *f)
{
	unsigned char trailer[8];
	struct private_data *data = (struct private_data *)f->data;
	int ret, r1;

	/* Finish compression cycle */
	ret = drive_compressor(f, data, 1);
	if (ret == TRANSFORM_OK) {
		/* Write the last compressed data. */
		ret = __transform_write_filter(f->next_filter,
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
		ret = __transform_write_filter(f->next_filter, trailer, 8);
	}

	switch (deflateEnd(&(data->stream))) {
	case Z_OK:
		break;
	default:
		transform_set_error(f->transform, TRANSFORM_ERRNO_MISC,
		    "Failed to clean up compressor");
		ret = TRANSFORM_FATAL;
	}
	r1 = __transform_write_close_filter(f->next_filter);
	return (r1 < ret ? r1 : ret);
}

static int
transform_compressor_gzip_free(struct transform_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	free(data->compressed);
	free(data);
	f->data = NULL;
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
drive_compressor(struct transform_write_filter *f,
    struct private_data *data, int finishing)
{
	int ret;

	for (;;) {
		if (data->stream.avail_out == 0) {
			ret = __transform_write_filter(f->next_filter,
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
			transform_set_error(f->transform, TRANSFORM_ERRNO_MISC,
			    "GZip compression failed:"
			    " deflate() call returned status %d",
			    ret);
			return (TRANSFORM_FATAL);
		}
	}
}

#endif /* HAVE_ZLIB_H */
