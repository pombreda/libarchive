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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write.c 201099 2009-12-28 03:03:00Z kientzle $");

/*
 * This file contains the "essential" portions of the write API, that
 * is, stuff that will essentially always be used by any client that
 * actually needs to write a archive.  Optional pieces have been, as
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

static struct archive_vtable *archive_write_vtable(void);

static int	_archive_filter_code(struct archive *, int);
static const char *_archive_filter_name(struct archive *, int);
static int64_t	_archive_filter_bytes(struct archive *, int);
static int	_archive_write_close(struct archive *);
static int	_archive_write_free(struct archive *);
static int	_archive_write_finish_entry(struct archive *);
static ssize_t	_archive_write_data(struct archive *, const void *, size_t);

struct archive_none {
	size_t buffer_size;
	size_t avail;
	char *buffer;
	char *next;
};

static struct archive_vtable *
archive_write_vtable(void)
{
	static struct archive_vtable av;
	static int inited = 0;

	if (!inited) {
		av.archive_close = _archive_write_close;
		av.archive_filter_bytes = _archive_filter_bytes;
		av.archive_filter_code = _archive_filter_code;
		av.archive_filter_name = _archive_filter_name;
		av.archive_free = _archive_write_free;
		av.archive_write_finish_entry = _archive_write_finish_entry;
		av.archive_write_data = _archive_write_data;
	}
	return (&av);
}

/*
 * Allocate, initialize and return an archive object.
 */
