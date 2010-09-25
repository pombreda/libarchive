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

/*
 * This code borrows heavily from "compress" source code, which is
 * protected by the following copyright.  (Clause 3 dropped by request
 * of the Regents.)
 */

/*-
 * Copyright (c) 1985, 1986, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis and James A. Woods, derived from original
 * work by Spencer Thomas and Joseph Orost.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include "transform_platform.h"
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_support_compression_compress.c 201094 2009-12-28 02:29:21Z kientzle $");

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

#include "transform.h"

/*
 * Because LZW decompression is pretty simple, I've just implemented
 * the whole decompressor here (cribbing from "compress" source code,
 * of course), rather than relying on an external library.  I have
 * made an effort to clarify and simplify the algorithm, so the
 * names and structure here don't exactly match those used by compress.
 */

struct private_data {
	/* Input variables. */
	const unsigned char	*next_in;
	size_t			 avail_in;
	size_t			 consume_unnotified;
	int			 bit_buffer;
	int			 bits_avail;
	size_t			 bytes_in_section;

	/* Decompression status variables. */
	int			 use_reset_code;
	int			 maxcode;	/* Largest code. */
	int			 maxcode_bits;	/* Length of largest code. */
	int			 section_end_code; /* When to increase bits. */
	int			 bits;		/* Current code length. */
	int			 oldcode;	/* Previous code. */
	int			 finbyte;	/* Last byte of prev code. */

	/* Dictionary. */
	int			 free_ent;       /* Next dictionary entry. */
	unsigned char		 suffix[65536];
	uint16_t		 prefix[65536];

	/*
	 * Scratch area for expanding dictionary entries.  Note:
	 * "worst" case here comes from compressing /dev/zero: the
	 * last code in the dictionary will code a sequence of
	 * 65536-256 zero bytes.  Thus, we need stack space to expand
	 * a 65280-byte dictionary entry.  (Of course, 32640:1
	 * compression could also be considered the "best" case. ;-)
	 */
	unsigned char		*stackp;
	unsigned char		 stack[65300];
	int 				stream_init_invoked;
};

static int	compress_bidder_bid(const void *, struct transform_read_filter *);
static int	compress_bidder_init(struct transform *, const void *);

static int	compress_filter_read(struct transform *, void *,
	struct transform_read_filter *, const void **, size_t *);
static int	compress_filter_close(struct transform *, void *);

static int getbits(struct private_data *, struct transform_read_filter *, int n);
static int next_code(struct transform *, struct private_data *,
	struct transform_read_filter *);

int
transform_read_add_compress(struct transform *_t)
{
	return (transform_read_bidder_add(_t, NULL, "compress", compress_bidder_bid, 
		compress_bidder_init, NULL, NULL) == NULL ? TRANSFORM_FATAL : TRANSFORM_OK);
}

int
transform_autodetect_add_compress(struct transform_read_bidder *trb)
{
	return (transform_autodetect_add_bidder_create(trb, NULL, "compress",
		compress_bidder_bid, 
		compress_bidder_init, NULL, NULL));
}

/*
 * Test whether we can handle this data.
 *
 * This logic returns zero if any part of the signature fails.  It
 * also tries to Do The Right Thing if a very short buffer prevents us
 * from verifying as much as we would like.
 */
static int
compress_bidder_bid(const void *bidder_data, struct transform_read_filter *filter)
{
	const unsigned char *buffer;
	ssize_t avail;
	int bits_checked;

	buffer = transform_read_filter_ahead(filter, 2, &avail);

	if (buffer == NULL)
		return (0);

	bits_checked = 0;
	if (buffer[0] != 037)	/* Verify first ID byte. */
		return (0);
	bits_checked += 8;

	if (buffer[1] != 0235)	/* Verify second ID byte. */
		return (0);
	bits_checked += 8;

	/*
	 * TODO: Verify more.
	 */

	return (bits_checked);
}

/*
 * Setup the callbacks.
 */
static int
compress_bidder_init(struct transform *transform, const void *bidder_data)
{
	struct private_data *state;
	int ret;

	state = (struct private_data *)calloc(sizeof(*state), 1);
	if (state == NULL) {
		free(state);
		transform_set_error(transform, ENOMEM,
		    "Can't allocate data for compress (.Z) decompression");
		return (TRANSFORM_FATAL);
	}

    ret = transform_read_add_new_filter(transform, (void *)state,
    	"compress (.Z)", TRANSFORM_FILTER_COMPRESS,
		compress_filter_read, NULL, compress_filter_close, NULL, 0);

	if (TRANSFORM_OK != ret) {
		compress_filter_close(transform, state);
	}
	return (ret);
}

static int
stream_init(struct transform *transform, struct private_data *state, 
	struct transform_read_filter *upstream)
{
	int code;
	/* XXX MOVE THE FOLLOWING OUT OF INIT() XXX */

	(void)getbits(state, upstream, 8); /* Skip first signature byte. */
	(void)getbits(state, upstream, 8); /* Skip second signature byte. */

	code = getbits(state, upstream, 8);
	state->maxcode_bits = code & 0x1f;
	state->maxcode = (1 << state->maxcode_bits);
	state->use_reset_code = code & 0x80;

	/* Initialize decompressor. */
	state->free_ent = 256;
	state->stackp = state->stack;
	if (state->use_reset_code)
		state->free_ent++;
	state->bits = 9;
	state->section_end_code = (1<<state->bits) - 1;
	state->oldcode = -1;
	for (code = 255; code >= 0; code--) {
		state->prefix[code] = 0;
		state->suffix[code] = code;
	}
	next_code(transform, state, upstream);

