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

__FBSDID("$FreeBSD: head/lib/libarchive/transform_write_set_compression_xz.c 201108 2009-12-28 03:28:21Z kientzle $");

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

#if ARCHIVE_VERSION_NUMBER < 4000000
int
transform_write_set_compression_lzma(struct transform *a)
{
	__transform_write_filters_free(a);
	return (transform_write_add_filter_lzma(a));
}

int
transform_write_set_compression_xz(struct transform *a)
{
	__transform_write_filters_free(a);
	return (transform_write_add_filter_xz(a));
}
#endif

#ifndef HAVE_LZMA_H
int
transform_write_add_filter_xz(struct transform *a)
{
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "xz compression not supported on this platform");
	return (ARCHIVE_FATAL);
}

int
transform_write_add_filter_lzma(struct transform *a)
{
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "lzma compression not supported on this platform");
	return (ARCHIVE_FATAL);
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

static int	archive_compressor_xz_options(struct transform_write_filter *,
		    const char *, const char *);
static int	archive_compressor_xz_open(struct transform_write_filter *);
static int	archive_compressor_xz_write(struct transform_write_filter *,
		    const void *, size_t);
static int	archive_compressor_xz_close(struct transform_write_filter *);
static int	archive_compressor_xz_free(struct transform_write_filter *);
static int	drive_compressor(struct transform_write_filter *,
		    struct private_data *, int finishing);


static int
common_setup(struct transform_write_filter *f)
{
	struct private_data *data;
	struct transform_write *a = (struct transform_write *)f->archive;
	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	f->data = data;
	data->compression_level = LZMA_PRESET_DEFAULT;
	f->open = &archive_compressor_xz_open;
	f->close = archive_compressor_xz_close;
	f->free = archive_compressor_xz_free;
	f->options = &archive_compressor_xz_options;
	return (ARCHIVE_OK);
}

/*
 * Add an xz compression filter to this write handle.
 */
int
transform_write_add_filter_xz(struct transform *_a)
{
	struct transform_write_filter *f;
	int r;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "transform_write_add_filter_xz");
	f = __transform_write_allocate_filter(_a);
	r = common_setup(f);
	if (r == ARCHIVE_OK) {
		f->code = ARCHIVE_FILTER_XZ;
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

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "transform_write_add_filter_lzma");
	f = __transform_write_allocate_filter(_a);
	r = common_setup(f);
	if (r == ARCHIVE_OK) {
		f->code = ARCHIVE_FILTER_LZMA;
		f->name = "lzma";
	}
	return (r);
}

static int
archive_compressor_xz_init_stream(struct transform_write_filter *f,
    struct private_data *data)
{
	int ret;

	data->stream = (lzma_stream)LZMA_STREAM_INIT;
	data->stream.next_out = data->compressed;
	data->stream.avail_out = data->compressed_buffer_size;
	if (f->code == ARCHIVE_FILTER_XZ)
		ret = lzma_stream_encoder(&(data->stream),
		    data->lzmafilters, LZMA_CHECK_CRC64);
	else
		ret = lzma_alone_encoder(&(data->stream), &data->lzma_opt);
	if (ret == LZMA_OK)
		return (ARCHIVE_OK);

	switch (ret) {
	case LZMA_MEM_ERROR:
		archive_set_error(f->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "Cannot allocate memory");
		break;
	default:
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "It's a bug in liblzma");
		break;
	}
	return (ARCHIVE_FATAL);
}

/*
 * Setup callback.
 */
