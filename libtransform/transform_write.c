/*-
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
__FBSDID("$FreeBSD: head/lib/libtransform/transform_write.c 201099 2009-12-28 03:03:00Z kientzle $");

/*
 * This file contains the "essential" portions of the write API, that
 * is, stuff that will essentially always be used by any client that
 * actually needs to write a transform.  Optional pieces have been, as
 * far as possible, separated out into separate files to reduce
 * needlessly bloating statically-linked clients.
 */

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "transform.h"
#include "transform_private.h"
#include "transform_write_private.h"

static struct transform_vtable *transform_write_vtable(void);

static int	_transform_filter_code(struct transform *, int);
static const char *_transform_filter_name(struct transform *, int);
static int64_t	_transform_filter_bytes(struct transform *, int);
static int  _transform_write_filter_count(struct transform *);
static int  _transform_visit_fds(struct transform *, 
	transform_fd_visitor *, const void *);
static int	_transform_write_close(struct transform *);
static int	_transform_write_free(struct transform *);
static void __transform_write_filters_free(struct transform_write *);
static int transform_write_close_filters(struct transform_write *);

struct transform_none {
	size_t buffer_size;
	size_t avail;
	char *buffer;
	char *next;
};


static struct transform_vtable *
transform_write_vtable(void)
{
	static struct transform_vtable av;
	static int inited = 0;

	if (!inited) {
		av.transform_close = _transform_write_close;
		av.transform_filter_bytes = _transform_filter_bytes;
		av.transform_filter_code = _transform_filter_code;
		av.transform_filter_name = _transform_filter_name;
		av.transform_filter_count = _transform_write_filter_count;
		av.transform_free = _transform_write_free;
		av.transform_visit_fds = _transform_visit_fds;
	}
	return (&av);
}

/*
 * Allocate, initialize and return an transform object.
 */
struct transform *
transform_write_new(void)
{
	struct transform_write *a;

	a = (struct transform_write *)calloc(1, sizeof(struct transform_write));
	if (a == NULL)
		return (NULL);
	a->transform.marker.magic = TRANSFORM_WRITE_MAGIC;
	a->transform.marker.state = TRANSFORM_STATE_NEW;
	a->transform.vtable = transform_write_vtable();
	a->bytes_per_block = DEFAULT_WRITE_BLOCK_SIZE;
	a->bytes_in_last_block = -1;	/* Default */

	return (&a->transform);
}

/*
 * Set the block size.  Returns 0 if successful.
 */
int
transform_write_set_bytes_per_block(struct transform *_a, int bytes_per_block)
{
	struct transform_write *a = (struct transform_write *)_a;
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_NEW, "transform_write_set_bytes_per_block");
	a->bytes_per_block = bytes_per_block;
	return (TRANSFORM_OK);
}

/*
 * Get the current block size.  -1 if it has never been set.
 */
int
transform_write_get_bytes_per_block(struct transform *_a)
{
	struct transform_write *a = (struct transform_write *)_a;
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_ANY, "transform_write_get_bytes_per_block");
	return (a->bytes_per_block);
}

/*
 * Set the size for the last block.
 * Returns 0 if successful.
 */
int
transform_write_set_bytes_in_last_block(struct transform *_a, int bytes)
{
	struct transform_write *a = (struct transform_write *)_a;
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_ANY, "transform_write_set_bytes_in_last_block");
	a->bytes_in_last_block = bytes;
	return (TRANSFORM_OK);
}

/*
 * Return the value set above.  -1 indicates it has not been set.
 */
int
transform_write_get_bytes_in_last_block(struct transform *_a)
{
	struct transform_write *a = (struct transform_write *)_a;
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_ANY, "transform_write_get_bytes_in_last_block");
	return (a->bytes_in_last_block);
}

