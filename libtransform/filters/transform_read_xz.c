/*-
 * Copyright (c) 2009,2010 Michihiro NAKAJIMA
 * Copyright (c) 2003-2008 Tim Kientzle and Miklos Vajna
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

__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_support_compression_xz.c 201167 2009-12-29 06:06:20Z kientzle $");

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
#if HAVE_LZMA_H
#include <lzma.h>
#elif HAVE_LZMADEC_H
#include <lzmadec.h>
#endif

#include "transform.h"
#include "transform_endian.h"
#include "transform_private.h"
#include "transform_read_private.h"

#if HAVE_LZMA_H && HAVE_LIBLZMA

struct private_data {
	lzma_stream	 stream;
	unsigned char	*out_block;
	size_t		 out_block_size;
	int64_t		 total_out;
	char		 eof; /* True = found end of compressed data. */
	char		 in_stream;

	/* Following variables are used for lzip only. */
	char		 lzip_ver;
	uint32_t	 crc32;
	int64_t		 member_in;
	int64_t		 member_out;
};

/* Combined lzip/lzma/xz filter */
static ssize_t	xz_filter_read(struct transform_read_filter *, const void **);
static int	xz_filter_close(struct transform_read_filter *);
static int	xz_lzma_bidder_init(struct transform_read_filter *);

#elif HAVE_LZMADEC_H && HAVE_LIBLZMADEC

struct private_data {
	lzmadec_stream	 stream;
	unsigned char	*out_block;
	size_t		 out_block_size;
	int64_t		 total_out;
	char		 eof; /* True = found end of compressed data. */
};

/* Lzma-only filter */
static ssize_t	lzma_filter_read(struct transform_read_filter *, const void **);
static int	lzma_filter_close(struct transform_read_filter *);
#endif

/*
 * Note that we can detect xz and lzma compressed files even if we
 * can't decompress them.  (In fact, we like detecting them because we
 * can give better error messages.)  So the bid framework here gets
 * compiled even if no lzma library is available.
 */
static int	xz_bidder_bid(struct transform_read_filter_bidder *,
		    struct transform_read_filter *);
static int	xz_bidder_init(struct transform_read_filter *);
static int	lzma_bidder_bid(struct transform_read_filter_bidder *,
		    struct transform_read_filter *);
static int	lzma_bidder_init(struct transform_read_filter *);
static int	lzip_has_member(struct transform_read_filter *);
static int	lzip_bidder_bid(struct transform_read_filter_bidder *,
		    struct transform_read_filter *);
static int	lzip_bidder_init(struct transform_read_filter *);

int
transform_read_support_compression_xz(struct transform *_a)
{
	struct transform_read *a = (struct transform_read *)_a;
	struct transform_read_filter_bidder *bidder = __transform_read_get_bidder(a);

	transform_clear_error(_a);
	if (bidder == NULL)
		return (TRANSFORM_FATAL);

	bidder->data = NULL;
	bidder->bid = xz_bidder_bid;
	bidder->init = xz_bidder_init;
	bidder->options = NULL;
	bidder->free = NULL;
#if HAVE_LZMA_H && HAVE_LIBLZMA
	return (TRANSFORM_OK);
#else
	transform_set_error(_a, TRANSFORM_ERRNO_MISC,
	    "Using external unxz program for xz decompression");
	return (TRANSFORM_WARN);
#endif
}

int
transform_read_support_compression_lzma(struct transform *_a)
{
	struct transform_read *a = (struct transform_read *)_a;
	struct transform_read_filter_bidder *bidder = __transform_read_get_bidder(a);

	transform_clear_error(_a);
	if (bidder == NULL)
		return (TRANSFORM_FATAL);

	bidder->data = NULL;
	bidder->bid = lzma_bidder_bid;
	bidder->init = lzma_bidder_init;
	bidder->options = NULL;
	bidder->free = NULL;
#if HAVE_LZMA_H && HAVE_LIBLZMA
	return (TRANSFORM_OK);
#elif HAVE_LZMADEC_H && HAVE_LIBLZMADEC
	return (TRANSFORM_OK);
#else
	transform_set_error(_a, TRANSFORM_ERRNO_MISC,
	    "Using external unlzma program for lzma decompression");
	return (TRANSFORM_WARN);
#endif
}

