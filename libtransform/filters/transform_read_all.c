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
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_support_compression_all.c 201248 2009-12-30 06:12:03Z kientzle $");

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
#include "transform_read_private.h"

#define AUTODETECT_MAGIC 0xfe4d1230U

struct private_data {
	/*
	 * we explicitly mark this bidder so that we can snag programmer
	 * screwups- recall that this bidder is externally exposed, albeit
	 * opaque.
	 */
	int magic;
	struct transform_read_bidder *bidders;
};

static int autodetect_create(struct transform *, const void *);
static int autodetect_free(const void *);

int
transform_autodetect_add_bidder_create(struct transform_read_bidder *,
	const void *, const char *,
	transform_read_bidder_bid_callback *,
	transform_read_bidder_create_filter_callback *,
	transform_read_bidder_free_callback *,
	transform_read_bidder_set_option_callback *);

int
transform_autodetect_add_bidder(struct transform_read_bidder *trb,
	struct transform_read_bidder *new);

int
transform_autodetect_add_bidder_create(struct transform_read_bidder *trb,
	const void *bidder_data, const char *bidder_name,
	transform_read_bidder_bid_callback *bid,
	transform_read_bidder_create_filter_callback *create_filter,
	transform_read_bidder_free_callback *dealloc,
	transform_read_bidder_set_option_callback *set_option)
{
	struct transform_read_bidder *new;
	int ret = TRANSFORM_FATAL;

	new = transform_read_bidder_create(bidder_data, bidder_name,
		bid, create_filter, dealloc, set_option);

	if (new) {
		ret = transform_autodetect_add_bidder(trb, new);
		if (TRANSFORM_OK != ret) {
			int ret2 = transform_read_bidder_free(new);
			if (ret2 < ret) {
				ret = ret2;
			}
		}
	}

	return (ret);
}

int
transform_autodetect_add_bidder(struct transform_read_bidder *trb,
	struct transform_read_bidder *new)
{
	struct private_data *data = (struct private_data *)trb->data;

	if (TRANSFORM_FATAL == __transform_read_bidder_check_magic(NULL, trb,
		TRANSFORM_READ_BIDDER_MAGIC, TRANSFORM_STATE_NEW,
		"transform_autodetect_add_bidder") ||
		!data || data->magic != AUTODETECT_MAGIC) {
		return (TRANSFORM_FATAL);
	}

	if (TRANSFORM_FATAL == __transform_read_bidder_check_magic(NULL, new,
		TRANSFORM_READ_BIDDER_MAGIC, TRANSFORM_STATE_NEW,
		"transform_autodetect_add_bidder") ||
		new->next) {
		return (TRANSFORM_FATAL);
	}

	new->next = data->bidders;
	data->bidders = new;

	return (TRANSFORM_OK);
}

struct transform_read_bidder *
transform_read_add_autodetect(struct transform *t)
{
	struct transform_read_bidder *b;
	struct private_data *data = calloc(1, sizeof(struct private_data));
	if (!data) {
		transform_set_error(t, ENOMEM, "failed to allocate private data");
		return NULL;
	}

	data->magic = AUTODETECT_MAGIC;

	b = transform_read_bidder_add(t, (void *)data, "autodetect",
		NULL, autodetect_create, autodetect_free, NULL);
	if (!b) {
		autodetect_free(b);
	}
	return (b);
}

/*
 * Allow each registered stream transform to bid on whether
 * it wants to handle this stream.  Repeat until we've finished
 * building the pipeline.
 */
static int
autodetect_create(struct transform *t, const void *_data)
{
	int bid, best_bid, ret;
	struct transform_read_bidder *bidder, *best_bidder;
	struct transform_read *tr = (struct transform_read *)t;
	struct private_data *data = (struct private_data *)_data;
	ssize_t avail;

	for (;;) {
		best_bid = 0;
		best_bidder = NULL;

		bidder = data->bidders;
		for (bidder = data->bidders; NULL != bidder; bidder = bidder->next) {
			if (bidder->bid != NULL) {
				/* cheat.  reach in, pull the filter off. */
				bid = (bidder->bid)(bidder->data, tr->filter);
				if (bid > best_bid) {
					best_bid = bid;
					best_bidder = bidder;
				}
			}
		}

		/* If no bidder, we're done. */
		if (best_bidder == NULL) {
			/* Verify the filter by asking it for some data. */
			transform_read_filter_ahead(tr->filter, 1, &avail);
			if (avail < 0) {
				return (TRANSFORM_FATAL);
			}
			return (TRANSFORM_OK);
		}

		
		ret = (best_bidder->create_filter)((struct transform *)t, best_bidder->data);

		if (TRANSFORM_FATAL == ret) {
			return (TRANSFORM_FATAL);
		}
	}

	return (TRANSFORM_FATAL);
}

static int
autodetect_free(const void *_data)
{
	struct private_data *data = (struct private_data *)_data;
	struct transform_read_bidder *tmp, *bidder = data->bidders;
	int ret = TRANSFORM_OK;

	while (bidder) {
		tmp = bidder->next;
		int ret2 = transform_read_bidder_free(bidder);
		if (ret < ret2) {
			ret = ret2;
		}
		bidder = tmp;
	}
	return (ret);
}

int
transform_autodetect_add_all(struct transform_read_bidder *trb)
{
	/* Bzip falls back to "bunzip2" command-line */
	transform_autodetect_add_bzip2(trb);
	/* The decompress code doesn't use an outside library. */
	transform_autodetect_add_compress(trb);
	/* Gzip decompress falls back to "gunzip" command-line. */
	transform_autodetect_add_gzip(trb);
	/* Lzip falls back to "unlzip" command-line program. */
	transform_autodetect_add_lzip(trb);
	/* The LZMA file format has a very weak signature, so it
	 * may not be feasible to keep this here, but we'll try.
	 * This will come back out if there are problems. */
	/* Lzma falls back to "unlzma" command-line program. */
	transform_autodetect_add_lzma(trb);
	/* Xz falls back to "unxz" command-line program. */
	transform_autodetect_add_xz(trb);
	/* The decode code doesn't use an outside library. */
	transform_autodetect_add_uu(trb);
	/* The decode code doesn't use an outside library. */
	transform_autodetect_add_rpm(trb);

	/* Note: We always return TRANSFORM_OK here, even if some of the
	 * above return TRANSFORM_WARN.  The intent here is to enable
	 * "as much as possible."  Clients who need specific
	 * compression should enable those individually so they can
	 * verify the level of support. */
	return (TRANSFORM_OK);
}

int
transform_read_add_autodetect_all(struct transform *t)
{
	struct transform_read_bidder *trb;

	trb = transform_read_add_autodetect(t);
	if (!trb) {
		return (TRANSFORM_FATAL);
	}

	transform_autodetect_add_all(trb);

	return (TRANSFORM_OK);
}
