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
 * actually needs to read an archive.  Optional pieces have been, as
 * far as possible, separated out into separate files to avoid
 * needlessly bloating statically-linked clients.
 */

#include "transform_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read.c 201157 2009-12-29 05:30:23Z kientzle $");

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

static int	build_stream(struct archive_read *);
static void	free_filters(struct archive_read *);
static int	close_filters(struct archive_read *);
static struct archive_vtable *archive_read_vtable(void);
static int64_t	_archive_filter_bytes(struct archive *, int);
static int	_archive_filter_code(struct archive *, int);
static const char *_archive_filter_name(struct archive *, int);
static int	_archive_read_close(struct archive *);
static int	_archive_read_free(struct archive *);

static struct archive_vtable *
archive_read_vtable(void)
{
	static struct archive_vtable av;
	static int inited = 0;

	if (!inited) {
		av.archive_filter_bytes = _archive_filter_bytes;
		av.archive_filter_code = _archive_filter_code;
		av.archive_filter_name = _archive_filter_name;
		av.archive_free = _archive_read_free;
		av.archive_close = _archive_read_close;
	}
	return (&av);
}

/*
 * Allocate, initialize and return a struct archive object.
 */
struct archive *
archive_read_new(void)
{
	struct archive_read *a;

	a = (struct archive_read *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->archive.magic = ARCHIVE_READ_MAGIC;

	a->archive.state = ARCHIVE_STATE_NEW;
	a->archive.vtable = archive_read_vtable();

	return (&a->archive);
}

/*
 * Set read options for the filter.
 */
int
archive_read_set_filter_options(struct archive *_a, const char *s)
{
	struct archive_read *a;
	struct archive_read_filter *filter;
	struct archive_read_filter_bidder *bidder;
	char key[64], val[64];
	int len, r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_set_filter_options");

	if (s == NULL || *s == '\0')
		return (ARCHIVE_OK);
	a = (struct archive_read *)_a;
	len = 0;
	for (filter = a->filter; filter != NULL; filter = filter->upstream) {
		bidder = filter->bidder;
		if (bidder == NULL)
			continue;
		if (bidder->options == NULL)
			/* This bidder does not support option */
			continue;
		while ((len = __archive_parse_options(s, filter->name,
		    sizeof(key), key, sizeof(val), val)) > 0) {
			if (val[0] == '\0')
				r = bidder->options(bidder, key, NULL);
			else
				r = bidder->options(bidder, key, val);
			if (r == ARCHIVE_FATAL)
				return (r);
			s += len;
		}
	}
	if (len < 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Illegal format options.");
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

/*
 * Set read options for the format and the filter.
 */
int
archive_read_set_options(struct archive *_a, const char *s)
{
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_set_options");
	archive_clear_error(_a);

	r = archive_read_set_filter_options(_a, s);
	if (r != ARCHIVE_OK)
		return (r);
	return (ARCHIVE_OK);
}

/*
 * Open the archive
 */
int
archive_read_open(struct archive *a, void *client_data,
    archive_open_callback *client_opener, archive_read_callback *client_reader,
    archive_close_callback *client_closer)
{
	/* Old archive_read_open() is just a thin shell around
	 * archive_read_open2. */
	return archive_read_open2(a, client_data, client_opener,
	    client_reader, NULL, client_closer);
}

static ssize_t
client_read_proxy(struct archive_read_filter *self, const void **buff)
{
	ssize_t r;
	r = (self->archive->client.reader)(&self->archive->archive,
	    self->data, buff);
	return (r);
}

static int64_t
client_skip_proxy(struct archive_read_filter *self, int64_t request)
{
	int64_t ask, get, total;
	/* Limit our maximum seek request to 1GB on platforms
	* with 32-bit off_t (such as Windows). */
	int64_t skip_limit = ((int64_t)1) << (sizeof(off_t) * 8 - 2);

	if (self->archive->client.skipper == NULL)
		return (0);
	total = 0;
	for (;;) {
		ask = request;
		if (ask > skip_limit)
			ask = skip_limit;
		get = (self->archive->client.skipper)(&self->archive->archive,
			self->data, ask);
		if (get == 0)
			return (total);
		request -= get;
		total += get;
	}
}

static int
client_close_proxy(struct archive_read_filter *self)
{
	int r = ARCHIVE_OK;

	if (self->archive->client.closer != NULL)
		r = (self->archive->client.closer)((struct archive *)self->archive,
		    self->data);
	self->data = NULL;
	return (r);
}


int
archive_read_open2(struct archive *_a, void *client_data,
    archive_open_callback *client_opener,
    archive_read_callback *client_reader,
    archive_skip_callback *client_skipper,
    archive_close_callback *client_closer)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_read_filter *filter;
	int e;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_open");
	archive_clear_error(&a->archive);

	if (client_reader == NULL)
		__archive_errx(1,
		    "No reader function provided to archive_read_open");

	/* Open data source. */
	if (client_opener != NULL) {
		e =(client_opener)(&a->archive, client_data);
		if (e != 0) {
			/* If the open failed, call the closer to clean up. */
			if (client_closer)
				(client_closer)(&a->archive, client_data);
			return (e);
		}
	}

	/* Save the client functions and mock up the initial source. */
	a->client.reader = client_reader;
	a->client.skipper = client_skipper;
	a->client.closer = client_closer;

	filter = calloc(1, sizeof(*filter));
	if (filter == NULL)
		return (ARCHIVE_FATAL);
	filter->bidder = NULL;
	filter->upstream = NULL;
	filter->archive = a;
	filter->data = client_data;
	filter->read = client_read_proxy;
	filter->skip = client_skip_proxy;
	filter->close = client_close_proxy;
	filter->name = "none";
	filter->code = ARCHIVE_FILTER_NONE;
	a->filter = filter;

	/* Build out the input pipeline. */
	e = build_stream(a);
	if (e == ARCHIVE_OK)
		a->archive.state = ARCHIVE_STATE_HEADER;

	return (e);
}

/*
 * Allow each registered stream transform to bid on whether
 * it wants to handle this stream.  Repeat until we've finished
 * building the pipeline.
 */
static int
build_stream(struct archive_read *a)
{
	int number_bidders, i, bid, best_bid;
	struct archive_read_filter_bidder *bidder, *best_bidder;
	struct archive_read_filter *filter;
	ssize_t avail;
	int r;

	for (;;) {
		number_bidders = sizeof(a->bidders) / sizeof(a->bidders[0]);

		best_bid = 0;
		best_bidder = NULL;

		bidder = a->bidders;
		for (i = 0; i < number_bidders; i++, bidder++) {
			if (bidder->bid != NULL) {
				bid = (bidder->bid)(bidder, a->filter);
				if (bid > best_bid) {
					best_bid = bid;
					best_bidder = bidder;
				}
			}
		}

		/* If no bidder, we're done. */
		if (best_bidder == NULL) {
			return (ARCHIVE_OK);
		}

		filter
		    = (struct archive_read_filter *)calloc(1, sizeof(*filter));
		if (filter == NULL)
			return (ARCHIVE_FATAL);
		filter->bidder = best_bidder;
		filter->archive = a;
		filter->upstream = a->filter;
		r = (best_bidder->init)(filter);
		if (r != ARCHIVE_OK) {
			free(filter);
			return (r);
		}
		a->filter = filter;
		/* Verify the filter by asking it for some data. */
		__archive_read_filter_ahead(filter, 1, &avail);
		if (avail < 0) {
			close_filters(a);
			free_filters(a);
			return (ARCHIVE_FATAL);
		}
	}
}

/*
 * Read data from an archive entry, using a read(2)-style interface.
 * This is a convenience routine that just calls
 * archive_read_data_block and copies the results into the client
 * buffer, filling any gaps with zero bytes.  Clients using this
 * API can be completely ignorant of sparse-file issues; sparse files
 * will simply be padded with nulls.
 *
 * DO NOT intermingle calls to this function and archive_read_data_block
 * to read a single entry body.
 */
ssize_t
archive_read_data(struct archive *_a, void *buff, size_t s)
{
	struct archive_read *a = (struct archive_read *)_a;
	char	*dest;
	const void *read_buf;
	size_t	 bytes_read;
	size_t	 len;
	int	 r;

	bytes_read = 0;
	dest = (char *)buff;

	while (s > 0) {
		if (a->read_data_remaining == 0) {
			read_buf = a->read_data_block;
			r = archive_read_data_block(&a->archive, &read_buf,
			    &a->read_data_remaining, &a->read_data_offset);
			a->read_data_block = read_buf;
			if (r == ARCHIVE_EOF)
				return (bytes_read);
			/*
			 * Error codes are all negative, so the status
			 * return here cannot be confused with a valid
			 * byte count.  (ARCHIVE_OK is zero.)
			 */
			if (r < ARCHIVE_OK)
				return (r);
		}

		if (a->read_data_offset < a->read_data_output_offset) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Encountered out-of-order sparse blocks");
			return (ARCHIVE_RETRY);
		}

		/* Compute the amount of zero padding needed. */
		if (a->read_data_output_offset + (off_t)s <
		    a->read_data_offset) {
			len = s;
		} else if (a->read_data_output_offset <
		    a->read_data_offset) {
			len = a->read_data_offset -
			    a->read_data_output_offset;
		} else
			len = 0;

		/* Add zeroes. */
		memset(dest, 0, len);
		s -= len;
		a->read_data_output_offset += len;
		dest += len;
		bytes_read += len;

		/* Copy data if there is any space left. */
		if (s > 0) {
			len = a->read_data_remaining;
			if (len > s)
				len = s;
			memcpy(dest, a->read_data_block, len);
			s -= len;
			a->read_data_block += len;
			a->read_data_remaining -= len;
			a->read_data_output_offset += len;
			a->read_data_offset += len;
			dest += len;
			bytes_read += len;
		}
	}
	return (bytes_read);
}

/*
 * Read the next block of entry data from the archive.
 * This is a zero-copy interface; the client receives a pointer,
 * size, and file offset of the next available block of data.
 *
 * Returns ARCHIVE_OK if the operation is successful, ARCHIVE_EOF if
 * the end of entry is encountered.
 */
#if ARCHIVE_VERSION_NUMBER < 3000000
int
archive_read_data_block(struct archive *_a,
    const void **buff, size_t *size, off_t *offset)
#else
int
archive_read_data_block(struct archive *_a,
    const void **buff, size_t *size, int64_t *offset)
#endif
{
	struct archive_read *a = (struct archive_read *)_a;
	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_data_block");

	if (a->format->read_data == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "Internal error: "
		    "No format_read_data_block function registered");
		return (ARCHIVE_FATAL);
	}

	return (a->format->read_data)(a, buff, size, offset);
}