int
transform_read_support_compression_lzip(struct transform *_a)
{
	struct transform_read *a = (struct transform_read *)_a;
	struct transform_read_filter_bidder *bidder = __transform_read_get_bidder(a);

	transform_clear_error(_a);
	if (bidder == NULL)
		return (TRANSFORM_FATAL);

	bidder->data = NULL;
	bidder->bid = lzip_bidder_bid;
	bidder->init = lzip_bidder_init;
	bidder->options = NULL;
	bidder->free = NULL;
#if HAVE_LZMA_H && HAVE_LIBLZMA
	return (TRANSFORM_OK);
#else
	transform_set_error(_a, TRANSFORM_ERRNO_MISC,
	    "Using external lzip program for lzip decompression");
	return (TRANSFORM_WARN);
#endif
}

/*
 * Test whether we can handle this data.
 */
static int
xz_bidder_bid(struct transform_read_filter_bidder *self,
    struct transform_read_filter *filter)
{
	const unsigned char *buffer;
	ssize_t avail;
	int bits_checked;

	(void)self; /* UNUSED */

	buffer = __transform_read_filter_ahead(filter, 6, &avail);
	if (buffer == NULL)
		return (0);

	/*
	 * Verify Header Magic Bytes : FD 37 7A 58 5A 00
	 */
	bits_checked = 0;
	if (buffer[0] != 0xFD)
		return (0);
	bits_checked += 8;
	if (buffer[1] != 0x37)
		return (0);
	bits_checked += 8;
	if (buffer[2] != 0x7A)
		return (0);
	bits_checked += 8;
	if (buffer[3] != 0x58)
		return (0);
	bits_checked += 8;
	if (buffer[4] != 0x5A)
		return (0);
	bits_checked += 8;
	if (buffer[5] != 0x00)
		return (0);
	bits_checked += 8;

	return (bits_checked);
}

/*
 * Test whether we can handle this data.
 *
 * <sigh> LZMA has a rather poor file signature.  Zeros do not
 * make good signature bytes as a rule, and the only non-zero byte
 * here is an ASCII character.  For example, an uncompressed tar
 * transform whose first file is ']' would satisfy this check.  It may
 * be necessary to exclude LZMA from compression_all() because of
 * this.  Clients of libtransform would then have to explicitly enable
 * LZMA checking instead of (or in addition to) compression_all() when
 * they have other evidence (file name, command-line option) to go on.
 */
static int
lzma_bidder_bid(struct transform_read_filter_bidder *self,
    struct transform_read_filter *filter)
{
	const unsigned char *buffer;
	ssize_t avail;
	uint32_t dicsize;
	uint64_t uncompressed_size;
	int bits_checked;

	(void)self; /* UNUSED */

	buffer = __transform_read_filter_ahead(filter, 14, &avail);
	if (buffer == NULL)
		return (0);

	/* First byte of raw LZMA stream is commonly 0x5d.
	 * The first byte is a special number, which consists of
	 * three parameters of LZMA compression, a number of literal
	 * context bits(which is from 0 to 8, default is 3), a number
	 * of literal pos bits(which is from 0 to 4, default is 0),
	 * a number of pos bits(which is from 0 to 4, default is 2).
	 * The first byte is made by
	 * (pos bits * 5 + literal pos bit) * 9 + * literal contest bit,
	 * and so the default value in this field is
	 * (2 * 5 + 0) * 9 + 3 = 0x5d.
	 * lzma of LZMA SDK has options to change those parameters.
	 * It means a range of this field is from 0 to 224. And lzma of
	 * XZ Utils with option -e records 0x5e in this field. */
	/* NOTE: If this checking of the first byte increases false
	 * recognition, we should allow only 0x5d and 0x5e for the first
	 * byte of LZMA stream. */
	bits_checked = 0;
	if (buffer[0] > (4 * 5 + 4) * 9 + 8)
		return (0);
	/* Most likely value in the first byte of LZMA stream. */
	if (buffer[0] == 0x5d || buffer[0] == 0x5e)
		bits_checked += 8;

