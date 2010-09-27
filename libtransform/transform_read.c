/*-
 * Copyright (c) 2010 Brian Harring
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

#ifndef assert
#define assert(x) (void)0
#endif

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
static int  _filter_read(struct transform_read_filter *filter, void **buff, size_t *buff_size);

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

	a = (struct transform_read *)calloc(1, sizeof(*a));
	if (a == NULL)
		return (NULL);

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
    transform_read_callback *client_reader,
    transform_skip_callback *client_skipper,
    transform_close_callback *client_closer)
{
	return transform_read_open2(_a, client_data,
		client_reader, client_skipper, client_closer, NULL, 0);
}

int
transform_read_open2(struct transform *_t, void *client_data,
    transform_read_callback *client_reader,
    transform_skip_callback *client_skipper,
    transform_close_callback *client_closer,
	transform_read_filter_visit_fds_callback *visit_fds,
	int64_t flags)
{
	struct transform_read_filter *filter = NULL;

	transform_check_magic(_t, TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW,
	    "transform_read_open");

	filter = transform_read_filter_new(client_data, "none", TRANSFORM_FILTER_NONE,
		client_reader, client_skipper, client_closer, visit_fds,
		flags | TRANSFORM_FILTER_SOURCE);

	if (!filter) {
		return TRANSFORM_FATAL;
	}

	/* flaws here, and why this api needs to die.  we don't know
	 * if build_stream failed, or if the filter addition did-
	 * either way, we don't know if we need to do a freeing, thus
	 * this is a leaky api in the error case
	 */
	return transform_read_open_source(_t, filter);
}

int
transform_read_open_source(struct transform *_t,
	struct transform_read_filter *source)
{
	struct transform_read *t = (struct transform_read *)_t;
	int e;

	transform_check_magic(_t, TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW,
	    "transform_read_open");

	if (TRANSFORM_FATAL == __transform_filter_check_magic2(_t, source,
		TRANSFORM_READ_FILTER_MAGIC, TRANSFORM_STATE_NEW,
			"transform_read_open_source")) {
		return (TRANSFORM_FATAL);
	}

	transform_clear_error(_t);

	if (!(source->base.flags & TRANSFORM_FILTER_SOURCE)) {
		transform_set_error(_t, TRANSFORM_ERRNO_PROGRAMMER,
			"filter given to transform_read_open_source isn't marked as a "
			"source filter");
		return (TRANSFORM_FATAL);
	}

	e = transform_read_add_filter(_t, source);

	if (TRANSFORM_FATAL == e) {
		return (TRANSFORM_FATAL);
	}

	/* Build out the input pipeline. */
	e = build_stream(t);
	if (e == TRANSFORM_OK) {
		t->transform.marker.state = TRANSFORM_STATE_DATA;
		free_bidders(t);
	}

	return (e);
}

/*
 * Allow each registered stream transform to bid on whether
 * it wants to handle this stream.  Repeat until we've finished
 * building the pipeline.
 */
static int
build_stream(struct transform_read *t)
{
	int ret;
	struct transform_read_bidder *bidder;
	ssize_t avail;

	bidder = t->bidders;
	while (bidder) {
		ret = (bidder->create_filter)((struct transform *)t, bidder->data);

		if (TRANSFORM_FATAL == ret) {
			close_filters(t);
			free_filters(t);
			return (TRANSFORM_FATAL);
		}

		/* do a single byte read to verify this filter transforms properly */
		transform_read_filter_ahead(t->filter, 1, &avail);
		if (TRANSFORM_FATAL == avail) {
			/* 
			 * this will ignore eof and warnings.
			 * we do not view an EOF as an error here- if a later
			 * opener requires a read and fails, than it's a failure
			 */
			ret = TRANSFORM_FATAL;
		} else if (0 == avail && TRANSFORM_PREMATURE_EOF == t->filter->end_of_file) {
			ret = TRANSFORM_FATAL;
		}			

		if (TRANSFORM_FATAL == ret) {
			close_filters(t);
			free_filters(t);
			return (TRANSFORM_FATAL);
		}
		bidder = bidder->next;
	}
	return (TRANSFORM_OK);
}

