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

__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_support_compression_bzip2.c 201108 2009-12-28 03:28:21Z kientzle $");

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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif

#include "transform.h"

#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
struct private_data {
	bz_stream	 stream;
	char		 valid; /* True = decompressor is initialized */
};

/* Bzip2 filter */
static int	bzip2_filter_read(struct transform *, void *,
	struct transform_read_filter *, const void **, size_t *);
static int	bzip2_filter_close(struct transform *, void *);
#endif

/*
 * Note that we can detect bzip2 transforms even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)  So the bid framework here gets compiled even
 * if bzlib is unavailable.
 */
static int	check_header(struct transform_read_filter *);
static int	bzip2_reader_bid(const void *, struct transform_read_filter *);
static int	bzip2_reader_init(struct transform *, const void *);

int
transform_read_add_bzip2(struct transform *_t)
{
	if (!transform_read_bidder_add(_t, NULL, "bzip2",
		bzip2_reader_bid, bzip2_reader_init, NULL, NULL)) {
		return (TRANSFORM_FATAL);
	}

#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
	return (TRANSFORM_OK);
#else
	transform_set_error(_a, TRANSFORM_ERRNO_MISC,
	    "Using external bunzip2 program");
	return (TRANSFORM_WARN);
#endif
}

int
transform_autodetect_add_bzip2(struct transform_read_bidder *trb)
{
	return (transform_autodetect_add_bidder_create(trb, NULL, "bzip2",
		bzip2_reader_bid, bzip2_reader_init, NULL, NULL));
}

static int
bzip2_reader_bid(const void *bidder_data, struct transform_read_filter *filter)
{
	int ret = check_header(filter);
	if (ret < 0)
		ret = 0;
	return (ret);
}
/*
 * Test whether we can handle this data.
 *
 * This logic return 0 if EOF precludes even a signature
 * -1 is returned if any part of the signature fails.  It
 * also tries to Do The Right Thing if a very short buffer prevents us
 * from verifying as much as we would like.
 */
static int
check_header(struct transform_read_filter *filter)
{
	const unsigned char *buffer;
	ssize_t avail;
	int bits_checked;

	/* Minimal bzip2 transform is 14 bytes. */
	buffer = transform_read_filter_ahead(filter, 14, &avail);
	if (buffer == NULL)
		return (0);

	/* First three bytes must be "BZh" */
	bits_checked = 0;
	if (buffer[0] != 'B' || buffer[1] != 'Z' || buffer[2] != 'h')
		return (-1);
	bits_checked += 24;

	/* Next follows a compression flag which must be an ASCII digit. */
	if (buffer[3] < '1' || buffer[3] > '9')
		return (-1);
	bits_checked += 5;

	/* After BZh[1-9], there must be either a data block
	 * which begins with 0x314159265359 or an end-of-data
	 * marker of 0x177245385090. */
	if (memcmp(buffer + 4, "\x31\x41\x59\x26\x53\x59", 6) == 0)
		bits_checked += 48;
	else if (memcmp(buffer + 4, "\x17\x72\x45\x38\x50\x90", 6) == 0)
		bits_checked += 48;
	else
		return (-1);

	return (bits_checked);
}

#if !defined(HAVE_BZLIB_H) || !defined(BZ_CONFIG_ERROR)

/*
 * If we don't have the library on this system, we can't actually do the
 * decompression.  We can, however, still detect compressed transforms
 * and emit a useful message.
 */
static int
bzip2_reader_init(struct transform *transform, const void *bidder_data)
{
	return (__transform_read_program(transform, "bunzip2",
		"bzip2", TRANSFORM_FILTER_BZIP2));
}


#else

/*
 * Setup the callbacks.
 */
static int
bzip2_reader_init(struct transform *transform, const void *bidder_data)
{
	int ret;
	struct private_data *state;

	state = (struct private_data *)calloc(sizeof(*state), 1);
	if (state == NULL) {
		transform_set_error(transform, ENOMEM,
		    "Can't allocate data for bzip2 decompression");
		free(state);
		return (TRANSFORM_FATAL);
	}

	ret = transform_read_add_new_filter(transform, (void *)state, "bzip2",
		TRANSFORM_FILTER_BZIP2,
		bzip2_filter_read, NULL,
		bzip2_filter_close, NULL, 0);

	if (TRANSFORM_OK != ret) {
		bzip2_filter_close(transform, state);
	}

	return (ret);
}

/*
 * Return the next block of decompressed data.
 */