	/* Sixth through fourteenth bytes are uncompressed size,
	 * stored in little-endian order. `-1' means uncompressed
	 * size is unknown and lzma of XZ Utils always records `-1'
	 * in this field. */
	uncompressed_size = transform_le64dec(buffer+5);
	if (uncompressed_size == (uint64_t)TRANSFORM_LITERAL_LL(-1))
		bits_checked += 64;

	/* Second through fifth bytes are dictionary size, stored in
	 * little-endian order. The minimum dictionary size is
	 * 1 << 12(4KiB) which the lzma of LZMA SDK uses with option
	 * -d12 and the maxinam dictionary size is 1 << 27(128MiB)
	 * which the one uses with option -d27.
	 * NOTE: A comment of LZMA SDK source code says this dictionary
	 * range is from 1 << 12 to 1 << 30. */
	dicsize = transform_le32dec(buffer+1);
	switch (dicsize) {
	case 0x00001000:/* lzma of LZMA SDK option -d12. */
	case 0x00002000:/* lzma of LZMA SDK option -d13. */
	case 0x00004000:/* lzma of LZMA SDK option -d14. */
	case 0x00008000:/* lzma of LZMA SDK option -d15. */
	case 0x00010000:/* lzma of XZ Utils option -0 and -1.
			 * lzma of LZMA SDK option -d16. */
	case 0x00020000:/* lzma of LZMA SDK option -d17. */
	case 0x00040000:/* lzma of LZMA SDK option -d18. */
	case 0x00080000:/* lzma of XZ Utils option -2.
			 * lzma of LZMA SDK option -d19. */
	case 0x00100000:/* lzma of XZ Utils option -3.
			 * lzma of LZMA SDK option -d20. */
	case 0x00200000:/* lzma of XZ Utils option -4.
			 * lzma of LZMA SDK option -d21. */
	case 0x00400000:/* lzma of XZ Utils option -5.
			 * lzma of LZMA SDK option -d22. */
	case 0x00800000:/* lzma of XZ Utils option -6.
			 * lzma of LZMA SDK option -d23. */
	case 0x01000000:/* lzma of XZ Utils option -7.
			 * lzma of LZMA SDK option -d24. */
	case 0x02000000:/* lzma of XZ Utils option -8.
			 * lzma of LZMA SDK option -d25. */
	case 0x04000000:/* lzma of XZ Utils option -9.
			 * lzma of LZMA SDK option -d26. */
	case 0x08000000:/* lzma of LZMA SDK option -d27. */
		bits_checked += 32;
		break;
	default:
		/* If a memory usage for encoding was not enough on
		 * the platform where LZMA stream was made, lzma of
		 * XZ Utils automatically decreased the dictionary
		 * size to enough memory for encoding by 1Mi bytes
		 * (1 << 20).*/
		if (dicsize <= 0x03F00000 && dicsize >= 0x00300000 &&
		    (dicsize & ((1 << 20)-1)) == 0 &&
		    bits_checked == 8 + 64) {
			bits_checked += 32;
			break;
		}
		/* Otherwise dictionary size is unlikely. But it is
		 * possible that someone makes lzma stream with
		 * liblzma/LZMA SDK in one's dictionary size. */
		return (0);
	}

	/* TODO: The above test is still very weak.  It would be
	 * good to do better. */

	return (bits_checked);
}

static int
lzip_has_member(struct transform_read_filter *filter)
{
	const unsigned char *buffer;
	ssize_t avail;
	int bits_checked;
	int log2dic;

	buffer = __transform_read_filter_ahead(filter, 6, &avail);
	if (buffer == NULL)
		return (0);

	/*
	 * Verify Header Magic Bytes : 4C 5A 49 50 (`LZIP')
	 */
	bits_checked = 0;
	if (buffer[0] != 0x4C)
		return (0);
	bits_checked += 8;
	if (buffer[1] != 0x5A)
		return (0);
	bits_checked += 8;
	if (buffer[2] != 0x49)
		return (0);
	bits_checked += 8;
	if (buffer[3] != 0x50)
		return (0);
	bits_checked += 8;

	/* A version number must be 0 or 1 */
	if (buffer[4] != 0 && buffer[4] != 1)
		return (0);
	bits_checked += 8;

	/* Dictionary size. */
	log2dic = buffer[5] & 0x1f;
	if (log2dic < 12 || log2dic > 27)
		return (0);
	bits_checked += 8;

	return (bits_checked);
}