static int
archive_compressor_xz_open(struct transform_write_filter *f)
{
	struct private_data *data = f->data;
	int ret;

	ret = __transform_write_open_filter(f->next_filter);
	if (ret != ARCHIVE_OK)
		return (ret);

	if (data->compressed == NULL) {
		data->compressed_buffer_size = 65536;
		data->compressed
		    = (unsigned char *)malloc(data->compressed_buffer_size);
		if (data->compressed == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (ARCHIVE_FATAL);
		}
	}

	f->write = archive_compressor_xz_write;

	/* Initialize compression library. */
	if (lzma_lzma_preset(&data->lzma_opt, data->compression_level)) {
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library");
	}
	data->lzmafilters[0].id = LZMA_FILTER_LZMA2;
	data->lzmafilters[0].options = &data->lzma_opt;
	data->lzmafilters[1].id = LZMA_VLI_UNKNOWN;/* Terminate */
	ret = archive_compressor_xz_init_stream(f, data);
	if (ret == LZMA_OK) {
		f->data = data;
		return (0);
	}
	return (ARCHIVE_FATAL);
}

/*
 * Set write options.
 */
static int
archive_compressor_xz_options(struct transform_write_filter *f,
    const char *key, const char *value)
{
	struct private_data *data = (struct private_data *)f->data;

	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0')
			return (ARCHIVE_WARN);
		data->compression_level = value[0] - '0';
		if (data->compression_level > 6)
			data->compression_level = 6;
		return (ARCHIVE_OK);
	}

	return (ARCHIVE_WARN);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_xz_write(struct transform_write_filter *f,
    const void *buff, size_t length)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret;

	/* Update statistics */
	data->total_in += length;

	/* Compress input data to output buffer */
	data->stream.next_in = buff;
	data->stream.avail_in = length;
	if ((ret = drive_compressor(f, data, 0)) != ARCHIVE_OK)
		return (ret);

	return (ARCHIVE_OK);
}


/*
 * Finish the compression...
 */
static int
archive_compressor_xz_close(struct transform_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret, r1;

	ret = drive_compressor(f, data, 1);
	if (ret == ARCHIVE_OK) {
		ret = __transform_write_filter(f->next_filter,
		    data->compressed,
		    data->compressed_buffer_size - data->stream.avail_out);
	}
	lzma_end(&(data->stream));
	r1 = __transform_write_close_filter(f->next_filter);
	return (r1 < ret ? r1 : ret);
}

static int
archive_compressor_xz_free(struct transform_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	free(data->compressed);
	free(data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

/*
 * Utility function to push input data through compressor,
 * writing full output blocks as necessary.
 *
 * Note that this handles both the regular write case (finishing ==
 * false) and the end-of-archive case (finishing == true).
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
			if (ret != ARCHIVE_OK)
				return (ARCHIVE_FATAL);
			data->stream.next_out = data->compressed;
			data->stream.avail_out = data->compressed_buffer_size;
		}

		/* If there's nothing to do, we're done. */
		if (!finishing && data->stream.avail_in == 0)
			return (ARCHIVE_OK);

		ret = lzma_code(&(data->stream),
		    finishing ? LZMA_FINISH : LZMA_RUN );

		switch (ret) {
		case LZMA_OK:
			/* In non-finishing case, check if compressor
			 * consumed everything */
			if (!finishing && data->stream.avail_in == 0)
				return (ARCHIVE_OK);
			/* In finishing case, this return always means
			 * there's more work */
			break;
		case LZMA_STREAM_END:
			/* This return can only occur in finishing case. */
			if (finishing)
				return (ARCHIVE_OK);
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "lzma compression data error");
			return (ARCHIVE_FATAL);
		case LZMA_MEMLIMIT_ERROR:
			archive_set_error(f->archive, ENOMEM,
			    "lzma compression error: "
			    "%ju MiB would have been needed",
			    (lzma_memusage(&(data->stream)) + 1024 * 1024 -1)
			    / (1024 * 1024));
			return (ARCHIVE_FATAL);
		default:
			/* Any other return value indicates an error. */
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "lzma compression failed:"
			    " lzma_code() call returned status %d",
			    ret);
			return (ARCHIVE_FATAL);
		}
	}
}

#endif /* HAVE_LZMA_H */