struct archive *
archive_write_new(void)
{
	struct archive_write *a;
	unsigned char *nulls;

	a = (struct archive_write *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->archive.magic = ARCHIVE_WRITE_MAGIC;
	a->archive.state = ARCHIVE_STATE_NEW;
	a->archive.vtable = archive_write_vtable();
	/*
	 * The value 10240 here matches the traditional tar default,
	 * but is otherwise arbitrary.
	 * TODO: Set the default block size from the format selected.
	 */
	a->bytes_per_block = 10240;
	a->bytes_in_last_block = -1;	/* Default */

	/* Initialize a block of nulls for padding purposes. */
	a->null_length = 1024;
	nulls = (unsigned char *)malloc(a->null_length);
	if (nulls == NULL) {
		free(a);
		return (NULL);
	}
	memset(nulls, 0, a->null_length);
	a->nulls = nulls;
	return (&a->archive);
}

/*
 * Set the block size.  Returns 0 if successful.
 */
int
archive_write_set_bytes_per_block(struct archive *_a, int bytes_per_block)
{
	struct archive_write *a = (struct archive_write *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_bytes_per_block");
	a->bytes_per_block = bytes_per_block;
	return (ARCHIVE_OK);
}

/*
 * Get the current block size.  -1 if it has never been set.
 */
int
archive_write_get_bytes_per_block(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_get_bytes_per_block");
	return (a->bytes_per_block);
}

/*
 * Set the size for the last block.
 * Returns 0 if successful.
 */
int
archive_write_set_bytes_in_last_block(struct archive *_a, int bytes)
{
	struct archive_write *a = (struct archive_write *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_set_bytes_in_last_block");
	a->bytes_in_last_block = bytes;
	return (ARCHIVE_OK);
}

/*
 * Return the value set above.  -1 indicates it has not been set.
 */
int
archive_write_get_bytes_in_last_block(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_get_bytes_in_last_block");
	return (a->bytes_in_last_block);
}


/*
 * dev/ino of a file to be rejected.  Used to prevent adding
 * an archive to itself recursively.
 */
#if ARCHIVE_VERSION_NUMBER < 3000000
int
archive_write_set_skip_file(struct archive *_a, dev_t d, ino_t i)
#else
int
archive_write_set_skip_file(struct archive *_a, int64_t d, int64_t i)
#endif
{
	struct archive_write *a = (struct archive_write *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_set_skip_file");
	a->skip_file_dev = d;
	a->skip_file_ino = i;
	return (ARCHIVE_OK);
}

/*
 * Allocate and return the next filter structure.
 */
struct archive_write_filter *
__archive_write_allocate_filter(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct archive_write_filter *f;

	f = calloc(1, sizeof(*f));
	f->archive = _a;
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
__archive_write_filter(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	int r;
	if (length == 0)
		return(ARCHIVE_OK);
	r = (f->write)(f, buff, length);
	f->bytes_written += length;
	return (r);
}

/*
 * Open a filter.
 */
int
__archive_write_open_filter(struct archive_write_filter *f)
{
	if (f->open == NULL)
		return (ARCHIVE_OK);
	return (f->open)(f);
}

/*
 * Close a filter.
 */
int
__archive_write_close_filter(struct archive_write_filter *f)
{
	if (f->close == NULL)
		return (ARCHIVE_OK);
	return (f->close)(f);
}

int
__archive_write_output(struct archive_write *a, const void *buff, size_t length)
{
	return (__archive_write_filter(a->filter_first, buff, length));
}

static int
archive_write_client_open(struct archive_write_filter *f)
{
	struct archive_write *a = (struct archive_write *)f->archive;
	struct archive_none *state;
	void *buffer;
	size_t buffer_size;

	f->bytes_per_block = archive_write_get_bytes_per_block(f->archive);
	f->bytes_in_last_block =
	    archive_write_get_bytes_in_last_block(f->archive);
	buffer_size = f->bytes_per_block;

	state = (struct archive_none *)calloc(1, sizeof(*state));
	buffer = (char *)malloc(buffer_size);
	if (state == NULL || buffer == NULL) {
		free(state);
		free(buffer);
		archive_set_error(f->archive, ENOMEM,
		    "Can't allocate data for output buffering");
		return (ARCHIVE_FATAL);
	}

	state->buffer_size = buffer_size;
	state->buffer = buffer;
	state->next = state->buffer;
	state->avail = state->buffer_size;
	f->data = state;

	if (a->client_opener == NULL)
		return (ARCHIVE_OK);
	return (a->client_opener(f->archive, a->client_data));
}

static int
archive_write_client_write(struct archive_write_filter *f,
    const void *_buff, size_t length)
{
	struct archive_write *a = (struct archive_write *)f->archive;
        struct archive_none *state = (struct archive_none *)f->data;
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
                        bytes_written = (a->client_writer)(&a->archive,
                            a->client_data, buff, remaining);
                        if (bytes_written <= 0)
                                return (ARCHIVE_FATAL);
                        remaining -= bytes_written;
                        buff += bytes_written;
                }
                return (ARCHIVE_OK);
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
                        bytes_written = (a->client_writer)(&a->archive,
                            a->client_data, state->buffer, state->buffer_size);
                        if (bytes_written <= 0)
                                return (ARCHIVE_FATAL);
                        /* XXX TODO: if bytes_written < state->buffer_size */
                        state->next = state->buffer;
                        state->avail = state->buffer_size;
                }
        }

        while ((size_t)remaining > state->buffer_size) {
                /* Write out full blocks directly to client. */
                bytes_written = (a->client_writer)(&a->archive,
                    a->client_data, buff, state->buffer_size);
                if (bytes_written <= 0)
                        return (ARCHIVE_FATAL);
                buff += bytes_written;
                remaining -= bytes_written;
        }

        if (remaining > 0) {
                /* Copy last bit into copy buffer. */
                memcpy(state->next, buff, remaining);
                state->next += remaining;
                state->avail -= remaining;
        }
        return (ARCHIVE_OK);
}

static int
archive_write_client_close(struct archive_write_filter *f)
{
	struct archive_write *a = (struct archive_write *)f->archive;
        struct archive_none *state = (struct archive_none *)f->data;
        ssize_t block_length;
        ssize_t target_block_length;
        ssize_t bytes_written;
        int ret = ARCHIVE_OK;

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
                bytes_written = (a->client_writer)(&a->archive,
                    a->client_data, state->buffer, block_length);
                ret = bytes_written <= 0 ? ARCHIVE_FATAL : ARCHIVE_OK;
        }
	if (a->client_closer)
		(*a->client_closer)(&a->archive, a->client_data);
	free(state->buffer);
        free(state);
        a->client_data = NULL;
        return (ret);
}

/*
 * Open the archive using the current settings.
 */
int
archive_write_open(struct archive *_a, void *client_data,
    archive_open_callback *opener, archive_write_callback *writer,
    archive_close_callback *closer)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct archive_write_filter *client_filter;
	int ret;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_open");
	archive_clear_error(&a->archive);

	a->client_writer = writer;
	a->client_opener = opener;
	a->client_closer = closer;
	a->client_data = client_data;

	client_filter = __archive_write_allocate_filter(_a);
	client_filter->open = archive_write_client_open;
	client_filter->write = archive_write_client_write;
	client_filter->close = archive_write_client_close;

	ret = __archive_write_open_filter(a->filter_first);

	a->archive.state = ARCHIVE_STATE_HEADER;

	if (a->format_init && ret == ARCHIVE_OK)
		ret = (a->format_init)(a);
	return (ret);
}