static int
lzip_bidder_bid(struct transform_read_filter_bidder *self,
    struct transform_read_filter *filter)
{

	(void)self; /* UNUSED */
	return (lzip_has_member(filter));
}

#if HAVE_LZMA_H && HAVE_LIBLZMA

/*
 * liblzma 4.999.7 and later support both lzma and xz streams.
 */
static int
xz_bidder_init(struct transform_read_filter *self)
{
	self->code = TRANSFORM_FILTER_XZ;
	self->name = "xz";
	return (xz_lzma_bidder_init(self));
}

static int
lzma_bidder_init(struct transform_read_filter *self)
{
	self->code = TRANSFORM_FILTER_LZMA;
	self->name = "lzma";
	return (xz_lzma_bidder_init(self));
}

static int
lzip_bidder_init(struct transform_read_filter *self)
{
	self->code = TRANSFORM_FILTER_LZIP;
	self->name = "lzip";
	return (xz_lzma_bidder_init(self));
}

/*
 * Set an error code and choose an error message
 */
static void
set_error(struct transform_read_filter *self, int ret)
{

	switch (ret) {
	case LZMA_STREAM_END: /* Found end of stream. */
	case LZMA_OK: /* Decompressor made some progress. */
		break;
	case LZMA_MEM_ERROR:
		transform_set_error(&self->transform->transform, ENOMEM,
		    "Lzma library error: Cannot allocate memory");
		break;
	case LZMA_MEMLIMIT_ERROR:
		transform_set_error(&self->transform->transform, ENOMEM,
		    "Lzma library error: Out of memory");
		break;
	case LZMA_FORMAT_ERROR:
		transform_set_error(&self->transform->transform,
		    TRANSFORM_ERRNO_MISC,
		    "Lzma library error: format not recognized");
		break;
	case LZMA_OPTIONS_ERROR:
		transform_set_error(&self->transform->transform,
		    TRANSFORM_ERRNO_MISC,
		    "Lzma library error: Invalid options");
		break;
	case LZMA_DATA_ERROR:
		transform_set_error(&self->transform->transform,
		    TRANSFORM_ERRNO_MISC,
		    "Lzma library error: Corrupted input data");
		break;
	case LZMA_BUF_ERROR:
		transform_set_error(&self->transform->transform,
		    TRANSFORM_ERRNO_MISC,
		    "Lzma library error:  No progress is possible");
		break;
	default:
		/* Return an error. */
		transform_set_error(&self->transform->transform,
		    TRANSFORM_ERRNO_MISC,
		    "Lzma decompression failed:  Unknown error");
		break;
	}
}

/*
 * Setup the callbacks.
 */
static int
xz_lzma_bidder_init(struct transform_read_filter *self)
{
	static const size_t out_block_size = 64 * 1024;
	void *out_block;
	struct private_data *state;
	int ret;

	state = (struct private_data *)calloc(sizeof(*state), 1);
	out_block = (unsigned char *)malloc(out_block_size);
	if (state == NULL || out_block == NULL) {
		transform_set_error(&self->transform->transform, ENOMEM,
		    "Can't allocate data for xz decompression");
		free(out_block);
		free(state);
		return (TRANSFORM_FATAL);
	}

	self->data = state;
	state->out_block_size = out_block_size;
	state->out_block = out_block;
	self->read = xz_filter_read;
	self->skip = NULL; /* not supported */
	self->close = xz_filter_close;

	state->stream.avail_in = 0;

	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	state->crc32 = 0;
	if (self->code == TRANSFORM_FILTER_LZIP) {
		/*
		 * We have to read a lzip header and use it to initialize
		 * compression library, thus we cannot initialize the
		 * library for lzip here.
		 */
		state->in_stream = 0;
		return (TRANSFORM_OK);
	} else
		state->in_stream = 1;

	/* Initialize compression library.
	 * TODO: I don't know what value is best for memlimit.
	 *       maybe, it needs to check memory size which
	 *       running system has.
	 */
	if (self->code == TRANSFORM_FILTER_XZ)
		ret = lzma_stream_decoder(&(state->stream),
		    (1U << 30),/* memlimit */
		    LZMA_CONCATENATED);
	else
		ret = lzma_alone_decoder(&(state->stream),
		    (1U << 30));/* memlimit */

	if (ret == LZMA_OK)
		return (TRANSFORM_OK);

	/* Library setup failed: Choose an error message and clean up. */
	set_error(self, ret);

	free(state->out_block);
	free(state);
	self->data = NULL;
	return (TRANSFORM_FATAL);
}

