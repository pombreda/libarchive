/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * Copyright (c) 2003-2010 Tim Kientzle
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

__FBSDID("$FreeBSD: head/lib/libtransform/transform_write_set_compression_xz.c 201108 2009-12-28 03:28:21Z kientzle $");

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
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif

#include "transform.h"
#include "transform_private.h"
#include "transform_write_private.h"

#ifndef HAVE_LZMA_H
int
transform_write_add_filter_xz(struct transform *a)
{
	transform_set_error(a, TRANSFORM_ERRNO_MISC,
	    "xz compression not supported on this platform");
	return (TRANSFORM_FATAL);
}

int
transform_write_add_filter_lzma(struct transform *a)
{
	transform_set_error(a, TRANSFORM_ERRNO_MISC,
	    "lzma compression not supported on this platform");
	return (TRANSFORM_FATAL);
}
#else
/* Don't compile this if we don't have liblzma. */

struct private_data {
	int		 compression_level;
	lzma_stream	 stream;
	lzma_filter	 lzmafilters[2];
	lzma_options_lzma lzma_opt;
	int64_t		 total_in;
	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
};

static int	transform_compressor_xz_options(struct transform_write_filter *,
		    const char *, const char *);
static int	transform_compressor_xz_open(struct transform_write_filter *);
static int	transform_compressor_xz_write(struct transform_write_filter *,
		    const void *, size_t);
static int	transform_compressor_xz_close(struct transform_write_filter *);
static int	transform_compressor_xz_free(struct transform_write_filter *);
static int	drive_compressor(struct transform_write_filter *,
		    struct private_data *, int finishing);


static int
common_setup(struct transform_write_filter *f)
{
	struct private_data *data;
	struct transform_write *a = (struct transform_write *)f->transform;
	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		transform_set_error(&a->transform, ENOMEM, "Out of memory");
		return (TRANSFORM_FATAL);
	}
	f->data = data;
	data->compression_level = LZMA_PRESET_DEFAULT;
	f->open = &transform_compressor_xz_open;
	f->close = transform_compressor_xz_close;
	f->free = transform_compressor_xz_free;
	f->options = &transform_compressor_xz_options;
	return (TRANSFORM_OK);
}

/*
 * Add an xz compression filter to this write handle.
 */
