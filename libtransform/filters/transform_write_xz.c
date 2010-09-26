/*-
 * Copyright (c) 2009,2010 Michihiro NAKAJIMA
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
#include "transform_endian.h"
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

int
transform_write_add_filter_lzip(struct transform *a)
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
	int64_t		 total_out;
	/* the CRC32 value of uncompressed data for lzip */
	uint32_t	 crc32;
};

static int	transform_compressor_xz_options(struct transform_write_filter *,
		    const char *, const char *);
static int	transform_compressor_xz_open(struct transform_write_filter *);
static int	transform_compressor_xz_write(struct transform_write_filter *,
	const void *, const void *, size_t);
static int	transform_compressor_xz_close(struct transform_write_filter *);
static int	transform_compressor_xz_free(struct transform *, void *);
static int	drive_compressor(struct transform_write_filter *,
		    struct private_data *, int finishing);

struct option_value {
	uint32_t dict_size;
	uint32_t nice_len;
	lzma_match_finder mf;
};
static const struct option_value option_values[] = {
	{ 1 << 16, 32, LZMA_MF_HC3},
	{ 1 << 20, 32, LZMA_MF_HC3},
	{ 3 << 19, 32, LZMA_MF_HC4},
	{ 1 << 21, 32, LZMA_MF_BT4},
	{ 3 << 20, 32, LZMA_MF_BT4},
	{ 1 << 22, 32, LZMA_MF_BT4},
	{ 1 << 23, 64, LZMA_MF_BT4},
	{ 1 << 24, 64, LZMA_MF_BT4},
	{ 3 << 23, 64, LZMA_MF_BT4},
	{ 1 << 25, 64, LZMA_MF_BT4}
};

static int
common_setup(struct transform *t, const char *name, int code)
{
	struct private_data *data = calloc(1, sizeof(*data));
	if (data == NULL) {
		transform_set_error(t, ENOMEM, "Out of memory");
		return (TRANSFORM_FATAL);
	}
	data->compression_level = LZMA_PRESET_DEFAULT;

	if (TRANSFORM_OK != transform_write_add_filter(t,
		data, name, code,
		transform_compressor_xz_options,
		transform_compressor_xz_open,
		transform_compressor_xz_write,
		transform_compressor_xz_close,
		transform_compressor_xz_free,
		0)) {
		free(data);
		return (TRANSFORM_FATAL);
	}
	return (TRANSFORM_OK);
}

/*
 * Add an xz compression filter to this write handle.
 */