static int
lzip_init(struct transform_read_filter *self)
{
	struct private_data *state;
	const unsigned char *h;
	lzma_filter filters[2];
	unsigned char props[5];
	ssize_t avail_in;
	uint32_t dicsize;
	int log2dic, ret;

	state = (struct private_data *)self->data;
	h = __transform_read_filter_ahead(self->upstream, 6, &avail_in);
	if (h == NULL && avail_in < 0)
		return (TRANSFORM_FATAL);

	/* Get a version number. */
	state->lzip_ver = h[4];

	/*
	 * Setup lzma property.
	 */
	props[0] = 0x5d;

	/* Get dictionary size. */
	log2dic = h[5] & 0x1f;
	if (log2dic < 12 || log2dic > 27)
		return (TRANSFORM_FATAL);
	dicsize = 1U << log2dic;
	if (log2dic > 12)
		dicsize -= (dicsize / 16) * (h[5] >> 5);
	transform_le32enc(props+1, dicsize);

	/* Consume lzip header. */
	__transform_read_filter_consume(self->upstream, 6);
	state->member_in = 6;

	filters[0].id = LZMA_FILTER_LZMA1;
	filters[0].options = NULL;
	filters[1].id = LZMA_VLI_UNKNOWN;
	filters[1].options = NULL;

	ret = lzma_properties_decode(&filters[0], NULL, props, sizeof(props));
	if (ret != LZMA_OK) {
		set_error(self, ret);
		return (TRANSFORM_FATAL);
	}
	ret = lzma_raw_decoder(&(state->stream), filters);
	if (ret != LZMA_OK) {
		set_error(self, ret);
		return (TRANSFORM_FATAL);
	}
	return (TRANSFORM_OK);
}

static int
lzip_tail(struct transform_read_filter *self)
{
	struct private_data *state;
	const unsigned char *f;
	ssize_t avail_in;
	int tail;

	state = (struct private_data *)self->data;
	if (state->lzip_ver == 0)
		tail = 12;
	else
		tail = 20;
	f = __transform_read_filter_ahead(self->upstream, tail, &avail_in);
	if (f == NULL && avail_in < 0)
		return (TRANSFORM_FATAL);
	if (avail_in < tail) {
		transform_set_error(&self->transform->transform, TRANSFORM_ERRNO_MISC,
		    "Lzip: Remaining data is less bytes");
		return (TRANSFORM_FAILED);
	}

	/* Check the crc32 value of the uncompressed data of the current
	 * member */
	if (state->crc32 != transform_le32dec(f)) {
		transform_set_error(&self->transform->transform, TRANSFORM_ERRNO_MISC,
		    "Lzip: CRC32 error");
		return (TRANSFORM_FAILED);
	}

	/* Check the uncompressed size of the current member */
	if ((uint64_t)state->member_out != transform_le64dec(f + 4)) {
		transform_set_error(&self->transform->transform, TRANSFORM_ERRNO_MISC,
		    "Lzip: Uncompressed size error");
		return (TRANSFORM_FAILED);
	}

	/* Check the total size of the current member */
	if (state->lzip_ver == 1 &&
	    (uint64_t)state->member_in + tail != transform_le64dec(f + 12)) {
		transform_set_error(&self->transform->transform, TRANSFORM_ERRNO_MISC,
		    "Lzip: Member size error");
		return (TRANSFORM_FAILED);
	}
	__transform_read_filter_consume(self->upstream, tail);

	/* If current lzip data consists of multi member, try decompressing
	 * a next member. */
	if (lzip_has_member(self->upstream) != 0) {
		state->in_stream = 0;
		state->crc32 = 0;
		state->member_out = 0;
		state->member_in = 0;
		state->eof = 0;
	}
	return (TRANSFORM_OK);
}