int
transform_write_add_filter_xz(struct transform *_a)
{
	struct transform_write_filter *f;
	int r;

	transform_check_magic(_a, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_add_filter_xz");
	f = __transform_write_allocate_filter(_a);
	r = common_setup(f);
	if (r == TRANSFORM_OK) {
		f->code = TRANSFORM_FILTER_XZ;
		f->name = "xz";
	}
	return (r);
}

/* LZMA is handled identically, we just need a different compression
 * code set.  (The liblzma setup looks at the code to determine
 * the one place that XZ and LZMA require different handling.) */
int
transform_write_add_filter_lzma(struct transform *_a)
{
	struct transform_write_filter *f;
	int r;

	transform_check_magic(_a, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_add_filter_lzma");
	f = __transform_write_allocate_filter(_a);
	r = common_setup(f);
	if (r == TRANSFORM_OK) {
		f->code = TRANSFORM_FILTER_LZMA;
		f->name = "lzma";
	}
	return (r);
}

static int
transform_compressor_xz_init_stream(struct transform_write_filter *f,
    struct private_data *data)
{
	int ret;

	data->stream = (lzma_stream)LZMA_STREAM_INIT;
	data->stream.next_out = data->compressed;
	data->stream.avail_out = data->compressed_buffer_size;
	if (f->code == TRANSFORM_FILTER_XZ)
		ret = lzma_stream_encoder(&(data->stream),
		    data->lzmafilters, LZMA_CHECK_CRC64);
	else
		ret = lzma_alone_encoder(&(data->stream), &data->lzma_opt);
	if (ret == LZMA_OK)
		return (TRANSFORM_OK);

	switch (ret) {
	case LZMA_MEM_ERROR:
		transform_set_error(f->transform, ENOMEM,
		    "Internal error initializing compression library: "
		    "Cannot allocate memory");
		break;
	default:
		transform_set_error(f->transform, TRANSFORM_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "It's a bug in liblzma");
		break;
	}
	return (TRANSFORM_FATAL);
}

/*
 * Setup callback.
 */
static int
transform_compressor_xz_open(struct transform_write_filter *f)
{
	struct private_data *data = f->data;
	int ret;

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

	f->write = transform_compressor_xz_write;

	/* Initialize compression library. */
	if (lzma_lzma_preset(&data->lzma_opt, data->compression_level)) {
		transform_set_error(f->transform, TRANSFORM_ERRNO_MISC,
		    "Internal error initializing compression library");
	}
	data->lzmafilters[0].id = LZMA_FILTER_LZMA2;
	data->lzmafilters[0].options = &data->lzma_opt;
	data->lzmafilters[1].id = LZMA_VLI_UNKNOWN;/* Terminate */
	ret = transform_compressor_xz_init_stream(f, data);
	if (ret == LZMA_OK) {
		f->data = data;
		return (0);
	}
	return (TRANSFORM_FATAL);
}

/*
 * Set write options.
 */
static int
transform_compressor_xz_options(struct transform_write_filter *f,
    const char *key, const char *value)
{
	struct private_data *data = (struct private_data *)f->data;

	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0')
			return (TRANSFORM_WARN);
		data->compression_level = value[0] - '0';
		if (data->compression_level > 6)
			data->compression_level = 6;
		return (TRANSFORM_OK);
	}

	return (TRANSFORM_WARN);
}

/*
 * Write data to the compressed stream.
 */
static int
transform_compressor_xz_write(struct transform_write_filter *f,
    const void *buff, size_t length)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret;

	/* Update statistics */
	data->total_in += length;

	/* Compress input data to output buffer */
	data->stream.next_in = buff;
	data->stream.avail_in = length;
	if ((ret = drive_compressor(f, data, 0)) != TRANSFORM_OK)
		return (ret);

	return (TRANSFORM_OK);
}


/*
 * Finish the compression...
 */
static int
transform_compressor_xz_close(struct transform_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret, r1;

	ret = drive_compressor(f, data, 1);
	if (ret == TRANSFORM_OK) {
		ret = __transform_write_filter(f->next_filter,
		    data->compressed,
		    data->compressed_buffer_size - data->stream.avail_out);
	}
	lzma_end(&(data->stream));
	r1 = __transform_write_close_filter(f->next_filter);
	return (r1 < ret ? r1 : ret);
}

static int
transform_compressor_xz_free(struct transform_write_filter *f)
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

		ret = lzma_code(&(data->stream),
		    finishing ? LZMA_FINISH : LZMA_RUN );

		switch (ret) {
		case LZMA_OK:
			/* In non-finishing case, check if compressor
			 * consumed everything */
			if (!finishing && data->stream.avail_in == 0)
				return (TRANSFORM_OK);
			/* In finishing case, this return always means
			 * there's more work */
			break;
		case LZMA_STREAM_END:
			/* This return can only occur in finishing case. */
			if (finishing)
				return (TRANSFORM_OK);
			transform_set_error(f->transform, TRANSFORM_ERRNO_MISC,
			    "lzma compression data error");
			return (TRANSFORM_FATAL);
		case LZMA_MEMLIMIT_ERROR:
			transform_set_error(f->transform, ENOMEM,
			    "lzma compression error: "
			    "%ju MiB would have been needed",
			    (lzma_memusage(&(data->stream)) + 1024 * 1024 -1)
			    / (1024 * 1024));
			return (TRANSFORM_FATAL);
		default:
			/* Any other return value indicates an error. */
			transform_set_error(f->transform, TRANSFORM_ERRNO_MISC,
			    "lzma compression failed:"
			    " lzma_code() call returned status %d",
			    ret);
			return (TRANSFORM_FATAL);
		}
	}
}

#endif /* HAVE_LZMA_H */
