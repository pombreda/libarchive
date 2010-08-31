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
 * This file contains the "essential" portions of the read API, that
 * is, stuff that will probably always be used by any client that
 * actually needs to read an transform.  Optional pieces have been, as
 * far as possible, separated out into separate files to avoid
 * needlessly bloating statically-linked clients.
 */

#include "transform_platform.h"
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read.c 201157 2009-12-29 05:30:23Z kientzle $");

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

#include "transform.h"
#include "transform_private.h"
#include "transform_read_private.h"

#define minimum(a, b) (a < b ? a : b)

static int	build_stream(struct transform_read *);
static void	free_filters(struct transform_read *);
static void	free_bidders(struct transform_read *);
static int	close_filters(struct transform_read *);
static struct transform_vtable *transform_read_vtable(void);
static int64_t	_transform_filter_bytes(struct transform *, int);
static int	_transform_filter_code(struct transform *, int);
static int	_transform_visit_fds(struct transform *, transform_fd_visitor *,
	const void *);
static const char *_transform_filter_name(struct transform *, int);
static int  _transform_filter_count(struct transform *);
static int	_transform_read_close(struct transform *);
static int	_transform_read_free(struct transform *);
static int64_t  advance_file_pointer(struct transform_read_filter *, int64_t);

static struct transform_vtable *
transform_read_vtable(void)
{
	static struct transform_vtable av;
	static int inited = 0;

	if (!inited) {
		av.transform_filter_bytes = _transform_filter_bytes;
		av.transform_filter_code = _transform_filter_code;
		av.transform_filter_name = _transform_filter_name;
		av.transform_filter_count = _transform_filter_count;
		av.transform_free = _transform_read_free;
		av.transform_close = _transform_read_close;
		av.transform_visit_fds = _transform_visit_fds;
	}
	return (&av);
}

/*
 * Allocate, initialize and return a struct transform object.
 */
