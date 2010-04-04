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
static int	_transform_write_close(struct transform *);
static int	_transform_write_free(struct transform *);
static ssize_t	_transform_write_data(struct transform *, const void *, size_t);
void __transform_write_filters_free(struct transform_write *);

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
		av.transform_free = _transform_write_free;
		av.transform_write_data = _transform_write_data;
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

	a = (struct transform_write *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->transform.magic = TRANSFORM_WRITE_MAGIC;
	a->transform.state = TRANSFORM_STATE_NEW;
	a->transform.vtable = transform_write_vtable();
	/*
	 * The value 10240 here matches the traditional tar default,
	 * but is otherwise arbitrary.
	 * TODO: Set the default block size from the format selected.
	 */
	a->bytes_per_block = 10240;
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


/*
 * Allocate and return the next filter structure.
 */
struct transform_write_filter *
__transform_write_allocate_filter(struct transform *_a)
{
	struct transform_write *a = (struct transform_write *)_a;
	struct transform_write_filter *f;

	f = calloc(1, sizeof(*f));
	f->transform = _a;
	if (a->filter_first == NULL)
		a->filter_first = f;
	else
		a->filter_last->next_filter = f;
	a->filter_last = f;
	return f;
}

/*
 * Write data to a particular filter.
 */
int
__transform_write_filter(struct transform_write_filter *f,
    const void *buff, size_t length)
{
	int r;
	if (length == 0)
		return(TRANSFORM_OK);
	r = (f->write)(f, buff, length);
	f->bytes_written += length;
	return (r);
}

/*
 * Open a filter.
 */
int
__transform_write_open_filter(struct transform_write_filter *f)
{
	if (f->open == NULL)
		return (TRANSFORM_OK);
	return (f->open)(f);
}

/*
 * Close a filter.
 */
int
__transform_write_close_filter(struct transform_write_filter *f)
{
	if (f->close == NULL)
		return (TRANSFORM_OK);
	return (f->close)(f);
}

int
transform_write_output(struct transform *_a, const void *buff, size_t length)
{
	struct transform_write *a = (struct transform_write *)_a;
	transform_check_magic(_a, TRANSFORM_WRITE_MAGIC,
		TRANSFORM_STATE_DATA, "transform_write_output");	
	return (__transform_write_filter(a->filter_first, buff, length));
}

static int
transform_write_client_open(struct transform_write_filter *f)
{
	struct transform_write *a = (struct transform_write *)f->transform;
	struct transform_none *state;
	void *buffer;
	size_t buffer_size;

	f->bytes_per_block = transform_write_get_bytes_per_block(f->transform);
	f->bytes_in_last_block =
	    transform_write_get_bytes_in_last_block(f->transform);
	buffer_size = f->bytes_per_block;

	state = (struct transform_none *)calloc(1, sizeof(*state));
	buffer = (char *)malloc(buffer_size);
	if (state == NULL || buffer == NULL) {
		free(state);
		free(buffer);
		transform_set_error(f->transform, ENOMEM,
		    "Can't allocate data for output buffering");
		return (TRANSFORM_FATAL);
	}

	state->buffer_size = buffer_size;
	state->buffer = buffer;
	state->next = state->buffer;
	state->avail = state->buffer_size;
	f->data = state;

	if (a->client_opener == NULL)
		return (TRANSFORM_OK);
	return (a->client_opener(f->transform, a->client_data));
}

static int
transform_write_client_write(struct transform_write_filter *f,
    const void *_buff, size_t length)
{
	struct transform_write *a = (struct transform_write *)f->transform;
        struct transform_none *state = (struct transform_none *)f->data;
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
                            a->client_data, buff, remaining);
                        if (bytes_written <= 0)
                                return (TRANSFORM_FATAL);
                        remaining -= bytes_written;
                        buff += bytes_written;
                }
                return (TRANSFORM_OK);
        }

        /* If the copy buffer isn't empty, try to fill it. */
        if (state->avail < state->buffer_size) {
                /* If buffer is not empty... */
                /* ... copy data into buffer ... */
                to_copy = ((size_t)remaining > state->avail) ?
                    state->avail : remaining;
                memcpy(state->next, buff, to_copy);
                state->next += to_copy;
                state->avail -= to_copy;
                buff += to_copy;
                remaining -= to_copy;
                /* ... if it's full, write it out. */
                if (state->avail == 0) {
                        bytes_written = (a->client_writer)(&a->transform,
                            a->client_data, state->buffer, state->buffer_size);
                        if (bytes_written <= 0)
                                return (TRANSFORM_FATAL);
                        /* XXX TODO: if bytes_written < state->buffer_size */
                        state->next = state->buffer;
                        state->avail = state->buffer_size;
                }
        }

        while ((size_t)remaining > state->buffer_size) {
                /* Write out full blocks directly to client. */
                bytes_written = (a->client_writer)(&a->transform,
                    a->client_data, buff, state->buffer_size);
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
        return (TRANSFORM_OK);
}