/*
 * Return the next block of decompressed data.
 */
static ssize_t
xz_filter_read(struct transform_read_filter *self, const void **p)
{
	struct private_data *state;
	size_t decompressed;
	ssize_t avail_in;
	int ret;

	state = (struct private_data *)self->data;

	/* Empty our output buffer. */
	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Try to fill the output buffer. */
	while (state->stream.avail_out > 0 && !state->eof) {
		if (!state->in_stream) {
			/*
			 * Initialize liblzma for lzip
			 */
			ret = lzip_init(self);
			if (ret != TRANSFORM_OK)
				return (ret);
			state->in_stream = 1;
		}
		state->stream.next_in =
		    __transform_read_filter_ahead(self->upstream, 1, &avail_in);
		if (state->stream.next_in == NULL && avail_in < 0)
			return (TRANSFORM_FATAL);
		state->stream.avail_in = avail_in;

		/* Decompress as much as we can in one pass. */
		ret = lzma_code(&(state->stream),
		    (state->stream.avail_in == 0)? LZMA_FINISH: LZMA_RUN);
		switch (ret) {
		case LZMA_STREAM_END: /* Found end of stream. */
			state->eof = 1;
			/* FALL THROUGH */
		case LZMA_OK: /* Decompressor made some progress. */
			__transform_read_filter_consume(self->upstream,
			    avail_in - state->stream.avail_in);
			state->member_in +=
			    avail_in - state->stream.avail_in;
			break;
		default:
			set_error(self, ret);
			return (TRANSFORM_FATAL);
		}
	}

	decompressed = state->stream.next_out - state->out_block;
	state->total_out += decompressed;
	state->member_out += decompressed;
	if (decompressed == 0)
		*p = NULL;
	else {
		*p = state->out_block;
		if (self->code == TRANSFORM_FILTER_LZIP) {
			state->crc32 = lzma_crc32(state->out_block,
			    decompressed, state->crc32);
			if (state->eof) {
				ret = lzip_tail(self);
				if (ret != TRANSFORM_OK)
					return (ret);
			}
		}
	}
	return (decompressed);
}

/*
 * Clean up the decompressor.
 */
static int
xz_filter_close(struct transform_read_filter *self)
{
	struct private_data *state;

	state = (struct private_data *)self->data;
	lzma_end(&(state->stream));
	free(state->out_block);
	free(state);
	return (TRANSFORM_OK);
}

#else

#if HAVE_LZMADEC_H && HAVE_LIBLZMADEC

/*
 * If we have the older liblzmadec library, then we can handle
 * LZMA streams but not XZ streams.
 */

/*
 * Setup the callbacks.
 */
