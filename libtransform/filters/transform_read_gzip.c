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

__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_support_compression_gzip.c 201082 2009-12-28 02:05:28Z kientzle $");


#ifdef HAVE_ERRNO_H
#include <errno.h>
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
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "transform.h"

#ifdef HAVE_ZLIB_H
struct private_data {
	z_stream	 stream;
	char		 in_stream;
	int64_t		 total_out;
	unsigned long	 crc;
};

/* Gzip Filter. */
static int	gzip_filter_read(struct transform *, void *,
	struct transform_read_filter *, const void **, size_t *);
static int	gzip_filter_close(struct transform *, void *);
#endif

/*
 * Note that we can detect gzip transforms even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)  So the bid framework here gets compiled even
 * if zlib is unavailable.
 *
 * TODO: If zlib is unavailable, gzip_bidder_init() should
 * use the compress_program framework to try to fire up an external
 * gunzip program.
 */
static int	gzip_bidder_bid(const void *, struct transform_read_filter *);
static int	gzip_bidder_init(struct transform *, const void *);

int
transform_read_add_gzip(struct transform *_t)
{
	if (NULL == transform_read_bidder_add(_t, NULL, "gzip", gzip_bidder_bid,
		gzip_bidder_init, NULL, NULL)) {
		return (TRANSFORM_FATAL);
	}

	/* Signal the extent of gzip support with the return value here. */
#if HAVE_ZLIB_H
	return (TRANSFORM_OK);
#else
	transform_set_error(_t, TRANSFORM_ERRNO_MISC,
	    "Using external gunzip program");
	return (TRANSFORM_WARN);
#endif
}

int
transform_autodetect_add_gzip(struct transform_read_bidder *trb)
{
	return (transform_autodetect_add_bidder_create(trb, NULL, "gzip",
		gzip_bidder_bid, gzip_bidder_init, NULL, NULL));
}	

/*
 * Read and verify the header.
 *
 * Returns zero if there isn't enough data for a stream.
 * Returns -1 if the header couldn't be validated (meaning it's likely not gzip)
 * else returns number of bytes in header.  If pbits is non-NULL, it receives a
 * count of bits verified, suitable for use by bidder.
 */
static int
peek_at_header(struct transform_read_filter *filter, int *pbits)
{
	const unsigned char *p;
	ssize_t avail, len;
	int bits = 0;
	int header_flags;

	/* Start by looking at the first ten bytes of the header, which
	 * is all fixed layout. */
	len = 10;
	p = transform_read_filter_ahead(filter, len, &avail);
	if (p == NULL || avail == 0)
		return (0);
	if (p[0] != 037)
		return (-1);
	bits += 8;
	if (p[1] != 0213)
		return (-1);
	bits += 8;
	if (p[2] != 8) /* We only support deflation. */
		return (-1);
	bits += 8;
	if ((p[3] & 0xE0)!= 0)	/* No reserved flags set. */
		return (-1);
	bits += 3;
	header_flags = p[3];
	/* Bytes 4-7 are mod time. */
	/* Byte 8 is deflate flags. */
	/* XXXX TODO: return deflate flags back to consume_header for use
	   in initializing the decompressor. */
	/* Byte 9 is OS. */

	/* Optional extra data:  2 byte length plus variable body. */
	if (header_flags & 4) {
		p = transform_read_filter_ahead(filter, len + 2, &avail);
		if (p == NULL)
			return (-1);
		len += ((int)p[len + 1] << 8) | (int)p[len];
		len += 2;
	}

	/* Null-terminated optional filename. */
	if (header_flags & 8) {
		do {
			++len;
			if (avail < len)
				p = transform_read_filter_ahead(filter,
				    len, &avail);
			if (p == NULL)
				return (-1);
		} while (p[len - 1] != 0);
	}

	/* Null-terminated optional comment. */
	if (header_flags & 16) {
		do {
			++len;
			if (avail < len)
				p = transform_read_filter_ahead(filter,
				    len, &avail);
			if (p == NULL)
				return (-1);
		} while (p[len - 1] != 0);
	}

	/* Optional header CRC */
	if ((header_flags & 2)) {
		p = transform_read_filter_ahead(filter, len + 2, &avail);
		if (p == NULL)
			return (-1);
#if 0
	int hcrc = ((int)p[len + 1] << 8) | (int)p[len];
	int crc = /* XXX TODO: Compute header CRC. */;
	if (crc != hcrc)
		return (-1);
	bits += 16;
#endif
		len += 2;
	}

	if (pbits != NULL)
		*pbits = bits;
	return (len);
}

