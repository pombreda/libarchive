/*-
 * Copyright (c) 2010 Brian Harring
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
__FBSDID("$FreeBSD");

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

struct window_data {
	int64_t start;
	int64_t allowed;
	/*
	 * note that this sharing is only bid -> filter; if it ever shifts from being
	 * 1->1, we'll have issues with the filter data being stored here
	 */
	int ref_count;
};

static int window_bidder_init(struct transform *, const void *);
static int window_bidder_free(const void *);
static int window_read(struct transform *, void *,
	struct transform_read_filter *, const void **, size_t *);
static int64_t window_skip(struct transform *, void *, struct transform_read_filter *,
	int64_t request);
static int window_free(struct transform *t, void *_data);

int
transform_read_add_window(struct transform *t, int64_t start_offset, int64_t length)
{
	struct window_data *w_bid_data = (struct window_data *)malloc(
		sizeof(struct window_data));

	if (!w_bid_data) {
		transform_set_error(t, ENOMEM,
			"allocation of bidder data failed");
		return (TRANSFORM_FATAL);
	}
	w_bid_data->start = start_offset;
	w_bid_data->allowed = length;
	w_bid_data->ref_count = 1;

	if (NULL == transform_read_bidder_add(t, w_bid_data,
		"window", NULL, window_bidder_init, window_bidder_free,
		NULL)) {
		window_bidder_free(w_bid_data);
		return (TRANSFORM_FATAL);
	}

	return (TRANSFORM_OK);
}

static int
window_bidder_free(const void *_data)
{
	struct window_data *w_data = (struct window_data *)_data;
	w_data->ref_count--;
	if (0 == w_data->ref_count) {
		free(w_data);
	}
	return (TRANSFORM_OK);
}

static int
window_bidder_init(struct transform *t, const void *w_bid_data)
{
	struct window_data *w_data = (struct window_data *)w_bid_data;

	if (!w_data) {
		transform_set_error(t, TRANSFORM_ERRNO_PROGRAMMER,
			"no bidder data supplied");
		return (TRANSFORM_FATAL);
	}

	if (0 == w_data->start && -1 == w_data->allowed) {
		/* someone requested a pointless window.  skip the filter addition. */
		return (TRANSFORM_OK);
	}

	if (TRANSFORM_FATAL == transform_read_filter_add(t, (void *)w_data,
		"window", TRANSFORM_FILTER_WINDOW, window_read, window_skip,
		window_free, NULL, TRANSFORM_FILTER_NOTIFY_ALL_CONSUME_FLAG))
		{

		/* note no close is needed- refcount wasn't increased after all. */
		return (TRANSFORM_FATAL);
	}

	/* up the refcnt so we don't have our data removed under our feet */
	w_data->ref_count++;
	return (TRANSFORM_OK);
}

static int
window_free(struct transform *t, void *_data)
{
	struct window_data *w_data = (struct window_data *)_data;
	w_data->ref_count--;
	if (0 == w_data->ref_count) {
		free(_data);
	}
	return (TRANSFORM_OK);
}

static int
window_read(struct transform *t, void *_data,
	struct transform_read_filter *upstream, const void **buff,
	size_t *bytes_read)
{
	struct window_data *w_data = (struct window_data *)_data;
	ssize_t available;
	int64_t consumed;
	
	if (0 == w_data->allowed) {
		*bytes_read = 0;
		return (TRANSFORM_EOF);
	}

	while (w_data->start) {
		consumed = transform_read_filter_consume(upstream, w_data->start);

		if (consumed < 0) {
			/* retry statuses? either way, pass back the error */
			return (consumed);
		}
		w_data->start -= consumed;
	}

	*buff = transform_read_filter_ahead(upstream, 1, &available);
	if (available < 0) {
		return (available);
	} else if (0 == available) {
		*bytes_read = 0;
		return (TRANSFORM_EOF);
	}

	if (-1 != w_data->allowed) {
		if (w_data->allowed < available) {
			*bytes_read = w_data->allowed;
			return (TRANSFORM_EOF);
		}
	}
	*bytes_read = available;
	return (TRANSFORM_OK);
}


static int64_t
window_skip(struct transform *t, void *_data,
	struct transform_read_filter *upstream, int64_t request)
{
	struct window_data *w_data = (struct window_data *)_data;
	int64_t skipped;

	if (-1 != w_data->allowed && request > w_data->allowed) {
		return (-1);
	}
	skipped = transform_read_filter_skip(upstream, request);
	if (-1 != w_data->allowed && skipped > 0)
		w_data->allowed -= skipped;
	return (skipped);
}