struct transform_write_filter *
transform_write_filter_new(const void *data, const char *name, int code,
	transform_write_options_callback *options_callback,
	transform_write_open_callback *open_callback,
	transform_write_filter_callback *write_callback,
	transform_write_close_callback *close_callback,
	transform_write_free_callback *free_callback,
	int64_t flags)
{
	struct transform_write_filter *filter;

	if (!write_callback)
		return (NULL);
	if (!name) {
		return (NULL);
	}

	filter = calloc(1, sizeof(struct transform_write_filter));
	if (!filter)
		return (NULL);

	filter->base.marker.magic = TRANSFORM_WRITE_FILTER_MAGIC;
	filter->base.marker.state = TRANSFORM_STATE_NEW;
	filter->base.data = (void *)data;
	filter->base.name = name;
	filter->base.code = code;
	filter->base.flags = flags;
	filter->options = options_callback;
	filter->open = open_callback;
	filter->write = write_callback;
	filter->close = close_callback;
	filter->free = free_callback;
	return (filter);
}

/*
 * create a write_filter, and add it to the transform.
 */
int
transform_write_add_filter(struct transform *_t,
	const void *data, const char *name, int code,
	transform_write_options_callback *options_callback,
	transform_write_open_callback *open_callback,
	transform_write_filter_callback *write_callback,
	transform_write_close_callback *close_callback,
	transform_write_free_callback *free_callback,
	int64_t flags)
{
	struct transform_write_filter *filter;
	struct transform_write *t = (struct transform_write *)_t;
	transform_check_magic(_t, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_NEW, "transform_write_add_filter");

	filter = transform_write_filter_new(data, name, code,
		options_callback, open_callback, write_callback,
		close_callback, free_callback, flags);
	if (!filter) {
		transform_set_error(_t, EINVAL,
			"failed to add a write filter; either memory allocation failed "
			"or the client forgot to supply a write callback");
		return (TRANSFORM_FATAL);
	}

	filter->base.transform = _t;

	if (t->filter_first == NULL)
		t->filter_first = filter;
	else
		t->filter_last->base.upstream.write = filter;
	t->filter_last = filter;

	return (TRANSFORM_OK);
}

/*
 * Write data to a particular filter.
 */
int
transform_write_filter_output(struct transform_write_filter *f,
	const void *buff, size_t length)
{
	ssize_t ret;
	
	if (TRANSFORM_FATAL == __transform_filter_check_magic(f,
		TRANSFORM_WRITE_FILTER_MAGIC, TRANSFORM_STATE_DATA,
		"transform_read_filter_output")) {
		 return (TRANSFORM_FATAL);
	}

	ret = (f->write)(f->base.transform, f->base.data, buff, length,
		f->base.upstream.write);
	if (ret < 0) {
		if (!transform_errno(f->base.transform)) {
			transform_set_error(f->base.transform, TRANSFORM_ERRNO_PROGRAMMER,
				"write_filter %s (code %d) returned non-ok (%d), but set no error",
				f->base.name, f->base.code, (int)ret);
		}
		return (ret);
	}
	f->base.bytes_consumed += ret;
	if (ret != length) {
		if (!transform_errno(f->base.transform)) {
			transform_set_error(f->base.transform, TRANSFORM_ERRNO_PROGRAMMER,
				"write_filter %s (code %d) was requested to write (%d), wrote (%d), but set no error",
				f->base.name, f->base.code, (int)length, (int)ret);
		}
		return (TRANSFORM_FATAL);
	}
	return (TRANSFORM_OK);
}

/*
 * Open a filter.
 */
static int
__transform_write_open_filter(struct transform_write_filter *filter)
{
	int r1 = TRANSFORM_OK, r2 = TRANSFORM_OK;

	if (TRANSFORM_FATAL == __transform_filter_check_magic(filter,
		TRANSFORM_WRITE_FILTER_MAGIC, TRANSFORM_STATE_NEW,
		"transform_read_filter_skip")) {
		 return (TRANSFORM_FATAL);
	}

	if (filter->base.upstream.write) {
		r1 = __transform_write_open_filter(filter->base.upstream.write);
		if (TRANSFORM_FATAL == r1) {
			return (r1);
		}
	}
	if (filter->open) {
		r2 = (filter->open)(filter->base.transform, &(filter->base.data));
	}
	if (TRANSFORM_FATAL != r2) {
		filter->base.marker.state = TRANSFORM_STATE_DATA;
	}
	return (r2 < r1 ? r2 : r1);
}