static int
bzip2_filter_read(struct transform *transform, void *_state,
	struct transform_read_filter *upstream, const void **p, size_t *bytes_read)
{
	struct private_data *state = (struct private_data *)_state;
	size_t decompressed;
	const char *read_buf;
	ssize_t ret;

	/* Empty our output buffer. */
	state->stream.next_out = (void *)*p;
	state->stream.avail_out = *bytes_read;

	/* Try to fill the output buffer. */
	for (;;) {
		if (!state->valid) {
			/* 
			 * might seem special, but the bidder doesn't need bidder data
			 * long term, should restructure to remove the need.
			 */
			ret = check_header(upstream);
			if (ret <= 0) {
				decompressed = (void *)state->stream.next_out
				    - (void *)*p;
				*bytes_read = decompressed;
				if (ret == -1) {
					/* remaining data isn't bzip2 */
					transform_set_error(transform, TRANSFORM_ERRNO_MISC,
						"premature EOF encountered- remaining data isn't a bzip2 stream");
					return (TRANSFORM_PREMATURE_EOF);
				}
				return (TRANSFORM_EOF);
			}
			/* Initialize compression library. */
			ret = BZ2_bzDecompressInit(&(state->stream),
					   0 /* library verbosity */,
					   0 /* don't use low-mem algorithm */);

			/* If init fails, try low-memory algorithm instead. */
			if (ret == BZ_MEM_ERROR)
				ret = BZ2_bzDecompressInit(&(state->stream),
					   0 /* library verbosity */,
					   1 /* do use low-mem algo */);

			if (ret != BZ_OK) {
				const char *detail = NULL;
				int err = TRANSFORM_ERRNO_MISC;
				switch (ret) {
				case BZ_PARAM_ERROR:
					detail = "invalid setup parameter";
					break;
				case BZ_MEM_ERROR:
					err = ENOMEM;
					detail = "out of memory";
					break;
				case BZ_CONFIG_ERROR:
					detail = "mis-compiled library";
					break;
				}
				transform_set_error(transform, err,
				    "Internal error initializing decompressor%s%s",
				    detail == NULL ? "" : ": ",
				    detail);
				return (TRANSFORM_FATAL);
			}
			state->valid = 1;
		}

		/* stream.next_in is really const, but bzlib
		 * doesn't declare it so. <sigh> */
		read_buf =
		    transform_read_filter_ahead(upstream, 1, &ret);
		if (read_buf == NULL)
			return (TRANSFORM_FATAL);
		state->stream.next_in = (char *)(uintptr_t)read_buf;
		state->stream.avail_in = ret;
		/* There is no more data, return whatever we have. */
		if (ret == 0) {
			*bytes_read = (void *)state->stream.next_out
			    - (void *)*p;
			return (TRANSFORM_EOF);
		}

		/* Decompress as much as we can in one pass. */
		ret = BZ2_bzDecompress(&(state->stream));
		transform_read_filter_consume(upstream,
		    state->stream.next_in - read_buf);

		switch (ret) {
		case BZ_STREAM_END: /* Found end of stream. */
			switch (BZ2_bzDecompressEnd(&(state->stream))) {
			case BZ_OK:
				break;
			default:
				transform_set_error(transform,
					  TRANSFORM_ERRNO_MISC,
					  "Failed to clean up decompressor");
				return (TRANSFORM_FATAL);
			}
			state->valid = 0;
			/* FALLTHROUGH */
		case BZ_OK: /* Decompressor made some progress. */
			/* If we filled our buffer, update stats and return. */
			if (state->stream.avail_out == 0) {
				*bytes_read = (void *)state->stream.next_out
				    - (void *)*p;
				return (TRANSFORM_OK);
			}
			break;
		default: /* Return an error. */
			transform_set_error(transform,
			    TRANSFORM_ERRNO_MISC, "bzip decompression failed");
			return (TRANSFORM_FATAL);
		}
	}
}

/*
 * Clean up the decompressor.
 */
static int
bzip2_filter_close(struct transform *transform, void *_state)
{
	struct private_data *state = (struct private_data *)_state;
	int ret = TRANSFORM_OK;

	if (state->valid) {
		switch (BZ2_bzDecompressEnd(&state->stream)) {
		case BZ_OK:
			break;
		default:
			transform_set_error(transform,
					  TRANSFORM_ERRNO_MISC,
					  "Failed to clean up decompressor");
			ret = TRANSFORM_FATAL;
		}
		state->valid = 0;
	}

	free(state);
	return (ret);
}

#endif /* HAVE_BZLIB_H && BZ_CONFIG_ERROR */