static int
close_filters(struct archive_read *a)
{
	struct archive_read_filter *f = a->filter;
	int r = ARCHIVE_OK;
	/* Close each filter in the pipeline. */
	while (f != NULL) {
		struct archive_read_filter *t = f->upstream;
		if (f->close != NULL) {
			int r1 = (f->close)(f);
			if (r1 < r)
				r = r1;
		}
		free(f->buffer);
		f->buffer = NULL;
		f = t;
	}
	return r;
}

static void
free_filters(struct archive_read *a)
{
	while (a->filter != NULL) {
		struct archive_read_filter *t = a->filter->upstream;
		free(a->filter);
		a->filter = t;
	}
}

/*
 * Close the file and all I/O.
 */
static int
_archive_read_close(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	int r = ARCHIVE_OK, r1 = ARCHIVE_OK;

	archive_check_magic(&a->archive, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_read_close");
	archive_clear_error(&a->archive);
	if (a->archive.state != ARCHIVE_STATE_FATAL)
		a->archive.state = ARCHIVE_STATE_CLOSED;

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
_archive_read_free(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	int i, n;
	int slots;
	int r = ARCHIVE_OK;

	if (_a == NULL)
		return (ARCHIVE_OK);
	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_read_free");
	if (a->archive.state != ARCHIVE_STATE_CLOSED
	    && a->archive.state != ARCHIVE_STATE_FATAL)
		r = archive_read_close(&a->archive);

	/* Call cleanup functions registered by optional components. */
	if (a->cleanup_archive_extract != NULL)
		r = (a->cleanup_archive_extract)(a);

	/* Cleanup format-specific data. */
	slots = sizeof(a->formats) / sizeof(a->formats[0]);
	for (i = 0; i < slots; i++) {
		a->format = &(a->formats[i]);
		if (a->formats[i].cleanup)
			(a->formats[i].cleanup)(a);
	}

	/* Free the filters */
	free_filters(a);

	/* Release the bidder objects. */
	n = sizeof(a->bidders)/sizeof(a->bidders[0]);
	for (i = 0; i < n; i++) {
		if (a->bidders[i].free != NULL) {
			int r1 = (a->bidders[i].free)(&a->bidders[i]);
			if (r1 < r)
				r = r1;
		}
	}

	archive_string_free(&a->archive.error_string);
	a->archive.magic = 0;
	free(a);
	return (r);
}

static struct archive_read_filter *
get_filter(struct archive *_a, int n)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_read_filter *f = a->filter;
	/* XXX handle n == -1 */
	if (n < 0)
		return NULL;
	while (n > 0 && f != NULL) {
		f = f->upstream;
		--n;
	}
	return (f);
}

static int
_archive_filter_code(struct archive *_a, int n)
{
	struct archive_read_filter *f = get_filter(_a, n);
	return f == NULL ? -1 : f->code;
}

static const char *
_archive_filter_name(struct archive *_a, int n)
{
	struct archive_read_filter *f = get_filter(_a, n);
	return f == NULL ? NULL : f->name;
}

static int64_t
_archive_filter_bytes(struct archive *_a, int n)
{
	struct archive_read_filter *f = get_filter(_a, n);
	return f == NULL ? -1 : f->bytes_consumed;
}

/*
 * Used internally by decompression routines to register their bid and
 * initialization functions.
 */
struct archive_read_filter_bidder *
__archive_read_get_bidder(struct archive_read *a)
{
	int i, number_slots;

	number_slots = sizeof(a->bidders) / sizeof(a->bidders[0]);

	for (i = 0; i < number_slots; i++) {
		if (a->bidders[i].bid == NULL) {
			memset(a->bidders + i, 0, sizeof(a->bidders[0]));
			return (a->bidders + i);
		}
	}

	__archive_errx(1, "Not enough slots for compression registration");
	return (NULL); /* Never actually executed. */
}

/*
 * The next three functions comprise the peek/consume internal I/O
 * system used by archive format readers.  This system allows fairly
 * flexible read-ahead and allows the I/O code to operate in a
 * zero-copy manner most of the time.
 *
 * In the ideal case, filters generate blocks of data
 * and __archive_read_ahead() just returns pointers directly into
 * those blocks.  Then __archive_read_consume() just bumps those
 * pointers.  Only if your request would span blocks does the I/O
 * layer use a copy buffer to provide you with a contiguous block of
 * data.  The __archive_read_skip() is an optimization; it scans ahead
 * very quickly (it usually translates into a seek() operation if
 * you're reading uncompressed disk files).
 *
 * A couple of useful idioms:
 *  * "I just want some data."  Ask for 1 byte and pay attention to
 *    the "number of bytes available" from __archive_read_ahead().
 *    You can consume more than you asked for; you just can't consume
 *    more than is available.  If you consume everything that's
 *    immediately available, the next read_ahead() call will pull
 *    the next block.
 *  * "I want to output a large block of data."  As above, ask for 1 byte,
 *    emit all that's available (up to whatever limit you have), then
 *    repeat until you're done.
 *  * "I want to peek ahead by a large amount."  Ask for 4k or so, then
 *    double and repeat until you get an error or have enough.  Note
 *    that the I/O layer will likely end up expanding its copy buffer
 *    to fit your request, so use this technique cautiously.  This
 *    technique is used, for example, by some of the format tasting
 *    code that has uncertain look-ahead needs.
 *
 * TODO: Someday, provide a more generic __archive_read_seek() for
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
 *  * If request can be met, returns pointer to data, returns NULL
 *    if request is not met.
 *
 * Note: If you just want "some data", ask for 1 byte and pay attention
 * to *avail, which will have the actual amount available.  If you
 * know exactly how many bytes you need, just ask for that and treat
 * a NULL return as an error.
 *
 * Important:  This does NOT move the file pointer.  See
 * __archive_read_consume() below.
 */

/*
 * This is tricky.  We need to provide our clients with pointers to
 * contiguous blocks of memory but we want to avoid copying whenever
 * possible.
 *
 * Mostly, this code returns pointers directly into the block of data
 * provided by the client_read routine.  It can do this unless the
 * request would split across blocks.  In that case, we have to copy
 * into an internal buffer to combine reads.
 */
const void *
__archive_read_ahead(struct archive_read *a, size_t min, ssize_t *avail)
{
	return (__archive_read_filter_ahead(a->filter, min, avail));
}

const void *
__archive_read_filter_ahead(struct archive_read_filter *filter,
    size_t min, ssize_t *avail)
{
	ssize_t bytes_read;
	size_t tocopy;

	if (filter->fatal) {
		if (avail)
			*avail = ARCHIVE_FATAL;
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
			bytes_read = (filter->read)(filter,
			    &filter->client_buff);
			if (bytes_read < 0) {		/* Read error. */
				filter->client_total = filter->client_avail = 0;
				filter->client_next = filter->client_buff = NULL;
				filter->fatal = 1;
				if (avail != NULL)
					*avail = ARCHIVE_FATAL;
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
			filter->position += bytes_read;
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
						archive_set_error(
							&filter->archive->archive,
							ENOMEM,
						    "Unable to allocate copy buffer");
						filter->fatal = 1;
						if (avail != NULL)
							*avail = ARCHIVE_FATAL;
						return (NULL);
					}
					s = t;
				}
				/* Now s >= min, so allocate a new buffer. */
				p = (char *)malloc(s);
				if (p == NULL) {
					archive_set_error(
						&filter->archive->archive,
						ENOMEM,
					    "Unable to allocate copy buffer");
					filter->fatal = 1;
					if (avail != NULL)
						*avail = ARCHIVE_FATAL;
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
 * __archive_read_ahead() returns data to you.  Don't try to move
 * ahead by more than the amount of data available according to
 * __archive_read_ahead().
 */
/*
 * Mark the appropriate data as used.  Note that the request here will
 * often be much smaller than the size of the previous read_ahead
 * request.
 */
ssize_t
__archive_read_consume(struct archive_read *a, size_t request)
{
	return (__archive_read_filter_consume(a->filter, request));
}

ssize_t
__archive_read_filter_consume(struct archive_read_filter * filter,
    size_t request)
{
	if (filter->avail > 0) {
		/* Read came from copy buffer. */
		filter->next += request;
		filter->avail -= request;
	} else {
		/* Read came from client buffer. */
		filter->client_next += request;
		filter->client_avail -= request;
	}
	filter->bytes_consumed += request;
	return (request);
}

/*
 * Move the file pointer ahead by an arbitrary amount.  If you're
 * reading uncompressed data from a disk file, this will actually
 * translate into a seek() operation.  Even in cases where seek()
 * isn't feasible, this at least pushes the read-and-discard loop
 * down closer to the data source.
 */
int64_t
__archive_read_skip(struct archive_read *a, int64_t request)
{
	int64_t skipped = __archive_read_skip_lenient(a, request);
	if (skipped == request)
		return (skipped);
	/* We hit EOF before we satisfied the skip request. */
	if (skipped < 0)  // Map error code to 0 for error message below.
		skipped = 0;
	archive_set_error(&a->archive,
	    ARCHIVE_ERRNO_MISC,
	    "Truncated input file (needed %jd bytes, only %jd available)",
	    (intmax_t)request, (intmax_t)skipped);
	return (ARCHIVE_FATAL);
}

int64_t
__archive_read_skip_lenient(struct archive_read *a, int64_t request)
{
	return (__archive_read_filter_skip(a->filter, request));
}

int64_t
__archive_read_filter_skip(struct archive_read_filter *filter, int64_t request)
{
	int64_t bytes_skipped, total_bytes_skipped = 0;
	size_t min;

	if (filter->fatal)
		return (-1);
	/*
	 * If there is data in the buffers already, use that first.
	 */
	if (filter->avail > 0) {
		min = minimum(request, (off_t)filter->avail);
		bytes_skipped = __archive_read_filter_consume(filter, min);
		request -= bytes_skipped;
		total_bytes_skipped += bytes_skipped;
	}
	if (filter->client_avail > 0) {
		min = minimum(request, (int64_t)filter->client_avail);
		bytes_skipped = __archive_read_filter_consume(filter, min);
		request -= bytes_skipped;
		total_bytes_skipped += bytes_skipped;
	}
	if (request == 0)
		return (total_bytes_skipped);

	/*
	 * If a client_skipper was provided, try that first.
	 */
	if (filter->skip != NULL) {
		bytes_skipped = (filter->skip)(filter, request);
		if (bytes_skipped < 0) {	/* error */
			filter->client_total = filter->client_avail = 0;
			filter->client_next = filter->client_buff = NULL;
			filter->fatal = 1;
			return (bytes_skipped);
		}
		filter->bytes_consumed += bytes_skipped;
		total_bytes_skipped += bytes_skipped;
		request -= bytes_skipped;
		filter->client_next = filter->client_buff;
		filter->client_avail = filter->client_total = 0;
	}
	/*
	 * Note that client_skipper will usually not satisfy the
	 * full request (due to low-level blocking concerns),
	 * so even if client_skipper is provided, we may still
	 * have to use ordinary reads to finish out the request.
	 */
	while (request > 0) {
		ssize_t bytes_read;
		(void)__archive_read_filter_ahead(filter, 1, &bytes_read);
		if (bytes_read < 0)
			return (bytes_read);
		if (bytes_read == 0) {
			return (total_bytes_skipped);
		}
		min = (size_t)(minimum(bytes_read, request));
		bytes_read = __archive_read_filter_consume(filter, min);
		total_bytes_skipped += bytes_read;
		request -= bytes_read;
	}
	return (total_bytes_skipped);
}