/*
 * Close a filter.
 */
static int
transform_write_close_filters(struct transform_write *t)
{
	int ret = TRANSFORM_OK, new_ret = TRANSFORM_OK;
	struct transform_write_filter *filter = t->filter_first;

	while (filter) {
		/* check the state- if it never was even opened, closing it is not sane */
		if (filter->base.marker.state == TRANSFORM_STATE_DATA) {
			if (filter->close) {
				new_ret = (filter->close)((struct transform *)t, filter->base.data,
					filter->base.upstream.write);
				ret = new_ret > ret ? ret : new_ret;
			}
			filter->base.marker.state = TRANSFORM_STATE_CLOSED;
		}
		filter = filter->base.upstream.write;
	}
	return (ret);
}

int
transform_write_output(struct transform *_a, const void *buff, size_t length)
{
	struct transform_write *a = (struct transform_write *)_a;
	transform_check_magic(_a, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_DATA, "transform_write_output");
	return (transform_write_filter_output(a->filter_first, buff, length));
}

static int
transform_write_client_open(struct transform *_a, void **_data)
{
	struct transform_write *a = (struct transform_write *)_a;
	struct transform_none *state;
	void *buffer;
	size_t buffer_size;

	buffer_size = transform_write_get_bytes_per_block(_a);

	state = (struct transform_none *)calloc(1, sizeof(*state));
	buffer = (char *)malloc(buffer_size);
	if (state == NULL || buffer == NULL) {
		free(state);
		free(buffer);
		transform_set_error(_a, ENOMEM,
			"Can't allocate data for output buffering");
		return (TRANSFORM_FATAL);
	}

	state->buffer_size = buffer_size;
	state->buffer = buffer;
	state->next = state->buffer;
	state->avail = state->buffer_size;
	*_data = state;

	if (a->client_opener == NULL)
		return (TRANSFORM_OK);
	return (a->client_opener(_a, a->client_data));
}

static ssize_t
transform_write_client_write(struct transform *_t,
	void *_state, const void *_buff, size_t length,
	struct transform_write_filter *upstream)
{
	struct transform_write *a = (struct transform_write *)_t;
	struct transform_none *state = (struct transform_none *)_state;
	const char *buff = (const char *)_buff;
	ssize_t remaining, to_copy;
	ssize_t bytes_written;

	remaining = length;

	/*
	 * If there is no buffer for blocking, just pass the data
	 * straight through to the client write callback.  In
	 * particular, this supports "no write delay" operation for
	 * special applications.  Just set the block size to zero.
	 */
	if (state->buffer_size == 0) {
		while (remaining > 0) {
			bytes_written = (a->client_writer)(&a->transform,
				a->client_data, buff, remaining, NULL);
			if (bytes_written <= 0)
				return (TRANSFORM_FATAL);
			remaining -= bytes_written;
			buff += bytes_written;
		}
		return (length);
	}

	/* If the copy buffer isn't empty, try to fill it. */
	if (state->avail < state->buffer_size) {
		/* If buffer is not empty... */
		/* ... copy data into buffer ... */
		to_copy = ((size_t)remaining > state->avail) ?
			state->avail : (size_t)remaining;
		memcpy(state->next, buff, to_copy);
		state->next += to_copy;
		state->avail -= to_copy;
		buff += to_copy;
		remaining -= to_copy;
		/* ... if it's full, write it out. */
		if (state->avail == 0) {
			char *p = state->buffer;
			size_t to_write = state->buffer_size;
			while (to_write > 0) {
				bytes_written = (a->client_writer)(&a->transform,
					a->client_data, p, to_write, NULL);
				if (bytes_written <= 0)
					return (TRANSFORM_FATAL);
				if ((size_t)bytes_written > to_write) {
					transform_set_error(&(a->transform),
						-1, "write overrun");
					return (TRANSFORM_FATAL);
				}
				p += bytes_written;
				to_write -= bytes_written;
			}
			state->next = state->buffer;
			state->avail = state->buffer_size;
		}
	}