/*
 * Bidder just verifies the header and returns the number of verified bits.
 */
static int
gzip_bidder_bid(const void *_data, struct transform_read_filter *filter)
{
	int bits_checked;

	/* -1 means "it's most definitely not gzip" */
	if (peek_at_header(filter, &bits_checked) != -1)
		return (bits_checked);
	return (0);
}


#ifndef HAVE_ZLIB_H

/*
 * If we don't have the library on this system, we can't do the
 * decompression directly.  We can, however, try to run gunzip
 * in case that's available.
 */
static int
gzip_bidder_init(struct transform *transform, const void *bidder_data)
{
	return (__transform_read_program(transform, "gunzip",
		"gzip", TRANSFORM_FILTER_GZIP));
}

#else

/*
 * Initialize the filter object.
 */
static int
gzip_bidder_init(struct transform *transform, const void *bidder_data)
{
	struct private_data *state;
	int ret;

	state = (struct private_data *)calloc(sizeof(*state), 1);
	if (state == NULL) {
		free(state);
		transform_set_error(transform, ENOMEM,
		    "Can't allocate data for gzip decompression");
		return (TRANSFORM_FATAL);
	}
	state->in_stream = 0; /* We're not actually within a stream yet. */

	ret = transform_read_add_new_filter(transform, (void *)state,
		"gzip", TRANSFORM_FILTER_GZIP,
		gzip_filter_read, NULL, gzip_filter_close, NULL, 0);

	if (TRANSFORM_OK != ret) {
		gzip_filter_close(transform, state);
	}
	return (ret);
}

static int
consume_header(struct transform *transform, struct private_data *state,
	struct transform_read_filter *upstream)
{
	ssize_t avail;
	size_t len;
	int ret;

	/* If this is a real header, consume it. */
	len = peek_at_header(upstream, NULL);
	if (len == 0) {
		return (TRANSFORM_EOF);
	} else if (len == -1) {
		transform_set_error(transform, TRANSFORM_ERRNO_MISC,
			"premature end of file was encountered- data remains, but isn't a gzip stream");
		return (TRANSFORM_PREMATURE_EOF);
	}
	transform_read_filter_consume(upstream, len);

	/* Initialize CRC accumulator. */
	state->crc = crc32(0L, NULL, 0);

	/* Initialize compression library. */
	state->stream.next_in = (unsigned char *)(uintptr_t)
	    transform_read_filter_ahead(upstream, 1, &avail);
	state->stream.avail_in = avail;
	ret = inflateInit2(&(state->stream),
	    -15 /* Don't check for zlib header */);