static int
lzma_bidder_init(struct transform_read_filter *self)
{
	static const size_t out_block_size = 64 * 1024;
	void *out_block;
	struct private_data *state;
	ssize_t ret, avail_in;

	self->code = TRANSFORM_FILTER_LZMA;
	self->name = "lzma";

	state = (struct private_data *)calloc(sizeof(*state), 1);
	out_block = (unsigned char *)malloc(out_block_size);
	if (state == NULL || out_block == NULL) {
		transform_set_error(&self->transform->transform, ENOMEM,
		    "Can't allocate data for lzma decompression");
		free(out_block);
		free(state);
		return (TRANSFORM_FATAL);
	}

	self->data = state;
	state->out_block_size = out_block_size;
	state->out_block = out_block;
	self->read = lzma_filter_read;
	self->skip = NULL; /* not supported */
	self->close = lzma_filter_close;

	/* Prime the lzma library with 18 bytes of input. */
	state->stream.next_in = (unsigned char *)(uintptr_t)
	    __transform_read_filter_ahead(self->upstream, 18, &avail_in);
	if (state->stream.next_in == NULL)
		return (TRANSFORM_FATAL);
	state->stream.avail_in = avail_in;
	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Initialize compression library. */
	ret = lzmadec_init(&(state->stream));
	__transform_read_filter_consume(self->upstream,
	    avail_in - state->stream.avail_in);
	if (ret == LZMADEC_OK)
		return (TRANSFORM_OK);

	/* Library setup failed: Clean up. */
	transform_set_error(&self->transform->transform, TRANSFORM_ERRNO_MISC,
	    "Internal error initializing lzma library");

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case LZMADEC_HEADER_ERROR:
		transform_set_error(&self->transform->transform,
		    TRANSFORM_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid header");
		break;
	case LZMADEC_MEM_ERROR:
		transform_set_error(&self->transform->transform, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	}

	free(state->out_block);
	free(state);
	self->data = NULL;
	return (TRANSFORM_FATAL);
}

/*
 * Return the next block of decompressed data.
 */
static ssize_t
lzma_filter_read(struct transform_read_filter *self, const void **p)
{
	struct private_data *state;
	size_t decompressed;
	ssize_t avail_in, ret;

	state = (struct private_data *)self->data;

	/* Empty our output buffer. */
	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Try to fill the output buffer. */
	while (state->stream.avail_out > 0 && !state->eof) {
		state->stream.next_in = (unsigned char *)(uintptr_t)
		    __transform_read_filter_ahead(self->upstream, 1, &avail_in);
		if (state->stream.next_in == NULL && avail_in < 0)
			return (TRANSFORM_FATAL);
		state->stream.avail_in = avail_in;

		/* Decompress as much as we can in one pass. */
		ret = lzmadec_decode(&(state->stream), avail_in == 0);
		switch (ret) {
		case LZMADEC_STREAM_END: /* Found end of stream. */
			state->eof = 1;
			/* FALL THROUGH */
		case LZMADEC_OK: /* Decompressor made some progress. */
			__transform_read_filter_consume(self->upstream,
			    avail_in - state->stream.avail_in);
			break;
		case LZMADEC_BUF_ERROR: /* Insufficient input data? */
			transform_set_error(&self->transform->transform,
			    TRANSFORM_ERRNO_MISC,
			    "Insufficient compressed data");
			return (TRANSFORM_FATAL);
		default:
			/* Return an error. */
			transform_set_error(&self->transform->transform,
			    TRANSFORM_ERRNO_MISC,
			    "Lzma decompression failed");
			return (TRANSFORM_FATAL);
		}
	}

	decompressed = state->stream.next_out - state->out_block;
	state->total_out += decompressed;
	if (decompressed == 0)
		*p = NULL;
	else
		*p = state->out_block;
	return (decompressed);
}

/*
 * Clean up the decompressor.
 */
static int
lzma_filter_close(struct transform_read_filter *self)
{
	struct private_data *state;
	int ret;

	state = (struct private_data *)self->data;
	ret = TRANSFORM_OK;
	switch (lzmadec_end(&(state->stream))) {
	case LZMADEC_OK:
		break;
	default:
		transform_set_error(&(self->transform->transform),
		    TRANSFORM_ERRNO_MISC,
		    "Failed to clean up %s compressor",
		    self->transform->transform.compression_name);
		ret = TRANSFORM_FATAL;
	}

	free(state->out_block);
	free(state);
	return (ret);
}

#else

/*
 *
 * If we have no suitable library on this system, we can't actually do
 * the decompression.  We can, however, still detect compressed
 * transforms and emit a useful message.
 *
 */
static int
lzma_bidder_init(struct transform_read_filter *self)
{
	int r;

	r = __transform_read_program(self, "unlzma");
	/* Note: We set the format here even if __transform_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	self->code = TRANSFORM_FILTER_LZMA;
	self->name = "lzma";
	return (r);
}

#endif /* HAVE_LZMADEC_H */


static int
xz_bidder_init(struct transform_read_filter *self)
{
	int r;

	r = __transform_read_program(self, "unxz");
	/* Note: We set the format here even if __transform_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	self->code = TRANSFORM_FILTER_XZ;
	self->name = "xz";
	return (r);
}

static int
lzip_bidder_init(struct transform_read_filter *self)
{
	int r;

	r = __transform_read_program(self, "unlzip");
	/* Note: We set the format here even if __transform_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	self->code = TRANSFORM_FILTER_LZIP;
	self->name = "lzip";
	return (r);
}


#endif /* HAVE_LZMA_H */