int
transform_write_add_filter_xz(struct transform *_a)
{
	transform_check_magic(_a, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_add_filter_xz");
	return  common_setup(_a, "xz", TRANSFORM_FILTER_XZ);
}

/* LZMA is handled identically, we just need a different compression
 * code set.  (The liblzma setup looks at the code to determine
 * the one place that XZ and LZMA require different handling.) */
int
transform_write_add_filter_lzma(struct transform *_a)
{
	transform_check_magic(_a, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_add_filter_lzma");
	return common_setup(_a, "lzma", TRANSFORM_FILTER_LZMA);
}

int
transform_write_add_filter_lzip(struct transform *_a)
{
	transform_check_magic(_a, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_add_filter_lzip");
	return common_setup(_a, "lzip", TRANSFORM_FILTER_LZIP);
}

static int
transform_compressor_xz_init_stream(struct transform_write_filter *f,
    struct private_data *data)
{
	static const lzma_stream lzma_stream_init_data = LZMA_STREAM_INIT;
	int ret;

	data->stream = lzma_stream_init_data;
	data->stream.next_out = data->compressed;
	data->stream.avail_out = data->compressed_buffer_size;
	if (f->base.code == TRANSFORM_FILTER_XZ)
		ret = lzma_stream_encoder(&(data->stream),
		    data->lzmafilters, LZMA_CHECK_CRC64);
	else if (f->base.code == TRANSFORM_FILTER_LZMA)
		ret = lzma_alone_encoder(&(data->stream), &data->lzma_opt);
	else {	/* TRANSFORM_FILTER_LZIP */
		int dict_size = data->lzma_opt.dict_size;
		int ds, log2dic, wedges;

		/* Calculate a coded dictionary size */
		if (dict_size < (1 << 12) || dict_size > (1 << 27)) {
			transform_set_error(f->base.transform, TRANSFORM_ERRNO_MISC,
			    "Unacceptable dictionary dize for lzip: %d",
			    dict_size);
			return (TRANSFORM_FATAL);
		}
		for (log2dic = 27; log2dic >= 12; log2dic--) {
			if (dict_size & (1 << log2dic))
				break;
		}
		if (dict_size > (1 << log2dic)) {
			log2dic++;
			wedges =
			    ((1 << log2dic) - dict_size) / (1 << (log2dic - 4));
		} else
			wedges = 0;
		ds = ((wedges << 5) & 0xe0) | (log2dic & 0x1f);

		data->crc32 = 0;
		/* Make a header */
		data->compressed[0] = 0x4C;
		data->compressed[1] = 0x5A;
		data->compressed[2] = 0x49;
		data->compressed[3] = 0x50;
		data->compressed[4] = 1;/* Version */
		data->compressed[5] = (unsigned char)ds;
		data->stream.next_out += 6;
		data->stream.avail_out -= 6;

		ret = lzma_raw_encoder(&(data->stream), data->lzmafilters);
	}
	if (ret == LZMA_OK)
		return (TRANSFORM_OK);

	switch (ret) {
	case LZMA_MEM_ERROR:
		transform_set_error(f->base.transform, ENOMEM,
		    "Internal error initializing compression library: "
		    "Cannot allocate memory");
		break;
	default:
		transform_set_error(f->base.transform, TRANSFORM_ERRNO_MISC,
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
	struct private_data *data = f->base.data;
	int ret;

	ret = __transform_write_open_filter(f->base.upstream.write);
	if (ret != TRANSFORM_OK)
		return (ret);

	if (data->compressed == NULL) {
		data->compressed_buffer_size = 65536;
		data->compressed
		    = (unsigned char *)malloc(data->compressed_buffer_size);
		if (data->compressed == NULL) {
			transform_set_error(f->base.transform, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (TRANSFORM_FATAL);
		}
	}

	/* Initialize compression library. */
	if (f->base.code == TRANSFORM_FILTER_LZIP) {
		const struct option_value *val =
		    &option_values[data->compression_level];

		data->lzma_opt.dict_size = val->dict_size;
		data->lzma_opt.preset_dict = NULL;
		data->lzma_opt.preset_dict_size = 0;
		data->lzma_opt.lc = LZMA_LC_DEFAULT;
		data->lzma_opt.lp = LZMA_LP_DEFAULT;
		data->lzma_opt.pb = LZMA_PB_DEFAULT;
		data->lzma_opt.mode =
		    data->compression_level<= 2? LZMA_MODE_FAST:LZMA_MODE_NORMAL;
		data->lzma_opt.nice_len = val->nice_len;
		data->lzma_opt.mf = val->mf;
		data->lzma_opt.depth = 0;
		data->lzmafilters[0].id = LZMA_FILTER_LZMA1;
		data->lzmafilters[0].options = &data->lzma_opt;
		data->lzmafilters[1].id = LZMA_VLI_UNKNOWN;/* Terminate */
	} else {
		if (lzma_lzma_preset(&data->lzma_opt, data->compression_level)) {
			transform_set_error(f->base.transform, TRANSFORM_ERRNO_MISC,
			    "Internal error initializing compression library");
		}
		data->lzmafilters[0].id = LZMA_FILTER_LZMA2;
		data->lzmafilters[0].options = &data->lzma_opt;
		data->lzmafilters[1].id = LZMA_VLI_UNKNOWN;/* Terminate */
	}
	ret = transform_compressor_xz_init_stream(f, data);
	if (ret == LZMA_OK) {
		f->base.data = data;
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
	struct private_data *data = (struct private_data *)f->base.data;

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
	const void *filter_data, const void *buff, size_t length)
{
	struct private_data *data = (struct private_data *)filter_data;
	int ret;

	/* Update statistics */
	data->total_in += length;
	if (f->base.code == TRANSFORM_FILTER_LZIP)
		data->crc32 = lzma_crc32(buff, length, data->crc32);

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
	struct private_data *data = (struct private_data *)f->base.data;
	int ret, r1;

	ret = drive_compressor(f, data, 1);
	if (ret == TRANSFORM_OK) {
		data->total_out +=
		    data->compressed_buffer_size - data->stream.avail_out;
		ret = __transform_write_filter(f->base.upstream.write,
		    data->compressed,
		    data->compressed_buffer_size - data->stream.avail_out);
		if (f->base.code == TRANSFORM_FILTER_LZIP && ret == TRANSFORM_OK) {
			transform_le32enc(data->compressed, data->crc32);
			transform_le64enc(data->compressed+4, data->total_in);
			transform_le64enc(data->compressed+12, data->total_out + 20);
			ret = __transform_write_filter(f->base.upstream.write,
			    data->compressed, 20);
		}
	}
	lzma_end(&(data->stream));
	r1 = __transform_write_close_filter(f->base.upstream.write);
	return (r1 < ret ? r1 : ret);
}

static int
transform_compressor_xz_free(struct transform *t, void *_data)
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
drive_compressor(struct transform_write_filter *f,
    struct private_data *data, int finishing)
{
	int ret;

	for (;;) {
		if (data->stream.avail_out == 0) {
			data->total_out += data->compressed_buffer_size;
			ret = __transform_write_filter(f->base.upstream.write,
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
			transform_set_error(f->base.transform, TRANSFORM_ERRNO_MISC,
			    "lzma compression data error");
			return (TRANSFORM_FATAL);
		case LZMA_MEMLIMIT_ERROR:
			transform_set_error(f->base.transform, ENOMEM,
			    "lzma compression error: "
			    "%ju MiB would have been needed",
			    (lzma_memusage(&(data->stream)) + 1024 * 1024 -1)
			    / (1024 * 1024));
			return (TRANSFORM_FATAL);
		default:
			/* Any other return value indicates an error. */
			transform_set_error(f->base.transform, TRANSFORM_ERRNO_MISC,
			    "lzma compression failed:"
			    " lzma_code() call returned status %d",
			    ret);
			return (TRANSFORM_FATAL);
		}
	}
}

#endif /* HAVE_LZMA_H */