	/* Decipher the error code. */
	switch (ret) {
	case Z_OK:
		state->in_stream = 1;
		return (TRANSFORM_OK);
	case Z_STREAM_ERROR:
		transform_set_error(transform,
		    TRANSFORM_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		transform_set_error(transform, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	case Z_VERSION_ERROR:
		transform_set_error(transform,
		    TRANSFORM_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid library version");
		break;
	default:
		transform_set_error(transform,
		    TRANSFORM_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    " Zlib error %d", ret);
		break;
	}
	return (TRANSFORM_FATAL);
}

static int
consume_trailer(struct transform *transform, struct private_data *state,
	struct transform_read_filter *upstream)
{
	const unsigned char *p;
	ssize_t avail;

	state->in_stream = 0;
	switch (inflateEnd(&(state->stream))) {
	case Z_OK:
		break;
	default:
		transform_set_error(transform,
		    TRANSFORM_ERRNO_MISC,
		    "Failed to clean up gzip decompressor");
		return (TRANSFORM_FATAL);
	}

	/* GZip trailer is a fixed 8 byte structure. */
	p = transform_read_filter_ahead(upstream, 8, &avail);
	if (p == NULL || avail == 0)
		return (TRANSFORM_FATAL);

	/* XXX TODO: Verify the length and CRC. */

	/* We've verified the trailer, so consume it now. */
	transform_read_filter_consume(upstream, 8);

	return (TRANSFORM_OK);
}

static int
gzip_filter_read(struct transform *transform, void *_state,
	struct transform_read_filter *upstream, const void **p, size_t *bytes_read)
{
	struct private_data *state = (struct private_data *)_state;
	size_t decompressed;
	ssize_t avail_in;
	int ret;
	int eof_encountered = 0;

	/* Empty our output buffer. */
	state->stream.next_out = (void *)*p;
	state->stream.avail_out = *bytes_read;

	/* Try to fill the output buffer. */
	while (state->stream.avail_out > 0) {
		/* If we're not in a stream, read a header
		 * and initialize the decompression library. */
		if (!state->in_stream) {
			ret = consume_header(transform, state, upstream);
			if (TRANSFORM_EOF == ret || TRANSFORM_PREMATURE_EOF == ret) {
				eof_encountered = ret;
				break;
			}
			if (ret < TRANSFORM_OK)
				return (ret);
		}

		/* Peek at the next available data. */
		/* ZLib treats stream.next_in as const but doesn't declare
		 * it so, hence this ugly cast. */
		state->stream.next_in = (unsigned char *)(uintptr_t)
		    transform_read_filter_ahead(upstream, 1, &avail_in);
		if (state->stream.next_in == NULL)
			return (TRANSFORM_FATAL);
		state->stream.avail_in = avail_in;

		/* Decompress and consume some of that data. */
		ret = inflate(&(state->stream), 0);
		switch (ret) {
		case Z_OK: /* Decompressor made some progress. */
			transform_read_filter_consume(upstream,
			    avail_in - state->stream.avail_in);
			break;
		case Z_STREAM_END: /* Found end of stream. */
			transform_read_filter_consume(upstream,
			    avail_in - state->stream.avail_in);
			/* Consume the stream trailer; release the
			 * decompression library. */
			ret = consume_trailer(transform, state, upstream);
			if (ret < TRANSFORM_OK)
				return (ret);
			break;
		default:
			/* Return an error. */
			transform_set_error(transform,
			    TRANSFORM_ERRNO_MISC,
			    "gzip decompression failed");
			return (TRANSFORM_FATAL);
		}
	}

	/* We've read as much as we can. */
	decompressed = (void *)state->stream.next_out - (void *)*p;
	if (*bytes_read < decompressed) {
		abort();
	}
	state->total_out += decompressed;
	*bytes_read = decompressed;
	return (eof_encountered ? eof_encountered : TRANSFORM_OK);
}

/*
 * Clean up the decompressor.
 */
static int
gzip_filter_close(struct transform *transform, void *_state)
{
	struct private_data *state = (struct private_data *)_state;
	int ret;

	ret = TRANSFORM_OK;

	if (state->in_stream) {
		switch (inflateEnd(&(state->stream))) {
		case Z_OK:
			break;
		default:
			transform_set_error(transform,
			    TRANSFORM_ERRNO_MISC,
			    "Failed to clean up gzip compressor");
			ret = TRANSFORM_FATAL;
		}
	}

	free(state);
	return (ret);
}

#endif /* HAVE_ZLIB_H */