	while ((size_t)remaining > state->buffer_size) {
		/* Write out full blocks directly to client. */
		bytes_written = (a->client_writer)(&a->transform,
			a->client_data, buff, state->buffer_size, NULL);
		if (bytes_written <= 0)
			return (TRANSFORM_FATAL);
		buff += bytes_written;
		remaining -= bytes_written;
	}

	if (remaining > 0) {
		/* Copy last bit into copy buffer. */
		memcpy(state->next, buff, remaining);
		state->next += remaining;
		state->avail -= remaining;
	}
	return (length);
}

static int
transform_write_client_close(struct transform *_t, void *_data,
	struct transform_write_filter *upstream)
{
	struct transform_write *a = (struct transform_write *)_t;
	struct transform_none *state = (struct transform_none *)_data;
	ssize_t bytes_written;
	int ret = TRANSFORM_OK;
	(void)upstream;

	/* If there's pending data, pad and write the last block */
	if (state->next != state->buffer) {
		bytes_written = (a->client_writer)(&a->transform,
			a->client_data, state->buffer, state->next - state->buffer, upstream);
		ret = bytes_written <= 0 ? TRANSFORM_FATAL : TRANSFORM_OK;
	}
	if (a->client_closer)
		(*a->client_closer)(&a->transform, a->client_data);
	free(state->buffer);
	free(state);
	a->client_data = NULL;
	return (ret);
}

/*
 * Open the transform using the current settings.
 */
int
transform_write_open(struct transform *_a, void *client_data,
	transform_open_callback *opener, transform_write_callback *writer,
	transform_close_callback *closer)
{
	return transform_write_open2(_a, client_data, opener, writer,
		closer, NULL);
}

int
transform_write_open2(struct transform *_a, void *client_data,
	transform_open_callback *opener, transform_write_callback *writer,
	transform_close_callback *closer, transform_visit_fds_callback *visit_fds)
{
	struct transform_write *a = (struct transform_write *)_a;
	int ret, r1;

	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_NEW, "transform_write_open");
	transform_clear_error(&a->transform);

	a->client_writer = writer;
	a->client_opener = opener;
	a->client_closer = closer;
	a->client_data = client_data;


	ret = transform_write_add_filter_dynamic_padding(_a);
	if (ret != TRANSFORM_OK) {
		return (TRANSFORM_FATAL);
	}

	if (TRANSFORM_OK != transform_write_add_filter(_a,
		client_data, "write:sink", TRANSFORM_FILTER_NONE,
		NULL,
		transform_write_client_open,
		transform_write_client_write,
		transform_write_client_close,
		NULL,
		TRANSFORM_FILTER_SOURCE)) {
		return (TRANSFORM_FATAL);
	}

	ret = __transform_write_open_filter(a->filter_first);
	if (ret < TRANSFORM_WARN) {
		r1 = transform_write_close_filters(a);
		return (r1 < ret ? r1 : ret);
	}

	a->transform.marker.state = TRANSFORM_STATE_DATA;
	return (ret);
}

/*
 * Close out the transform.
 */