struct transform *
transform_read_new(void)
{
	struct transform_read *a;

	a = (struct transform_read *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->transform.marker.magic = TRANSFORM_READ_MAGIC;

	a->transform.marker.state = TRANSFORM_STATE_NEW;
	a->transform.vtable = transform_read_vtable();

	return (&a->transform);
}

/*
 * Set read options for the filter.
 */
int
transform_read_set_filter_options(struct transform *_a, const char *s)
{
	struct transform_read *a;
	struct transform_read_bidder *bidder;
	char key[64], val[64];
	int len, r;

	transform_check_magic(_a, TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW,
	    "transform_read_set_filter_options");

	if (s == NULL || *s == '\0')
		return (TRANSFORM_OK);
	a = (struct transform_read *)_a;
	len = 0;
	for(bidder = a->bidders; bidder != NULL; bidder = bidder->next) {
		if (bidder == NULL)
			continue;
		if (bidder->set_option == NULL)
			/* This bidder does not support option */
			continue;
		while ((len = __transform_parse_options(s, bidder->name,
		    sizeof(key), key, sizeof(val), val)) > 0) {
			if (val[0] == '\0')
				r = bidder->set_option(bidder->data, key, NULL);
			else
				r = bidder->set_option(bidder->data, key, val);
			if (r == TRANSFORM_FATAL)
				return (r);
			s += len;
		}
	}
	if (len < 0) {
		transform_set_error(&a->transform, TRANSFORM_ERRNO_MISC,
		    "Illegal format options.");
		return (TRANSFORM_WARN);
	}
	return (TRANSFORM_OK);
}

/*
 * Open the transform
 */
int
transform_read_open(struct transform *_a, void *client_data,
    transform_open_callback *client_opener,
    transform_read_callback *client_reader,
    transform_skip_callback *client_skipper,
    transform_close_callback *client_closer)
{
	return transform_read_open2(_a, client_data, client_opener,
		client_reader, client_skipper, client_closer, NULL);
}

int
transform_read_open2(struct transform *_a, void *client_data,
    transform_open_callback *client_opener,
    transform_read_callback *client_reader,
    transform_skip_callback *client_skipper,
    transform_close_callback *client_closer,
	transform_read_filter_visit_fds_callback *visit_fds)
{

	struct transform_read *a = (struct transform_read *)_a;
	struct transform_read_filter *filter;
	int e;

	transform_check_magic(_a, TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW,
	    "transform_read_open");
	transform_clear_error(&a->transform);

	if (client_reader == NULL)
		__transform_errx(1,
		    "No reader function provided to transform_read_open");

	/* Open data source. */
	if (client_opener != NULL) {
		e =(client_opener)(&a->transform, client_data);
		if (TRANSFORM_OK != e) {
			/* If the open failed, call the closer to clean up. */
			if (client_closer)
				(client_closer)(&a->transform, client_data);
			return (e);
		}
	}

	/* Save the client functions and mock up the initial source. */
	a->client.reader = client_reader;
	a->client.skipper = client_skipper;
	a->client.closer = client_closer;

	filter = calloc(1, sizeof(struct transform_read_filter));
	if (filter == NULL)
		return (TRANSFORM_FATAL);

	filter->marker.magic = TRANSFORM_READ_FILTER_MAGIC;
	filter->marker.state = TRANSFORM_STATE_DATA;
	filter->upstream = NULL;
	filter->transform = a;
	filter->data = client_data;
	filter->read = client_reader;
	filter->skip = client_skipper;
	filter->close = client_closer;
	filter->visit_fds = visit_fds;
	filter->name = "none";
	filter->code = TRANSFORM_FILTER_NONE;
	a->filter = filter;

	/* Build out the input pipeline. */
	e = build_stream(a);
	if (e == TRANSFORM_OK) {
		a->transform.marker.state = TRANSFORM_STATE_DATA;
		free_bidders(a);
	}

	return (e);
}

/*
 * Allow each registered stream transform to bid on whether
 * it wants to handle this stream.  Repeat until we've finished
 * building the pipeline.
 */
static int
build_stream(struct transform_read *a)
{
	int bid, best_bid, ret;
	struct transform_read_bidder *bidder, *best_bidder;
	ssize_t avail;

	for (;;) {
		best_bid = 0;
		best_bidder = NULL;

		bidder = a->bidders;
		for (bidder = a->bidders; NULL != bidder; bidder = bidder->next) {
			if (bidder->bid != NULL) {
				bid = (bidder->bid)(bidder->data, a->filter);
				if (bid > best_bid) {
					best_bid = bid;
					best_bidder = bidder;
				}
			}
		}

		/* If no bidder, we're done. */
		if (best_bidder == NULL) {
			/* Verify the filter by asking it for some data. */
			transform_read_filter_ahead(a->filter, 1, &avail);
			if (avail < 0) {
				close_filters(a);
				free_filters(a);
				return (TRANSFORM_FATAL);
			}
			return (TRANSFORM_OK);
		}

		ret = (best_bidder->create_filter)((struct transform *)a, best_bidder->data);

		/* cleanup and deallocation... */
		if (TRANSFORM_FATAL == ret) {
			close_filters(a);
			free_filters(a);
			return (TRANSFORM_FATAL);
		}
	}
}

static int
close_filters(struct transform_read *a)
{
	struct transform_read_filter *f = a->filter;
	int r = TRANSFORM_OK;
	/* Close each filter in the pipeline. */
	while (f != NULL) {
		struct transform_read_filter *t = f->upstream;
		if (f->close != NULL) {
			int r1 = (f->close)((struct transform *)a, f->data);
			if (r1 < r)
				r = r1;
			f->data = NULL;
		}
		if(f->buffer) {
			free(f->buffer);
		}
		f->buffer = NULL;
		f = t;
	}
	return r;
}

static void
free_filters(struct transform_read *a)
{
	while (a->filter != NULL) {
		struct transform_read_filter *t = a->filter->upstream;
		free(a->filter);
		a->filter = t;
	}
}

static void
free_bidders(struct transform_read *transform)
{
	struct transform_read_bidder *tmp;
	while (transform->bidders) {
		tmp = transform->bidders->next;
		transform->bidders->marker.magic = 0;
		free(transform->bidders);
		transform->bidders = tmp;
	}
}		

/*
 * return the count of # of filters in use
 */
static int
_transform_filter_count(struct transform *_a)
{
	struct transform_read *a = (struct transform_read *)_a;
	struct transform_read_filter *p = a->filter;
	int count = 0;
	while(p) {
		count++;
		p = p->upstream;
	}
	return count;
}

/*
 * invoke each filter with a visitor, so external code can find out
 * what fd's we own
 */
static int
_transform_visit_fds(struct transform *_a, transform_fd_visitor *visitor,
	const void *visitor_data)
{
	struct transform_read *a = (struct transform_read *)_a;
	struct transform_read_filter *p;
	int position, ret = TRANSFORM_OK;
	
	for(p=a->filter, position=0; NULL != p; position++, p = p->upstream) {
		if (p->visit_fds) {
			if(TRANSFORM_OK != (ret = (p->visit_fds)(_a, p->data,
				visitor, visitor_data))) {
				break;
			}
		}
	}
	return (ret);
}

/*
 * Close the file and all I/O.
 */
static int
_transform_read_close(struct transform *_a)
{
	struct transform_read *a = (struct transform_read *)_a;
	int r = TRANSFORM_OK, r1 = TRANSFORM_OK;

	transform_check_magic(&a->transform, TRANSFORM_READ_MAGIC,
	    TRANSFORM_STATE_ANY | TRANSFORM_STATE_FATAL, "transform_read_close");
	transform_clear_error(&a->transform);
	if (a->transform.marker.state != TRANSFORM_STATE_FATAL)
		a->transform.marker.state = TRANSFORM_STATE_CLOSED;

	/* TODO: Clean up the formatters. */

	/* Release the filter objects. */
	r1 = close_filters(a);
	if (r1 < r)
		r = r1;

	return (r);
}

/*
 * Release memory and other resources.
 */
static int
_transform_read_free(struct transform *_a)
{
	struct transform_read *a = (struct transform_read *)_a;
	int i, n;
	int r = TRANSFORM_OK;

	if (_a == NULL)
		return (TRANSFORM_OK);
	transform_check_magic(_a, TRANSFORM_READ_MAGIC,
	    TRANSFORM_STATE_ANY | TRANSFORM_STATE_FATAL, "transform_read_free");
	if (a->transform.marker.state != TRANSFORM_STATE_CLOSED
	    && a->transform.marker.state != TRANSFORM_STATE_FATAL)
		r = transform_read_close(&a->transform);

	/* Free the filters.  Note this is safe to invoke multiple times. */
	free_filters(a);

	/* Free the bidders.  Note this is safe to invoke multiple times. */
	free_bidders(a);

	/* Release the bidder objects. */
	n = sizeof(a->bidders)/sizeof(a->bidders[0]);
	for (i = 0; i < n; i++) {
		if (a->bidders[i].free != NULL) {
			int r1 = (a->bidders[i].free)(&a->bidders[i]);
			if (r1 < r)
				r = r1;
		}
	}

	transform_string_free(&a->transform.error_string);
	a->transform.marker.magic = 0;
	free(a);
	return (r);
}

static struct transform_read_filter *
get_filter(struct transform *_a, int n)
{
	struct transform_read *a = (struct transform_read *)_a;
	struct transform_read_filter *f = a->filter;
	/* We use n == -1 for 'the last filter', which is always the client proxy. */
	if (n == -1 && f != NULL) {
		struct transform_read_filter *last = f;
		f = f->upstream;
		while (f != NULL) {
			last = f;
			f = f->upstream;
		}
		return (last);
	}
	if (n < 0)
		return NULL;
	while (n > 0 && f != NULL) {
		f = f->upstream;
		--n;
	}
	return (f);
}

static int
_transform_filter_code(struct transform *_a, int n)
{
	struct transform_read_filter *f = get_filter(_a, n);
	return f == NULL ? -1 : f->code;
}

static const char *
_transform_filter_name(struct transform *_a, int n)
{
	struct transform_read_filter *f = get_filter(_a, n);
	return f == NULL ? NULL : f->name;
}

static int64_t
_transform_filter_bytes(struct transform *_a, int n)
{
	struct transform_read_filter *f = get_filter(_a, n);
	return f == NULL ? -1 : f->bytes_consumed;
}


/*
 * create a new bidder, adding it to this transform
 */
struct transform_read_bidder *
transform_read_bidder_add(struct transform *_t,
	const void *bidder_data, const char *bidder_name,
	transform_read_bidder_bid_method *bid,
	transform_read_bidder_create_filter *create_filter,
	transform_read_bidder_free *dealloc,
	transform_read_bidder_set_option *set_option)
{
	struct transform_read *t = (struct transform_read *)_t;
	struct transform_read_bidder *bidder;

	if(TRANSFORM_OK != __transform_check_magic(_t, &(_t->marker), 
		TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW, "transform",
		"transform_read_bidder_add")) {
		return (NULL);
	}

	bidder = calloc(1, sizeof(struct transform_read_bidder));
	if (!bidder) {
		transform_set_error(_t, ENOMEM,
			"bidder allocation failed");
		return (NULL);
	}
	bidder->marker.magic = TRANSFORM_READ_BIDDER_MAGIC;
	bidder->marker.state = TRANSFORM_STATE_NEW;
	bidder->name = bidder_name;
	bidder->data = (void *)bidder_data;
	bidder->bid = bid;
	bidder->create_filter = create_filter;
	bidder->free = dealloc;
	bidder->set_option = set_option;
	bidder->next = t->bidders;
	t->bidders = bidder;
	return (bidder);
}

/*
 * add a new filter to the stack
 */

int transform_read_filter_add(struct transform *_t,
	const void *data, const char *name, int code,
	transform_read_filter_read_callback *reader,
	transform_read_filter_skip_callback *skipper,
	transform_read_filter_close_callback *closer,
	transform_read_filter_visit_fds_callback *visit_fds)
{
	struct transform_read *t = (struct transform_read *)_t;
	struct transform_read_filter *f;

	transform_check_magic(_t, TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW,
		"transform_read_filter_add");

	if (NULL == (f = calloc(1, sizeof(struct transform_read_filter)))) {
		transform_set_error(_t, ENOMEM,
			"failed to allocate a read filter");
		return (TRANSFORM_FATAL);
	}

	f->marker.magic = TRANSFORM_READ_FILTER_MAGIC;
	f->marker.state = TRANSFORM_STATE_DATA;

	f->code = code;
	f->name = name;
	f->transform = t;
	f->data = (void *)data;
	f->read = reader;
	f->skip = skipper;
	f->close = closer;
	f->visit_fds = visit_fds;
	f->upstream = t->filter;
	t->filter = f;

	return (TRANSFORM_OK);
}


/*
 * The next section implements the peek/consume internal I/O
 * system used by transform readers.  This system allows simple
 * read-ahead for consumers while preserving zero-copy operation
 * most of the time.
 *
 * The two key operations:
 *  * The read-ahead function returns a pointer to a block of data
 *    that satisfies a minimum request.
 *  * The consume function advances the file pointer.
 *
 * In the ideal case, filters generate blocks of data
 * and transform_read_ahead() just returns pointers directly into
 * those blocks.  Then transform_read_consume() just bumps those
 * pointers.  Only if your request would span blocks does the I/O
 * layer use a copy buffer to provide you with a contiguous block of
 * data.
 *
 * A couple of useful idioms:
 *  * "I just want some data."  Ask for 1 byte and pay attention to
 *    the "number of bytes available" from transform_read_ahead().
 *    Consume whatever you actually use.
 *  * "I want to output a large block of data."  As above, ask for 1 byte,
 *    emit all that's available (up to whatever limit you have), consume
 *    it all, then repeat until you're done.  This effectively means that
 *    you're passing along the blocks that came from your provider.
 *  * "I want to peek ahead by a large amount."  Ask for 4k or so, then
 *    double and repeat until you get an error or have enough.  Note
 *    that the I/O layer will likely end up expanding its copy buffer
 *    to fit your request, so use this technique cautiously.  This
 *    technique is used, for example, by some of the format tasting
 *    code that has uncertain look-ahead needs.
 *
 * TODO: Someday, provide a more generic __transform_read_seek() for
 * those cases where it's useful.  This is tricky because there are lots
 * of cases where seek() is not available (reading gzip data from a
 * network socket, for instance), so there needs to be a good way to
 * communicate whether seek() is available and users of that interface
 * need to use non-seeking strategies whenever seek() is not available.
 */

/*
 * Looks ahead in the input stream:
 *  * If 'avail' pointer is provided, that returns number of bytes available
 *    in the current buffer, which may be much larger than requested.
 *  * If end-of-file, *avail gets set to zero.
 *  * If error, *avail gets error code.
 *  * If request can be met, returns pointer to data.
 *  * If minimum request cannot be met, returns NULL.
 *    if request is not met.
 *
 * Note: If you just want "some data", ask for 1 byte and pay attention
 * to *avail, which will have the actual amount available.  If you
 * know exactly how many bytes you need, just ask for that and treat
 * a NULL return as an error.
 *
 * Important:  This does NOT move the file pointer.  See
 * transform_read_consume() below.
 */
const void *
transform_read_ahead(struct transform *_a, size_t min, ssize_t *avail)
{
	struct transform_read *a = (struct transform_read *)_a;
	if (TRANSFORM_OK != __transform_check_magic(_a, &(_a->marker),
		TRANSFORM_READ_MAGIC, TRANSFORM_STATE_DATA, 
		"transform", "transform_read_ahead"))
		return (NULL);
	return (transform_read_filter_ahead(a->filter, min, avail));
}

const void *
transform_read_filter_ahead(struct transform_read_filter *filter,
    size_t min, ssize_t *avail)
{
	ssize_t bytes_read;
	size_t tocopy;

	if (TRANSFORM_FATAL == __transform_filter_check_magic(filter,
		TRANSFORM_READ_FILTER_MAGIC, TRANSFORM_STATE_DATA,
		"transform_read_filter_ahead")) {
		return (NULL);
	}

	if (filter->fatal) {
		if (avail)
			*avail = TRANSFORM_FATAL;
		return (NULL);
	}

	/*
	 * Keep pulling more data until we can satisfy the request.
	 */
	for (;;) {

		/*
		 * If we can satisfy from the copy buffer (and the
		 * copy buffer isn't empty), we're done.  In particular,
		 * note that min == 0 is a perfectly well-defined
		 * request.
		 */
		if (filter->avail >= min && filter->avail > 0) {
			if (avail != NULL)
				*avail = filter->avail;
			return (filter->next);
		}

		/*
		 * We can satisfy directly from client buffer if everything
		 * currently in the copy buffer is still in the client buffer.
		 */
		if (filter->client_total >= filter->client_avail + filter->avail
		    && filter->client_avail + filter->avail >= min) {
			/* "Roll back" to client buffer. */
			filter->client_avail += filter->avail;
			filter->client_next -= filter->avail;
			/* Copy buffer is now empty. */
			filter->avail = 0;
			filter->next = filter->buffer;
			/* Return data from client buffer. */
			if (avail != NULL)
				*avail = filter->client_avail;
			return (filter->client_next);
		}

		/* Move data forward in copy buffer if necessary. */
		if (filter->next > filter->buffer &&
		    filter->next + min > filter->buffer + filter->buffer_size) {
			if (filter->avail > 0)
				memmove(filter->buffer, filter->next, filter->avail);
			filter->next = filter->buffer;
		}

		/* If we've used up the client data, get more. */
		if (filter->client_avail <= 0) {
			if (filter->end_of_file) {
				if (avail != NULL)
					*avail = 0;
				return (NULL);
			}
			bytes_read = (filter->read)((struct transform *)filter->transform, filter->data,
			    filter->upstream, &filter->client_buff);
			if (bytes_read < 0) {		/* Read error. */
				filter->client_total = filter->client_avail = 0;
				filter->client_next = filter->client_buff = NULL;
				filter->fatal = 1;
				if (avail != NULL)
					*avail = TRANSFORM_FATAL;
				return (NULL);
			}
			if (bytes_read == 0) {	/* Premature end-of-file. */
				filter->client_total = filter->client_avail = 0;
				filter->client_next = filter->client_buff = NULL;
				filter->end_of_file = 1;
				/* Return whatever we do have. */
				if (avail != NULL)
					*avail = filter->avail;
				return (NULL);
			}
			filter->client_total = bytes_read;
			filter->client_avail = filter->client_total;
			filter->client_next = filter->client_buff;
		}
		else
		{
			/*
			 * We can't satisfy the request from the copy
			 * buffer or the existing client data, so we
			 * need to copy more client data over to the
			 * copy buffer.
			 */

			/* Ensure the buffer is big enough. */
			if (min > filter->buffer_size) {
				size_t s, t;
				char *p;

				/* Double the buffer; watch for overflow. */
				s = t = filter->buffer_size;
				if (s == 0)
					s = min;
				while (s < min) {
					t *= 2;
					if (t <= s) { /* Integer overflow! */
						transform_set_error(
							&filter->transform->transform,
							ENOMEM,
						    "Unable to allocate copy buffer");
						filter->fatal = 1;
						if (avail != NULL)
							*avail = TRANSFORM_FATAL;
						return (NULL);
					}
					s = t;
				}
				/* Now s >= min, so allocate a new buffer. */
				p = (char *)malloc(s);
				if (p == NULL) {
					transform_set_error(
						&filter->transform->transform,
						ENOMEM,
					    "Unable to allocate copy buffer");
					filter->fatal = 1;
					if (avail != NULL)
						*avail = TRANSFORM_FATAL;
					return (NULL);
				}
				/* Move data into newly-enlarged buffer. */
				if (filter->avail > 0)
					memmove(p, filter->next, filter->avail);
				free(filter->buffer);
				filter->next = filter->buffer = p;
				filter->buffer_size = s;
			}

			/* We can add client data to copy buffer. */
			/* First estimate: copy to fill rest of buffer. */
			tocopy = (filter->buffer + filter->buffer_size)
			    - (filter->next + filter->avail);
			/* Don't waste time buffering more than we need to. */
			if (tocopy + filter->avail > min)
				tocopy = min - filter->avail;
			/* Don't copy more than is available. */
			if (tocopy > filter->client_avail)
				tocopy = filter->client_avail;

			memcpy(filter->next + filter->avail, filter->client_next,
			    tocopy);
			/* Remove this data from client buffer. */
			filter->client_next += tocopy;
			filter->client_avail -= tocopy;
			/* add it to copy buffer. */
			filter->avail += tocopy;
		}
	}
}

/*
 * Move the file pointer forward.  This should be called after
 * transform_read_ahead() returns data to you.  Don't try to move
 * ahead by more than the amount of data available according to
 * transform_read_ahead().
 */
int64_t
transform_read_consume(struct transform *_a, int64_t request)
{
	struct transform_read *a = (struct transform_read *)_a;
	transform_check_magic(_a, TRANSFORM_READ_MAGIC,
		TRANSFORM_STATE_DATA, "transform_read_consume");

	return (transform_read_filter_consume(a->filter, request));
}

int64_t
transform_read_filter_consume(struct transform_read_filter *filter,
    int64_t request)
{
	int64_t skipped;

	if (TRANSFORM_FATAL == __transform_filter_check_magic(filter,
		TRANSFORM_READ_FILTER_MAGIC, TRANSFORM_STATE_DATA,
		"transform_read_filter_consume")) {
		return (TRANSFORM_FATAL);
	}

	skipped = advance_file_pointer(filter, request);

	if (skipped == request)
		return (skipped);
	/* We hit EOF before we satisfied the skip request. */
	if (skipped < 0)  // Map error code to 0 for error message below.
		skipped = 0;
	transform_set_error(&filter->transform->transform,
	    TRANSFORM_ERRNO_MISC,
	    "Truncated input file (needed %jd bytes, only %jd available)",
	    (intmax_t)request, (intmax_t)skipped);
	return (TRANSFORM_FATAL);
}

/*
 * Advance the file pointer by the amount requested.
 * Returns the amount actually advanced, which may be less than the
 * request if EOF is encountered first.
 * Returns a negative value if there's an I/O error.
 */
static int64_t
advance_file_pointer(struct transform_read_filter *filter, int64_t request)
{
	int64_t bytes_skipped, total_bytes_skipped = 0;
	ssize_t bytes_read;
	size_t min;

	if (filter->fatal)
		return (-1);

	/* Use up the copy buffer first. */
	if (filter->avail > 0) {
		min = minimum(request, (int64_t)filter->avail);
		filter->next += min;
		filter->avail -= min;
		request -= min;
		filter->bytes_consumed += min;
		total_bytes_skipped += min;
	}

	/* Then use up the client buffer. */
	if (filter->client_avail > 0) {
		min = minimum(request, (int64_t)filter->client_avail);
		filter->client_next += min;
		filter->client_avail -= min;
		request -= min;
		filter->bytes_consumed += min;
		total_bytes_skipped += min;
	}
	if (request == 0)
		return (total_bytes_skipped);

	/* If there's an optimized skip function, use it. */
	if (filter->skip != NULL) {
		bytes_skipped = (filter->skip)((struct transform *)filter->transform, 
			filter->data, request);
		if (bytes_skipped < 0) {	/* error */
			filter->fatal = 1;
			return (bytes_skipped);
		}
		filter->bytes_consumed += bytes_skipped;
		total_bytes_skipped += bytes_skipped;
		request -= bytes_skipped;
		if (request == 0)
			return (total_bytes_skipped);
	}

	/* Use ordinary reads as necessary to complete the request. */
	for (;;) {
		bytes_read = (filter->read)((struct transform *)filter->transform,
			filter->data, filter->upstream, &filter->client_buff);

		if (bytes_read < 0) {
			filter->client_buff = NULL;
			filter->fatal = 1;
			return (bytes_read);
		}

		if (bytes_read == 0) {
			filter->client_buff = NULL;
			filter->end_of_file = 1;
			return (total_bytes_skipped);
		}

		if (bytes_read >= request) {
			filter->client_next =
			    ((const char *)filter->client_buff) + request;
			filter->client_avail = bytes_read - request;
			filter->client_total = bytes_read;
			total_bytes_skipped += request;
			return (total_bytes_skipped);
		}

		total_bytes_skipped += bytes_read;
		request -= bytes_read;
	}
}

int64_t
transform_read_bytes_consumed(struct transform *_a)
{
	struct transform_read *a = (struct transform_read *)_a;
	transform_check_magic(_a, TRANSFORM_READ_MAGIC,
		TRANSFORM_STATE_DATA, "transform_read_bytes_consumed");

	return a->filter->bytes_consumed;
}

int
transform_is_read(struct transform *t)
{
	return (t->marker.magic == TRANSFORM_READ_MAGIC);
}