static int
close_filters(struct transform_read *a)
{
	struct transform_read_filter *f = a->filter;
	int r = TRANSFORM_OK;
	/* Close each filter in the pipeline. */
	while (f != NULL) {
		struct transform_read_filter *t = f->base.upstream.read;
		if (f->close != NULL) {
			int r1 = (f->close)((struct transform *)a, f->base.data);
			if (r1 < r)
				r = r1;
			f->base.data = NULL;
		}
		if(f->resizing.buffer) {
			free(f->resizing.buffer);
		}
		f->resizing.buffer = NULL;
		if(f->managed_buffer) {
			free(f->managed_buffer);
		}
		f->managed_buffer = NULL;
		f = t;
	}
	return r;
}

static void
free_filters(struct transform_read *a)
{
	while (a->filter != NULL) {
		struct transform_read_filter *t = a->filter->base.upstream.read;
		free(a->filter);
		a->filter = t;
	}
}

int
transform_read_bidder_free(struct transform_read_bidder *bidder)
{
	int ret = TRANSFORM_OK;

	bidder->marker.magic = 0;
	bidder->marker.state = TRANSFORM_STATE_FATAL;

	if (bidder->free) {
		ret = (bidder->free)(bidder->data);
	}

	free(bidder);

	return (ret);
}