	return (TRANSFORM_OK);
}

/*
 * Return a block of data from the decompression buffer.  Decompress more
 * as necessary.
 */
static int
compress_filter_read(struct transform *transform, void *_state,
	struct transform_read_filter *upstream, const void **pblock,
		size_t *bytes_read)
{
	struct private_data *state = (struct private_data *)_state;
	unsigned char *p, *end;
	int ret = TRANSFORM_EOF;

	if(!state->stream_init_invoked) {
		stream_init(transform, state, upstream);
		state->stream_init_invoked = 1;
	}

	p = (unsigned char *)*pblock;
	end = p + *bytes_read;

	while (p < end) {
		if (state->stackp > state->stack) {
			*p++ = *--state->stackp;
		} else {
			ret = next_code(transform, state, upstream);
			if (ret == TRANSFORM_EOF) {
				break;
			} else if (ret != TRANSFORM_OK) {
				return (ret);
			}
		}
	}

	*bytes_read = ((void *)p - (void *)*pblock);
	return (ret);
}

/*
 * Close and release the filter.
 */
static int
compress_filter_close(struct transform *self, void *_data)
{
	struct private_data *state = (struct private_data *)_data;

	free(state);
	return (TRANSFORM_OK);
}

/*
 * Process the next code and fill the stack with the expansion
 * of the code.  Returns TRANSFORM_FATAL if there is a fatal I/O or
 * format error, TRANSFORM_EOF if we hit end of data, TRANSFORM_OK otherwise.
 */
static int
next_code(struct transform *transform, struct private_data *state,
	struct transform_read_filter *upstream)
{
	int code, newcode;

	static int debug_buff[1024];
	static unsigned debug_index;

	code = newcode = getbits(state, upstream, state->bits);
	if (code < 0)
		return (code);

	debug_buff[debug_index++] = code;
	if (debug_index >= sizeof(debug_buff)/sizeof(debug_buff[0]))
		debug_index = 0;

	/* If it's a reset code, reset the dictionary. */
	if ((code == 256) && state->use_reset_code) {
		/*
		 * The original 'compress' implementation blocked its
		 * I/O in a manner that resulted in junk bytes being
		 * inserted after every reset.  The next section skips
		 * this junk.  (Yes, the number of *bytes* to skip is
		 * a function of the current *bit* length.)
		 */
		int skip_bytes =  state->bits -
		    (state->bytes_in_section % state->bits);
		skip_bytes %= state->bits;
		state->bits_avail = 0; /* Discard rest of this byte. */
		while (skip_bytes-- > 0) {
			code = getbits(state, upstream, 8);
			if (code < 0)
				return (code);
		}
		/* Now, actually do the reset. */
		state->bytes_in_section = 0;
		state->bits = 9;
		state->section_end_code = (1 << state->bits) - 1;
		state->free_ent = 257;
		state->oldcode = -1;
		return (next_code(transform, state, upstream));
	}

	if (code > state->free_ent) {
		/* An invalid code is a fatal error. */
		transform_set_error(transform, -1,
		    "Invalid compressed data");
		return (TRANSFORM_FATAL);
	}

	/* Special case for KwKwK string. */
	if (code >= state->free_ent) {
		*state->stackp++ = state->finbyte;
		code = state->oldcode;
	}

	/* Generate output characters in reverse order. */
	while (code >= 256) {
		*state->stackp++ = state->suffix[code];
		code = state->prefix[code];
	}
	*state->stackp++ = state->finbyte = code;

	/* Generate the new entry. */
	code = state->free_ent;
	if (code < state->maxcode && state->oldcode >= 0) {
		state->prefix[code] = state->oldcode;
		state->suffix[code] = state->finbyte;
		++state->free_ent;
	}
	if (state->free_ent > state->section_end_code) {
		state->bits++;
		state->bytes_in_section = 0;
		if (state->bits == state->maxcode_bits)
			state->section_end_code = state->maxcode;
		else
			state->section_end_code = (1 << state->bits) - 1;
	}

	/* Remember previous code. */
	state->oldcode = newcode;
	return (TRANSFORM_OK);
}

/*
 * Return next 'n' bits from stream.
 *
 * -1 indicates end of available data.
 */
static int
getbits(struct private_data *state, struct transform_read_filter *upstream,
	int n)
{
	int code;
	ssize_t ret;
	static const int mask[] = {
		0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff,
		0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff
	};

	while (state->bits_avail < n) {
		if (state->avail_in <= 0) {
			if (state->consume_unnotified) {
				transform_read_filter_consume(upstream,
					state->consume_unnotified);
				state->consume_unnotified = 0;
			}
			state->next_in
			    = transform_read_filter_ahead(upstream, 1, &ret);
			if (ret == 0)
				return (TRANSFORM_EOF);
			if (ret < 0 || state->next_in == NULL)
				return (TRANSFORM_FATAL);
			state->avail_in = ret;
			state->consume_unnotified = state->avail_in = ret;
		}
		state->bit_buffer |= *state->next_in++ << state->bits_avail;
		state->avail_in--;
		state->bits_avail += 8;
		state->bytes_in_section++;
	}

	code = state->bit_buffer;
	state->bit_buffer >>= n;
	state->bits_avail -= n;

	return (code & mask[n]);
}