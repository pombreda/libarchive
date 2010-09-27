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

	if (TRANSFORM_FATAL == transform_read_add_new_filter(t, (void *)w_data,
		"window", TRANSFORM_FILTER_WINDOW, window_read, window_skip,
		window_free, NULL, TRANSFORM_FILTER_IS_PASSTHRU |
			TRANSFORM_FILTER_SELF_BUFFERING))
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
	size_t desired = *bytes_read;

	if (0 == w_data->allowed) {
		*bytes_read = 0;
		return (TRANSFORM_EOF);
	}

	while (w_data->start) {
		consumed = transform_read_filter_consume(upstream, w_data->start);

		if (consumed < 0) {
			/* retry statuses? either way, pass back the error */
			return (consumed);
		} else if (consumed == 0) {
			*bytes_read = 0;
			*buff = NULL;
			return (TRANSFORM_EOF);
		}
		w_data->start -= consumed;
	}

	if (-1 != w_data->allowed) {
		if (desired > w_data->allowed) {
			/* minimal request is larger than what is allowed.  EOF it */
			desired = w_data->allowed;
		}
	}
		
	*buff = transform_read_filter_ahead(upstream, desired, &available);
	if (*buff) {
		*bytes_read = available;
		if (-1 != w_data->allowed) {
			if (available >= w_data->allowed) {
				*bytes_read = w_data->allowed;
				return (TRANSFORM_EOF);
			}
		}
		return (TRANSFORM_OK);
	} else if (available < 0) {
		/* error, and not an EOF error */
		return (available);
	}

	if (available == 0) {
		/* EOF on a boundary. */
		*buff = NULL;
		*bytes_read = 0;
		return (w_data->allowed > 0 ? TRANSFORM_PREMATURE_EOF : TRANSFORM_EOF);
	}
	/* finally, the fun one. request was too large.  grab what's available,
	 * return that size, and set EOF
	 */
	*buff = transform_read_filter_ahead(upstream, available, NULL);
	if (!*buff) {
		transform_set_error(t, TRANSFORM_ERRNO_PROGRAMMER,
			"windowed request %u failed- impossible unless read_filter or this filter "
			"is buggy", (unsigned int)available);
		return (TRANSFORM_FATAL);
	}
	*bytes_read = available;
	return (TRANSFORM_EOF);
}


static int64_t
window_skip(struct transform *t, void *_data,
	struct transform_read_filter *upstream, int64_t request)
{
	struct window_data *w_data = (struct window_data *)_data;
	int64_t skipped, allowed = request;

	if (w_data->allowed >= 0) {
		if (!w_data->allowed)
			return 0;
		if (request > w_data->allowed) {
			allowed = w_data->allowed;
		}
	}
	skipped = transform_read_filter_skip(upstream, allowed);
	if (skipped > 0 && -1 != w_data->allowed) {
		w_data->allowed -= skipped;
	}
	return (skipped);
}