/*
 * Close out the archive.
 */
static int
_archive_write_close(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int r = ARCHIVE_OK, r1 = ARCHIVE_OK;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL,
	    "archive_write_close");
	if (a->archive.state == ARCHIVE_STATE_NEW
	    || a->archive.state == ARCHIVE_STATE_CLOSED)
		return (ARCHIVE_OK); // Okay to close() when not open.

	archive_clear_error(&a->archive);

	/* Finish the last entry. */
	if (a->archive.state == ARCHIVE_STATE_DATA)
		r = ((a->format_finish_entry)(a));

	/* Finish off the archive. */
	/* TODO: have format closers invoke compression close. */
	if (a->format_close != NULL) {
		r1 = (a->format_close)(a);
		if (r1 < r)
			r = r1;
	}

	/* Finish the compression and close the stream. */
	r1 = __archive_write_close_filter(a->filter_first);
	if (r1 < r)
		r = r1;

	if (a->archive.state != ARCHIVE_STATE_FATAL)
		a->archive.state = ARCHIVE_STATE_CLOSED;
	return (r);
}

void
__archive_write_filters_free(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int r = ARCHIVE_OK, r1;

	while (a->filter_first != NULL) {
		struct archive_write_filter *next
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
 * Destroy the archive structure.
 *
 * Be careful: user might just call write_new and then write_free.
 * Don't assume we actually wrote anything or performed any non-trivial
 * initialization.
 */
static int
_archive_write_free(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int r = ARCHIVE_OK, r1;

	if (_a == NULL)
		return (ARCHIVE_OK);
	/* It is okay to call free() in state FATAL. */
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_write_free");
	if (a->archive.state != ARCHIVE_STATE_FATAL)
		r = archive_write_close(&a->archive);

	/* Release format resources. */
	if (a->format_free != NULL) {
		r1 = (a->format_free)(a);
		if (r1 < r)
			r = r1;
	}

	__archive_write_filters_free(_a);

	/* Release various dynamic buffers. */
	free((void *)(uintptr_t)(const void *)a->nulls);
	archive_string_free(&a->archive.error_string);
	a->archive.magic = 0;
	free(a);
	return (r);
}

static int
_archive_write_finish_entry(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret = ARCHIVE_OK;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_write_finish_entry");
	if (a->archive.state & ARCHIVE_STATE_DATA)
		ret = (a->format_finish_entry)(a);
	a->archive.state = ARCHIVE_STATE_HEADER;
	return (ret);
}

/*
 * Note that the compressor is responsible for blocking.
 */
static ssize_t
_archive_write_data(struct archive *_a, const void *buff, size_t s)
{
	struct archive_write *a = (struct archive_write *)_a;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_DATA, "archive_write_data");
	archive_clear_error(&a->archive);
	return ((a->format_write_data)(a, buff, s));
}

static struct archive_write_filter *
filter_lookup(struct archive *_a, int n)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct archive_write_filter *f = a->filter_first;
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
_archive_filter_code(struct archive *_a, int n)
{
	struct archive_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? -1 : f->code;
}

static const char *
_archive_filter_name(struct archive *_a, int n)
{
	struct archive_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? NULL : f->name;
}

static int64_t
_archive_filter_bytes(struct archive *_a, int n)
{
	struct archive_write_filter *f = filter_lookup(_a, n);
	return f == NULL ? -1 : f->bytes_written;
}