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

#include "archive_platform.h"
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

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"

#define minimum(a, b) (a < b ? a : b)

static int	choose_format(struct archive_read *);
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
	struct transform *t;

	t = transform_read_new();
	if (NULL == t)
		return (NULL);

	a = (struct archive_read *)malloc(sizeof(*a));
	if (a == NULL) {
		transform_read_free(t);
		return (NULL);
	}
	memset(a, 0, sizeof(*a));

	a->archive.transform = t;
	a->archive.magic = ARCHIVE_READ_MAGIC;

	a->archive.state = ARCHIVE_STATE_NEW;
	a->entry = archive_entry_new();
	a->archive.vtable = archive_read_vtable();

	return (&a->archive);
}

/*
 * Record the do-not-extract-to file. This belongs in archive_read_extract.c.
 */
#if ARCHIVE_VERSION_NUMBER < 3000000
void
archive_read_extract_set_skip_file(struct archive *_a, dev_t d, ino_t i)
#else
void
archive_read_extract_set_skip_file(struct archive *_a, int64_t d, int64_t i)
#endif
{
	struct archive_read *a = (struct archive_read *)_a;

	if (ARCHIVE_OK != __archive_check_magic(_a, ARCHIVE_READ_MAGIC,
		ARCHIVE_STATE_ANY, "archive_read_extract_set_skip_file"))
		return;

	a->skip_file_dev = d;
	a->skip_file_ino = i;
}

/*
 * Set read options for the format.
 */