static int
transform_write_client_close(struct transform_write_filter *f)
{
	struct transform_write *a = (struct transform_write *)f->transform;
        struct transform_none *state = (struct transform_none *)f->data;
        ssize_t block_length;
        ssize_t target_block_length;
        ssize_t bytes_written;
        int ret = TRANSFORM_OK;

        /* If there's pending data, pad and write the last block */
        if (state->next != state->buffer) {
                block_length = state->buffer_size - state->avail;

                /* Tricky calculation to determine size of last block */
                if (a->bytes_in_last_block <= 0)
                        /* Default or Zero: pad to full block */
                        target_block_length = a->bytes_per_block;
                else
                        /* Round to next multiple of bytes_in_last_block. */
                        target_block_length = a->bytes_in_last_block *
                            ( (block_length + a->bytes_in_last_block - 1) /
                                a->bytes_in_last_block);
                if (target_block_length > a->bytes_per_block)
                        target_block_length = a->bytes_per_block;
                if (block_length < target_block_length) {
                        memset(state->next, 0,
                            target_block_length - block_length);
                        block_length = target_block_length;
                }
                bytes_written = (a->client_writer)(&a->transform,
                    a->client_data, state->buffer, block_length);
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
	struct transform_write *a = (struct transform_write *)_a;
	struct transform_write_filter *client_filter;
	int ret;

	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_NEW, "transform_write_open");
	transform_clear_error(&a->transform);

	a->client_writer = writer;
	a->client_opener = opener;
	a->client_closer = closer;
	a->client_data = client_data;

	client_filter = __transform_write_allocate_filter(_a);
	client_filter->open = transform_write_client_open;
	client_filter->write = transform_write_client_write;
	client_filter->close = transform_write_client_close;

	ret = __transform_write_open_filter(a->filter_first);

	a->transform.state = TRANSFORM_STATE_DATA;

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
	if (a->transform.state == TRANSFORM_STATE_NEW
	    || a->transform.state == TRANSFORM_STATE_CLOSED)
		return (TRANSFORM_OK); // Okay to close() when not open.

	transform_clear_error(&a->transform);

	/* Finish the compression and close the stream. */
	r1 = __transform_write_close_filter(a->filter_first);
	if (r1 < r)
		r = r1;

	if (a->transform.state != TRANSFORM_STATE_FATAL)
		a->transform.state = TRANSFORM_STATE_CLOSED;
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

void
__transform_write_filters_free(struct transform_write *a)
{
	int r = TRANSFORM_OK, r1;

	while (a->filter_first != NULL) {
		struct transform_write_filter *next
		    = a->filter_first->next_filter;
		if (a->filter_first->free != NULL) {
			r1 = (*a->filter_first->free)(a->filter_first);
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
	int r = TRANSFORM_OK, r1;

	if (_a == NULL)
		return (TRANSFORM_OK);
	/* It is okay to call free() in state FATAL. */
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_ANY | TRANSFORM_STATE_FATAL, "transform_write_free");
	if (a->transform.state != TRANSFORM_STATE_FATAL)
		r = transform_write_close(&a->transform);

	__transform_write_filters_free(a);

	transform_string_free(&a->transform.error_string);
	a->transform.magic = 0;
	free(a);
	return (r);
}

/*
 * Note that the compressor is responsible for blocking.
 */
static ssize_t
_transform_write_data(struct transform *_a, const void *buff, size_t s)
{
	struct transform_write *a = (struct transform_write *)_a;
	transform_check_magic(&a->transform, TRANSFORM_WRITE_MAGIC,
	    TRANSFORM_STATE_DATA, "transform_write_data");
	transform_clear_error(&a->transform);
	return ((a->format_write_data)(a, buff, s));
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
		f = f->next_filter;
		--n;
	}
	return f;
}

static int
_transform_filter_code(struct transform *_a, int n)
{
	struct transform_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? -1 : f->code;
}

static const char *
_transform_filter_name(struct transform *_a, int n)
{
	struct transform_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? NULL : f->name;
}

static int64_t
_transform_filter_bytes(struct transform *_a, int n)
{
	struct transform_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? -1 : f->bytes_written;
}