static void
free_bidders(struct transform_read *transform)
{
	struct transform_read_bidder *tmp;
	int ret = TRANSFORM_OK;
	while (transform->bidders) {
		tmp = transform->bidders->next;
		int ret2 = transform_read_bidder_free(transform->bidders);
		if (ret2 < ret) {
			ret = ret2;
		}

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
		p = p->base.upstream.read;
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
	
	for(p=a->filter, position=0; NULL != p; position++, p = p->base.upstream.read) {
		if (p->visit_fds) {
			if(TRANSFORM_OK != (ret = (p->visit_fds)(_a, p->base.data,
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

	transform_string_free(&a->transform.error_string);
	a->transform.marker.magic = 0;
	free(a);
	return (r);
}

struct transform_read_filter *
transform_read_get_filter(struct transform *_t, int n)
{
	struct transform_read *t = (struct transform_read *)_t;
	struct transform_read_filter *f;
	if(TRANSFORM_OK != __transform_check_magic(_t, &(_t->marker), 
		TRANSFORM_READ_MAGIC, TRANSFORM_STATE_ANY, "transform",
		"transform_read_get_filter")) {
		return (NULL);
	}
	/* We use n == -1 for 'the last filter', which is always the source filter */
	f = t->filter;
	while ((n > 0 && f) || (n <= -1 && f->base.upstream.read)) {
		f = f->base.upstream.read;
		n--;
	}

	return f;
}

static int
_transform_filter_code(struct transform *_t, int n)
{
	struct transform_read_filter *f = transform_read_get_filter(_t, n);
	return f ? f->base.code : -1;
}

static const char *
_transform_filter_name(struct transform *_t, int n)
{
	struct transform_read_filter *f = transform_read_get_filter(_t, n);
	return f == NULL ? NULL : f->base.name;
}

static int64_t
_transform_filter_bytes(struct transform *_t, int n)
{
	struct transform_read_filter *f = transform_read_get_filter(_t, n);
	return f ? f->base.bytes_consumed : -1;
}

struct transform_read_bidder *
transform_read_bidder_create(
	const void *bidder_data, const char *bidder_name,
	transform_read_bidder_bid_callback *bid,
	transform_read_bidder_create_filter_callback *create_filter,
	transform_read_bidder_free_callback *dealloc,
	transform_read_bidder_set_option_callback *set_option)
{
	struct transform_read_bidder *bidder;

	bidder = calloc(1, sizeof(struct transform_read_bidder));
	if (!bidder) {
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
	bidder->next = NULL;
	return (bidder);
}
/*
 * create a new bidder, adding it to this transform
 */
struct transform_read_bidder *
transform_read_bidder_add(struct transform *_t,
	const void *bidder_data, const char *bidder_name,
	transform_read_bidder_bid_callback *bid,
	transform_read_bidder_create_filter_callback *create_filter,
	transform_read_bidder_free_callback *dealloc,
	transform_read_bidder_set_option_callback *set_option)
{
	struct transform_read *t = (struct transform_read *)_t;
	struct transform_read_bidder *bidder, *ptr;

	if(TRANSFORM_OK != __transform_check_magic(_t, &(_t->marker), 
		TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW, "transform",
		"transform_read_bidder_add")) {
		return (NULL);
	}

	bidder = transform_read_bidder_create(bidder_data, bidder_name,
		bid, create_filter, dealloc, set_option);

	if (!bidder) {
		transform_set_error(_t, ENOMEM,
			"bidder allocation failed");
		return (NULL);
	}

	if (t->bidders) {
		ptr = t->bidders;
		while (ptr->next) {
			ptr = ptr->next;
		}
		ptr->next = bidder;
	} else {
		t->bidders = bidder;
	}
	bidder->next = NULL;
	return (bidder);
}


struct transform_read_filter *
transform_read_filter_new(const void *data, const char *name,
	int code, 
	transform_read_filter_read_callback *reader,
	transform_read_filter_skip_callback *skipper,
	transform_read_filter_close_callback *closer,
	transform_read_filter_visit_fds_callback *visit_fds,
	int64_t flags)
{
	struct transform_read_filter *f;

	if (!reader) {
		return (NULL);
	}
	
	if (NULL == (f = calloc(1, sizeof(struct transform_read_filter)))) {
		return (NULL);
	}

	f->base.marker.magic = TRANSFORM_READ_FILTER_MAGIC;
	f->base.marker.state = TRANSFORM_STATE_NEW;

	f->base.code = code;
	f->base.name = name;
	f->base.transform = NULL;
	f->base.data = (void *)data;
	f->read = reader;
	f->skip = skipper;
	f->close = closer;
	f->visit_fds = visit_fds;
	f->base.upstream.read = NULL;
	f->base.flags = flags;
	f->alignment = 1;
	return (f);
}

int
transform_read_filter_finalize(struct transform_read_filter *filter)
{
	int ret = TRANSFORM_OK;
	if (TRANSFORM_FATAL == __transform_filter_check_magic(filter,
		TRANSFORM_READ_FILTER_MAGIC, TRANSFORM_STATE_NEW,
		"transform_read_filter_skip")) {
		return (TRANSFORM_FATAL);
	}
	/* mark it; buffering setting will not alloc if it's in a new state
	 * we could alloc ourselves, but the redundancy is pointless.
	 */
	filter->base.marker.state = TRANSFORM_STATE_DATA;
	if (!(filter->base.flags & TRANSFORM_FILTER_SELF_BUFFERING)) {
		ret = transform_read_filter_set_buffering(filter,
			filter->managed_size ? filter->managed_size : 64 * 1024);
	}
	if (TRANSFORM_OK != ret) {
		/* revert it back,  possibly go fatal instead? */
		filter->base.marker.state = TRANSFORM_STATE_NEW;
	}
	return ret;
}

int
transform_read_filter_set_buffering(struct transform_read_filter *filter, size_t size)
{
	if (TRANSFORM_FATAL == __transform_filter_check_magic(filter,
		TRANSFORM_READ_FILTER_MAGIC, TRANSFORM_STATE_DATA | TRANSFORM_STATE_NEW,
		"transform_read_set_buffering")) {
		return (TRANSFORM_FATAL);
	}
	if (filter->base.flags & TRANSFORM_FILTER_SELF_BUFFERING) {
		if (filter->base.transform) {
			transform_set_error((struct transform *)filter->base.transform,
				TRANSFORM_ERRNO_PROGRAMMER,
				"filter is self managing");
		}
		return (TRANSFORM_FATAL);
	}

	/* floor it to an aligned size */
	size /= filter->alignment;
	if (size <= 1) {
		/* validate this heuristic- assumption is two windows of data is best, may not be */
		size = 2;
	}
	size *= filter->alignment;
	
	filter->managed_size = size;

	/* only realloc the buffer if we're in a data state. */
	if (filter->base.marker.state == TRANSFORM_STATE_DATA) {
		if(NULL == (filter->managed_buffer = realloc(filter->managed_buffer, size))) {
			if (filter->base.transform) {
				transform_set_error((struct transform *)filter->base.transform, ENOMEM,
					"memory allocation for filter buffer of size %lu failed",
					(unsigned long)size);
			}
			return (TRANSFORM_FATAL);
		}
	}
	return (TRANSFORM_OK);
}	

int
transform_read_filter_set_alignment(struct transform_read_filter *filter, size_t size)
{
	if (TRANSFORM_FATAL == __transform_filter_check_magic(filter,
		TRANSFORM_READ_FILTER_MAGIC, TRANSFORM_STATE_DATA | TRANSFORM_STATE_NEW,
		"transform_read_filter_skip")) {
		return (TRANSFORM_FATAL);
	}
	filter->alignment = size;
	/* realign the buffering */
	return transform_read_filter_set_buffering(filter, filter->managed_size);
}


/*
 * add a new filter to the stack
 */

int
transform_read_add_new_filter(struct transform *_t,
	const void *data, const char *name, int code,
	transform_read_filter_read_callback *reader,
	transform_read_filter_skip_callback *skipper,
	transform_read_filter_close_callback *closer,
	transform_read_filter_visit_fds_callback *visit_fds,
	int64_t flags)
{
	struct transform_read_filter *f;

	transform_check_magic(_t, TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW,
		"transform_read_add_new_filter");

	f = transform_read_filter_new(data, name, code, reader, skipper, closer,
		visit_fds, flags);

	if (!f) {
		transform_set_error(_t, ENOMEM,
			"failed to allocate a read filter");
		return (TRANSFORM_FATAL);
	}

	if (TRANSFORM_OK == transform_read_add_filter(_t, f)) {
		return (TRANSFORM_OK);
	}
	free(f);
	return (TRANSFORM_FATAL);
}

int
transform_read_add_filter(struct transform *_t,
	struct transform_read_filter *filter)
{
	struct transform_read *t = (struct transform_read *)_t;
	transform_check_magic(_t, TRANSFORM_READ_MAGIC, TRANSFORM_STATE_NEW,
		"transform_read_add_new_filter");

	if (TRANSFORM_FATAL == __transform_filter_check_magic2(_t, filter,
		TRANSFORM_READ_FILTER_MAGIC, TRANSFORM_STATE_NEW,
		"transform_read_add_filter")) {
		return (TRANSFORM_FATAL);
	}

	if ((filter->base.flags & TRANSFORM_FILTER_SOURCE) && t->filter) {
		transform_set_error(_t, TRANSFORM_ERRNO_PROGRAMMER,
			"filter %s is a source filter, but this transform already has filters added",
			filter->base.name);
		return (TRANSFORM_FATAL);
	}

	filter->base.transform = _t;
	if (TRANSFORM_OK != transform_read_filter_finalize(filter)) {
		return (TRANSFORM_FATAL);
	}
	filter->base.upstream.read = t->filter;
	t->filter = filter;

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

static int
_filter_read(struct transform_read_filter *filter, void **buff, size_t *buff_size)
{
	int ret;

	if (filter->end_of_file) {
		*buff_size = 0;
		return (TRANSFORM_FATAL);
	}

	ret = (filter->read)((struct transform *)filter->base.transform, filter->base.data,
		    filter->base.upstream.read, (const void **)buff, buff_size);

	if (TRANSFORM_FATAL == ret) {
		filter->fatal = 1;
		return (TRANSFORM_FATAL);
	} else if (TRANSFORM_EOF == ret || TRANSFORM_PREMATURE_EOF == ret) {
		filter->end_of_file = ret;
	}
	return (TRANSFORM_OK);
}

const void *
transform_read_filter_ahead(struct transform_read_filter *filter,
    size_t min, ssize_t *avail)
{
	size_t tocopy;
	int ret;

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

	if (filter->base.flags & TRANSFORM_FILTER_IS_PASSTHRU) {
		/* handle this seperately from the resizing crap below. */
		
		if (filter->client.avail >= min) {
			if (avail)
				*avail = filter->client.avail;
			return (filter->client.next);
		} else if (filter->end_of_file) {
			if (avail)
				*avail = filter->client.avail;
			return (NULL);
		}
		filter->client.avail = min;

		ret = _filter_read(filter, (void **)&filter->client.buffer,
			&filter->client.avail);

		filter->client.next = filter->client.buffer;
		filter->client.size = filter->client.avail;

		if (TRANSFORM_FATAL == ret) {
			filter->client.size = filter->client.avail = 0;
			filter->client.next = filter->client.buffer = NULL;
			if (avail != NULL)
				*avail = TRANSFORM_FATAL;
			return (NULL);
		}
		
		if (avail)
			*avail = filter->client.avail;

		if (min > filter->client.avail) {
			/* EOF was encountered, or a broken filter */
			if (!filter->end_of_file) {
				transform_set_error(filter->base.transform, TRANSFORM_ERRNO_PROGRAMMER,
					"pass thru filter %s was asked for %u, returned %u, but didn't "
					"set EOF", filter->base.name, (unsigned int)min,
					(unsigned int)filter->client.avail);
			}
			return (NULL);
		}
		return (filter->client.next);
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
		if (filter->resizing.avail >= min && filter->resizing.avail > 0) {
			if (avail != NULL)
				*avail = filter->resizing.avail;
			return (filter->resizing.next);
		}

		/*
		 * We can satisfy directly from client buffer if everything
		 * currently in the copy buffer is still in the client buffer.
		 */
		if (filter->client.size >= filter->client.avail + filter->resizing.avail
		    && filter->client.avail + filter->resizing.avail >= min) {

			/* "Roll back" to client buffer. */
			filter->client.avail += filter->resizing.avail;
			filter->client.next -= filter->resizing.avail;

			/* Copy buffer is now empty. */
			filter->resizing.avail = 0;
			filter->resizing.next = filter->resizing.buffer;

			/* Return data from client buffer. */
			if (avail != NULL)
				*avail = filter->client.avail;
			return (filter->client.next);
		}

		/* Move data forward in copy buffer if necessary. */
		if (filter->resizing.next > filter->resizing.buffer &&
		    filter->resizing.next + min > filter->resizing.buffer + filter->resizing.size) {
			if (filter->resizing.avail > 0)
				memmove(filter->resizing.buffer, filter->resizing.next, filter->resizing.avail);
			filter->resizing.next = filter->resizing.buffer;
		}

		/* If we've used up the client data, get more. */
		if (filter->client.avail <= 0) {
			if (filter->end_of_file) {
				if (avail != NULL)
					*avail = 0;
				return (NULL);
			}

			filter->client.buffer = filter->managed_buffer;
			filter->client.avail = filter->client.size = filter->managed_size;

			ret = _filter_read(filter, (void **)&filter->client.buffer,
				&filter->client.avail);

			if (TRANSFORM_FATAL == ret) {
				filter->client.size = filter->client.avail = 0;
				filter->client.next = filter->client.buffer = NULL;
				if (avail != NULL)
					*avail = TRANSFORM_FATAL;
				return (NULL);
			}

			/* only dupe the size if we cannot know it's size due to it being an
			 * unmanaged buffer */
			if (filter->client.buffer != filter->managed_buffer)
				filter->client.size = filter->client.avail;

			if (! filter->client.avail) {
				filter->client.size = filter->client.avail = 0;
				filter->client.next = filter->client.buffer = NULL;
				/* Return whatever we do have. */
				if (avail != NULL)
					*avail = filter->resizing.avail;
				return (NULL);
			}

			filter->client.next = filter->client.buffer;
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
			if (min > filter->resizing.size) {
				size_t s, t;
				char *p;

				/* Double the buffer; watch for overflow. */
				s = t = filter->resizing.size;
				if (s == 0)
					s = min;
				while (s < min) {
					t *= 2;
					if (t <= s) { /* Integer overflow! */
						transform_set_error(
							filter->base.transform,
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
				p = (char *)realloc(filter->resizing.buffer, s);
				if (p == NULL) {
					transform_set_error(
						filter->base.transform,
						ENOMEM,
					    "Unable to allocate copy buffer");
					filter->fatal = 1;
					if (avail != NULL)
						*avail = TRANSFORM_FATAL;
					return (NULL);
				}
				/* we only need to move data if there wasn't a buffer already */
				if (filter->resizing.avail > 0 && !filter->resizing.buffer)
					memmove(p, filter->resizing.next, filter->resizing.avail);
				filter->resizing.next = filter->resizing.buffer = p;
				filter->resizing.size = s;
			}

			/* We can add client data to copy buffer. */
			/* First estimate: copy to fill rest of buffer. */
			tocopy = (filter->resizing.buffer + filter->resizing.size)
			    - (filter->resizing.next + filter->resizing.avail);
			/* Don't waste time buffering more than we need to. */
			if (tocopy + filter->resizing.avail > min)
				tocopy = min - filter->resizing.avail;
			/* Don't copy more than is available. */
			if (tocopy > filter->client.avail)
				tocopy = filter->client.avail;

			/* really feels like we could struct it to avoid this, returning
			 * the buffer directly (or reading into the appropriately sized buffer
			 */
			memcpy(filter->resizing.next + filter->resizing.avail, filter->client.next,
			    tocopy);
			/* Remove this data from client buffer. */
			filter->client.next += tocopy;
			filter->client.avail -= tocopy;
			/* add it to copy buffer. */
			filter->resizing.avail += tocopy;
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

	skipped = transform_read_filter_skip(filter, request);

	if (skipped == request)
		return (skipped);
	/* We hit EOF before we satisfied the skip request. */
	if (skipped < 0)  // Map error code to 0 for error message below.
		skipped = 0;
	transform_set_error(filter->base.transform,
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
int64_t
transform_read_filter_skip(struct transform_read_filter *filter, int64_t request)
{
	int64_t bytes_skipped, total_bytes_skipped = 0;
	size_t min;
	int ret;

	if (TRANSFORM_FATAL == __transform_filter_check_magic(filter,
		TRANSFORM_READ_FILTER_MAGIC, TRANSFORM_STATE_DATA,
		"transform_read_filter_skip")) {
		return (TRANSFORM_FATAL);
	}

	if (filter->fatal)
		return (-1);

	/* Use up the copy buffer first. */
	if (filter->resizing.avail > 0) {
		min = minimum(request, (int64_t)filter->resizing.avail);
		filter->resizing.next += min;
		filter->resizing.avail -= min;
		request -= min;
		filter->base.bytes_consumed += min;
		total_bytes_skipped += min;
	}

	/* Then use up the client buffer. */
	if (filter->client.avail > 0) {
		min = minimum(request, (int64_t)filter->client.avail);
		filter->client.next += min;
		filter->client.avail -= min;
		request -= min;
		filter->base.bytes_consumed += min;
		total_bytes_skipped += min;
	}

	if (filter->base.flags & TRANSFORM_FILTER_IS_PASSTHRU) {
		if (total_bytes_skipped && filter->skip) {
			(filter->skip)((struct transform *)filter->base.transform,
				filter->base.data, filter->base.upstream.read, total_bytes_skipped);
		}
	}

	if (request == 0 || filter->end_of_file) {
		return (total_bytes_skipped);
	}

	/* If there's an optimized skip function, use it. */
	if (filter->skip != NULL) {
		int64_t aligned_request = (request / filter->alignment) * filter->alignment;
		if (aligned_request) {
			bytes_skipped = (filter->skip)((struct transform *)filter->base.transform, 
				filter->base.data, filter->base.upstream.read, aligned_request);
			if (bytes_skipped < 0) {	/* error */
				filter->fatal = 1;
				return (bytes_skipped);
			}
			filter->base.bytes_consumed += bytes_skipped;
			total_bytes_skipped += bytes_skipped;
			request -= bytes_skipped;
			if (request == 0)
				return (total_bytes_skipped);
		}
	}

	if (filter->end_of_file || filter->fatal) {
		return (total_bytes_skipped);
	}

	/* Use ordinary reads as necessary to complete the request. */
	for (;;) {

		filter->client.buffer = filter->managed_buffer;
		filter->client.size = filter->client.avail = filter->managed_size;

		ret = _filter_read(filter, (void **)&filter->client.buffer,
			&filter->client.avail);

		if (TRANSFORM_FATAL == ret) {
			filter->client.buffer = NULL;
			filter->client.avail = filter->client.size = 0;
			return (ret);
		}
		if (!filter->client.avail) {
			filter->client.buffer = NULL;
			return (total_bytes_skipped);
		}

		assert(filter->client.avail > 0);

		if (filter->client.avail >= request) {
			filter->client.next =
			    ((const char *)filter->client.buffer) + request;
			filter->client.avail -= request;
			if (filter->client.buffer != filter->managed_buffer)
				filter->client.size = filter->client.avail;
			total_bytes_skipped += request;
			return (total_bytes_skipped);
		}

		total_bytes_skipped += filter->client.avail;
		request -= filter->client.avail;
	}
}

ssize_t
transform_read_consume_block(struct transform *_t, void *buff, size_t request)
{
	struct transform_read_filter *filter;
	const void *data, *end = buff + request;
	ssize_t avail;

	transform_check_magic(_t, TRANSFORM_READ_MAGIC, TRANSFORM_STATE_DATA,
		"transform_read_consume_block");

	filter = ((struct transform_read *)_t)->filter;

	while (end != buff) {
		data = transform_read_filter_ahead(filter, 1, &avail);
		if (!data) {
			if (0 != avail) {
				/* error of some sort- pass the code down */
				return avail;
			}
			/* else eof... return what we've got. */
			break;
		}
		avail = minimum(avail, (end - buff));
		memcpy(buff, data, avail);
		buff += avail;
	}
	return (request - (end - buff));
}

int64_t
transform_read_bytes_consumed(struct transform *_a)
{
	struct transform_read *a = (struct transform_read *)_a;
	transform_check_magic(_a, TRANSFORM_READ_MAGIC,
		TRANSFORM_STATE_DATA, "transform_read_bytes_consumed");

	return a->filter->base.bytes_consumed;
}

int
transform_is_read(struct transform *t)
{
	return (t->marker.magic == TRANSFORM_READ_MAGIC);
}