int
archive_read_set_format_options(struct archive *_a, const char *s)
{
	struct archive_read *a;
	struct archive_format_descriptor *format;
	char key[64], val[64];
	char *valp;
	size_t i;
	int len, r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_set_format_options");

	if (s == NULL || *s == '\0')
		return (ARCHIVE_OK);
	a = (struct archive_read *)_a;
	len = 0;
	for (i = 0; i < sizeof(a->formats)/sizeof(a->formats[0]); i++) {
		format = &a->formats[i];
		if (format == NULL || format->options == NULL ||
		    format->name == NULL)
			/* This format does not support option. */
			continue;

		while ((len = __archive_parse_options(s, format->name,
		    sizeof(key), key, sizeof(val), val)) > 0) {
			valp = val[0] == '\0' ? NULL : val;
			a->format = format;
			r = format->options(a, key, valp);
			a->format = NULL;
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
 * Set read options for the filter.
 */
int
archive_read_set_filter_options(struct archive *a, const char *s)
{
	if ( s == NULL || *s == '\0')
		return (ARCHIVE_OK);

	archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_set_filter_options");

	return __convert_transform_error_to_archive_error(a, a->transform,
		transform_read_set_filter_options(a->transform, s));
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

	r = archive_read_set_format_options(_a, s);
	if (r != ARCHIVE_OK)
		return (r);
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

int
archive_read_open2(struct archive *a, void *_client_data,
    archive_open_callback *client_opener,
    archive_read_callback *client_reader,
    archive_skip_callback *client_skipper,
    archive_close_callback *client_closer)
{
	void *client_data = __archive_shim_new(a, _client_data,
		client_opener, client_closer, NULL, client_reader, client_skipper);

	if (NULL == client_data) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}

	return archive_read_open_transform(a, client_data, __archive_shim_open,
		__archive_shim_read, __archive_shim_skip, __archive_shim_close);
}

int
archive_read_open_transform(struct archive *a, void *client_data,
    transform_open_callback *client_opener,
    transform_read_callback *client_reader,
    transform_skip_callback *client_skipper,
    transform_close_callback *client_closer)
{
	int e;

	archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_open");
	archive_clear_error(a);

	e = transform_read_open2(a->transform,
		client_data,
		client_opener,
		client_reader,
		client_skipper,
		client_closer);
	e = __convert_transform_error_to_archive_error(a, a->transform, e);
	if (ARCHIVE_OK == e)
		a->state = ARCHIVE_STATE_HEADER;

	return (e);
}

/*
 * Read header of next entry.
 */
int
archive_read_next_header2(struct archive *_a, struct archive_entry *entry)
{
	struct archive_read *a = (struct archive_read *)_a;
	int slot, ret;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_read_next_header");

	++_a->file_count;
	archive_entry_clear(entry);
	archive_clear_error(&a->archive);

	/*
	 * If no format has yet been chosen, choose one.
	 */
	if (a->format == NULL) {
		slot = choose_format(a);
		if (slot < 0) {
			a->archive.state = ARCHIVE_STATE_FATAL;
			return (ARCHIVE_FATAL);
		}
		a->format = &(a->formats[slot]);
	}

	/*
	 * If client didn't consume entire data, skip any remainder
	 * (This is especially important for GNU incremental directories.)
	 */
	if (a->archive.state == ARCHIVE_STATE_DATA) {
		ret = archive_read_data_skip(&a->archive);
		if (ret == ARCHIVE_EOF) {
			archive_set_error(&a->archive, EIO, "Premature end-of-file.");
			a->archive.state = ARCHIVE_STATE_FATAL;
			return (ARCHIVE_FATAL);
		}
		if (ret != ARCHIVE_OK)
			return (ret);
	}

	/* Record start-of-header offset in uncompressed stream. */
	a->header_position = transform_read_bytes_consumed(a->archive.transform);

	ret = (a->format->read_header)(a, entry);

	/*
	 * EOF and FATAL are persistent at this layer.  By
	 * modifying the state, we guarantee that future calls to
	 * read a header or read data will fail.
	 */
	switch (ret) {
	case ARCHIVE_EOF:
		a->archive.state = ARCHIVE_STATE_EOF;
		break;
	case ARCHIVE_OK:
		a->archive.state = ARCHIVE_STATE_DATA;
		break;
	case ARCHIVE_WARN:
		a->archive.state = ARCHIVE_STATE_DATA;
		break;
	case ARCHIVE_RETRY:
		break;
	case ARCHIVE_FATAL:
		a->archive.state = ARCHIVE_STATE_FATAL;
		break;
	}

	a->read_data_output_offset = 0;
	a->read_data_remaining = 0;
	return (ret);
}

int
archive_read_next_header(struct archive *_a, struct archive_entry **entryp)
{
	int ret;
	struct archive_read *a = (struct archive_read *)_a;
	*entryp = NULL;
	ret = archive_read_next_header2(_a, a->entry);
	*entryp = a->entry;
	return ret;
}

/*
 * Allow each registered format to bid on whether it wants to handle
 * the next entry.  Return index of winning bidder.
 */
static int
choose_format(struct archive_read *a)
{
	int slots;
	int i;
	int bid, best_bid;
	int best_bid_slot;

	slots = sizeof(a->formats) / sizeof(a->formats[0]);
	best_bid = -1;
	best_bid_slot = -1;

	/* Set up a->format and a->pformat_data for convenience of bidders. */
	a->format = &(a->formats[0]);
	for (i = 0; i < slots; i++, a->format++) {
		if (a->format->bid) {
			bid = (a->format->bid)(a);
			if (bid == ARCHIVE_FATAL)
				return (ARCHIVE_FATAL);
			if ((bid > best_bid) || (best_bid_slot < 0)) {
				best_bid = bid;
				best_bid_slot = i;
			}
		}
	}

	/*
	 * There were no bidders; this is a serious programmer error
	 * and demands a quick and definitive abort.
	 */
	if (best_bid_slot < 0)
		__archive_errx(1, "No formats were registered; you must "
		    "invoke at least one archive_read_support_format_XXX "
		    "function in order to successfully read an archive.");

	/*
	 * There were bidders, but no non-zero bids; this means we
	 * can't support this stream.
	 */
	if (best_bid < 1) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unrecognized archive format");
		return (ARCHIVE_FATAL);
	}

	return (best_bid_slot);
}

/*
 * Return the file offset (within the uncompressed data stream) where
 * the last header started.
 */
int64_t
archive_read_header_position(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_read_header_position");
	return (a->header_position);
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

#if ARCHIVE_API_VERSION < 3
/*
 * Obsolete function provided for compatibility only.  Note that the API
 * of this function doesn't allow the caller to detect if the remaining
 * data from the archive entry is shorter than the buffer provided, or
 * even if an error occurred while reading data.
 */
int
archive_read_data_into_buffer(struct archive *a, void *d, ssize_t len)
{

	archive_read_data(a, d, len);
	return (ARCHIVE_OK);
}
#endif

/*
 * Skip over all remaining data in this entry.
 */
int
archive_read_data_skip(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	int r;
	const void *buff;
	size_t size;
#if ARCHIVE_VERSION_NUMBER < 3000000
	off_t offset;
#else
	int64_t offset;
#endif

	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_data_skip");

	if (a->format->read_data_skip != NULL)
		r = (a->format->read_data_skip)(a);
	else {
		while ((r = archive_read_data_block(&a->archive,
			    &buff, &size, &offset))
		    == ARCHIVE_OK)
			;
	}

	if (r == ARCHIVE_EOF)
		r = ARCHIVE_OK;

	a->archive.state = ARCHIVE_STATE_HEADER;
	return (r);
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

/*
 * Close the file and all I/O.
 */
static int
_archive_read_close(struct archive *a)
{
	int r = ARCHIVE_OK, r1 = ARCHIVE_OK;

	archive_check_magic(a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_read_close");
	archive_clear_error(a);
	if (a->state != ARCHIVE_STATE_FATAL)
		a->state = ARCHIVE_STATE_CLOSED;

	/* TODO: Clean up the formatters. */

	r1 = transform_read_close(a->transform);
	r1 = __convert_transform_error_to_archive_error(a, a->transform, r1);
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
	int i;
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

	transform_read_free(a->archive.transform);

	archive_string_free(&a->archive.error_string);
	if (a->entry)
		archive_entry_free(a->entry);
	a->archive.magic = 0;
	free(a);
	return (r);
}

static int
_archive_filter_code(struct archive *a, int n)
{
	return transform_filter_code(a->transform, n);
}

static const char *
_archive_filter_name(struct archive *a, int n)
{
	return transform_filter_name(a->transform, n);
}

static int64_t
_archive_filter_bytes(struct archive *a, int n)
{
	return transform_filter_bytes(a->transform, n);
}

/*
 * Used internally by read format handlers to register their bid and
 * initialization functions.
 */
int
__archive_read_register_format(struct archive_read *a,
    void *format_data,
    const char *name,
    int (*bid)(struct archive_read *),
    int (*options)(struct archive_read *, const char *, const char *),
    int (*read_header)(struct archive_read *, struct archive_entry *),
#if ARCHIVE_VERSION_NUMBER < 3000000
    int (*read_data)(struct archive_read *, const void **, size_t *, off_t *),
#else
    int (*read_data)(struct archive_read *, const void **, size_t *, int64_t *),
#endif
    int (*read_data_skip)(struct archive_read *),
    int (*cleanup)(struct archive_read *))
{
	int i, number_slots;

	archive_check_magic(&a->archive,
	    ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "__archive_read_register_format");

	number_slots = sizeof(a->formats) / sizeof(a->formats[0]);

	for (i = 0; i < number_slots; i++) {
		if (a->formats[i].bid == bid)
			return (ARCHIVE_WARN); /* We've already installed */
		if (a->formats[i].bid == NULL) {
			a->formats[i].bid = bid;
			a->formats[i].options = options;
			a->formats[i].read_header = read_header;
			a->formats[i].read_data = read_data;
			a->formats[i].read_data_skip = read_data_skip;
			a->formats[i].cleanup = cleanup;
			a->formats[i].data = format_data;
			a->formats[i].name = name;
			return (ARCHIVE_OK);
		}
	}

	__archive_errx(1, "Not enough slots for format registration");
	return (ARCHIVE_FATAL); /* Never actually called. */
}

/*
 * The next section implements the peek/consume internal I/O
 * system used by archive readers.  This system allows simple
 * read-ahead for consumers while preserving zero-copy operation
 * most of the time.
 *
 * The two key operations:
 *  * The read-ahead function returns a pointer to a block of data
 *    that satisfies a minimum request.
 *  * The consume function advances the file pointer.
 *
 * In the ideal case, filters generate blocks of data
 * and __archive_read_ahead() just returns pointers directly into
 * those blocks.  Then __archive_read_consume() just bumps those
 * pointers.  Only if your request would span blocks does the I/O
 * layer use a copy buffer to provide you with a contiguous block of
 * data.
 *
 * A couple of useful idioms:
 *  * "I just want some data."  Ask for 1 byte and pay attention to
 *    the "number of bytes available" from __archive_read_ahead().
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
 * __archive_read_consume() below.
 */

const void *
__archive_read_ahead(struct archive_read *a, size_t min, ssize_t *avail)
{
	const void *data;
	data = transform_read_ahead(a->archive.transform, min, avail);
	if (avail && TRANSFORM_FATAL == *avail) {
		__archive_set_error_from_transform(&a->archive, a->archive.transform);
		*avail = ARCHIVE_FATAL;
	}
	return (data);
}

/*
 * Move the file pointer forward.
 */
int64_t
__archive_read_consume(struct archive_read *a, int64_t request)
{
	int64_t ret;
	ret = transform_read_consume(a->archive.transform, request);
	if (TRANSFORM_FATAL == ret) {
		__archive_set_error_from_transform(&a->archive, a->archive.transform);
		ret = ARCHIVE_FATAL;
	}
	return (ret);
}