static int
_transform_write_close(struct transform *_a)
{
	struct transform_write *a = (struct transform_write *)_a;
	int r = TRANSFORM_OK, r1 = TRANSFORM_OK;

	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_ANY | TRANSFORM_STATE_FATAL,
		"transform_write_close");
	if (a->transform.marker.state == TRANSFORM_STATE_NEW
		|| a->transform.marker.state == TRANSFORM_STATE_CLOSED)
		return (TRANSFORM_OK); // Okay to close() when not open.

	transform_clear_error(&a->transform);

	/* Finish the compression and close the stream. */
	r1 = transform_write_close_filters(a);
	if (r1 < r)
		r = r1;

	if (a->transform.marker.state != TRANSFORM_STATE_FATAL)
		a->transform.marker.state = TRANSFORM_STATE_CLOSED;
	return (r);
}

int
transform_write_reset_filters(struct transform *_a)
{
	struct transform_write *a = (struct transform_write *)_a;
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_NEW, "transform_write_reset_filters");
	__transform_write_filters_free(a);
	return (TRANSFORM_OK);
}

static int
_transform_write_filter_count(struct transform *_a)
{
	struct transform_write *a = (struct transform_write *)_a;
	struct transform_write_filter *p = a->filter_first;
	int count = 0;
	while(p) {
		count++;
		p = p->base.upstream.write;
	}
	return count;
}

static int
_transform_visit_fds(struct transform *_t,
	transform_fd_visitor *visitor, const void *visitor_data)
{
	struct transform_write *t = (struct transform_write *)_t;
	struct transform_write_filter *p;
	int position, ret = TRANSFORM_OK;

	for(position=0, p=t->filter_first; NULL != p; position++, p = p->base.upstream.write) {
		if (p->visit_fds) {
			if (TRANSFORM_OK != (ret = (p->visit_fds)(_t, p->base.data, visitor,
				visitor_data))) {
				break;
			}
		}
	}
	return (ret);
}

static void
__transform_write_filters_free(struct transform_write *a)
{
	int r = TRANSFORM_OK, r1;

	while (a->filter_first != NULL) {
		struct transform_write_filter *next
			= a->filter_first->base.upstream.write;
		if (a->filter_first->free != NULL) {
			r1 = (*a->filter_first->free)((struct transform *)a, a->filter_first->base.data);
			if (r > r1)
				r = r1;
		}
		free(a->filter_first);
		a->filter_first = next;
	}
	a->filter_last = NULL;
}

/*
 * Destroy the transform structure.
 *
 * Be careful: user might just call write_new and then write_free.
 * Don't assume we actually wrote anything or performed any non-trivial
 * initialization.
 */
static int
_transform_write_free(struct transform *_a)
{
	struct transform_write *a = (struct transform_write *)_a;
	int r = TRANSFORM_OK;

	if (_a == NULL)
		return (TRANSFORM_OK);
	/* It is okay to call free() in state FATAL. */
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_ANY | TRANSFORM_STATE_FATAL, "transform_write_free");
	if (a->transform.marker.state != TRANSFORM_STATE_FATAL)
		r = transform_write_close(&a->transform);

	__transform_write_filters_free(a);

	/* Release various dynamic buffers. */
	transform_string_free(&a->transform.error_string);
	a->transform.marker.magic = 0;
	free(a);
	return (r);
}

static struct transform_write_filter *
filter_lookup(struct transform *_a, int n)
{
	struct transform_write *a = (struct transform_write *)_a;
	struct transform_write_filter *f = a->filter_first;
	if (n == -1)
		return a->filter_last;
	if (n < 0)
		return NULL;
	while (n > 0 && f != NULL) {
		f = f->base.upstream.write;
		--n;
	}
	return f;
}

static int
_transform_filter_code(struct transform *_a, int n)
{
	struct transform_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? -1 : f->base.code;
}

static const char *
_transform_filter_name(struct transform *_a, int n)
{
	struct transform_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? NULL : f->base.name;
}

static int64_t
_transform_filter_bytes(struct transform *_a, int n)
{
	struct transform_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? -1 : f->base.bytes_consumed;
}

int
transform_is_write(struct transform *t)
{
	return (t->marker.magic == TRANSFORM_WRITE_MAGIC);
}
